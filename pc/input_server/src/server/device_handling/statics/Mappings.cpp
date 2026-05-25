//for statics
#include "server/device_handling/InputMap.h"
#include "Mappings.h"
#include "server/protocol.h"
#include "Objects.h"
#include "Configs.h"
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
        static time_point    last_touch{time::cur}, last_touch_release{time::cur};
        static uint16_t      consec_taps {0};
        static coords<float> last_touch_pos {0, 0};
        static bool          currently_pressed {false};
        static bool          is_drag {false};

        // returns <consec_taps, currently_pressed>; call only from TOUCH diff slot.
        std::pair<int, bool> determine_input(const RawInput& code) {
            auto& tp = config::get<InputMouse>().touchpad;
            bool pressed_now = code.touch_active != 0;

            if (pressed_now && !currently_pressed) {
                last_touch       = time::cur;
                last_touch_pos   = {code.touch[0], code.touch[1]};
                is_drag          = false;
                currently_pressed = true;
            } else if (!pressed_now && currently_pressed) {
                last_touch_release = time::cur;
                currently_pressed  = false;
                auto dur = GET_DELTA_MS(last_touch, time::cur);
                if (!is_drag && dur < tp.tap_release_ms) ++consec_taps;
                else consec_taps = 0;
            }
            return {consec_taps, currently_pressed};
        }

        // call every frame from ALWAYS; fires click once tap_reenter_ms elapses.
        void flush_pending_taps(InputMouse& mouse) {
            auto& tp = config::get<InputMouse>().touchpad;
            if (currently_pressed || consec_taps == 0) return;
            if (GET_DELTA_MS(last_touch_release, time::cur) <= tp.tap_reenter_ms) return;
            int btn = (consec_taps == 1) ? BTN_LEFT
                    : (consec_taps == 2) ? BTN_RIGHT
                    : BTN_MIDDLE;
            mouse.button_click(btn);
            consec_taps = 0;
        }

        // returns dir + mag; gates below dead_zone_delta (no debt); updates last_touch_pos.
        bool get_direction(const RawInput& code, coords<float>& dir_out, float& mag_out) {
            auto pos = coords<float>{code.touch[0], code.touch[1]};
            auto raw = pos - last_touch_pos;
            last_touch_pos = pos;
            auto mag = raw.magnitude();
            if (mag < global_settings.touchpad.dead_zone_delta) return false;
            dir_out = raw.normalized();
            mag_out = mag;
            return true;
        }

        float compute_velocity(float input_mag) {
            auto& tp = config::get<InputMouse>().touchpad;
            if (!tp.scale_speed) return tp.rate * input_mag;
            if (input_mag <= 0)            return 0;
            if (input_mag <= tp.min_delta) return tp.min_v * (input_mag / tp.min_delta);
            if (input_mag >= tp.max_delta) return tp.max_v;
            float t = (input_mag - tp.min_delta) / (tp.max_delta - tp.min_delta);
            return tp.min_v + t * (tp.max_v - tp.min_v);
        }
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
// /controller
namespace gamepad {
    using Side = InputGamepad::Side;

    struct BtnPair { ButtonMask src; int evdev; };
    constexpr BtnPair button_table[] = {
        {Buttons::A, BTN_A}, {Buttons::B, BTN_B},
        {Buttons::X, BTN_X}, {Buttons::Y, BTN_Y},
        {Buttons::L, BTN_TL}, {Buttons::R, BTN_TR},
        {Buttons::SELECT, BTN_SELECT}, {Buttons::START, BTN_START},
    };

    // zero a stick component within the shared circle-pad dead zone
    float deadzone(float v, float dz) { return (v <= dz && v >= -dz) ? 0.f : v; }

    void apply(InputGamepad& gp, const RawInput& code) {
        auto b = code.buttons;

        for (auto& [src, evdev] : button_table)
            ON(src, b) ? gp.button_down(evdev) : gp.button_up(evdev);

        // d-pad hat: evdev convention is down = +1, up = -1
        gp.set_dpad(coords<float>{
            static_cast<float>((ON(Buttons::RIGHT, b) ? 1 : 0) - (ON(Buttons::LEFT, b) ? 1 : 0)),
            static_cast<float>((ON(Buttons::DOWN,  b) ? 1 : 0) - (ON(Buttons::UP,   b) ? 1 : 0)),
        });

        // 3DS ZL/ZR are digital -> 0 or full
        gp.set_trigger(Side::LEFT,  ON(Buttons::ZL, b) ? 1.f : 0.f);
        gp.set_trigger(Side::RIGHT, ON(Buttons::ZR, b) ? 1.f : 0.f);

        // sticks: negate Y (3DS up is +Y; evdev ABS_Y is down-positive)
        auto dz = global_settings.circle_pads.dead_zone;
        gp.set_joystick(Side::LEFT, coords<float>{
             deadzone(code.circle_pad[0], dz),
            -deadzone(code.circle_pad[1], dz),
        });
        if (global_settings.circle_pads.pro.enabled) {
            gp.set_joystick(Side::RIGHT, coords<float>{
                 deadzone(code.circle_pad_pro[0], dz),
                -deadzone(code.circle_pad_pro[1], dz),
            });
        }
    }
}

