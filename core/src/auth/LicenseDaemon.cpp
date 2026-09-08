#include "auth/LicenseDaemon.hpp"

#include <chrono>
#include <thread>

namespace ttbox::core::auth {

namespace {
int64_t now_unix_ms() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}
}  // namespace

LicenseDaemon::LicenseDaemon(ILicenseClient& client) : client_(client) {
    fp_ = DeviceFingerprint::detect();
}

LicenseDaemon::~LicenseDaemon() {
    stop();
}

void LicenseDaemon::set_card(const std::string& card_plain) {
    std::lock_guard<std::mutex> lk(mu_);
    card_plain_ = card_plain;
}

LicenseStatus LicenseDaemon::status_snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    return status_;
}

bool LicenseDaemon::allow_run() const {
    std::lock_guard<std::mutex> lk(mu_);
    if (status_.state == LicenseState::kValid ||
        status_.state == LicenseState::kFallback) {
        return true;
    }
    return false;
}

bool LicenseDaemon::is_pro() const {
    std::lock_guard<std::mutex> lk(mu_);
    return status_.is_pro;
}

bool LicenseDaemon::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false;
    }
    thread_ = std::thread(&LicenseDaemon::thread_loop, this);
    return true;
}

void LicenseDaemon::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

bool LicenseDaemon::verify_now_blocking(std::string* err_message) {
    std::string card;
    std::string bind;
    {
        std::lock_guard<std::mutex> lk(mu_);
        card = card_plain_;
        bind = fp_.bind_string();
        status_.state = LicenseState::kChecking;
    }
    LicenseStatus out;
    bool ok = client_.verify_once(card, bind, out, err_message);
    {
        std::lock_guard<std::mutex> lk(mu_);
        status_ = out;
        status_.bind_device = bind;
        if (ok && (out.state == LicenseState::kValid)) {
            status_.verified_at_ms = now_unix_ms();
            status_.next_check_ms = status_.verified_at_ms + heartbeat_ms_;
        }
    }
    return ok;
}

void LicenseDaemon::set_heartbeat_interval_ms(int64_t ms) {
    heartbeat_ms_ = ms;
}

void LicenseDaemon::set_backoff_base_ms(int64_t ms) {
    backoff_base_ms_ = ms;
}

void LicenseDaemon::thread_loop() {
    while (running_.load()) {
        std::string card;
        std::string bind;
        int64_t sleep_ms = 0;
        {
            std::lock_guard<std::mutex> lk(mu_);
            card = card_plain_;
            bind = fp_.bind_string();
            if (card.empty()) {
                status_.state = LicenseState::kInvalidCard;
                status_.last_error = "card not set";
                sleep_ms = std::min(backoff_base_ms_, backoff_cap_ms_);
            } else {
                const int64_t now = now_unix_ms();
                if (status_.state == LicenseState::kValid &&
                    now < status_.next_check_ms &&
                    (status_.expire_unix_ms == 0 ||
                     now < status_.expire_unix_ms)) {
                    // 有效期内且未到下一次心跳：睡到 next_check
                    sleep_ms = status_.next_check_ms - now;
                    // 若 expire 更早提前到期
                    if (status_.expire_unix_ms > 0) {
                        int64_t till_exp = status_.expire_unix_ms - now;
                        if (till_exp > 0 && till_exp < sleep_ms) sleep_ms = till_exp;
                    }
                } else {
                    // 需要本轮检查
                    do_check_cycle_locked(card, bind);
                    sleep_ms = cur_backoff_ms_ > 0
                                   ? cur_backoff_ms_
                                   : std::max<int64_t>(1000, heartbeat_ms_);
                }
            }
        }
        // 分段 sleep：响应 shutdown
        constexpr int64_t kSliceMs = 250;
        int64_t remain = sleep_ms;
        while (remain > 0 && running_.load()) {
            int64_t sl = remain < kSliceMs ? remain : kSliceMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(sl));
            remain -= sl;
        }
    }
}

void LicenseDaemon::do_check_cycle_locked(const std::string& card_plain,
                                            const std::string& bind_device) {
    LicenseStatus out;
    std::string err;
    bool req_ok = client_.verify_once(card_plain, bind_device, out, &err);
    status_ = out;
    status_.bind_device = bind_device;
    if (!card_plain.empty()) {
        // 脱敏：前 8 位 + ******
        std::string s = card_plain;
        if (s.size() > 8) s = s.substr(0, 8) + "******";
        status_.card = s;
    }
    const int64_t now = now_unix_ms();
    if (req_ok && out.state == LicenseState::kValid) {
        status_.verified_at_ms = now;
        status_.next_check_ms = now + heartbeat_ms_;
        cur_backoff_ms_ = 0;
    } else {
        // Fallback：上次验证成功且未到 expire_time
        if (status_.state != LicenseState::kValid &&
            status_.verified_at_ms > 0 &&
            status_.expire_unix_ms > 0 &&
            now < status_.expire_unix_ms) {
            status_.state = LicenseState::kFallback;
        }
        // 指数退避
        if (cur_backoff_ms_ <= 0) {
            cur_backoff_ms_ = backoff_base_ms_;
        } else {
            cur_backoff_ms_ = std::min(cur_backoff_ms_ * 2, backoff_cap_ms_);
        }
    }
}

}  // namespace ttbox::core::auth
