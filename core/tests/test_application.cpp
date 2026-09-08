// test_application.cpp — Application：startup / run / clean shutdown
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "app/Application.hpp"
#include "common/Logger.hpp"
#include "test_util.hpp"

// TTBOX_PROJECT_ROOT 由 CMake 注入（<root>/ttbox/core）
#ifndef TTBOX_PROJECT_ROOT
#define TTBOX_PROJECT_ROOT "."
#endif

namespace {

std::string real_config_path() {
    return std::string(TTBOX_PROJECT_ROOT) + "/config/default.json";
}

std::string tmp_socket_path() {
#if defined(_WIN32)
    return "tcp:127.0.0.1:39127";
#else
    return "/tmp/ttbox_core_app_test_" + std::to_string(static_cast<long>(::getpid())) + ".sock";
#endif
}

}  // namespace

TEST(application_startup_run_clean_shutdown) {
    // 复位 Logger sink（单例可能已被前序测试添加），避免重复输出
    ttbox::core::Logger::instance().clear_sinks();

    // 复位全局 shutdown 标志（Application 内部）
    ttbox::core::Application app;

    std::string cfg = "--config";
    std::string cfg_path = real_config_path();
    std::string ipc = "--ipc";
    std::string ipc_path = tmp_socket_path();
    char* argv[] = {const_cast<char*>("ttbox_core"), cfg.data(), cfg_path.data(),
                    ipc.data(), ipc_path.data(), nullptr};
    int rc = app.initialize(5, argv);
    CHECK_EQ(rc, 0);
    if (rc != 0) {
        return;
    }

    // run() 在独立线程运行，主线程请求退出
    std::thread runner([&app] { app.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(app.running());
    ttbox::core::Application::request_shutdown();
    runner.join();
    app.shutdown();
    CHECK(!app.running());

    // IPC 也应已关闭（socket 文件已删除）
    std::string response;
    std::string error;
    bool ok = ttbox::core::ipc_request(ipc_path, R"({"type":"PING"})", response, 500, &error);
    CHECK(!ok);
}

TEST(application_initialize_missing_config_fails) {
    ttbox::core::Logger::instance().clear_sinks();
    ttbox::core::Application app;
    std::string cfg = "--config";
    std::string bad_path = "/tmp/not_exist_ttbox_config.json";
    char* argv[] = {const_cast<char*>("ttbox_core"), cfg.data(), bad_path.data(), nullptr};
    int rc = app.initialize(3, argv);
    CHECK_NE(rc, 0);  // 配置缺失必须明确失败，不允许 silent fallback
}
