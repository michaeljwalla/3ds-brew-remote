#include "Objects.h"
#include "Mappings.h"

#include <algorithm>
#include <cassert>
#include <cmath>
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

//InputCemuGamepad
namespace {
    // Translate a Linux BTN_* code into the matching DSU ButtonState bool.
    // Returns nullptr for codes the DSU protocol does not represent — the
    // caller silently drops them (we have no telemetry hook here yet).
    bool* dsu_button_field(cemuhook::ButtonState& b, int btn) {
        switch (btn) {
            case BTN_A:          return &b.a;
            case BTN_B:          return &b.b;
            case BTN_X:          return &b.x;
            case BTN_Y:          return &b.y;
            case BTN_TL:         return &b.l1;
            case BTN_TR:         return &b.r1;
            case BTN_TL2:        return &b.l2;
            case BTN_TR2:        return &b.r2;
            case BTN_SELECT:     return &b.share;     // a.k.a. Select
            case BTN_START:      return &b.options;   // a.k.a. Start
            case BTN_MODE:       return &b.home;      // PS / Home
            case BTN_THUMBL:     return &b.l3;
            case BTN_THUMBR:     return &b.r3;
            case BTN_DPAD_UP:    return &b.dpad_up;
            case BTN_DPAD_DOWN:  return &b.dpad_down;
            case BTN_DPAD_LEFT:  return &b.dpad_left;
            case BTN_DPAD_RIGHT: return &b.dpad_right;
            default: return nullptr;
        }
    }
    // Per-button analog pressure byte (12 of the 16 buttons have one — HOME,
    // SELECT, START, L3, R3 and the touch-click do not).
    uint8_t* dsu_analog_field(cemuhook::ControllerData& d, int btn) {
        switch (btn) {
            case BTN_DPAD_LEFT:  return &d.analog_dpad_left;
            case BTN_DPAD_DOWN:  return &d.analog_dpad_down;
            case BTN_DPAD_RIGHT: return &d.analog_dpad_right;
            case BTN_DPAD_UP:    return &d.analog_dpad_up;
            case BTN_Y:          return &d.analog_y;
            case BTN_B:          return &d.analog_b;
            case BTN_A:          return &d.analog_a;
            case BTN_X:          return &d.analog_x;
            case BTN_TR:         return &d.analog_r1;
            case BTN_TL:         return &d.analog_l1;
            case BTN_TR2:        return &d.analog_r2;
            case BTN_TL2:        return &d.analog_l2;
            default: return nullptr;
        }
    }
    // [-1, 1] float to a DSU stick byte. Neutral is 128.
    uint8_t dsu_stick_byte(float v) {
        long x = 128 + std::lround(v * 127.0f);
        return uint8_t(std::clamp<long>(x, 0, 255));
    }
    // [0, 1] float to a DSU analog-pressure byte.
    uint8_t dsu_pressure_byte(float amount) {
        long x = std::lround(std::clamp(amount, 0.0f, 1.0f) * 255.0f);
        return uint8_t(std::clamp<long>(x, 0, 255));
    }
}

InputCemuGamepad::InputCemuGamepad(ObjectID id, std::string_view name):
    InputGamepad(id, name)
{}

void InputCemuGamepad::init() {
    if (!con) {
        // No borrowed connection was set pre-init — own a fresh one.
        con_owned = std::make_unique<cemuhook::Connection>(port_pref_);
        con       = con_owned.get();
    }
    con->open();
    // Slot 0 is the default Connection slot; everything else starts absent.
    push_slot_cfg();
    mapping = &get_mapping(InputTypes::CEMU_GAMEPAD);
}

void InputCemuGamepad::use_connection(cemuhook::Connection* borrowed, uint8_t slot) {
    assert(!is_initialized() && "use_connection() must be called before init()");
    assert(borrowed != nullptr);
    con      = borrowed;
    slot_id_ = slot;
    // con_owned stays null — caller retains ownership.
}

