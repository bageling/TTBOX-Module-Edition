// ipc_ping.cpp — IPC 命令行测试工具
//
// 用法: ipc_ping [--socket <path>] [--type PING|GET_STATUS|GET_CONFIG] [--json <raw>]
// 默认: PING /tmp/ttbox_core.sock
#include <cstdio>
#include <string>

#include "common/Logger.hpp"
#include "ipc/IpcServer.hpp"

int main(int argc, char** argv) {
    std::string socket_path = "/tmp/ttbox_core.sock";
    std::string type = "PING";
    std::string raw_json;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "缺少参数值: %s\n", name);
                std::exit(1);
            }
            return argv[++i];
        };
        if (arg == "--socket") {
            socket_path = next("--socket");
        } else if (arg == "--type") {
            type = next("--type");
        } else if (arg == "--json") {
            raw_json = next("--json");
        } else {
            std::fprintf(stderr, "未知参数: %s\n", arg.c_str());
            return 1;
        }
    }

    std::string request;
    if (!raw_json.empty()) {
        request = raw_json;
    } else {
        if (type == "PING") {
            request = R"({"type":"PING"})";
        } else if (type == "GET_STATUS") {
            request = R"({"type":"GET_STATUS"})";
        } else if (type == "GET_CONFIG") {
            request = R"({"type":"GET_CONFIG"})";
        } else {
            std::fprintf(stderr, "未知 --type: %s\n", type.c_str());
            return 1;
        }
    }

    std::string response;
    std::string error;
    if (!ttbox::core::ipc_request(socket_path, request, response, 2000, &error)) {
        std::fprintf(stderr, "IPC 请求失败: %s\n", error.c_str());
        return 1;
    }
    std::printf("%s", response.c_str());
    return 0;
}
