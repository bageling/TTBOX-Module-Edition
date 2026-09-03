// CoreRuntime.cpp — Capture/Worker/AimThread 统一生命周期。
#include "runtime/CoreRuntime.hpp"
#include "common/Logger.hpp"

#include <chrono>
#include <algorithm>

namespace ttbox::core {

namespace {
int64_t steady_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
}  // namespace

bool CoreRuntime::initialize(const Params& p, std::string* error) {
    if (!p.output) {
        if (error) *error = "输出后端不能为空";
        return false;
    }
    if (p.workers.worker_cores.empty() ||
        p.workers.worker_cores.size() > aim::AimTargetMailbox::kMaxWorkers) {
        if (error) *error = "Worker 数量必须为 1~3";
        return false;
    }
    runtime_config_ = p.runtime_config;
    output_ = p.output;
    worker_params_ = p.workers;
    preview_params_ = p.preview;
    mailbox_ = std::make_unique<aim::AimTargetMailbox>(p.workers.worker_cores.size());
    capture_ = std::make_unique<V4L2Capture>();
    workers_ = std::make_unique<WorkerPool>();
    if (!capture_->configure(p.capture, error)) {
        return false;
    }
    return true;
}

bool CoreRuntime::start(std::string* error) {
    if (!capture_ || !workers_ || !mailbox_ || running_.exchange(true)) {
        return false;
    }
    start_steady_ms_.store(steady_now_ms());
    if (!capture_->open(error) || !capture_->start(error)) {
        running_ = false;
        return false;
    }
    worker_params_.latest = capture_->latest_frame_ref();
    worker_params_.aim_mailbox = mailbox_.get();
    worker_params_.runtime_config = runtime_config_;
    const auto& fmt = capture_->format();
    worker_params_.frame_w = fmt.width;
    worker_params_.frame_h = fmt.height;
    if (!workers_->start(worker_params_, error)) {
        capture_->stop();
        capture_->close();
        running_ = false;
        return false;
    }
    std::string mouse_error;
    if (!mouse_reader_.start("", &mouse_error)) {
        TTBOX_LOG_WARN("PhysicalMouseReader 启动失败（不阻塞 AI 流水线）: " + mouse_error);
    }
    if (auto* backend = dynamic_cast<output::OutputBackend*>(output_.get())) {
        backend->set_button_source(mouse_reader_.button_source());
        backend->set_config_source(runtime_config_);
    }
    if (!aim_thread_.start(mailbox_.get(), output_, 4000, runtime_config_,
                           mouse_reader_.button_source())) {
        workers_->stop();
        capture_->stop();
        capture_->close();
        running_ = false;
        return false;
    }
    // Phase2：启动低帧预览（失败仅告警，不影响 AI 流水线）
    {
        std::string perr;
        preview_ = std::make_unique<PreviewModule>();
        PreviewModule::Params pp = preview_params_;
        pp.runtime_config = runtime_config_;  // 预览跟随截取区域（capture ROI，YU 同款语义）
        // 预览帧率热配置：runtime_profile.preview.fps > 0 时覆盖 config 默认值
        // （yu latency.preview_interval_ms 经网关/bridge 翻译为 preview.fps）
        // 输出尺寸对齐 YU：有 ROI 时 1:1 输出 ROI 尺寸（不拉伸），无 ROI 保持 config 默认
        if (runtime_config_) {
            if (auto snap = runtime_config_->snapshot()) {
                if (snap->preview.fps > 0 && snap->preview.fps <= 60) {
                    pp.fps = static_cast<int>(snap->preview.fps);
                }
                const auto& cap = snap->capture;
                const auto& fmt = capture_->format();
                if (cap.width > 0 && cap.height > 0 &&
                    cap.width <= fmt.width && cap.height <= fmt.height) {
                    pp.out_width = cap.width;
                    pp.out_height = cap.height;
                }
            }
        }
        if (!preview_->start(capture_->latest_frame_ref(), pp, &perr)) {
            TTBOX_LOG_WARN("Preview 启动失败（不影响流水线）: " + perr);
            preview_.reset();
        } else {
            TTBOX_LOG_INFO("Preview 已启动: " + std::to_string(pp.out_width) + "x" +
                           std::to_string(pp.out_height) + " @" + std::to_string(pp.fps) + "fps");
        }
    }
    return true;
}

void CoreRuntime::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    start_steady_ms_.store(0);
    if (preview_) {
        preview_->stop();
        preview_.reset();
    }
    aim_thread_.stop();
    mouse_reader_.stop();
    if (workers_) {
        workers_->stop();
    }
    if (capture_) {
        capture_->stop();
        capture_->close();
    }
}

