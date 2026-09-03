// AimThread.hpp — 独立瞄准控制线程骨架。
// 当前阶段只验证 Worker -> Mailbox -> AimThread 的数据链路，不改变现有采集/推理行为。
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include "pipeline/AimTargetMailbox.hpp"
#include "output/IHidOutput.hpp"
#include "mouse/AimStateMachine.hpp"
#include "mouse/MotionController.hpp"
#include "mouse/TargetSelector.hpp"
#include "model/RuntimeProfile.hpp"
#include "aim/Pid1Controller.hpp"
namespace ttbox::core::aim {
class AimThread {
public:
    struct Status {
        bool running = false;
        bool has_task = false;
        bool has_target = false;
        int target_id = -1;
        int target_class_id = -1;
        float target_width = 0.0f;
        float target_height = 0.0f;
        float target_x1 = 0.0f;
        float target_y1 = 0.0f;
        float target_x2 = 0.0f;
        float target_y2 = 0.0f;
        std::vector<DetectionBox> detection_boxes;
        float target_point_x = 0.0f;
        float target_point_y = 0.0f;
        float reference_x = 0.0f;
        float reference_y = 0.0f;
        float error_x = 0.0f;
        float error_y = 0.0f;
        float pid_output_x = 0.0f;
        float pid_output_y = 0.0f;
        float scheduler_input_x = 0.0f;
        float scheduler_input_y = 0.0f;
        int16_t move_x = 0;
        int16_t move_y = 0;
        uint64_t last_frame = 0;
        uint64_t consumed = 0;
        uint64_t stale = 0;
        uint64_t target_frames = 0;
        uint64_t no_target_frames = 0;
        uint32_t tracks = 0;           // 当前跟踪中的目标数（YU detections/tracks 显示）
        float predicted_x = 0.0f;
        float predicted_y = 0.0f;
        float control_x = 0.0f;
        float control_y = 0.0f;
        float smith_dx = 0.0f;
        float smith_dy = 0.0f;
        int16_t min_move_x = 0;
        int16_t max_move_x = 0;
        int16_t min_move_y = 0;
        int16_t max_move_y = 0;
        uint64_t clipped_frames = 0;
        uint64_t gated_frames = 0;      // Hotkey Gate 拦截的周期数（热键未按）
        uint16_t last_hotkey_bits = 0;  // 最近一次采样的物理按键位图（遥测）
        bool last_injection_allowed = false;  // 最近一次 Gate 判定结果
        uint64_t last_timestamp_us = 0;
    };
    AimThread() = default;
    ~AimThread() { stop(); }
    bool start(AimTargetMailbox* mailbox, std::shared_ptr<output::IHidOutput> output, int interval_us = 4000, RuntimeConfig* runtime_config = nullptr, std::atomic<uint16_t>* physical_buttons = nullptr);
    void stop();
    Status status() const;
private:
    void loop();
    AimTargetMailbox* mailbox_ = nullptr;
    std::shared_ptr<output::IHidOutput> output_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int interval_us_ = 4000;
    RuntimeConfig* runtime_config_ = nullptr;
    std::atomic<uint16_t>* physical_buttons_ = nullptr;
    TargetSelector selector_;
    MotionController controller_;
    Pid1Controller pid_x_;   // pid1.cpp P_PID 1:1 移植：X 轴（predict=3.0）
    Pid1Controller pid_y_;   // pid1.cpp P_PID 1:1 移植：Y 轴（predict=0.0）
    AimStateMachine state_machine_;
    uint64_t last_timestamp_us_ = 0;
    float remainder_x_ = 0.0f;
    float remainder_y_ = 0.0f;
    int last_target_id_ = -1;
    mutable std::mutex status_mutex_;
    Status status_{};
};
}  // namespace ttbox::core::aim
