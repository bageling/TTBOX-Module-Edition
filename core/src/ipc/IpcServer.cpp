// IpcServer.cpp — IPC 服务端/客户端实现（Unix AF_UNIX，Windows TCP loopback）
#include "ipc/IpcServer.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>

#include "common/Logger.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace ttbox::core {

namespace {

#if defined(_WIN32)
// MSVC 无 ssize_t：Windows 分支统一用 long long 语义的别名。
using ssize_t = long long;
// Windows：路径 "tcp:<port>"。返回监听 fd（SOCKET 转 int），失败 -1。
int listen_tcp(const std::string& path, std::string* error) {
    static bool ws_inited = []() {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    (void)ws_inited;

    std::string port_str = path;
    const std::string prefix = "tcp:";
    if (port_str.rfind(prefix, 0) == 0) port_str = port_str.substr(prefix.size());
    int port = 0;
    try {
        port = std::stoi(port_str);
    } catch (...) {
        if (error) *error = "非法 TCP 端口: " + path;
        return -1;
    }

    SOCKET fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        if (error) *error = "socket() 失败";
        return -1;
    }
    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (error) *error = "bind() 失败 (端口 " + port_str + " 可能被占用)";
        ::closesocket(fd);
        return -1;
    }
    if (::listen(fd, 8) != 0) {
        if (error) *error = "listen() 失败";
        ::closesocket(fd);
        return -1;
    }
    return static_cast<int>(fd);
}

int connect_tcp(const std::string& path, std::string* error) {
    std::string port_str = path;
    const std::string prefix = "tcp:";
    if (port_str.rfind(prefix, 0) == 0) port_str = port_str.substr(prefix.size());
    int port = 0;
    try {
        port = std::stoi(port_str);
    } catch (...) {
        if (error) *error = "非法 TCP 端口: " + path;
        return -1;
    }
    SOCKET fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        if (error) *error = "socket() 失败";
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (error) *error = "connect() 失败";
        ::closesocket(fd);
        return -1;
    }
    return static_cast<int>(fd);
}

ssize_t sock_send(int fd, const void* buf, size_t len) {
    return static_cast<ssize_t>(::send(static_cast<SOCKET>(fd),
                                       reinterpret_cast<const char*>(buf),
                                       static_cast<int>(len), 0));
}

ssize_t sock_recv(int fd, void* buf, size_t len) {
    return static_cast<ssize_t>(::recv(static_cast<SOCKET>(fd),
                                       reinterpret_cast<char*>(buf),
                                       static_cast<int>(len), 0));
}

void sock_close(int fd) { ::closesocket(static_cast<SOCKET>(fd)); }

#else  // !_WIN32 (Unix)

int listen_unix(const std::string& path, std::string* error) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        if (error) *error = "socket() 失败: " + std::string(std::strerror(errno));
        return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        if (error) *error = "socket 路径过长: " + path;
        ::close(fd);
        return -1;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    // 清理残留 socket 文件（若存在且可写）
    ::unlink(path.c_str());
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (error) *error = "bind() 失败 (" + path + "): " + std::string(std::strerror(errno));
        ::close(fd);
        return -1;
    }
    if (::listen(fd, 8) != 0) {
        if (error) *error = "listen() 失败: " + std::string(std::strerror(errno));
        ::close(fd);
        ::unlink(path.c_str());
        return -1;
    }
    ::chmod(path.c_str(), 0666);  // 允许非 root 客户端连接
    return fd;
}

int connect_unix(const std::string& path, std::string* error) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        if (error) *error = "socket() 失败";
        return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        if (error) *error = "socket 路径过长: " + path;
        ::close(fd);
        return -1;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (error) *error = "connect() 失败 (" + path + "): " + std::string(std::strerror(errno));
        ::close(fd);
        return -1;
    }
    return fd;
}

ssize_t sock_send(int fd, const void* buf, size_t len) {
    return ::send(fd, buf, len, 0);
}

ssize_t sock_recv(int fd, void* buf, size_t len) {
    return ::recv(fd, buf, len, 0);
}

void sock_close(int fd) { ::close(fd); }

#endif  // _WIN32

