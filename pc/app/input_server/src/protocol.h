#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <netinet/in.h>

static constexpr std::array<uint8_t, 2> MAGIC = { 0x3d, 0x53 };
static constexpr uint8_t VERSION = 2;
static constexpr std::array<uint16_t, 2> TOUCHSCREEN_DIMS = {320, 240};
static constexpr uint16_t PACKET_SIZE = 55;

static const char HELLO_MSG[10] = "HELLO_3DS";
static const  char ACK_MSG[8] = "ACK_3DS";

enum ButtonMask: uint16_t {
    A      = 1 << 0,
    B      = 1 << 1,
    X      = 1 << 2,
    Y      = 1 << 3,
    UP     = 1 << 4,
    DOWN   = 1 << 5,
    LEFT   = 1 << 6,
    RIGHT  = 1 << 7,
    L      = 1 << 8,
    R      = 1 << 9,
    ZL     = 1 << 10,
    ZR     = 1 << 11,
    SELECT = 1 << 12,
    START  = 1 << 13
};


//
#pragma pack(push, 1)
struct RawInput {                                                                                                                         
    uint8_t  magic[2];                                                                                                          
    uint8_t  version;                                                                                                           
    uint8_t  cpp_present;      //this false positives so use delta check
    float    circle_pad[2];                                                                                      
    float    circle_pad_pro[2];                                                                                        
    uint16_t buttons;            
    uint8_t  touch_active;                                                                                                       
    float    touch[2];           
    float    gyro[3];            
    float    accel[3];                                                                                       
};
#pragma pack(pop)


inline bool unpack_payload(const void* buf, size_t len, RawInput &out) {
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