void InputCemuGamepad::set_port(uint16_t port) {
    assert(!is_initialized() && "set_port() must be called before init()");
    port_pref_ = port;
}

void InputCemuGamepad::push_slot_cfg() {
    if (con) con->set_slot(slot_id_, slot_cfg_);
}

void InputCemuGamepad::set_slot(uint8_t slot) {
    slot_id_ = slot;
    push_slot_cfg();
}

// ── Standard InputGamepad surface ────────────────────────────────────────
void InputCemuGamepad::button_down(int btn) {
    if (auto* bf = dsu_button_field(frame_.buttons, btn)) *bf = true;
    // If the user has switched to explicit analog pressures, keep the
    // analog field consistent with the digital edge.
    if (!frame_.derive_analog_from_digital) {
        if (auto* af = dsu_analog_field(frame_, btn)) *af = 255;
    }
}
void InputCemuGamepad::button_up(int btn) {
    if (auto* bf = dsu_button_field(frame_.buttons, btn)) *bf = false;
    if (!frame_.derive_analog_from_digital) {
        if (auto* af = dsu_analog_field(frame_, btn)) *af = 0;
    }
}
void InputCemuGamepad::button_click(int btn) {
    button_down(btn);
    sync(true);
    button_up(btn);
    sync(true);
}
void InputCemuGamepad::set_joystick(Side which, coords<float> dir) {
    if (which == Side::LEFT) {
        frame_.left_stick_x  = dsu_stick_byte(dir.x);
        frame_.left_stick_y  = dsu_stick_byte(dir.y);
    } else {
        frame_.right_stick_x = dsu_stick_byte(dir.x);
        frame_.right_stick_y = dsu_stick_byte(dir.y);
    }
}
void InputCemuGamepad::set_dpad(coords<float> dir) {
    int dx = snap(dir.x);
    int dy = snap(dir.y);
    frame_.buttons.dpad_left  = (dx < 0);
    frame_.buttons.dpad_right = (dx > 0);
    frame_.buttons.dpad_up    = (dy < 0);
    frame_.buttons.dpad_down  = (dy > 0);
    if (!frame_.derive_analog_from_digital) {
        frame_.analog_dpad_left  = frame_.buttons.dpad_left  ? 255 : 0;
        frame_.analog_dpad_right = frame_.buttons.dpad_right ? 255 : 0;
        frame_.analog_dpad_up    = frame_.buttons.dpad_up    ? 255 : 0;
        frame_.analog_dpad_down  = frame_.buttons.dpad_down  ? 255 : 0;
    }
}
void InputCemuGamepad::set_trigger(Side which, float amount) {
    int btn = (which == Side::LEFT) ? BTN_TL2 : BTN_TR2;
    if (auto* bf = dsu_button_field(frame_.buttons, btn)) *bf = amount > 0.0f;
    // set_analog_pressure flips derive_analog_from_digital to false on its
    // first call and back-fills the other 11 bytes, so the trigger's true
    // pressure transmits without losing the digital state of any other
    // button.
    set_analog_pressure(btn, amount);
}
void InputCemuGamepad::sync(bool /*force*/) {
    if (!con) return;
    // DSU is stateless re-emit — every sync sends the current frame
    con->send(slot_id_, frame_);
}