std::string read_line(int fd, bool* ok) {
    std::string buf;
    char tmp[4096];
    *ok = true;
    while (true) {
        ssize_t n = sock_recv(fd, tmp, sizeof(tmp));
        if (n <= 0) {
            *ok = false;
            break;
        }
        buf.append(tmp, static_cast<size_t>(n));
        auto pos = buf.find('\n');
        if (pos != std::string::npos) {
            buf.resize(pos);
            break;
        }
        if (buf.size() > 65536) {
            *ok = false;  // 超长请求，防滥用
            break;
        }
    }
    return buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// IpcResponse
// ---------------------------------------------------------------------------

std::string IpcResponse::to_json() const {
    JsonValue resp = JsonValue::object();
    resp.set("id", JsonValue::string(id));
    resp.set("type", JsonValue::string(type));
    resp.set("status", JsonValue::number(static_cast<double>(static_cast<int>(status))));
    resp.set("data", data);
    if (!error.empty()) {
        resp.set("error", JsonValue::string(error));
    }
    return resp.dump() + "\n";
}

// ---------------------------------------------------------------------------
// IpcServer
// ---------------------------------------------------------------------------

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(const std::string& socket_path, std::string* error) {
    if (running_.load()) {
        if (error) *error = "IpcServer 已在运行";
        return false;
    }
    int fd = -1;
#if defined(_WIN32)
    fd = listen_tcp(socket_path, error);
#else
    fd = listen_unix(socket_path, error);
#endif
    if (fd < 0) return false;

    socket_path_ = socket_path;
    listen_fd_ = fd;
    running_.store(true);
    accept_thread_ = std::thread(&IpcServer::accept_loop, this);

    // 自举握手：Windows 下连接刚 listen 的 socket 偶发 WSAECONNREFUSED（accept 尚未就绪），
    // 这里内部自连一次 PING，确保 start() 返回后任何客户端首次连接都能成功。
    // 失败重试 5 次 × 40ms；全部失败则回滚启动（fail-fast，不带病运行）。
    bool ready = false;
    std::string probe_error;
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::string pong;
        if (ipc_request(socket_path_, R"({"type":"PING"})", pong, 500, &probe_error)) {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    if (!ready) {
        running_.store(false);
        if (accept_thread_.joinable()) accept_thread_.join();
#if defined(_WIN32)
        ::closesocket(static_cast<SOCKET>(listen_fd_));
#else
        ::close(listen_fd_);
#endif
        listen_fd_ = -1;
        if (error) *error = "IPC 自举握手失败: " + probe_error;
        return false;
    }

    TTBOX_LOG_INFO("IPC 服务已启动: " + socket_path);
    return true;
}

void IpcServer::stop() {
    if (!running_.exchange(false)) return;

#if defined(_WIN32)
    if (listen_fd_ >= 0) {
        // Windows 下 shutdown() 不保证唤醒阻塞 accept；先关闭监听 socket，
        // 让 accept 返回 INVALID_SOCKET，再回收 accept 线程。
        ::shutdown(static_cast<SOCKET>(listen_fd_), SD_BOTH);
        ::closesocket(static_cast<SOCKET>(listen_fd_));
        listen_fd_ = -1;
    }
#else
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);  // 使阻塞的 accept 立即返回
    }
#endif
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    if (listen_fd_ >= 0) {
        sock_close(listen_fd_);
        listen_fd_ = -1;
    }
#if !defined(_WIN32)
    ::unlink(socket_path_.c_str());
#endif
    TTBOX_LOG_INFO("IPC 服务已停止: " + socket_path_);
}

void IpcServer::accept_loop() {
    while (running_.load()) {
        int client_fd = -1;
#if defined(_WIN32)
        SOCKET c = ::accept(static_cast<SOCKET>(listen_fd_), nullptr, nullptr);
        client_fd = c == INVALID_SOCKET ? -1 : static_cast<int>(c);
#else
        client_fd = ::accept(listen_fd_, nullptr, nullptr);
#endif
        if (client_fd < 0) {
            if (running_.load()) {
                TTBOX_LOG_WARN("accept() 失败（服务停止中则忽略）");
            }
            continue;
        }
        std::thread([this, client_fd] { handle_connection(client_fd); }).detach();
    }
}

