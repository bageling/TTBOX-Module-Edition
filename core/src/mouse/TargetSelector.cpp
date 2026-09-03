// TargetSelector.cpp — A10 目标选择器实现（多目标追踪 + 分层选择）
/*
 * TTBOX 文件说明
 *
 * 文件：TargetSelector.cpp
 *
 * 作用：
 *   从多个检测结果中选择一个最佳目标进行瞄准。
 *
 * 小白理解：
 *   AI 可能检测出 5 个目标，但一次只能瞄准一个。
 *   TargetSelector 根据规则选一个：
 *   - 离瞄准点最近的目标
 *   - 连续出现多帧的目标（更稳定）
 *   - 跟踪已有目标（不会突然跳走）
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#include "mouse/TargetSelector.hpp"

#include <algorithm>
#include <cmath>

namespace ttbox::core::aim {

namespace {
float box_center_x(const DetectionBox& b) { return (b.x1 + b.x2) * 0.5f; }
float box_center_y(const DetectionBox& b) { return (b.y1 + b.y2) * 0.5f; }
float box_diag(const DetectionBox& b) {
    return std::hypot(b.x2 - b.x1, b.y2 - b.y1);
}
}  // namespace

std::vector<TargetSelector::Candidate> TargetSelector::collect_candidates(
    const std::vector<DetectionBox>& dets, const TargetSelectorConfig& cfg, float cx, float cy,
    float radius_sq) const {
    std::vector<Candidate> out;
    for (const auto& b : dets) {
        if (b.score < cfg.confidence) continue;
        if (!cfg.class_filter.empty()) {
            const bool in = std::find(cfg.class_filter.begin(), cfg.class_filter.end(),
                                      b.class_id) != cfg.class_filter.end();
            if (!in) continue;
        }
        const float bdx = b.x1 + (b.x2 - b.x1) * cfg.aim_ratio_x - cx;
        const float bdy = b.y1 + (b.y2 - b.y1) * cfg.aim_ratio_y - cy;
        const float d_sq = bdx * bdx + bdy * bdy;
        if (d_sq > radius_sq) continue;  // FOV 范围外
        out.push_back({b, box_center_x(b), box_center_y(b), d_sq});
    }
    std::sort(out.begin(), out.end(),
              [](const Candidate& a, const Candidate& b) { return a.dist_sq < b.dist_sq; });
    return out;
}

TargetSelection TargetSelector::select(const std::vector<DetectionBox>& dets,
                                       const TargetSelectorConfig& cfg, uint32_t now_ms) {
    TargetSelection out;
    if (dets.empty() || cfg.roi_w == 0 || cfg.roi_h == 0) {
        // 无检测：激活 track 丢失计数 + 宽限判定
        for (auto& t : tracks_) {
            if (t.active) {
                t.lost_frames++;
                // 宽限耗尽 → 放弃激活（不立即删 track，允许后续重建）
                if (t.lost_frames * 7 >= static_cast<uint32_t>(cfg.lost_grace_ms + 7)) {
                    t.active = false;
                    active_track_ = -1;
                }
            }
        }
        last_reason_ = TargetSelection::kNone;
        return out;
    }

    const float cx = static_cast<float>(cfg.roi_w) * cfg.center_x;
    const float cy = static_cast<float>(cfg.roi_h) * cfg.center_y;
    const float radius = std::min(cfg.roi_w, cfg.roi_h) * 0.5f * cfg.fov_range;
    const float radius_sq = radius * radius;

    auto cands = collect_candidates(dets, cfg, cx, cy, radius_sq);
    if (cands.empty()) {
        // 有检测但全被过滤/出范围：同上宽限判定
        for (auto& t : tracks_) {
            if (t.active) {
                t.lost_frames++;
                if (t.lost_frames * 7 >= static_cast<uint32_t>(cfg.lost_grace_ms + 7)) {
                    t.active = false;
                    active_track_ = -1;
                }
            }
        }
        last_reason_ = TargetSelection::kNone;
        return out;
    }

    // ---- 第 1 层：track_lock（候选与激活 track 相同 id）----
    // 激活 track 的匹配范围：自身对角 × 2（容忍检测抖动）
    if (active_track_ >= 0) {
        TrackEntry* at = nullptr;
        for (auto& t : tracks_) {
            if (t.id == active_track_) { at = &t; break; }
        }
        if (at) {
            // track_lock 匹配半径：目标对角 × 1.0 + 8px（同目标连续帧小位移；
            // 大位移/重编号由第 2 层 rect_lock 兜底，防误匹配邻近目标）
            const float match_r = box_diag(at->box) * 1.0f + 8.0f;
            const float match_r_sq = match_r * match_r;
            // 找离锁定框中心最近的候选
            const Candidate* best = nullptr;
            float best_d = match_r_sq;
            for (const auto& c : cands) {
                const float d = (c.cx - at->cx) * (c.cx - at->cx) +
                                (c.cy - at->cy) * (c.cy - at->cy);
                if (d < best_d) { best_d = d; best = &c; }
            }
            if (best) {
                // 更新 track
                at->box = best->box;
                at->cx = best->cx;
                at->cy = best->cy;
                at->last_seen_ms = now_ms;
                at->lost_frames = 0;
                out.valid = true;
                out.box = best->box;
                out.target_id = at->id;
                out.distance = std::sqrt(best->dist_sq);
                out.lock_radius = std::max(1.0f, 0.06f * (best->box.x2 - best->box.x1));
                out.reason = TargetSelection::kTrackLock;
                last_reason_ = out.reason;
                return out;
            }
            // 激活 track 未匹配：丢失宽限
            at->lost_frames++;
            const bool grace_exhausted =
                at->lost_frames * 7 >= static_cast<uint32_t>(cfg.lost_grace_ms + 7);
            if (grace_exhausted) {
                at->active = false;
                active_track_ = -1;
                // 继续走第 2/3 层
            } else {
                // 宽限内：保持原目标（用锁定框），不切换
                out.valid = true;
                out.box = at->box;
                out.target_id = at->id;
                const float ddx = at->cx - cx, ddy = at->cy - cy;
                out.distance = std::sqrt(ddx * ddx + ddy * ddy);
                out.lock_radius = std::max(1.0f, 0.06f * (at->box.x2 - at->box.x1));
                out.reason = TargetSelection::kTrackLock;
                last_reason_ = out.reason;
                return out;
            }
        } else {
            active_track_ = -1;  // 激活 track 不存在（被清理）
        }
    }

    // ---- 第 2 层：rect_lock / continuity（候选与任一 track 位置匹配）----
    // 遍历非激活 track（含宽限内旧目标），按位置匹配
    {
        float best_d = 1e30f;
        const Candidate* best_c = nullptr;
        TrackEntry* best_t = nullptr;
        for (auto& t : tracks_) {
            if (t.active) continue;  // 已有激活走第 1 层
            const float match_r = box_diag(t.box) * cfg.switch_match_ratio + 24.0f;
            const float match_r_sq = match_r * match_r;
            for (const auto& c : cands) {
                const float d = (c.cx - t.cx) * (c.cx - t.cx) + (c.cy - t.cy) * (c.cy - t.cy);
                if (d < match_r_sq && d < best_d) { best_d = d; best_c = &c; best_t = &t; }
            }
        }
        if (best_c && best_t) {
            best_t->box = best_c->box;
            best_t->cx = best_c->cx;
            best_t->cy = best_c->cy;
            best_t->last_seen_ms = now_ms;
            best_t->lost_frames = 0;
            best_t->active = true;
            active_track_ = best_t->id;
            out.valid = true;
            out.box = best_c->box;
            out.target_id = best_t->id;
            out.distance = std::sqrt(best_c->dist_sq);
            out.lock_radius = std::max(1.0f, 0.06f * (best_c->box.x2 - best_c->box.x1));
            out.reason = TargetSelection::kRectLock;
            last_reason_ = out.reason;
            return out;
        }
    }

    // ---- 第 3 层：score（无锁定，新建 track 或复用最近 track）----
    {
        const Candidate& c = cands.front();  // 已按距离排序，取最近
        // 复用已存在但未激活且距离近的 track（防同目标重复建 track）
        TrackEntry* reuse = nullptr;
        for (auto& t : tracks_) {
            if (t.active) continue;
            const float d = (c.cx - t.cx) * (c.cx - t.cx) + (c.cy - t.cy) * (c.cy - t.cy);
            if (d < 1600.0f) { reuse = &t; break; }  // <40px 复用
        }
        if (reuse) {
            reuse->box = c.box;
            reuse->cx = c.cx;
            reuse->cy = c.cy;
            reuse->last_seen_ms = now_ms;
            reuse->lost_frames = 0;
            reuse->active = true;
            active_track_ = reuse->id;
            out.box = c.box;
            out.target_id = reuse->id;
        } else {
            // 新建 track
            TrackEntry nt;
            nt.id = next_id_++;
            nt.box = c.box;
            nt.cx = c.cx;
            nt.cy = c.cy;
            nt.last_seen_ms = now_ms;
            nt.lost_frames = 0;
            nt.active = true;
            tracks_.push_back(nt);
            active_track_ = nt.id;
            out.box = c.box;
            out.target_id = nt.id;
        }
        out.valid = true;
        out.distance = std::sqrt(c.dist_sq);
        out.lock_radius = std::max(1.0f, 0.06f * (c.box.x2 - c.box.x1));
        out.reason = TargetSelection::kScore;
        last_reason_ = out.reason;
        return out;
    }
}

}  // namespace ttbox::core::aim
