#pragma once
#include "static_inputs.h"

template<typename T>
struct Config;

template<>
struct Config<InputMouse> {
    private:
    using fcoord = coords<float>;
    
    public:
    //a multiplier for moving the mouse
    fcoord speed {5, 5};
    struct accelerate {
        bool enabled = false;
        fcoord min_speed {5, 5};
        fcoord max_speed {10, 10};
        float rate = 5;

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