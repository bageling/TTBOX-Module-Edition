// MouseTypes.hpp — A10 AI 鼠标注入基础类型
//
// 物理透传 + AI 注入双路径。本文件只定义类型/配置，不含逻辑。
// 命名空间 ttbox::core::aim（避免与 hid/HidTypes.hpp 的 MouseState/CoordinateTransform 冲突）。
#pragma once

#include <cstdint>
#include <cstring>

#include "common/Types.hpp"
#include "mouse/MouseProxyMode.hpp"

namespace ttbox::core::aim {

// 瞄准状态机状态（AimStateMachine）
enum class AimState : int {
    kIdle = 0,       // 无目标 / 未激活
    kSelecting = 1,  // 目标检测命中，待选中
    kAiming = 2,     // 目标选中 + 热键有效，持续输出
    kLostGrace = 3,  // 目标丢失宽限期（默认 78ms）
};

inline const char* aim_state_name(AimState s) {
    switch (s) {
        case AimState::kSelecting: return "SELECTING";
        case AimState::kAiming: return "AIMING";
        case AimState::kLostGrace: return "LOST_GRACE";
        default: return "IDLE";
    }
}

// 瞄准热键触发方式：0=任一按键(any) 1=同时按下(all)
inline const char* mouse_hotkey_mode_name(int mode) {
    return mode == 1 ? "all" : "any";
}
inline int mouse_hotkey_mode_from_string(const char* s) {
    return (s && strcmp(s, "all") == 0) ? 1 : 0;
}

// 物理鼠标相对移动（HID 层解析结果，int16 保真）
struct PhysicalMotion {
    int16_t dx = 0;
    int16_t dy = 0;
    uint16_t buttons = 0;  // bit0=left bit1=right bit2=middle bit3=back bit4=forward
    int8_t wheel = 0;
    uint64_t timestamp_us = 0;
};

// AI 注入移动（经 P → scale → deadzone → smooth → clamp 后）
struct AiMove {
    int16_t dx = 0;
    int16_t dy = 0;
};

// 合并后最终移动
struct MergedMove {
    int16_t dx = 0;
    int16_t dy = 0;
    uint16_t buttons = 0;
    int8_t wheel = 0;
};

// 类别级瞄准点偏移（class_offsets[]：按 class_id + priority 覆盖默认 offset）
struct ClassOffset {
    int class_id = 0;
    float offset_x = 0.5f;  // 框内比例 0~1（0=左/上 1=右/下）
    float offset_y = 0.5f;
    int priority = 0;       // 优先级（同类别多个 offset 时取 priority 最高）
};

// 头部瞄准约束（第3项，参考 VisionForge head_aim_policy）
// 把瞄准点限制在"头框内部安全区"，防止瞄准点飘出头部（锁头稳定）。在 body 框上估算头区：
//   head_top = y1 + head_offset_top_fraction × h（默认 0.04）
//   head_bottom = y1 + (head_offset_top_fraction + head_height_fraction) × h（默认 0.04+0.28=0.32 → 上 32% 为头区）
// 约束：aim point 必须落在头区内部（带安全内缩 fraction），且相对锚点单帧滞后 ≤ max_lag。
// 只对"瞄头"生效（offset_y < 0.5）；瞄身体时不做头约束（保留身体偏移自由）。
struct HeadAimConfig {
    bool enabled = false;                 // 是否启用头区约束（默认关，保持现有行为）
    float head_offset_top_fraction = 0.04f;  // 头区顶在 body 框内的 y 比例
    float head_height_fraction = 0.28f;      // 头区高度占 body 框比例（0.28 ≈ 头占上 1/3）
    float safe_inset_fraction = 0.12f;       // 头区内安全内缩比例（0~0.45）
    float max_lag_fraction = 0.18f;          // 锚点滞后上限（相对头区尺寸比例）
    float max_lag_px = 1.25f;                // 锚点滞后上限（px，取两者较小）
};

// 瞄准点配置（AimPointProfile）：
//   默认瞄准点 = 框中心 + offset × 框尺寸；class_offsets 按类别覆盖。
//   aim_offset_x/y = 瞄准参考点（准星）偏移，crop 系像素。
struct AimPointProfile {
    float offset_x = 0.5f;
    float offset_y = 0.5f;
    float aim_offset_x = 0.0f;  // crop 系 px（crop 中心 + 偏移 = 准星）
    float aim_offset_y = 0.0f;
    std::vector<ClassOffset> class_offsets;
    int switch_delay_ms = 30;   // 类别偏移切换延迟（未启用前仅记录）
    HeadAimConfig head_aim;     // 头部瞄准约束（第3项）
};

// 拉枪曲线（pull_curve：目标距离 ≥ min_distance 时在拉枪方向附加弧线/抖动）
struct PullCurveConfig {
    bool enabled = true;
    float strength = 0.8f;       // 弧线强度（0.8）
    float jitter_px = 3.0f;      // 抖动幅度 px
    float min_distance = 80.0f;  // 激活距离（crop 系 px）
};

// 持续提前量（continuous_lead：AI 输出同向累计超 enter 后附加 X 偏置，渐入渐出）
struct ContinuousLeadConfig {
    bool enabled = false;
    float enter_distance = 150.0f;      // 触发累计距离
    float scale = 0.5f;                 // 偏置比例
    float fade_in_ms = 300.0f;
    float fade_out_ms = 300.0f;
    float near_disable_ratio = 0.66f;   // 目标接近时衰减比例（保留字段）
};

// 拟人化（humanize：目标输出附加抖动 + 曲线平滑，用于压枪与瞄准共用）
struct HumanizeConfig {
    bool enabled = true;
    float curve_strength = 0.45f;  // 曲线混合强度
    float jitter_px = 0.25f;       // 抖动幅度 px
    float jitter_frequency = 8.0f; // 抖动频率 Hz
};

// 拟人化整形引擎（personal_trajectory_shader：Fitts 时长 + 速度包络 + 垂直抖动 + 自适应抑制 + 安全守卫）
// 参考 VisionForge personal_trajectory_shaper 算法移植（见 docs/web/TTBOX_VISIONFORGE_对照分析.md 第 1 项）。
// 作用在 AimThread 输出链 move_x/move_y（int16 HID count）上、热键 Gate 之前：
//   把"恒定 PID 输出"整形为"接近真人手部动作"的移动包络（加速→减速 + 垂直随机抖动 + 安全钳制）。
// 不绕过 PID / 死区 / 热键安全门（整形后仍被 Gate 归零）。
// 区分于 personal_motion（倍率曲线，只改输出倍率）：本引擎是"完整移动轨迹整形"。
struct PersonalTrajectoryConfig {
    bool enabled = false;          // 总开关（默认关，保持现有行为）
    // -- Fitts 时长模型：目标距离 → 一次移动的期望时长（接近真人）
    float fitts_intercept_ms = 120.0f;      // 拦截常数（ms）
    float fitts_slope_ms_per_bit = 85.0f;   // 每位斜率（ms）、Fitts law 对数距离
    // -- 速度包络（transport 阶段加速/减速）
    float speed_scale = 1.0f;               // 整体速度系数（0.75~1.25，>1 更快 = 时长更短）
    float stability_scale = 1.0f;           // 稳定性系数（0.70~1.35，>1 更稳 = 时长更长）
    // -- 抖动 / 曲线（垂直向随机游走，模拟人手曲线）
    float variation_scale = 1.0f;           // 抖动幅度系数（0.40~1.80）
    float max_extra_px = 2.0f;              // 单轴最大附加量（count）
    float max_visual_variation_px = 1.5f;   // 视觉抖动上限（px）
    float curve_time_constant_ms = 32.0f;   // 抖动自相关时间常数（ms）
    float curve_rms_px = 0.8f;              // 抖动 RMS 基准（px）
    float jitter_amp_px = 0.20f;            // 抖动幅度（px）
    // -- 自适应抑制（大误差/目标快/目标老/方向突变 → 自动降强度或停用，防乱晃）
    bool adaptive_enabled = true;           // 是否启用自适应抑制
    float min_error_px = 18.0f;             // 小于此误差不抖动（close to target）
    float urgent_error_px = 72.0f;          // 超过此误差视为大误差 → 停用整形（直出）
    float urgent_speed_px_s = 520.0f;       // 目标移动速度超过此 → 停用整形
    float max_target_age_ms = 18.0f;        // 目标年龄超过此 → 停用整形
    float capture_priority_ms = 5.0f;       // 捕获前几 ms 停用整形（等镜头稳定）
    float transport_gain = 0.16f;           // 中间段增益（加速峰，0~0.2）
    float direction_change_cosine = 0.15f;  // 方向突变检测余弦阈值
    // -- 响应参数（视觉抖动预算换算：target_radius / response_px_per_count）
    float response_px_per_count = 0.65f;    // 每 count 对应 px（来自 gain_x/y_px_per_count 标定）
};

// 压枪（recoil：按住开火键期间持续下压，补偿后坐力）
// 参考 YU 压枪模块行为设计（12 参数语义一致），但输出链完全基于 TTBOX 自身：
//   压枪量在 AimThread 输出链 pull_curve 之后、deadzone 之前注入 scaled_y，
//   与 PID 输出融合后统一走 deadzone → remainder → int16 → 拟人化整形 → 热键 Gate。
// 不照搬 YU 独立 recoil 链路；默认全关，保持旧行为。
struct RecoilConfig {
    bool enabled = false;            // 总开关
    int hotkey = 0x01;               // 开火热键位掩码（1=left，复用 y_axis_fire_hotkey 语义）
    int hotkey2 = 0x00;              // 副开火热键位掩码（0=不使用）
    int hotkey_mode = 1;             // 触发方式：1=any 任一命中 2=all 同时按下
    bool only_when_target_visible = true;  // 仅有目标时才压（防空压）
    float target_lost_release_ms = 200.0f; // 目标丢失后仍压时长（ms），0=立即释放
    bool trigger_delay_enabled = false;    // 延迟触发开关（防单点误触）
    float trigger_delay_ms = 120.0f;       // 按住超过该时长才开始压（松开重新计时）
    float strength = 0.0f;           // 下压速率基准（px/s，0=不输出）
    float speed = 1.0f;              // 下压倍率（乘在 strength 上）
    bool humanize_enabled = true;    // 拟人化开关（缓入缓出 + X 轴微动）
    float humanize_curve_strength = 0.45f;  // 下压拆步缓入缓出比例
    float humanize_jitter_px = 0.25f;       // 压枪时附加 X 轴微动幅度（px）
    float humanize_jitter_frequency = 8.0f; // X 轴微动变化频率（Hz）
};

// 目标锁定确认配置（ENTER/HOLD 双阈值 + 确认帧 + instant-enter，第2项）
// 参考 VisionForge control_gate：新目标需更高置信度连续确认，已锁目标用较低阈值保持（防闪烁），
// 近距离高置信目标跳过确认窗（快瞄）。
struct LockConfirmConfig {
    int confirmation_frames = 1;      // 新目标连续确认帧数（默认 1=首帧即锁，兼容旧行为）
    float enter_conf = 0.0f;          // 进入（新锁）置信度阈值
    float hold_conf = 0.0f;           // 保持（已锁）置信度阈值（<= enter）
    bool instant_enter_enabled = true; // 近距离高置信目标跳过确认窗
    float instant_enter_dist = 105.0f; // instant-enter 距离阈值（px）
    float instant_enter_conf = 0.50f;  // instant-enter 置信度阈值
};

// TTBOX 个人移动曲线模型：只保存已训练模型的安全运行参数。
// 原始训练样本留在 Gateway 的独立 profile.json，Core 热路径只读 knots。
struct PersonalMotionConfig {
    bool enabled = false;
    float curve_blend = 1.0f;
    float speed_blend = 1.0f;
    float reaction_blend = 0.7f;
    float max_reaction_delay_ms = 250.0f;
    std::vector<float> knots;
};

// 鼠标配置（RuntimeProfile.mouse，与模型彻底分离）
struct MouseProfile {
    bool enabled = false;                       // AI 注入总开关（false = 纯物理透传，与 A9 一致）
    MouseProxyMode proxy_mode = MouseProxyMode::kFullPassthrough;  // V1 仅 full_passthrough
    uint8_t aim_hotkey = 0x02;                  // 瞄准主热键位掩码：1=left 2=right 4=middle 8=back 16=forward
    uint8_t aim_hotkey2 = 0x00;                 // 瞄准副热键位掩码（0=不使用）
    int aim_hotkey_mode = 0;                    // 触发方式：0=任一按键(any) 1=同时按下(all)
    float fov_range = 1.0f;                     // 目标选择范围（0~1，仅影响目标选择）
    float confidence = 0.25f;                   // 目标置信度阈值（目标选择）
    float prediction_s = 0.0f;                  // 预测时间（s）：predicted = pos + vel × prediction_s
    // PID 默认值以用户提供的 pid1.cpp 权威参数为准（X: kp=25 kd=25 predict=3 rate=0.3；Y: predict=0）
        float kp_x = 25.0f;                         // X 比例增益（P 控制）
        float kp_y = 25.0f;
        float ki_x = 0.0f;                          // 预留（V1 纯 P，不使用）
        float ki_y = 0.0f;
        float kd_x = 25.0f;                         // 微分增益（pid1 刹车，防过冲）
        float kd_y = 25.0f;
    // A10.1：FOV 角度换算模式（参考 PD Aim fov 算法，可选）
    bool fov_mode = false;                      // true = 角度换算输出（替代 kp×err）
    float hfov = 83.105f;                       // 水平视场角（度）
    float vfov = 53.0f;                         // 垂直视场角（度）
    float move_speed_x = 500.0f;                // X 每整圈移动像素（角度换算）
    float move_speed_y = 500.0f;                // Y 每整圈移动像素（角度换算）
    int aim_part = 0;                           // 瞄准部位：0=脚 10=头（offset_y=1.0-ap*0.09）
    float rate_x = 0.3f;                        // 输出速率（X 独立；pid1 kp_gain_rate=0.3）
        float rate_y = 0.3f;
    float sensitivity = 1.0f;                   // 灵敏度
    float output_scale = 1.0f;                  // 输出缩放（与 fov_range 严格分离）
    // 标定产物：游戏灵敏度（px/count）——鼠标 1 count = 画面多少 px。
    // 输出换算：count = kp×err / gain（px → count 正确换算，防单位错乱过冲）。
    float gain_x_px_per_count = 0.65f;          // X 轴（标定测得；默认 0.65 近似）
    float gain_y_px_per_count = 0.65f;          // Y 轴
    float response_delay_ms = 0.0f;             // 输入到画面反馈延迟
    float smith_dead_ms = 28.4f;                 // Smith 在途窗口
    float alpha = 0.8f;                          // α-β-γ 位置增益
    float beta = 0.3f;
    float gamma = 0.1f;
    float predict_dt_ms = 50.0f;                 // 目标提前预测时间             // 输入→画面响应延迟（标定测得）
    float deadzone_x = 1.0f;                    // X 死区（count，|v|<dz → 0）
    float deadzone_y = 1.0f;
    float smooth = 0.0f;                        // 平滑低通 alpha（0~1；0=关闭；TTBox 自实现）
    // controller 公式（kp×rate×err + predict×vel）与输出链参数
    float predict_x = 3.0f;                   // pid1 X 前馈 3.0（追左右移动目标）
        float predict_y = 0.0f;                   // pid1 Y 不带前馈（仅位置纠正）
    float smooth_x = 9900.0f;                   // smooth 参考值（9900≈不过滤；TTBox 用 smooth 0~1 兼容）
    float smooth_y = 9900.0f;
    float output_deadzone = 1.0f;               // output_deadzone（自适应死区基准）
    float selector_search_radius = 170.0f;      // selector_search_radius
    bool aim_fire_lock_y = false;               // 开火锁 Y
    int y_axis_fire_hotkey = 0x01;              // 开火热键位掩码（1=left）
    float y_axis_fire_release_delay_sec = 0.3f; // 开火锁 Y 释放延迟
    // 插件配置（pull_curve / continuous_lead / humanize）
        PullCurveConfig pull_curve;
        ContinuousLeadConfig continuous_lead;
        HumanizeConfig humanize;
        PersonalMotionConfig personal_motion;
        PersonalTrajectoryConfig personal_trajectory;  // 拟人化整形引擎（Fitts 时长+包络+垂直抖动+自适应抑制+安全守卫）
    RecoilConfig recoil;                    // 压枪（输出链 pull_curve 后、deadzone 前注入 scaled_y）
    AimPointProfile aim_point;
    float lost_grace_ms = 78.0f;                // 目标丢失宽限期
    LockConfirmConfig lock_confirm;                 // 目标锁定确认（ENTER/HOLD + instant-enter，第2项）
    // A11 标定闭环：calibrating 强制 AIMING；calibration_bias_* 把准星带到偏置位再拉回
    bool calibrating = false;                   // 标定模式（自瞄全程输出，用偏置测闭环响应）
    float calibration_bias_x = 0.0f;            // 标定偏置 px（加在参考点上，自瞄自动拉到该点）
    float calibration_bias_y = 0.0f;
    bool block_physical_x = false;              // 瞄准时屏蔽物理 X
    bool block_physical_y = false;              // 瞄准时屏蔽物理 Y
};

}  // namespace ttbox::core::aim
