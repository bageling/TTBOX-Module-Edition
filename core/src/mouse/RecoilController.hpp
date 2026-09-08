// RecoilController.hpp — 压枪引擎（recoil assist）
//
// 功能（小白理解）：
//   按住开火键（默认左键）时，游戏枪械会有后坐力把准星往上顶。
//   压枪 = 开火期间自动给鼠标一个持续向下的补偿移动，让弹着点不飘。
//
// 设计原则：
//   本引擎只负责"算压枪量"，不直接发鼠标命令。
//   算出的 y 压枪量（count）由 AimThread 注入自身输出链
//   （pull_curve 之后、deadzone 之前，与 PID 输出融合），
//   之后统一走 deadzone → remainder → int16 → 拟人化整形 → 热键安全门。
//   与 YU 的独立 recoil 链路不同：这里所有输出都受 TTBOX 安全边界约束。
//
// 行为（对齐 YU 压枪模块，参数语义一致）：
//   1. 热键按住（hotkey/hotkey2，any=任一 / all=同时）才开始计时
//   2. trigger_delay_ms：按住超过该时长才压（防单点误触），松开重新计时
//   3. only_when_target_visible：有目标才压；目标丢失后 target_lost_release_ms 内继续压（保持窗口）
//   4. 下压速率 = strength × speed（px/s）× 帧间隔 dt
//   5. humanize：缓入缓出拆步（curve_strength）+ X 轴微动（jitter）
//   6. 亚像素残差累计：压枪量与 PID remainder 同域，小数不丢精度
//
// 默认值全部保持"关闭/零输出"，不改变现有行为。
#pragma once

#include <cmath>
#include <cstdint>

#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

class RecoilController {
public:
    // 压枪输出（count 域）：y 为正下压量；x 为拟人微动量（可正可负）
    struct RecoilDelta {
        float y = 0.0f;
        float x = 0.0f;
    };

    // 每帧调用一次，返回本帧压枪量（count 域）。
    // hotkey_bits: 物理鼠标按键位图（左1 右2 中4 侧8 侧16）
    // target_visible: 当前是否有目标（selector 已选中）
    // cfg: 压枪配置（每帧从 RuntimeProfile 快照更新）
    // dt_ms: 帧间隔（ms）
    // px_per_count: 游戏灵敏度标定（gain_y_px_per_count 语义，默认 0.65 px/count）
    RecoilDelta update(uint16_t hotkey_bits, bool target_visible,
                       const RecoilConfig& cfg, float dt_ms, float px_per_count);

    // 目标切换/模型切换等重置：清计时与残差
    void reset() {
        fire_press_ms_ = 0.0f;
        target_lost_ms_ = 0.0f;
        recoil_ramp_ = 0.0f;
        recoil_x_jitter_phase_ = 0.0f;
        recoil_residual_y_ = 0.0f;
        recoil_residual_x_ = 0.0f;
        had_target_ = false;
    }

    // 当前是否处于压枪激活状态（供状态 API/日志用）
    bool active() const { return active_; }

private:
    // 热键触发：any = 任一命中，all = 同时按下
    static bool hotkey_hit(uint16_t bits, int k1, int k2, int mode) {
        if (mode == 2) {  // all
            const bool a = k1 != 0 && (bits & k1) != 0;
            const bool b = k2 != 0 && (bits & k2) != 0;
            return a && b;
        }
        const bool a = k1 != 0 && (bits & k1) != 0;
        const bool b = k2 != 0 && (bits & k2) != 0;
        return a || b;
    }

    float fire_press_ms_ = 0.0f;      // 本次按住持续时长（ms，松开清零）
    float target_lost_ms_ = 0.0f;     // 目标丢失持续时长（ms，重新见目标清零）
    float recoil_ramp_ = 0.0f;        // 缓入缓出系数 [0,1]
    float recoil_x_jitter_phase_ = 0.0f;  // X 微动相位（rad）
    float recoil_residual_y_ = 0.0f;  // Y 亚像素残差（count）
    float recoil_residual_x_ = 0.0f;  // X 亚像素残差（count）
    bool had_target_ = false;         // 是否曾经见过目标（丢失窗口仅对曾见目标生效）
    bool active_ = false;             // 本帧是否激活压枪
};

