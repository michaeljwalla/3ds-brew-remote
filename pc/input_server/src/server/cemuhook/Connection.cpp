#include "Connection.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <random>

namespace cemuhook {

namespace {

uint32_t random_server_id() {
    std::random_device rd;
    return uint32_t(rd());
}

// std::visit helper
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace

bool Endpoint::same_as(const Endpoint& other) const {
    return addr.sin_addr.s_addr == other.addr.sin_addr.s_addr &&
           addr.sin_port == other.addr.sin_port;
}

// ── Construction / lifetime ────────────────────────────────────────────────
Connection::Connection(uint16_t port) : Connection(port, random_server_id()) {}

Connection::Connection(uint16_t port, uint32_t server_id)
    : port_(port),
      server_id_(server_id),
      base_mac_(0x665544000000ULL | (uint64_t(server_id) & 0xFFFFFFu)),
      sock_(-1),
      motor_count_(0),
      timeout_(std::chrono::seconds(5)),
      slots_{},
      packet_counter_{},
      subs_{},
      rumble_{},
      start_time_(std::chrono::steady_clock::now()) {
    // Default: a single controller in slot 0; slots 1..3 absent.
    for (uint8_t i = 1; i < MAX_SLOTS; ++i) slots_[i].present = false;
}

Connection::~Connection() { close(); }

Connection::Connection(Connection&& o) noexcept
    : port_(o.port_),
      server_id_(o.server_id_),
      base_mac_(o.base_mac_),
      sock_(o.sock_),
      motor_count_(o.motor_count_),
      timeout_(o.timeout_),
      slots_(o.slots_),
      packet_counter_(o.packet_counter_),
      subs_(std::move(o.subs_)),
      rumble_(o.rumble_),
      start_time_(o.start_time_) {
    o.sock_ = -1;
}

Connection& Connection::operator=(Connection&& o) noexcept {
    if (this != &o) {
        close();
        port_           = o.port_;
        server_id_      = o.server_id_;
        base_mac_       = o.base_mac_;
        sock_           = o.sock_;
        motor_count_    = o.motor_count_;
        timeout_        = o.timeout_;
        slots_          = o.slots_;
        packet_counter_ = o.packet_counter_;
        subs_           = std::move(o.subs_);
        rumble_         = o.rumble_;
        start_time_     = o.start_time_;
        o.sock_         = -1;
    }
    return *this;
}

// ── Socket ─────────────────────────────────────────────────────────────────
bool Connection::open() {
    if (sock_ >= 0) return true;

    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return false;

    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }

    sock_ = fd;
    return true;
}

void Connection::close() {
    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
    }
}

// ── Configuration ──────────────────────────────────────────────────────────
void Connection::set_slot(uint8_t slot, const SlotConfig& cfg) {
    if (slot < MAX_SLOTS) slots_[slot] = cfg;
}

uint8_t Connection::rumble(uint8_t slot, uint8_t motor) const {
    if (slot >= MAX_SLOTS || motor >= 2) return 0;
    return rumble_[slot][motor];
}

// ── Inbound ────────────────────────────────────────────────────────────────
void Connection::poll() {
    if (sock_ < 0) return;

    uint8_t buf[2048];
    for (;;) {
        sockaddr_in from{};
        socklen_t   from_len = sizeof(from);
        ssize_t     n = ::recvfrom(sock_, buf, sizeof(buf), MSG_DONTWAIT,
                                   reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n < 0) break;  // EAGAIN/EWOULDBLOCK/etc — socket drained
        if (size_t(n) < HEADER_SIZE) continue;

        Endpoint ep;
        ep.addr = from;
        if (auto req = parse_client_packet(buf, size_t(n))) handle(*req, ep);
    }

    prune();
}

