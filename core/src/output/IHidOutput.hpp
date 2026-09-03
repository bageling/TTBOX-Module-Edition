// IHidOutput.hpp — AI 输出后端抽象。
// AimThread 只生成 OutputAction，不直接写 FIFO/HID，避免控制逻辑与设备输出耦合。
#pragma once
#include <cstdint>
namespace ttbox::core::output {
struct OutputAction {
    int16_t move_x = 0;
    int16_t move_y = 0;
    uint8_t button_mask = 0;
    uint8_t control_flags = 0;
    uint64_t frame_number = 0;
    uint64_t timestamp_us = 0;
};
class IHidOutput {
public:
    virtual ~IHidOutput() = default;
    virtual bool send(const OutputAction& action) = 0;
};
class NullHidOutput final : public IHidOutput {
public:
    bool send(const OutputAction&) override { return true; }
};
}  // namespace ttbox::core::output
