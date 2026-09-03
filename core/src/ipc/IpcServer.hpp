// IpcServer.hpp — AF_UNIX JSON 行协议服务端（阶段 A-1：最小 IPC）
//
// 协议（详见 docs/ipc-protocol.md）：
//   - 传输：AF_UNIX SOCK_STREAM（Unix）/ TCP loopback（Windows，路径 "tcp:PORT"）
//   - 帧格式：一行 JSON（'\n' 分隔）
//   - 请求：{"id":<可选>,"type":"PING|GET_STATUS|GET_CONFIG|SET_CONFIG|RUNTIME_CONTROL","params":<可选>}
//   - 响应：{"id":...,"type":...,"status":<错误码>,"data":{...},"error":<可选>}
//   - 错误码：0 OK / 1 BAD_REQUEST / 2 NOT_FOUND / 3 INTERNAL / 4 UNSUPPORTED
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <thread>

#include "common/Json.hpp"
#include "common/Metrics.hpp"

namespace ttbox::core {

// IPC 错误码（与 docs/ipc-protocol.md 保持一致）
enum class IpcError : int {
    kOk = 0,
    kBadRequest = 1,
    kNotFound = 2,
    kInternal = 3,
    kUnsupported = 4,
};

struct IpcResponse {
    IpcError status = IpcError::kInternal;
    std::string id;
    std::string type;
    std::string error;
    JsonValue data = JsonValue::object();

    // 序列化为一行 JSON（含换行）
    std::string to_json() const;
};

class IpcServer {
public:
    ~IpcServer();

    // 启动监听。socket_path: Unix 下为文件路径（默认 /tmp/ttbox_core.sock）；
    // Windows 下为 "tcp:<port>"。
    bool start(const std::string& socket_path, std::string* error = nullptr);
    void stop();
    bool running() const { return running_.load(); }
    const std::string& socket_path() const { return socket_path_; }

    // 数据提供回调（由 Application 注入）
    using StatusProvider = std::function<SystemStatus()>;
    using ConfigProvider = std::function<JsonValue()>;
    void set_status_provider(StatusProvider p) { status_provider_ = std::move(p); }
    void set_config_provider(ConfigProvider p) { config_provider_ = std::move(p); }

    // Phase2：预览快照回调（返回 JPEG 字节；无帧返回 false）
    using PreviewProvider = std::function<bool(std::vector<uint8_t>*, uint64_t*)>;  // 出参2: 帧序号（seq，供 web 去重）
    void set_preview_provider(PreviewProvider p) { preview_provider_ = std::move(p); }

    // 配置写入回调（SET_CONFIG）：由 Application 注入。
    // 原子序（在回调内实现）：JSON 解析 → RuntimeProfile::validate → RuntimeConfig.update → 持久化。
    // 返回 false 时 resp.error 带失败原因，当前运行配置保证不被污染。
    // persisted=true 表示已写入配置文件；data 返回 {"applied":true,"persisted":bool}。
    using ConfigUpdateHandler = std::function<bool(const JsonValue& profile_json,
                                                   std::string* error, bool* persisted)>;
    void set_config_update_handler(ConfigUpdateHandler h) { config_update_ = std::move(h); }

    // Runtime 启停回调（RUNTIME_CONTROL）：action = "start"|"stop"|"restart"。
    using RuntimeControlHandler = std::function<bool(const std::string& action, std::string* error)>;
    void set_runtime_control_handler(RuntimeControlHandler h) { runtime_control_ = std::move(h); }

    // ---- 模型管理回调（v0.3）：桥接 ModelRegistry，由 Application 注入 ----
    // 注意：文件上传不走 IPC（IPC 帧不适合大二进制）。上传 = 调用方先把 .rknn
    // 写入 Core 可访问的 staging 收件目录（默认 models/_incoming/<文件名>），
    // 再发 MODEL_IMPORT 引用该路径。Gateway 的 HTTP 上传端点负责落盘到该目录。
    // list：返回 installed 模型清单 + 当前激活 id
    using ModelListHandler = std::function<JsonValue()>;
    // import：src_path（已在收件目录）→ model_id, label
    using ModelImportHandler = std::function<bool(const std::string& src_path,
                                                  const std::string& model_id,
                                                  const std::string& label,
                                                  std::string* error)>;
    // validate / install / activate / remove：按 model_id
    using ModelActionHandler = std::function<bool(const std::string& model_id, std::string* error)>;
    void set_model_list_handler(ModelListHandler h) { model_list_ = std::move(h); }
    void set_model_import_handler(ModelImportHandler h) { model_import_ = std::move(h); }
    void set_model_validate_handler(ModelActionHandler h) { model_validate_ = std::move(h); }
    void set_model_install_handler(ModelActionHandler h) { model_install_ = std::move(h); }
    void set_model_activate_handler(ModelActionHandler h) { model_activate_ = std::move(h); }
    void set_model_remove_handler(ModelActionHandler h) { model_remove_ = std::move(h); }

private:
    void accept_loop();
    void handle_connection(int fd);
    IpcResponse handle_request(const JsonValue& request);

    std::string socket_path_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    StatusProvider status_provider_;
    PreviewProvider preview_provider_;
    ConfigProvider config_provider_;
    ConfigUpdateHandler config_update_;
    RuntimeControlHandler runtime_control_;
    ModelListHandler model_list_;
    ModelImportHandler model_import_;
    ModelActionHandler model_validate_;
    ModelActionHandler model_install_;
    ModelActionHandler model_activate_;
    ModelActionHandler model_remove_;
};

// 同步请求客户端：发送一行 JSON 请求，读取一行 JSON 响应。
// 成功返回 true，response 为响应 JSON 文本；失败返回 false（error 说明）。
bool ipc_request(const std::string& socket_path, const std::string& request_json,
                 std::string& response, int timeout_ms = 2000, std::string* error = nullptr);

// 便捷 PING：成功返回 true（可同时拿到 data.pong）
bool ipc_ping(const std::string& socket_path, std::string* error = nullptr);

// SystemStatus -> JSON（供 GET_STATUS 输出）
JsonValue system_status_to_json(const SystemStatus& status);

}  // namespace ttbox::core
