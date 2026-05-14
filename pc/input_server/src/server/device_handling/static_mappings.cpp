//for statics
#include "mappings.h"
#include "static_mappings.h"
#include "server/protocol.h"
#include "static_inputs.h"
#include "static_configs.h"
#include "logger.h"
#include <chrono>
#include <cstdint>
#include <linux/input-event-codes.h>
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
    using time_point = clock::time_point;

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

    template<typename F>
    class FireAfterScope {
        F call;
    public:
        explicit FireAfterScope(F f) : call{std::move(f)} {}
        ~FireAfterScope() { call(); }
    };

    // deduction guide
    template<typename F>
    FireAfterScope(F) -> FireAfterScope<F>;

    template<typename T>
    class UpdateAfterScope{
        private:
            T& upd;
            T value;
        public:
        explicit UpdateAfterScope(T& to_update, T with_value): upd{to_update}, value{with_value} {}
        ~UpdateAfterScope() {
            upd = std::move(value);
            return;
        }
    };
    template<typename T>
    UpdateAfterScope(T&, T) -> UpdateAfterScope<T>;
    
    template<typename T>
    T& as(void* x) { return *static_cast<T*>(x); }

    static auto& global_settings = config::get<TotalInputMask>();
}

namespace mouse {
    static auto& cfg = config::get<InputMouse>();
    namespace time {
        static auto cur = clock::now();
        static auto last = cur;
    }
    namespace touchpad {
        static time_point last_touch{time::cur}, last_touch_release{time::cur};
    }
    namespace calculations {
        static coords<float> debt{0,0};
        static coords<float> velocity{0, 0};
        static auto last_success = time::cur;
        static bool last_scrolled = false;

        bool get_direction(const RawInput& data, coords<float>& out) {
            coords<float> dir;
            auto& buttons = data.buttons;
            if (global_settings.circle_pads.main.enabled) {
                auto dz = global_settings.circle_pads.dead_zone;
                auto cp = data.circle_pad;
                dir = coords<float>{
                    IGNORE(*cp, dz, 0) + -IGNORE(-*cp, dz, 0),           //works for if dir is neg/pos (one is always dropped)
                    -((IGNORE(*(cp+1), dz, 0) + -IGNORE(-*(cp+1), dz, 0))),
                };
                if (dir.magnitude()) {
                    std::exchange(out, dir);
                    return true;
                }
            }
            if (global_settings.circle_pads.pro.enabled) {
                auto dz = global_settings.circle_pads.dead_zone;
                auto cp = data.circle_pad_pro;
                dir = coords<float>{
                    IGNORE(*cp, dz, 0) + IGNORE(-*cp, dz, 0),           //works for if dir is neg/pos (one is always dropped)
                    -((IGNORE(*(cp+1), dz, 0) + IGNORE(-*(cp+1), dz, 0))),
                };
                if (dir.magnitude()) {
                    std::exchange(out, dir);
                    return true;
                }
            }
            //
            dir = coords<int>{
                (ON(Buttons::LEFT, buttons) ? -1 : 0) + (ON(Buttons::RIGHT, buttons) ? 1 : 0),
                -((ON(Buttons::DOWN, buttons) ? -1 : 0) + (ON(Buttons::UP, buttons) ? 1 : 0))
            }.normalized();
            if (dir.magnitude()) {
                std::exchange(out,dir);
                return true;
            }
            return false;
        }

        bool get_delta(
            const RawInput& data,
            const coords<float>& dir,
            const coords<float> scale,
            coords<int16_t>& out)
        {
            using namespace time;

            float ms_to_s = 1e-3;
            auto buttons = data.buttons;
            auto raw_diff = GET_DELTA_MS(last, cur);
            auto diff = CAP_DELTA_MS(raw_diff, 1000);
            auto accel = cfg.accelerate;
            auto delta_last_success = GET_DELTA_MS(last_success, cur);

            //if (!dir.magnitude()) return {0, 0};
            if (accel.enabled && delta_last_success > accel.preserve_ms) velocity *= 0;
            last_success = last;

            //only accel up, not down (insta stop)
            if (accel.enabled && !ON(accel.temp_disable, buttons)) {
                float dv = accel.rate * diff * ms_to_s;
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
            } else {
                velocity = cfg.velocity;
            }

            //calculate losses from integer floor
            auto final = scale * (dir * velocity * diff * ms_to_s) + debt;
            auto final_int = static_cast<coords<int16_t>>(final);
            //still remember distance traveled even if no int delta occurs
            if (!final_int.magnitude()) {
                debt = std::move(final);
                return false;
            } else {
                debt = std::move(final - final_int);
                out = std::move(final_int);
                return true;
            }
        }
    }

}

