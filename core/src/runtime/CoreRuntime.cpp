// CoreRuntime.cpp — Capture/Worker/AimThread 统一生命周期。
#include "runtime/CoreRuntime.hpp"
#include "common/Logger.hpp"

#include <algorithm>
#include <chrono>

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
    pipeline_debug_enabled_ = p.pipeline_debug_enabled;
    pipeline_debug_interval_ = p.pipeline_debug_interval;
    pid_trace_enabled_ = p.pid_trace_enabled;
    pid_trace_path_ = p.pid_trace_path;
    prediction_time_s_ = p.prediction_time_s;
    mailbox_ = std::make_unique<aim::AimTargetMailbox>(p.workers.worker_cores.size());
    capture_ = std::make_unique<V4L2Capture>();
    workers_ = std::make_unique<WorkerPool>();
    if (!capture_->configure(p.capture, error)) return false;
    return true;
}

bool CoreRuntime::start(std::string* error) {
    if (!capture_ || !workers_ || !mailbox_ || running_.exchange(true)) return false;
    // 重启（停止→启动）时清空 mailbox 残留任务：V4L2 sequence 重新从 0 计数，
    // 不清空会导致 AimThread last_frame 被旧任务抬高，新帧全被 take_latest 去重丢弃
    // （重启后 1~3 分钟检测框不更新，直到帧号重新涨回旧值）。
    mailbox_->clear();
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
    aim_thread_.set_pipeline_debug(pipeline_debug_enabled_, pipeline_debug_interval_);
    if (pid_trace_enabled_) aim_thread_.set_pid_trace(true, pid_trace_path_);
    aim_thread_.set_prediction_time(prediction_time_s_);

    {
        std::string preview_error;
        preview_ = std::make_unique<PreviewModule>();
        PreviewModule::Params preview_params = preview_params_;
        preview_params.runtime_config = runtime_config_;
        if (runtime_config_) {
            if (auto profile = runtime_config_->snapshot()) {
                if (profile->preview.fps > 0 && profile->preview.fps <= 60) {
                    preview_params.fps = static_cast<int>(profile->preview.fps);
                }
            }
        }
        if (!preview_->start(capture_->latest_frame_ref(), preview_params, &preview_error)) {
            TTBOX_LOG_WARN("Preview 启动失败（不影响流水线）: " + preview_error);
            preview_.reset();
        } else {
            if (preview_params.draw_detections) {
                preview_->set_detections_provider(
                    [this]() { return aim_thread_.status().detection_boxes; });
            }
            uint32_t crop_width = preview_params.crop_width;
            uint32_t crop_height = preview_params.crop_height;
            if (runtime_config_) {
                if (auto profile = runtime_config_->snapshot()) {
                    if (profile->capture.width > 0) crop_width = profile->capture.width;
                    if (profile->capture.height > 0) crop_height = profile->capture.height;
                }
            }
            TTBOX_LOG_INFO("Preview 已启动: center crop " +
                           std::to_string(crop_width) + "x" + std::to_string(crop_height) +
                           " @" + std::to_string(preview_params.fps) + "fps" +
                           (preview_params.draw_detections ? " +draw_detections" : ""));
        }
    }
    return true;
}

bool CoreRuntime::model_ready() const {
    if (!running_.load() || !workers_ || workers_->worker_count() == 0) return false;
    for (const auto& worker : workers_->workers()) {
        if (!worker) continue;
        const auto& stats = worker->stats();
        if (stats.inference_ok.load() > 0 && stats.decode_ok.load() > 0) return true;
    }
    return false;
}

uint64_t CoreRuntime::model_errors() const {
    if (!workers_) return 0;
    return workers_->total_errors();
}

void CoreRuntime::stop() {
    if (!running_.exchange(false)) return;
    start_steady_ms_.store(0);
    if (preview_) {
        preview_->stop();
        preview_.reset();
    }
    aim_thread_.stop();
    mouse_reader_.stop();
    if (workers_) workers_->stop();
    if (capture_) {
        capture_->stop();
        capture_->close();
    }
}

