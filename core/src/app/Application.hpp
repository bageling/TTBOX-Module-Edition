// Application.hpp — C++ Core 应用生命周期（initialize / run / shutdown）
#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "auth/AiboxLicenseClient.hpp"
#include "auth/LicenseDaemon.hpp"
#include "common/Metrics.hpp"
#include "config/ConfigManager.hpp"
#include "ipc/IpcServer.hpp"
#include "model/ModelManagement.hpp"
#include "model/RuntimeProfile.hpp"
#include "output/IHidOutput.hpp"
#include "runtime/CoreRuntime.hpp"

namespace ttbox::core {

// Application — 应用主类："总入口"。
// 职责：解析命令行/配置 → 授权校验 → 组装 CoreRuntime → 启动 IPC 服务
//       → 事件循环（自动重试/心跳）→ 优雅退出。
// 输入：命令行参数（--config 路径 / --ipc socket 路径）
// 输出：运行中的完整系统（核心 + IPC + 授权）
class Application {
public:
    Application() = default;
    ~Application();

    // 解析 CLI + 初始化 Logger/Config/IPC/CoreRuntime/授权。
    // 成功返回 0；失败返回非 0（含明确错误日志）。
    int initialize(int argc, char** argv);

    // 事件循环：启动 CoreRuntime，阻塞直到收到 shutdown 请求。
    // 每 tick 执行状态同步 + IPC 心跳；退出时按序停止 CoreRuntime。
    void run();

    // 请求退出（线程安全；signal handler 可直接调用，仅置原子标志）。
    static void request_shutdown();

    // 清理：停止 CoreRuntime → 停止 IPC → 停止 授权 → 写日志。
    void shutdown();

    bool running() const { return running_.load(); }
    const ConfigManager& config() const { return config_; }
    SystemStatus status() const;

    // 对外查询接口（授权状态 / 卡号 / Pro 功能）
    auth::LicenseStatus license_status_snapshot() const;
    bool license_allow_run() const;
    bool license_is_pro() const;

private:
    // 供 IpcServer providers 使用
    SystemStatus status_provider() const;
    JsonValue config_provider() const;

    // SET_CONFIG 原子更新（解析→validate→RuntimeConfig.update→落盘，任一失败不污染现配置）。
    // 返回 false 时 error 说明原因；persisted 表示是否写入配置文件。
    bool handle_config_update(const JsonValue& profile_json, std::string* error, bool* persisted);
    // RUNTIME_CONTROL 启停（复用 CoreRuntime start/stop，不改状态机）
    bool handle_runtime_control(const std::string& action, std::string* error);
    // 模型管理（v0.3）：桥接 ModelRegistry（import/validate/install/activate/remove/list）
    bool handle_model_import(const std::string& src_path, const std::string& model_id,
                             const std::string& label, std::string* error);
    JsonValue handle_model_list();
    bool handle_model_validate(const std::string& model_id, std::string* error);
    bool handle_model_install(const std::string& model_id, std::string* error);
    bool handle_model_activate(const std::string& model_id, std::string* error);
    bool handle_model_remove(const std::string& model_id, std::string* error);

    // 从配置构造 CoreRuntime 参数
    bool build_runtime_params(CoreRuntime::Params& out_params, std::string* error);

    // 加载卡号：优先级 --license > /etc/ttbox/license.key > 配置 license_card_key
    std::string resolve_license_card(const std::string& cli_license) const;

    ConfigManager config_;
    IpcServer ipc_;
    std::string ipc_path_ = "/tmp/ttbox_core.sock";
    std::string config_path_;
    std::atomic<bool> running_{false};
    double start_time_ms_ = 0.0;
    bool initialized_ = false;
    bool verify_only_ = false;  // --verify-only：授权完立即退出，不启推理

    // ---- 核心链路（接入 Application 生命周期）----
    RuntimeConfig runtime_config_;
    std::shared_ptr<output::IHidOutput> hid_output_;
    std::unique_ptr<CoreRuntime> core_runtime_;
    bool runtime_started_ = false;
    // 期望运行标志（自动启停核心）：true=应保持 runtime 运行，false=用户手动停止。
    // 开机/start 时置 true；用户 /api/control/stop 置 false。主循环据此自动重试/自恢复。
    std::atomic<bool> want_runtime_running_{true};
    // 模型仓库（v0.3）：root = 配置 model_registry_root 或 <项目>/models
    std::unique_ptr<ModelManagement> model_management_;

    // ---- 授权（等价原 aibox-bl cardVerifyThreadFunc）----
    std::unique_ptr<auth::AiboxLicenseClient> license_client_;
    std::unique_ptr<auth::LicenseDaemon> license_daemon_;
    std::string license_override_pro_endpoint_;   // --debug-license-pro-endpoint
    std::string license_override_normal_endpoint_; // --debug-license-normal-endpoint
    std::string license_server_secret_;           // ACCESS_KEY 签名密钥（仅开发环境显式传入）
};

}  // namespace ttbox::core