std::unordered_map<InputTypes, InputMap<InputObject>> available {
pair(InputTypes::MOUSE, {"MOUSE", {
//left click
{Options::FIRST, [](Params data) {
    using namespace mouse::time;
    cur = clock::now();
    return 0;
}},
{Options::L, [](Params data) {
    auto buttons = data.code.buttons;
    auto& mouse = as<InputMouse>(data.parent);

    if ( ON(Buttons::L, buttons) ) mouse.button_down( BTN_LEFT );
    else mouse.button_up( BTN_LEFT );
    return 0;
}},
{Options::A, [](Params data) {
    auto buttons = data.code.buttons;
    auto& mouse = as<InputMouse>(data.parent);

    if ( ON(Buttons::A, buttons) ) mouse.button_down( BTN_LEFT );
    else mouse.button_up( BTN_LEFT );
    return 0;
}},

//right click
{Options::R, [](Params data) {
    auto buttons = data.code.buttons;
    auto& mouse = as<InputMouse>(data.parent);

    if ( ON(Buttons::R, buttons) ) mouse.button_down( BTN_RIGHT );
    else mouse.button_up( BTN_RIGHT );
    return 0;
}},
{Options::B, [](Params data) {
    auto buttons = data.code.buttons;
    auto& mouse = as<InputMouse>(data.parent);

    if ( ON(Buttons::B, buttons) ) mouse.button_down( BTN_RIGHT );
    else mouse.button_up( BTN_RIGHT );
    return 0;
}},

{Options::X, [](Params data) {
    static auto last_press = clock::now();

    auto& mouse = as<InputMouse>(data.parent);
    auto buttons = data.code.buttons;
    auto& cfg = config::get<InputMouse>();

    bool pressed = ON(ButtonMask::X, buttons);
    auto cur = clock::now();

    if (pressed) {
        last_press = cur;
    } else if (GET_DELTA_MS(last_press, cur) < cfg.scroll.is_click_threshold_ms) {
        // release within threshold -> treat as middle click instead of scroll-toggle
        mouse.button_click(BTN_MIDDLE);
    }
    return 0;
} },
{Options::TOUCH, [](Params data) {
    using namespace mouse::touchpad;
    using namespace mouse::time;
    
    auto& mouse = as<InputMouse>(data.parent);
    auto codes = data.code;
    auto& cfg = config::get<InputMouse>();


    bool touched = codes.touch_active;
    (touched ? last_touch : last_touch_release) = cur;

    if (touched) {
        // on touch, reset cursor to center (since we only track relative movement)
        mouse.set_pos({0,0});
    }
    return 0;
}},
//mouse movement
{Options::ALWAYS, [](Params data) {
    //each lambda tracks its own last to prevent diff accumulation
    
    float to_s = 0.001;
    auto& mouse = as<InputMouse>(data.parent);
    auto buttons = data.code.buttons;
    auto& cfg = config::get<InputMouse>();
    auto& cfg_generics = config::get<TotalInputMask>();
    
    //calculate direction/movement
    //y axis flipped since origin is top left
    bool moved = [&](){
        using namespace mouse;
        namespace C = calculations;
        //
        // cur/last are advanced by Options::FIRST / Options::LAST handlers;
        // we just read them here.
        auto scroll = cfg.scroll;
        bool  is_scrolling = scroll.enabled && ON(scroll.enable_key, buttons);
        if (is_scrolling ^ C::last_scrolled) C::velocity = {0,0}; // reset (accel'd v) on state change
        auto _ = FireAfterScope([is_scrolling]() {
            C::last_scrolled = is_scrolling;
        });

        // direction to move
        coords<float> dir;
        if (!C::get_direction(data.code, dir)) {
            return false;
        }
        *(data.log) << 1;

        coords<float> scale = ((is_scrolling) ?
            coords<float>(scroll.dampener, -scroll.dampener) * (scroll.smooth ? 120 : 1)
            : static_cast<coords<float>>(v_one)
        );

        // amount to move
        coords<int16_t> delta;
        if (
            !(C::get_delta(data.code, dir, scale, delta)
            && delta.magnitude())
        ) return false;

        //
        *(data.log) << delta;
        if (is_scrolling) {
            scroll.smooth ? mouse.scroll_smooth(delta) : mouse.scroll_smooth(delta);
        } else {
            mouse.move(delta);
        }

        return true;
    }();

    return 0;
}},
{Options::LAST, [](Params data) {
    using namespace mouse::time;
    last = cur;
    as<InputMouse>(data.parent).sync();
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