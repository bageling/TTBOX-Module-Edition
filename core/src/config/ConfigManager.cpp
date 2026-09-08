// ConfigManager.cpp — 配置读取实现
/*
 * TTBOX 文件说明
 *
 * 文件：ConfigManager.cpp
 *
 * 作用：
 *   读取和管理 JSON 格式的配置文件。
 *
 * 小白理解：
 *   TTBOX 的所有参数都存在 JSON 文件里。
 *   这个模块负责读这些文件，并把参数分发给各个模块。
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#include "config/ConfigManager.hpp"

#include <sstream>

namespace ttbox::core {

bool ConfigManager::load(const std::string& path, std::string* error) {
    auto set_error = [error](const std::string& msg) {
        if (error) *error = msg;
    };

    JsonParseResult parsed = json_parse_file(path);
    if (!parsed.ok) {
        set_error("配置加载失败: " + parsed.error);
        loaded_ = false;
        return false;
    }
    if (!parsed.value.is_object()) {
        set_error("配置加载失败: 根节点必须是 JSON 对象 (" + path + ")");
        loaded_ = false;
        return false;
    }

    root_ = std::move(parsed.value);
    path_ = path;
    loaded_ = true;
    return true;
}

std::string ConfigManager::get_string(const std::string& key, const std::string& def) const {
    const JsonValue* v = root_.find(key);
    return v ? v->as_string(def) : def;
}

double ConfigManager::get_double(const std::string& key, double def) const {
    const JsonValue* v = root_.find(key);
    return v ? v->as_number(def) : def;
}

int64_t ConfigManager::get_int(const std::string& key, int64_t def) const {
    const JsonValue* v = root_.find(key);
    return v ? v->as_int(def) : def;
}

bool ConfigManager::get_bool(const std::string& key, bool def) const {
    const JsonValue* v = root_.find(key);
    return v ? v->as_bool(def) : def;
}

std::vector<std::pair<std::string, std::string>> ConfigManager::flatten() const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& [key, value] : root_.as_object()) {
        switch (value.type()) {
            case JsonType::kString:
                out.emplace_back(key, value.as_string());
                break;
            case JsonType::kNumber: {
                std::ostringstream ss;
                ss << value.as_number();
                out.emplace_back(key, ss.str());
                break;
            }
            case JsonType::kBool:
                out.emplace_back(key, value.as_bool() ? "true" : "false");
                break;
            case JsonType::kNull:
                out.emplace_back(key, "null");
                break;
            case JsonType::kArray:
            case JsonType::kObject:
                out.emplace_back(key, value.dump());  // 嵌套结构按紧凑 JSON 输出
                break;
        }
    }
    return out;
}

}  // namespace ttbox::core
