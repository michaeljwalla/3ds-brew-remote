#include "Messages.h"

#include <cassert>

namespace cemuhook {

// ── Shared header ──────────────────────────────────────────────────────────
void SharedHeader::serialize(ByteWriter& w) const {
    w.u8(slot);
    w.u8(uint8_t(state));
    w.u8(uint8_t(model));
    w.u8(uint8_t(connection));
    w.u48(mac);
    w.u8(uint8_t(battery));
}

// ── Touch point ────────────────────────────────────────────────────────────
void TouchPoint::serialize(ByteWriter& w) const {
    w.u8(active ? 1 : 0);
    w.u8(id);
    w.u16(x);
    w.u16(y);
}

// ── Buttons ────────────────────────────────────────────────────────────────
uint8_t ButtonState::byte16() const {
    return uint8_t((dpad_left << 7) | (dpad_down << 6) | (dpad_right << 5) |
                   (dpad_up << 4) | (options << 3) | (r3 << 2) | (l3 << 1) |
                   (share << 0));
}

uint8_t ButtonState::byte17() const {
    return uint8_t((y << 7) | (b << 6) | (a << 5) | (x << 4) | (r1 << 3) |
                   (l1 << 2) | (r2 << 1) | (l2 << 0));
}

// ── Controller data frame ──────────────────────────────────────────────────
std::vector<uint8_t> ControllerData::serialize() const {
    ByteWriter w;
    header.serialize(w);                  // [0..10]
    w.u8(connected ? 1 : 0);              // [11]
    w.u32(packet_number);                 // [12..15]
    w.u8(buttons.byte16());               // [16]
    w.u8(buttons.byte17());               // [17]
    w.u8(buttons.home ? 1 : 0);           // [18]
    w.u8(buttons.touch_button ? 1 : 0);   // [19]
    w.u8(left_stick_x);                   // [20]
    w.u8(left_stick_y);                   // [21]
    w.u8(right_stick_x);                  // [22]
    w.u8(right_stick_y);                  // [23]

    if (derive_analog_from_digital) {     // [24..35]
        auto a = [](bool on) -> uint8_t { return on ? 255 : 0; };
        w.u8(a(buttons.dpad_left));
        w.u8(a(buttons.dpad_down));
        w.u8(a(buttons.dpad_right));
        w.u8(a(buttons.dpad_up));
        w.u8(a(buttons.y));
        w.u8(a(buttons.b));
        w.u8(a(buttons.a));
        w.u8(a(buttons.x));
        w.u8(a(buttons.r1));
        w.u8(a(buttons.l1));
        w.u8(a(buttons.r2));
        w.u8(a(buttons.l2));
    } else {
        w.u8(analog_dpad_left);
        w.u8(analog_dpad_down);
        w.u8(analog_dpad_right);
        w.u8(analog_dpad_up);
        w.u8(analog_y);
        w.u8(analog_b);
        w.u8(analog_a);
        w.u8(analog_x);
        w.u8(analog_r1);
        w.u8(analog_l1);
        w.u8(analog_r2);
        w.u8(analog_l2);
    }

    touch1.serialize(w);                  // [36..41]
    touch2.serialize(w);                  // [42..47]
    w.u64(timestamp_us);                  // [48..55]
    w.f32(accel_x);                       // [56..59]
    w.f32(accel_y);                       // [60..63]
    w.f32(accel_z);                       // [64..67]
    w.f32(gyro_pitch);                    // [68..71]
    w.f32(gyro_yaw);                      // [72..75]
    w.f32(gyro_roll);                     // [76..79]

    assert(w.size() == DATA_PAYLOAD_SIZE);
    return w.take();
}

// ── Small outgoing payloads ────────────────────────────────────────────────
std::vector<uint8_t> VersionResponse::serialize() const {
    ByteWriter w;
    w.u16(max_version);
    return w.take();
}

std::vector<uint8_t> PortInfo::serialize() const {
    ByteWriter w;
    header.serialize(w);
    w.pad(1);  // trailing 0x00
    return w.take();
}

std::vector<uint8_t> MotorInfo::serialize() const {
    ByteWriter w;
    header.serialize(w);
    w.u8(motor_count);
    return w.take();
}

// ── Inbound parsing ────────────────────────────────────────────────────────
namespace {

// Decode the 8-byte controller selector: flags(1) + slot(1) + MAC(6).
ControllerId read_controller_id(ByteReader& r) {
    ControllerId id;
    id.mode = static_cast<SubscribeMode>(r.u8());
    id.slot = r.u8();
    id.mac  = r.u48();
    return id;
}

}  // namespace

std::optional<ClientRequest> parse_client_packet(const uint8_t* data, size_t len,
                                                 bool verify_crc) {
    auto hdr = parse_header(data, len, verify_crc);
    if (!hdr) return std::nullopt;

    ByteReader r(data + hdr->payload_offset, hdr->payload_size);

    switch (hdr->type) {
        case MsgType::Version:
            return ClientRequest{VersionQuery{}};

        case MsgType::PortInfo: {
            PortInfoQuery q;
            uint32_t      n = r.u32();
            if (!r.ok()) return std::nullopt;
            if (n > 4) n = 4;
            q.count = uint8_t(n);
            for (uint8_t i = 0; i < q.count; ++i) q.slots[i] = r.u8();
            if (!r.ok()) return std::nullopt;
            return ClientRequest{q};
        }

        case MsgType::Data: {
            DataSubscription s;
            s.id = read_controller_id(r);
            if (!r.ok()) return std::nullopt;
            return ClientRequest{s};
        }

        case MsgType::MotorInfo: {
            MotorInfoQuery q;
            q.id = read_controller_id(r);
            if (!r.ok()) return std::nullopt;
            return ClientRequest{q};
        }

        case MsgType::Rumble: {
            RumbleCommand c;
            c.id        = read_controller_id(r);
            c.motor     = r.u8();
            c.intensity = r.u8();
            if (!r.ok()) return std::nullopt;
            return ClientRequest{c};
        }
    }
    return std::nullopt;  // unknown message type
}

}  // namespace cemuhook