void Connection::handle(const ClientRequest& req, const Endpoint& from) {
    std::visit(
        overloaded{
            [&](const VersionQuery&) {
                VersionResponse v;
                send_raw(frame_packet(server_id_, MsgType::Version, v.serialize()),
                         from);
            },
            [&](const PortInfoQuery& q) {
                for (uint8_t i = 0; i < q.count; ++i) {
                    uint8_t slot = q.slots[i];
                    if (slot >= MAX_SLOTS) continue;
                    PortInfo info;
                    info.header = header_for(slot);
                    send_raw(frame_packet(server_id_, MsgType::PortInfo,
                                          info.serialize()),
                             from);
                }
            },
            [&](const DataSubscription& s) { touch_subscriber(from, s.id); },
            [&](const MotorInfoQuery& q) {
                uint8_t   slot = q.id.slot < MAX_SLOTS ? q.id.slot : 0;
                MotorInfo m;
                m.header      = header_for(slot);
                m.motor_count = motor_count_;
                send_raw(frame_packet(server_id_, MsgType::MotorInfo,
                                      m.serialize()),
                         from);
            },
            [&](const RumbleCommand& c) {
                if (c.id.slot < MAX_SLOTS && c.motor < 2)
                    rumble_[c.id.slot][c.motor] = c.intensity;
            },
        },
        req);
}

void Connection::touch_subscriber(const Endpoint& from, const ControllerId& id) {
    auto now = std::chrono::steady_clock::now();
    for (auto& s : subs_) {
        if (s.ep.same_as(from)) {
            s.id        = id;
            s.last_seen = now;
            return;
        }
    }
    subs_.push_back(Subscriber{from, id, now});
}

void Connection::prune() {
    auto   now    = std::chrono::steady_clock::now();
    size_t before = subs_.size();
    std::erase_if(subs_, [&](const Subscriber& s) {
        return now - s.last_seen > timeout_;
    });
    // If every client is gone, drop any held rumble (DSU spec safety rule).
    if (subs_.empty() && before != 0)
        for (auto& slot : rumble_) slot = {};
}

// ── Outbound ───────────────────────────────────────────────────────────────
void Connection::send(uint8_t slot, ControllerData frame) {
    if (sock_ < 0 || slot >= MAX_SLOTS || subs_.empty()) return;

    frame.header        = header_for(slot);
    frame.connected     = slots_[slot].present;
    frame.packet_number = packet_counter_[slot]++;
    if (frame.timestamp_us == 0) frame.timestamp_us = now_us();

    auto pkt = frame_packet(server_id_, MsgType::Data, frame.serialize());
    for (const auto& s : subs_)
        if (subscriber_wants(s, frame)) send_raw(pkt, s.ep);
}

void Connection::send_raw(const std::vector<uint8_t>& bytes, const Endpoint& to) {
    if (sock_ < 0) return;
    ::sendto(sock_, bytes.data(), bytes.size(), MSG_DONTWAIT,
             reinterpret_cast<const sockaddr*>(&to.addr), sizeof(to.addr));
}

// ── Helpers ────────────────────────────────────────────────────────────────
SharedHeader Connection::header_for(uint8_t slot) const {
    const SlotConfig& c = slots_[slot];  // callers guarantee slot < MAX_SLOTS
    SharedHeader      h;
    h.slot       = slot;
    h.state      = c.present ? SlotState::Connected : SlotState::Disconnected;
    h.model      = c.model;
    h.connection = c.connection;
    h.battery    = c.battery;
    h.mac        = c.mac != 0 ? c.mac : (base_mac_ + (uint64_t(slot) << 8));
    return h;
}

bool Connection::subscriber_wants(const Subscriber& s,
                                  const ControllerData& f) const {
    switch (s.id.mode) {
        case SubscribeMode::All:  return true;
        case SubscribeMode::Slot: return s.id.slot == f.header.slot;
        case SubscribeMode::Mac:  return s.id.mac == f.header.mac;
    }
    return false;
}

uint64_t Connection::now_us() const {
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - start_time_)
                        .count());
}

}  // namespace cemuhook
