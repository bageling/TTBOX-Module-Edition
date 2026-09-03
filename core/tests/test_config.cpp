// test_config.cpp — ConfigManager：真实 default.json / 缺失文件 / 坏 JSON
#include <cstdio>
#include <fstream>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "config/ConfigManager.hpp"
#include "test_util.hpp"

// TTBOX_PROJECT_ROOT 由 CMake 注入（<root>/ttbox/core）
#ifndef TTBOX_PROJECT_ROOT
#define TTBOX_PROJECT_ROOT "."
#endif

namespace {

long getpid_like() {
#if defined(_WIN32)
    return 12345L;
#else
    return static_cast<long>(::getpid());
#endif
}

std::string real_config_path() {
    return std::string(TTBOX_PROJECT_ROOT) + "/config/default.json";
}

std::string tmp_bad_json_path() {
    return std::string("/tmp/ttbox_core_test_bad_") + std::to_string(getpid_like()) + ".json";
}

}  // namespace

TEST(config_loads_existing_default_json) {
    ttbox::core::ConfigManager cfg;
    std::string error;
    bool ok = cfg.load(real_config_path(), &error);
    CHECK(ok);
    if (ok) {
        CHECK(cfg.loaded());
        // conf 是用户可配置项（A-7），default.json 提供运行时默认值；断言能读到有效正数
        CHECK(cfg.get_double("conf", -1.0) > 0.0);
        CHECK(cfg.get_int("model_input_width", 0) == 640);
        CHECK(cfg.get_int("model_input_height", 0) == 640);
        CHECK(cfg.get_string("aim_keys_text", "") == "KEY_LEFTSHIFT,KEY_RIGHTSHIFT");
        CHECK(!cfg.flatten().empty());
    } else {
        std::printf("  [info] 配置加载失败: %s\n", error.c_str());
    }
}

TEST(config_missing_file_errors_explicitly) {
    ttbox::core::ConfigManager cfg;
    std::string error;
    bool ok = cfg.load("/tmp/definitely_not_exists_ttbox.json", &error);
    CHECK(!ok);
    CHECK(!cfg.loaded());
    CHECK(!error.empty());  // 明确错误，不允许 silent fallback
}

TEST(config_bad_json_errors_explicitly) {
    const std::string path = tmp_bad_json_path();
    {
        std::ofstream f(path, std::ios::trunc);
        f << "{ \"conf\": 0.25, \"broken\" ";  // 语法错误（未闭合）
    }
    ttbox::core::ConfigManager cfg;
    std::string error;
    bool ok = cfg.load(path, &error);
    CHECK(!ok);
    CHECK(!cfg.loaded());
    CHECK(!error.empty());
    CHECK(error.find("JSON") != std::string::npos);  // 明确提示 JSON 错误
    std::remove(path.c_str());
}

TEST(config_non_object_root_errors) {
    const std::string path = tmp_bad_json_path();
    {
        std::ofstream f(path, std::ios::trunc);
        f << "[1,2,3]";
    }
    ttbox::core::ConfigManager cfg;
    std::string error;
    bool ok = cfg.load(path, &error);
    CHECK(!ok);
    CHECK(error.find("对象") != std::string::npos);
    std::remove(path.c_str());
}
