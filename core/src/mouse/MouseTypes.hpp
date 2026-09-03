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
};

// 拉枪曲线（pull_curve，YU：目标距离 ≥ min_distance 时在拉枪方向附加弧线/抖动）
struct PullCurveConfig {
    bool enabled = true;
    float strength = 0.8f;       // 弧线强度（YU 0.8）
    float jitter_px = 3.0f;      // 抖动幅度 px
    float min_distance = 80.0f;  // 激活距离（crop 系 px）
};

// 持续提前量（continuous_lead，YU：AI 输出同向累计超 enter 后附加 X 偏置，渐入渐出）
struct ContinuousLeadConfig {
    bool enabled = false;
    float enter_distance = 150.0f;      // 触发累计距离
    float scale = 0.5f;                 // 偏置比例
    float fade_in_ms = 300.0f;
    float fade_out_ms = 300.0f;
    float near_disable_ratio = 0.66f;   // 目标接近时衰减比例（保留字段）
};

// 拟人化（humanize，YU：目标输出附加抖动 + 曲线平滑，用于压枪与瞄准共用）
struct HumanizeConfig {
    bool enabled = true;
    float curve_strength = 0.45f;  // 曲线混合强度
    float jitter_px = 0.25f;       // 抖动幅度 px
    float jitter_frequency = 8.0f; // 抖动频率 Hz
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
    float kp_x = 17.0f;                         // X 比例增益（P 控制）
    float kp_y = 10.0f;
    float ki_x = 0.0f;                          // 预留（V1 纯 P，不使用）
    float ki_y = 0.0f;
    float kd_x = 0.0f;                          // 预留
    float kd_y = 0.0f;
    // A10.1：FOV 角度换算模式（参考 PD Aim fov 算法，可选）
    bool fov_mode = false;                      // true = 角度换算输出（替代 kp×err）
    float hfov = 83.105f;                       // 水平视场角（度）
    float vfov = 53.0f;                         // 垂直视场角（度）
    float move_speed_x = 500.0f;                // X 每整圈移动像素（角度换算）
    float move_speed_y = 500.0f;                // Y 每整圈移动像素（角度换算）
    int aim_part = 0;                           // 瞄准部位：0=脚 10=头（offset_y=1.0-ap*0.09）
    float rate_x = 1.0f;                        // 输出速率（X 独立）
    float rate_y = 1.0f;
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
    float smooth = 0.0f;                        // 平滑低通 alpha（0~1；0=关闭；TTBox 自实现，非 YU 公式）
    // YU 对齐：controller 公式（kp×rate×err + predict×vel）与输出链参数
    float predict_x = 0.008f;                   // 预测提前量（秒，≈1 帧@125Hz；旧值 0.5s 会在
                                                // 速度 clamp 下产生 1250px 提前量 → 转圈，已修）
    float predict_y = 0.008f;
    float smooth_x = 9900.0f;                   // YU smooth（9900≈不过滤；TTBox 用 smooth 0~1 兼容）
    float smooth_y = 9900.0f;
    float output_deadzone = 1.0f;               // YU output_deadzone（自适应死区基准）
    float selector_search_radius = 170.0f;      // YU selector_search_radius
    bool aim_fire_lock_y = false;               // 开火锁 Y
    int y_axis_fire_hotkey = 0x01;              // 开火热键位掩码（1=left）
    float y_axis_fire_release_delay_sec = 0.3f; // 开火锁 Y 释放延迟
    // 插件配置（pull_curve / continuous_lead / humanize）
    PullCurveConfig pull_curve;
    ContinuousLeadConfig continuous_lead;
    HumanizeConfig humanize;
    PersonalMotionConfig personal_motion;
    AimPointProfile aim_point;
    float lost_grace_ms = 78.0f;                // 目标丢失宽限期
    // A11 标定闭环：calibrating 强制 AIMING；calibration_bias_* 把准星带到偏置位再拉回
    bool calibrating = false;                   // 标定模式（自瞄全程输出，用偏置测闭环响应）
    float calibration_bias_x = 0.0f;            // 标定偏置 px（加在参考点上，自瞄自动拉到该点）
    float calibration_bias_y = 0.0f;
    bool block_physical_x = false;              // 瞄准时屏蔽物理 X
    bool block_physical_y = false;              // 瞄准时屏蔽物理 Y
};

}  // namespace ttbox::core::aim
