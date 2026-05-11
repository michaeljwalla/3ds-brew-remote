#include "server.h"
#include "server/protocol.h"
#include "logger.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Constants from receiver.py
// threshold to flag as 'change,' any more can be handled by Handlers.
static constexpr float DELTA_EPSILON = 0.01f;

static constexpr float HANDSHAKE_TIMEOUT_S = 0.5f;
static constexpr int HANDSHAKE_RETRIES = 30;
static constexpr float POLL_TIMEOUT_S = 1.0f;
static constexpr float AUTO_TERMINATE_S = 30.0f;
static constexpr float KEEPALIVE_INTERVAL_S = 5.0f;

using namespace LoggerCommons;

// states just for definite output/end
static const LoggerState LOG_GOOD {0};
static const LoggerState LOG_ERR {1};
static const LoggerState& LOG_END = LoggerState::END;

static void set_socket_timeout(int sockfd, float seconds) {
    struct timeval tv;
    tv.tv_sec = (long)seconds;
    tv.tv_usec = (long)((seconds - tv.tv_sec) * 1000000);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}


static bool do_handshake(int sockfd, const struct sockaddr_in& ds_addr, Logger& log) {
    set_socket_timeout(sockfd, HANDSHAKE_TIMEOUT_S);

    for (int attempt = 1; attempt <= HANDSHAKE_RETRIES; ++attempt) {
        // Send HELLO_3DS
        sendto(sockfd, HELLO_MSG, strlen(HELLO_MSG), 0,
               (const struct sockaddr*)&ds_addr, sizeof(ds_addr));

        // Wait for ACK_3DS
        char buffer[64];
        struct sockaddr_in reply_addr;
        socklen_t reply_len = sizeof(reply_addr);

        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&reply_addr, &reply_len);

        if (n > 0 && (size_t)n == strlen(ACK_MSG) &&
            std::memcmp(buffer, ACK_MSG, strlen(ACK_MSG)) == 0) {
            log << LOG_GOOD <<"Handshake successful: ACK received from "
                      << inet_ntoa(reply_addr.sin_addr) << ":"
                      << ntohs(reply_addr.sin_port) << endl << LOG_END;
            return true;
        }
    }

    log << LOG_ERR << "Handshake failed after " << HANDSHAKE_RETRIES << " attempts" << endl << LOG_END;
    return false;
}


static bool validate_socket(const Endpoint& ep, int& sockfd_out, sockaddr_in& pc_addr_out, sockaddr_in& ds_addr_out, Logger& log) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log << LOG_ERR << "Failed to create socket" << endl << LOG_END;
        return false;
    }

    // Bind to PC port
    sockaddr_in pc_addr{};
    pc_addr.sin_family = AF_INET;
    pc_addr.sin_addr.s_addr = INADDR_ANY;
    pc_addr.sin_port = htons(ep.Port_PC);

    if (bind(sockfd, (const sockaddr*)&pc_addr, sizeof(pc_addr)) < 0) {
        log << LOG_ERR << "Failed to bind to port " << ep.Port_PC << endl << LOG_END;
        close(sockfd);
        return false;
    }

    //build 3DS address
    struct sockaddr_in ds_addr{};
    ds_addr.sin_family = AF_INET;
    ds_addr.sin_port = htons(ep.Port_3DS);
    if (inet_pton(AF_INET, ep.IP_Address.c_str(), &ds_addr.sin_addr) <= 0) {
        log << LOG_ERR << "Invalid IP address: " << ep.IP_Address << endl << LOG_END;
        close(sockfd);
        return false;
    }

    sockfd_out = sockfd;
    pc_addr_out = pc_addr;
    ds_addr_out = ds_addr;
    return true;
}

