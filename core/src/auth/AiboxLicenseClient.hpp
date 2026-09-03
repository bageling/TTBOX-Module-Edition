#pragma once

#include <string>
#include "auth/LicenseDaemon.hpp"

namespace ttbox::core::auth {

// 完全复刻原 aibox-bl 授权协议：
//  - isPro=true  → POST https://blpro.antszy.com:443/api/v1/verifyCard
//  - isPro=false → POST https://blpt.antszy.com:443/api/v1/verifyCard
//  - user-data 访问 https://data.antszy.com:443/api/v1/user-data/...
//
// 请求体 form-urlencoded：
//   timestamp=<unix_ms>&card=<card>&access_key=<ak>&bind_device=<cpu_serial>
//
// 响应 JSON 字段（与逆向一致）：
//   { expire_time: ms, isPro: bool, bind_device: str, errorCode: str,
//     message: str, access_token?: str }
//
// 错误码映射：
//   "SERIALIZATION_ERROR" / "INVALID_CARD" → kInvalidCard
//   "BIND_MISMATCH" → kBoundElsewhere
//   "EXPIRED"       → kExpired
//   errorCode 不存在 + 200 + expire_time 合法 → kValid
class AiboxLicenseClient : public ILicenseClient {
public:
    AiboxLicenseClient() = default;
    ~AiboxLicenseClient() override = default;

    // 预先判断：卡号前缀判定 Pro/普通，避免与服务端双跳
    static bool card_prefix_is_pro(const std::string& card);

    // 方便开发：允许覆写域名（--debug-license-endpoint）
    void override_endpoint(const std::string& pro_host,
                           const std::string& normal_host);

    // ILicenseClient
    bool verify_once(const std::string& card,
                     const std::string& bind_device,
                     LicenseStatus& out_status,
                     std::string* err_message) override;

private:
    std::string pro_host_ = "blpro.antszy.com";
    std::string normal_host_ = "blpt.antszy.com";
    int port_ = 443;
};

// 公共 HMAC-SHA256 access_key 生成（与原系统 timestamp|card|server_secret 一致）
// server_secret 在生产镜像中应独立分发（不进源码仓库），这里保留空实现供对接
std::string aibox_make_access_key(const std::string& card,
                                   const std::string& timestamp_ms,
                                   const std::string& server_secret);

}  // namespace ttbox::core::auth
