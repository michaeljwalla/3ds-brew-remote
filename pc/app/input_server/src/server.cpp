#include "server.h"
#include "protocol.h"
#include "logger.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Constants from receiver.py
static constexpr float HANDSHAKE_TIMEOUT_S = 0.5f;
static constexpr int HANDSHAKE_RETRIES = 30;
static constexpr float POLL_TIMEOUT_S = 1.0f;
static constexpr float AUTO_TERMINATE_S = 30.0f;
static constexpr float KEEPALIVE_INTERVAL_S = 5.0f;


static void set_socket_timeout(int sockfd, float seconds) {
    struct timeval tv;
    tv.tv_sec = (long)seconds;
    tv.tv_usec = (long)((seconds - tv.tv_sec) * 1000000);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}


static bool do_handshake(int sockfd, const struct sockaddr_in& ds_addr) {
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
            std::cout << "Handshake successful: ACK received from "
                      << inet_ntoa(reply_addr.sin_addr) << ":"
                      << ntohs(reply_addr.sin_port) << std::endl;
            return true;
        }
    }

    std::cerr << "Handshake failed after " << HANDSHAKE_RETRIES << " attempts"
              << std::endl;
    return false;
}


static void validate_socket(const Endpoint& ep, int& sockfd_out, sockaddr_in& pc_addr_out, sockaddr_in& ds_addr_out) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    // Bind to PC port
    sockaddr_in pc_addr{};
    pc_addr.sin_family = AF_INET;
    pc_addr.sin_addr.s_addr = INADDR_ANY;
    pc_addr.sin_port = htons(ep.Port_PC);

    if (bind(sockfd, (const sockaddr*)&pc_addr, sizeof(pc_addr)) < 0) {
        std::cerr << "Failed to bind to port " << ep.Port_PC << std::endl;
        close(sockfd);
        return;
    }

    //build 3DS address
    struct sockaddr_in ds_addr{};
    ds_addr.sin_family = AF_INET;
    ds_addr.sin_port = htons(ep.Port_3DS);
    if (inet_pton(AF_INET, ep.IP_Address.c_str(), &ds_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << ep.IP_Address << std::endl;
        close(sockfd);
        return;
    }

    sockfd_out = sockfd;
    pc_addr_out = pc_addr;
    ds_addr_out = ds_addr;
    return;
}

//main receiver runner
void run_client(const Endpoint& ep) {
    // Create UDP socket
    int sockfd;
    sockaddr_in pc_addr, ds_addr;
    validate_socket(ep, sockfd, pc_addr, ds_addr);

    std::cout << "Listening on port " << ep.Port_PC << std::endl;
    std::cout << "Connecting to 3DS at " << ep.IP_Address << ":" << ep.Port_3DS << std::endl;

    if (!do_handshake(sockfd, ds_addr)) {
        close(sockfd);
        return;
    }

    set_socket_timeout(sockfd, POLL_TIMEOUT_S);
    
    //loop read section
    std::cout << "Streaming input packets. Press Ctrl+C to stop." << std::endl;

    using Clock = std::chrono::steady_clock;
    auto last_rx = Clock::now();
    auto last_keepalive = Clock::now();

    uint64_t packet_count = 0;

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
                std::cout << "No input for " << AUTO_TERMINATE_S
                          << " seconds. Terminating." << std::endl;
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

        if (packet_count % 4 == 0) {  //print at 30hz
            std::cout << "\rPackets: " << packet_count
                      << " | Circle: (" << input.circle_pad[0] << ", "
                      << input.circle_pad[1] << ") | Buttons: 0x" << std::hex
                      << input.buttons << std::dec << " | Touch: "
                      << (input.touch_active ? "YES" : "NO") << "      "
                      << std::flush;
        }
    }

    std::cout << std::endl;
    close(sockfd);
}
