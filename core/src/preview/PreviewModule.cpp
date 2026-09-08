// PreviewModule.cpp — Capture 中心可调尺寸预览实现
#include "preview/PreviewModule.hpp"

#if defined(_WIN32)
namespace ttbox::core {
}
#else

#include <algorithm>
#include <chrono>
#include <csetjmp>
#include <cstdio>
#include <cstring>

#include <jpeglib.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "common/Logger.hpp"

namespace ttbox::core {

namespace {

using clock = std::chrono::steady_clock;

struct JpegErrorManager {
    jpeg_error_mgr base;
    jmp_buf jump;
};

void jpeg_error_exit(j_common_ptr info) {
    auto* manager = reinterpret_cast<JpegErrorManager*>(info->err);
    longjmp(manager->jump, 1);
}

bool encode_bgr_jpeg(const uint8_t* bgr, uint32_t width, uint32_t height,
                     uint32_t stride, int quality, std::vector<uint8_t>* output,
                     std::string* error) {
    if (bgr == nullptr || output == nullptr || width == 0 || height == 0 ||
        stride < width * 3) {
        if (error) *error = "Preview JPEG 输入无效";
        return false;
    }

    jpeg_compress_struct compressor{};
    JpegErrorManager manager{};
    compressor.err = jpeg_std_error(&manager.base);
    manager.base.error_exit = jpeg_error_exit;
    if (setjmp(manager.jump)) {
        jpeg_destroy_compress(&compressor);
        if (error) *error = "Preview JPEG 编码失败";
        return false;
    }

    jpeg_create_compress(&compressor);
    unsigned char* encoded = nullptr;
    unsigned long encoded_size = 0;
    jpeg_mem_dest(&compressor, &encoded, &encoded_size);
    compressor.image_width = width;
    compressor.image_height = height;
    compressor.input_components = 3;
    compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor);
    jpeg_set_quality(&compressor, std::clamp(quality, 1, 100), TRUE);
    jpeg_start_compress(&compressor, TRUE);

    std::vector<uint8_t> rgb_row(static_cast<size_t>(width) * 3);
    while (compressor.next_scanline < compressor.image_height) {
        const auto* source = bgr + static_cast<size_t>(compressor.next_scanline) * stride;
        for (uint32_t x = 0; x < width; ++x) {
            rgb_row[x * 3] = source[x * 3 + 2];
            rgb_row[x * 3 + 1] = source[x * 3 + 1];
            rgb_row[x * 3 + 2] = source[x * 3];
        }
        JSAMPROW row = rgb_row.data();
        jpeg_write_scanlines(&compressor, &row, 1);
    }
    jpeg_finish_compress(&compressor);
    jpeg_destroy_compress(&compressor);

    if (encoded == nullptr || encoded_size == 0) {
        if (error) *error = "Preview JPEG 输出为空";
        return false;
    }
    output->assign(encoded, encoded + encoded_size);
    std::free(encoded);
    return true;
}

}  // namespace

void PreviewModule::resolve_crop_size(uint32_t* crop_width, uint32_t* crop_height) const {
    if (crop_width == nullptr || crop_height == nullptr) return;
    uint32_t width = params_.crop_width;
    uint32_t height = params_.crop_height;
    if (params_.runtime_config != nullptr) {
        if (auto profile = params_.runtime_config->snapshot()) {
            if (profile->capture.width > 0) width = profile->capture.width;
            if (profile->capture.height > 0) height = profile->capture.height;
        }
    }
    *crop_width = std::max<uint32_t>(1, width);
    *crop_height = std::max<uint32_t>(1, height);
}

