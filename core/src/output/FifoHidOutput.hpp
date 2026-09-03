// FifoHidOutput.hpp — 兼容现有 ttbox-hid-bridge 的 FIFO 输出后端。
#pragma once
#include "output/IHidOutput.hpp"
#include <string>
namespace ttbox::core::output {
class FifoHidOutput final : public IHidOutput {
public:
    explicit FifoHidOutput(std::string path) : path_(std::move(path)) {}
    ~FifoHidOutput() override { close(); }
    bool send(const OutputAction& action) override;
    void close();
private:
    bool open_if_needed();
    bool send_control();
    std::string path_;
    int fd_ = -1;
    bool control_sent_ = false;
};
}