void IpcServer::handle_connection(int fd) {
    bool ok = false;
    std::string request_text = read_line(fd, &ok);
    std::string response_text;
    if (ok && !request_text.empty()) {
        JsonParseResult parsed = json_parse(request_text);
        IpcResponse resp;
        if (!parsed.ok) {
            resp.status = IpcError::kBadRequest;
            resp.type = "";
            resp.error = "invalid JSON request: " + parsed.error;
        } else if (!parsed.value.is_object()) {
            resp.status = IpcError::kBadRequest;
            resp.error = "request must be a JSON object";
        } else {
            resp = handle_request(parsed.value);
        }
        response_text = resp.to_json();
    } else {
        // 空/异常连接：无需响应
    }
    if (!response_text.empty()) {
        sock_send(fd, response_text.data(), response_text.size());
    }
    sock_close(fd);
}

IpcResponse IpcServer::handle_request(const JsonValue& request) {
    IpcResponse resp;
    const JsonValue* id_v = request.find("id");
    resp.id = id_v ? id_v->as_string() : "";

    const JsonValue* type_v = request.find("type");
    if (type_v == nullptr || !type_v->is_string()) {
        resp.status = IpcError::kBadRequest;
        resp.error = "missing string field 'type'";
        return resp;
    }
    const std::string type = type_v->as_string();
    resp.type = type;

    if (type == "PING") {
        JsonValue data = JsonValue::object();
        data.set("pong", JsonValue::boolean(true));
        data.set("server", JsonValue::string("ttbox_core"));
        resp.status = IpcError::kOk;
        resp.data = std::move(data);
        return resp;
    }

    if (type == "GET_STATUS") {
        if (!status_provider_) {
            resp.status = IpcError::kInternal;
            resp.error = "status provider not registered";
            return resp;
        }
        resp.status = IpcError::kOk;
        resp.data = system_status_to_json(status_provider_());
        return resp;
    }

        if (type == "GET_PREVIEW") {
        if (!preview_provider_) {
            resp.status = IpcError::kInternal;
            resp.error = "preview provider not registered";
            return resp;
        }
        std::vector<uint8_t> jpeg;
        uint64_t pseq = 0;
        if (!preview_provider_(&jpeg, &pseq) || jpeg.empty()) {
            resp.status = IpcError::kNotFound;
            resp.error = "暂无预览帧";
            return resp;
        }
        // base64 编码进 JSON
        static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string b64;
        b64.reserve(((jpeg.size() + 2) / 3) * 4);
        for (size_t i = 0; i < jpeg.size(); i += 3) {
            const uint32_t v = (static_cast<uint32_t>(jpeg[i]) << 16) |
                               (i + 1 < jpeg.size() ? static_cast<uint32_t>(jpeg[i+1]) << 8 : 0) |
                               (i + 2 < jpeg.size() ? static_cast<uint32_t>(jpeg[i+2]) : 0);
            b64 += table[(v >> 18) & 63];
            b64 += table[(v >> 12) & 63];
            b64 += (i + 1 < jpeg.size()) ? table[(v >> 6) & 63] : '=';
            b64 += (i + 2 < jpeg.size()) ? table[v & 63] : '=';
        }
        JsonValue pd = JsonValue::object();
        pd.set("jpeg_base64", JsonValue::string(b64));
        pd.set("bytes", JsonValue::number(static_cast<double>(jpeg.size())));
        pd.set("seq", JsonValue::number(static_cast<double>(pseq)));
        resp.status = IpcError::kOk;
        resp.data = std::move(pd);
        return resp;
    }

if (type == "GET_CONFIG") {
        if (!config_provider_) {
            resp.status = IpcError::kInternal;
            resp.error = "config provider not registered";
            return resp;
        }
        resp.status = IpcError::kOk;
        resp.data = config_provider_();
        return resp;
    }

    // ---- SET_CONFIG：原子更新运行时配置（解析→校验→update→落盘）----
    // 请求：{"type":"SET_CONFIG","params":{"profile":{...RuntimeProfile JSON...}}}
    // 任意一步失败都直接返回错误，当前运行配置与配置文件不被污染。
    if (type == "SET_CONFIG") {
        if (!config_update_) {
            resp.status = IpcError::kInternal;
            resp.error = "config update handler not registered";
            return resp;
        }
        const JsonValue* params = request.find("params");
        const JsonValue* profile_v = params ? params->find("profile") : nullptr;
        if (!profile_v || !profile_v->is_object()) {
            resp.status = IpcError::kBadRequest;
            resp.error = "missing object field params.profile";
            return resp;
        }
        std::string handler_error;
        bool persisted = false;
        if (!config_update_(*profile_v, &handler_error, &persisted)) {
            resp.status = IpcError::kBadRequest;
            resp.error = handler_error.empty() ? "config update rejected" : handler_error;
            return resp;
        }
        JsonValue data = JsonValue::object();
        data.set("applied", JsonValue::boolean(true));
        data.set("persisted", JsonValue::boolean(persisted));
        resp.status = IpcError::kOk;
        resp.data = std::move(data);
        return resp;
    }

    // ---- RUNTIME_CONTROL：启动/停止/重启 AI 流水线 ----
    // 请求：{"type":"RUNTIME_CONTROL","params":{"action":"start|stop|restart"}}
    if (type == "RUNTIME_CONTROL") {
        if (!runtime_control_) {
            resp.status = IpcError::kInternal;
            resp.error = "runtime control handler not registered";
            return resp;
        }
        const JsonValue* params = request.find("params");
        const JsonValue* action_v = params ? params->find("action") : nullptr;
        const std::string action = action_v ? action_v->as_string() : "";
        if (action != "start" && action != "stop" && action != "restart") {
            resp.status = IpcError::kBadRequest;
            resp.error = "params.action must be start|stop|restart";
            return resp;
        }
        std::string handler_error;
        if (!runtime_control_(action, &handler_error)) {
            resp.status = IpcError::kInternal;
            resp.error = handler_error.empty() ? ("runtime " + action + " failed") : handler_error;
            return resp;
        }
        JsonValue data = JsonValue::object();
        data.set("action", JsonValue::string(action));
        resp.status = IpcError::kOk;
        resp.data = std::move(data);
        return resp;
    }

    // ---- 模型管理（v0.3）：LIST / IMPORT / VALIDATE / INSTALL / ACTIVATE / REMOVE ----
    // 通用参数校验辅助：取 params.<field> 字符串
    auto param_str = [&request](const char* field, std::string* out) -> bool {
        const JsonValue* params = request.find("params");
        const JsonValue* v = params ? params->find(field) : nullptr;
        if (!v || !v->is_string() || v->as_string().empty()) return false;
        *out = v->as_string();
        return true;
    };
    // 防 path traversal：model_id 只允许 [A-Za-z0-9_-]
    auto valid_model_id = [](const std::string& id) -> bool {
        if (id.size() < 1 || id.size() > 64) return false;
        for (char c : id) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
        }
        return true;
    };

    if (type == "MODEL_LIST") {
        if (!model_list_) {
            resp.status = IpcError::kInternal;
            resp.error = "model list handler not registered";
            return resp;
        }
        resp.status = IpcError::kOk;
        resp.data = model_list_();
        return resp;
    }

    if (type == "MODEL_IMPORT") {
        if (!model_import_) {
            resp.status = IpcError::kInternal;
            resp.error = "model import handler not registered";
            return resp;
        }
        std::string src_path, model_id, label;
        if (!param_str("src_path", &src_path) || !param_str("model_id", &model_id)) {
            resp.status = IpcError::kBadRequest;
            resp.error = "params.src_path 与 params.model_id 必填（字符串）";
            return resp;
        }
        (void)param_str("label", &label);  // 可选
        if (!valid_model_id(model_id)) {
            resp.status = IpcError::kBadRequest;
            resp.error = "model_id 只允许字母/数字/下划线/连字符（1~64 字符）";
            return resp;
        }
        std::string handler_error;
        if (!model_import_(src_path, model_id, label, &handler_error)) {
            resp.status = IpcError::kBadRequest;
            resp.error = handler_error.empty() ? "模型导入失败" : handler_error;
            return resp;
        }
        JsonValue data = JsonValue::object();
        data.set("model_id", JsonValue::string(model_id));
        resp.status = IpcError::kOk;
        resp.data = std::move(data);
        return resp;
    }

    // MODEL_VALIDATE / MODEL_INSTALL / MODEL_ACTIVATE / MODEL_REMOVE 共用形态
    auto handle_model_action = [&](const char* action_name,
                                   const ModelActionHandler& handler) -> IpcResponse {
        IpcResponse r;
        r.id = resp.id;
        r.type = type;
        if (!handler) {
            r.status = IpcError::kInternal;
            r.error = std::string(action_name) + " handler not registered";
            return r;
        }
        std::string model_id;
        if (!param_str("model_id", &model_id)) {
            r.status = IpcError::kBadRequest;
            r.error = "params.model_id 必填（字符串）";
            return r;
        }
        if (!valid_model_id(model_id)) {
            r.status = IpcError::kBadRequest;
            r.error = "model_id 只允许字母/数字/下划线/连字符";
            return r;
        }
        std::string handler_error;
        if (!handler(model_id, &handler_error)) {
            r.status = IpcError::kBadRequest;
            r.error = handler_error.empty() ? (std::string(action_name) + " 失败") : handler_error;
            return r;
        }
        JsonValue data = JsonValue::object();
        data.set("model_id", JsonValue::string(model_id));
        data.set("action", JsonValue::string(action_name));
        r.status = IpcError::kOk;
        r.data = std::move(data);
        return r;
    };

    if (type == "MODEL_VALIDATE") {
        return handle_model_action("validate", model_validate_);
    }
    if (type == "MODEL_INSTALL") {
        return handle_model_action("install", model_install_);
    }
    if (type == "MODEL_ACTIVATE") {
        return handle_model_action("activate", model_activate_);
    }
    if (type == "MODEL_REMOVE") {
        return handle_model_action("remove", model_remove_);
    }

    resp.status = IpcError::kUnsupported;
    resp.error = "unsupported request type: " + type;
    return resp;
}

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------

