// test_ipc.cpp — IPC 服务端：PING / GET_STATUS / GET_CONFIG / 错误处理
#include <chrono>
#include <string>
#include <thread>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "common/Json.hpp"
#include "common/Metrics.hpp"
#include "ipc/IpcServer.hpp"
#include "test_util.hpp"

namespace {

std::string tmp_socket_path() {
#if defined(_WIN32)
    return "tcp:39128";
#else
    return "/tmp/ttbox_core_test_" + std::to_string(static_cast<long>(::getpid())) + ".sock";
#endif
}

// Windows TIME_WAIT 下固定端口偶发 bind 失败：带重试的启动（总等待 ~1s）
static bool start_with_retry(ttbox::core::IpcServer& server, std::string* error = nullptr) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (server.start(tmp_socket_path(), error)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

ttbox::core::SystemStatus test_status() {
    ttbox::core::SystemStatus st;
    st.running = true;
    st.app_name = "test";
    st.version = "0.0.0";
    st.uptime_ms = 12.5;
    st.ipc_socket = "test.sock";
    st.config_file = "test.json";
    return st;
}

ttbox::core::JsonValue test_config_json() {
    ttbox::core::JsonValue cfg = ttbox::core::JsonValue::object();
    cfg.set("conf", ttbox::core::JsonValue::string("0.25"));
    cfg.set("nms", ttbox::core::JsonValue::string("0.45"));
    return cfg;
}

}  // namespace

TEST(ipc_ping_roundtrip) {
    ttbox::core::IpcServer server;
    std::string error;
    CHECK(start_with_retry(server, &error));
    if (!server.running()) {
        std::printf("  [info] server start failed: %s\n", error.c_str());
        return;
    }

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(), R"({"type":"PING"})", response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 0);
        const auto* data_v = parsed.value.find("data");
        CHECK(data_v != nullptr);
        if (data_v != nullptr) {
            const auto* pong_v = data_v->find("pong");
            CHECK(pong_v != nullptr && pong_v->as_bool() == true);
        }
    }
    server.stop();
}

TEST(ipc_get_status) {
    ttbox::core::IpcServer server;
    server.set_status_provider(test_status);
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(), R"({"type":"GET_STATUS"})", response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 0);
        const auto* data_v = parsed.value.find("data");
        CHECK(data_v != nullptr);
        if (data_v != nullptr) {
            const auto* running_v = data_v->find("running");
            CHECK(running_v != nullptr && running_v->as_bool() == true);
            const auto* version_v = data_v->find("version");
            CHECK(version_v != nullptr && version_v->as_string() == "0.0.0");
        }
    }
    server.stop();
}

TEST(ipc_get_config) {
    ttbox::core::IpcServer server;
    server.set_config_provider(test_config_json);
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(), R"({"type":"GET_CONFIG"})", response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 0);
        const auto* data_v = parsed.value.find("data");
        CHECK(data_v != nullptr && data_v->is_object());
        if (data_v != nullptr && data_v->is_object()) {
            const auto* conf_v = data_v->find("conf");
            CHECK(conf_v != nullptr && conf_v->as_string() == "0.25");
        }
    }
    server.stop();
}

TEST(ipc_unsupported_type_error) {
    ttbox::core::IpcServer server;
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(), R"({"type":"DO_NOTHING"})", response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        // 错误码 4 = UNSUPPORTED
        CHECK(status_v != nullptr && status_v->as_int() == 4);
    }
    server.stop();
}

TEST(ipc_bad_json_error) {
    ttbox::core::IpcServer server;
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(), "{ not json", response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        // 错误码 1 = BAD_REQUEST
        CHECK(status_v != nullptr && status_v->as_int() == 1);
    }
    server.stop();
}

TEST(ipc_client_connection_refused) {
    std::string response;
    std::string error;
    bool ok = ttbox::core::ipc_request("/tmp/nonexistent_ttbox_core.sock",
                                       R"({"type":"PING"})", response, 500, &error);
    CHECK(!ok);  // 连接被拒 → 明确失败
}
