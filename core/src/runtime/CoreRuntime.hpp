// CoreRuntime.hpp — 正式 AI Runtime 生命周期入口。
// 本类统一拥有 AimThread；Capture/Worker 接入在板端硬件阶段完成。
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include "aim/AimThread.hpp"
#include "common/Metrics.hpp"
#include "output/IHidOutput.hpp"
#include "output/OutputBackend.hpp"
#include "pipeline/AimTargetMailbox.hpp"
#include "capture/V4L2Capture.hpp"
#include "preview/PreviewModule.hpp"
#include "rknn/WorkerPool.hpp"
#include "model/RuntimeProfile.hpp"
#include "input/PhysicalMouseReader.hpp"
namespace ttbox::core {
// CoreRuntime — 核心运行时："总指挥"。
// 职责：把采集(Capture)、推理(WorkerPool)、瞄准(AimThread)、预览(Preview)、
//       IPC、输出(Output) 组装在一起，统一管理它们的启动/停止/状态。
// 输入：Params（采集/Worker/预览/输出/配置）
// 输出：运行状态 + collect_metrics() 聚合指标（帧率/延迟/检测数）
class CoreRuntime {
public:
    CoreRuntime() = default; ~CoreRuntime(){stop();}
    struct Params {
        V4L2Capture::Params capture;
        WorkerPool::Params workers;
        PreviewModule::Params preview;
        std::shared_ptr<output::IHidOutput> output;
        RuntimeConfig* runtime_config = nullptr;
    };
    bool initialize(const Params& params, std::string* error=nullptr);
    bool start(std::string* error=nullptr); void stop(); bool running() const { return running_.load(); }
    aim::AimTargetMailbox* aim_mailbox(){return mailbox_.get();}
    input::PhysicalMouseReader& mouse_reader(){return mouse_reader_;}

    // G1：聚合真实运行指标（capture/worker 统计 + 最近任务目标数）。
    // 全部来自现有统计，无估算；runtime 未启动时各值保持 0（= unavailable）。
    void collect_metrics(PipelineMetrics* out) const;

    // Phase2：低帧实时预览（10fps，独立线程，不影响 AI 流水线）
    PreviewModule* preview() { return preview_.get(); }
private:
    std::unique_ptr<aim::AimTargetMailbox> mailbox_;
    std::unique_ptr<V4L2Capture> capture_;
    std::unique_ptr<WorkerPool> workers_;
    RuntimeConfig* runtime_config_ = nullptr;
    WorkerPool::Params worker_params_{};
    PreviewModule::Params preview_params_{};
    aim::AimThread aim_thread_;
    input::PhysicalMouseReader mouse_reader_;
    std::shared_ptr<output::IHidOutput> output_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> start_steady_ms_{0};
    std::unique_ptr<PreviewModule> preview_;  // G1：start() 时刻（steady 时钟，算推理 FPS 分母）
};
}
