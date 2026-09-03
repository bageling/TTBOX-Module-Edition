// ConfigManager.hpp — 运行时配置读取（阶段 A-1：只做最基础读取）
//
// 约定（与现有 Python 配置兼容，不重新设计格式）：
//   - 读取 ttbox2/config/default.json（路径可由调用方/CLI 指定）
//   - 配置文件不存在 -> 明确错误，不允许 silent fallback
//   - JSON 解析失败 -> 明确错误（含位置信息），不允许静默忽略
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "common/Json.hpp"

namespace ttbox::core {

class ConfigManager {
public:
    // 加载配置文件。失败返回 false 并给出明确错误（路径/解析/结构）。
    bool load(const std::string& path, std::string* error = nullptr);

    bool loaded() const { return loaded_; }
    const std::string& path() const { return path_; }

    // 结构化读取：key 不存在时返回默认值（调用方显式给默认值，非静默兜底）
    std::string get_string(const std::string& key, const std::string& def = "") const;
    double get_double(const std::string& key, double def = 0.0) const;
    int64_t get_int(const std::string& key, int64_t def = 0) const;
    bool get_bool(const std::string& key, bool def = false) const;

    // 扁平化为字符串键值（供 IPC GET_CONFIG 输出）
    std::vector<std::pair<std::string, std::string>> flatten() const;

    const JsonValue& root() const { return root_; }

private:
    JsonValue root_ = JsonValue::object();
    bool loaded_ = false;
    std::string path_;
};

}  // namespace ttbox::core
