// test_ipc_control.cpp — Phase 1 新增 IPC 消息验收：SET_CONFIG / RUNTIME_CONTROL。
//
// SET_CONFIG 原子序：params.profile 校验 → 通过回调更新 → 落盘。
//   - 合法 profile → status=0, applied=true
//   - 非法 profile（confidence 越界）→ status=1 + 明确 error，运行配置不被污染
//   - 缺 params.profile → status=1
//   - 未注册 handler → status=3
//   - 落盘失败 → 仍 applied=true 但 persisted=false（内存已生效）
//   - GET_CONFIG 兼容性：SET 后读回的是新配置
// RUNTIME_CONTROL：
//   - start/stop/restart → status=0 且 handler 收到正确 action
//   - 非法 action → status=1
//   - 未注册 handler → status=3
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "common/Json.hpp"
#include "ipc/IpcServer.hpp"
#include "model/RuntimeProfile.hpp"
#include "test_util.hpp"

namespace {

std::string tmp_socket_path2() {
#if defined(_WIN32)
    return "tcp:39129";
#else
    return "/tmp/ttbox_core_test_ctrl_" + std::to_string(static_cast<long>(::getpid())) + ".sock";
#endif
}

// Windows TIME_WAIT 下固定端口偶发 bind 失败：带重试的启动（总等待 ~1s）
static bool start_with_retry(ttbox::core::IpcServer& server, std::string* error = nullptr) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (server.start(tmp_socket_path2(), error)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

// 记录 handler 调用的轻量夹具：镜像 Application::handle_config_update 的真实原子序
// （from_json → validate → 收/拒），profile 只在完全通过后才写入 last（可见污染语义）。
struct ConfigFixture {
    
ttbox::core::JsonValue last = ttbox::core::JsonValue::object();
    bool reject = false;
    std::string reject_reason;
    bool persist_ok = true;
    bool called = false;
    bool validate_enabled = true;  // 打开后走真实 RuntimeProfile 校验（与产品一致）

    bool handle(const ttbox::core::JsonValue& profile, std::string* error, bool* persisted) {
        called = true;
        if (validate_enabled) {
            ttbox::core::RuntimeProfile rp = ttbox::core::RuntimeProfile::from_json(profile);
            std::string verr;
            if (!rp.validate(&verr)) {
                if (error) *error = verr.empty() ? "profile 校验失败" : ("profile 校验失败: " + verr);
                if (persisted) *persisted = false;
                return false;  // 校验失败：last 不被写入（不污染）
            }
        }
        if (reject) {
            if (error) *error = reject_reason;
            if (persisted) *persisted = false;
            return false;
        }
        last = profile;
        if (persisted) *persisted = persist_ok;
        return true;
    }
};

struct RuntimeFixture {
    std::vector<std::string> actions;
    bool ok = true;
    std::string fail_on;
    std::string fail_reason = "simulated start failure";

    bool handle(const std::string& action, std::string* error) {
        actions.push_back(action);
        if (!ok && action == fail_on) {
            if (error) *error = fail_reason;
            return false;
        }
        return true;
    }
};

ttbox::core::JsonValue make_profile(double confidence, bool enabled) {
    ttbox::core::JsonValue p = ttbox::core::JsonValue::object();
    p.set("model_id", ttbox::core::JsonValue::string(""));
    ttbox::core::JsonValue inf = ttbox::core::JsonValue::object();
    inf.set("confidence", ttbox::core::JsonValue::number(confidence));
    inf.set("iou", ttbox::core::JsonValue::number(0.45));
    inf.set("class_filter", ttbox::core::JsonValue::array());
    inf.set("max_detections", ttbox::core::JsonValue::number(20));
    p.set("inference", inf);
    ttbox::core::JsonValue m = ttbox::core::JsonValue::object();
    m.set("enabled", ttbox::core::JsonValue::boolean(enabled));
    m.set("aim_hotkey", ttbox::core::JsonValue::number(2));
    m.set("aim_hotkey2", ttbox::core::JsonValue::number(0));
    m.set("aim_hotkey_mode", ttbox::core::JsonValue::string("any"));
    p.set("mouse", m);
    ttbox::core::JsonValue fov = ttbox::core::JsonValue::object();
    fov.set("enabled", ttbox::core::JsonValue::boolean(false));
    fov.set("shape", ttbox::core::JsonValue::number(0));
    fov.set("radius", ttbox::core::JsonValue::number(0.5));
    fov.set("center_x", ttbox::core::JsonValue::number(0.5));
    fov.set("center_y", ttbox::core::JsonValue::number(0.5));
    p.set("fov", fov);
    return p;
}

std::string set_config_request(const std::string& profile_json) {
    return R"({"type":"SET_CONFIG","params":{"profile":)" + profile_json + R"(}})";
}

}  // namespace

TEST(ipc_set_config_ok) {
    ttbox::core::IpcServer server;
    ConfigFixture fx;
    server.set_config_update_handler(
        [&fx](const ttbox::core::JsonValue& p, std::string* e, bool* persisted) {
            return fx.handle(p, e, persisted);
        });
    std::string error;
    CHECK(start_with_retry(server, &error));

    // 合法 profile（confidence=0.3, enabled=true）
    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          set_config_request(make_profile(0.3, true).dump()), response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 0);
        const auto* data_v = parsed.value.find("data");
        CHECK(data_v != nullptr);
        if (data_v) {
            const auto* applied = data_v->find("applied");
            const auto* persisted = data_v->find("persisted");
            CHECK(applied != nullptr && applied->as_bool() == true);
            CHECK(persisted != nullptr && persisted->as_bool() == true);
        }
    }
    CHECK(fx.called);
    // handler 收到的 profile 内容与请求一致
    const auto* conf = fx.last.find("inference");
    CHECK(conf != nullptr);
    if (conf) {
        const auto* c = conf->find("confidence");
        CHECK(c != nullptr && c->as_number() == 0.3);
    }
    server.stop();
}