// InputCemuRelay3DS::translate lives here so it can reuse gamepad::button_table
// and gamepad::deadzone — both are file-local to this TU. DSU convention
// differs from evdev's: PROTOCOL.md says stick Y axes are +up (same as the
// 3DS), so we do NOT negate Y the way gamepad::apply does for evdev.
void InputCemuRelay3DS::translate(const RawInput& code) {
    using Side = InputGamepad::Side;
    auto b = code.buttons;

    // Face / shoulder / system buttons via the shared 3DS→evdev BTN_* table;
    // InputCemuGamepad::button_down/up then maps BTN_* to the DSU bool.
    for (auto& [src, btn] : gamepad::button_table)
        ON(src, b) ? button_down(btn) : button_up(btn);

    // D-pad: 3DS sends digital bits → {-1, 0, 1}² → set_dpad's four bools.
    set_dpad(coords<float>{
        static_cast<float>((ON(Buttons::RIGHT, b) ? 1 : 0) - (ON(Buttons::LEFT, b) ? 1 : 0)),
        static_cast<float>((ON(Buttons::DOWN,  b) ? 1 : 0) - (ON(Buttons::UP,   b) ? 1 : 0)),
    });

    // ZL/ZR are digital on the 3DS → full-pressure DSU L2/R2 triggers.
    set_trigger(Side::LEFT,  ON(Buttons::ZL, b) ? 1.0f : 0.0f);
    set_trigger(Side::RIGHT, ON(Buttons::ZR, b) ? 1.0f : 0.0f);

    // Sticks — both sender and receiver are +up, so pass Y straight through.
    auto dz = global_settings.circle_pads.dead_zone;
    set_joystick(Side::LEFT, coords<float>{
        gamepad::deadzone(code.circle_pad[0], dz),
        gamepad::deadzone(code.circle_pad[1], dz),
    });
    if (global_settings.circle_pads.pro.enabled) {
        set_joystick(Side::RIGHT, coords<float>{
            gamepad::deadzone(code.circle_pad_pro[0], dz),
            gamepad::deadzone(code.circle_pad_pro[1], dz),
        });
    }

    // Motion. Units already match the DSU wire: gyro deg/s, accel g.
    // Axis mapping is index-aligned: 3DS gyro[0/1/2] ↔ DSU pitch/yaw/roll,
    // empirically verified against Azahar + padtest. The 3DS source's
    // gyro_x/y/z = roll/pitch/yaw labels (3ds/source/main.cpp:17-19) are
    // dev nomenclature for a different coordinate system; the underlying
    // *physical* axes happen to line up index-for-index with DSU's. Only
    // the pitch axis needs sign inversion (nose-up on the 3DS read as
    // nose-down in-game otherwise). Accel handedness is still unverified
    // per DESIGN.md §10.3.
    set_gyro(-code.gyro[0], code.gyro[1], code.gyro[2]);
    set_accel(code.accel[0], code.accel[1], code.accel[2]);

    // Touch. The 3DS side normalises the touch position to [0, 1]
    // (px/319, py/239) before sending — see 3ds/source/main.cpp:271 — so
    // we have to scale BACK up to a meaningful integer range before
    // handing it to DSU's uint16 fields. Native 3DS pixel range (×319,
    // ×239) is the honest choice: it preserves every pixel the 3DS can
    // physically resolve, and the four corners land at (0,0), (319,0),
    // (0,239), (319,239) for client calibration UIs to pin against.
    // (Bug history: casting the normalised float straight to uint16
    // collapsed every tap to {0, 1}², which "passed" the top-left
    // calibration step at (0,0) but stuck every subsequent corner on
    // top of it.) Touch ID bumps on each press edge per DESIGN.md §9.4.
    bool now_touching = code.touch_active != 0;
    if (now_touching && !prev_touch_) ++touch_id_;
    prev_touch_ = now_touching;
    set_touch(0, now_touching,
              static_cast<uint16_t>(code.touch[0] * 319.0f),
              static_cast<uint16_t>(code.touch[1] * 239.0f),
              touch_id_);
    // The touchpad-position fields above are the *surface* — clients like
    // Azahar's "Use controller touchpad" read them to drive the cursor.
    // The touch-button bit (byte 19) is the *click* — what tap-bound game
    // actions actually fire on. On the 3DS the screen IS the click (no
    // hover state on a resistive screen), so mirror touch_active straight
    // into it. If a future subclass wants real tap-vs-drag discrimination
    // (à la mouse::touchpad::determine_input + flush_pending_taps), lift
    // that state machine into the subclass — its statics are mouse-local
    // and not safe to call from here.
    set_touch_button(now_touching);
}