template<typename T>
concept isFP = std::is_floating_point_v<T>;
template<isFP T>
//floats specifically
bool is_diff(T& a, T& b) {
    return std::abs(a - b) > DELTA_EPSILON;
}
uint32_t get_diffs(RawInput& prev, RawInput& cur) {
    uint32_t diff = 0;
    diff |= (prev.buttons ^ cur.buttons); //first 0-13 is the same
    //circle pad
    diff |= (
        (is_diff(prev.circle_pad[0], cur.circle_pad[0])) ||
        (is_diff(prev.circle_pad[1], cur.circle_pad[1]))
    ) << 14;
    //cpad pro
    diff |= (
        (is_diff(prev.circle_pad_pro[0], cur.circle_pad_pro[0])) ||
        (is_diff(prev.circle_pad_pro[1], cur.circle_pad_pro[1]))
    ) << 15;
    //touch sensor
    diff |= (
        prev.touch_active ^ cur.touch_active ||
        is_diff(prev.touch[0], cur.touch[0]) ||
        is_diff(prev.touch[1], cur.touch[1])
    ) << 16;
    //gyroscope
    diff |= (
        (is_diff(prev.gyro[0], cur.gyro[0])) ||
        (is_diff(prev.gyro[1], cur.gyro[1])) ||
        (is_diff(prev.gyro[2], cur.gyro[2]))
    ) << 17;
    //accelerometer
    diff |= (
        (is_diff(prev.accel[0], cur.accel[0])) ||
        (is_diff(prev.accel[1], cur.accel[1])) ||
        (is_diff(prev.accel[2], cur.accel[2]))
    ) << 18;
    //always runs per packet (not guaranteed rate)
    diff |= 1u << 19; //ALWAYS
    diff |= 1u << 20; //AFTER
    return diff;
}
//main receiver runner
void run_client(const Endpoint& ep, InputController& controller) {
    auto& log = controller.get_logger() ? *controller.get_logger() : Logger::singleton(); 
    
    // Create UDP socket
    int sockfd;
    sockaddr_in pc_addr, ds_addr;
    if ( !validate_socket(ep, sockfd, pc_addr, ds_addr, log)) return;

    log << LOG_GOOD;
    log << "Listening on port " << ep.Port_PC << endl;
    log << "Connecting to 3DS at " << ep.IP_Address << ":" << ep.Port_3DS << endl;
    log << LOG_END;

    if (!do_handshake(sockfd, ds_addr, log)) {
        close(sockfd);
        return;
    }

    set_socket_timeout(sockfd, POLL_TIMEOUT_S);
    
    //loop read section
    log << LOG_GOOD << "Streaming input packets. Press Ctrl+C to stop." << endl << LOG_END;

    using Clock = std::chrono::steady_clock;
    auto last_rx = Clock::now();
    auto last_keepalive = Clock::now();

    uint64_t packet_count = 0;
    RawInput prev_input{}; //for delta checks
    while (true) {
        auto now = Clock::now();

        // keepalive test
        std::chrono::duration<float> keepalive_elapsed = now - last_keepalive;
        if (keepalive_elapsed.count() >= KEEPALIVE_INTERVAL_S) {
            sendto(sockfd, HELLO_MSG, strlen(HELLO_MSG), 0,
                   (const struct sockaddr*)&ds_addr, sizeof(ds_addr));
            last_keepalive = now;
        }

        //

        uint8_t buffer[PACKET_SIZE + 32];
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);

        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&src_addr, &src_len);

        if (n < 0) {
            // Timeout or other error
            std::chrono::duration<float> idle = now - last_rx;
            if (idle.count() >= AUTO_TERMINATE_S) {
                log << LOG_GOOD << "No input for " << AUTO_TERMINATE_S
                          << " seconds. Terminating." << endl << LOG_END;
                break;
            }
            continue;
        }

        // Discard keepalive ACK
        if ((size_t)n == strlen(ACK_MSG) &&
            std::memcmp(buffer, ACK_MSG, strlen(ACK_MSG)) == 0) {
            continue;
        }

        RawInput input;
        if (!unpack_payload(buffer, n, input)) {
            continue;
        }

        last_rx = now;
        ++packet_count;

        // process input and fire handlers
        auto diff_mask = get_diffs(prev_input, input);
        for (uint32_t i = 0u; i < NUM_INPUTS; ++i) {
            const TotalInputMask btn = static_cast<TotalInputMask>(1 << i);
            if (diff_mask & (1u << i)) controller.fire(btn, input);
        }
        std::exchange(prev_input, input); //update prev for next round
        //output
        if (packet_count % 4 == 0) {  //print at 30hz
            log << LOG_GOOD
                << "\rPackets: " << packet_count
                << " Diffs: " << std::format("{:b}", diff_mask)
                << flush << LOG_END;
        }
    }

    log << LOG_GOOD << endl << LOG_END;
    close(sockfd);
}