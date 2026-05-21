#include "static_inputs.h"
#include "static_mappings.h"
#include <linux/input-event-codes.h>


//InputMouse
InputMouse::InputMouse(ObjectID id, std::string_view name):
    InputObject(id, name, InputTypes::MOUSE),
    pos()
{}

void InputMouse::init() {
    mapping = &get_mapping(InputTypes::MOUSE);
    os.spawn();
    os.enableKey(BTN_LEFT);
    os.enableKey(BTN_RIGHT);
    os.enableKey( BTN_MIDDLE );
    os.enableRelAxis(REL_X);
    os.enableRelAxis(REL_Y);
    os.enableRelAxis( REL_WHEEL );
    os.enableRelAxis( REL_HWHEEL );
    os.enableRelAxis( REL_WHEEL_HI_RES );
    os.enableRelAxis( REL_HWHEEL_HI_RES );
    os.create(name);
    return;
}
void InputMouse::button_click(int btn) {
    os.emit(EV_KEY, btn, 1);
    os.sync(true);
    os.emit(EV_KEY, btn, 0);
    os.sync(true);
}
void InputMouse::button_down(int btn) {
    os.emit(EV_KEY, btn, 1);
    ;
}
void InputMouse::button_up(int btn) {
    os.emit(EV_KEY, btn, 0);
    ;
}
void InputMouse::move(InputMouse::coordinate delta) {
    if (!delta.magnitude()) return;
    pos += delta;
    os.emit(EV_REL, REL_X, delta.x);
    os.emit(EV_REL, REL_Y, delta.y);
    ;
}
void InputMouse::scroll(InputMouse::coordinate delta) {
    if (!delta.magnitude()) return;
    os.emit(EV_REL, REL_HWHEEL, delta.x);
    os.emit(EV_REL, REL_WHEEL, delta.y);
    ;
}
void InputMouse::scroll_smooth(InputMouse::coordinate delta) {
    if (!delta.magnitude()) return;
    os.emit(EV_REL, REL_HWHEEL_HI_RES, delta.x);
    os.emit(EV_REL, REL_WHEEL_HI_RES, delta.y);
    ;
}
void InputMouse::set_pos(InputMouse::coordinate newPos) {
    move(newPos - pos);
}
void InputMouse::sync(bool force) {
    os.sync(force);
    return;
}

InputMouse::coordinate InputMouse::get_pos() const{
    return pos;
}