// MakcuMouseBackend.cpp — MAKCU 官方代理协议实现
#include "output/MakcuMouseBackend.hpp"

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

#include <chrono>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

namespace ttbox::core::output {
namespace {
constexpr uint16_t kMagic = 0x4F50;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kPingReq = 1;
constexpr uint8_t kPingResp = 2;
constexpr uint8_t kErrorResp = 3;
constexpr uint8_t kMoveCmd = 4;

#pragma pack(push, 1)
struct PacketHeader {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t request_id;
};
#pragma pack(pop)

uint32_t next_id(uint32_t* value) {
    const uint32_t id = (*value)++;
    if (*value == 0) *value = 1;
    return id == 0 ? next_id(value) : id;
}
}

bool MakcuMouseBackend::connect(std::string* error) {
#if defined(_WIN32)
    if (error) *error = "Windows 不支持 Unix SOCK_SEQPACKET";
    health_.state = BackendState::kError;
    health_.detail = "unsupported platform";
    return false;
#else
    if (fd_ >= 0) return true;
    health_.state = BackendState::kConnecting;
    fd_ = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd_ < 0) {
        health_.state = BackendState::kError;
        health_.detail = std::strerror(errno);
        if (error) *error = "创建 MAKCU socket 失败: " + health_.detail;
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
        if (error) *error = "MAKCU socket 路径过长";
        ::close(fd_); fd_ = -1;
        health_.state = BackendState::kError;
        health_.detail = "socket path too long";
        return false;
    }
    std::memcpy(addr.sun_path, socket_path_.c_str(), socket_path_.size() + 1);
    if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        health_.state = BackendState::kError;
        health_.detail = std::strerror(errno);
        if (error) *error = "连接 MAKCU socket 失败: " + health_.detail;
        ::close(fd_); fd_ = -1;
        return false;
    }
    health_.state = BackendState::kConnected;
    health_.detail = "connected";
    return true;
#endif
}

void MakcuMouseBackend::disconnect() {
#if !defined(_WIN32)
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#endif
    health_.state = BackendState::kDisconnected;
    health_.detail = "disconnected";
}

bool MakcuMouseBackend::reconnect(std::string* error) {
    disconnect();
    ++health_.reconnect_count;
    return connect(error);
}

BackendHealth MakcuMouseBackend::health() const { return health_; }

bool MakcuMouseBackend::send_packet(uint8_t type, uint32_t request_id,
                                    const void* payload, size_t payload_size,
                                    std::string* error) {
#if defined(_WIN32)
    (void)type; (void)request_id; (void)payload; (void)payload_size;
    if (error) *error = "Windows 不支持 Unix socket";
    return false;
#else
    if (fd_ < 0) {
        if (error) *error = "MAKCU socket 未连接";
        return false;
    }
    std::string packet(sizeof(PacketHeader) + payload_size, '\0');
    PacketHeader header{kMagic, kVersion, type, request_id};
    std::memcpy(packet.data(), &header, sizeof(header));
    if (payload_size != 0) std::memcpy(packet.data() + sizeof(header), payload, payload_size);
    const ssize_t sent = ::send(fd_, packet.data(), packet.size(), MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(packet.size())) {
        if (error) *error = std::strerror(errno);
        return false;
    }
    return true;
#endif
}

bool MakcuMouseBackend::ping(std::string* error) {
#if defined(_WIN32)
    (void)error; return false;
#else
    const uint32_t id = next_id(&next_request_id_);
    if (!send_packet(kPingReq, id, nullptr, 0, error)) return false;
    unsigned char response[256]{};
    const ssize_t n = ::recv(fd_, response, sizeof(response), 0);
    if (n < static_cast<ssize_t>(sizeof(PacketHeader))) {
        if (error) *error = "MAKCU ping 响应过短";
        return false;
    }
    PacketHeader header{};
    std::memcpy(&header, response, sizeof(header));
    if (header.magic != kMagic || header.version != kVersion ||
        header.request_id != id || header.type != kPingResp) {
        if (error) *error = "MAKCU ping 响应不匹配";
        return false;
    }
    return true;
#endif
}

bool MakcuMouseBackend::send_move(int32_t dx, int32_t dy, int32_t wheel, std::string* error) {
#if defined(_WIN32)
    (void)dx; (void)dy; (void)wheel; (void)error; return false;
#else
    struct MovePayload { int32_t dx; int32_t dy; int32_t wheel; } payload{dx, dy, wheel};
    return send_packet(kMoveCmd, next_id(&next_request_id_), &payload, sizeof(payload), error);
#endif
}

bool MakcuMouseBackend::mouse_move(int32_t dx, int32_t dy, int32_t wheel) {
    if (!gate_allows()) { ++health_.send_fail; return false; }
    std::string error;
    if (fd_ < 0 && !connect(&error)) { ++health_.send_fail; return false; }
    if (!send_move(dx, dy, wheel, &error)) {
        health_.detail = error;
        ++health_.send_fail;
        return false;
    }
    ++health_.send_ok;
    health_.last_send_ok_us = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return true;
}

bool MakcuMouseBackend::mouse_button(uint8_t, uint8_t) {
    // 本阶段只接入移动命令；按钮仍由物理鼠标/代理管理。
    return gate_allows();
}

bool MakcuMouseBackend::mouse_click(uint8_t button) {
    return mouse_button(button, kActClick);
}

}  // namespace ttbox::core::output
