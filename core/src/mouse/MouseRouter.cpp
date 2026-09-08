// MouseRouter.cpp — A10 物理鼠标报告解析实现
#include "mouse/MouseRouter.hpp"

namespace ttbox::core::aim {

bool MouseRouter::parse(const uint8_t* data, size_t size, uint64_t timestamp_us,
                        const MouseLayout& layout, PhysicalMotion* out) const {
    if (!data || !out) return false;
    // ReportID 校验（偏移字段均为"绝对偏移"，含 ReportID）
    if (layout.report_id != 0) {
        if (size < 1 || data[0] != layout.report_id) return false;
    }
    const size_t axis_need = static_cast<size_t>(layout.axis_offset) + layout.axis_size;
    if (size < axis_need) return false;
    if (size < static_cast<size_t>(layout.buttons_offset) + layout.buttons_size) return false;

    PhysicalMotion m;
    m.timestamp_us = timestamp_us;
    // buttons（1B 或 2B LE）
    uint32_t btns = 0;
    for (uint8_t i = 0; i < layout.buttons_size; ++i) {
        btns |= static_cast<uint32_t>(data[layout.buttons_offset + i]) << (8u * i);
    }
    m.buttons = static_cast<uint16_t>(btns);
    // 轴（1B int8 或 2B int16 LE）
    const uint8_t* ap = data + layout.axis_offset;
    if (layout.axis_size == 1) {
        m.dx = static_cast<int8_t>(ap[0]);
        m.dy = static_cast<int8_t>(ap[1]);
    } else {
        const int16_t dx = static_cast<int16_t>(ap[0] | (static_cast<uint16_t>(ap[1]) << 8));
        const int16_t dy = static_cast<int16_t>(ap[2] | (static_cast<uint16_t>(ap[3]) << 8));
        m.dx = dx;
        m.dy = dy;
    }
    // wheel（可选）
    if (layout.wheel_offset != 255 && size > layout.wheel_offset) {
        const uint8_t w = data[layout.wheel_offset];
        m.wheel = layout.wheel_signed ? static_cast<int8_t>(w) : static_cast<int8_t>(w & 0x7F);
    }
    *out = m;
    return true;
}

}  // namespace ttbox::core::aim
