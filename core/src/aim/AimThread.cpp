// AimThread.cpp — 独立瞄准线程最小可验证实现。
#include "aim/AimThread.hpp"
#include <chrono>
#include <utility>
#include "aim/AimError.hpp"
#include "mouse/FovAngle.hpp"
#include "mouse/CoordinateTransform.hpp"
#include "mouse/AimPointProfile.hpp"
#include "mouse/PersonalMotion.hpp"
namespace ttbox::core::aim {
bool AimThread::start(AimTargetMailbox* mailbox, std::shared_ptr<output::IHidOutput> output, int interval_us, RuntimeConfig* runtime_config, std::atomic<uint16_t>* physical_buttons) {
    if (!mailbox || !output || running_.exchange(true)) return false;
    mailbox_ = mailbox; output_ = std::move(output); interval_us_ = interval_us > 0 ? interval_us : 4000; runtime_config_ = runtime_config; physical_buttons_ = physical_buttons;
    { std::lock_guard<std::mutex> lk(status_mutex_); status_ = {}; status_.running = true; }
    // pid1.cpp main() 原始参数：X predict=3.0，Y predict=0.0。
    pid_x_.init(25.0, 25.0, 3.0, 0.3, 9900.0);
    pid_y_.init(25.0, 25.0, 0.0, 0.3, 9900.0);
    thread_ = std::thread(&AimThread::loop, this);
    return true;
}
void AimThread::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lk(status_mutex_); status_.running = false;
}
AimThread::Status AimThread::status() const { std::lock_guard<std::mutex> lk(status_mutex_); return status_; }
void AimThread::loop() {
    uint64_t last_frame = 0;
    while (running_.load(std::memory_order_acquire)) {
        AimTargetTask task;
        if (mailbox_->take_latest(&task, last_frame)) {
            last_frame = task.frame_number;
            const uint64_t previous_timestamp_us = last_timestamp_us_;
            // 新控制链：目标选择 → 误差 → 纯 PID/P 控制 → OutputAction。
            TargetSelectorConfig scfg;
            scfg.roi_w = task.frame_width; scfg.roi_h = task.frame_height;
            // 用户置信度阈值（RuntimeProfile.mouse.confidence）：0 = 用模型默认（选 0.25 底线）。
            // 修复点：此前写死 0.0f 导致 Web 置信度阈值参数无效（中看不中用）。
            float out_sensitivity = 1.0f, out_scale = 1.0f;
            float out_deadzone = 1.0f;
            PersonalMotionConfig personal_motion;
            float kp_x = 0.0f, kp_y = 0.0f, kd_x = 0.0f, kd_y = 0.0f;
            AimPointProfile aim_point;
            if (runtime_config_) {
                auto profile = runtime_config_->snapshot();
                if (profile) {
                    scfg.fov_range = profile->fov.enabled ? profile->fov.radius * 2.0f : 1.0f;
                    scfg.lost_grace_ms = profile->mouse.lost_grace_ms;
                    scfg.confidence = profile->mouse.confidence > 0.0f
                                          ? profile->mouse.confidence : 0.25f;
                    scfg.aim_ratio_x = profile->mouse.aim_point.offset_x;
                    scfg.aim_ratio_y = profile->mouse.aim_point.offset_y;
                    kp_x = profile->mouse.kp_x; kp_y = profile->mouse.kp_y;
                    kd_x = profile->mouse.kd_x; kd_y = profile->mouse.kd_y;
                    aim_point = profile->mouse.aim_point;
                    // 输出链参数（YU 对齐）：sens 全局缩放 × output_scale × output_deadzone
                    out_sensitivity = profile->mouse.sensitivity;
                    out_scale = profile->mouse.output_scale;
                    out_deadzone = profile->mouse.output_deadzone;
                    personal_motion = profile->mouse.personal_motion;
                    pid_x_.configure(kp_x, kd_x, profile->mouse.predict_x,
                                     profile->mouse.rate_x, profile->mouse.smooth_x);
                    pid_y_.configure(kp_y, kd_y, profile->mouse.predict_y,
                                     profile->mouse.rate_y, profile->mouse.smooth_y);
                }
            }
            const auto selected = selector_.select(task.detections, scfg,
                static_cast<uint32_t>(task.timestamp_us / 1000ULL));
            // ---- Hotkey Gate 输入解析（每周期独立计算，AI 链路照常运行）----
            // any 模式：主键或副键任一按下即触发；all 模式：两者同时按下。
            uint16_t hotkey_bits = 0;
            bool injection_allowed = false;
            if (physical_buttons_) hotkey_bits = physical_buttons_->load(std::memory_order_acquire);
            if (runtime_config_) {
                auto p = runtime_config_->snapshot();
                if (p) {
                    const bool a = (hotkey_bits & p->mouse.aim_hotkey) != 0;
                    const bool b = p->mouse.aim_hotkey2 != 0 && (hotkey_bits & p->mouse.aim_hotkey2) != 0;
                    // 鼠标五键统一位图：左1、右2、中4、侧1 8、侧2 16。
                    // 热键位全部来自用户配置快照（每周期重读 → 改配置即时生效，无需重启）。
                    // mouse.enabled 是总开关；any 模式主/副键任一命中即可，all 模式需同时按下。
                    // A11 标定闭环：calibrating=true 期间无视物理热键强制放行
                    // （标定线程注入运动帧，物理鼠标不参与），与 C 桥 compute_aiming 语义一致。
                    injection_allowed = p->mouse.calibrating ||
                                        (p->mouse.enabled &&
                                        (p->mouse.aim_hotkey_mode == 1 ? (a && b) : (a || b)));
                }
            }
            AimStateEvent event; event.has_target = selected.valid;
            event.hotkey_active = injection_allowed;
            event.now_ms = task.timestamp_us / 1000ULL;
            if (state_machine_.update(event, scfg.lost_grace_ms)) { controller_.reset(); pid_x_.reset(); pid_y_.reset(); remainder_x_=0.0f; remainder_y_=0.0f; last_target_id_=-1; }
            int16_t move_x = 0, move_y = 0; float ex = 0.0f, ey = 0.0f;
            float tx = 0.0f, ty = 0.0f, ref_x = 0.0f, ref_y = 0.0f;
            float aibox_x = 0.0f, aibox_y = 0.0f, scaled_x = 0.0f, scaled_y = 0.0f;
            float trace_control_x = 0.0f, trace_control_y = 0.0f;
            float trace_smith_dx = 0.0f, trace_smith_dy = 0.0f;
            // ---- Hotkey Gate：最终输出安全边界 ----
            // 热键未按下时：无论目标/误差/PID 状态如何，
            // 本周期一律跳过移动计算（不积分、不累计余数），
            // 只保留误差遥测；最终发送的 OutputAction 强制 dx=dy=0。
            // AI 链路（mailbox→selector→误差遥测）不受热键影响，始终运行。
            if (!injection_allowed) {
                pid_x_.reset();      // 清在途量/PID 状态，防止旧状态绕过 Gate
                pid_y_.reset();
                remainder_x_ = 0.0f;
                remainder_y_ = 0.0f;
            }
            if (selected.valid && task.frame_width > 0 && task.frame_height > 0) {
                if (!aim_point_at(selected.box, selected.box.class_id, aim_point, &tx, &ty)) {
                    tx = (selected.box.x1 + selected.box.x2) * 0.5f;
                    ty = selected.box.y1 + (selected.box.y2 - selected.box.y1) * 0.15f;
                }
                if (last_target_id_ != -1 && selected.target_id != last_target_id_) {
                    // 目标切换：速度/加速度来自旧目标，必须清除预测状态。
                    pid_x_.reset(); pid_y_.reset(); controller_.reset(); remainder_x_ = remainder_y_ = 0.0f;
                }
                last_target_id_ = selected.target_id;
                // AIBOX 对标：不做位置外推；误差直接来自本帧检测结果。
                // 速度信息只进入 P_PID 的前馈/Kalman，不在目标坐标层 coast。
                CoordinateTransform::reference_point(static_cast<float>(task.frame_width),
                                                     static_cast<float>(task.frame_height),
                                                     aim_point, &ref_x, &ref_y);
                ex = tx - ref_x;
                ey = ty - ref_y;
                float control_x = ex;
                float control_y = ey;
                if (runtime_config_) {
                    auto profile = runtime_config_->snapshot();
                    if (profile) {
                        // 自动标定偏置进入同一控制误差域，复用正式 PID/输出链测量响应。
                        if (profile->mouse.calibrating) {
                            control_x += profile->mouse.calibration_bias_x;
                            control_y += profile->mouse.calibration_bias_y;
                        }
                        if (profile->mouse.fov_mode) {
                        // FOV 模式：先将像素误差转换为角度对应的鼠标移动量。
                        control_x = fov_move_x(ex, static_cast<float>(task.frame_width),
                                               profile->mouse.hfov, profile->mouse.move_speed_x);
                        control_y = fov_move_y(ey, static_cast<float>(task.frame_height),
                                               profile->mouse.vfov, profile->mouse.move_speed_y);
                        }
                    }
                }
                const float dt = previous_timestamp_us > 0 && task.timestamp_us > previous_timestamp_us
                    ? static_cast<float>(task.timestamp_us - previous_timestamp_us) / 1000000.0f : 0.004f;
                (void)dt;
                // pid1.cpp P_PID 直接消费控制域误差（FOV 开启时已是 HID count 域）。
                trace_smith_dx = 0.0f; trace_smith_dy = 0.0f;
                trace_control_x = control_x; trace_control_y = control_y;
                // pid1.cpp P_PID：X predict=3.0，Y predict=0（main() 原始参数）。
                aibox_x = static_cast<float>(pid_x_.update(control_x));
                aibox_y = static_cast<float>(pid_y_.update(control_y));
                // 输出链（YU 对齐）：P_PID 输出 × sens（全局灵敏度） × output_scale。
                // rate_x/y 已在 Pid1 内部作为 kp_gain_rate 消费，此处不再重复。
                const float out_gain = out_sensitivity * out_scale;
                scaled_x = aibox_x * out_gain;
                scaled_y = aibox_y * out_gain;
                // 个人曲线只改变输出倍率，不绕过 PID、死区和热键安全门。
                const float personal_distance = std::hypot(control_x, control_y);
                const float personal_gain = PersonalMotion{}.scale(personal_distance, personal_motion);
                scaled_x *= personal_gain;
                scaled_y *= personal_gain;
                // output_deadzone（YU 自适应死区基准）：低于死区的输出归零（防微抖）。
                if (std::abs(scaled_x) < out_deadzone) scaled_x = 0.0f;
                if (std::abs(scaled_y) < out_deadzone) scaled_y = 0.0f;
                // 保留小数余量，避免小幅连续误差被整数 HID count 截断。
                remainder_x_ += scaled_x; remainder_y_ += scaled_y;
                // int16 截断保护：单帧输出 clamp 到 HID count 范围（-32768..32767），
                // 防异常大值 static_cast 产生实现定义行为（乱飞）。
                constexpr float kHidMax = 32767.0f;
                constexpr float kHidMin = -32768.0f;
                const float cx_f = std::clamp(remainder_x_, kHidMin, kHidMax);
                const float cy_f = std::clamp(remainder_y_, kHidMin, kHidMax);
                move_x = static_cast<int16_t>(cx_f);
                move_y = static_cast<int16_t>(cy_f);
                remainder_x_ -= static_cast<float>(move_x); remainder_y_ -= static_cast<float>(move_y);
                // 兜底：若余数已非有限（理论上 validate 已挡），立即清零防持续乱飞
                if (!std::isfinite(remainder_x_) || !std::isfinite(remainder_y_)) {
                    remainder_x_ = 0.0f; remainder_y_ = 0.0f;
                    pid_x_.reset(); pid_y_.reset();
                }
            }
            // ---- Hotkey Gate 兜底（安全边界最后一行）----
            // 无论前面算出什么，热键未按下时最终动作强制归零。
            if (!injection_allowed) {
                move_x = 0;
                move_y = 0;
            }
            output_->send(output::OutputAction{move_x, move_y, 0, 0, task.frame_number, task.timestamp_us});
            last_timestamp_us_ = task.timestamp_us;
            std::lock_guard<std::mutex> lk(status_mutex_);
            status_.has_task = true;
            status_.detection_boxes = task.detections;
            status_.has_target = selected.valid;
            status_.target_id = selected.valid ? selected.target_id : -1;
            status_.target_class_id = selected.valid ? selected.box.class_id : -1;
            status_.target_width = selected.valid ? selected.box.x2 - selected.box.x1 : 0.0f;
            status_.target_height = selected.valid ? selected.box.y2 - selected.box.y1 : 0.0f;
            // 显示框使用同一目标的关联检测框并集，避免只显示头/躯干局部框。
            // 控制链仍使用 selected.box，显示框扩展不会改变瞄准行为。
            if (selected.valid) {
                float display_x1 = selected.box.x1;
                float display_y1 = selected.box.y1;
                float display_x2 = selected.box.x2;
                float display_y2 = selected.box.y2;
                const float selected_cx = (selected.box.x1 + selected.box.x2) * 0.5f;
                const float selected_cy = (selected.box.y1 + selected.box.y2) * 0.5f;
                const float selected_w = std::max(1.0f, selected.box.x2 - selected.box.x1);
                const float selected_h = std::max(1.0f, selected.box.y2 - selected.box.y1);
                for (const auto& candidate : task.detections) {
                    if (&candidate == &selected.box) continue;
                    const float candidate_cx = (candidate.x1 + candidate.x2) * 0.5f;
                    const float candidate_cy = (candidate.y1 + candidate.y2) * 0.5f;
                    const bool vertical_overlap = candidate.y2 >= selected.box.y1 &&
                                                  candidate.y1 <= selected.box.y2;
                    const bool horizontal_near = std::fabs(candidate_cx - selected_cx) <=
                                                 std::max(selected_w * 1.5f, 120.0f);
                    const bool vertical_near = std::fabs(candidate_cy - selected_cy) <= selected_h * 0.75f;
                    if (vertical_overlap && horizontal_near && vertical_near) {
                        display_x1 = std::min(display_x1, candidate.x1);
                        display_y1 = std::min(display_y1, candidate.y1);
                        display_x2 = std::max(display_x2, candidate.x2);
                        display_y2 = std::max(display_y2, candidate.y2);
                    }
                }
                status_.target_x1 = display_x1;
                status_.target_y1 = display_y1;
                status_.target_x2 = display_x2;
                status_.target_y2 = display_y2;
            } else {
                status_.target_x1 = 0.0f;
                status_.target_y1 = 0.0f;
                status_.target_x2 = 0.0f;
                status_.target_y2 = 0.0f;
            }
            if (selected.valid) ++status_.target_frames; else ++status_.no_target_frames;
            uint32_t active_tracks = 0;
            for (const auto& te : selector_.tracks()) {
                if (te.active) ++active_tracks;
            }
            status_.tracks = active_tracks;
            status_.predicted_x = selected.valid ? (selected.box.x1 + selected.box.x2) * 0.5f : 0.0f;
            status_.predicted_y = selected.valid ? (selected.box.y1 + (selected.box.y2 - selected.box.y1) * 0.15f) : 0.0f;
            status_.target_point_x = selected.valid ? tx : 0.0f;
            status_.target_point_y = selected.valid ? ty : 0.0f;
            status_.reference_x = selected.valid ? ref_x : 0.0f;
            status_.reference_y = selected.valid ? ref_y : 0.0f;
            status_.error_x = ex;
            status_.error_y = ey;
            status_.pid_output_x = selected.valid ? aibox_x : 0.0f;
            status_.pid_output_y = selected.valid ? aibox_y : 0.0f;
            status_.scheduler_input_x = selected.valid ? scaled_x : 0.0f;
            status_.scheduler_input_y = selected.valid ? scaled_y : 0.0f;
            status_.control_x = trace_control_x;
            status_.control_y = trace_control_y;
            status_.smith_dx = trace_smith_dx;
            status_.smith_dy = trace_smith_dy;
            status_.move_x = move_x;
            status_.move_y = move_y;
            if (status_.consumed == 0) {
                status_.min_move_x = status_.max_move_x = move_x;
                status_.min_move_y = status_.max_move_y = move_y;
            } else {
                status_.min_move_x = std::min(status_.min_move_x, move_x);
                status_.max_move_x = std::max(status_.max_move_x, move_x);
                status_.min_move_y = std::min(status_.min_move_y, move_y);
                status_.max_move_y = std::max(status_.max_move_y, move_y);
            }
            if (move_x <= -127 || move_x >= 127 || move_y <= -127 || move_y >= 127) ++status_.clipped_frames;
            if (!injection_allowed) ++status_.gated_frames;
            status_.last_hotkey_bits = hotkey_bits;
            status_.last_injection_allowed = injection_allowed;
            status_.last_timestamp_us = task.timestamp_us;
            status_.last_frame = task.frame_number;
            ++status_.consumed;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(interval_us_));
    }
}
}  // namespace ttbox::core::aim
