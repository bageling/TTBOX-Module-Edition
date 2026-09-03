#include "mouse/TargetSelector.hpp"
#include <cstdio>
#include <vector>
using namespace ttbox::core::aim;
using ttbox::core::DetectionBox;

int main() {
    TargetSelector sel;
    TargetSelectorConfig cfg;
    cfg.fov_range = 1.0f;
    cfg.confidence = 0.25f;
    cfg.roi_w = 320; cfg.roi_h = 320;
    cfg.lost_grace_ms = 30.0f;

    // 目标1 框(100,100)-(140,180) 中心(120,140) 距画面中心(160,160)=44.7px 对角89px
    // 目标2 框(210,210)-(245,285) 中心(227.5,247.5) 距画面中心=112px 对角~81px
    // 目标2 距目标1中心 = √(107²+107²)=151px > 目标1对角89 → track_lock 不误匹配
    // 场景1：单目标 → score 新建 track
    {
        std::vector<DetectionBox> dets = {{100,100,140,180,0.9f,5}};
        auto r = sel.select(dets, cfg, 1000);
        printf("S1: valid=%d tid=%d reason=%d lock=%.1f (期望: 1 tid=1 reason=3 score)\n",
               r.valid, r.target_id, (int)r.reason, r.lock_radius);
    }
    // 场景2：同目标下一帧 → track_lock
    {
        std::vector<DetectionBox> dets = {{102,102,142,182,0.9f,5}};
        auto r = sel.select(dets, cfg, 1007);
        printf("S2: valid=%d tid=%d reason=%d (期望: tid=1 reason=1 track_lock)\n",
               r.valid, r.target_id, (int)r.reason);
    }
    // 场景3：出现第二个目标（更远），仍锁原目标 → track_lock
    {
        std::vector<DetectionBox> dets = {{100,100,140,180,0.9f,5},{210,210,245,285,0.9f,6}};
        auto r = sel.select(dets, cfg, 1014);
        printf("S3: valid=%d tid=%d reason=%d (期望: tid=1 保持 track_lock)\n",
               r.valid, r.target_id, (int)r.reason);
    }
    // 场景4：原目标丢失1帧 → 宽限内保持（track_lock 用旧框）
    {
        std::vector<DetectionBox> dets = {{210,210,245,285,0.9f,6}};
        auto r = sel.select(dets, cfg, 1021);
        printf("S4: valid=%d tid=%d reason=%d (期望: tid=1 宽限保持)\n",
               r.valid, r.target_id, (int)r.reason);
    }
    // 场景5：原目标恢复 → track_lock
    {
        std::vector<DetectionBox> dets = {{104,104,144,184,0.9f,5},{210,210,245,285,0.9f,6}};
        auto r = sel.select(dets, cfg, 1028);
        printf("S5: valid=%d tid=%d reason=%d (期望: tid=1 恢复 track_lock)\n",
               r.valid, r.target_id, (int)r.reason);
    }
    // 场景6：原目标丢失超宽限(>4帧=28ms) → 切到第二个目标
    {
        std::vector<DetectionBox> dets = {{210,210,245,285,0.9f,6}};
        TargetSelection r;
        for (int i=0;i<8;i++) {   // 8帧=56ms > 30ms宽限
            r = sel.select(dets, cfg, 1035 + i*7);
        }
        printf("S6: valid=%d tid=%d reason=%d (期望: 宽限耗尽切到 tid=2)\n",
               r.valid, r.target_id, (int)r.reason);
    }
    // 场景7：第二目标持续锁定 → track_lock
    {
        std::vector<DetectionBox> dets = {{212,212,247,287,0.9f,6}};
        auto r = sel.select(dets, cfg, 1090);
        printf("S7: valid=%d tid=%d reason=%d (期望: tid=2 track_lock)\n",
               r.valid, r.target_id, (int)r.reason);
    }
    return 0;
}