// ── Cemuhook-specific extensions ─────────────────────────────────────────
void InputCemuGamepad::set_gyro(float pitch, float yaw, float roll) {
    frame_.gyro_pitch = pitch;
    frame_.gyro_yaw   = yaw;
    frame_.gyro_roll  = roll;
}
void InputCemuGamepad::set_accel(float x, float y, float z) {
    frame_.accel_x = x;
    frame_.accel_y = y;
    frame_.accel_z = z;
}
void InputCemuGamepad::set_touch(int index, bool active,
                                 uint16_t x, uint16_t y, uint8_t id) {
    cemuhook::TouchPoint& tp = (index == 0) ? frame_.touch1 : frame_.touch2;
    tp.active = active;
    tp.x      = x;
    tp.y      = y;
    tp.id     = id;
}
void InputCemuGamepad::set_home(bool down) {
    frame_.buttons.home = down;
}
void InputCemuGamepad::set_touch_button(bool down) {
    frame_.buttons.touch_button = down;
}
void InputCemuGamepad::set_analog_pressure(int btn, float amount) {
    if (frame_.derive_analog_from_digital) {
        // Switching modes — capture the current digital→analog mapping for
        // every other button so they don't all snap to 0 when we stop
        // deriving. After this back-fill, individual bytes can be set
        // freely without losing the rest.
        auto B = [](bool on) -> uint8_t { return on ? 255 : 0; };
        const auto& b = frame_.buttons;
        frame_.analog_dpad_left  = B(b.dpad_left);
        frame_.analog_dpad_down  = B(b.dpad_down);
        frame_.analog_dpad_right = B(b.dpad_right);
        frame_.analog_dpad_up    = B(b.dpad_up);
        frame_.analog_y          = B(b.y);
        frame_.analog_b          = B(b.b);
        frame_.analog_a          = B(b.a);
        frame_.analog_x          = B(b.x);
        frame_.analog_r1         = B(b.r1);
        frame_.analog_l1         = B(b.l1);
        frame_.analog_r2         = B(b.r2);
        frame_.analog_l2         = B(b.l2);
        frame_.derive_analog_from_digital = false;
    }
    if (auto* af = dsu_analog_field(frame_, btn))
        *af = dsu_pressure_byte(amount);
}
void InputCemuGamepad::clear_analog_pressures() {
    frame_.derive_analog_from_digital = true;
}

// ── Slot / device descriptor ─────────────────────────────────────────────
void InputCemuGamepad::set_battery(cemuhook::BatteryStatus b) {
    slot_cfg_.battery = b;
    push_slot_cfg();
}
void InputCemuGamepad::set_connection_type(cemuhook::ConnectionType c) {
    slot_cfg_.connection = c;
    push_slot_cfg();
}
void InputCemuGamepad::set_device_model(cemuhook::DeviceModel m) {
    slot_cfg_.model = m;
    push_slot_cfg();
}
void InputCemuGamepad::set_mac(uint64_t mac) {
    slot_cfg_.mac = mac;
    push_slot_cfg();
}
void InputCemuGamepad::set_connected(bool connected) {
    slot_cfg_.present = connected;
    frame_.connected  = connected;  // Connection::send will overwrite, but
                                    // keep the local frame self-consistent.
    push_slot_cfg();
}

// ── Per-frame metadata ──────────────────────────────────────────────────
void InputCemuGamepad::set_packet_number(uint32_t n) { frame_.packet_number = n; }
void InputCemuGamepad::set_timestamp_us(uint64_t us) { frame_.timestamp_us  = us; }

// ── Connection plumbing ─────────────────────────────────────────────────
void InputCemuGamepad::poll() {
    if (con) con->poll();
}
uint8_t InputCemuGamepad::rumble(uint8_t motor) const {
    return con ? con->rumble(slot_id_, motor) : 0;
}
size_t InputCemuGamepad::subscriber_count() const {
    return con ? con->subscriber_count() : 0;
}

//InputCemuRelay3DS
InputCemuRelay3DS::InputCemuRelay3DS(ObjectID id, std::string_view name):
    InputCemuGamepad(id, name)
{}

void InputCemuRelay3DS::init() {
    // Base does the heavy lifting (own / open the Connection, push the
    // slot descriptor, set mapping = CEMU_GAMEPAD). We then swap the
    // mapping for the relay-specific one — CEMU_RELAY_3DS — so any
    // overrides registered in available take precedence over the base.
    InputCemuGamepad::init();
    mapping = &get_mapping(InputTypes::CEMU_RELAY_3DS);
}

// translate() lives in Mappings.cpp next to gamepad::button_table and
// gamepad::deadzone — both are file-local helpers in that translation
// unit and the relay's translation reuses them.
