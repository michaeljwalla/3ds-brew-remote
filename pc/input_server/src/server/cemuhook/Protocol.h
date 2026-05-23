// Cemuhook / DSU (DualShock UDP) — Layer 0: wire primitives.
//
// Self-contained: depends only on the C++ standard library. Knows nothing
// about controllers — only bytes, CRC, and packet framing. See PROTOCOL.md
// and DESIGN.md in this directory for the full contract.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace cemuhook {

// ── Wire constants ─────────────────────────────────────────────────────────
inline constexpr char     MAGIC_SERVER[4]   = {'D', 'S', 'U', 'S'};
inline constexpr char     MAGIC_CLIENT[4]   = {'D', 'S', 'U', 'C'};
inline constexpr uint16_t PROTOCOL_VERSION  = 1001;
inline constexpr uint16_t DSU_PORT          = 26760;
inline constexpr size_t   HEADER_SIZE       = 20;  // 16-byte header + msg type
inline constexpr size_t   DATA_PAYLOAD_SIZE = 80;  // see PROTOCOL.md accounting

enum class MsgType : uint32_t {
    Version   = 0x100000,
    PortInfo  = 0x100001,
    Data      = 0x100002,
    MotorInfo = 0x110001,  // unofficial extension
    Rumble    = 0x110002,  // unofficial extension
};

// ── CRC32 ──────────────────────────────────────────────────────────────────
// Standard CRC32, reversed polynomial 0xEDB88320 (zlib / PNG / Ethernet).
uint32_t crc32(const uint8_t* data, size_t len);

// ── Little-endian byte writer ──────────────────────────────────────────────
// Every numeric is appended least-significant-byte-first regardless of host
// endianness — see PROTOCOL.md "Endianness Trap".
class ByteWriter {
public:
    void u8(uint8_t v);
    void u16(uint16_t v);
    void u32(uint32_t v);
    void u64(uint64_t v);
    void u48(uint64_t v);  // 48-bit MAC address
    void f32(float v);
    void raw(const uint8_t* p, size_t n);
    void pad(size_t n);    // append n zero bytes

    size_t                      size()  const { return buf_.size(); }
    const std::vector<uint8_t>& bytes() const { return buf_; }
    std::vector<uint8_t>        take()        { return std::move(buf_); }

private:
    std::vector<uint8_t> buf_;
};

// ── Little-endian byte reader (bounds-checked) ─────────────────────────────
// Any read past the end sets ok() to false and yields 0; callers check ok()
// once after a group of reads rather than per-read.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t len) : data_(data), len_(len) {}

    uint8_t  u8();
    uint16_t u16();
    uint32_t u32();
    uint64_t u64();
    uint64_t u48();
    float    f32();
    void     skip(size_t n);

    bool   ok()        const { return ok_; }
    bool   has(size_t n) const { return ok_ && pos_ + n <= len_; }
    size_t remaining() const { return pos_ <= len_ ? len_ - pos_ : 0; }

private:
    const uint8_t* data_;
    size_t         len_;
    size_t         pos_ = 0;
    bool           ok_  = true;
};

// ── Packet framing ─────────────────────────────────────────────────────────
// Wrap an already-serialized payload into a complete "DSUS" packet: prepend
// the 20-byte header, fill the length field, then compute and write the CRC32.
std::vector<uint8_t> frame_packet(uint32_t server_id, MsgType type,
                                  const std::vector<uint8_t>& payload);

// Parsed view of a validated inbound packet header.
struct InboundHeader {
    MsgType  type;
    uint32_t client_id;
    size_t   payload_offset;  // always HEADER_SIZE on success
    size_t   payload_size;    // bytes of payload after the header
};

// Validate an inbound "DSUC" packet — magic, declared length, and (when
// verify_crc is set) the CRC32. Returns nullopt on anything malformed.
std::optional<InboundHeader> parse_header(const uint8_t* data, size_t len,
                                          bool verify_crc = true);

}  // namespace cemuhook
