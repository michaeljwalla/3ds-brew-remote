#include "Objects.h"
#include "Mappings.h"
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
//

//InputGamepad
InputGamepad::InputGamepad(ObjectID id, std::string_view name):
    InputObject(id, name, InputTypes::GAMEPAD)
{}
void InputGamepad::init() {
    mapping = &get_mapping(InputTypes::GAMEPAD);
    os.spawn();

    os.enableKey(BTN_A);
    os.enableKey(BTN_B);
    os.enableKey(BTN_X);
    os.enableKey(BTN_Y);
    os.enableKey(BTN_TL);
    os.enableKey(BTN_TR);
    os.enableKey(BTN_SELECT);
    os.enableKey(BTN_START);
    os.enableKey(BTN_MODE);
    os.enableKey(BTN_THUMBL);
    os.enableKey(BTN_THUMBR);

    input_absinfo stick    { 0, -32768, 32767, 16, 128, 0 };
    input_absinfo trigger  { 0,      0,   255,  0,   0, 0 };
    input_absinfo hat      { 0,     -1,     1,  0,   0, 0 };

    os.enableAbsAxis(ABS_X,     stick);
    os.enableAbsAxis(ABS_Y,     stick);
    os.enableAbsAxis(ABS_Z,     trigger);
    os.enableAbsAxis(ABS_RX,    stick);
    os.enableAbsAxis(ABS_RY,    stick);
    os.enableAbsAxis(ABS_RZ,    trigger);
    os.enableAbsAxis(ABS_HAT0X, hat);
    os.enableAbsAxis(ABS_HAT0Y, hat);

    os.create("Microsoft X-Box 360 pad", 0x045e, 0x028e, 0x0110);
    return;
}

void InputGamepad::button_down(int btn) {
    os.emit(EV_KEY, btn, 1);
}
void InputGamepad::button_up(int btn) {
    os.emit(EV_KEY, btn, 0);
}
void InputGamepad::button_click(int btn) {
    os.emit(EV_KEY, btn, 1);
    os.sync(true);
    os.emit(EV_KEY, btn, 0);
    os.sync(true);
}
void InputGamepad::sync(bool force) {
    os.sync(force);
}

static int snap(float v) {
    return v > 0.0f ? 1 : (v < 0.0f ? -1 : 0);
}

void InputGamepad::set_joystick(Side which, coords<float> dir) {
    int axisX = (which == Side::LEFT) ? ABS_X  : ABS_RX;
    int axisY = (which == Side::LEFT) ? ABS_Y  : ABS_RY;
    os.emit(EV_ABS, axisX, static_cast<int>(dir.x * 32767));
    os.emit(EV_ABS, axisY, static_cast<int>(dir.y * 32767));
}
void InputGamepad::set_trigger(Side which, float amount) {
    int axis = (which == Side::LEFT) ? ABS_Z : ABS_RZ;
    os.emit(EV_ABS, axis, static_cast<int>(amount * 255));
}
void InputGamepad::set_dpad(coords<float> dir) {
    os.emit(EV_ABS, ABS_HAT0X, snap(dir.x));
    os.emit(EV_ABS, ABS_HAT0Y, snap(dir.y));
}