// G1：聚合真实运行指标（只读现有统计，无估算）。
//   - capture：V4L2Metrics（capture_frames / dropped / 滚动 capture_fps）
//   - worker：WorkerStats（published / stages / e2e / decode）
//   - 目标数：mailbox 最新任务（detections.size()；无任务 = 0）
// runtime 未启动时 out 保持调用方传入的初始值（全 0 = unavailable）。
void CoreRuntime::collect_metrics(PipelineMetrics* out) const {
    if (out == nullptr || !running_.load()) {
        return;
    }
    if (capture_) {
        const auto& cm = capture_->metrics();
        out->frames_total = cm.capture_frames.load();
        out->dropped_frames = cm.dropped_latest_frames.load();
        out->capture_fps = cm.capture_fps.load();
        // 采集排队（YU buffer_age 同口径）：
        //   buffer_age_ms = worker 认领等待（帧时间戳 → 认领，真实排队时间）
        //   last_dequeued_count = 已 DQBUF 未归还的 buffer 数；buffer_count = 驱动 buffer 总数
        out->last_dequeued_count = capture_->in_use_count();
        out->buffer_count = capture_->buffer_count();
        const auto& format = capture_->format();
        out->input_width = format.width;
        out->input_height = format.height;
    }
    if (workers_ && workers_->worker_count() > 0) {
        // 聚合所有 worker：published 累计 → 推理 FPS；耗时 avg 直接平均
        uint64_t published = 0;
        double infer_avg_us = 0.0;
        double si_avg_us = 0.0, run_avg_us = 0.0, out_avg_us = 0.0;
        double decode_avg_us = 0.0;
        double e2e_avg_us = 0.0;
        double convert_avg_us = 0.0;
        double qwait_avg_us = 0.0;
        size_t n = workers_->worker_count();
        // 分位数：合并各 worker 样本到临时收集器（不污染 worker 统计），
        // 再算统一 P50/P95/P99/Max（跨 worker 真实分位，非分位均值）。
        StatsCollector e2e_all, infer_all, decode_all;
        for (const auto& w : workers_->workers()) {
            if (!w) continue;
            const auto& s = w->stats();
            published += s.published.load();
            infer_avg_us += s.stages.total.avg();
            si_avg_us += s.stages.set_input.avg();
            run_avg_us += s.stages.run.avg();
            out_avg_us += s.stages.output.avg();
            decode_avg_us += s.decode_stages.total.avg();
            e2e_avg_us += s.e2e.avg();
            convert_avg_us += s.convert.avg();
            qwait_avg_us += s.queue_wait.avg();
            e2e_all.absorb(s.e2e);
            infer_all.absorb(s.stages.total);
            decode_all.absorb(s.decode_stages.total);
        }
        out->infer_total = published;
        out->fps = published;
        const int64_t started = start_steady_ms_.load();
        if (started > 0) {
            const double elapsed_s =
                static_cast<double>(steady_now_ms() - started) / 1000.0;
            if (elapsed_s > 0.0) {
                out->fps = static_cast<double>(published) / elapsed_s;
            }
        }
        out->infer_ms = infer_avg_us / static_cast<double>(n) / 1000.0;
        out->infer_set_input_ms = si_avg_us / static_cast<double>(n) / 1000.0;
        out->infer_run_ms = run_avg_us / static_cast<double>(n) / 1000.0;
        out->infer_output_ms = out_avg_us / static_cast<double>(n) / 1000.0;
        out->decode_ms = decode_avg_us / static_cast<double>(n) / 1000.0;
        out->e2e_ms = e2e_avg_us / static_cast<double>(n) / 1000.0;
        out->resize_ms = convert_avg_us / static_cast<double>(n) / 1000.0;
        out->buffer_age_ms = qwait_avg_us / static_cast<double>(n) / 1000.0;  // YU 口径：排队等待
        // 真实分位数（us → ms；无样本时 percentile 返回 0）
        out->e2e_p50_ms = e2e_all.percentile(50) / 1000.0;
        out->e2e_p95_ms = e2e_all.percentile(95) / 1000.0;
        out->e2e_p99_ms = e2e_all.percentile(99) / 1000.0;
        out->e2e_max_ms = e2e_all.max() / 1000.0;
        out->infer_p50_ms = infer_all.percentile(50) / 1000.0;
        out->infer_p95_ms = infer_all.percentile(95) / 1000.0;
        out->infer_p99_ms = infer_all.percentile(99) / 1000.0;
        out->decode_p50_ms = decode_all.percentile(50) / 1000.0;
        out->decode_p95_ms = decode_all.percentile(95) / 1000.0;
        out->decode_p99_ms = decode_all.percentile(99) / 1000.0;
    }
    if (mailbox_) {
        // 最近任务目标数（取任意 slot 最新帧；mailbox take_latest 按帧号取最新）
        aim::AimTargetTask task;
        if (mailbox_->take_latest(&task)) {
            out->detect_count = task.detections.size();
        }
    }
    // YU 同语义 tracks：当前跟踪中的目标数（AimThread 实时状态）
    out->tracks = aim_thread_.status().tracks;
    const auto ast = aim_thread_.status();
    out->aim_error_x = ast.error_x;
    out->aim_error_y = ast.error_y;
    out->target_point_x = ast.target_point_x;
    out->target_point_y = ast.target_point_y;
    out->reference_x = ast.reference_x;
    out->reference_y = ast.reference_y;
    out->pid_output_x = ast.pid_output_x;
    out->pid_output_y = ast.pid_output_y;
    out->scheduler_input_x = ast.scheduler_input_x;
    out->scheduler_input_y = ast.scheduler_input_y;
    // 目标中心（crop 系 px）：selected 框中心，标定状态机需要真实目标位移
    out->aim_pos_x = ast.predicted_x;
    out->aim_pos_y = ast.predicted_y;
    out->aim_has_target = ast.has_target;
    out->aim_target_id = ast.target_id;
    out->aim_target_class_id = ast.target_class_id;
    out->aim_target_width = ast.target_width;
    out->aim_target_height = ast.target_height;
    out->aim_target_x1 = ast.target_x1;
    out->aim_target_y1 = ast.target_y1;
    out->aim_target_x2 = ast.target_x2;
    out->aim_target_y2 = ast.target_y2;
    out->detection_boxes = ast.detection_boxes;
    if (preview_) {
        const auto& pm = preview_->metrics();
        out->preview_fps = pm.fps.load();
        out->preview_encode_ms = pm.encode_ms.load();
        out->preview_width = pm.width.load();
        out->preview_height = pm.height.load();
        out->preview_bytes = pm.bytes.load();
        out->preview_frames = pm.frames.load();
        out->preview_dropped = pm.dropped.load();
    }
    // G1-2：AimThread 真实瞄准/注入状态（DX/DY/门控帧/目标帧）
    {
        const auto st = aim_thread_.status();
        out->mouse_dx = st.move_x;
        out->mouse_dy = st.move_y;
        out->gated_frames = st.gated_frames;
        out->target_frames = st.target_frames;
        out->no_target_frames = st.no_target_frames;
        out->aim_active = st.has_target;
        out->injection_allowed = st.last_injection_allowed;
        if (auto* backend = dynamic_cast<output::OutputBackend*>(output_.get())) {
            const auto h = backend->health();
            out->mouse_control_connected = h.state == output::BackendState::kConnected;
            out->mouse_control_socket_write_ok = h.socket_write_ok;
            out->mouse_control_socket_write_fail = h.socket_write_fail;
            out->mouse_control_send_count = h.send_count;
            out->last_mouse_control_dx = h.last_dx;
            out->last_mouse_control_dy = h.last_dy;
            out->last_mouse_control_wheel = h.last_wheel;
            out->last_mouse_control_timestamp_us = h.last_timestamp_us;
        }
    }
}

}  // namespace ttbox::core
