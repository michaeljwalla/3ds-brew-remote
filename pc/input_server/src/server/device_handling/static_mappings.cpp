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
    using pair = std::pair<InputTypes, InputMap<IO>>;
    using Params = HandlerParams<InputObject>;

    #define ON(a,b) (static_cast<uint32_t>(a) & static_cast<uint32_t>(b))
    #define LOG_BTN(mask, code, log) \
        *(log) << #mask " " << (ON(mask, (code).buttons) ? "ON" : "OFF") << endl;
    #define LOG_DELTA(name, state, log) \
        *(log) << name " " << (state ? "DELTA" : "STOP") << endl;
}


std::unordered_map<InputTypes, InputMap<InputObject>> available {
    pair(InputTypes::NONE, {}),
    pair(InputTypes::MOUSE, {"MOUSE", {
        {}
    }}),
    pair(InputTypes::LOGGING, { "LOGGING", {
        {TotalInputMask::A, [](Params data) {
            LOG_BTN(ButtonMask::A, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::B, [](Params data) {
            LOG_BTN(ButtonMask::B, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::X, [](Params data) {
            LOG_BTN(ButtonMask::X, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::Y, [](Params data) {
            LOG_BTN(ButtonMask::Y, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::LEFT, [](Params data) {
            LOG_BTN(ButtonMask::LEFT, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::RIGHT, [](Params data) {
            LOG_BTN(ButtonMask::RIGHT, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::UP, [](Params data) {
            LOG_BTN(ButtonMask::UP, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::DOWN, [](Params data) {
            LOG_BTN(ButtonMask::DOWN, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::L, [](Params data) {
            LOG_BTN(ButtonMask::L, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::R, [](Params data) {
            LOG_BTN(ButtonMask::R, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::ZL, [](Params data) {
            LOG_BTN(ButtonMask::ZL, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::ZR, [](Params data) {
            LOG_BTN(ButtonMask::ZR, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::SELECT, [](Params data) {
            LOG_BTN(ButtonMask::SELECT, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::START, [](Params data) {
            LOG_BTN(ButtonMask::START, data.code, data.log);
            return 0;
        }},
        {TotalInputMask::TOUCH, [](Params data) {
            LOG_DELTA("TOUCH", data.code.touch_active, data.log);
            return 0;
        }},
        {TotalInputMask::CIRCLE_PAD, [](Params data) {
            LOG_DELTA("CIRCLE_PAD", true, data.log);
            return 0;
        }},
        {TotalInputMask::CIRCLE_PAD_PRO, [](Params data) {
            LOG_DELTA("CIRCLE_PAD_PRO", true, data.log);
            return 0;
        }},
        {TotalInputMask::GYRO, [](Params data) {
            LOG_DELTA("GYRO", true, data.log);
            return 0;
        }},
        {TotalInputMask::ACCEL, [](Params data) {
            LOG_DELTA("ACCEL", data.code.touch_active, data.log);
            return 0;
        }},
        
        // {TotalInputMask::ALWAYS, [](Params data) {
        //     LOG_DELTA("PACKET", true, data.log);
        //     return 0;
        // }},
        
    }}),
};

InputMap<InputObject>& get_mapping(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error(std::string("no mapping for type ") + getInputTypeName(type));
    return it->second;
}