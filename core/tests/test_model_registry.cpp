// test_model_registry.cpp — A-8 单元测试：ModelRegistry 仓库生命周期
//
// 使用临时目录 + 模拟 validator（不依赖真模型/NPU），验证：
//   import → validate → install → activate → deactivate → remove
//   禁止删除 active 模型、激活失败恢复、quarantine
#include "test_util.hpp"

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "model/IModelSource.hpp"
#include "model/ModelRegistry.hpp"

namespace fs = std::filesystem;

using namespace ttbox::core;

namespace {

std::string g_tmp_root;

// 模拟 validator：检查文件存在则返回 metadata（模拟 RKNN+Adapter 校验）
bool fake_validator(const std::string& rknn_path, JsonValue* meta_out, std::string* err) {
    if (!fs::exists(rknn_path)) {
        if (err) *err = "模型文件不存在";
        return false;
    }
    JsonValue m = JsonValue::object();
    m.set("input_width", JsonValue::number(320));
    m.set("input_height", JsonValue::number(320));
    m.set("decode_type", JsonValue::number(2));  // dfl
    m.set("class_count", JsonValue::number(2));
    if (meta_out) *meta_out = m;
    return true;
}

std::string make_fake_model(const std::string& path) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f << "FAKE-RKNN-DATA-0123456789";
    f.close();
    return path;
}

std::string unique_root() {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/tmp/ttbox_reg_%d", static_cast<int>(::getpid()));
    return buf;
}

struct RegistryFixture {
    std::string root;
    ModelRegistry reg;
    explicit RegistryFixture(bool init = true)
        : root(unique_root()), reg(ModelRegistryOptions{root}) {
        std::string err;
        if (init) reg.init(&err);
    }
    ~RegistryFixture() { std::error_code ec; fs::remove_all(root, ec); }
};

}  // namespace

TEST(registry_init_creates_dirs) {
    RegistryFixture fx;
    CHECK(fs::is_directory(fx.root + "/registry"));
    CHECK(fs::is_directory(fx.root + "/installed"));
    CHECK(fs::is_directory(fx.root + "/staging"));
    CHECK(fs::is_directory(fx.root + "/cache"));
    CHECK(fs::is_directory(fx.root + "/quarantine"));
}

TEST(registry_import_validate_install_activate) {
    RegistryFixture fx;
    fx.reg.set_validator(fake_validator);
    const std::string fake = make_fake_model("/tmp/ttbox_fake_src.rknn");

    ModelManifest m;
    m.label = "huangwa";
    m.origin = "local";
    std::string err;
    CHECK(fx.reg.import(fake, "hw320", m, &err));
    CHECK(fs::exists(fx.root + "/staging/hw320/model.rknn"));
    CHECK(fs::exists(fx.root + "/staging/hw320/manifest.json"));

    // 未验证不允许 install
    CHECK(!fx.reg.install("hw320", &err));
    CHECK(!err.empty());

    // 验证 → install
    CHECK(fx.reg.validate("hw320", &err));
    CHECK(fs::exists(fx.root + "/staging/hw320/validation/ok.json"));
    CHECK(fx.reg.install("hw320", &err));
    CHECK(fs::exists(fx.root + "/installed/hw320/model.rknn"));
    CHECK(fs::exists(fx.root + "/installed/hw320/manifest.json"));

    // list 可见
    auto models = fx.reg.list();
    CHECK_EQ(models.size(), 1u);
    if (models.size() == 1) {
        CHECK(models[0].model_id == "hw320");
        CHECK(models[0].status == ModelStatus::kInstalled);
    }

    // 激活
    CHECK(fx.reg.activate("hw320", &err));
    CHECK(fx.reg.active_model() == "hw320");

    // active 模型禁止删除
    CHECK(!fx.reg.remove("hw320", &err));
    CHECK(!err.empty());
    CHECK(fs::exists(fx.root + "/installed/hw320"));

    // 停用后可删除
    CHECK(fx.reg.deactivate(&err));
    CHECK(fx.reg.active_model().empty());
    CHECK(fx.reg.remove("hw320", &err));
    CHECK(!fs::exists(fx.root + "/installed/hw320"));
    CHECK(fx.reg.list().empty());
    std::remove("/tmp/ttbox_fake_src.rknn");
}

TEST(registry_activate_failure_restores_old) {
    RegistryFixture fx;
    // validator：staging 校验（validate）始终通过；installed 中 bad 模型激活时校验失败
    fx.reg.set_validator([](const std::string& rknn, JsonValue* m, std::string* err) {
        if (rknn.find("/installed/bad") != std::string::npos) {
            if (err) *err = "校验失败: 模型损坏";
            return false;
        }
        JsonValue mm = JsonValue::object();
        mm.set("ok", JsonValue::number(1));
        if (m) *m = mm;
        return true;
    });
    const std::string f1 = make_fake_model("/tmp/ttbox_f1.rknn");
    const std::string f2 = make_fake_model("/tmp/ttbox_f2.rknn");
    std::string err;
    ModelManifest m;
    CHECK(fx.reg.import(f1, "good", m, &err));
    CHECK(fx.reg.validate("good", &err));
    CHECK(fx.reg.install("good", &err));
    CHECK(fx.reg.activate("good", &err));
    CHECK(fx.reg.active_model() == "good");

    // 导入 bad 并安装（staging 校验通过）
    CHECK(fx.reg.import(f2, "bad", m, &err));
    CHECK(fx.reg.validate("bad", &err));
    CHECK(fx.reg.install("bad", &err));
    // 激活 bad → installed 校验失败 → active 保持 good
    CHECK(!fx.reg.activate("bad", &err));
    CHECK(fx.reg.active_model() == "good");
    std::remove("/tmp/ttbox_f1.rknn");
    std::remove("/tmp/ttbox_f2.rknn");
}

TEST(registry_quarantine_failed_model) {
    RegistryFixture fx;
    fx.reg.set_validator([](const std::string&, JsonValue*, std::string* err) {
        if (err) *err = "不支持的算子";
        return false;
    });
    const std::string fake = make_fake_model("/tmp/ttbox_fake_bad.rknn");
    std::string err;
    ModelManifest m;
    CHECK(fx.reg.import(fake, "badmodel", m, &err));
    CHECK(!fx.reg.validate("badmodel", &err));
    CHECK(!err.empty());
    // 验证失败 → 移入 quarantine
    CHECK(fx.reg.quarantine("badmodel", err, &err));
    CHECK(fs::exists(fx.root + "/quarantine/badmodel/reason.json"));
    CHECK(fx.reg.list().empty());
    std::remove("/tmp/ttbox_fake_bad.rknn");
}

TEST(registry_local_file_source) {
    const std::string src = "/tmp/ttbox_src_model.rknn";
    make_fake_model(src);
    LocalFileSource lfs(src);
    CHECK(lfs.source_id().find("local:") == 0);
    const std::string dst = "/tmp/ttbox_dst_model.rknn";
    std::string err;
    CHECK(lfs.fetch_to(dst, &err));
    CHECK(fs::exists(dst));
    CHECK(fs::file_size(dst) > 0);

    CloudModelSource cms("cloud-m1");
    CHECK(!cms.fetch_to("/tmp/x.rknn", &err));  // 预留：未实现
    CHECK(!err.empty());
    std::remove(src.c_str());
    std::remove(dst.c_str());
    std::remove("/tmp/x.rknn");
}
