// OutputBackend.hpp — 统一输出后端抽象（对齐 YU 外设后端）
//
// 目标：AimThread 只产生 OutputAction（dx/dy/buttons），不判断设备类型。
//       OutputBackend 作为 IHidOutput 的兼容实现，当前只含 LocalHidBackend。
//
// 设计依据：docs/research/YU_OUTPUT_BACKEND_RESEARCH.md（YU 真机实证）。
// 纪律：
//   - Hotkey Gate / mouse.enabled 实时判定逻辑保持与现 AiboxHidOutput 完全一致；
//   - send() 热路径零分配、无锁、无日志；
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "output/IHidOutput.hpp"

namespace ttbox::core { class RuntimeConfig; }

namespace ttbox::core::output {

struct OutputAction;  // 来自 IHidOutput.hpp

// ---------------------------------------------------------------------------
// 后端状态 / 健康
// ---------------------------------------------------------------------------
enum class BackendState {
    kDisconnected,
    kConnecting,
    kConnected,
    kError,
};

struct BackendHealth {
    BackendState state = BackendState::kDisconnected;
    std::string detail;           // 人类可读（Web 展示）
    uint64_t send_ok = 0;
    uint64_t send_fail = 0;
    uint64_t reconnect_count = 0;
    int64_t last_send_ok_us = 0;  // 最近成功发送时刻（steady，us）
    uint64_t socket_write_ok = 0;
    uint64_t socket_write_fail = 0;
    uint64_t send_count = 0;
    int32_t last_dx = 0;
    int32_t last_dy = 0;
    int32_t last_wheel = 0;
    uint64_t last_timestamp_us = 0;
};

// ---------------------------------------------------------------------------
// 按钮/动作编码（对齐 YU usb-proxy 实证；各后端映射到自己协议）
// ---------------------------------------------------------------------------
constexpr uint8_t kBtnLeft = 1;
constexpr uint8_t kBtnRight = 2;
constexpr uint8_t kBtnMiddle = 3;
constexpr uint8_t kBtnBack = 4;
constexpr uint8_t kBtnForward = 5;
constexpr uint8_t kActDown = 1;
constexpr uint8_t kActUp = 2;
constexpr uint8_t kActClick = 3;

// ---------------------------------------------------------------------------
// IOutputBackend：一种物理设备协议
// ---------------------------------------------------------------------------
class IOutputBackend {
public:
    virtual ~IOutputBackend() = default;

    // 生命周期
    virtual bool connect(std::string* error = nullptr) = 0;
    virtual void disconnect() = 0;
    virtual bool reconnect(std::string* error = nullptr) = 0;
    virtual BackendHealth health() const = 0;

    // 输出
    virtual bool mouse_move(int32_t dx, int32_t dy, int32_t wheel = 0) = 0;
    virtual bool mouse_button(uint8_t button, uint8_t action) = 0;
    virtual bool mouse_click(uint8_t button) = 0;

    virtual const char* name() const = 0;

    // ---- Hotkey Gate / 总闸（基类实现，与现 AiboxHidOutput 一致）----
    void set_enabled(bool enabled) { enabled_ = enabled; }
    void set_button_source(std::atomic<uint16_t>* source) { button_source_ = source; }
    void set_config_source(RuntimeConfig* config) { config_source_ = config; }

protected:
    // 发送前调用：false = 被 Gate 拦截（不发送）。
    // 判定顺序与 AiboxHidOutput::send 完全一致（fail-closed）：
    //   1) 静态总闸；2) config 缺失；3) mouse.enabled；4) 热键 mask 缺失；5) 热键未按下。
    // 实现位于 OutputBackend.cpp（需 RuntimeConfig 完整定义）。
    bool gate_allows() const;

    std::atomic<uint16_t>* button_source_ = nullptr;
    RuntimeConfig* config_source_ = nullptr;
    bool enabled_ = false;
};

// ---------------------------------------------------------------------------
// OutputBackend：设备选择器（IHidOutput 兼容实现，AimThread 零改动）
// ---------------------------------------------------------------------------
class OutputBackend final : public IHidOutput {
public:
    struct Params {
        std::string kind = "local_hid";   // local_hid
        std::string hidg_path = "/dev/hidg1";
        std::string proxy_socket_path = "/run/orangepi-mouse-passthrough/cmd.sock";
        // Gate / 运行时
        RuntimeConfig* runtime_config = nullptr;
        std::atomic<uint16_t>* button_source = nullptr;
        bool enabled = false;
    };

    OutputBackend() = default;
    ~OutputBackend();

    bool configure(const Params& p, std::string* error = nullptr);

    IOutputBackend* backend() { return backend_.get(); }
    const IOutputBackend* backend() const { return backend_.get(); }
    const Params& params() const { return params_; }

    bool send(const OutputAction& action) override;

    BackendHealth health() const;
    const char* backend_name() const;

    void set_enabled(bool enabled);
    void set_button_source(std::atomic<uint16_t>* source);
    void set_config_source(RuntimeConfig* config);

private:
    std::unique_ptr<IOutputBackend> backend_;
    Params params_;
};

}  // namespace ttbox::core::output
