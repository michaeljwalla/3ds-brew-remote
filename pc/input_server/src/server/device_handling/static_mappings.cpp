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

    using LS = LoggerState;

    //all options
    using Options = TotalInputMask;
    //just buttons, included within options
    using Buttons = ButtonMask;

    using clock = std::chrono::steady_clock;

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

    #define MAX(a, b) \
        ((a) > (b)  ? (a) : (b))
    #define MIN(a, b) \
        ((a) < (b) ? (a) : (b))

    #define IGNORE(other,goal,out) \
        ((other) <= (goal) ? (out) : (other))
    
    // -eps <= a - b <= eps ~ |a-b| <= eps
    #define WITHIN(a,b,eps) \
        ((-(eps) <= ((a)-(b))) && (((a)-(b)) <= (eps)))


    // ex IS_SIMILAR(0.95, 1.0, 95, 0.1) ~ a & b are 95+-0.1% similar
    #define IS_SIMILAR(a,b, p, eps) \
        (((a) == 0 || (b) == 0) ? false : \
        (WITHIN((a)/(b), (p)*1e-2, (eps)*1e-2) || WITHIN((b)/(a), (p)*1e-2, (eps)*1e-2)))
}

std::unordered_map<InputTypes, InputMap<InputObject>> available {
    pair(InputTypes::NONE, {}),
    pair(InputTypes::MOUSE, {"MOUSE", {
        //left click
        {Options::L, [](Params data) {
            auto buttons = data.code.buttons;
            auto& mouse = *static_cast<InputMouse*>(data.parent);

            if ( ON(Buttons::L, buttons) ) mouse.button_down( BTN_LEFT );
            else mouse.button_up( BTN_LEFT );
            return 0;
        }},
        {Options::A, [](Params data) {
            auto buttons = data.code.buttons;
            auto& mouse = *static_cast<InputMouse*>(data.parent);

            if ( ON(Buttons::A, buttons) ) mouse.button_down( BTN_LEFT );
            else mouse.button_up( BTN_LEFT );
            return 0;
        }},

        //right click
        {Options::R, [](Params data) {
            auto buttons = data.code.buttons;
            auto& mouse = *static_cast<InputMouse*>(data.parent);

            if ( ON(Buttons::R, buttons) ) mouse.button_down( BTN_RIGHT );
            else mouse.button_up( BTN_RIGHT );
            return 0;
        }},
        {Options::B, [](Params data) {
            auto buttons = data.code.buttons;
            auto& mouse = *static_cast<InputMouse*>(data.parent);

            if ( ON(Buttons::B, buttons) ) mouse.button_down( BTN_RIGHT );
            else mouse.button_up( BTN_RIGHT );
            return 0;
        }},

        //mouse movement
        {Options::ALWAYS, [](Params data) {
            //each lambda tracks its own last to prevent diff accumulation
            
            float to_s = 0.001;
            auto& mouse = *static_cast<InputMouse*>(data.parent);
            auto buttons = data.code.buttons;
            auto& cfg = config::get<InputMouse>();
            auto& cfg_keys = config::get<TotalInputMask>();
            
            //calculate direction/movement
            //y axis flipped since origin is top left
            bool moved = [&](){
                static auto last = clock::now();
                static coords<float> debt{0,0};
                static coords<float> velocity{0, 0};
                static auto last_success = last;
                //
                auto cur = clock::now();
                auto raw_diff = GET_DELTA_MS(std::exchange(last, cur), cur);
                auto diff = CAP_DELTA_MS(raw_diff, 1000);
                //
                coords<float> dir;
                bool success = false;
                if (cfg_keys.circle_pads.main.enabled) {
                    auto dz = cfg_keys.circle_pads.dead_zone;
                    auto cp = data.code.circle_pad;
                    dir = coords<float>{
                        IGNORE(*cp, dz, 0) + -IGNORE(-*cp, dz, 0),           //works for if dir is neg/pos (one is always dropped)
                        -((IGNORE(*(cp+1), dz, 0) + -IGNORE(-*(cp+1), dz, 0))),
                    };
                    success = dir.magnitude();
                }
                if (!success & cfg_keys.circle_pads.pro.enabled) {
                    auto dz = cfg_keys.circle_pads.dead_zone;
                    auto cp = data.code.circle_pad_pro;
                    dir = coords<float>{
                        IGNORE(*cp, dz, 0) + IGNORE(-*cp, dz, 0),           //works for if dir is neg/pos (one is always dropped)
                        -((IGNORE(*(cp+1), dz, 0) + IGNORE(-*(cp+1), dz, 0))),
                    };
                    success = dir.magnitude();
                }
                if (!success) {
                    dir = coords<int>{
                        (ON(Buttons::LEFT, buttons) ? -1 : 0) + (ON(Buttons::RIGHT, buttons) ? 1 : 0),
                        -((ON(Buttons::DOWN, buttons) ? -1 : 0) + (ON(Buttons::UP, buttons) ? 1 : 0))
                    }.normalized();
                    success = dir.magnitude();
                }


                auto accel = cfg.accelerate;
                auto delta_last_success = GET_DELTA_MS(last_success, cur);
                if (!success) { //nothing requested.
                    if (
                        !accel.enabled \
                        || delta_last_success > accel.preserve_ms
                    ) {
                        velocity *= 0;
                    }
                    return false;
                }
                last_success = last;

                //
                // apply time modifications (cfg, time)

                if (accel.enabled && !ON(accel.temp_disable, buttons) && dir.magnitude())
                    // &&  IS_SIMILAR(velocity.magnitude(), accel.max_v.magnitude(), 99, 1))
                { //only accel up, not down (insta stop)
                    float dv = accel.rate * diff * to_s;
                    float add = ((raw_diff < accel.start_double_tap_ms //try to avoid lag spikes
                        && delta_last_success > accel.start_double_tap_ms
                        && delta_last_success < accel.end_double_tap_ms)
                    ? accel.double_tap_add : 0);
                    
                    // decided to move addvec outside bounds; allows extra speeding.
                    auto vm = velocity.magnitude(), am = accel.max_v.magnitude();
                    if (!add && (vm < am)) {
                        velocity = coords<float>{
                        MAX(MIN(velocity.x + dv, accel.max_v.x), accel.min_v.x),
                        MAX(MIN(velocity.y + dv, accel.max_v.y), accel.min_v.y)
                        };
                    } else if (add) { //the double click exceeded barrier
                        velocity = coords<float> {
                            MAX(velocity.x + add, accel.min_v.x),
                            MAX(velocity.y + add, accel.min_v.y)
                        };
                    }
                    // determine a double-click
                } else {
                    velocity = cfg.velocity;
                }

                auto final = (dir * velocity * diff * to_s) + debt;
                auto final_int = static_cast<coords<int16_t>>(final);
                //still remember distance traveled even if no int delta occurs
                if (!final_int.magnitude()) {
                    debt = final;
                    return false;
                } else {
                    mouse.move( final_int );
                    debt = final - final_int;
                    return true;
                }
            }();

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