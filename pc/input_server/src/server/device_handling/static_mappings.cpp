//for statics
#include "mappings.h"
#include "static_mappings.h"
#include "server/protocol.h"
#include "static_inputs.h"
#include "static_configs.h"
#include "logger.h"
#include <chrono>
#include <cstdint>
#include <utility>
#include "datatypes.h"

namespace {
    using namespace LoggerCommons;
    using namespace mappingtypes;
    //
    using IO = InputObject;
    using pair = std::pair<InputTypes, InputMap<IO>>;
    using Params = HandlerParams<InputObject>;

    //all options
    using Options = TotalInputMask;
    //just buttons, included within options
    using Buttons = ButtonMask;

    using clock = std::chrono::steady_clock;
    using ms = std::chrono::milliseconds;
    

    #define ON(a,b) (static_cast<uint32_t>(a) & static_cast<uint32_t>(b))
    #define LOG_BTN(mask, code, log) \
        *(log) << #mask " " << (ON(mask, (code).buttons) ? "ON" : "OFF") << endl;
    #define LOG_DELTA(name, state, log) \
        *(log) << name " " << (state ? "DELTA" : "STOP") << endl;
    
    //get delta
    #define GET_DELTA_MS(start, end) \
        std::chrono::duration_cast<std::chrono::milliseconds>((end) - (start)).count()

    //reset and cap are for jitter/drops. Choose how you wanna handle!
    #define RESET_DELTA_MS(delta, limit) \
        delta > limit ? 0 : delta
    
    #define CAP_DELTA_MS(delta, limit) \
        delta > limit ? limit : delta
}


std::unordered_map<InputTypes, InputMap<InputObject>> available {
    pair(InputTypes::NONE, {}),
    pair(InputTypes::MOUSE, {"MOUSE", {
        {Options::ALWAYS, [](Params data) {
            static auto last = clock::now();

            auto& mouse = *static_cast<InputMouse*>(data.parent);
            auto buttons = data.code.buttons;
            auto cfg = config::get<InputMouse>();

            //direction
            coords<float> dir = coords<int>{
                (ON(Buttons::LEFT, buttons) ? -1 : 0) + (ON(Buttons::RIGHT, buttons) ? 1 : 0),
                (ON(Buttons::DOWN, buttons) ? -1 : 0) + (ON(Buttons::UP, buttons) ? 1 : 0)
            }.normalized();

            //apply modifications (cfg, time)
            auto cur = clock::now();
            auto diff = CAP_DELTA_MS( GET_DELTA_MS(cur, std::exchange(last, cur)), 1000);

            mouse.move( static_cast<coords<int16_t>>(dir * cfg.speed * diff) );
            return 0;
        }}
    }}),
    pair(InputTypes::LOGGING, { "LOGGING", {
        {Options::A, [](Params data) {
            LOG_BTN(Buttons::A, data.code, data.log);
            return 0;
        }},
        {Options::B, [](Params data) {
            LOG_BTN(Buttons::B, data.code, data.log);
            return 0;
        }},
        {Options::X, [](Params data) {
            LOG_BTN(Buttons::X, data.code, data.log);
            return 0;
        }},
        {Options::Y, [](Params data) {
            LOG_BTN(Buttons::Y, data.code, data.log);
            return 0;
        }},
        {Options::LEFT, [](Params data) {
            LOG_BTN(Buttons::LEFT, data.code, data.log);
            return 0;
        }},
        {Options::RIGHT, [](Params data) {
            LOG_BTN(Buttons::RIGHT, data.code, data.log);
            return 0;
        }},
        {Options::UP, [](Params data) {
            LOG_BTN(Buttons::UP, data.code, data.log);
            return 0;
        }},
        {Options::DOWN, [](Params data) {
            LOG_BTN(Buttons::DOWN, data.code, data.log);
            return 0;
        }},
        {Options::L, [](Params data) {
            LOG_BTN(Buttons::L, data.code, data.log);
            return 0;
        }},
        {Options::R, [](Params data) {
            LOG_BTN(Buttons::R, data.code, data.log);
            return 0;
        }},
        {Options::ZL, [](Params data) {
            LOG_BTN(Buttons::ZL, data.code, data.log);
            return 0;
        }},
        {Options::ZR, [](Params data) {
            LOG_BTN(Buttons::ZR, data.code, data.log);
            return 0;
        }},
        {Options::SELECT, [](Params data) {
            LOG_BTN(Buttons::SELECT, data.code, data.log);
            return 0;
        }},
        {Options::START, [](Params data) {
            LOG_BTN(Buttons::START, data.code, data.log);
            return 0;
        }},
        {Options::TOUCH, [](Params data) {
            LOG_DELTA("TOUCH", data.code.touch_active, data.log);
            return 0;
        }},
        {Options::CIRCLE_PAD, [](Params data) {
            LOG_DELTA("CIRCLE_PAD", true, data.log);
            return 0;
        }},
        {Options::CIRCLE_PAD_PRO, [](Params data) {
            LOG_DELTA("CIRCLE_PAD_PRO", true, data.log);
            return 0;
        }},
        {Options::GYRO, [](Params data) {
            LOG_DELTA("GYRO", true, data.log);
            return 0;
        }},
        {Options::ACCEL, [](Params data) {
            LOG_DELTA("ACCEL", data.code.touch_active, data.log);
            return 0;
        }},
        
        // {Options::ALWAYS, [](Params data) {
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