// MouseControlClient.hpp — usb-proxy 官方 mouse-control 薄客户端
// 只负责 MOVE 包编码与 cmd.sock 写入，不直接访问 HID/raw-gadget。
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ttbox::core::output {

struct MouseControlTelemetry {
    bool connected = false;
    uint64_t socket_write_ok = 0;
    uint64_t socket_write_fail = 0;
    uint64_t send_count = 0;
    int32_t last_dx = 0;
    int32_t last_dy = 0;
    int32_t last_wheel = 0;
    uint64_t last_timestamp_us = 0;
};

class MouseControlClient {
public:
    explicit MouseControlClient(std::string socket_path = "/run/orangepi-mouse-passthrough/cmd.sock")
        : socket_path_(std::move(socket_path)) {}
    ~MouseControlClient();

    MouseControlClient(const MouseControlClient&) = delete;
    MouseControlClient& operator=(const MouseControlClient&) = delete;

    static std::vector<uint8_t> encode_move(uint32_t request_id, int32_t dx, int32_t dy, int32_t wheel = 0);
    bool connect(std::string* error = nullptr);
    void disconnect();
    bool send_move(int32_t dx, int32_t dy, int32_t wheel = 0, std::string* error = nullptr);
    MouseControlTelemetry telemetry() const;
    bool connected() const { return fd_ >= 0; }
    uint32_t next_request_id() const { return next_request_id_; }

private:
    std::string socket_path_;
    int fd_ = -1;
    uint32_t next_request_id_ = 1;
    std::atomic<uint64_t> socket_write_ok_{0};
    std::atomic<uint64_t> socket_write_fail_{0};
    std::atomic<uint64_t> send_count_{0};
    std::atomic<int32_t> last_dx_{0};
    std::atomic<int32_t> last_dy_{0};
    std::atomic<int32_t> last_wheel_{0};
    std::atomic<uint64_t> last_timestamp_us_{0};
};

}  // namespace ttbox::core::output
