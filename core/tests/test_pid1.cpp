// test_pid1.cpp — pid1.cpp P_PID 1:1 移植逐点对照测试。
//
// 对照方法：把 pid1.cpp 原文的 P_PID 类原样内联进本测试（g_test_pid），
// 用同一输入序列驱动 Pid1Controller 与 g_test_pid，逐点断言输出一致。
// 若移植与参考实现有任何公式/状态差异，此处即失败。
#include <cmath>
#include <cstdio>
#include <vector>

#include "aim/Pid1Controller.hpp"

using ttbox::core::aim::Pid1Controller;

// ---- pid1.cpp 原文（只保留类，去掉 main/runAxis；语义零改动）----
namespace {

constexpr double PID_BANDWIDTH = 10000.0;
constexpr double SOFT_LIMIT_NUMERATOR = 4.0 / 15.0;
constexpr double SOFT_LIMIT_DENOMINATOR = 3.0 / 5.0;

double smoothTerm(double value, double bandwidth, double outputScale) {
    double ratio = value / bandwidth;
    double squared = ratio * ratio;
    return (ratio * (1.0 + SOFT_LIMIT_NUMERATOR * squared) /
        (1.0 + SOFT_LIMIT_DENOMINATOR * squared)) * outputScale;
}

class P_PID {
public:
    P_PID() = default;

    bool init(double kp_, double kd_, double predict_, double rate_, double smooth_) {
        kp = kp_;
        kd = kd_;
        bandwidth = PID_BANDWIDTH;
        smooth = smooth_;
        kp_gain_rate = rate_;
        predict = predict_;
        ki_deadband = 0.5;
        return true;
    }

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

}  // namespace

int main() {
    int fails = 0;
    const double eps = 1e-9;
    const std::vector<double> errors = {
        -160.0, -120.0, -80.0, -40.0, -20.0, -10.0, -5.0, -2.0,
        -1.0, -0.2, 0.0, 0.2, 1.0, 2.0, 5.0, 10.0, 20.0,
        40.0, 80.0, 120.0, 160.0
    };

    // X 轴参数：kp=25 kd=25 predict=3.0 rate=0.3 smooth=9900（pid1 main 原样）
    {
        P_PID ref;
        ref.init(25.0, 25.0, 3.0, 0.3, 9900.0);
        Pid1Controller dut;
        dut.init(25.0, 25.0, 3.0, 0.3, 9900.0);
        for (size_t i = 0; i < errors.size(); ++i) {
            const double r = ref.update(errors[i]);
            const double d = dut.update(errors[i]);
            if (std::abs(r - d) > eps) {
                std::printf("X 轴第 %zu 步不匹配: ref=%.9f dut=%.9f\n", i, r, d);
                ++fails;
            }
        }
    }

    // Y 轴参数：kp=25 kd=25 predict=0.0 rate=0.3 smooth=9900（pid1 main 原样）
    {
        P_PID ref;
        ref.init(25.0, 25.0, 0.0, 0.3, 9900.0);
        Pid1Controller dut;
        dut.init(25.0, 25.0, 0.0, 0.3, 9900.0);
        for (size_t i = 0; i < errors.size(); ++i) {
            const double r = ref.update(errors[i]);
            const double d = dut.update(errors[i]);
            if (std::abs(r - d) > eps) {
                std::printf("Y 轴第 %zu 步不匹配: ref=%.9f dut=%.9f\n", i, r, d);
                ++fails;
            }
        }
    }

    // 抖动序列（含跨越 30 触发 reset 的分支）
    {
        P_PID ref;
        ref.init(25.0, 25.0, 3.0, 0.3, 9900.0);
        Pid1Controller dut;
        dut.init(25.0, 25.0, 3.0, 0.3, 9900.0);
        const std::vector<double> seq = {50.0, 5.0, -100.0, 20.0, -50.0, 0.5, 0.2, -0.5, 200.0};
        for (size_t i = 0; i < seq.size(); ++i) {
            const double r = ref.update(seq[i]);
            const double d = dut.update(seq[i]);
            if (std::abs(r - d) > eps) {
                std::printf("抖动序列第 %zu 步不匹配: ref=%.9f dut=%.9f\n", i, r, d);
                ++fails;
            }
        }
    }

    // 手动 reset 后行为一致
    {
        P_PID ref;
        ref.init(25.0, 25.0, 3.0, 0.3, 9900.0);
        Pid1Controller dut;
        dut.init(25.0, 25.0, 3.0, 0.3, 9900.0);
        ref.update(100.0); dut.update(100.0);
        ref.reset(); dut.reset();
        for (size_t i = 0; i < errors.size(); ++i) {
            const double r = ref.update(errors[i]);
            const double d = dut.update(errors[i]);
            if (std::abs(r - d) > eps) {
                std::printf("reset 后第 %zu 步不匹配: ref=%.9f dut=%.9f\n", i, r, d);
                ++fails;
            }
        }
    }

    if (fails == 0) std::printf("test_pid1: PASS\n");
    else std::printf("test_pid1: %d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
