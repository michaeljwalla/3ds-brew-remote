#pragma once
#include <array>
#include <cstdint>
#include <sys/types.h>

namespace {
    constexpr std::array<uint8_t, 2> MAGIC = { 0x3d, 0x53 };
    constexpr uint8_t VERSION = 2;
    constexpr std::array<uint16_t, 2> TOUCHSCREEN_DIMS = {320, 240};
    constexpr uint16_t PACKET_SIZE = 55;

    const char HELLO_MSG[10] = "HELLO_3DS";
    const  char ACK_MSG[8] = "ACK_3DS";
}


enum class ButtonMask: uint16_t {
    NEVER  = 0,
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
    START  = 1 << 13,
};

//added for parsing on server, which will perform simple delta checks
//button states will fire once after they are "unchecked", the RawInput
//should be used to determine this "end"
//
//methods fire in linear order but exceptions will not catch
//(use your own try catches)
enum class TotalInputMask: uint32_t {
    A      = 1,
    B      = 2,
    X      = 3,
    Y      = 4,
    UP     = 5,
    DOWN   = 6,
    LEFT   = 7,
    RIGHT  = 8,
    L      = 9,
    R      = 10,
    ZL     = 11,
    ZR     = 12,
    SELECT = 13,
    START  = 14,
    // not real "button" states but still discrete input events (still needa check)
    CIRCLE_PAD     = 15,
    CIRCLE_PAD_PRO = 16,
    TOUCH          = 17,
    GYRO           = 18,
    ACCEL          = 19,
    //per-packet hooks
    FIRST = 0, //ex initialize/refetch a persistent datatype across methods
    ALWAYS = 20, // take a guess
    LAST   = 21, // same as ALWAYS (but after); just explicit to put cleanup here 
};

//
static constexpr uint16_t NUM_INPUTS = 22;
static constexpr int REFRESH_RATE = 60; //Hz

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

bool unpack_payload(const void* buf, size_t len, RawInput &out);