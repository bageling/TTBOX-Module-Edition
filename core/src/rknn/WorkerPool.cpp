// WorkerPool.cpp — 多 Worker 并发推理实现
#include "rknn/WorkerPool.hpp"

#if defined(_WIN32)
namespace ttbox::core {
}
#else

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include "common/Logger.hpp"
#include "common/CpuAffinity.hpp"

namespace ttbox::core {

namespace {

using clock = std::chrono::steady_clock;

// float -> IEEE half（就近舍入，A-6 检测精度路径：u8->FP16 输入必须精确）
}  // namespace

// ---------------------------------------------------------------------------
// InferenceWorker
// ---------------------------------------------------------------------------

InferenceWorker::~InferenceWorker() {
    stop();
}

bool InferenceWorker::start(const Params& params, std::string* error) {
    if (running_.load()) {
        if (error) *error = "worker 已在运行";
        return false;
    }
    if (params.latest == nullptr || params.model_path.empty()) {
        if (error) *error = "worker 参数无效（latest/model 缺失）";
        return false;
    }
    if (params.total_workers < 1) {
        if (error) *error = "total_workers 无效";
        return false;
    }
    params_ = params;
    id_ = params.id;

    // 模型加载先行：RGA 输出尺寸/decode 输入尺寸一律以模型实际输入为准
    // （不猜、不硬编码；黄瓦=320x320、yolo261n=640x640）
    engine_ = std::make_unique<RKNNEngine>();
    std::string eng_err;
    RKNNEngine::Params ep;
    ep.model_path = params.model_path;
    ep.core_mask = params.core_mask;
    ep.pass_through = params.pass_through;
    if (!engine_->init(ep, &eng_err)) {
        if (error) *error = "worker RKNN init 失败: " + eng_err;
        engine_.reset();
        return false;
    }
    if (params.pass_through) {
        std::string zero_copy_error;
        if (!engine_->init_zero_copy(&zero_copy_error)) {
            TTBOX_LOG_WARN("worker[" + std::to_string(id_) + "] 零拷贝不可用，回退兼容 I/O: " + zero_copy_error);
        }
    }
    const uint32_t in_w = engine_->info().input_width;
    const uint32_t in_h = engine_->info().input_height;

    preprocess_ = std::make_unique<Preprocess>();
    PreprocessConfig pcfg;
    pcfg.detect_size = {in_w, in_h};
    pcfg.input_type = engine_->info().input_type;
    pcfg.input_size = engine_->info().input_size;
    pcfg.backend = PreprocessBackend::kRga;
    pcfg.center_crop = true;
    pcfg.color_order = params.color_order;
    std::string preprocess_error;
    if (!preprocess_->init(pcfg, &preprocess_error)) {
        if (error) *error = "worker Preprocess/RGA init 失败: " + preprocess_error;
        preprocess_.reset();
        engine_.reset();
        return false;
    }

    raw_outputs_.clear();
    raw_buf_ptrs_.clear();
    raw_sizes_.clear();
    for (const auto& oi : engine_->info().outputs) {
        raw_outputs_.emplace_back(oi.size, 0);
        raw_buf_ptrs_.push_back(raw_outputs_.back().data());
        raw_sizes_.push_back(oi.size);
    }
    // ---- A-6/A-7：解码器（优先 ModelAdapter 创建；否则默认 DecodeNMS）----
    std::string derr;
    if (params.adapter != nullptr) {
        decoder_ = params.adapter->create_decoder(&derr);
        if (!decoder_) {
            if (error) *error = "worker decoder 创建失败: " + derr;
            preprocess_.reset();
            engine_.reset();
            return false;
        }
        decoder_->set_frame(params.frame_w, params.frame_h);
    } else {
        DecodeParams dp;
        dp.conf_thres = params.conf_thres;
        dp.iou_thres = params.iou_thres;
        dp.classwise = true;
        dp.input_w = in_w;  // 模型实际输入尺寸（DFL stride 计算依赖）
        dp.input_h = in_h;
        dp.frame_w = params.frame_w;
        dp.frame_h = params.frame_h;
        auto d = std::make_unique<DecoderImpl>();
        if (!d->configure(dp, &derr)) {
            if (error) *error = "worker DecodeNMS 配置失败: " + derr;
            preprocess_.reset();
            engine_.reset();
            return false;
        }
        decoder_ = std::move(d);
    }
    if (params.runtime_config != nullptr) {
        if (auto profile = params.runtime_config->snapshot()) geometry_filter_.set_config(profile->geometry_filter);
    }

    TTBOX_LOG_INFO("worker[" + std::to_string(id_) + "] 就绪: core_mask=" +
                   std::to_string(params.core_mask) + " 模型加载 " +
                   std::to_string(engine_->load_ms()) + "ms");

    running_.store(true);
    thread_ = std::thread(&InferenceWorker::loop, this);
    // 推理线程绑定大核（CPU4~7）：RGA 缩放 + NMS 后处理都是 CPU 密集，
    // 与采集线程同域，避免小核调度抖动；NPU 三核并行由 core_mask 1/2/4 保证。
    {
        std::string aerr;
        if (!CpuAffinity::set_thread_affinity(CpuAffinity::kBigCoreMask, &aerr)) {
            TTBOX_LOG_WARN("worker[" + std::to_string(id_) + "] 绑定大核失败: " + aerr);
        } else {
            TTBOX_LOG_INFO("worker[" + std::to_string(id_) + "] 已绑定大核 (cpu4-7)");
        }
    }
    return true;
}

void InferenceWorker::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    if (engine_) engine_->destroy();
    preprocess_.reset();
    engine_.reset();
}

