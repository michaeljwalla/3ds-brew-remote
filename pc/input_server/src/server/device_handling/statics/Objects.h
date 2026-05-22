#pragma once

#include "datatypes.h"
#include "server/device_handling/Inputs.h"

class InputMouse: public InputObject {
    using coordinate = coords<int16_t>;
    coordinate pos;
    InputMouse(ObjectID id, std::string_view name);
    
    void init() override;
    public:
        void button_down(int btn);
        void button_up(int btn);
        // this fires syncs immediately (others do not)
        // since button down/up have to be in different batches to register properly
        void button_click(int btn);
        void move(coordinate delta);
        //1 = 1 line
        void scroll(coordinate delta);
        //120 = 1 line
        void scroll_smooth(coordinate delta);
        void set_pos(coordinate newPos);
        coordinate get_pos() const;

        // flush
        void sync(bool force = false);

    friend class InputController;

};