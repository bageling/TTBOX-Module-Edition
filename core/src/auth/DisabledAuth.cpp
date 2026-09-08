// DisabledAuth.cpp — 实时核心的离线/测试授权替身。
//
// 默认板端核心不编译在线 HTTPS 授权层，避免把 OpenSSL 开发依赖带进
// 推理进程。正式授权由独立控制面接入；显式打开 TTBOX_CORE_BUILD_AUTH
// 时使用真实实现。
#include "auth/AiboxLicenseClient.hpp"
#include "auth/LicenseDaemon.hpp"

namespace ttbox::core::auth {

bool AiboxLicenseClient::card_prefix_is_pro(const std::string&) { return false; }

void AiboxLicenseClient::override_endpoint(const std::string& pro_host,
                                           const std::string& normal_host) {
    if (!pro_host.empty()) pro_host_ = pro_host;
    if (!normal_host.empty()) normal_host_ = normal_host;
}

bool AiboxLicenseClient::verify_once(const std::string&,
                                     const std::string&,
                                     LicenseStatus& out_status,
                                     std::string* err_message) {
    out_status = {};
    out_status.state = LicenseState::kNetworkError;
    if (err_message) *err_message = "在线授权层未编译（TTBOX_CORE_BUILD_AUTH=OFF）";
    return false;
}

std::string aibox_make_access_key(const std::string&,
                                  const std::string&,
                                  const std::string&) {
    return {};
}

LicenseDaemon::LicenseDaemon(ILicenseClient& client) : client_(client) {}
LicenseDaemon::~LicenseDaemon() { stop(); }
void LicenseDaemon::set_card(const std::string& card_plain) { std::lock_guard<std::mutex> lk(mu_); card_plain_ = card_plain; }
LicenseStatus LicenseDaemon::status_snapshot() const { std::lock_guard<std::mutex> lk(mu_); return status_; }
bool LicenseDaemon::allow_run() const { return true; }
bool LicenseDaemon::is_pro() const { return false; }
bool LicenseDaemon::start() { running_.store(true); std::lock_guard<std::mutex> lk(mu_); status_.state = LicenseState::kFallback; return true; }
void LicenseDaemon::stop() { running_.store(false); if (thread_.joinable()) thread_.join(); }
bool LicenseDaemon::verify_now_blocking(std::string*) { return true; }
void LicenseDaemon::set_heartbeat_interval_ms(int64_t ms) { heartbeat_ms_ = ms; }
void LicenseDaemon::set_backoff_base_ms(int64_t ms) { backoff_base_ms_ = ms; }

}  // namespace ttbox::core::auth