// A-8：应用最新 RuntimeProfile（仅当配置快照变化时）。
// conf/iou/class_filter/max_detections/FOV → decoder；ROI → decoder+RGA（安全点）。
void InferenceWorker::apply_runtime_profile() {
    if (params_.runtime_config == nullptr || !decoder_) return;
    auto prof = params_.runtime_config->snapshot();
    if (!prof || prof == applied_profile_) return;

    decoder_->apply_runtime(prof->inference, prof->fov);
    geometry_filter_.set_config(prof->geometry_filter);
    const uint32_t rw = prof->capture.width;
    const uint32_t rh = prof->capture.height;
    const uint32_t fw = params_.frame_w, fh = params_.frame_h;
    if (rw > 0 && rh > 0 && fw > 0 && fh > 0 && rw <= fw && rh <= fh) {
        // ROI 中心 = 屏幕中心 + offset（offset 语义=相对屏幕中心偏移），
        // 转左上角起点并 clamp 到全帧内。
        const int32_t cx = static_cast<int32_t>(fw / 2) + prof->capture.offset_x;
        const int32_t cy = static_cast<int32_t>(fh / 2) + prof->capture.offset_y;
        const int32_t rx = std::max<int32_t>(0, std::min<int32_t>(
            cx - static_cast<int32_t>(rw / 2), static_cast<int32_t>(fw - rw)));
        const int32_t ry = std::max<int32_t>(0, std::min<int32_t>(
            cy - static_cast<int32_t>(rh / 2), static_cast<int32_t>(fh - rh)));
        decoder_->set_roi(static_cast<uint32_t>(rx), static_cast<uint32_t>(ry), rw, rh);
        if (preprocess_) preprocess_->set_crop(static_cast<uint32_t>(rx), static_cast<uint32_t>(ry), rw, rh);
    }
    applied_profile_ = std::move(prof);
}

