# TTBOX 热键（02 热键页 / profiles-page）

> 最后更新：2026-09-01（Phase 8.2 交付，Phase 8.3 建档）
> 血缘审查：Phase 8.2 热键能力代码血缘审查（commit 7f70657）

## 产品目标

热键页管理「AI 什么时候动」：瞄准主/副热键、触发方式（任一/同时）、目标类别筛选、移动倍率、X/Y 瞄准点偏移，以及物理按键屏蔽列表。

## 数据流

```
浏览器 → PUT /api/config
  → Gateway（热键字符串 ↔ 位掩码）
  → RuntimeProfile.mouse.aim_hotkey / aim_hotkey2 / aim_hotkey_mode
  → AimThread 每帧快照重读（改配置即时生效，无需重启）
  → OutputBackend 发送前 Gate（fail-closed）
```

## 关键实现事实

1. 热键位图：左1、右2、中4、侧1 8、侧2 16；主/副热键 + any/all 模式。
2. AimThread 每周期重读配置快照，热键修改即时生效。
3. OutputBackend 发送前重新判定热键，配置缺失禁止注入（fail-closed）。
4. 目标类别 → RuntimeProfile.inference.class_filter（NMS 之后过滤）。

## 已知边界（诚实标注）

- 热键守护（全局禁用）：Core 未提供，控件禁用 PLANNED。
- 热键 FOV 缩放 / 偏移切换（备用 X/Y）：Core 无消费字段，PLANNED。

## 验证记录（Phase 8.2 浏览器闭环）

主/副热键读取修改保持恢复、目标类别全选/单选、移动倍率 1.70→1.23→恢复、X/Y 偏移修改保持恢复：全部 REAL（详见 commit 3934002）。
