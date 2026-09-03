// AiboxHidOutput.hpp — AIBOX 兼容的 /dev/hidg0 直接输出后端
// 报告格式: buttons(16bit LE) + X(int16 LE) + Y(int16 LE) + wheel(8) + pan(8)
#pragma once
#include "output/IHidOutput.hpp"
#include <string>
#include <atomic>
namespace ttbox::core { class RuntimeConfig; }
namespace ttbox::core::output {
class AiboxHidOutput final : public IHidOutput {
public:
    explicit AiboxHidOutput(std::string hidg_path = "/dev/hidg1") : path_(std::move(hidg_path)) {}
    ~AiboxHidOutput() override { close(); }
    bool send(const OutputAction& action) override;
    // 静态总闸（output_enabled / 外部 kill 开关）；mouse.enabled 由配置实时判定。
    void set_enabled(bool enabled) { enabled_ = enabled; }
    // 保险门按钮源；放行掩码不再由调用方传入（禁止写死），每次发送时从 config_source_ 实时读取。
    void set_button_source(std::atomic<uint16_t>* source) { button_source_ = source; }
    // 绑定运行时配置：热键 mask 与 mouse.enabled 每次发送时取实时快照，改配置即时生效。
    void set_config_source(ttbox::core::RuntimeConfig* config) { config_source_ = config; }
    void close();
private:
    bool open_if_needed();
    std::string path_;
    int fd_ = -1;
    bool enabled_ = false;
    std::atomic<uint16_t>* button_source_ = nullptr;
    ttbox::core::RuntimeConfig* config_source_ = nullptr;
};
}  // namespace ttbox::core::output
