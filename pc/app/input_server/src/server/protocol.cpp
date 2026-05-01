// Implementation of protocol helpers for input_server
#include "protocol.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>

bool unpack_payload(const void* buf, size_t len, RawInput &out) {
    if (len != sizeof(RawInput)) return false;

    RawInput in;
    std::memcpy(&in, buf, sizeof(RawInput));

    if (in.magic[0] != MAGIC[0] || in.magic[1] != MAGIC[1]) return false;

    // type-pun floats to int32 to align endian-ness
    for (int i = 0; i < 2; i++) {
        uint32_t u32;
        std::memcpy(&u32, &in.circle_pad[i], sizeof(uint32_t));
        u32 = ntohl(u32);
        std::memcpy(&in.circle_pad[i], &u32, sizeof(uint32_t));
    }

    for (int i = 0; i < 2; i++) {
        uint32_t u32;
        std::memcpy(&u32, &in.circle_pad_pro[i], sizeof(uint32_t));
        u32 = ntohl(u32);
        std::memcpy(&in.circle_pad_pro[i], &u32, sizeof(uint32_t));
    }

    in.buttons = ntohs(in.buttons);

    for (int i = 0; i < 2; i++) {
        uint32_t u32;
        std::memcpy(&u32, &in.touch[i], sizeof(uint32_t));
        u32 = ntohl(u32);
        std::memcpy(&in.touch[i], &u32, sizeof(uint32_t));
    }

    for (int i = 0; i < 3; i++) {
        uint32_t u32;
        std::memcpy(&u32, &in.gyro[i], sizeof(uint32_t));
        u32 = ntohl(u32);
        std::memcpy(&in.gyro[i], &u32, sizeof(uint32_t));
    }

    for (int i = 0; i < 3; i++) {
        uint32_t u32;
        std::memcpy(&u32, &in.accel[i], sizeof(uint32_t));
        u32 = ntohl(u32);
        std::memcpy(&in.accel[i], &u32, sizeof(uint32_t));
    }
    out = std::move(in);
    return true;
}
