#pragma once
#include "static_inputs.h"
#include "server/protocol.h"

template<typename T>
struct Config {};

namespace {
    coords<int> v_one {1, 1};
    constexpr float EXPECTED_DELTA_MS = 1000.0/REFRESH_RATE;
}

template<>
struct Config<TotalInputMask> {
    private:
        struct touchpad {
            //magnitude of movement (0 to 1) generally considered to ignore
            float dead_zone = 0.05; 
            float dead_zone_delta = 0.005;
        };

        struct circle_pads {
            float dead_zone = 0.1;
            struct main {
                bool enabled = true;
            };
            struct pro {
                bool enabled = false;
            };
            main main;
            pro pro;
        };
    public:
        circle_pads circle_pads;
        touchpad touchpad;
};

template<>
struct Config<InputMouse> {
    private:
    using fcoord = coords<float>;

    static constexpr float global_mul = 250;
    struct touchpad {
        bool enabled = true;

        float rate = global_mul;
        float focus_scale = 0.25;
        
        bool scale_speed = true;
        //delta dead zone (below this = linear to min_rate)
        float min_delta = 0.1;
        float min_v = rate/2;

        //delta
        float max_delta = 0.6;
        float max_v = rate*2;
        int preserve_ms = EXPECTED_DELTA_MS*REFRESH_RATE/4; //will reapply cur velocity if moved within this threshold

        //time after entry to look for release
        float tap_release_ms = EXPECTED_DELTA_MS*5;
        //time after release to look for re-entry (detect double taps)
        float tap_reenter_ms = EXPECTED_DELTA_MS*10;
    };

    struct accel {

        bool enabled = true;
        fcoord min_v = v_one * global_mul;
        fcoord max_v = v_one * global_mul * 3;
        float rate = global_mul * 1.5; // /2 is 0.5 mul per s^2
        int preserve_ms = EXPECTED_DELTA_MS*REFRESH_RATE/4; //will reapply cur velocity if moved within this threshold

        ButtonMask temp_disable = ButtonMask::Y;

        float double_tap_add = 2*global_mul; // stop-starting quickly speeds up
        float start_double_tap_ms = EXPECTED_DELTA_MS*5; //so holding down doesnt do this
        float end_double_tap_ms = EXPECTED_DELTA_MS*10;

        bool spring = false;
        float spring_dampen = 1;
        float spring_stiffness = 1;
    };
    struct scroll {
        bool enabled = true;
        bool smooth = true;
        ButtonMask enable_key = ButtonMask::X;

        //any time mouse movement is captured with ON(...mouse_switch)
        //will attempt to scroll L/R/U/D
        float dampener = 0.05;
        //treat release within x ms as a middle-click
        float is_click_threshold_ms = 200;

    };
    public:
    //a multiplier for moving the mouse
    fcoord velocity = v_one * global_mul;
    accel accelerate;
    scroll scroll;
    touchpad touchpad;
    

};

namespace config {
    template<typename T>
    Config<T>& get() {
        static Config<T> inst;
        return inst;
    }
}