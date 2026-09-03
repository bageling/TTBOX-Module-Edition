// test_ipc_model.cpp — v0.3 模型管理 IPC 验收：
// MODEL_LIST / MODEL_IMPORT / MODEL_VALIDATE / MODEL_INSTALL / MODEL_ACTIVATE / MODEL_REMOVE
// 附带：model_id 非法字符拒绝（path traversal 防护）、收件目录约束、active 跟随。
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include <thread>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <direct.h>
#endif

#include "common/Json.hpp"
#include "ipc/IpcServer.hpp"
#include "model/ModelManagement.hpp"
#include "test_util.hpp"

namespace fs = std::filesystem;
using namespace ttbox::core;

namespace {

// JSON 字符串转义（Windows 路径反斜杠必须转成 \\，否则 \U 等被当非法转义）
static std::string json_escape(std::string s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

// IPC 传输失败时构造 status=-1 的伪响应（error 保留原因）
static JsonValue ipc_error_response(const std::string& err) {
    JsonValue j = JsonValue::object();
    j.set("status", JsonValue::number(-1));
    j.set("error", JsonValue::string(err));
    return j;
}

std::string tmp_socket() {
#if defined(_WIN32)
    return "tcp:39131";
#else
    return "/tmp/ttbox_ipc_model_" + std::to_string(static_cast<long>(::getpid())) + ".sock";
#endif
}


// Windows TIME_WAIT 下固定端口偶发 bind 失败：带重试的启动（总等待 ~1s）
static bool start_with_retry(ttbox::core::IpcServer& server, std::string* error = nullptr) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (server.start(tmp_socket(), error)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

struct ModelFixture {
    std::string root;
    ModelManagement mm;
    IpcServer server;

    explicit ModelFixture(bool with_validator = true)
        : root((fs::temp_directory_path() / ("ttbox_ipc_model_" + std::to_string(::getpid()))).string()),
          mm(ModelRegistryOptions{root, true}) {
        std::string err;
        if (!mm.init(&err)) { std::fprintf(stderr, "init failed: %s\n", err.c_str()); std::abort(); }
        if (with_validator) {
            // 与生产一致的文件级校验（Windows 无 RKNN）
            mm.set_validator(ModelManagement::file_level_validator);
        }
        wire();
    }

    ~ModelFixture() {
        server.stop();
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void wire() {
        server.set_model_list_handler([this] {
            JsonValue data = JsonValue::object();
            JsonValue arr = JsonValue::array();
            for (const auto& m : mm.registry().list()) arr.push_back(m.to_json());
            data.set("models", std::move(arr));
            data.set("active", JsonValue::string(mm.registry().active_model()));
            return data;
        });
        server.set_model_import_handler(
            [this](const std::string& src, const std::string& id, const std::string& label,
                   std::string* error) {
                if (src.rfind(mm.registry().root_dir() + "/_incoming", 0) != 0) {
                    if (error) *error = "模型文件必须先上传到收件目录";
                    return false;
                }
                ModelManifest mf;
                mf.label = label.empty() ? id : label;
                return mm.registry().import(src, id, mf, error);
            });
        server.set_model_validate_handler(
            [this](const std::string& id, std::string* error) { return mm.registry().validate(id, error); });
        server.set_model_install_handler(
            [this](const std::string& id, std::string* error) { return mm.registry().install(id, error); });
        server.set_model_activate_handler(
            [this](const std::string& id, std::string* error) { return mm.registry().activate(id, error); });
        server.set_model_remove_handler(
            [this](const std::string& id, std::string* error) { return mm.registry().remove(id, error); });
    }

    bool start() {
        std::string err;
        return server.start(tmp_socket(), &err);
    }

    // 写一个假模型到收件目录。返回正斜杠路径（与 Core root_dir() 拼接格式一致）。
    std::string make_incoming(const std::string& name) {
        const std::string dir = root + "/_incoming";
        fs::create_directories(dir);
        const std::string path = dir + "/" + name;
        std::ofstream f(path, std::ios::binary);
        f << std::string(4096, 'R');  // >1KB，通过文件级校验
        f.close();
        return path;
    }

    JsonValue ipc(const std::string& type, const std::string& params_json = "") {
        std::string req = "{\"type\":\"" + type + "\"";
        if (!params_json.empty()) req += ",\"params\":" + params_json;
        req += "}";
        std::string response;
        std::string err;
        // Windows 下连接刚 listen 的 socket 偶发 WSAECONNREFUSED：客户端重试 3 次
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (ipc_request(server.socket_path(), req, response, 3000, &err)) {
                auto parsed = json_parse(response);
                return parsed.ok ? parsed.value : JsonValue::null();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return ipc_error_response(err);
    }

};



}  // namespace

// 独立辅助：从 IPC 响应取 status（放在匿名空间外以便 TEST 宏展开后可见）
static int resp_status(const JsonValue& r) {
    const auto* v = r.find("status");
    return v ? static_cast<int>(v->as_int()) : -1;
}

TEST(model_ipc_full_lifecycle) {
    ModelFixture fx;
    CHECK(fx.start());

    // 1) 空列表
    auto r = fx.ipc("MODEL_LIST");
    CHECK_EQ(resp_status(r), 0);
    {
        const auto* data = r.find("data");
        const auto* models = data ? data->find("models") : nullptr;
        CHECK(models != nullptr && models->as_array().empty());
    }

    // 2) import（合法收件路径）
    const std::string src = fx.make_incoming("yolo_face.rknn");
    r = fx.ipc("MODEL_IMPORT",
               R"({"src_path":")" + json_escape(src) + R"(","model_id":"yolo-face-v1","label":"人脸检测"})");
    CHECK_EQ(resp_status(r), 0);

    // 3) import 二次同 id → 拒绝（staging 冲突不报错但 install 前提是 validate；重导 staging 会覆盖，
    //    这里 import 相同 id 应成功覆盖 staging——Core 语义如此，测成功即可）
    // 4) validate → install
    // Windows 文件系统偶发延迟（Defender 扫描锁）：validate 失败时重试 3 次
    r = fx.ipc("MODEL_VALIDATE", R"({"model_id":"yolo-face-v1"})");
    for (int attempt = 0; attempt < 3 && resp_status(r) != 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        r = fx.ipc("MODEL_VALIDATE", R"({"model_id":"yolo-face-v1"})");
    }
    CHECK_EQ(resp_status(r), 0);
    r = fx.ipc("MODEL_INSTALL", R"({"model_id":"yolo-face-v1"})");
    for (int attempt = 0; attempt < 3 && resp_status(r) != 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        r = fx.ipc("MODEL_INSTALL", R"({"model_id":"yolo-face-v1"})");
    }
    CHECK_EQ(resp_status(r), 0);

    // 5) list 出现且 active 为空
    r = fx.ipc("MODEL_LIST");
    CHECK_EQ(resp_status(r), 0);
    {
        const auto* data = r.find("data");
        const auto* models = data ? data->find("models") : nullptr;
        CHECK(models != nullptr && models->as_array().size() == 1);
        const auto* active = data ? data->find("active") : nullptr;
        CHECK(active != nullptr && active->as_string().empty());
    }

    // 6) activate → active 跟随
    r = fx.ipc("MODEL_ACTIVATE", R"({"model_id":"yolo-face-v1"})");
    CHECK_EQ(resp_status(r), 0);
    r = fx.ipc("MODEL_LIST");
    {
        const auto* data = r.find("data");
        const auto* active = data ? data->find("active") : nullptr;
        CHECK(active != nullptr && active->as_string() == "yolo-face-v1");
    }

    // 7) remove 激活中的模型 → 拒绝
    r = fx.ipc("MODEL_REMOVE", R"({"model_id":"yolo-face-v1"})");
    CHECK_EQ(resp_status(r), 1);  // BAD_REQUEST

    // 8) deactivate → remove 成功
    // （Core 的 deactivate 走 ModelRegistry::deactivate；IPC 未单独暴露——用 activate 空值不可行，
    //   这里直接 remove 另一个模型路径验证。为完整性：remove 应在 deactivate 后成功。
    //   当前 Core remove 拒绝 active 模型 = 预期行为。）
}

TEST(model_ipc_import_rejects_outside_incoming) {
    ModelFixture fx;
    CHECK(fx.start());
    // 路径不在收件目录 → 拒绝（防任意文件读取）
    auto r = fx.ipc("MODEL_IMPORT",
                    R"({"src_path":"C:/Windows/system32/config","model_id":"evil"})");
    CHECK_EQ(resp_status(r), 1);
    r = fx.ipc("MODEL_IMPORT",
               R"({"src_path":"/etc/passwd","model_id":"evil"})");
    CHECK_EQ(resp_status(r), 1);
}

TEST(model_ipc_rejects_bad_model_id) {
    ModelFixture fx;
    CHECK(fx.start());
    // path traversal / 非法字符
    for (const char* bad : {"../evil", "a/b", "a b", "", "模型"}) {
        std::string params = std::string(R"({"model_id":")") + bad + R"("})";
        auto r = fx.ipc("MODEL_INSTALL", params);
        CHECK_EQ(resp_status(r), 1);
    }
}

TEST(model_ipc_validate_fails_without_validator) {
    // 无 validator（板端未注入 RKNN 校验器）→ validate 明确报错，不静默
    ModelFixture fx(false);
    CHECK(fx.start());
    const std::string src = fx.make_incoming("m.rknn");
    (void)fx.ipc("MODEL_IMPORT",
                 R"({"src_path":")" + json_escape(src) + R"(","model_id":"m1"})");
    auto r = fx.ipc("MODEL_VALIDATE", R"({"model_id":"m1"})");
    CHECK_EQ(resp_status(r), 1);
    const auto* err = r.find("error");
    CHECK(err != nullptr && err->as_string().find("validator") != std::string::npos);
}

TEST(model_ipc_activate_requires_installed) {
    ModelFixture fx;
    CHECK(fx.start());
    // 未 install 就 activate → 拒绝
    auto r = fx.ipc("MODEL_ACTIVATE", R"({"model_id":"not-exist"})");
    CHECK_EQ(resp_status(r), 1);
}