void InferenceWorker::loop() {
    using clock = std::chrono::steady_clock;
    while (running_.load()) {
        auto frame = params_.latest->get();
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        const uint32_t seq = frame->info.sequence;
        if (seq == last_seq_) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;  // 本 worker 已处理过该帧
        }
        // 认领规则：seq % N == id（帧只被一个 worker 处理，无重复）
        if (seq % static_cast<uint32_t>(params_.total_workers) !=
            static_cast<uint32_t>(params_.id)) {
            stats_.skipped.fetch_add(1);
            last_seq_ = seq;  // 该帧由其他 worker 处理
            continue;
        }
        last_seq_ = seq;

        // A-8：热更新配置（仅变化时应用，无逐帧 JSON/IPC）
        apply_runtime_profile();

        // ---- E2E 起点：帧采集时刻（v4l2 单调时钟，与 steady_clock 同基准）----
        const double recv_ms = frame->info.timestamp_ms;
        const auto e2e_t0 = clock::now();
        // 排队等待（YU buffer_age 同口径）：帧时间戳 → worker 认领
        const double claim_ms_q = std::chrono::duration<double, std::milli>(e2e_t0.time_since_epoch()).count();
        stats_.queue_wait.add(static_cast<uint64_t>(std::max(0.0, (claim_ms_q - recv_ms) * 1000.0)));

    // ---- 唯一预处理入口：Preprocess（生产默认 RGA，CPU 仅显式 fallback）----
    PreprocessedFrame prepared;
    std::string preprocess_error;
    const auto preprocess_begin = clock::now();
    if (!preprocess_ || !preprocess_->process(*frame, &prepared, &preprocess_error)) {
        stats_.errors.fetch_add(1);
        if (stats_.errors.load() <= 3) std::fprintf(stderr, "worker[%d] Preprocess: %s\n", id_, preprocess_error.c_str());
        continue;
    }
    if (preprocess_->using_rga()) {
        stats_.rga_ok.fetch_add(1);
        stats_.rga.add(std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - preprocess_begin).count());
    } else {
        stats_.direct_ok.fetch_add(1);
    }
    const uint8_t* input_ptr = prepared.tensor_data ? prepared.tensor_data : prepared.data;
    const size_t input_bytes = prepared.tensor_size ? prepared.tensor_size : prepared.size;
    if (!input_ptr || input_bytes == 0) {
        stats_.errors.fetch_add(1);
        continue;
    }
    frame.reset();

        // ---- RKNN 推理（本 worker 独立 context）+ 原生输出 + Decode/NMS ----
        std::string ierr;
        const auto t_infer0 = clock::now();
        bool infer_ok = false;
        if (engine_->zero_copy_ready()) {
            if (!engine_->input_memory() || input_bytes > engine_->input_memory_size()) {
                ierr = "零拷贝输入尺寸不匹配";
            } else {
                std::memcpy(engine_->input_memory(), input_ptr, input_bytes);
                infer_ok = engine_->run_zero_copy(&ierr);
            }
        } else {
            infer_ok = engine_->set_input(input_ptr, input_bytes, &ierr) && engine_->run(&ierr);
        }
        if (!infer_ok) {
            stats_.errors.fetch_add(1);
            if (stats_.errors.load() <= 3) std::fprintf(stderr, "worker[%d] infer: %s\\n", id_, ierr.c_str());
            continue;
        }
        stats_.inference_ok.fetch_add(1);
        if (engine_->zero_copy_ready()) {
            raw_buf_ptrs_.clear();
            raw_sizes_.clear();
            for (uint32_t i = 0; i < engine_->info().n_outputs; ++i) {
                raw_buf_ptrs_.push_back(engine_->output_memory(i));
                raw_sizes_.push_back(engine_->output_memory_size(i));
            }
        } else if (!engine_->get_raw_outputs(raw_buf_ptrs_.data(), raw_sizes_.data(), &ierr)) {
            stats_.errors.fetch_add(1);
            if (stats_.errors.load() <= 3) std::fprintf(stderr, "worker[%d] raw_outputs: %s\\n", id_, ierr.c_str());
            continue;
        }
        std::string derr;
        if (!decoder_->process(engine_->info(), raw_buf_ptrs_.data(), &detections_, &derr)) {
            stats_.errors.fetch_add(1);
            if (stats_.errors.load() <= 3) std::fprintf(stderr, "worker[%d] decode: %s\n", id_, derr.c_str());
            continue;
        }
        if (geometry_filter_.config().enabled && params_.frame_w > 0 && params_.frame_h > 0) {
            detections_ = geometry_filter_.filter(detections_, static_cast<float>(params_.frame_w) * 0.5f, static_cast<float>(params_.frame_h) * 0.5f);
        }
        stats_.decode_ok.fetch_add(1);
        stats_.processed.fetch_add(1);
        const uint64_t now_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(clock::now().time_since_epoch()).count());
        if (params_.aim_mailbox) {
            aim::AimTargetTask task;
            task.frame_number = seq;
            task.timestamp_us = now_us;
            task.worker_id = id_;
            task.frame_width = params_.frame_w;
            task.frame_height = params_.frame_h;
            task.detections = detections_;
            if (!detections_.empty()) {
                task.has_target = true;
                task.target = detections_.front();
                task.aim_point = {
                    (task.target.x1 + task.target.x2) * 0.5f,
                    (task.target.y1 + task.target.y2) * 0.5f};
                task.target_width = task.target.x2 - task.target.x1;
                task.target_height = task.target.y2 - task.target.y1;
            }
            params_.aim_mailbox->offer(static_cast<std::size_t>(id_), std::move(task));
            stats_.published.fetch_add(1);
        }
        // 帧级 total = set_input + run + output（与 A-4 infer() total 语义一致）
        stats_.stages.total.add(
            std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t_infer0).count());
        // 吸收本帧 RKNNEngine 阶段统计（absorb 后 reset，避免重复累计）
        {
            const auto& est = engine_->stats();
            stats_.stages.set_input.absorb(est.set_input);
            stats_.stages.run.absorb(est.run);
            stats_.stages.output.absorb(est.output);
            stats_.stages.total.absorb(est.total);
            engine_->reset_stats();
        }
        // 吸收本帧 Decode/NMS 统计 + 候选/目标计数
        {
            const auto& ds = decoder_->stats();
            stats_.decode_stages.decode.absorb(ds.decode);
            stats_.decode_stages.nms.absorb(ds.nms);
            stats_.decode_stages.total.absorb(ds.total);
            stats_.candidates.fetch_add(ds.candidates.load());
            stats_.detections.fetch_add(ds.detections.load());
            decoder_->reset_stats();
        }
        // E2E（us）= 帧采集→认领（单调毫秒差值，v4l2 与 steady_clock 同基准）
        //          + 认领→完成（处理耗时）
        const double claim_ms =
            std::chrono::duration<double, std::milli>(e2e_t0.time_since_epoch()).count();
        const uint64_t e2e_us =
            static_cast<uint64_t>((claim_ms - recv_ms) * 1000.0) +
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                      clock::now() - e2e_t0).count());
        stats_.e2e.add(e2e_us);
    }
}