bool ipc_request(const std::string& socket_path, const std::string& request_json,
                 std::string& response, int timeout_ms, std::string* error) {
    int fd = -1;
#if defined(_WIN32)
    fd = connect_tcp(socket_path, error);
#else
    fd = connect_unix(socket_path, error);
#endif
    if (fd < 0) return false;

#if defined(_WIN32)
    struct timeval tv {};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv {};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    std::string payload = request_json;
    if (payload.empty() || payload.back() != '\n') payload.push_back('\n');

    ssize_t sent = sock_send(fd, payload.data(), payload.size());
    if (sent <= 0) {
        if (error) *error = "发送请求失败";
        sock_close(fd);
        return false;
    }

    bool ok = false;
    response = read_line(fd, &ok);
    sock_close(fd);
    if (!ok) {
        if (error) *error = "读取响应失败（超时或连接关闭）";
        return false;
    }
    return true;
}

bool ipc_ping(const std::string& socket_path, std::string* error) {
    std::string resp_text;
    if (!ipc_request(socket_path, R"({"type":"PING"})", resp_text, 2000, error)) {
        return false;
    }
    JsonParseResult parsed = json_parse(resp_text);
    if (!parsed.ok || !parsed.value.is_object()) {
        if (error) *error = "PING 响应解析失败";
        return false;
    }
    const JsonValue* status_v = parsed.value.find("status");
    return status_v != nullptr && status_v->as_int() == static_cast<int64_t>(IpcError::kOk);
}