TEST(ipc_set_config_invalid_rejected_without_pollution) {
    ttbox::core::IpcServer server;
    ConfigFixture fx;
    server.set_config_update_handler(
        [&fx](const ttbox::core::JsonValue& p, std::string* e, bool* persisted) {
            return fx.handle(p, e, persisted);
        });
    std::string error;
    CHECK(start_with_retry(server, &error));

    // 非法 profile：confidence = 1.5 越界（RuntimeProfile::validate 会拒绝）
    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          set_config_request(make_profile(1.5, true).dump()), response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 1);  // BAD_REQUEST
        const auto* err_v = parsed.value.find("error");
        CHECK(err_v != nullptr && err_v->as_string().find("confidence") != std::string::npos);
    }
    CHECK(fx.called);                        // handler 被触达并拒绝
    CHECK(!fx.last.is_object() || fx.last.as_object().empty());  // 运行配置未被污染
    server.stop();
}

TEST(ipc_set_config_missing_profile_bad_request) {
    ttbox::core::IpcServer server;
    ConfigFixture fx;
    server.set_config_update_handler(
        [&fx](const ttbox::core::JsonValue& p, std::string* e, bool* persisted) {
            return fx.handle(p, e, persisted);
        });
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(), R"({"type":"SET_CONFIG"})",
          response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 1);
    }
    CHECK(!fx.called);  // 缺参时不应触达 handler
    server.stop();
}

TEST(ipc_set_config_no_handler_internal) {
    ttbox::core::IpcServer server;  // 不注册 handler
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          set_config_request(make_profile(0.3, true).dump()), response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 3);  // INTERNAL
    }
    server.stop();
}

