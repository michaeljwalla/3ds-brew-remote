// Cemuhook / DSU — Layers 2+3: the connection management class.
//
// Connection owns a DSU/Cemuhook UDP server socket and all protocol session
// state (subscriptions, per-slot packet counters, rumble feedback). It is
// move-only — it owns a socket file descriptor — exactly like the project's
// OSWrapper owns a uinput device.
//
// Usage mirrors os.emit()/os.sync(): build a ControllerData frame, hand it to
// send(); call poll() once per tick to service client handshakes. Nothing
// here is 3DS-aware — translating a device into a ControllerData is the
// caller's job (see DESIGN.md §9).
#pragma once

#include "Messages.h"
#include "Protocol.h"

#include <netinet/in.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace cemuhook {

inline constexpr uint8_t MAX_SLOTS = 4;

// A remote DSU client address. Copyable; wraps a sockaddr_in.
struct Endpoint {
    sockaddr_in addr{};
    bool        same_as(const Endpoint& other) const;
};

// Per-slot controller description — what INFO replies and DATA headers report.
struct SlotConfig {
    bool           present    = true;
    DeviceModel    model      = DeviceModel::FullGyro;
    ConnectionType connection = ConnectionType::Bluetooth;
    BatteryStatus  battery    = BatteryStatus::Full;
    uint64_t       mac        = 0;  // 0 -> auto-derived per slot
};

class Connection {
public:
    // Default construction yields a single-controller DSU server on the
    // standard port with a random server id. slot 0 starts connected.
    explicit Connection(uint16_t port = DSU_PORT);
    Connection(uint16_t port, uint32_t server_id);
    ~Connection();

    // move-only — owns the socket fd
    Connection(const Connection&)            = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    // Bind the UDP socket. Returns false if the socket could not be opened or
    // bound (e.g. the port is already in use). Safe to call when already open.
    bool open();
    void close();
    bool is_open() const { return sock_ >= 0; }

    uint32_t server_id() const { return server_id_; }
    uint16_t port() const { return port_; }
    size_t   subscriber_count() const { return subs_.size(); }

    // Configure a controller slot. Marking it not-present makes INFO replies
    // and DATA frames report it absent.
    void set_slot(uint8_t slot, const SlotConfig& cfg);
    // Motor count advertised in reply to a 0x110001 query (default 0).
    void set_motor_count(uint8_t count) { motor_count_ = count; }
    // Subscriber inactivity timeout (DSU spec recommends ~5 s).
    void set_timeout(std::chrono::milliseconds t) { timeout_ = t; }

    // Service every pending inbound datagram: answer version/info queries,
    // register or refresh data subscriptions, record rumble commands. Then
    // prune subscribers that have gone silent. Non-blocking.
    void poll();

    // Build and send one ControllerData frame for `slot` to every current
    // subscriber. The frame's header, connected flag, packet_number and (when
    // left 0) timestamp_us are filled from connection-owned state, so the
    // caller only sets buttons / sticks / motion / touch. No-op when the
    // socket is closed or there are no subscribers.
    void send(uint8_t slot, ControllerData frame);

    // Latest rumble intensity the client requested for (slot, motor); 0 if
    // none. The 3DS has no rumble motor — this is here for completeness.
    uint8_t rumble(uint8_t slot, uint8_t motor = 0) const;

private:
    struct Subscriber {
        Endpoint                              ep;
        ControllerId                          id;
        std::chrono::steady_clock::time_point last_seen;
    };

    void         handle(const ClientRequest& req, const Endpoint& from);
    void         send_raw(const std::vector<uint8_t>& bytes, const Endpoint& to);
    void         touch_subscriber(const Endpoint& from, const ControllerId& id);
    void         prune();
    SharedHeader header_for(uint8_t slot) const;
    bool         subscriber_wants(const Subscriber& s, const ControllerData& f) const;
    uint64_t     now_us() const;

    uint16_t                  port_;
    uint32_t                  server_id_;
    uint64_t                  base_mac_;
    int                       sock_;
    uint8_t                   motor_count_;
    std::chrono::milliseconds timeout_;

    std::array<SlotConfig, MAX_SLOTS> slots_;
    std::array<uint32_t, MAX_SLOTS>   packet_counter_;
    std::vector<Subscriber>           subs_;
    std::array<std::array<uint8_t, 2>, MAX_SLOTS> rumble_;  // [slot][motor]

    std::chrono::steady_clock::time_point start_time_;
};

}  // namespace cemuhook
