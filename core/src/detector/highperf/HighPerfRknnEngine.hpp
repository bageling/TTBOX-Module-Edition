// HighPerfRknnEngine.hpp — TTBOX 内部高性能 RKNN 后端边界。
// 只暴露 TTBOX 所需的模型信息与内存地址，不暴露参考项目类型。
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ttbox::core::highperf {

struct TensorInfo {
    uint32_t index = 0;
    uint32_t n_elems = 0;
    uint32_t size = 0;
    uint32_t size_with_stride = 0;
    int type = 0;
    int format = 0;
    int32_t zero_point = 0;
    float scale = 0.0f;
    std::vector<uint32_t> dims;
};

struct ModelInfo {
    uint32_t input_count = 0;
    uint32_t output_count = 0;
    TensorInfo input;
    std::vector<TensorInfo> outputs;
};

class HighPerfRknnEngine {
public:
    HighPerfRknnEngine();
    ~HighPerfRknnEngine();
    HighPerfRknnEngine(const HighPerfRknnEngine&) = delete;
    HighPerfRknnEngine& operator=(const HighPerfRknnEngine&) = delete;

    // 每个实例只初始化一次，并绑定一个 NPU core mask：1/2/4。
    bool init(const std::string& model_path, int core_mask, bool pass_through,
              std::string* error = nullptr);
    void destroy();
    bool initialized() const { return initialized_; }
    bool zero_copy_ready() const { return zero_copy_ready_; }
    const ModelInfo& info() const { return info_; }

    void* input_memory() const;
    size_t input_memory_size() const;
    void* output_memory(uint32_t index) const;
    size_t output_memory_size(uint32_t index) const;

    // 仅复制已完成 RGA 输出；不触碰 FrameBuffer 生命周期。
    bool copy_input(const void* data, size_t size, std::string* error = nullptr);
    bool run(std::string* error = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ModelInfo info_;
    bool initialized_ = false;
    bool zero_copy_ready_ = false;
    bool pass_through_ = false;
};

}  // namespace ttbox::core::highperf
