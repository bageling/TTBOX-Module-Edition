// HighPerfRknnEngine.cpp — 每 Worker 独占 context + I/O memory。
#include "detector/highperf/HighPerfRknnEngine.hpp"

#if defined(_WIN32)
namespace ttbox::core::highperf {
struct HighPerfRknnEngine::Impl {};
HighPerfRknnEngine::HighPerfRknnEngine() : impl_(std::make_unique<Impl>()) {}
HighPerfRknnEngine::~HighPerfRknnEngine() = default;
bool HighPerfRknnEngine::init(const std::string&, int, bool, std::string* error) { if (error) *error = "RKNN 仅在 Unix 后端可用"; return false; }
void HighPerfRknnEngine::destroy() {}
void* HighPerfRknnEngine::input_memory() const { return nullptr; }
size_t HighPerfRknnEngine::input_memory_size() const { return 0; }
void* HighPerfRknnEngine::output_memory(uint32_t) const { return nullptr; }
size_t HighPerfRknnEngine::output_memory_size(uint32_t) const { return 0; }
bool HighPerfRknnEngine::copy_input(const void*, size_t, std::string* error) { if (error) *error = "RKNN 仅在 Unix 后端可用"; return false; }
bool HighPerfRknnEngine::run(std::string* error) { if (error) *error = "RKNN 仅在 Unix 后端可用"; return false; }
}
#else

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <rknn_api.h>

