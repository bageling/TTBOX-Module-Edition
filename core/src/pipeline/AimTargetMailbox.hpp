// AimTargetMailbox.hpp — Worker 到 AimThread 的最新任务邮箱。
// 任务只含小型检测结果，不传图像；使用 shared_ptr 原子快照避免读写数据竞争。
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include "pipeline/AimTargetTask.hpp"
namespace ttbox::core::aim {
class AimTargetMailbox {
public:
    static constexpr std::size_t kMaxWorkers = 3;
    explicit AimTargetMailbox(std::size_t workers = 1)
        : worker_count_(workers > kMaxWorkers ? kMaxWorkers : workers) {}
    std::size_t worker_count() const { return worker_count_; }
    bool offer(std::size_t worker_id, AimTargetTask task) {
        if (worker_id >= worker_count_) return false;
        auto value = std::make_shared<const AimTargetTask>(std::move(task));
        std::atomic_store_explicit(&slots_[worker_id], std::move(value), std::memory_order_release);
        return true;
    }
    bool take_latest(AimTargetTask* out, uint64_t last_frame = 0) const {
        if (!out) return false;
        std::shared_ptr<const AimTargetTask> best;
        uint64_t best_frame = last_frame;
        for (std::size_t i = 0; i < worker_count_; ++i) {
            auto task = std::atomic_load_explicit(&slots_[i], std::memory_order_acquire);
            if (task && task->frame_number > best_frame) {
                best_frame = task->frame_number;
                best = std::move(task);
            }
        }
        if (!best) return false;
        *out = *best;
        return true;
    }
    // 清空所有槽位。CoreRuntime 重启（停止→启动）时 V4L2 sequence 从 0 重新计数，
    // 若不清除上一周期的残留任务，AimThread 的 last_frame 会被旧任务抬到旧帧号，
    // 新周期所有任务（frame_number 从 0 重新递增）都会被 take_latest 去重丢弃，
    // 表现为重启后 1~3 分钟检测框不更新（直到帧号重新涨回旧值）。
    void clear() {
        for (std::size_t i = 0; i < worker_count_; ++i) {
            std::atomic_store_explicit(&slots_[i], std::shared_ptr<const AimTargetTask>{},
                                       std::memory_order_release);
        }
    }
private:
    std::array<std::shared_ptr<const AimTargetTask>, kMaxWorkers> slots_{};
    std::size_t worker_count_;
};
}
