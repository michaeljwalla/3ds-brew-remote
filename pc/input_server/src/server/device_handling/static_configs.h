#pragma once
#include "static_inputs.h"

template<typename T>
struct Config;

namespace {
    coords<int> v_one {1, 1};
}

template<>
struct Config<InputMouse> {
    private:
    using fcoord = coords<float>;

    static constexpr float global_mul = 250;
    
    public:
    //a multiplier for moving the mouse
    fcoord speed = v_one * global_mul;
    struct accelerate {
        bool enabled = false;
        fcoord min_speed = v_one * global_mul;
        fcoord max_speed = max_speed * 2;
        float rate = global_mul;

        bool spring = false;
        float spring_dampen = 1;
        float spring_stiffness = 1;
    };
    

};

namespace config {
    template<typename T>
    Config<T> get() {
        static Config<T> inst;
        return inst;
    }
}