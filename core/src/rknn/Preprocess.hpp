// Preprocess.hpp — 统一采集帧到检测输入的预处理边界
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common/Types.hpp"
#include "rga/RgaProcessor.hpp"
#include "common/CoreContracts.hpp"

namespace ttbox::core {

// 统一检测尺寸：Detector 的工作输入尺寸。模型尺寸由 ActiveModel/RKNN 元数据决定。
struct DetectSize {
    uint32_t width = 0;
    uint32_t height = 0;
};

enum class PreprocessBackend { kRga, kCpuFallback };

struct PreprocessConfig {
    DetectSize detect_size;
    int input_type = 3;  // RKNN 输入类型：2=INT8，3=UINT8，其余按 FP16/FLOAT 转换
    size_t input_size = 0;
    PreprocessBackend backend = PreprocessBackend::kRga;
    bool center_crop = true;
    int color_order = 0;  // 0=BGR, 1=RGB
    uint32_t crop_x = 0;
    uint32_t crop_y = 0;
    uint32_t crop_width = 0;
    uint32_t crop_height = 0;
};

struct PreprocessedFrame {
    bool ok = false;
    DetectSize detect_size;
    PixelFormat format = PixelFormat::kUnknown;
    uint32_t stride = 0;
    int dma_fd = -1;
    const uint8_t* data = nullptr;
    size_t size = 0;
    const uint8_t* tensor_data = nullptr;
    size_t tensor_size = 0;
    RgaOutput rga_output{};
    std::shared_ptr<std::vector<uint8_t>> cpu_storage;
    std::shared_ptr<std::vector<uint16_t>> fp16_storage;
};

class Preprocess {
public:
    Preprocess() = default;
    ~Preprocess() = default;
    Preprocess(const Preprocess&) = delete;
    Preprocess& operator=(const Preprocess&) = delete;

    bool init(const PreprocessConfig& config, std::string* error = nullptr);
    bool process(const FrameBuffer& input, PreprocessedFrame* output,
                 std::string* error = nullptr);
    void set_crop(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    const PreprocessConfig& config() const { return config_; }
    bool using_rga() const {
#if defined(__unix__) && !defined(__APPLE__)
        return rga_ != nullptr;
#else
        return false;
#endif
    }
    const RgaMetrics* rga_metrics() const {
#if defined(__unix__) && !defined(__APPLE__)
        return rga_ ? &rga_->metrics() : nullptr;
#else
        return nullptr;
#endif
    }

private:
    bool process_cpu(const FrameBuffer& input, PreprocessedFrame* output,
                     std::string* error);

    PreprocessConfig config_{};
#if defined(__unix__) && !defined(__APPLE__)
    std::unique_ptr<RgaProcessor> rga_;
#endif
    bool initialized_ = false;
};

}  // namespace ttbox::core