// ==================== 实现 ====================

inline RecoilController::RecoilDelta RecoilController::update(
    uint16_t hotkey_bits, bool target_visible,
    const RecoilConfig& cfg, float dt_ms, float px_per_count) {
    RecoilDelta out;
    active_ = false;
    if (!cfg.enabled) return out;

    const float dt = (dt_ms > 0.0f) ? (dt_ms * 0.001f) : 0.0f;  // s
    const bool firing = hotkey_hit(hotkey_bits, cfg.hotkey, cfg.hotkey2, cfg.hotkey_mode);

    // ---- 触发计时 ----
    if (firing) fire_press_ms_ += dt_ms;
    else fire_press_ms_ = 0.0f;

    // 延迟触发：按住超过 trigger_delay_ms 才算"开火中"
    bool pressing = firing;
    if (cfg.trigger_delay_enabled && cfg.trigger_delay_ms > 0.0f) {
        pressing = firing && (fire_press_ms_ >= cfg.trigger_delay_ms);
    }

    // ---- 目标门控 ----
    bool target_ok = !cfg.only_when_target_visible || target_visible;
    if (target_visible) { target_lost_ms_ = 0.0f; had_target_ = true; }
    else target_lost_ms_ += dt_ms;

    // 目标丢失保持窗口：仅当"曾经见过目标"时，丢失 target_lost_release_ms 内仍压。
    // （从未见过目标 → 窗口无效，防空压；YU 语义一致）
    if (cfg.only_when_target_visible && !target_visible && had_target_) {
        const float keep_ms = (cfg.target_lost_release_ms >= 0.0f) ? cfg.target_lost_release_ms : 0.0f;
        target_ok = target_lost_ms_ <= keep_ms;
    }

    // ---- 缓入缓出 ramp ----
    const bool want = pressing && target_ok;
    if (want) {
        recoil_ramp_ += dt * cfg.humanize_curve_strength * 4.0f;
        if (recoil_ramp_ > 1.0f) recoil_ramp_ = 1.0f;
    } else {
        recoil_ramp_ -= dt * cfg.humanize_curve_strength * 4.0f;
        if (recoil_ramp_ < 0.0f) recoil_ramp_ = 0.0f;
    }

    // ---- 下压量生成 ----
    if (recoil_ramp_ <= 0.0f) return out;

    const float rate_px_per_s = cfg.strength * cfg.speed;  // px/s
    if (rate_px_per_s <= 0.0f) return out;

    // px → count：复用标定响应（gain_y_px_per_count 语义，默认 0.65 px/count）
    const float ppc = (px_per_count > 0.05f) ? px_per_count : 0.65f;
    const float base_count = rate_px_per_s * dt / ppc;
    const float ramp_count = base_count * recoil_ramp_;

    // ---- X 轴微动（拟人）：相位推进 + 幅度随 ramp ----
    if (cfg.humanize_enabled && cfg.humanize_jitter_px > 0.0f) {
        recoil_x_jitter_phase_ += dt * cfg.humanize_jitter_frequency * 6.2832f;
        const float jitter_count =
            (cfg.humanize_jitter_px * recoil_ramp_ / ppc) * std::sin(recoil_x_jitter_phase_);
        recoil_residual_x_ += jitter_count;
        out.x = recoil_residual_x_;
        recoil_residual_x_ = 0.0f;
    }

    // ---- Y 残差累计（与 PID remainder 同域，不丢精度）----
    recoil_residual_y_ += ramp_count;
    out.y = recoil_residual_y_;
    recoil_residual_y_ = 0.0f;
    active_ = true;
    return out;
}

}  // namespace ttbox::core::aim
