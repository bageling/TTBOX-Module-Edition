// RKNNEngine.hpp — RKNN C API 推理引擎（阶段 A-4，单 Worker）
//
// 目标：
//   - 全部 C++（librknnrt.so + rknn_api.h），Python 不进入高速 AI 链路
//   - 输入零拷贝优先：set_input 直接引用调用方 buffer（RGA 输出 mmap va），
//     pass_through=1 时绕过 runtime 内部复制/格式转换
//   - 模型/配置外置：model_path / core_mask / 输入尺寸全部由调用方从 config 提供
//   - 单 Worker 耗时统计：set_input / run / output / total（min/avg/p50/p95/p99/max）
//
// 接口边界（后续 A-5 Worker / A-6 Decode 依赖）：
//   RKNNEngine::infer(input_buf) -> 输出张量（float32，want_float=1）
/*
 * TTBOX 文件说明
 *
 * 文件：RKNNEngine.hpp
 *
 * 作用：
 *   NPU 推理引擎的定义。
 *
 * 小白理解：
 *   这是 RKNNEngine.cpp 的头文件，定义了 NPU 推理的接口。
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common/Stats.hpp"

namespace ttbox::core {

// 模型/输入输出信息（init 时由 rknn_query 获取，全部来自 runtime，不猜测）
struct RknnOutputInfo {
    uint32_t n_elems = 0;   // 元素数（张量元素个数）
    uint32_t size = 0;      // 原生字节数（attr.size，含对齐）
    int type = 0;           // rknn_tensor_type（FLOAT32/FLOAT16/INT8/...）
    int fmt = 0;            // rknn_tensor_format（NCHW/NHWC）
    std::vector<uint32_t> dims;  // 原始 dims（如 {1,84,8400}）
    float scale = 0.0f;     // 反量化 scale（INT8 等需要）
    int zp = 0;             // 反量化 zero point
};

struct RknnModelInfo {
    uint32_t n_inputs = 0;
    uint32_t n_outputs = 0;

    // 输入 0（当前单输入模型）
    std::vector<uint32_t> input_dims;   // 例如 {1, 640, 640, 3}
    uint32_t input_width = 0;
    uint32_t input_height = 0;
    uint32_t input_size = 0;            // 输入 tensor 所需字节（含对齐）
    int input_type = 0;                 // rknn_tensor_type（来自 query）
    int input_fmt = 0;                  // rknn_tensor_format

    // 输出（全部）
    std::vector<uint32_t> output_n_elems;  // 各输出元素数
    std::vector<uint32_t> output_sizes;    // 各输出字节（native type）
    std::vector<RknnOutputInfo> outputs;   // 各输出完整属性（A-6 Decode 反量化用）
};

// 单 Worker 分阶段统计
struct RknnStageStats {
    StatsCollector set_input;
    StatsCollector run;
    StatsCollector output;
    StatsCollector total;
};

class RKNNEngine {
public:
    struct Impl;  // pimpl（A-6 outputs_get_impl 辅助函数需要访问）
    struct Params {
        std::string model_path;       // .rknn 路径（由 config/ModelStore 提供，不硬编码）
        int core_mask = 0;            // 0=RKNN_NPU_CORE_AUTO（来自 config/model 配置，不写死）
        bool pass_through = true;     // true=输入零拷贝（要求 dtype/layout 与模型完全一致）；
                                      // 失败自动回退并记录
    };

    RKNNEngine();
    ~RKNNEngine();
    RKNNEngine(const RKNNEngine&) = delete;
    RKNNEngine& operator=(const RKNNEngine&) = delete;

    // 加载模型 + 初始化 runtime + 设置 core_mask。成功返回 true。
    bool init(const Params& params, std::string* error = nullptr);
    void destroy();

    bool initialized() const { return inited_; }
    const RknnModelInfo& info() const { return info_; }
    const RknnStageStats& stats() const { return stats_; }
    void reset_stats();  // 清空分阶段统计（预热后调用）
    double load_ms() const { return load_ms_; }          // 模型加载 + init 耗时（ms）
    bool pass_through_active() const { return pass_through_active_; }

    // RK3588 高性能路径：初始化一次性绑定的 RKNN tensor memory。
    // 失败时调用方继续使用 set_input/run/get_raw_outputs 回退路径。
    bool init_zero_copy(std::string* error = nullptr);
    void* input_memory() const;
    size_t input_memory_size() const;
    void* output_memory(uint32_t index) const;
    size_t output_memory_size(uint32_t index) const;
    bool zero_copy_ready() const { return zero_copy_ready_; }
    bool run_zero_copy(std::string* error = nullptr);

    // 设置输入（零拷贝：直接引用 buf，不复制）。size 为 buf 有效字节。
    bool set_input(const void* buf, size_t size, std::string* error = nullptr);

    // 执行 NPU 推理
    bool run(std::string* error = nullptr);

    // 获取输出（want_float=1，float32；调用方预分配 out_bufs[i] 至少 info_.output_sizes[i]*4 字节）
    bool get_outputs(void** out_bufs, size_t* out_sizes, std::string* error = nullptr);

    // 获取原生输出（want_float=0，零转换；调用方按 info_.outputs[i].size 预分配 buffer）。
    // A-6 Decode 直供路径：禁止无意义的 float 转换。
    bool get_raw_outputs(void** out_bufs, size_t* out_sizes, std::string* error = nullptr);

    // 便捷：一帧完整推理（set_input + run + outputs），输出为 float32 向量
    bool infer(const void* input_buf, size_t input_size,
               std::vector<std::vector<float>>& outputs,
               std::string* error = nullptr);

private:
    std::unique_ptr<Impl> impl_;
    Params params_;
    RknnModelInfo info_;
    RknnStageStats stats_;
    double load_ms_ = 0.0;
    bool inited_ = false;
    bool pass_through_active_ = false;
    bool zero_copy_ready_ = false;
};

}  // namespace ttbox::core
