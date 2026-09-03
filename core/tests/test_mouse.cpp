// test_mouse.cpp — A10 AI 鼠标注入模块单元测试
//
// 覆盖：TargetSelector / AimPointProfile / CoordinateTransform / AimTracker /
//       MotionController / Deadzone / RateLimit / MotionMerge / AimState /
//       RuntimeProfile(mouse 段) + 关键场景。
#include "test_util.hpp"

#include <cmath>

#include "mouse/AimPointProfile.hpp"
#include "mouse/AimStateMachine.hpp"
#include "mouse/AimTracker.hpp"
#include "mouse/CoordinateTransform.hpp"
#include "mouse/ContinuousLead.hpp"
#include "mouse/Deadzone.hpp"
#include "mouse/FovAngle.hpp"
#include "mouse/Humanize.hpp"
#include "mouse/MotionController.hpp"
#include "mouse/MotionMerge.hpp"
#include "mouse/MouseRouter.hpp"
#include "mouse/MouseTypes.hpp"
#include "mouse/OutputScale.hpp"
#include "mouse/PullCurve.hpp"
#include "mouse/RateLimit.hpp"
#include "mouse/Smooth.hpp"
#include "mouse/TargetSelector.hpp"
#include "model/RuntimeProfile.hpp"

using namespace ttbox::core;

// ---------------------------------------------------------------------------
// 1. TargetSelector
// ---------------------------------------------------------------------------
TEST(mouse_target_selector_fov_and_confidence) {
    aim::TargetSelector sel;
    aim::TargetSelectorConfig cfg;
    cfg.fov_range = 1.0f;
    cfg.confidence = 0.5f;
    cfg.roi_w = 320;
    cfg.roi_h = 320;

    std::vector<DetectionBox> dets;
    // 中心远（conf 0.9，超出半径）与近（conf 0.6）
    DetectionBox far, near;
    far.x1 = 40; far.y1 = 40; far.x2 = 60; far.y2 = 60; far.score = 0.9f; far.class_id = 0;
    near.x1 = 150; near.y1 = 150; near.x2 = 170; near.y2 = 170; near.score = 0.6f; near.class_id = 0;
    dets.push_back(far);
    dets.push_back(near);

    // fov_range=1：半径=160，两者都在；取最近（near）
    auto s = sel.select(dets, cfg);
    CHECK(s.valid);
    CHECK_EQ(static_cast<int>(s.box.x1), 150);

    // 低置信度被过滤
    cfg.confidence = 0.95f;
    s = sel.select(dets, cfg);
    CHECK(!s.valid);

    // fov_range=0.1：半径=16，far(中心距 155) 被过滤；near(距 0) 仍命中
    cfg.confidence = 0.5f;
    cfg.fov_range = 0.1f;
    std::vector<DetectionBox> only_far = {far};
    s = sel.select(only_far, cfg);
    CHECK(!s.valid);
    s = sel.select(dets, cfg);
    CHECK(s.valid);
    CHECK_EQ(static_cast<int>(s.box.x1), 150);
}

TEST(mouse_target_selector_class_filter) {
    aim::TargetSelector sel;
    aim::TargetSelectorConfig cfg;
    cfg.roi_w = 320; cfg.roi_h = 320;
    std::vector<DetectionBox> dets;
    DetectionBox b;
    b.x1 = 100; b.y1 = 100; b.x2 = 120; b.y2 = 120; b.score = 0.9f; b.class_id = 1;
    dets.push_back(b);
    cfg.class_filter = {0};
    CHECK(!sel.select(dets, cfg).valid);  // class 1 被过滤
    cfg.class_filter = {1};
    CHECK(sel.select(dets, cfg).valid);
}

