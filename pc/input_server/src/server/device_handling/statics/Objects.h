#pragma once

#include "datatypes.h"
#include "server/device_handling/Inputs.h"
#include "server/cemuhook/CemuhookUDP.h"

#include <memory>

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

class InputGamepad: public InputObject {
    protected:
        InputGamepad(ObjectID id, std::string_view name);

    virtual void init() override;
    public:
        enum class Side {
            LEFT, RIGHT
        };
        virtual void button_down(int btn);
        virtual void button_up(int btn);
        // this fires syncs immediately (others do not)
        // since button down/up have to be in different batches to register properly
        virtual void button_click(int btn);
        // dir components in [-1.0, 1.0]
        virtual void set_joystick(Side which, coords<float> dir);
        // dir components snapped to {-1, 0, 1}
        virtual void set_dpad(coords<float> dir);
        // amount in [0.0, 1.0]
        virtual void set_trigger(Side which, float amount);

        // flush
        virtual void sync(bool force = false);

    friend class InputController;

};

// The base exposes the DSU controller surface
// (buttons, sticks, triggers, dpad, gyro, accel, two touch points, system
// buttons, per-button analog pressure, slot/battery/connection descriptor)
// and leaves device-specific translation to the mapping 

// tried to keep it similarly structured to linux virutal input lib
class InputCemuGamepad: public InputGamepad {
    protected:
        InputCemuGamepad(ObjectID id, std::string_view name);
        std::unique_ptr<cemuhook::Connection> con_owned;
        cemuhook::Connection* con = nullptr;

        // The frame being built up between sync() flushes
        cemuhook::ControllerData frame_;
        // Local copy of this controller's slot descriptor — pushed to the
        // connection on every change so PortInfo replies and Data headers
        // stay consistent with what we send.
        cemuhook::SlotConfig     slot_cfg_{};
        // Which DSU slot (0..3) this object publishes as
        uint8_t                  slot_id_ = 0;
        // Bind port for the default-owned connection. Honored only by the
        // first init() call when no borrowed connection has been set.
        uint16_t                 port_pref_ = cemuhook::DSU_PORT;

        void init() override;

        // Push slot_cfg_ to the connection (no-op before init()).
        void push_slot_cfg();

    public:
        void use_connection(cemuhook::Connection* borrowed, uint8_t slot = 0);

        // Must be called before init(); ignored once a connection exists.
        void set_port(uint16_t port);
        // Set this controller's slot id (0..3). Safe to call before or after
        // init(); the new slot's descriptor is pushed to the connection.
        void set_slot(uint8_t slot);
        uint8_t slot() const { return slot_id_; }

        // btn is a Linux BTN_* code. Codes with no DSU equivalent are
        // silently dropped (return code TBD if telemetry is needed).
        void button_down(int btn) override;
        void button_up(int btn) override;
        
        void button_click(int btn) override;

        // dir components in [-1.0, 1.0]; mapped to bytes via 128 + v*127.
        void set_joystick(Side which, coords<float> dir) override;
        // dir components snapped to {-1, 0, 1}.
        void set_dpad(coords<float> dir) override;

        void set_trigger(Side which, float amount) override;
        // Broadcast the current frame to every subscriber.
        void sync(bool force = false) override;

        // RawInput → setters translation, called by the CEMU_GAMEPAD
        virtual void translate(const RawInput& code) { (void)code; }

        void set_gyro(float pitch, float yaw, float roll);
        void set_accel(float x, float y, float z);

        // DSU exposes two simultaneous touch points (index 0 or 1). `id` is
        // the stable identifier for one continuous contact — bump it when
        // the finger lifts and a fresh press starts so clients can tell
        // them apart.
        void set_touch(int index, bool active,
                       uint16_t x = 0, uint16_t y = 0, uint8_t id = 0);

        // System buttons not covered by the InputGamepad surface. The HOME
        // / PS button also has BTN_MODE as its Linux equivalent, so
        // button_down(BTN_MODE) sets the same flag; set_home is provided
        // for readability. The touch-button has no canonical BTN_*.
        void set_home(bool down);
        void set_touch_button(bool down);

        void set_analog_pressure(int btn, float amount);
        void clear_analog_pressures();

        // These mutate the local SlotConfig and push it to the connection
        // immediately, so the next PortInfo reply reports the new state.
        void set_battery(cemuhook::BatteryStatus b);
        void set_connection_type(cemuhook::ConnectionType c);
        void set_device_model(cemuhook::DeviceModel m);
        // MAC of 0 (the default) tells Connection to derive one from its
        // server_id and the slot index — distinct per slot, stable per run.
        void set_mac(uint64_t mac);
        // false reports the slot as disconnected in both PortInfo replies
        // and outbound Data headers; the frame's `connected` flag follows.
        void set_connected(bool connected);

        // ── Per-frame metadata escape hatches ─────────────────────────────
        // Connection::send stamps both fields if left at 0, so callers
        // normally leave these alone. Use to forward a hardware timestamp
        // or a counter from an upstream source.
        void set_packet_number(uint32_t n);
        void set_timestamp_us(uint64_t us);

        // ── Connection plumbing ───────────────────────────────────────────
        // Service every pending inbound datagram (handshakes, port-info
        // queries, data subscriptions, rumble commands). Non-blocking;
        // intended to be called once per fire-loop tick from the mapping
        // (typically the FIRST slot, before per-input handlers run).
        void poll();
        // Latest rumble intensity the client requested for (this slot,
        // motor); 0 if none. Motor index 0 or 1.
        uint8_t rumble(uint8_t motor = 0) const;
        size_t  subscriber_count() const;

        // ── Escape hatches for advanced callers ───────────────────────────
        cemuhook::ControllerData&       frame()       { return frame_; }
        const cemuhook::ControllerData& frame() const { return frame_; }
        cemuhook::Connection*           connection()  { return con; }
        const cemuhook::Connection*     connection() const { return con; }

    friend class InputController;

};

// 3DS-specific relay: overrides translate() to decode a 3DS RawInput
// (ButtonMask + circle pads + gyro + accel + touch) into the DSU frame
// the base class broadcasts. Uses the CEMU_RELAY_3DS mapping, which is
// CEMU_GAMEPAD extended (no overrides yet) — demonstrating the pattern
// so future cemu derivatives (Wii remote, DualShock, ...) can add per-slot
// handlers without rewriting the base.
class InputCemuRelay3DS : public InputCemuGamepad {
    protected:
        InputCemuRelay3DS(ObjectID id, std::string_view name);

        void init() override;
        void translate(const RawInput& code) override;

        // Touch ID bookkeeping. DSU's TouchPoint.id is the stable
        // identifier for one continuous contact, so it bumps on every
        // press edge to let clients distinguish consecutive contacts.
        bool    prev_touch_ = false;
        uint8_t touch_id_   = 0;

    friend class InputController;

};
