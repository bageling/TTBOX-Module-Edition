// TargetSelector.hpp — A10 目标选择器（多目标追踪 + 分层选择）
//
// 全部在 ROI/crop 坐标系内选择目标（不恢复到全帧）。
// 对齐参考系统（aiassistance）目标选择层级：
//   track_lock → rect_lock/continuity → score
//   （1）selector_track_lock：候选存在与上一帧相同 track_id 的目标
//   （2）selector_rect_lock：track id 变但候选矩形与锁定目标位置匹配（同一目标被重编号）
//   （3）selector_score：无既有锁定，从候选集按评分选最优（距离+尺寸综合）
// 丢失宽限：锁定目标短时丢失（≤ lost_grace_ms）保持 track 不切换，宽限耗尽才切/放弃。
// 自适应锁定半径：lock_radius = max(1.0, 0.06 × 框宽)（对齐参考公式）。
/*
 * TTBOX 文件说明
 *
 * 文件：TargetSelector.hpp
 *
 * 作用：
 *   目标选择器的定义。
 *
 * 小白理解：
 *   从多个检测结果中选择一个最佳目标。
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#pragma once

#include <cstdint>
#include <vector>

#include "common/Types.hpp"
#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

// 目标选择配置（由 MouseProfile + ROI 尺寸派生，运行时组装）
struct TargetSelectorConfig {
    float fov_range = 1.0f;          // 0~1；搜索半径 = min(roi_w, roi_h) / 2 × fov_range
    float confidence = 0.25f;        // 置信度阈值
    std::vector<int> class_filter;   // 空 = 全部保留
    uint32_t roi_w = 0;              // ROI/crop 宽（DetectionBox 所在坐标系）
    uint32_t roi_h = 0;
    float center_x = 0.5f;           // 选择中心（crop 系归一化）
    float center_y = 0.5f;
    float lost_grace_ms = 30.0f;     // 目标丢失宽限（对齐参考 selector_lost_grace_ms=30）
    float aim_ratio_x = 0.5f;
    float aim_ratio_y = 0.2f;
    float switch_match_ratio = 0.4f; // rect_lock 匹配距离 = 目标对角 × 此比例
};

// 选择结果
struct TargetSelection {
    bool valid = false;
    DetectionBox box;
    int target_id = -1;              // 稳定追踪 id（track_id）
    float distance = 0.0f;           // 到选择中心的距离（px）
    float lock_radius = 0.0f;        // 自适应锁定半径（px）
    // 选择层级 reason（对齐参考 trace reason）
    enum Reason { kNone = 0, kTrackLock, kRectLock, kScore } reason = kNone;
};

// 单个追踪轨迹
struct TrackEntry {
    int id = -1;
    DetectionBox box;
    float cx = 0.0f;                 // 框中心
    float cy = 0.0f;
    uint32_t last_seen_ms = 0;       // 最后出现（外部时钟 ms）
    uint32_t lost_frames = 0;        // 连续丢失帧数
    bool active = false;             // 是否激活（锁定目标）
};

// TargetSelector — 目标选择器：从多个检测框(DetectionBox)中挑出唯一要跟踪的目标。
// 输入：检测框列表 + 当前时钟(now_ms)
// 输出：TargetSelection（选中的目标：类别/位置/锁定状态）
// 规则：多帧稳定防跳变 + 类别过滤 + 丢失宽限；被 AimThread 每帧调用
class TargetSelector {
public:
    // 有状态选择：内部维护多目标 track 表。
    // now_ms = 当前毫秒时钟（用于丢失宽限判定）。
    // 返回选择结果（valid=false 表示无目标）。
    TargetSelection select(const std::vector<DetectionBox>& dets,
                           const TargetSelectorConfig& cfg, uint32_t now_ms = 0);

    // 最近一次选择的 reason（供外部观测）
    TargetSelection::Reason last_reason() const { return last_reason_; }

    // 重置所有 track（目标切换/瞄准退出时）
    void reset() { tracks_.clear(); active_track_ = -1; last_reason_ = TargetSelection::kNone; }

    const std::vector<TrackEntry>& tracks() const { return tracks_; }

private:
    // 从检测框列表匹配候选（过滤 + 距离排序）
    struct Candidate { DetectionBox box; float cx, cy, dist_sq; };
    std::vector<Candidate> collect_candidates(const std::vector<DetectionBox>& dets,
                                              const TargetSelectorConfig& cfg, float cx, float cy,
                                              float radius_sq) const;

    std::vector<TrackEntry> tracks_;
    int active_track_ = -1;          // 当前激活锁定 track id
    TargetSelection::Reason last_reason_ = TargetSelection::kNone;
    uint32_t next_id_ = 1;
};

}  // namespace ttbox::core::aim
