// test_selector_stress.cpp — C6：TargetSelector 压力测试（防横跳专项）。
// 场景：单目标 / 多目标 / 同Y / 同X / 交叉 / 突现 / 消失 / 抖动 / 排序变化。
// 断言核心：激活后 target_id 不横跳（宽限内保持原目标，不切换到邻近目标）。
#include <cmath>
#include <vector>

#include "mouse/TargetSelector.hpp"
#include "test_util.hpp"

using namespace ttbox::core;
using namespace ttbox::core::aim;

namespace {

TargetSelectorConfig make_cfg() {
    TargetSelectorConfig c;
    c.fov_range = 1.0f;
    c.confidence = 0.25f;
    c.roi_w = 640;
    c.roi_h = 480;
    c.lost_grace_ms = 30.0f;
    return c;
}

DetectionBox box(float cx, float cy, float w, float h) {
    DetectionBox b;
    b.x1 = cx - w * 0.5f; b.y1 = cy - h * 0.5f;
    b.x2 = cx + w * 0.5f; b.y2 = cy + h * 0.5f;
    b.score = 0.9f;
    b.class_id = 0;
    return b;
}

}  // namespace

// 单目标抖动：目标框每帧 ±3px 抖动，track id 必须稳定
TEST(selector_single_target_jitter_stable_id) {
    TargetSelector sel;
    auto cfg = make_cfg();
    int first_id = -1;
    for (int i = 0; i < 60; ++i) {
        const float jx = static_cast<float>((i % 7) - 3);
        const float jy = static_cast<float>((i % 5) - 2);
        auto r = sel.select({box(320 + jx, 240 + jy, 60, 120)}, cfg, static_cast<uint32_t>(i * 7));
        CHECK(r.valid);
        if (i == 10) first_id = r.target_id;
        if (i >= 10) CHECK_EQ(r.target_id, first_id);  // 锁定后不换 id
    }
}

// 双目标交叉：A 从右向左、B 从左向右穿过 —— 锁定的目标不能中途跳到另一个
TEST(selector_crossing_targets_no_switch) {
    TargetSelector sel;
    auto cfg = make_cfg();
    int locked = -1;
    for (int i = 0; i < 80; ++i) {
        // 初始 A=500（近右），B=200（左）——最近的是 A，锁定 A
        const float ax = 500.0f - static_cast<float>(i) * 5.0f;  // A 向左
        const float bx = 200.0f + static_cast<float>(i) * 5.0f;  // B 向右
        auto r = sel.select({box(ax, 240, 60, 120), box(bx, 240, 60, 120)}, cfg,
                            static_cast<uint32_t>(i * 7));
        CHECK(r.valid);
        if (i == 5) locked = r.target_id;
        // 锁定后即使两目标交叉（i≈30 时 ax≈bx≈350），id 不能变
        if (i >= 5) CHECK_EQ(r.target_id, locked);
    }
}

// 目标消失：空帧时 selector 返回 invalid（宽限保持由 AimStateMachine 层负责），
// 恢复检测后 id 延续（track 仍在宽限内）。
TEST(selector_target_gone_then_reacquire) {
    TargetSelector sel;
    auto cfg = make_cfg();
    auto r1 = sel.select({box(320, 240, 60, 120)}, cfg, 100);
    CHECK(r1.valid);
    const int id1 = r1.target_id;
    // 空帧：invalid（设计行为）
    auto r2 = sel.select({}, cfg, 115);
    CHECK(!r2.valid);
    // 宽限内目标重现：track 延续，id 不变（不重建新 track）
    auto r3 = sel.select({box(322, 242, 60, 120)}, cfg, 125);
    CHECK(r3.valid);
    CHECK_EQ(r3.target_id, id1);
}

// 目标排序变化（vector 顺序翻转）：track id 仍按位置匹配，不因排序变化换人
TEST(selector_detection_order_flip_stable) {
    TargetSelector sel;
    auto cfg = make_cfg();
    int locked = -1;
    for (int i = 0; i < 40; ++i) {
        const float y = 200.0f + static_cast<float>(i % 4) * 2.0f;
        std::vector<DetectionBox> dets;
        // 每 4 帧换一次顺序
        if ((i / 4) % 2 == 0) {
            dets = {box(260, y, 50, 100), box(380, y, 50, 100)};
        } else {
            dets = {box(380, y, 50, 100), box(260, y, 50, 100)};
        }
        auto r = sel.select(dets, cfg, static_cast<uint32_t>(i * 7));
        CHECK(r.valid);
        if (i == 10) locked = r.target_id;
        if (i >= 10) CHECK_EQ(r.target_id, locked);
    }
}

// 目标突现：空场 60ms 后新目标出现，能正常捕获（不卡死）
TEST(selector_target_appears_after_idle) {
    TargetSelector sel;
    auto cfg = make_cfg();
    for (int i = 0; i < 10; ++i) {
        auto r = sel.select({}, cfg, static_cast<uint32_t>(i * 7));
        CHECK(!r.valid);
    }
    auto r = sel.select({box(320, 240, 60, 120)}, cfg, 100);
    CHECK(r.valid);
    CHECK(r.target_id >= 0);
}

// 目标跳变（teleport 到远处）：应切换目标（rect_lock 匹配失败 → 新 acquire），不卡死
TEST(selector_target_teleport_reattach) {
    TargetSelector sel;
    auto cfg = make_cfg();
    auto r1 = sel.select({box(300, 260, 60, 120)}, cfg, 100);
    CHECK(r1.valid);
    const int id1 = r1.target_id;
    // 目标瞬间跳到 500,400（超出 rect_lock 匹配半径）
    auto r2 = sel.select({box(500, 400, 60, 120)}, cfg, 130);
    CHECK(r2.valid);
    // 允许换 id（teleport 语义）——重点是 valid 且不崩
    (void)id1;
    CHECK(r2.target_id >= 0);
}
#include <cstdio>

int main() {
    return ttbox_test::run_all();
}