// ---------------------------------------------------------------------------
// 2. AimPointProfile
// ---------------------------------------------------------------------------
TEST(mouse_aim_point_profile_default_and_class_override) {
    aim::AimPointProfile prof;
    prof.offset_x = 0.5f;
    prof.offset_y = 0.5f;
    DetectionBox box;
    box.x1 = 100; box.y1 = 200; box.x2 = 200; box.y2 = 400;
    float tx = 0, ty = 0;
    // 默认：框中心
    CHECK(aim::aim_point_at(box, 0, prof, &tx, &ty));
    CHECK_EQ(tx, 150.0f);
    CHECK_EQ(ty, 300.0f);

    // class_offsets 覆盖（class 0：0.25, 0.25）
    aim::ClassOffset co;
    co.class_id = 0;
    co.offset_x = 0.25f;
    co.offset_y = 0.25f;
    co.priority = 0;
    prof.class_offsets.push_back(co);
    CHECK(aim::aim_point_at(box, 0, prof, &tx, &ty));
    CHECK_EQ(tx, 125.0f);
    CHECK_EQ(ty, 250.0f);
    // class 1 未覆盖 → 默认
    CHECK(aim::aim_point_at(box, 1, prof, &tx, &ty));
    CHECK_EQ(tx, 150.0f);
}

// ---------------------------------------------------------------------------
// 3. CoordinateTransform（ROI/crop 系）
// ---------------------------------------------------------------------------
TEST(mouse_coord_transform_pixel_error) {
    aim::AimPointProfile prof;  // 默认 0.5/0.5
    DetectionBox box;
    box.x1 = 190; box.y1 = 190; box.x2 = 210; box.y2 = 210;  // 中心 (200,200)
    float ex = 0, ey = 0;
    // roi 320×320，准星 (160,160)，目标中心 (200,200) → err +40/+40
    CHECK(aim::CoordinateTransform::pixel_error(box, 0, prof, 320, 320, &ex, &ey));
    CHECK_EQ(ex, 40.0f);
    CHECK_EQ(ey, 40.0f);
    // ROI 改变（192）→ 准星 (96,96) → err +104/+104（自动重算）
    CHECK(aim::CoordinateTransform::pixel_error(box, 0, prof, 192, 192, &ex, &ey));
    CHECK_EQ(ex, 104.0f);
    // aim_offset 偏移准星
    prof.aim_offset_x = -10.0f;
    prof.aim_offset_y = 10.0f;
    CHECK(aim::CoordinateTransform::pixel_error(box, 0, prof, 320, 320, &ex, &ey));
    CHECK_EQ(ex, 50.0f);   // 200 - (160-10)
    CHECK_EQ(ey, 30.0f);   // 200 - (160+10)
}

// ---------------------------------------------------------------------------
// 4. AimTracker（帧差速度 + EMA 平滑 + 目标切换重置 + 预测）
// ---------------------------------------------------------------------------
TEST(mouse_aim_tracker_velocity_and_switch) {
    aim::AimTracker tr;
    tr.update(100.0f, 100.0f, 0, 0);
    // 100ms 移动 +10px → raw=100px/s, EMA(0.4)=40px/s
    tr.update(110.0f, 100.0f, 0, 100000);
    CHECK_EQ(tr.state().vx, 40.0f);
    CHECK_EQ(tr.state().vy, 0.0f);
    // 预测 0.5s（EMA 速度 40）
    float px = 0, py = 0;
    tr.predict(0.5f, &px, &py);
    CHECK_EQ(px, 130.0f);  // 110 + 40*0.5 = 130
    // 目标切换（target_id 变化）→ 速度清零
    tr.update(200.0f, 200.0f, 1, 200000);
    CHECK_EQ(tr.state().vx, 0.0f);
    CHECK_EQ(tr.state().vy, 0.0f);
    // reset
    tr.reset();
    CHECK(!tr.state().valid);
}

