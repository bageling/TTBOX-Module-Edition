#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "auth/DeviceFingerprint.hpp"

namespace ttbox::core::auth {

enum class LicenseState : int {
    kUnknown = 0,       // 启动时未验证
    kChecking = 1,      // 正在与服务端握手
    kValid = 2,         // 授权有效（Pro或普通）
    kExpired = 3,       // 卡已过期
    kInvalidCard = 4,   // 卡号非法
    kBoundElsewhere = 5,// 卡已绑定到其他 cpu_serial
    kNetworkError = 6,  // 无法访问授权服务器
    kFallback = 7,      // 本地缓存通过（无法连网时）
};

struct LicenseStatus {
    LicenseState state = LicenseState::kUnknown;
    bool is_pro = false;        // Pro 版 / 普通版
    int64_t expire_unix_ms = 0; // 过期时间点（Unix ms，0 = 未知）
    std::string card;           // 当前卡号（前 8 位 + ****** 脱敏公开）
    std::string bind_device;    // 当前绑定 cpu_serial
    std::string last_error;     // 上次失败原因（仅调试用，不对外公开敏感）
    int64_t verified_at_ms = 0; // 上次成功验卡时间
    int64_t next_check_ms = 0;  // 下次必查时间（心跳周期）
    std::string cached_token;   // 本地缓存 token（Fallback 时校验签名）
};

// 授权客户端：与原 aibox-bl 一致，支持双域名分流
//   Pro 卡 → https://blpro.antszy.com/api/v1/verifyCard
//   普通 → https://blpt.antszy.com/api/v1/verifyCard
//   user-data → https://data.antszy.com/api/v1/user-data/
class ILicenseClient {
public:
    virtual ~ILicenseClient() = default;

    // 发送一次 verifyCard 请求；返回 true 表示请求本身成功（不代表卡有效）
    // 协议（与逆向证据一致）：
    //   POST /api/v1/verifyCard
    //   Body: timestamp=<unix_ms>&card=<card>&access_key=<ak>&bind_device=<cpu_serial>
    //   Resp (JSON): { expire_time, isPro, bind_device, errorCode, access_token }
    virtual bool verify_once(const std::string& card,
                             const std::string& bind_device,
                             LicenseStatus& out_status,
                             std::string* err_message = nullptr) = 0;
};

// 授权守护：后台线程，周期 heartbeat 与原 cardVerifyThreadFunc 等价
//  - 启动：立即 verify_once；成功后按心跳（默认 1h）
//  - 失败：按指数退避（30s → 2m → 10m → 30m max）
//  - 本地 Fallback：若上次已通过且当前时间 < expire_time，允许运行
class LicenseDaemon {
public:
    explicit LicenseDaemon(ILicenseClient& client);
    ~LicenseDaemon();

    // 卡号来源：配置 + 命令行 --license；优先使用命令行
    void set_card(const std::string& card_plain);

    LicenseStatus status_snapshot() const;
    bool allow_run() const;   // 是否允许 AI 主功能（Valid | Fallback）
    bool is_pro() const;      // Pro 功能：武器扩展模型、高级 FOV 选项

    // 启动/停止后台线程；不会阻塞调用者
    bool start();
    void stop();

    // 同步触发一次立即验卡（用于 --verify-only CLI 选项）
    bool verify_now_blocking(std::string* err_message = nullptr);

    // ---- 便于测试 / mock 的注入点 ----
    void set_heartbeat_interval_ms(int64_t ms);
    void set_backoff_base_ms(int64_t ms);

private:
    void thread_loop();
    void do_check_cycle_locked(const std::string& card_plain,
                                const std::string& bind_device);

    ILicenseClient& client_;
    mutable std::mutex mu_;
    LicenseStatus status_;
    std::string card_plain_;      // 完整卡号（内存中，不进入日志/JSON dump）
    DeviceFingerprint fp_;

    int64_t heartbeat_ms_ = 3600 * 1000;   // 1h 默认
    int64_t backoff_base_ms_ = 30 * 1000;  // 30s
    int64_t backoff_cap_ms_ = 1800 * 1000; // 30m max
    int64_t cur_backoff_ms_ = 0;

    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace ttbox::core::auth
