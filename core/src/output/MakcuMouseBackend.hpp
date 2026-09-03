// MakcuMouseBackend.hpp — MAKCU 官方代理输出后端
// 协议来源：板端 /opt/usb-proxy/bin/usb-proxy-mouse-client
#pragma once

#include "output/OutputBackend.hpp"
#include <string>

namespace ttbox::core::output {

class MakcuMouseBackend final : public IOutputBackend {
public:
    explicit MakcuMouseBackend(std::string socket_path = "/run/aiassistance-makcu/cmd.sock")
        : socket_path_(std::move(socket_path)) {}
    ~MakcuMouseBackend() override { disconnect(); }

    bool connect(std::string* error = nullptr) override;
    void disconnect() override;
    bool reconnect(std::string* error = nullptr) override;
    BackendHealth health() const override;

    bool mouse_move(int32_t dx, int32_t dy, int32_t wheel = 0) override;
    bool mouse_button(uint8_t button, uint8_t action) override;
    bool mouse_click(uint8_t button) override;
    const char* name() const override { return "makcu_proxy"; }

private:
    bool send_packet(uint8_t type, uint32_t request_id, const void* payload,
                     size_t payload_size, std::string* error = nullptr);
    bool send_move(int32_t dx, int32_t dy, int32_t wheel, std::string* error);
    bool ping(std::string* error);

    std::string socket_path_;
    int fd_ = -1;
    uint32_t next_request_id_ = 1;
    mutable BackendHealth health_;
};

}  // namespace ttbox::core::output
