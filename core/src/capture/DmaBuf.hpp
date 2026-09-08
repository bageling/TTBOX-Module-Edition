// DmaBuf.hpp — DMA-BUF fd RAII 封装
//
// 生命周期要求（阶段 A-2）：
//   - 构造接管 fd，析构自动 close
//   - move-only（拷贝禁止，防止 double close）
//   - close() 幂等（已关闭再调用无害）
#pragma once

#include <cstdint>
#include <utility>

namespace ttbox::core {

class DmaBufFd {
public:
    DmaBufFd() = default;
    DmaBufFd(int fd, uint32_t length) noexcept : fd_(fd), length_(length) {}
    ~DmaBufFd() { close(); }

    DmaBufFd(const DmaBufFd&) = delete;
    DmaBufFd& operator=(const DmaBufFd&) = delete;

    DmaBufFd(DmaBufFd&& other) noexcept { *this = std::move(other); }
    DmaBufFd& operator=(DmaBufFd&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            length_ = other.length_;
            other.fd_ = -1;
            other.length_ = 0;
        }
        return *this;
    }

    int fd() const noexcept { return fd_; }
    uint32_t length() const noexcept { return length_; }
    bool valid() const noexcept { return fd_ >= 0; }

    // 幂等关闭：fd 置 -1，防止 double close
    void close() noexcept;

private:
    int fd_ = -1;
    uint32_t length_ = 0;
};

}  // namespace ttbox::core
