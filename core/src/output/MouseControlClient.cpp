// MouseControlClient.cpp — 官方 usb-proxy MOVE 包编码与发送
#include "output/MouseControlClient.hpp"

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#endif

namespace ttbox::core::output {
namespace {
void put_u16(std::vector<uint8_t>& out, uint16_t v) { out.push_back(static_cast<uint8_t>(v)); out.push_back(static_cast<uint8_t>(v >> 8)); }
void put_u32(std::vector<uint8_t>& out, uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8))); }
void put_i32(std::vector<uint8_t>& out, int32_t v) { put_u32(out, static_cast<uint32_t>(v)); }
}

MouseControlClient::~MouseControlClient() { disconnect(); }

std::vector<uint8_t> MouseControlClient::encode_move(uint32_t request_id, int32_t dx, int32_t dy, int32_t wheel) {
    std::vector<uint8_t> out;
    out.reserve(20);
    put_u16(out, 0x4F50);
    out.push_back(1);
    out.push_back(4);
    put_u32(out, request_id);
    put_i32(out, dx);
    put_i32(out, dy);
    put_i32(out, wheel);
    return out;
}

bool MouseControlClient::connect(std::string* error) {
#if defined(_WIN32)
    if (error) *error = "Windows 不支持 Unix SOCK_SEQPACKET";
    return false;
#else
    if (fd_ >= 0) return true;
    fd_ = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd_ < 0) { if (error) *error = std::strerror(errno); return false; }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
        if (error) *error = "socket 路径过长"; disconnect(); return false;
    }
    std::memcpy(addr.sun_path, socket_path_.c_str(), socket_path_.size() + 1);
    if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        if (error) *error = std::strerror(errno); disconnect(); return false;
    }
    return true;
#endif
}

MouseControlTelemetry MouseControlClient::telemetry() const {
    MouseControlTelemetry t;
    t.connected = fd_ >= 0;
    t.socket_write_ok = socket_write_ok_.load(std::memory_order_relaxed);
    t.socket_write_fail = socket_write_fail_.load(std::memory_order_relaxed);
    t.send_count = send_count_.load(std::memory_order_relaxed);
    t.last_dx = last_dx_.load(std::memory_order_relaxed);
    t.last_dy = last_dy_.load(std::memory_order_relaxed);
    t.last_wheel = last_wheel_.load(std::memory_order_relaxed);
    t.last_timestamp_us = last_timestamp_us_.load(std::memory_order_relaxed);
    return t;
}

void MouseControlClient::disconnect() {
#if !defined(_WIN32)
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#endif
}

bool MouseControlClient::send_move(int32_t dx, int32_t dy, int32_t wheel, std::string* error) {
#if defined(_WIN32)
    (void)dx; (void)dy; (void)wheel; if (error) *error = "Windows 不支持 Unix socket"; return false;
#else
    if (fd_ < 0 && !connect(error)) return false;
    const auto packet = encode_move(next_request_id_, dx, dy, wheel);
    const ssize_t n = ::send(fd_, packet.data(), packet.size(), MSG_NOSIGNAL);
    send_count_.fetch_add(1, std::memory_order_relaxed);
    if (n != static_cast<ssize_t>(packet.size())) {
        socket_write_fail_.fetch_add(1, std::memory_order_relaxed);
        if (error) *error = std::strerror(errno);
        disconnect();
        return false;
    }
    ++next_request_id_;
    if (next_request_id_ == 0) next_request_id_ = 1;
    socket_write_ok_.fetch_add(1, std::memory_order_relaxed);
    last_dx_.store(dx, std::memory_order_relaxed);
    last_dy_.store(dy, std::memory_order_relaxed);
    last_wheel_.store(wheel, std::memory_order_relaxed);
    last_timestamp_us_.store(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()), std::memory_order_relaxed);
    return true;
#endif
}

}  // namespace ttbox::core::output
