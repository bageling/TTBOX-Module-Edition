// test_bytetrack_upgrade.cpp — TargetSelector ByteTrack 增强（第4项）单元测试
// 验证新增能力：
//   Case1: 轨迹生命周期（修"只增不删"隐患）—— 目标反复进出，轨迹数不无限增长
//   Case2: 轨迹上限 max_tracks —— 大量目标同时出现时轨迹数封顶
//   Case3: 卡尔曼速度学习 —— 匀速目标命中多帧后 vx 收敛到实际位移方向/大小
//   Case4: use_kalman_predict=true 且匀速目标 → 预测中心沿移动方向偏移（pred_cx != cx）
//   Case5: use_kalman_predict=false（默认）→ 关联参考点回退裸框心，预测中心不影响选择
#include <cmath>
#include <cstdio>
#include <vector>

#include "mouse/TargetSelector.hpp"
#include "common/Types.hpp"
#include "test_util.hpp"

using namespace ttbox::core::aim;
using ttbox::core::DetectionBox;

namespace {
TargetSelectorConfig make_cfg() {
    TargetSelectorConfig c;
    c.fov_range = 1.0f;
    c.confidence = 0.0f;   // 不过滤，便于压轨迹数
    c.roi_w = 640;
    c.roi_h = 480;
    c.lost_grace_ms = 30.0f;
    return c;
}
DetectionBox box(float cx, float cy, float w = 40, float h = 80, float conf = 0.9f) {
    DetectionBox b;
    b.x1 = cx - w * 0.5f; b.y1 = cy - h * 0.5f;
    b.x2 = cx + w * 0.5f; b.y2 = cy + h * 0.5f;
    b.score = conf; b.class_id = 0;
    return b;
}
}  // namespace

// Case1: 轨迹生命周期——目标反复进出，轨迹数在 track_buffer 内收敛（不无限增长）
TEST(bytetrack_track_lifecycle_bounded) {
    TargetSelector sel;
    auto cfg = make_cfg();
    cfg.confidence = 0.25f;
    // 反复出现(3帧)→消失(远超 buffer 50帧) 循环多轮
    for (int round = 0; round < 20; ++round) {
        float cx = 320.0f;
        for (int i = 0; i < 3; ++i) {
            sel.select({box(cx, 240)}, cfg, static_cast<uint32_t>(1000 + round * 60 + i * 7));
        }
        // 消失足够久（track_buffer=30 帧）→ 轨迹应被删除
        for (int i = 0; i < 50; ++i) {
            sel.select({}, cfg, static_cast<uint32_t>(1000 + round * 60 + 30 + i * 7));
        }
        // 轨迹数不应随轮次累积（每次清空/收敛）
        CHECK(sel.tracks().size() <= 2u);
    }
}

// Case2: 轨迹上限——84 个目标同时出现，tracks 不得超 max_tracks(64)
TEST(bytetrack_max_tracks_cap) {
    TargetSelector sel;
    auto cfg = make_cfg();
    // 84 个目标平铺
    std::vector<DetectionBox> dets;
    int n = 0;
    for (int r = 0; r < 7 && n < 84; ++r)
        for (int c = 0; c < 12 && n < 84; ++c) {
            dets.push_back(box(50 + c * 50, 50 + r * 50, 30, 60));
            ++n;
        }
    auto selr = sel.select(dets, cfg, 1000);
    CHECK(selr.valid);
    // 轨迹数封顶
    CHECK(sel.tracks().size() <= cfg.max_tracks);
}

// Case3: 卡尔曼速度学习——匀速目标 15 帧后 vx 方向/大小应反映实际移动
TEST(bytetrack_kalman_velocity_learned) {
    TargetSelector sel;
    auto cfg = make_cfg();
    // 目标从左向右匀速 +5px/帧
    float cx = 200.0f;
    uint32_t t = 1000;
    for (int i = 0; i < 15; ++i) {
        auto r = sel.select({box(cx, 240)}, cfg, t += 7);
        cx += 5.0f;
    }
    CHECK(sel.tracks().size() == 1u);
    const auto& tr = sel.tracks().front();
    // 向右移动 → vx 应为正（学习到位）
    CHECK(tr.vx > 0.0f);
    // 幅度接近真实速度（±容差）
    CHECK(tr.vx > 1.0f && tr.vx < 25.0f);
}

// Case4: use_kalman_predict=true + 匀速目标 → 预测中心沿移动方向偏移（pred_cx > cx）
TEST(bytetrack_kalman_predict_forward) {
    TargetSelector sel;
    auto cfg = make_cfg();
    cfg.use_kalman_predict = true;
    float cx = 200.0f;
    uint32_t t = 1000;
    for (int i = 0; i < 12; ++i) {
        auto r = sel.select({box(cx, 240)}, cfg, t += 7);
        cx += 5.0f;
    }
    CHECK(sel.tracks().size() == 1u);
    const auto& tr = sel.tracks().front();
    // 预测中心应超前当前平滑位置（往移动方向）
    CHECK(tr.pred_cx > tr.kx);
}

// Case5: use_kalman_predict=false（默认）→ 预测中心不影响选择，行为回退裸框心
TEST(bytetrack_kalman_predict_off_default) {
    TargetSelector sel;
    auto cfg = make_cfg();
    // 不开启 use_kalman_predict（默认 false）
    uint32_t t = 1000;
    auto r1 = sel.select({box(320, 240)}, cfg, t += 7);
    CHECK(r1.valid);
    const int id1 = r1.target_id;
    // 匀速移动
    float cx = 320.0f;
    for (int i = 0; i < 10; ++i) {
        auto r = sel.select({box(cx, 240)}, cfg, t += 7);
        CHECK(r.valid);
        CHECK_EQ(r.target_id, id1);  // 同一目标保持 id
        cx += 5.0f;
    }
    // 即使 kalman 状态已更新，选择结果仍稳定（不因 pred 跳变换目标）
    CHECK(sel.tracks().size() == 1u);
}

int main() {
    return ttbox_test::run_all();
}