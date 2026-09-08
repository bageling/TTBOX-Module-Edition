// TargetSelector.hpp — A10 目标选择器（多目标追踪 + 分层选择）
//
// 全部在 ROI/crop 坐标系内选择目标（不恢复到全帧）。
// 目标选择层级：
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

        // ---- ByteTrack 增强（第4项，参考 VisionForge bytetrack_tracker.py）----
        // 在保持原有 track_lock/rect_lock/score 三层选择语义不变的前提下，
        // 为轨迹增加"卡尔曼速度预测 + 轨迹生命周期"：
        //   (1) association 参考点用 Kalman 预测框（非裸框心），
        //       慢速/匀速目标在测试中预测≈当前 → 行为兼容旧用例。
        //   (2) track 超 lost_frames 达 track_buffer_frames 即删除（修"只增不删"隐患）。
        //   (3) 总轨迹数超过 max_tracks 时优先裁剪最早未命中（最长静默）轨迹。
        float kalman_velocity_gain = 0.30f;  // 速度学习增益（VisionForge 0.22+q*8≈0.30）
            float kalman_max_speed_px = 25.0f;   // 预测速度上限（px/帧），防抖预测过度
            bool use_kalman_predict = false;      // 是否用卡尔曼预测中心做关联参考点
                                                  // 默认关：保持"裸框心最近邻"传统行为（109 用例兼容）。
                                                  // 开启时：匀速/快速目标用预测中心，抗遮挡/快速移动更稳，
                                                  // 但会改变关联参考点（需真机调参验证后再启用）。
            uint32_t track_buffer_frames = 30;   // 丢失缓冲帧数，超过即删除轨迹
            uint32_t max_tracks = 64;            // 轨迹总上限（防内存无限增长）

        // ---- 选择器行为（第13阶段确认）----
    // 选择排序：距离FOV中心排序后，若启用 priority，则同距离段内按优先级（越大越优先）。
    // 优先级（priority）用于"同距离竞争"时优先生成新 track / 参与 score 层。
    // 未启用（默认 0）时完全保持原有"距离最近优先"行为，兼容旧测试。
    bool priority_enabled = false;   // 是否启用优先级排序
    std::vector<int> priority_classes;      // 优先类别（优先级=1，列表内优先）
    std::vector<int> priority_classes_high; // 高优先类别（优先级=2，最优先）
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

    // ---- ByteTrack 卡尔曼状态（第4项）----
    // 匀速模型 [cx, cy, w, h, vx, vy, vw, vh]，关联时用预测中心 (px, py)。
    float kx = 0.0f, ky = 0.0f;      // 卡尔曼平滑位置
    float kw = 0.0f, kh = 0.0f;      // 卡尔曼平滑尺寸
    float vx = 0.0f, vy = 0.0f;      // 速度（px/帧）
    float vw = 0.0f, vh = 0.0f;      // 尺寸变化率（px/帧）
    float pred_cx = 0.0f, pred_cy = 0.0f;  // 预测中心（关联参考点）
    uint32_t hits = 0;               // 累计命中帧数
    bool confirmed = false;          // 是否已确认（hits 达标）
    uint32_t created_ms = 0;         // 创建时间（用于存在时长排序，裁剪最旧）
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
    struct Candidate {
        DetectionBox box;
        float cx, cy, dist_sq;
        int priority = 0;  // 类别优先级（0=普通 1=优先 2=高优先）
    };
    // 计算类别优先级：命中 high 列表=2，命中普通列表=1，否则=0
    int class_priority(const TargetSelectorConfig& cfg, int class_id) const {
        if (!cfg.priority_enabled) return 0;
        for (int c : cfg.priority_classes_high) if (c == class_id) return 2;
        for (int c : cfg.priority_classes) if (c == class_id) return 1;
        return 0;
    }
    std::vector<Candidate> collect_candidates(const std::vector<DetectionBox>& dets,
                                                  const TargetSelectorConfig& cfg, float cx, float cy,
                                                  float radius_sq) const;

        // ---- ByteTrack 辅助（第4项）----
        // 用卡尔曼匀速模型预测轨迹下一帧中心（写入 pred_cx/pred_cy），关联参考点。
        void kalman_predict(TrackEntry& t, const TargetSelectorConfig& cfg) const;
        // 用观测框更新卡尔曼状态（位置平滑 + 速度学习），并重算预测中心。
        void kalman_update(TrackEntry& t, const DetectionBox& obs, const TargetSelectorConfig& cfg,
                           uint32_t now_ms);
        // 轨迹生命周期：删除丢失超 buffer 的轨迹，并裁剪总轨迹数到 max_tracks 上限。
        void trim_tracks(const TargetSelectorConfig& cfg);

        std::vector<TrackEntry> tracks_;
                    int active_track_ = -1;          // 当前激活锁定 track id
                    TargetSelection::Reason last_reason_ = TargetSelection::kNone;
                    uint32_t next_id_ = 1;
        };

}  // namespace ttbox::core::aim
