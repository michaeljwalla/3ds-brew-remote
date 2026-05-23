// Cemuhook / DSU — Layer 1: the message model.
//
// Plain structs whose every field has a sensible default, so a caller assigns
// only what changed. Covers both directions: outgoing builders (serialize())
// and an inbound parser (parse_client_packet()). Field offsets/orderings are
// defined by PROTOCOL.md.
#pragma once

#include "Protocol.h"

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace cemuhook {

// ── Field enums ────────────────────────────────────────────────────────────
enum class SlotState : uint8_t {
    Disconnected = 0,
    Reserved     = 1,
    Connected    = 2,
};

enum class DeviceModel : uint8_t {
    NA          = 0,
    PartialGyro = 1,
    FullGyro    = 2,
};

enum class ConnectionType : uint8_t {
    NA        = 0,
    USB       = 1,
    Bluetooth = 2,
};

enum class BatteryStatus : uint8_t {
    NA       = 0x00,
    Dying    = 0x01,
    Low      = 0x02,
    Medium   = 0x03,
    High     = 0x04,
    Full     = 0x05,
    Charging = 0xEE,
    Charged  = 0xEF,
};

// ── Shared 11-byte controller header (used by INFO and DATA payloads) ──────
struct SharedHeader {
    uint8_t        slot       = 0;
    SlotState      state      = SlotState::Connected;
    DeviceModel    model      = DeviceModel::FullGyro;
    ConnectionType connection = ConnectionType::Bluetooth;
    uint64_t       mac        = 0;  // 48-bit; 0 == not applicable
    BatteryStatus  battery    = BatteryStatus::Full;

    void serialize(ByteWriter& w) const;  // writes exactly 11 bytes
};

// ── One 6-byte touch point ─────────────────────────────────────────────────
struct TouchPoint {
    bool     active = false;
    uint8_t  id     = 0;  // stable for one continuous touch
    uint16_t x      = 0;
    uint16_t y      = 0;

    void serialize(ByteWriter& w) const;  // writes exactly 6 bytes
};

// ── Digital buttons ────────────────────────────────────────────────────────
// 16 bitmask buttons plus the two standalone button bytes. byte16()/byte17()
// encode the exact bit order from PROTOCOL.md.
struct ButtonState {
    // byte 16 (Buttons 1)
    bool dpad_left = false, dpad_down = false, dpad_right = false, dpad_up = false;
    bool options = false;  // a.k.a. Start
    bool r3 = false, l3 = false;
    bool share = false;    // a.k.a. Select
    // byte 17 (Buttons 2)
    bool y = false, b = false, a = false, x = false;
    bool r1 = false, l1 = false, r2 = false, l2 = false;
    // standalone bytes
    bool home         = false;  // byte 18
    bool touch_button = false;  // byte 19

    uint8_t byte16() const;
    uint8_t byte17() const;
};

// ── Full controller-data frame (0x100002 payload) ──────────────────────────
// Defaults describe a connected pad at rest: neutral sticks, no buttons, no
// touch, gravity on +Z.
struct ControllerData {
    SharedHeader header;

    bool        connected     = true;
    uint32_t    packet_number = 0;  // normally stamped by Connection::send()
    ButtonState buttons;

    // Analog sticks — neutral = 128. PROTOCOL.md: stick Y axes are +up.
    uint8_t left_stick_x  = 128, left_stick_y  = 128;
    uint8_t right_stick_x = 128, right_stick_y = 128;

    // Explicit analog button pressures (0..255). Used only when
    // derive_analog_from_digital == false.
    uint8_t analog_dpad_left = 0, analog_dpad_down = 0;
    uint8_t analog_dpad_right = 0, analog_dpad_up = 0;
    uint8_t analog_y = 0, analog_b = 0, analog_a = 0, analog_x = 0;
    uint8_t analog_r1 = 0, analog_l1 = 0, analog_r2 = 0, analog_l2 = 0;
    // When true (default), serialize() derives the 12 analog bytes from the
    // digital ButtonState (0 or 255). This is PROTOCOL.md's "Dolphin quirk".
    bool derive_analog_from_digital = true;

    TouchPoint touch1, touch2;

    uint64_t timestamp_us = 0;  // normally stamped by Connection::send() if 0
    float    accel_x = 0.0f, accel_y = 0.0f, accel_z = 1.0f;  // g
    float    gyro_pitch = 0.0f, gyro_yaw = 0.0f, gyro_roll = 0.0f;  // deg/s

    // Serialize to the exact 80-byte DATA payload.
    std::vector<uint8_t> serialize() const;
};

// ── Other outgoing message payloads ────────────────────────────────────────
struct VersionResponse {
    uint16_t             max_version = PROTOCOL_VERSION;
    std::vector<uint8_t> serialize() const;  // 2 bytes
};

struct PortInfo {
    SharedHeader         header;
    std::vector<uint8_t> serialize() const;  // 12 bytes (header + pad byte)
};

struct MotorInfo {
    SharedHeader         header;
    uint8_t              motor_count = 0;
    std::vector<uint8_t> serialize() const;  // 12 bytes
};

// ── Inbound client requests ────────────────────────────────────────────────
struct VersionQuery {};

struct PortInfoQuery {
    uint8_t               count = 0;  // number of valid entries in slots
    std::array<uint8_t, 4> slots{};
};

enum class SubscribeMode : uint8_t {
    All  = 0,
    Slot = 1,
    Mac  = 2,
};

// The 8-byte controller selector shared by the data subscription and the
// unofficial motor/rumble messages.
struct ControllerId {
    SubscribeMode mode = SubscribeMode::All;
    uint8_t       slot = 0;
    uint64_t      mac  = 0;
};

struct DataSubscription {
    ControllerId id;
};

struct MotorInfoQuery {
    ControllerId id;
};

struct RumbleCommand {
    ControllerId id;
    uint8_t      motor     = 0;
    uint8_t      intensity = 0;
};

using ClientRequest = std::variant<VersionQuery, PortInfoQuery, DataSubscription,
                                   MotorInfoQuery, RumbleCommand>;

// Parse a full inbound datagram into a typed request. Returns nullopt if the
// header is malformed or the message type is unknown.
std::optional<ClientRequest> parse_client_packet(const uint8_t* data, size_t len,
                                                 bool verify_crc = true);

}  // namespace cemuhook