void CoreRuntime::collect_metrics(PipelineMetrics* out) const {
    if (out == nullptr || !running_.load()) return;
    if (capture_) {
        const auto& cm = capture_->metrics();
        out->frames_total = cm.capture_frames.load();
        out->dropped_frames = cm.dropped_latest_frames.load();
        out->capture_fps = cm.capture_fps.load();
        out->last_dequeued_count = capture_->in_use_count();
        out->buffer_count = capture_->buffer_count();
        const auto& format = capture_->format();
        out->input_width = format.width;
        out->input_height = format.height;
    }
    if (workers_ && workers_->worker_count() > 0) {
        uint64_t published = 0;
        double infer_avg_us = 0.0;
        double si_avg_us = 0.0, run_avg_us = 0.0, out_avg_us = 0.0;
        double decode_avg_us = 0.0, e2e_avg_us = 0.0;
        double convert_avg_us = 0.0, qwait_avg_us = 0.0;
        const size_t worker_count = workers_->worker_count();
        StatsCollector e2e_all, infer_all, decode_all;
        for (const auto& worker : workers_->workers()) {
            if (!worker) continue;
            const auto& stats = worker->stats();
            published += stats.published.load();
            infer_avg_us += stats.stages.total.avg();
            si_avg_us += stats.stages.set_input.avg();
            run_avg_us += stats.stages.run.avg();
            out_avg_us += stats.stages.output.avg();
            decode_avg_us += stats.decode_stages.total.avg();
            e2e_avg_us += stats.e2e.avg();
            convert_avg_us += stats.convert.avg();
            qwait_avg_us += stats.queue_wait.avg();
            e2e_all.absorb(stats.e2e);
            infer_all.absorb(stats.stages.total);
            decode_all.absorb(stats.decode_stages.total);
        }
        out->infer_total = published;
        out->fps = published;
        const int64_t started = start_steady_ms_.load();
        if (started > 0) {
            const double elapsed_s = static_cast<double>(steady_now_ms() - started) / 1000.0;
            if (elapsed_s > 0.0) out->fps = static_cast<double>(published) / elapsed_s;
        }
        out->infer_ms = infer_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->infer_set_input_ms = si_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->infer_run_ms = run_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->infer_output_ms = out_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->decode_ms = decode_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->e2e_ms = e2e_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->resize_ms = convert_avg_us / static_cast<double>(worker_count) / 1000.0;
        out->buffer_age_ms = qwait_avg_us / static_cast<double>(worker_count) / 1000.0;
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
        aim::AimTargetTask task;
        if (mailbox_->take_latest(&task)) out->detect_count = task.detections.size();
    }
    out->tracks = aim_thread_.status().tracks;
    const auto aim_status = aim_thread_.status();
    out->aim_error_x = aim_status.error_x;
    out->aim_error_y = aim_status.error_y;
    out->target_point_x = aim_status.target_point_x;
    out->target_point_y = aim_status.target_point_y;
    out->reference_x = aim_status.reference_x;
    out->reference_y = aim_status.reference_y;
    out->pid_output_x = aim_status.pid_output_x;
    out->pid_output_y = aim_status.pid_output_y;
    out->scheduler_input_x = aim_status.scheduler_input_x;
    out->scheduler_input_y = aim_status.scheduler_input_y;
    out->aim_pos_x = aim_status.predicted_x;
    out->aim_pos_y = aim_status.predicted_y;
    out->aim_has_target = aim_status.has_target;
    out->aim_target_id = aim_status.target_id;
    out->aim_target_class_id = aim_status.target_class_id;
    out->aim_target_width = aim_status.target_width;
    out->aim_target_height = aim_status.target_height;
    out->aim_target_x1 = aim_status.target_x1;
    out->aim_target_y1 = aim_status.target_y1;
    out->aim_target_x2 = aim_status.target_x2;
    out->aim_target_y2 = aim_status.target_y2;
    out->detection_boxes = aim_status.detection_boxes;
    if (preview_) {
        const auto& metrics = preview_->metrics();
        out->preview_fps = metrics.fps.load();
        out->preview_encode_ms = metrics.encode_ms.load();
        out->preview_width = metrics.width.load();
        out->preview_height = metrics.height.load();
        out->preview_bytes = metrics.bytes.load();
        out->preview_frames = metrics.frames.load();
        out->preview_dropped = metrics.dropped.load();
    }
    const auto final_status = aim_thread_.status();
    out->mouse_dx = final_status.move_x;
    out->mouse_dy = final_status.move_y;
    out->gated_frames = final_status.gated_frames;
    out->target_frames = final_status.target_frames;
    out->no_target_frames = final_status.no_target_frames;
    out->aim_active = final_status.has_target;
    out->injection_allowed = final_status.last_injection_allowed;
    if (auto* backend = dynamic_cast<output::OutputBackend*>(output_.get())) {
        const auto health = backend->health();
        out->mouse_control_connected = health.state == output::BackendState::kConnected;
        out->mouse_control_socket_write_ok = health.socket_write_ok;
        out->mouse_control_socket_write_fail = health.socket_write_fail;
        out->mouse_control_send_count = health.send_count;
        out->last_mouse_control_dx = health.last_dx;
        out->last_mouse_control_dy = health.last_dy;
        out->last_mouse_control_wheel = health.last_wheel;
        out->last_mouse_control_timestamp_us = health.last_timestamp_us;
    }
}

}  // namespace ttbox::core
