// HardwareRunner.hpp — RK3588 硬件流水线拥有者。
// 只负责 Capture/WorkerPool/AimThread 的生命周期，不承载算法实现。
#pragma once
#include <memory>
#include <string>
#include <atomic>
#include "capture/V4L2Capture.hpp"
#include "rknn/WorkerPool.hpp"
#include "pipeline/AimTargetMailbox.hpp"
#include "aim/AimThread.hpp"
#include "output/IHidOutput.hpp"
#include "input/PhysicalMouseReader.hpp"
#include "model/ModelAdapter.hpp"
namespace ttbox::core {
class HardwareRunner {
public:
    struct Params {
        V4L2Capture::Params capture;
        WorkerPool::Params workers;
        RuntimeConfig* runtime_config=nullptr;
        std::shared_ptr<output::IHidOutput> output;
        // 仅 Trace/Null 仿真热键；真实 FIFO 模式禁止使用，避免绕过物理安全门。
        uint16_t simulated_buttons = 0;
    };
    bool initialize(const Params&,std::string* error=nullptr);
    bool start(std::string* error=nullptr);
    void stop();
    struct Status {
        bool running = false;
        uint64_t capture_frames = 0;
        uint64_t worker_processed = 0;
        uint64_t worker_rga_ok = 0;
        uint64_t worker_inference_ok = 0;
        uint64_t worker_decode_ok = 0;
        uint64_t worker_published = 0;
        uint64_t worker_candidates = 0;
        uint64_t worker_detections = 0;
        uint64_t worker_errors = 0;
        uint64_t worker_skipped = 0;
        uint64_t aim_consumed = 0;
        uint64_t aim_target_frames = 0;
        uint64_t aim_no_target_frames = 0;
        uint64_t aim_last_frame = 0;
        float aim_predicted_x = 0.0f;
        float aim_predicted_y = 0.0f;
        float aim_control_x = 0.0f;
        float aim_control_y = 0.0f;
        float aim_smith_dx = 0.0f;
        float aim_smith_dy = 0.0f;
        int16_t aim_min_move_x = 0;
        int16_t aim_max_move_x = 0;
        int16_t aim_min_move_y = 0;
        int16_t aim_max_move_y = 0;
        uint64_t aim_clipped_frames = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    bool running() const{return running_.load();}
    Status status() const;
private:
    Params params_{};
    std::unique_ptr<V4L2Capture> capture_;
    std::unique_ptr<WorkerPool> workers_;
    std::unique_ptr<aim::AimTargetMailbox> mailbox_;
    aim::AimThread aim_thread_;
    std::atomic<bool> running_{false};
    input::PhysicalMouseReader mouse_reader_;
    std::unique_ptr<ModelAdapter> adapter_;
    std::atomic<uint16_t> simulated_buttons_{0};
};
}
