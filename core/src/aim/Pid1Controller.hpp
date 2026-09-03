// Pid1Controller.hpp — 用户提供的 pid1.cpp P_PID 1:1 移植（X/Y 两轴共用）。
//
// 算法来源：外部参考实现 pid1.cpp（2026-08 用户提供）。本文件只做移植与
// 热更新接线，不改任何公式、不叠加旧控制器。
// 与旧 AiboxPpidController 的差异（均为 pid1 原始行为，非本仓库改动）：
//   1) 速度 Kalman 的输入是 (error_diff + last_u)，而非仅 error_diff；
//   2) smooth 非 0 时才启用 soft-limit；smooth=0 时 Kp/Ki/Kd 直通；
//   3) K_i 的 soft outputScale 固定为 bandwidth-1000。
/*
 * TTBOX 文件说明
 *
 * 文件：Pid1Controller.hpp
 *
 * 作用：
 *   PID 控制器的实现。
 *   计算鼠标移动量，使鼠标准星平滑地跟踪目标。
 *
 * 小白理解：
 *   PID 控制器的目标是让鼠标准星和目标的偏差缩小到 0。
 *   它用三个参数来控制：
 *   - P（比例）：偏差越大，移动越快
 *   - I（积分）：长期偏差，慢慢纠正
 *   - D（微分）：防止超调，刹车作用
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#pragma once
#include <algorithm>
#include <cmath>
namespace ttbox::core::aim {

class Pid1Controller {
public:
    static constexpr double kBandwidth = 10000.0;

    Pid1Controller() = default;

    // 与 pid1.cpp P_PID::init 完全一致。
    bool init(double kp_in, double kd_in, double predict_in,
              double rate_in, double smooth_in) {
        kp = kp_in;
        kd = kd_in;
        bandwidth = kBandwidth;
        smooth = smooth_in;
        kp_gain_rate = rate_in;
        predict = predict_in;
        ki_deadband = 0.5;
        return true;
    }

    // 运行中参数热更新（AimThread 每帧从 RuntimeProfile 接线用）：
    // 只赋参数，不重置控制器内部状态（与旧 configure 语义一致）。
    void configure(double kp_in, double kd_in, double predict_in,
                   double rate_in, double smooth_in) {
        kp = kp_in;
        kd = kd_in;
        predict = predict_in;
        kp_gain_rate = rate_in;
        smooth = smooth_in;
        bandwidth = kBandwidth;
    }

    // 与 pid1.cpp P_PID::update 完全一致。
    double update(double error) {
        if (std::abs(error) < 0.3) error = 0.0;
        if (std::abs(error - last_error) > 30.0) reset();

        adjust_integral(error);
        kp_integral(error);

        double error_diff = error - last_error;
        double target_velocity = error_diff + last_u;
        target_velocity = update_velocity_filter(target_velocity);

        double raw_velocity_input = target_velocity;
        if (std::abs(error) < 1.0 && std::abs(error_diff) < 0.1) {
            raw_velocity_input = error_diff + last_u * 0.5;
        }

        double ki_raw = raw_velocity_input;
        ki_raw = (std::abs(ki_raw) > 0.5) ? ki_raw : 0.0;
        ki_raw = (ki_raw * predict) * integral_gain;
        ki_raw = update_integral_filter(ki_raw);

        double K_p = kp * error;
        double K_i = ki_raw;
        double K_d = kd * (error - last_error);

        if (smooth) {
            K_p = smoothTerm(K_p, bandwidth, bandwidth - smooth);
            K_i = smoothTerm(K_i, bandwidth, bandwidth - 1000.0);
            K_d = smoothTerm(K_d, bandwidth, bandwidth - smooth);
        }

        double u = K_p + K_i + K_d;
        u_filtered = u * kp_gain;
        last_u = u_filtered;
        last_error = error;
        last_integral_term = integral_term;
        return u_filtered;
    }

    // 与 pid1.cpp P_PID::reset 完全一致。
    void reset() {
        kp_gain = 0.0;
        integral_gain = 0.0;
        u_filtered = 0.0;
        last_error = 0.0;
        integral_term = 0.0;
        last_u = 0.0;
        velocity_filter_x = 0.0;
        velocity_filter_p = 0.0;
        integral_filter_x = 0.0;
        integral_filter_p = 0.0;
    }

private:
    // 与 pid1.cpp smoothTerm 完全一致。
    double smoothTerm(double value, double bandwidth_v, double outputScale) const {
        double ratio = value / bandwidth_v;
        double squared = ratio * ratio;
        return (ratio * (1.0 + kSoftLimitNumerator * squared) /
            (1.0 + kSoftLimitDenominator * squared)) * outputScale;
    }

    void adjust_integral(double error) {
        double abs_error = std::abs(error);
        if (abs_error < integral_gain_threshold) {
            double ratio = 1.0 - (abs_error / integral_gain_threshold);
            integral_gain += (ratio - integral_gain) * integral_gain_rate;
        } else {
            double ratio = integral_gain_threshold / abs_error;
            integral_gain += (ratio * integral_gain - integral_gain) * 0.1;
        }
        integral_gain = std::clamp(integral_gain, 0.0, 1.0);
    }

    void kp_integral(double error) {
        double abs_error = std::abs(error);
        if (abs_error < kp_gain_threshold) {
            double ratio = 1.0 - (abs_error / kp_gain_threshold);
            kp_gain += (ratio - kp_gain) * kp_gain_rate;
        } else {
            double ratio = kp_gain_threshold / abs_error;
            kp_gain += (ratio * kp_gain - kp_gain) * 0.1;
        }
        kp_gain = std::clamp(kp_gain, 0.0, 1.0);
    }

    double update_velocity_filter(double measurement) {
        constexpr double q = 0.01;
        constexpr double r = 1.0;
        double predicted_x = velocity_filter_x;
        double predicted_p = velocity_filter_p + q;
        double k = predicted_p / (predicted_p + r);
        velocity_filter_x = predicted_x + k * (measurement - predicted_x);
        velocity_filter_p = (1 - k) * predicted_p;
        return velocity_filter_x;
    }

    double update_integral_filter(double measurement) {
        constexpr double q = 0.5;
        constexpr double r = 1.0;
        double predicted_x = integral_filter_x;
        double predicted_p = integral_filter_p + q;
        double k = predicted_p / (predicted_p + r);
        integral_filter_x = predicted_x + k * (measurement - predicted_x);
        integral_filter_p = (1 - k) * predicted_p;
        return integral_filter_x;
    }

    static constexpr double kSoftLimitNumerator = 4.0 / 15.0;
    static constexpr double kSoftLimitDenominator = 3.0 / 5.0;

    double kp = 0.0;
    double kd = 0.0;
    double bandwidth = 0.0;
    double smooth = 0.0;
    double predict = 0.0;
    double ki_deadband = 0.0;

    double u_filtered = 0.0;
    double integral_term = 0.0;
    double last_integral_term = 0.0;
    double last_error = 0.0;
    double last_u = 0.0;

    double kp_gain = 0.0;
    double kp_gain_threshold = 1920.0;
    double kp_gain_rate = 0.03;

    double integral_gain = 0.0;
    double integral_gain_threshold = 50.0;
    double integral_gain_rate = 0.025;

    double velocity_filter_x = 0.0;
    double velocity_filter_p = 1.0;
    double integral_filter_x = 0.0;
    double integral_filter_p = 1.0;
};

}  // namespace ttbox::core::aim