// ---------------------------------------------------------------------------
// SystemStatus -> JSON
// ---------------------------------------------------------------------------

JsonValue system_status_to_json(const SystemStatus& status) {
    JsonValue data = JsonValue::object();
    data.set("running", JsonValue::boolean(status.running));
    data.set("runtime_running", JsonValue::boolean(status.runtime_running));
    data.set("app_name", JsonValue::string(status.app_name));
    data.set("version", JsonValue::string(status.version));
    data.set("uptime_ms", JsonValue::number(status.uptime_ms));
    data.set("ipc_socket", JsonValue::string(status.ipc_socket));
    data.set("config_file", JsonValue::string(status.config_file));

    JsonValue m = JsonValue::object();
    m.set("fps", JsonValue::number(status.metrics.fps));
    m.set("capture_fps", JsonValue::number(status.metrics.capture_fps));
    m.set("infer_total", JsonValue::number(static_cast<double>(status.metrics.infer_total)));
    m.set("mouse_dx", JsonValue::number(static_cast<double>(status.metrics.mouse_dx)));
    m.set("mouse_dy", JsonValue::number(static_cast<double>(status.metrics.mouse_dy)));
    m.set("gated_frames", JsonValue::number(static_cast<double>(status.metrics.gated_frames)));
    m.set("target_frames", JsonValue::number(static_cast<double>(status.metrics.target_frames)));
    m.set("no_target_frames", JsonValue::number(static_cast<double>(status.metrics.no_target_frames)));
    m.set("aim_active", JsonValue::boolean(status.metrics.aim_active));
    m.set("injection_allowed", JsonValue::boolean(status.metrics.injection_allowed));
    m.set("mouse_control_connected", JsonValue::boolean(status.metrics.mouse_control_connected));
    m.set("mouse_control_socket_write_ok", JsonValue::number(static_cast<double>(status.metrics.mouse_control_socket_write_ok)));
    m.set("mouse_control_socket_write_fail", JsonValue::number(static_cast<double>(status.metrics.mouse_control_socket_write_fail)));
    m.set("mouse_control_send_count", JsonValue::number(static_cast<double>(status.metrics.mouse_control_send_count)));
    m.set("last_mouse_control_dx", JsonValue::number(static_cast<double>(status.metrics.last_mouse_control_dx)));
    m.set("last_mouse_control_dy", JsonValue::number(static_cast<double>(status.metrics.last_mouse_control_dy)));
    m.set("last_mouse_control_wheel", JsonValue::number(static_cast<double>(status.metrics.last_mouse_control_wheel)));
    m.set("last_mouse_control_timestamp_us", JsonValue::number(static_cast<double>(status.metrics.last_mouse_control_timestamp_us)));
    m.set("aim_error_x", JsonValue::number(status.metrics.aim_error_x));
    m.set("aim_error_y", JsonValue::number(status.metrics.aim_error_y));
    m.set("target_point_x", JsonValue::number(status.metrics.target_point_x));
    m.set("target_point_y", JsonValue::number(status.metrics.target_point_y));
    m.set("reference_x", JsonValue::number(status.metrics.reference_x));
    m.set("reference_y", JsonValue::number(status.metrics.reference_y));
    m.set("pid_output_x", JsonValue::number(status.metrics.pid_output_x));
    m.set("pid_output_y", JsonValue::number(status.metrics.pid_output_y));
    m.set("scheduler_input_x", JsonValue::number(status.metrics.scheduler_input_x));
    m.set("scheduler_input_y", JsonValue::number(status.metrics.scheduler_input_y));
    // 目标中心（标定状态机的真实目标位移数据源）
    m.set("aim_pos_x", JsonValue::number(status.metrics.aim_pos_x));
    m.set("aim_pos_y", JsonValue::number(status.metrics.aim_pos_y));
    m.set("aim_has_target", JsonValue::boolean(status.metrics.aim_has_target));
    m.set("aim_target_id", JsonValue::number(static_cast<double>(status.metrics.aim_target_id)));
    m.set("aim_target_class_id", JsonValue::number(static_cast<double>(status.metrics.aim_target_class_id)));
    m.set("aim_target_width", JsonValue::number(status.metrics.aim_target_width));
    m.set("aim_target_height", JsonValue::number(status.metrics.aim_target_height));
    m.set("aim_target_x1", JsonValue::number(status.metrics.aim_target_x1));
    m.set("aim_target_y1", JsonValue::number(status.metrics.aim_target_y1));
    m.set("aim_target_x2", JsonValue::number(status.metrics.aim_target_x2));
    m.set("aim_target_y2", JsonValue::number(status.metrics.aim_target_y2));
    JsonValue boxes = JsonValue::array();
    for (const auto& box : status.metrics.detection_boxes) {
        JsonValue item = JsonValue::object();
        item.set("x1", JsonValue::number(box.x1));
        item.set("y1", JsonValue::number(box.y1));
        item.set("x2", JsonValue::number(box.x2));
        item.set("y2", JsonValue::number(box.y2));
        item.set("score", JsonValue::number(box.score));
        item.set("class_id", JsonValue::number(static_cast<double>(box.class_id)));
        boxes.push_back(std::move(item));
    }
    m.set("detection_boxes", std::move(boxes));
    m.set("preview_fps", JsonValue::number(status.metrics.preview_fps));
    m.set("preview_encode_ms", JsonValue::number(status.metrics.preview_encode_ms));
    m.set("preview_width", JsonValue::number(static_cast<double>(status.metrics.preview_width)));
    m.set("preview_height", JsonValue::number(static_cast<double>(status.metrics.preview_height)));
    m.set("preview_bytes", JsonValue::number(static_cast<double>(status.metrics.preview_bytes)));
    m.set("preview_frames", JsonValue::number(static_cast<double>(status.metrics.preview_frames)));
    m.set("preview_dropped", JsonValue::number(static_cast<double>(status.metrics.preview_dropped)));
    m.set("buffer_age_ms", JsonValue::number(status.metrics.buffer_age_ms));
    m.set("last_dequeued_count", JsonValue::number(static_cast<double>(status.metrics.last_dequeued_count)));
    m.set("buffer_count", JsonValue::number(static_cast<double>(status.metrics.buffer_count)));
    m.set("input_width", JsonValue::number(static_cast<double>(status.metrics.input_width)));
    m.set("input_height", JsonValue::number(static_cast<double>(status.metrics.input_height)));
    m.set("capture_ms", JsonValue::number(status.metrics.capture_ms));
    m.set("resize_ms", JsonValue::number(status.metrics.resize_ms));
    m.set("infer_ms", JsonValue::number(status.metrics.infer_ms));
    m.set("infer_set_input_ms", JsonValue::number(status.metrics.infer_set_input_ms));
    m.set("infer_run_ms", JsonValue::number(status.metrics.infer_run_ms));
    m.set("infer_output_ms", JsonValue::number(status.metrics.infer_output_ms));
    m.set("decode_ms", JsonValue::number(status.metrics.decode_ms));
    m.set("aim_ms", JsonValue::number(status.metrics.aim_ms));
    m.set("e2e_ms", JsonValue::number(status.metrics.e2e_ms));
    // 分位数（真实样本，ms）
    m.set("e2e_p50_ms", JsonValue::number(status.metrics.e2e_p50_ms));
    m.set("e2e_p95_ms", JsonValue::number(status.metrics.e2e_p95_ms));
    m.set("e2e_p99_ms", JsonValue::number(status.metrics.e2e_p99_ms));
    m.set("e2e_max_ms", JsonValue::number(status.metrics.e2e_max_ms));
    m.set("infer_p50_ms", JsonValue::number(status.metrics.infer_p50_ms));
    m.set("infer_p95_ms", JsonValue::number(status.metrics.infer_p95_ms));
    m.set("infer_p99_ms", JsonValue::number(status.metrics.infer_p99_ms));
    m.set("decode_p50_ms", JsonValue::number(status.metrics.decode_p50_ms));
    m.set("decode_p95_ms", JsonValue::number(status.metrics.decode_p95_ms));
    m.set("decode_p99_ms", JsonValue::number(status.metrics.decode_p99_ms));
    m.set("detect_count", JsonValue::number(static_cast<double>(status.metrics.detect_count)));
    m.set("tracks", JsonValue::number(static_cast<double>(status.metrics.tracks)));
    m.set("dropped_frames", JsonValue::number(static_cast<double>(status.metrics.dropped_frames)));
    m.set("frames_total", JsonValue::number(static_cast<double>(status.metrics.frames_total)));
    data.set("metrics", std::move(m));
    return data;
}

}  // namespace ttbox::core