// ---------------------------------------------------------------------------
// WorkerPool
// ---------------------------------------------------------------------------

bool WorkerPool::start(const Params& params, std::string* error) {
    if (!workers_.empty()) {
        if (error) *error = "WorkerPool 已在运行";
        return false;
    }
    if (params.worker_cores.empty()) {
        if (error) *error = "worker_cores 为空（worker 数量必须 ≥1）";
        return false;
    }
    const size_t n = params.worker_cores.size();
    for (size_t i = 0; i < n; ++i) {
        auto worker = std::make_unique<InferenceWorker>();
        InferenceWorker::Params wp;
        wp.id = static_cast<int>(i);
        wp.core_mask = params.worker_cores[i];
        wp.model_path = params.model_path;
        wp.pass_through = params.pass_through;
        wp.out_w = params.out_w;
        wp.out_h = params.out_h;
        wp.latest = params.latest;
        wp.total_workers = static_cast<int>(n);
        wp.conf_thres = params.conf_thres;
        wp.iou_thres = params.iou_thres;
        wp.frame_w = params.frame_w;
        wp.frame_h = params.frame_h;
        wp.color_order = params.color_order;
        wp.adapter = params.adapter;
        wp.runtime_config = params.runtime_config;
        wp.aim_mailbox = params.aim_mailbox;
        std::string werr;
        if (!worker->start(wp, &werr)) {
            stop();
            if (error) *error = "worker[" + std::to_string(i) + "] 启动失败: " + werr;
            return false;
        }
        workers_.push_back(std::move(worker));
    }
    return true;
}

void WorkerPool::stop() {
    for (auto& w : workers_) {
        if (w) w->stop();
    }
    workers_.clear();
}

uint64_t WorkerPool::total_processed() const {
    uint64_t s = 0;
    for (const auto& w : workers_) s += w->stats().processed;
    return s;
}

uint64_t WorkerPool::total_errors() const {
    uint64_t s = 0;
    for (const auto& w : workers_) s += w->stats().errors;
    return s;
}

uint64_t WorkerPool::total_skipped() const {
    uint64_t s = 0;
    for (const auto& w : workers_) s += w->stats().skipped;
    return s;
}

}  // namespace ttbox::core

#endif  // !_WIN32
