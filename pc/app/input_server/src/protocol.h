#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <sys/types.h>

namespace {
    static constexpr std::array<uint8_t, 2> MAGIC = { 0x3d, 0x53 };
    static constexpr uint8_t VERSION = 2;
    static constexpr std::array<uint16_t, 2> TOUCHSCREEN_DIMS = {320, 240};
    //
    static constexpr std::string_view HELLO_MSG = "HELLO_3DS";
    static constexpr std::string_view ACK_MSG   = "ACK_3DS";
}
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

struct SocketPorts {
    static constexpr uint16_t DEFAULT_3DS_PORT = 6000;
    static constexpr uint16_t DEFAULT_PC_PORT  = 6001;

    uint16_t DS = DEFAULT_3DS_PORT;
    uint16_t PC  = DEFAULT_PC_PORT;
};

//
#pragma pack(push, 1)
struct RawInput {         //55-bytes
    uint8_t  magic[2];
    uint8_t  field1;
    uint8_t  field2;
    float    floats1[4];
    uint16_t field3;
    uint8_t  field4;
    float    floats2[8];
};
#pragma pack(pop)

