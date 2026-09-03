# 2026-09-02 夜间开发报告

## 结论

本夜完成了 YU→TTBOX 全量差异基线、P0 链路盘点和一处真实假成功修复。TTBOX 当前仍不是 YU 的完整产品等价体；核心 AI 运行稳定，但 Hotkey/HID 实际闭环、目标选择行为、部分 Movement/Assist 消费和危险副作用流程仍需专项验证。

## 本夜完成

1. 检查仓库是否存在 `YU_FUNCTION_MAP.md`、`YU_COMPLETE_PRODUCT_SPEC.md`：未找到。
2. 采用已有 `TTBOX_YU_REFERENCE_MAP.md`、Web 功能真值表、当前源码和 RK3588 实际状态建立矩阵。
3. 真机读取：Core/Web active，YU inactive；capture≈142.58 FPS，inference≈106.79 FPS，detections=1，tracks=1，class=3。
4. 发现并修复远程模型接口假成功：不再返回 `mock-remote`、空模型列表或“已导入/已删除”。现在明确返回 HTTP 501 / `status=planned`。
5. 发现并修复鼠标圆周测试假成功：不再返回“测试已开始”，当前明确返回 HTTP 501 / `status=planned`，且不发送任何动作。
6. 发现并修复云加密模型与 Web 端口假成功：分别明确返回 HTTP 501，不写入模型仓库、不修改监听配置。
7. 生成 `YU_TARGET_OBJECT_COMPARISON.md`，将 YU 与 TTBOX 的 Detection、TrackedTarget、TargetPoint、Candidate、CandidateRect、OverlayBox 分层对照。
8. 同步真机并验证 `ttbox-web.service` active，4 个受影响接口均真实回读 HTTP 501。
9. Python、JavaScript、`git diff --check` 通过。

## 结果统计（按当前可枚举能力粗粒度）

- YU 功能总数：约 20 个能力组（矩阵中逐项拆分）
- TTBOX 已等价：已确认的状态/预览/基础配置/API/模型本地管理能力约 6 组
- TTBOX 部分等价：Movement、Assist、Profiles、Hardware、Presets、System、Preview、Update 约 10 组
- TTBOX 缺失或未接入：远程模型协议、YU 全局热键守护、YU native Target Object 等约 3 组
- 本夜已修复：1 组（远程模型假成功改为诚实 planned）
- 无法验证：需要真实输入/鼠标/屏幕观察或危险副作用的能力约 10 组

上述统计按能力组计算，不把页面数量冒充产品功能数量。

## 关键差距

### P0

- `DetectionBox → Target Object/CandidateRect`：TTBOX 仍是单个 selected DetectionBox；当前真机只有一个约 65×250 的 class 3 框，不能安全生成完整人体框。
- Hotkey → AimThread → output → 实际 HID：代码有 Gate，但真机消费者路径仍需确认。
- TargetSelector 双目标、交叉、遮挡、短暂消失：缺少可重复真实场景证据。
- Movement/Assist 参数：大部分可保存回读，但若无真实输出变化证据，只标 VERIFY。

### P1

- Profiles 完整切换和重启保持。
- 模型切换后实际 Runtime reload。
- Presets 完整导入/导出/重命名回归。
- Hardware/Preview/System/Update 分项真实闭环。
- 性能 skip/drop/jitter 长时差值测量。

## 构建与环境

- 本地 `core/build` 不存在。
- Windows 本机没有 `g++`。
- 全量 `pytest -q` 被仓库目录 `platform` 遮蔽 Python 标准库，并误收集 Unix-only `yu-backend/ipc_test.py`；该结果标记为测试环境问题，未修改业务代码。
- Web Python/JS 语法通过。

## 本轮文件

新增：

```text
docs/nightly/YU_TTBOX_GAP_MATRIX.md
docs/nightly/2026-09-02-yu-compat-test.md
docs/nightly/2026-09-02-nightly-report.md
```

修改：

```text
scripts/ttbox_web.py
```

## 未执行

- 未重启整机。
- 未修改网络、SSH、防火墙、内核/BSP。
- 未执行 OTA 安装/回滚、重启/关机。
- 未修改前端 CSS、识别框放大、坐标缩放。
- 未修改 TTBOX Core。
- 未启动或接入 YU daemon。

## 当前状态

```text
产品功能完成度：部分完成，差异矩阵已建立
核心链路完成度：Core运行链稳定，Target Object/HID行为闭环仍有差距
真实硬件完成度：Core/Web真实运行已验证；鼠标最终效果待专项验证
性能：capture≈142.58 FPS，inference≈106.79 FPS；需长时差值测试
稳定性：服务 active；目标当前单框、单 track
```
