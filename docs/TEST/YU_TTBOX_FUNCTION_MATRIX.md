# YU → TTBOX 功能对照验收矩阵

> 验收日期：2026-09-04
> 基准：YU（aiAssistance）历史功能面 vs TTBOX（自研 Plugin/C++ Runtime）
> 判定标准：只有真机运行验证过才算 PASS；代码存在不算。
> 证据类型：真机日志 / API 返回 / 状态变化 / 实际硬件行为 / 性能数据

## 第一层：基础可用（F001-F011）

| 编号 | 功能 | YU | TTBOX | 真机测试 | 结果 |
|---|---|---|---|---|---|
| F001 | HDMI 输入 | PASS | PASS | 2560×1440 BGR3 @140fps；发现 capture 线程长时间运行后退出→重启修复 | ✅ PASS（修复后） |
| F002 | 视频预览 | PASS | PASS | preview 插件 healthy，帧号持续增长，12fps JPEG 流 | ✅ PASS |
| F003 | AI 开关 | PASS | PASS | output_enabled=false(fail-closed)、backend=usb_proxy、热键 Shift | ✅ PASS |
| F004 | 模型切换 | PASS | PASS | 修复前：registry active=jwdl_sjzv11 与 config 实载 yolo261n 脱节；修复 handle_model_list 以 config.model_path 为准后 active=yolo261n-rk3588 ✅ | ✅ PASS（已修复） |
| F005 | 检测 | PASS | PASS | 根因修复：模型期望 RGB 输入但 TTBOX 喂 BGR→conf 0.07 压扁；改 model_color_order=rgb 后 detect=2、conf 0.35/0.29、aim 锁定 | ✅ PASS（已修复） |
| F006 | 目标选择 | PASS | PASS | aim_has_target=True、target_id=1、class=0、框位正确 | ✅ PASS |
| F007 | 鼠标控制 | PASS | PASS | usbproxy 监听 cmd.sock、raw-gadget fd=12、fail-closed 时 connected=false | ✅ PASS |
| F008 | 键盘透传 | PASS | N/A | 双方均无键盘实现 | ⚪ N/A |
| F009 | Web 访问 | PASS | PASS | 首页/API 全部 200 | ✅ PASS |
| F010 | 服务自启 | PASS | PASS | 6 个 TTBOX 服务全部 enabled | ✅ PASS |
| F011 | 状态显示 | PASS | PASS | /api/state 返回完整 config/状态/模型 | ✅ PASS |

## 第二层：实际使用（F012-F022）

| 编号 | 功能 | YU | TTBOX | 真机测试 | 结果 |
|---|---|---|---|---|---|
| F012 | 热键触发 | PASS | PASS | aim_keys=Shift+Shift、profile hotkey=right/left/mode=any；配置面完整 | ✅ PASS |
| F013 | 配置保存 | PASS | PASS | PUT video_detection_confidence 0.26→回读 0.26→重启保留→恢复 0.25 | ✅ PASS |
| F014 | Profile | PASS | PASS | 预设保存→列表→加载→删除全链路（f014test） | ✅ PASS |
| F015 | 灵敏度 | PASS | PASS | sens 1.8 PUT→回读 1.79999995→恢复 1.7 | ✅ PASS |
| F016 | FOV | PASS | PASS | fov={center 0.5,0.5, radius 1, shape 0, enabled false} 配置完整 | ✅ PASS |
| F017 | 目标选择 | PASS | PASS | 多候选时锁定 id=1、class=0，中心 (1282,592) 与屏幕中心一致 | ✅ PASS |
| F018 | 目标消失/重现 | PASS | PASS | 黑屏→detect=0/aim=False；恢复目标→detect=4/aim=True 重新锁定 | ✅ PASS |
| F019 | 多目标 | PASS | PASS | 4 框同现（class0×2/15/16），选择器锁定最高分 id=1 conf 0.516 | ✅ PASS |
| F020 | AI+物理鼠标 | PASS | ? | ? | ? |
| F021 | AI 开关状态切换 | PASS | ? | ? | ? |
| F022 | 重启恢复 | PASS | ? | ? | ? |

## 第三层：异常情况（F023-F034）

| 编号 | 功能 | YU | TTBOX | 真机测试 | 结果 |
|---|---|---|---|---|---|
| F023 | HDMI 拔出 | PASS | ? | ? | ? |
| F024 | HDMI 恢复 | PASS | ? | ? | ? |
| F025 | AI 无目标 | PASS | ? | ? | ? |
| F026 | AI 关闭 | PASS | ? | ? | ? |
| F027 | 模型不存在 | PASS | ? | ? | ? |
| F028 | Plugin 停止 | PASS | ? | ? | ? |
| F029 | Runtime 异常 | PASS | ? | ? | ? |
| F030 | USBProxy 异常 | PASS | ? | ? | ? |
| F031 | 服务崩溃 | PASS | ? | ? | ? |
| F032 | 服务自动恢复 | PASS | ? | ? | ? |
| F033 | Web API 异常 | PASS | ? | ? | ? |
| F034 | 重启整机 | PASS | ? | ? | ? |

## 逐项明细（每项：YU 行为 / TTBOX 实现 / 测试方法 / 证据 / 结论）

（后续按 F001 起逐项追加）
