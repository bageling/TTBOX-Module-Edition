// LocalHidBackend.hpp — 本机 HID 输出后端（迁移自 AiboxHidOutput，行为不变）
#pragma once

#include "output/OutputBackend.hpp"
#include <string>

namespace ttbox::core::output {

// 本机 /dev/hidg0 直接输出。报告格式与 AiboxHidOutput 完全一致：
//   buttons(16bit LE) + X(int16 LE) + Y(int16 LE) + wheel(8) + pan(8)，共 9 字节。
// Hotkey Gate / mouse.enabled 实时判定在基类 gate_allows() 中（与 AiboxHidOutput 相同）。
class LocalHidBackend final : public IOutputBackend {
public:
    explicit LocalHidBackend(std::string hidg_path = "/dev/hidg1")
        : path_(std::move(hidg_path)) {}
    ~LocalHidBackend() override { disconnect(); }

    bool connect(std::string* error = nullptr) override;
    void disconnect() override;
    bool reconnect(std::string* error = nullptr) override;
    BackendHealth health() const override;

    bool mouse_move(int32_t dx, int32_t dy, int32_t wheel = 0) override;
    bool mouse_button(uint8_t button, uint8_t action) override;
    bool mouse_click(uint8_t button) override;

    const char* name() const override { return "local_hid"; }

private:
    bool open_if_needed();
    bool write_report(const unsigned char report[9]);

    std::string path_;
    int fd_ = -1;
    mutable BackendHealth health_;
};

}  // namespace ttbox::core::output
