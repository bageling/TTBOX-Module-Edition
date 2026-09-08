// LocalHidBackend.cpp — 本机 HID 后端（迁移自 AiboxHidOutput）
#include "output/LocalHidBackend.hpp"

#if !defined(_WIN32)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <chrono>

namespace ttbox::core::output {

bool LocalHidBackend::open_if_needed() {
#if defined(_WIN32)
    return false;
#else
    if (fd_ >= 0) return true;
    fd_ = ::open(path_.c_str(), O_WRONLY | O_NONBLOCK);
    return fd_ >= 0;
#endif
}

bool LocalHidBackend::connect(std::string* error) {
#if defined(_WIN32)
    if (error) *error = "Windows 无 V4L2/HID 硬件";
    health_.state = BackendState::kError;
    health_.detail = "unsupported platform";
    return false;
#else
    health_.state = BackendState::kConnecting;
    health_.detail = "opening " + path_;
    if (open_if_needed()) {
        health_.state = BackendState::kConnected;
        health_.detail = "connected";
        return true;
    }
    health_.state = BackendState::kError;
    health_.detail = std::string(std::strerror(errno));
    if (error) *error = "open " + path_ + " failed: " + std::string(std::strerror(errno));
    return false;
#endif
}

void LocalHidBackend::disconnect() {
#if !defined(_WIN32)
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
    health_.state = BackendState::kDisconnected;
    health_.detail = "disconnected";
}

bool LocalHidBackend::reconnect(std::string* error) {
    disconnect();
    ++health_.reconnect_count;
    return connect(error);
}

BackendHealth LocalHidBackend::health() const { return health_; }

bool LocalHidBackend::write_report(const unsigned char report[9]) {
#if defined(_WIN32)
    (void)report;
    return false;
#else
    const ssize_t n = ::write(fd_, report, 9);
    if (n == static_cast<ssize_t>(9)) return true;
    if (errno == EPIPE || errno == ENXIO) {
        disconnect();  // 设备掉线，下次发送前重开
    }
    return false;
#endif
}

bool LocalHidBackend::mouse_move(int32_t dx, int32_t dy, int32_t wheel) {
    if (!gate_allows()) return false;
    if (!open_if_needed()) return false;
    // 与 AiboxHidOutput 相同的 9 字节报告：
    // ReportID=2 + buttons(16bit LE, 置 0) + X(int16 LE) + Y(int16 LE) + wheel + pan
    const unsigned char report[9] = {
        0x02, 0x00, 0x00,
        static_cast<unsigned char>(dx & 0xff), static_cast<unsigned char>((dx >> 8) & 0xff),
        static_cast<unsigned char>(dy & 0xff), static_cast<unsigned char>((dy >> 8) & 0xff),
        static_cast<unsigned char>(wheel & 0xff), 0x00};
    const bool ok = write_report(report);
    if (ok) {
        ++health_.send_ok;
        health_.last_send_ok_us = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    } else {
        ++health_.send_fail;
    }
    return ok;
}

bool LocalHidBackend::mouse_button(uint8_t button, uint8_t action) {
    // 本机 gadget 当前只注入移动（与 AiboxHidOutput 行为一致：不改按钮状态）。
    // 保留接口：需要按钮输出时在此按 gadget report 填充 button 位。
    (void)button; (void)action;
    return gate_allows() ? true : false;
}

bool LocalHidBackend::mouse_click(uint8_t button) {
    return mouse_button(button, kActClick);
}

}  // namespace ttbox::core::output