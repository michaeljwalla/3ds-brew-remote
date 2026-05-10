//for statics
#include "mappings.h"
#include "static_mappings.h"
#include "server/protocol.h"
#include "static_inputs.h"
#include "logger.h"
#include <utility>

namespace {
    using namespace LoggerCommons;
    using namespace mappingtypes;
    //
    using IO = InputObject;
    using Datagram = const RawInput&;
    using pair = std::pair<InputTypes, InputMap<IO>>;

    #define ON(a,b) (static_cast<uint32_t>(a) & static_cast<uint32_t>(b))
    #define LOG_BTN(mask, code, log) \
        *(log) << #mask " " << (ON(mask, (code).buttons) ? "ON" : "OFF") << endl;
    #define LOG_DELTA(name, state, log) \
        *(log) << name " " << (state ? "DELTA" : "STOP") << endl;
}


std::unordered_map<InputTypes, InputMap<InputObject>> available {
    pair(InputTypes::NONE, {}),
    pair(InputTypes::MOUSE, {}),
    pair(InputTypes::LOGGING, { "LOGGING", {
        {TotalInputMask::A, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::A, code, log);
            return 0;
        }},
        {TotalInputMask::B, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::B, code, log);
            return 0;
        }},
        {TotalInputMask::X, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::X, code, log);
            return 0;
        }},
        {TotalInputMask::Y, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::Y, code, log);
            return 0;
        }},
        {TotalInputMask::LEFT, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::LEFT, code, log);
            return 0;
        }},
        {TotalInputMask::RIGHT, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::RIGHT, code, log);
            return 0;
        }},
        {TotalInputMask::UP, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::UP, code, log);
            return 0;
        }},
        {TotalInputMask::DOWN, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::DOWN, code, log);
            return 0;
        }},
        {TotalInputMask::L, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::L, code, log);
            return 0;
        }},
        {TotalInputMask::R, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::R, code, log);
            return 0;
        }},
        {TotalInputMask::ZL, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::ZL, code, log);
            return 0;
        }},
        {TotalInputMask::ZR, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::ZR, code, log);
            return 0;
        }},
        {TotalInputMask::SELECT, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::SELECT, code, log);
            return 0;
        }},
        {TotalInputMask::START, [](IO* in, Datagram code, Logger* log) {
            LOG_BTN(ButtonMask::START, code, log);
            return 0;
        }},
        {TotalInputMask::TOUCH, [](IO* in, Datagram code, Logger* log) {
            LOG_DELTA("TOUCH", code.touch_active, log);
            return 0;
        }},
        {TotalInputMask::CIRCLE_PAD, [](IO* in, Datagram code, Logger* log) {
            LOG_DELTA("CIRCLE_PAD", true, log);
            return 0;
        }},
        {TotalInputMask::CIRCLE_PAD_PRO, [](IO* in, Datagram code, Logger* log) {
            LOG_DELTA("CIRCLE_PAD_PRO", true, log);
            return 0;
        }},
        {TotalInputMask::GYRO, [](IO* in, Datagram code, Logger* log) {
            LOG_DELTA("GYRO", true, log);
            return 0;
        }},
        {TotalInputMask::ACCEL, [](IO* in, Datagram code, Logger* log) {
            LOG_DELTA("ACCEL", code.touch_active, log);
            return 0;
        }},
        
        // {TotalInputMask::ALWAYS, [](IO* in, Datagram code, Logger* log) {
        //     LOG_DELTA("PACKET", true, log);
        //     return 0;
        // }},
        
    }}),
};

InputMap<InputObject>& get_mapping(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error(std::string("no mapping for type ") + getInputTypeName(type));
    return it->second;
}