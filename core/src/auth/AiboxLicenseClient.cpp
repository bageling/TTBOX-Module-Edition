#include "auth/AiboxLicenseClient.hpp"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>

#if defined(__linux__) || defined(__APPLE__)
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

#include "auth/HttpClient.hpp"
#include "common/Json.hpp"

namespace ttbox::core::auth {

namespace {
int64_t now_unix_ms() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string hex_encode(const uint8_t* data, size_t n) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = hex[(data[i] >> 4) & 0x0F];
        out[2 * i + 1] = hex[data[i] & 0x0F];
    }
    return out;
}
}  // namespace

bool AiboxLicenseClient::card_prefix_is_pro(const std::string& card) {
    // 原系统 Pro/普通版分流通过配置 + 服务端 isPro 字段回传最终判定。
    // 这里保守默认都用普通版域名请求，让服务端返回 isPro 决定。
    (void)card;
    return false;
}

void AiboxLicenseClient::override_endpoint(const std::string& pro_host,
                                            const std::string& normal_host) {
    if (!pro_host.empty()) pro_host_ = pro_host;
    if (!normal_host.empty()) normal_host_ = normal_host;
}

bool AiboxLicenseClient::verify_once(const std::string& card,
                                      const std::string& bind_device,
                                      LicenseStatus& out_status,
                                      std::string* err_message) {
    out_status = LicenseStatus{};
    if (card.empty()) {
        out_status.state = LicenseState::kInvalidCard;
        out_status.last_error = "card empty";
        if (err_message) *err_message = out_status.last_error;
        return false;
    }
    const std::string ts = std::to_string(now_unix_ms());
    // access_key：若未提供 server_secret 则与 card 相同（开发占位）
    std::string ak = aibox_make_access_key(card, ts, "");
    if (ak.empty()) ak = card;

    std::ostringstream form;
    form << "timestamp=" << ts
         << "&card=" << card
         << "&access_key=" << ak
         << "&bind_device=" << bind_device;
    const std::string& host = card_prefix_is_pro(card) ? pro_host_ : normal_host_;
    HttpsPostResult r = https_post_form(host, port_, "/api/v1/verifyCard", form.str());
    if (r.status_code == 0) {
        out_status.state = LicenseState::kNetworkError;
        out_status.last_error = r.error.empty() ? std::string("network") : r.error;
        if (err_message) *err_message = out_status.last_error;
        return false;
    }
    if (r.status_code != 200) {
        out_status.state = LicenseState::kNetworkError;
        out_status.last_error = "http " + std::to_string(r.status_code);
        if (err_message) *err_message = out_status.last_error;
        return false;
    }
    if (r.body.empty()) {
        out_status.state = LicenseState::kInvalidCard;
        out_status.last_error = "empty body";
        if (err_message) *err_message = out_status.last_error;
        return false;
    }

    // 解析 JSON：接受 errorCode / expire_time / isPro / bind_device / access_token
    JsonValue json{};
    std::string parse_err;
    if (!JsonValue::parse(r.body, json, &parse_err)) {
        out_status.state = LicenseState::kInvalidCard;
        out_status.last_error = "bad json: " + parse_err;
        if (err_message) *err_message = out_status.last_error;
        return false;
    }
    const std::string ec = json.has("errorCode") ? json.at("errorCode").to_string()
                                                 : std::string();
    if (!ec.empty()) {
        if (ec == "EXPIRED") {
            out_status.state = LicenseState::kExpired;
        } else if (ec == "BIND_MISMATCH") {
            out_status.state = LicenseState::kBoundElsewhere;
        } else if (ec == "INVALID_CARD" || ec == "SERIALIZATION_ERROR") {
            out_status.state = LicenseState::kInvalidCard;
        } else {
            out_status.state = LicenseState::kInvalidCard;
        }
        out_status.last_error = ec;
        if (json.has("message")) {
            out_status.last_error += ":" + json.at("message").to_string();
        }
        if (err_message) *err_message = out_status.last_error;
        return true;
    }
    if (json.has("expire_time")) {
        out_status.expire_unix_ms = static_cast<int64_t>(json.at("expire_time").to_number());
    }
    if (json.has("isPro")) {
        out_status.is_pro = json.at("isPro").to_boolean();
    }
    if (json.has("bind_device")) {
        out_status.bind_device = json.at("bind_device").to_string();
    }
    if (json.has("access_token")) {
        out_status.cached_token = json.at("access_token").to_string();
    }
    out_status.state = LicenseState::kValid;
    return true;
}

std::string aibox_make_access_key(const std::string& card,
                                   const std::string& timestamp_ms,
                                   const std::string& server_secret) {
    if (server_secret.empty()) {
        // 未提供 server_secret：返回空，caller 降级为 card 原值占位
        return {};
    }
#if defined(__linux__) || defined(__APPLE__)
    std::string payload = timestamp_ms + "|" + card;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    HMAC(EVP_sha256(),
         server_secret.data(), static_cast<int>(server_secret.size()),
         reinterpret_cast<const unsigned char*>(payload.data()), payload.size(),
         md, &md_len);
    return hex_encode(md, md_len);
#else
    return {};
#endif
}

}  // namespace ttbox::core::auth