void PreviewModule::draw_boxes(uint8_t* crop, uint32_t width, uint32_t height,
                               uint32_t stride,
                               const std::vector<DetectionBox>& boxes,
                               uint32_t origin_x, uint32_t origin_y) const {
    if (crop == nullptr || boxes.empty() || width == 0 || height == 0 ||
        stride < width * 3) {
        return;
    }

    cv::Mat image(static_cast<int>(height), static_cast<int>(width), CV_8UC3,
                  crop, stride);
    for (const auto& box : boxes) {
        const int x1 = std::clamp(static_cast<int>(box.x1) - static_cast<int>(origin_x),
                                  0, static_cast<int>(width - 1));
        const int y1 = std::clamp(static_cast<int>(box.y1) - static_cast<int>(origin_y),
                                  0, static_cast<int>(height - 1));
        const int x2 = std::clamp(static_cast<int>(box.x2) - static_cast<int>(origin_x),
                                  0, static_cast<int>(width - 1));
        const int y2 = std::clamp(static_cast<int>(box.y2) - static_cast<int>(origin_y),
                                  0, static_cast<int>(height - 1));
        if (x2 <= x1 || y2 <= y1) continue;

        const cv::Scalar color = box.class_id == 1
            ? cv::Scalar(0, 200, 0)
            : box.class_id == 0
                ? cv::Scalar(0, 0, 255)
                : cv::Scalar(0, 200, 200);
        const int thickness = box.class_id == 1 ? 3 : 2;
        cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), color,
                      thickness, cv::LINE_8);

        char label[64];
        std::snprintf(label, sizeof(label), "%d %.2f", box.class_id, box.score);
        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(
            label, cv::FONT_HERSHEY_SIMPLEX, 0.45, 1, &baseline);
        const int label_x = x1;
        const int label_y = std::max(text_size.height + 4, y1 - 2);
        cv::rectangle(image,
                      cv::Point(label_x, label_y - text_size.height - 2),
                      cv::Point(label_x + text_size.width + 4, label_y + baseline),
                      cv::Scalar(0, 0, 0), cv::FILLED, cv::LINE_8);
        cv::putText(image, label, cv::Point(label_x + 2, label_y - 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1, cv::LINE_8);
    }
}

void PreviewModule::smooth_boxes(const std::vector<DetectionBox>& raw,
                                 std::vector<DetectionBox>* output) {
    if (output == nullptr) return;
    if (raw.empty()) {
        ++smooth_lost_count_;
        if (smooth_lost_count_ <= 3) {
            *output = smooth_prev_;
            return;
        }
        smooth_prev_.clear();
        output->clear();
        return;
    }

    smooth_lost_count_ = 0;
    const float alpha = 0.35f;
    const float max_distance_squared = 120.0f * 120.0f;
    std::vector<bool> used(smooth_prev_.size(), false);
    std::vector<DetectionBox> result;
    result.reserve(raw.size());

    for (const auto& current : raw) {
        DetectionBox blended = current;
        const float cx = (current.x1 + current.x2) * 0.5f;
        const float cy = (current.y1 + current.y2) * 0.5f;
        int best_index = -1;
        float best_distance = max_distance_squared;
        for (size_t index = 0; index < smooth_prev_.size(); ++index) {
            if (used[index] || smooth_prev_[index].class_id != current.class_id) continue;
            const auto& previous = smooth_prev_[index];
            const float px = (previous.x1 + previous.x2) * 0.5f;
            const float py = (previous.y1 + previous.y2) * 0.5f;
            const float distance = (px - cx) * (px - cx) + (py - cy) * (py - cy);
            if (distance < best_distance) {
                best_distance = distance;
                best_index = static_cast<int>(index);
            }
        }
        if (best_index >= 0) {
            const auto& previous = smooth_prev_[best_index];
            used[best_index] = true;
            blended.x1 = previous.x1 * (1.0f - alpha) + current.x1 * alpha;
            blended.y1 = previous.y1 * (1.0f - alpha) + current.y1 * alpha;
            blended.x2 = previous.x2 * (1.0f - alpha) + current.x2 * alpha;
            blended.y2 = previous.y2 * (1.0f - alpha) + current.y2 * alpha;
            blended.score = previous.score * (1.0f - alpha) + current.score * alpha;
        }
        result.push_back(blended);
    }
    smooth_prev_ = result;
    *output = std::move(result);
}

bool PreviewModule::start(const LatestFrame* frame_source, const Params& params,
                          std::string* error) {
    if (frame_source == nullptr) {
        if (error) *error = "预览帧源为空";
        return false;
    }
    if (params.fps <= 0 || params.fps > 60 || params.jpeg_quality < 1 ||
        params.jpeg_quality > 100 || params.crop_width == 0 || params.crop_height == 0) {
        if (error) *error = "预览参数无效";
        return false;
    }
    if (running_.exchange(true)) return false;

    latest_ = frame_source;
    params_ = params;
    metrics_.width.store(params.crop_width);
    metrics_.height.store(params.crop_height);
    metrics_.bytes.store(0);
    metrics_.frames.store(0);
    metrics_.dropped.store(0);
    metrics_.fps.store(0.0);
    metrics_.encode_ms.store(0.0);
    smooth_prev_.clear();
    smooth_lost_count_ = 0;
    {
        std::lock_guard<std::mutex> lock(jpeg_mutex_);
        jpeg_.clear();
    }
    thread_ = std::thread(&PreviewModule::loop, this);
    return true;
}

