// MouseProxyMode.hpp — A10 鼠标代理模式
//
// full_passthrough：真实物理鼠标继续透传，同时允许 AI 注入（V1 唯一实现）。
// synthetic：仅保留接口与配置，V1 不实现合成 HID 后端。
#pragma once

#include <string>

namespace ttbox::core::aim {

enum class MouseProxyMode : int {
    kFullPassthrough = 0,  // 完整透传（V1 实现）
    kSynthetic = 1,        // 合成模式（V1 仅接口预留）
};

inline const char* mouse_proxy_mode_name(MouseProxyMode m) {
    return m == MouseProxyMode::kSynthetic ? "synthetic" : "full_passthrough";
}

// 解析字符串（容错：未知 → full_passthrough）
inline MouseProxyMode mouse_proxy_mode_from_string(const std::string& s) {
    if (s == "synthetic") return MouseProxyMode::kSynthetic;
    return MouseProxyMode::kFullPassthrough;
}

}  // namespace ttbox::core::aim