// Shared keymap for every cemu-derived device. Returns a fresh map each
// call so two slots in `available` (CEMU_GAMEPAD and CEMU_RELAY_3DS extended
// from it) can be initialised in the same initializer list without
// referring to each other. The per-tick translation is virtual on the
// object — both poll() and translate() dispatch through the runtime type
// of the parent InputCemuGamepad*.
namespace {
    InputMap<IO> make_cemu_gamepad_base() {
        return InputMap<IO>{"CEMU_GAMEPAD", {
            {Options::FIRST,  [](Params data) { as<InputCemuGamepad>(data.parent).poll();             return 0; }},
            {Options::ALWAYS, [](Params data) { as<InputCemuGamepad>(data.parent).translate(data.code); return 0; }},
            {Options::LAST,   [](Params data) { as<InputCemuGamepad>(data.parent).sync();             return 0; }},
        }};
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
    if (!config::get<InputMouse>().touchpad.enabled) return 0;
    mouse::touchpad::determine_input(data.code);
    return 0;
}},
//mouse movement
{Options::ALWAYS, [](Params data) {
    using namespace mouse;
    namespace C = calculations;
    namespace TP = touchpad;

    auto& mouse = as<InputMouse>(data.parent);
    auto buttons = data.code.buttons;
    auto& cfg = config::get<InputMouse>();

    TP::flush_pending_taps(mouse);

    auto scroll = cfg.scroll;
    bool is_scrolling = scroll.enabled && ON(scroll.enable_key, buttons);
    if (is_scrolling ^ C::last_scrolled) C::velocity = {0,0};
    auto _ = FireAfterScope([is_scrolling]() { C::last_scrolled = is_scrolling; });

    coords<float> scale = (is_scrolling)
        ? coords<float>(scroll.dampener, -scroll.dampener) * (scroll.smooth ? 120 : 1)
        : static_cast<coords<float>>(v_one);

    coords<int16_t> delta;
    bool moved = false;

    //touchpad logic
    if (cfg.touchpad.enabled && data.code.touch_active && TP::currently_pressed) {
        coords<float> dir;
        float mag;
        if (TP::get_direction(data.code, dir, mag)) {
            float v = TP::compute_velocity(mag);
            if (ON(Buttons::Y, buttons)) v *= cfg.touchpad.focus_scale;
            auto raw = scale * (dir * v) + C::debt;
            auto raw_int = static_cast<coords<int16_t>>(raw);
            if (raw_int.magnitude()) {
                C::debt = raw - raw_int;
                delta   = raw_int;
                moved   = true;
                raw_int *= coords<float>(1, /*-2*static_cast<int>(is_scrolling) +*/ 1);
                //drop tap logic, sustained drop would likely be jitter
                TP::is_drag     = true;
                TP::consec_taps = 0;
            } else {
                C::debt = raw;
            }
        }
    } else { //button logic (circle pad, dpad)
        coords<float> dir;
        moved = C::get_direction(data.code, dir)
            && C::get_delta(data.code, dir, scale, delta) //assigns delta here
            && delta.magnitude();
    }

    if (moved) (is_scrolling) ? mouse.scroll_smooth(delta) : mouse.move(delta);

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
pair(InputTypes::GAMEPAD, {"GAMEPAD", {
{Options::ALWAYS, [](Params data) {
    gamepad::apply(as<InputGamepad>(data.parent), data.code);
    return 0;
}},
{Options::LAST, [](Params data) {
    as<InputGamepad>(data.parent).sync();
    return 0;
}}
}}),
pair(InputTypes::CEMU_GAMEPAD, make_cemu_gamepad_base()),
// CEMU_RELAY_3DS: the base piped onto itself with no overrides yet. The
// right-hand of extend() is the new mapping's slot handlers; any slot
// the right side doesn't define is preserved from the base. Adding a
// per-slot override (e.g. a LOGGING-style A/B/X/Y hook for debug, or a
// custom LAST that prunes subscribers manually) means adding entries to
// the right-hand initializer below — they will override the base.
pair(InputTypes::CEMU_RELAY_3DS,
     make_cemu_gamepad_base().extend("CEMU_RELAY_3DS", { /* overrides go here */ })),
};

InputMap<InputObject>& get_mapping(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error(std::string("no mapping for type ") + getInputTypeName(type));
    return it->second;
}