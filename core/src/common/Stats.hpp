// Stats.hpp — 通用耗时统计（min/avg/p50/p95/p99/max），header-only
#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <vector>

namespace ttbox::core {

// 样本收集 + 分位数统计（单/多线程安全；样本保留在内存中）
class StatsCollector {
public:
    void add(uint64_t us) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(us);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
    }

    // 合并另一收集器的全部样本（A-5 Worker：每帧吸收 RKNNEngine 阶段统计）
    void absorb(const StatsCollector& other) {
        if (this == &other) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::lock_guard<std::mutex> lock2(other.mutex_);
        samples_.insert(samples_.end(), other.samples_.begin(), other.samples_.end());
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size();
    }

    uint64_t min() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.empty() ? 0
               : *std::min_element(samples_.begin(), samples_.end());
    }

    double avg() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        return static_cast<double>(std::accumulate(samples_.begin(), samples_.end(),
                                                   uint64_t{0})) /
               static_cast<double>(samples_.size());
    }

    uint64_t max() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.empty() ? 0
               : *std::max_element(samples_.begin(), samples_.end());
    }

    // 分位数（0~100）。排序后按最近秩插值。
    uint64_t percentile(double p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0;
        std::vector<uint64_t> s = samples_;
        std::sort(s.begin(), s.end());
        if (p <= 0.0) return s.front();
        if (p >= 100.0) return s.back();
        const double idx = (static_cast<double>(s.size()) - 1.0) * (p / 100.0);
        const size_t lo = static_cast<size_t>(idx);
        const size_t hi = std::min(lo + 1, s.size() - 1);
        const double frac = idx - static_cast<double>(lo);
        return static_cast<uint64_t>(static_cast<double>(s[lo]) +
                                     frac * (static_cast<double>(s[hi]) - static_cast<double>(s[lo])));
    }

private:
    mutable std::mutex mutex_;
    std::vector<uint64_t> samples_;
};

}  // namespace ttbox::core
