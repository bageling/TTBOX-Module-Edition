// AimPointProfile.cpp — A10 瞄准点计算实现
#include "mouse/AimPointProfile.hpp"

namespace ttbox::core::aim {

void class_offset_for(const AimPointProfile& prof, int class_id,
                      float* offset_x, float* offset_y) {
    const ClassOffset* best = nullptr;
    for (const auto& c : prof.class_offsets) {
        if (c.class_id != class_id) continue;
        if (!best || c.priority > best->priority) best = &c;
    }
    if (best) {
        *offset_x = best->offset_x;
        *offset_y = best->offset_y;
    } else {
        *offset_x = prof.offset_x;
        *offset_y = prof.offset_y;
    }
}

bool aim_point_at(const DetectionBox& box, int class_id, const AimPointProfile& prof,
                  float* tx, float* ty) {
    const float w = box.x2 - box.x1;
    const float h = box.y2 - box.y1;
    if (w <= 0.0f || h <= 0.0f) return false;
    float ox = prof.offset_x;
    float oy = prof.offset_y;
    class_offset_for(prof, class_id, &ox, &oy);
    *tx = box.x1 + ox * w;
    *ty = box.y1 + oy * h;
    return true;
}

}  // namespace ttbox::core::aim