TEST(ipc_set_config_persist_failure_still_applied) {
    ttbox::core::IpcServer server;
    ConfigFixture fx;
    fx.persist_ok = false;  // 模拟落盘失败
    server.set_config_update_handler(
        [&fx](const ttbox::core::JsonValue& p, std::string* e, bool* persisted) {
            return fx.handle(p, e, persisted);
        });
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          set_config_request(make_profile(0.3, true).dump()), response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 0);      // 内存已应用
        const auto* data_v = parsed.value.find("data");
        if (data_v) {
            const auto* persisted = data_v->find("persisted");
            CHECK(persisted != nullptr && persisted->as_bool() == false);  // 但未落盘
        }
    }
    server.stop();
}

TEST(ipc_set_config_get_config_roundtrip) {
    // 兼容性：SET_CONFIG 之后 GET_CONFIG 读回的是新配置（同一份 handler 存储）。
    ttbox::core::IpcServer server;
    ConfigFixture fx;
    server.set_config_update_handler(
        [&fx](const ttbox::core::JsonValue& p, std::string* e, bool* persisted) {
            return fx.handle(p, e, persisted);
        });
    server.set_config_provider([&fx] { return fx.last; });
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          set_config_request(make_profile(0.77, false).dump()), response, 2000, &error));
    // GET_CONFIG 读回
    CHECK(ttbox::core::ipc_request(server.socket_path(), R"({"type":"GET_CONFIG"})",
          response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* data_v = parsed.value.find("data");
        CHECK(data_v != nullptr);
        if (data_v) {
            const auto* inf = data_v->find("inference");
            CHECK(inf != nullptr);
            if (inf) {
                const auto* c = inf->find("confidence");
                CHECK(c != nullptr && c->as_number() == 0.77);
            }
        }
    }
    server.stop();
}

TEST(ipc_runtime_control_ok) {
    ttbox::core::IpcServer server;
    RuntimeFixture fx;
    server.set_runtime_control_handler(
        [&fx](const std::string& action, std::string* e) { return fx.handle(action, e); });
    std::string error;
    CHECK(start_with_retry(server, &error));

    for (const char* action : {"start", "stop", "restart"}) {
        std::string req = std::string(R"({"type":"RUNTIME_CONTROL","params":{"action":")") +
                          action + R"("}})";
        std::string response;
        CHECK(ttbox::core::ipc_request(server.socket_path(), req, response, 2000, &error));
        auto parsed = ttbox::core::json_parse(response);
        CHECK(parsed.ok);
        if (parsed.ok) {
            const auto* status_v = parsed.value.find("status");
            CHECK(status_v != nullptr && status_v->as_int() == 0);
            const auto* data_v = parsed.value.find("data");
            if (data_v) {
                const auto* a = data_v->find("action");
                CHECK(a != nullptr && a->as_string() == action);
            }
        }
    }
    CHECK_EQ(fx.actions.size(), static_cast<size_t>(3));
    server.stop();
}

TEST(ipc_runtime_control_invalid_action) {
    ttbox::core::IpcServer server;
    RuntimeFixture fx;
    server.set_runtime_control_handler(
        [&fx](const std::string& action, std::string* e) { return fx.handle(action, e); });
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          R"({"type":"RUNTIME_CONTROL","params":{"action":"explode"}})",
          response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 1);  // BAD_REQUEST
    }
    CHECK(fx.actions.empty());
    server.stop();
}

TEST(ipc_runtime_control_no_handler_internal) {
    ttbox::core::IpcServer server;  // 不注册 handler
    std::string error;
    CHECK(start_with_retry(server, &error));

    std::string response;
    CHECK(ttbox::core::ipc_request(server.socket_path(),
          R"({"type":"RUNTIME_CONTROL","params":{"action":"start"}})",
          response, 2000, &error));
    auto parsed = ttbox::core::json_parse(response);
    CHECK(parsed.ok);
    if (parsed.ok) {
        const auto* status_v = parsed.value.find("status");
        CHECK(status_v != nullptr && status_v->as_int() == 3);
    }
    server.stop();
}
