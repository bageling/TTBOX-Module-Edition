// Stats.hpp — 通用耗时统计（min/avg/p50/p95/p99/max），header-only
#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <vector>

namespace ttbox::core {

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

    void absorb(const StatsCollector& other) {
        if (this == &other) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::lock_guard<std::mutex> other_lock(other.mutex_);
        samples_.insert(samples_.end(), other.samples_.begin(), other.samples_.end());
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size();
    }

    uint64_t min() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.empty() ? 0 : *std::min_element(samples_.begin(), samples_.end());
    }

    double avg() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0.0;
        const uint64_t total = std::accumulate(samples_.begin(), samples_.end(), uint64_t{0});
        return static_cast<double>(total) / static_cast<double>(samples_.size());
    }

    uint64_t max() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.empty() ? 0 : *std::max_element(samples_.begin(), samples_.end());
    }

    uint64_t percentile(double p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (samples_.empty()) return 0;
        std::vector<uint64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        if (p <= 0.0) return sorted.front();
        if (p >= 100.0) return sorted.back();
        const double index = (static_cast<double>(sorted.size()) - 1.0) * (p / 100.0);
        const size_t lower = static_cast<size_t>(index);
        const size_t upper = std::min(lower + 1, sorted.size() - 1);
        const double fraction = index - static_cast<double>(lower);
        return static_cast<uint64_t>(static_cast<double>(sorted[lower]) +
                                     fraction * static_cast<double>(sorted[upper] - sorted[lower]));
    }

private:
    mutable std::mutex mutex_;
    std::vector<uint64_t> samples_;
};

}  // namespace ttbox::core