namespace ttbox::core::highperf {

struct HighPerfRknnEngine::Impl {
    rknn_context context = 0;
    rknn_tensor_mem* input_mem = nullptr;
    std::vector<rknn_tensor_mem*> output_mems;
};

HighPerfRknnEngine::HighPerfRknnEngine() : impl_(std::make_unique<Impl>()) {}
HighPerfRknnEngine::~HighPerfRknnEngine() { destroy(); }

bool HighPerfRknnEngine::init(const std::string& model_path, int core_mask,
                              bool pass_through, std::string* error) {
    if (initialized_) { if (error) *error = "高性能 RKNN 已初始化"; return false; }
    if (model_path.empty()) { if (error) *error = "模型路径为空"; return false; }
    if (core_mask != 1 && core_mask != 2 && core_mask != 4) {
        if (error) *error = "高性能后端要求 core_mask 为 1、2 或 4"; return false;
    }
    std::ifstream file(model_path, std::ios::binary | std::ios::ate);
    if (!file) { if (error) *error = "模型文件无法打开: " + model_path; return false; }
    const auto length = file.tellg();
    if (length <= 0) { if (error) *error = "模型文件为空"; return false; }
    std::vector<uint8_t> model(static_cast<size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(model.data()), length);
    if (!file) { if (error) *error = "模型文件读取失败"; return false; }

    int rc = rknn_init(&impl_->context, model.data(), static_cast<uint32_t>(model.size()), 0, nullptr);
    if (rc != RKNN_SUCC) { if (error) *error = "rknn_init 失败 rc=" + std::to_string(rc); return false; }
    rc = rknn_set_core_mask(impl_->context, static_cast<rknn_core_mask>(core_mask));
    if (rc != RKNN_SUCC) { if (error) *error = "rknn_set_core_mask 失败 rc=" + std::to_string(rc); destroy(); return false; }

    rknn_input_output_num numbers{};
    if (rknn_query(impl_->context, RKNN_QUERY_IN_OUT_NUM, &numbers, sizeof(numbers)) != RKNN_SUCC || numbers.n_input != 1) {
        if (error) *error = "模型必须是单输入模型"; destroy(); return false;
    }
    rknn_tensor_attr input{}; input.index = 0;
    if (rknn_query(impl_->context, RKNN_QUERY_INPUT_ATTR, &input, sizeof(input)) != RKNN_SUCC) {
        if (error) *error = "输入 tensor 查询失败"; destroy(); return false;
    }
    info_.input_count = numbers.n_input;
    info_.output_count = numbers.n_output;
    info_.input.index = 0; info_.input.n_elems = input.n_elems; info_.input.size = input.size;
    info_.input.size_with_stride = input.size_with_stride; info_.input.type = input.type; info_.input.format = input.fmt;
    info_.input.zero_point = input.zp; info_.input.scale = input.scale;
    info_.input.dims.assign(input.dims, input.dims + input.n_dims);

    if (pass_through && (input.type != RKNN_TENSOR_INT8 || input.fmt != RKNN_TENSOR_NHWC)) {
        if (error) *error = "当前模型不是 INT8/NHWC，拒绝启用 pass_through"; destroy(); return false;
    }
    rknn_tensor_attr input_binding = input;
    if (pass_through) { input_binding.type = RKNN_TENSOR_INT8; input_binding.fmt = RKNN_TENSOR_NHWC; input_binding.pass_through = 1; }
    impl_->input_mem = rknn_create_mem(impl_->context, input_binding.size_with_stride);
    if (!impl_->input_mem || rknn_set_io_mem(impl_->context, impl_->input_mem, &input_binding) != RKNN_SUCC) {
        if (error) *error = "输入 rknn_create_mem/rknn_set_io_mem 失败"; destroy(); return false;
    }

    for (uint32_t i = 0; i < numbers.n_output; ++i) {
        rknn_tensor_attr output{}; output.index = i;
        if (rknn_query(impl_->context, RKNN_QUERY_OUTPUT_ATTR, &output, sizeof(output)) != RKNN_SUCC) {
            if (error) *error = "输出 tensor 查询失败"; destroy(); return false;
        }
        TensorInfo tensor; tensor.index=i; tensor.n_elems=output.n_elems; tensor.size=output.size;
        tensor.size_with_stride=output.size_with_stride; tensor.type=output.type; tensor.format=output.fmt;
        tensor.zero_point=output.zp; tensor.scale=output.scale; tensor.dims.assign(output.dims, output.dims+output.n_dims); info_.outputs.push_back(tensor);
        auto* memory = rknn_create_mem(impl_->context, output.size_with_stride);
        if (!memory || rknn_set_io_mem(impl_->context, memory, &output) != RKNN_SUCC) { if (memory) rknn_destroy_mem(impl_->context,memory); if (error) *error = "输出零拷贝绑定失败"; destroy(); return false; }
        impl_->output_mems.push_back(memory);
    }
    pass_through_ = pass_through; initialized_ = true; zero_copy_ready_ = true; return true;
}

void HighPerfRknnEngine::destroy() {
    if (!impl_) return;
    if (impl_->input_mem && impl_->context) rknn_destroy_mem(impl_->context, impl_->input_mem);
    impl_->input_mem=nullptr;
    for (auto* memory: impl_->output_mems) if (memory && impl_->context) rknn_destroy_mem(impl_->context,memory);
    impl_->output_mems.clear();
    if (impl_->context) rknn_destroy(impl_->context);
    impl_->context=0; initialized_=false; zero_copy_ready_=false; info_={};
}

void* HighPerfRknnEngine::input_memory() const { return impl_ && impl_->input_mem ? impl_->input_mem->virt_addr : nullptr; }
size_t HighPerfRknnEngine::input_memory_size() const { return impl_ && impl_->input_mem ? impl_->input_mem->size : 0; }
void* HighPerfRknnEngine::output_memory(uint32_t index) const { return impl_ && index<impl_->output_mems.size() ? impl_->output_mems[index]->virt_addr : nullptr; }
size_t HighPerfRknnEngine::output_memory_size(uint32_t index) const { return impl_ && index<impl_->output_mems.size() ? impl_->output_mems[index]->size : 0; }

bool HighPerfRknnEngine::copy_input(const void* data, size_t size, std::string* error) {
    if (!zero_copy_ready_ || !input_memory() || size > input_memory_size()) { if (error) *error = "零拷贝输入缓冲区无效或尺寸超限"; return false; }
    std::memcpy(input_memory(), data, size); return true;
}
bool HighPerfRknnEngine::run(std::string* error) {
    if (!zero_copy_ready_) { if (error) *error = "零拷贝后端未初始化"; return false; }
    const int rc=rknn_run(impl_->context,nullptr); if (rc!=RKNN_SUCC) { if (error) *error="rknn_run 失败 rc="+std::to_string(rc); return false; } return true;
}
}
#endif
