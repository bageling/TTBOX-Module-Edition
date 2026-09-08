// DmaBuf.cpp — DMA-BUF fd RAII 实现
#include "capture/DmaBuf.hpp"

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace ttbox::core {

void DmaBufFd::close() noexcept {
    if (fd_ < 0) {
        return;  // 已关闭（double close 保护）
    }
#if !defined(_WIN32)
    ::close(fd_);
#else
    // Windows 占位（本阶段无 DMA-BUF）
#endif
    fd_ = -1;
    length_ = 0;
}

}  // namespace ttbox::core
