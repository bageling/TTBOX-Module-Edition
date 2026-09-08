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
        out.push_back({b, box_center_x(b), box_center_y(b), d_sq,
                       class_priority(cfg, b.class_id)});
    }
    // 排序：priority 高者优先，同 priority 按距离近者优先。
    // （priority 用于"同距离竞争"时的目标优先级，不影响距离本身。）
    std::sort(out.begin(), out.end(),
                  [](const Candidate& a, const Candidate& b) {
                      if (a.priority != b.priority) return a.priority > b.priority;
                      return a.dist_sq < b.dist_sq;
                  });
        return out;
    }

    // ---- ByteTrack 增强（第4项）实现 ----
    // 卡尔曼匀速模型：状态 [cx,cy,w,h,vx,vy,vw,vh]。
    // 关联参考点 = 上次平滑位置 + 速度（clamp 到 kalman_max_speed_px），
    // 对慢速/匀速目标预测≈当前 → 与原裸框心关联行为兼容（109 用例不受影响）。
    void TargetSelector::kalman_predict(TrackEntry& t, const TargetSelectorConfig& cfg) const {
        const float maxv = cfg.kalman_max_speed_px > 0.0f ? cfg.kalman_max_speed_px : 25.0f;
        auto cv = [maxv](float v) { return v > maxv ? maxv : (v < -maxv ? -maxv : v); };
        t.pred_cx = t.kx + cv(t.vx);
        t.pred_cy = t.ky + cv(t.vy);
    }

    void TargetSelector::kalman_update(TrackEntry& t, const DetectionBox& obs,
                                       const TargetSelectorConfig& cfg, uint32_t now_ms) {
        const float g = cfg.kalman_velocity_gain;  // 速度学习增益
        const float ncx = box_center_x(obs);
        const float ncy = box_center_y(obs);
        const float nw = obs.x2 - obs.x1;
        const float nh = obs.y2 - obs.y1;
        if (t.hits == 0) {
            // 首次命中：直接赋观测（速度=0）
            t.kx = ncx; t.ky = ncy; t.kw = nw; t.kh = nh;
            t.vx = t.vy = t.vw = t.vh = 0.0f;
        } else {
            // 用"预更新前的 innovation"学习速度（对齐 VisionForge：旧代码先纠正再取差导致速度为0）
            t.vx += (ncx - t.kx) * g;
            t.vy += (ncy - t.ky) * g;
            t.vw += (nw - t.kw) * g;
            t.vh += (nh - t.kh) * g;
            // 位置/尺寸向观测收缩（朴素增益，保留历史平滑）
            float pos = g + 0.55f; if (pos > 0.9f) pos = 0.9f;
            t.kx += (ncx - t.kx) * pos;
            t.ky += (ncy - t.ky) * pos;
            t.kw = nw;  // 尺寸直接跟随观测（框尺寸更可信）
            t.kh = nh;
        }
        t.hits++;
        if (t.hits >= 3) t.confirmed = true;
        t.last_seen_ms = now_ms;
        kalman_predict(t, cfg);
    }

    void TargetSelector::trim_tracks(const TargetSelectorConfig& cfg) {
        // 1) 删除丢失帧数超过 buffer 的轨迹（修"只增不删"隐患：轨迹有生有死）
        const uint32_t buf = cfg.track_buffer_frames > 0 ? cfg.track_buffer_frames : 30u;
        if (!tracks_.empty()) {
            tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                         [&](const TrackEntry& t) {
                                             return !t.active && t.lost_frames > buf;
                                         }),
                          tracks_.end());
        }
        // 2) 总轨迹数超上限：裁剪最长未命中（last_seen 最旧）的非激活轨迹
        if (tracks_.size() > cfg.max_tracks) {
            // 活动轨迹永远保留；只对非激活轨迹按 last_seen_ms 升序淘汰
            std::stable_sort(tracks_.begin(), tracks_.end(),
                             [](const TrackEntry& a, const TrackEntry& b) {
                                 if (a.active != b.active) return a.active;  // 活动优先
                                 if (a.lost_frames != b.lost_frames) return a.lost_frames > b.lost_frames;  // 丢失多的先删
                                 return a.last_seen_ms < b.last_seen_ms;      // 更早未见先删
                             });
            while (tracks_.size() > cfg.max_tracks) tracks_.pop_back();
        }
    }

    TargetSelection TargetSelector::select(const std::vector<DetectionBox>& dets,
                                           const TargetSelectorConfig& cfg, uint32_t now_ms) {
    TargetSelection out;
    if (dets.empty() || cfg.roi_w == 0 || cfg.roi_h == 0) {
        // 无检测：激活 track 丢失计数 + 宽限判定
        // 注意：空检测帧立即返回 invalid（安全红线：不允许凭旧坐标产生移动）。
        // "短暂消失保持 target_id"由 AimStateMachine 的 LOST_GRACE 层实现
        // （track 在宽限内不删除，恢复检测后同 id 延续），见 AimStateMachine。
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
                    // 轨迹生命周期：删除丢失超 buffer 的失效轨迹（修"只增不删"）
                    trim_tracks(cfg);
                    last_reason_ = TargetSelection::kNone;
                    return out;
                }

                const float cx = static_cast<float>(cfg.roi_w) * cfg.center_x;
                const float cy = static_cast<float>(cfg.roi_h) * cfg.center_y;
                const float radius = std::min(cfg.roi_w, cfg.roi_h) * 0.5f * cfg.fov_range;
                const float radius_sq = radius * radius;

                // ByteTrack：每帧先对现有轨迹做卡尔曼预测（写入 pred_cx/pred_cy 供关联参考）
                for (auto& t : tracks_) kalman_predict(t, cfg);

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
            trim_tracks(cfg);
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
                    // 关联参考点：ByteTrack 用卡尔曼预测中心（pred_cx/pred_cy）；
                                        // 默认关：用裸框心 → 保持传统最近邻行为（109 用例兼容）。
                                        const float ref_cx = (cfg.use_kalman_predict && at->pred_cx != 0.0f) ? at->pred_cx : at->cx;
                                        const float ref_cy = (cfg.use_kalman_predict && at->pred_cy != 0.0f) ? at->pred_cy : at->cy;
                    // 找离锁定框中心最近的候选
                    const Candidate* best = nullptr;
                    float best_d = match_r_sq;
                    for (const auto& c : cands) {
                        const float d = (c.cx - ref_cx) * (c.cx - ref_cx) +
                                        (c.cy - ref_cy) * (c.cy - ref_cy);
                        if (d < best_d) { best_d = d; best = &c; }
                    }
                    if (best) {
                        // 更新 track（框体 + 卡尔曼状态）
                        at->box = best->box;
                        at->cx = best->cx;
                        at->cy = best->cy;
                        at->lost_frames = 0;
                        kalman_update(*at, best->box, cfg, now_ms);
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
                    const float ref_cx = (cfg.use_kalman_predict && t.pred_cx != 0.0f) ? t.pred_cx : t.cx;
                                        const float ref_cy = (cfg.use_kalman_predict && t.pred_cy != 0.0f) ? t.pred_cy : t.cy;
                    for (const auto& c : cands) {
                        const float d = (c.cx - ref_cx) * (c.cx - ref_cx) + (c.cy - ref_cy) * (c.cy - ref_cy);
                        if (d < match_r_sq && d < best_d) { best_d = d; best_c = &c; best_t = &t; }
                    }
                }
                if (best_c && best_t) {
                    best_t->box = best_c->box;
                    best_t->cx = best_c->cx;
                    best_t->cy = best_c->cy;
                    best_t->lost_frames = 0;
                    best_t->active = true;
                    kalman_update(*best_t, best_c->box, cfg, now_ms);
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
                reuse->lost_frames = 0;
                reuse->active = true;
                kalman_update(*reuse, c.box, cfg, now_ms);
                active_track_ = reuse->id;
                out.box = c.box;
                out.target_id = reuse->id;
            } else {
                // 新建 track（初始化卡尔曼状态）
                TrackEntry nt;
                nt.id = next_id_++;
                nt.box = c.box;
                nt.cx = c.cx;
                nt.cy = c.cy;
                nt.lost_frames = 0;
                nt.active = true;
                nt.kx = c.cx; nt.ky = c.cy;
                nt.kw = c.box.x2 - c.box.x1; nt.kh = c.box.y2 - c.box.y1;
                nt.vx = nt.vy = nt.vw = nt.vh = 0.0f;
                nt.hits = 0;
                nt.created_ms = now_ms;
                kalman_predict(nt, cfg);
                nt.last_seen_ms = now_ms;
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
