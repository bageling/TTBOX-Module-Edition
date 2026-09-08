// ModelRuntimeState.hpp — 模型运行状态提交门。
// 只允许在真实 inference 与 Decode 都成功后更新 running_model_id。
#pragma once

#include <string>

namespace ttbox::core {

class ModelRuntimeState {
public:
    void select(const std::string& model_id) {
        selected_model_id_ = model_id;
        failure_code_.clear();
    }

    void set_running_model(const std::string& model_id) {
        running_model_id_ = model_id;
    }

    void mark_init_failed() {
        inference_passed_ = false;
        decode_passed_ = false;
        failure_code_ = "RKNN_INIT_FAILED";
    }

    void mark_inference_failed() {
        inference_passed_ = false;
        decode_passed_ = false;
        failure_code_ = "INFERENCE_FAILED";
    }

    void mark_decode_failed() {
        decode_passed_ = false;
        failure_code_ = "DECODE_FAILED";
    }

    void mark_inference_pass() {
        inference_passed_ = true;
    }

    void mark_decode_pass() {
        decode_passed_ = true;
    }

    bool commit_ready() {
        if (!inference_passed_ || !decode_passed_ || selected_model_id_.empty()) return false;
        running_model_id_ = selected_model_id_;
        failure_code_.clear();
        return true;
    }

    void stop() {
        running_model_id_.clear();
        failure_code_.clear();
        inference_passed_ = false;
        decode_passed_ = false;
    }

    const std::string& selected_model_id() const { return selected_model_id_; }
    const std::string& running_model_id() const { return running_model_id_; }
    const std::string& failure_code() const { return failure_code_; }
    bool inference_passed() const { return inference_passed_; }
    bool decode_passed() const { return decode_passed_; }

private:
    std::string selected_model_id_;
    std::string running_model_id_;
    std::string failure_code_;
    bool inference_passed_ = false;
    bool decode_passed_ = false;
};

}  // namespace ttbox::core