void PreviewModule::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    latest_ = nullptr;
    std::lock_guard<std::mutex> lock(jpeg_mutex_);
    jpeg_.clear();
    metrics_.bytes.store(0);
    metrics_.width.store(0);
    metrics_.height.store(0);
}

bool PreviewModule::snapshot(std::vector<uint8_t>* jpeg_out) const {
    if (jpeg_out == nullptr) return false;
    std::lock_guard<std::mutex> lock(jpeg_mutex_);
    if (jpeg_.empty()) return false;
    *jpeg_out = jpeg_;
    return true;
}

void PreviewModule::loop() {
    const auto start_time = clock::now();
    const auto interval = std::chrono::milliseconds(1000 / params_.fps);
    auto next_tick = start_time;

    while (running_.load()) {
        const auto now = clock::now();
        if (now < next_tick) std::this_thread::sleep_for(next_tick - now);
        next_tick = clock::now() + interval;

        auto frame = latest_ ? latest_->get() : nullptr;
        if (!frame || frame->size == 0 || frame->info.cpu_va == nullptr) {
            metrics_.dropped.fetch_add(1);
            continue;
        }

        const auto encode_start = clock::now();
        std::vector<uint8_t> jpeg;
        std::string error;
        if (!encode_frame(*frame, &jpeg, &error)) {
            metrics_.dropped.fetch_add(1);
            if (metrics_.dropped.load() <= 3) TTBOX_LOG_WARN("Preview 编码失败: " + error);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(jpeg_mutex_);
            jpeg_ = std::move(jpeg);
            metrics_.bytes.store(static_cast<uint32_t>(jpeg_.size()));
        }
        metrics_.frames.fetch_add(1);
        metrics_.encode_ms.store(std::chrono::duration<double, std::milli>(
            clock::now() - encode_start).count());
        const double elapsed = std::chrono::duration<double>(clock::now() - start_time).count();
        if (elapsed > 0.0) {
            metrics_.fps.store(static_cast<double>(metrics_.frames.load()) / elapsed);
        }
    }
}

bool PreviewModule::encode_frame(const FrameBuffer& frame,
                                 std::vector<uint8_t>* jpeg_out,
                                 std::string* error) {
    const uint32_t frame_width = frame.info.width;
    const uint32_t frame_height = frame.info.height;
    const uint32_t frame_stride = frame.info.stride ? frame.info.stride : frame_width * 3;
    uint32_t crop_width = 0;
    uint32_t crop_height = 0;
    resolve_crop_size(&crop_width, &crop_height);
    if (crop_width > frame_width || crop_height > frame_height ||
        frame_stride < frame_width * 3 || frame.info.cpu_va == nullptr) {
        if (error) *error = "Preview 中心截取尺寸超出 Capture 或 CPU 映射不可用";
        return false;
    }

    const uint32_t origin_x = (frame_width - crop_width) / 2;
    const uint32_t origin_y = (frame_height - crop_height) / 2;
    const uint32_t crop_stride = crop_width * 3;
    std::vector<uint8_t> crop(static_cast<size_t>(crop_stride) * crop_height);
    const auto* source = static_cast<const uint8_t*>(frame.info.cpu_va);
    for (uint32_t y = 0; y < crop_height; ++y) {
        const auto* source_row = source + static_cast<size_t>(origin_y + y) * frame_stride +
                                 static_cast<size_t>(origin_x) * 3;
        std::memcpy(crop.data() + static_cast<size_t>(y) * crop_stride,
                    source_row, crop_stride);
    }

    if (params_.draw_detections) {
        std::vector<DetectionBox> raw;
        std::vector<DetectionBox> boxes;
        {
            std::lock_guard<std::mutex> lock(provider_mutex_);
            if (detections_provider_) raw = detections_provider_();
        }
        smooth_boxes(raw, &boxes);
        draw_boxes(crop.data(), crop_width, crop_height, crop_stride,
                   boxes, origin_x, origin_y);
    }

    metrics_.width.store(crop_width);
    metrics_.height.store(crop_height);
    return encode_bgr_jpeg(crop.data(), crop_width, crop_height, crop_stride,
                           params_.jpeg_quality, jpeg_out, error);
}

}  // namespace ttbox::core
#endif  // !_WIN32
