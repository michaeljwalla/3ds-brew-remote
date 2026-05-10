#include "static_inputs.h"
#include "static_mappings.h"
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
    os.enableRelAxis(REL_X);
    os.enableRelAxis(REL_Y);
    os.create(name);
    return;
}
void InputMouse::button_click(int btn) {
    os.emit(EV_KEY, btn, 1); os.sync();
    os.emit(EV_KEY, btn, 0); os.sync();
}
void InputMouse::button_down(int btn) {
    os.emit(EV_KEY, btn, 1); os.sync();
}
void InputMouse::button_up(int btn) {
    os.emit(EV_KEY, btn, 0); os.sync();
}
void InputMouse::move(InputMouse::coordinate delta) {
    pos += delta;
    os.emit(EV_REL, REL_X, delta.x);
    os.emit(EV_REL, REL_Y, delta.y);
    os.sync();
}
void InputMouse::set_pos(InputMouse::coordinate newPos) {
    move(newPos - pos);
}

InputMouse::coordinate InputMouse::get_pos() const{
    return pos;
}