// ---------------------------------------------------------------------------
// 5. MotionController（PID：kp 项 + ki/kd 默认 0 = 纯 P）
// ---------------------------------------------------------------------------
TEST(mouse_motion_controller_p_only) {
    aim::MotionController c;
    auto o = c.update(10.0f, -4.0f, 17.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    CHECK_EQ(o.out_x, 127.0f);
    CHECK_EQ(o.out_y, -40.0f);
}

TEST(mouse_motion_controller_ki) {
    aim::MotionController c;
    // ki=0.1：积分会更新，但最终输出受 +127 限幅
    auto o1 = c.update(10.0f, 0.0f, 17.0f, 10.0f, 0.1f, 0.0f, 0.0f, 0.0f);
    CHECK_EQ(o1.out_x, 127.0f);
    // 持续 err=10：积分再 +1
    auto o2 = c.update(10.0f, 0.0f, 17.0f, 10.0f, 0.1f, 0.0f, 0.0f, 0.0f);
    CHECK_EQ(o2.out_x, 127.0f);
}

TEST(mouse_motion_controller_kd) {
    aim::MotionController c;
    // kd=2：微分会参与控制，但最终输出受 +127 限幅
    auto o1 = c.update(10.0f, 0.0f, 17.0f, 10.0f, 0.0f, 0.0f, 2.0f, 0.0f);
    (void)o1;
    auto o2 = c.update(8.0f, 0.0f, 17.0f, 10.0f, 0.0f, 0.0f, 2.0f, 0.0f);
    CHECK_EQ(o2.out_x, -127.0f);
    c.reset();
    auto o3 = c.update(8.0f, 0.0f, 17.0f, 10.0f, 0.0f, 0.0f, 2.0f, 0.0f);
    CHECK_EQ(o3.out_x, 127.0f);  // reset 后无微分，误差为正时恢复正向输出
}

// ---------------------------------------------------------------------------
// 6. Deadzone（X/Y 独立）
// ---------------------------------------------------------------------------
TEST(mouse_deadzone_xy_independent) {
    aim::MouseProfile p;
    p.deadzone_x = 1.0f;
    p.deadzone_y = 2.0f;
    CHECK_EQ(aim::deadzone_x(0.5f, p), 0.0f);
    CHECK_EQ(aim::deadzone_x(3.0f, p), 3.0f);
    CHECK_EQ(aim::deadzone_y(1.9f, p), 0.0f);
    CHECK_EQ(aim::deadzone_y(2.1f, p), 2.1f);
}

// ---------------------------------------------------------------------------
// 7. RateLimit（±127 拆包）
// ---------------------------------------------------------------------------
TEST(mouse_rate_limit_split_reports) {
    aim::RateLimiter lim;
    // 单次 300 超出 127 → 第一包 127，pending 173
    auto s1 = lim.step(300, 0, 127);
    CHECK_EQ(s1.dx, 127);
    CHECK_EQ(s1.pending_dx, 173);
    // 第二包继续消耗
    auto s2 = lim.step(0, 0, 127);
    CHECK_EQ(s2.dx, 127);
    CHECK_EQ(s2.pending_dx, 46);
    auto s3 = lim.step(0, 0, 127);
    CHECK_EQ(s3.dx, 46);
    CHECK_EQ(s3.pending_dx, 0);
    // 负方向
    lim.reset();
    auto sn = lim.step(-300, -5, 127);
    CHECK_EQ(sn.dx, -127);
    CHECK_EQ(sn.dy, -5);
    CHECK_EQ(sn.pending_dx, -173);
}

// ---------------------------------------------------------------------------
// 8. MotionMerge（物理 + AI；block 屏蔽；int16 clamp）
// ---------------------------------------------------------------------------
TEST(mouse_merge_no_ai_passthrough_unchanged) {
    // AI 未启用（ai=0）→ 物理透传不变
    aim::PhysicalMotion phys; phys.dx = 10; phys.dy = -5; phys.buttons = 1;
    aim::AiMove ai; ai.dx = 0; ai.dy = 0;
    auto m = aim::MotionMerge::merge(phys, ai, false, false);
    CHECK_EQ(m.dx, 10);
    CHECK_EQ(m.dy, -5);
    CHECK_EQ(m.buttons, 1u);
}

TEST(mouse_merge_ai_only) {
    aim::PhysicalMotion phys;  // 全零
    aim::AiMove ai; ai.dx = 20; ai.dy = -20;
    auto m = aim::MotionMerge::merge(phys, ai, false, false);
    CHECK_EQ(m.dx, 20);
    CHECK_EQ(m.dy, -20);
}

TEST(mouse_merge_ai_plus_physical) {
    aim::PhysicalMotion phys; phys.dx = 10; phys.dy = 5;
    aim::AiMove ai; ai.dx = 20; ai.dy = -20;
    auto m = aim::MotionMerge::merge(phys, ai, false, false);
    CHECK_EQ(m.dx, 30);
    CHECK_EQ(m.dy, -15);
}

TEST(mouse_merge_block_x_and_y) {
    aim::PhysicalMotion phys; phys.dx = 10; phys.dy = 5;
    aim::AiMove ai; ai.dx = 20; ai.dy = -20;
    // X block：物理 X 被屏蔽，Y 保留
    auto mx = aim::MotionMerge::merge(phys, ai, true, false);
    CHECK_EQ(mx.dx, 20);
    CHECK_EQ(mx.dy, -15);
    // Y block
    auto my = aim::MotionMerge::merge(phys, ai, false, true);
    CHECK_EQ(my.dx, 30);
    CHECK_EQ(my.dy, -20);
    // 全 block
    auto mb = aim::MotionMerge::merge(phys, ai, true, true);
    CHECK_EQ(mb.dx, 20);
    CHECK_EQ(mb.dy, -20);
}

TEST(mouse_merge_int16_clamp) {
    aim::PhysicalMotion phys; phys.dx = 32000;
    aim::AiMove ai; ai.dx = 2000;
    auto m = aim::MotionMerge::merge(phys, ai, false, false);
    CHECK_EQ(m.dx, 32767);
    aim::PhysicalMotion pn; pn.dx = -32000;
    aim::AiMove an; an.dx = -2000;
    auto mn = aim::MotionMerge::merge(pn, an, false, false);
    CHECK_EQ(mn.dx, -32768);
    // 未溢出：-32000 + 2000 = -30000（int16 内，不 clamp）
    aim::AiMove ai2; ai2.dx = 2000;
    auto m2 = aim::MotionMerge::merge(pn, ai2, false, false);
    CHECK_EQ(m2.dx, -30000);
}

// ---------------------------------------------------------------------------
// 9. AimState（78ms 丢失宽限 / 超时 Reset / 热键）
// ---------------------------------------------------------------------------
TEST(mouse_aim_state_lost_grace_recovery) {
    aim::AimStateMachine sm;
    aim::AimStateEvent ev;
    ev.hotkey_active = true;
    ev.now_ms = 1000;
    ev.has_target = true;
    CHECK(sm.update(ev, 78.0f));          // IDLE → AIMING（进入重置）
    CHECK(sm.state() == aim::AimState::kAiming);
    ev.has_target = false;
    CHECK(!sm.update(ev, 78.0f));         // AIMING → LOST_GRACE
    CHECK(sm.state() == aim::AimState::kLostGrace);
    ev.has_target = true;
    ev.now_ms = 1030;                     // 30ms 内找回
    CHECK(!sm.update(ev, 78.0f));
    CHECK(sm.state() == aim::AimState::kAiming);
}

TEST(mouse_aim_state_lost_grace_timeout_reset) {
    aim::AimStateMachine sm;
    aim::AimStateEvent ev;
    ev.hotkey_active = true;
    ev.has_target = true;
    ev.now_ms = 1000;
    sm.update(ev, 78.0f);
    CHECK(sm.state() == aim::AimState::kAiming);
    ev.has_target = false;
    sm.update(ev, 78.0f);
    ev.now_ms = 1100;                     // 超过 78ms
    const bool reset = sm.update(ev, 78.0f);
    CHECK(reset);                          // 超时 → IDLE + Reset
    CHECK(sm.state() == aim::AimState::kIdle);
}

TEST(mouse_aim_state_hotkey_release) {
    aim::AimStateMachine sm;
    aim::AimStateEvent ev;
    ev.hotkey_active = true;
    ev.has_target = true;
    ev.now_ms = 1000;
    sm.update(ev, 78.0f);
    CHECK(sm.state() == aim::AimState::kAiming);
    ev.hotkey_active = false;
    CHECK(sm.update(ev, 78.0f));           // 松开 → IDLE + Reset
    CHECK(sm.state() == aim::AimState::kIdle);
}

// ---------------------------------------------------------------------------
// 10. RuntimeProfile mouse 段序列化/反序列化
// ---------------------------------------------------------------------------
TEST(mouse_runtime_profile_json_roundtrip) {
    RuntimeProfile p;
    p.mouse.enabled = true;
    p.mouse.proxy_mode = aim::MouseProxyMode::kFullPassthrough;
    p.mouse.aim_hotkey = 0x02;
    p.mouse.fov_range = 0.41f;
    p.mouse.kp_x = 17.0f;
    p.mouse.kp_y = 10.0f;
    p.mouse.rate_x = 0.4f;
    p.mouse.rate_y = 0.3f;
    p.mouse.output_scale = 1.0f;
    p.mouse.deadzone_x = 1.0f;
    p.mouse.smooth = 0.2f;
    p.mouse.lost_grace_ms = 78.0f;
    p.mouse.block_physical_x = true;
    p.mouse.aim_point.aim_offset_x = 12.0f;
    p.mouse.fov_mode = true;
    p.mouse.hfov = 90.0f;
    p.mouse.vfov = 60.0f;
    p.mouse.move_speed_x = 700.0f;
    p.mouse.move_speed_y = 650.0f;
    p.mouse.aim_part = 1;
    aim::ClassOffset co;
    co.class_id = 0; co.offset_x = 0.48f; co.offset_y = 0.49f; co.priority = 0;
    p.mouse.aim_point.class_offsets.push_back(co);

    const JsonValue j = p.to_json();
    RuntimeProfile q = RuntimeProfile::from_json(j);
    CHECK(q.mouse.enabled);
    CHECK(q.mouse.proxy_mode == aim::MouseProxyMode::kFullPassthrough);
    CHECK_EQ(q.mouse.aim_hotkey, 0x02u);
    CHECK(q.mouse.fov_range == 0.41f);
    CHECK(q.mouse.kp_x == 17.0f);
    CHECK(q.mouse.kp_y == 10.0f);
    CHECK(q.mouse.rate_x == 0.4f);
    CHECK(q.mouse.output_scale == 1.0f);
    CHECK(q.mouse.deadzone_x == 1.0f);
    CHECK(q.mouse.smooth == 0.2f);
    CHECK(q.mouse.lost_grace_ms == 78.0f);
    CHECK(q.mouse.block_physical_x);
    CHECK(q.mouse.aim_point.aim_offset_x == 12.0f);
    CHECK(q.mouse.fov_mode);
    CHECK(q.mouse.hfov == 90.0f);
    CHECK(q.mouse.vfov == 60.0f);
    CHECK(q.mouse.move_speed_x == 700.0f);
    CHECK(q.mouse.move_speed_y == 650.0f);
    CHECK_EQ(q.mouse.aim_part, 1);
    CHECK_EQ(q.mouse.aim_point.class_offsets.size(), 1u);
    CHECK(q.mouse.aim_point.class_offsets[0].offset_y == 0.49f);
    // validate 通过（合法配置）
    std::string verr;
    CHECK(q.validate(&verr));
    // 非法值被拒
    q.mouse.fov_range = 1.5f;
    CHECK(!q.validate(&verr));
}

// ---------------------------------------------------------------------------
// 附加：FOV 只影响目标选择，不影响输出缩放
// ---------------------------------------------------------------------------
TEST(mouse_fov_not_affect_output_scale) {
    aim::MouseProfile a, b;
    a.fov_range = 1.0f;
    b.fov_range = 0.1f;
    b.kp_x = a.kp_x;
    b.rate_x = a.rate_x;
    b.sensitivity = a.sensitivity;
    b.output_scale = a.output_scale;
    // 相同输入，输出缩放一致（fov_range 不参与）
    CHECK_EQ(aim::output_scale_x(50.0f, a), aim::output_scale_x(50.0f, b));
}

// ---------------------------------------------------------------------------
// 附加：FovAngle（角度换算输出模式）
// ---------------------------------------------------------------------------
TEST(mouse_fov_angle_direction_and_scale) {
    // 方向：目标右侧(err>0) → 移动>0；左侧 → <0（与纯 P 一致）
    CHECK(aim::fov_move_x(50.0f, 320.0f, 83.105f, 500.0f) > 0.0f);
    CHECK(aim::fov_move_x(-50.0f, 320.0f, 83.105f, 500.0f) < 0.0f);
    CHECK(aim::fov_move_y(30.0f, 320.0f, 53.0f, 500.0f) > 0.0f);
    CHECK(aim::fov_move_y(-30.0f, 320.0f, 53.0f, 500.0f) < 0.0f);
    // 单调：误差越大移动越大
    const float s1 = aim::fov_move_x(20.0f, 320.0f, 83.105f, 500.0f);
    const float s2 = aim::fov_move_x(60.0f, 320.0f, 83.105f, 500.0f);
    CHECK(s2 > s1);
    // 速度越大移动越大
    CHECK(aim::fov_move_x(40.0f, 320.0f, 83.105f, 800.0f) >
          aim::fov_move_x(40.0f, 320.0f, 83.105f, 500.0f));
    // 非法 FOV 回退线性（不崩溃）
    CHECK(aim::fov_move_x(10.0f, 320.0f, 0.0f, 500.0f) == 10.0f);
}

// ---------------------------------------------------------------------------
// 附加：MouseRouter 解析（罗技 c53f input1：ReportID 0x02 + int16 轴）
// ---------------------------------------------------------------------------
TEST(mouse_router_parse_logitech_layout) {
    aim::MouseLayout lay;
    lay.report_id = 0x02;
    lay.buttons_offset = 1;
    lay.buttons_size = 2;
    lay.axis_offset = 3;
    lay.axis_size = 2;
    lay.wheel_offset = 7;
    // 9 字节：ReportID, buttons(2B), X(2B), Y(2B), wheel, pan
    uint8_t rep[9] = {0x02, 0x01, 0x00, 0x0A, 0x00, 0xFE, 0xFF, 0x05, 0x00};
    aim::MouseRouter router;
    aim::PhysicalMotion m;
    CHECK(router.parse(rep, sizeof(rep), 123, lay, &m));
    CHECK_EQ(m.buttons, 1u);
    CHECK_EQ(m.dx, 10);
    CHECK_EQ(m.dy, -2);
    CHECK_EQ(m.wheel, 5);
    // ReportID 不匹配 → false
    rep[0] = 0x03;
    CHECK(!router.parse(rep, sizeof(rep), 123, lay, &m));
    // 长度不足 → false
    CHECK(!router.parse(rep, 5, 123, lay, &m));
}

// ---------------------------------------------------------------------------
// YU 插件：PullCurve（拉枪曲线）/ ContinuousLead（持续提前量）/ Humanize（拟人）
// ---------------------------------------------------------------------------
TEST(mouse_yu_pull_curve_activates_only_beyond_min_distance) {
    aim::PullCurveConfig cfg;  // enabled=true min_distance=80 strength=0.8
    aim::PullCurve pc;
    // 距离 50 < 80：不激活 → 附加 0
    CHECK_EQ(pc.apply(-30.0f, 40.0f, 10.0f, 5.0f, cfg, 4.0f), 0.0f);
    // 距离 100 > 80：激活（|附加| > 0）
    const float v = pc.apply(60.0f, 80.0f, 20.0f, 0.0f, cfg, 4.0f);
    CHECK(std::fabs(v) > 0.0f);
    // 方向：out_x >= 0 → 弧线方向为正
    CHECK(v > 0.0f);
    pc.reset();
}

TEST(mouse_yu_continuous_lead_needs_accumulated_distance) {
    aim::ContinuousLeadConfig cfg;  // enabled=false 默认
    aim::ContinuousLead cl;
    // 未启用：返回 0
    CHECK_EQ(cl.apply(50, 0, 4.0f, cfg), 0.0f);
    // 启用 + 单次未达 enter_distance → 0
    cfg.enabled = true;
    cfg.enter_distance = 150.0f;
    cfg.scale = 0.5f;
    CHECK_EQ(cl.apply(50, 0, 4.0f, cfg), 0.0f);   // accum=50
    CHECK_EQ(cl.apply(50, 0, 4.0f, cfg), 0.0f);   // accum=100
    // 累计 50×3=150 ≥ 150 → 开始输出偏置
    CHECK(std::fabs(cl.apply(50, 0, 4.0f, cfg)) > 0.0f);  // accum=150
    cl.reset();
    // 复位后需重新累计
    CHECK_EQ(cl.apply(50, 0, 4.0f, cfg), 0.0f);   // accum=50
    cl.reset();
}

TEST(mouse_yu_humanize_adds_jitter_only_when_enabled) {
    aim::HumanizeConfig cfg;  // enabled=true jitter_px=0.25
    aim::Humanize hz;
    float x = 0.0f, y = 0.0f;
    hz.apply(&x, &y, 4.0f, cfg);
    // 抖动幅度 ≤ jitter_px（0.25）
    CHECK(std::fabs(x) <= 0.26f);
    CHECK(std::fabs(y) <= 0.26f);
    // 关闭：不抖动
    cfg.enabled = false;
    x = 0.0f; y = 0.0f;
    hz.apply(&x, &y, 4.0f, cfg);
    CHECK_EQ(x, 0.0f);
    CHECK_EQ(y, 0.0f);
}

// ---------------------------------------------------------------------------
// YU 对齐：RuntimeProfile mouse 段序列化（predict/插件/自适应死区参数）
// ---------------------------------------------------------------------------
TEST(mouse_profile_yu_fields_roundtrip) {
    RuntimeProfile p;
    p.mouse.predict_x = 0.6f;
    p.mouse.predict_y = 0.7f;
    p.mouse.smooth_x = 5000.0f;
    p.mouse.smooth_y = 4000.0f;
    p.mouse.output_deadzone = 1.5f;
    p.mouse.selector_search_radius = 200.0f;
    p.mouse.aim_fire_lock_y = true;
    p.mouse.y_axis_fire_hotkey = 2;
    p.mouse.y_axis_fire_release_delay_sec = 0.25f;
    p.mouse.pull_curve.enabled = true;
    p.mouse.pull_curve.strength = 0.9f;
    p.mouse.pull_curve.jitter_px = 2.5f;
    p.mouse.pull_curve.min_distance = 100.0f;
    p.mouse.continuous_lead.enabled = true;
    p.mouse.continuous_lead.enter_distance = 160.0f;
    p.mouse.continuous_lead.scale = 0.6f;
    p.mouse.humanize.enabled = false;
    p.mouse.humanize.jitter_px = 0.1f;

    const JsonValue j = p.to_json();
    const RuntimeProfile q = RuntimeProfile::from_json(j);
    CHECK(q.mouse.predict_x == 0.6f);
    CHECK(q.mouse.predict_y == 0.7f);
    CHECK(q.mouse.smooth_x == 5000.0f);
    CHECK(q.mouse.output_deadzone == 1.5f);
    CHECK(q.mouse.selector_search_radius == 200.0f);
    CHECK(q.mouse.aim_fire_lock_y);
    CHECK_EQ(q.mouse.y_axis_fire_hotkey, 2);
    CHECK(q.mouse.y_axis_fire_release_delay_sec == 0.25f);
    CHECK(q.mouse.pull_curve.enabled);
    CHECK(q.mouse.pull_curve.strength == 0.9f);
    CHECK(q.mouse.pull_curve.min_distance == 100.0f);
    CHECK(q.mouse.continuous_lead.enabled);
    CHECK(q.mouse.continuous_lead.enter_distance == 160.0f);
    CHECK(!q.mouse.humanize.enabled);
    CHECK(q.mouse.humanize.jitter_px == 0.1f);
}

