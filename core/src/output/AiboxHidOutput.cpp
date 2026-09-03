// AiboxHidOutput.cpp — AIBOX 兼容鼠标报告写入 /dev/hidg0
#include "output/AiboxHidOutput.hpp"
#include "model/RuntimeProfile.hpp"
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace ttbox::core::output {
bool AiboxHidOutput::open_if_needed() {
#if defined(_WIN32)
    return false;
#else
    if (fd_ >= 0) return true;
    fd_ = ::open(path_.c_str(), O_WRONLY | O_NONBLOCK);
    return fd_ >= 0;
#endif
}
void AiboxHidOutput::close() {
#if !defined(_WIN32)
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#endif
}
bool AiboxHidOutput::send(const OutputAction& a) {
#if defined(_WIN32)
    (void)a; return false;
#else
    // 静态总闸：未显式启用时不写入真实鼠标 Gadget。
    if (!enabled_) return false;
    // 配置实时保险门（每次发送重新判定，改热键/总开关即时生效，无需重启）：
    //   1) mouse.enabled 为总开关；
    //   2) 放行掩码 = aim_hotkey | aim_hotkey2，全部来自用户配置，不写死任何键位。
    //      主/副热键均为 0 时视为配置缺失，直接拒绝注入（fail-closed）。
    if (config_source_) {
        auto p = config_source_->snapshot();
        if (!p) return false;
        if (!p->mouse.enabled) return false;
        const uint16_t mask = static_cast<uint16_t>(
            static_cast<uint16_t>(p->mouse.aim_hotkey) | static_cast<uint16_t>(p->mouse.aim_hotkey2));
        if (mask == 0) return false;  // 配置缺失 → 禁止注入
        if (button_source_ && (button_source_->load(std::memory_order_acquire) & mask) == 0) return false;
    } else if (button_source_) {
        // 无配置源时无从得知用户热键 → 禁止注入（fail-closed，不猜默认键）。
        return false;
    }
    if (!open_if_needed()) return false;
    // 当前 gadget 鼠标描述符要求 9 字节：ReportID=2 + buttons(16bit LE)
    // + X(int16 LE) + Y(int16 LE) + wheel + pan。
    // 只注入鼠标移动，不改按钮状态。
    const unsigned char report[9] = {
        0x02, 0x00, 0x00,
        static_cast<unsigned char>(a.move_x & 0xff), static_cast<unsigned char>((a.move_x >> 8) & 0xff),
        static_cast<unsigned char>(a.move_y & 0xff), static_cast<unsigned char>((a.move_y >> 8) & 0xff),
        0x00, 0x00};
    const ssize_t n = ::write(fd_, report, sizeof(report));
    if (n == static_cast<ssize_t>(sizeof(report))) return true;
    if (errno == EPIPE || errno == ENXIO) close();
    return false;
#endif
}
}  // namespace ttbox::core::output
