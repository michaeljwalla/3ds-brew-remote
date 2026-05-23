#include "Protocol.h"

#include <cstring>

namespace cemuhook {

uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return ~crc;
}

// ── ByteWriter ─────────────────────────────────────────────────────────────
void ByteWriter::u8(uint8_t v) { buf_.push_back(v); }

void ByteWriter::u16(uint16_t v) {
    u8(uint8_t(v));
    u8(uint8_t(v >> 8));
}

void ByteWriter::u32(uint32_t v) {
    for (int i = 0; i < 4; ++i) u8(uint8_t(v >> (8 * i)));
}

void ByteWriter::u64(uint64_t v) {
    for (int i = 0; i < 8; ++i) u8(uint8_t(v >> (8 * i)));
}

void ByteWriter::u48(uint64_t v) {
    for (int i = 0; i < 6; ++i) u8(uint8_t(v >> (8 * i)));
}

void ByteWriter::f32(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    u32(bits);
}

void ByteWriter::raw(const uint8_t* p, size_t n) {
    buf_.insert(buf_.end(), p, p + n);
}

void ByteWriter::pad(size_t n) { buf_.insert(buf_.end(), n, uint8_t{0}); }

// ── ByteReader ─────────────────────────────────────────────────────────────
uint8_t ByteReader::u8() {
    if (!has(1)) {
        ok_ = false;
        return 0;
    }
    return data_[pos_++];
}

uint16_t ByteReader::u16() {
    uint16_t a = u8();
    uint16_t b = u8();
    return uint16_t(a | (b << 8));
}

uint32_t ByteReader::u32() {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t(u8()) << (8 * i);
    return v;
}

uint64_t ByteReader::u64() {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(u8()) << (8 * i);
    return v;
}

uint64_t ByteReader::u48() {
    uint64_t v = 0;
    for (int i = 0; i < 6; ++i) v |= uint64_t(u8()) << (8 * i);
    return v;
}

float ByteReader::f32() {
    uint32_t bits = u32();
    float    v;
    std::memcpy(&v, &bits, 4);
    return v;
}

void ByteReader::skip(size_t n) {
    if (!has(n)) {
        ok_ = false;
        return;
    }
    pos_ += n;
}

// ── Framing ────────────────────────────────────────────────────────────────
std::vector<uint8_t> frame_packet(uint32_t server_id, MsgType type,
                                  const std::vector<uint8_t>& payload) {
    ByteWriter w;
    w.raw(reinterpret_cast<const uint8_t*>(MAGIC_SERVER), 4);  // [0..3]
    w.u16(PROTOCOL_VERSION);                                   // [4..5]
    w.u16(uint16_t(payload.size() + 4));  // [6..7]  length excludes 16B header
    w.u32(0);                             // [8..11] CRC placeholder
    w.u32(server_id);                     // [12..15]
    w.u32(uint32_t(type));                // [16..19]
    w.raw(payload.data(), payload.size());

    std::vector<uint8_t> pkt = w.take();
    // CRC covers the whole packet with bytes 8..11 held at zero.
    uint32_t c = crc32(pkt.data(), pkt.size());
    pkt[8]  = uint8_t(c);
    pkt[9]  = uint8_t(c >> 8);
    pkt[10] = uint8_t(c >> 16);
    pkt[11] = uint8_t(c >> 24);
    return pkt;
}

std::optional<InboundHeader> parse_header(const uint8_t* data, size_t len,
                                          bool verify_crc) {
    if (len < HEADER_SIZE) return std::nullopt;
    if (std::memcmp(data, MAGIC_CLIENT, 4) != 0) return std::nullopt;

    ByteReader r(data, len);
    r.skip(4);             // magic
    r.u16();               // protocol version — accept any; only 1001 is known
    uint16_t length     = r.u16();
    uint32_t stored_crc = r.u32();
    uint32_t client_id  = r.u32();
    uint32_t raw_type   = r.u32();
    if (!r.ok()) return std::nullopt;

    // The length field counts the message type (4) + payload, i.e. everything
    // after byte 16. The full packet is therefore 16 + length bytes long.
    if (length < 4 || size_t(length) + 16 > len) return std::nullopt;

    if (verify_crc) {
        std::vector<uint8_t> tmp(data, data + len);
        tmp[8] = tmp[9] = tmp[10] = tmp[11] = 0;
        if (crc32(tmp.data(), tmp.size()) != stored_crc) return std::nullopt;
    }

    InboundHeader h;
    h.type           = static_cast<MsgType>(raw_type);
    h.client_id      = client_id;
    h.payload_offset = HEADER_SIZE;
    h.payload_size   = size_t(length) - 4;
    return h;
}

}  // namespace cemuhook
