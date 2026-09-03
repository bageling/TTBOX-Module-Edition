# AI Handoff — TTBOX 项目交接文档

## 项目定位

TTBOX 是**独立产品**，不是 YU 的兼容版，不是 AIBox 的复刻版。

历史项目（YU / AIBox）只提供产品设计参考，不参与 TTBOX 架构定义。

## 核心原则

1. **TTBOX 拥有自己的：**
   - Product Blueprint（产品蓝图）
   - Feature Matrix（功能矩阵）
   - Domain Model（领域模型）
   - API Contract（API 契约）
   - Core Architecture（核心架构）
   - Runtime Model（运行时模型）

2. **禁止以下行为：**
   - 自行复制历史产品 API
   - 自行复制历史产品字段
   - 增加临时兼容层作为永久方案
   - 通过假成功隐藏未实现功能
   - 绕过 Domain Model 直接修改 Core
   - 因为当前没有实现就删除产品规划

3. **新功能开发流程：**
   ```
   用户需求 → 判断是否为 TTBOX Feature → 建立/更新 Feature Spec → 更新 Product Blueprint → 更新 Domain Model → 更新 API Contract → 实现 Core → 实现 Gateway → 连接 Web → 真机测试 → Feature Matrix 更新
   ```

## Phase 8.4 自动标定交接

- 行为模型：`docs/architecture/TTBOX_CALIBRATION_BEHAVIOR_MODEL.md`
- 参考映射：`docs/architecture/TTBOX_YU_REFERENCE_MAP.md`
- TTBOX 自动标定按自己的 Calibration Domain 实现：Core 输出目标 ID、类别、中心和框尺寸；Gateway 使用显式状态机执行分轴采样、Median/MAD 拟合、验证和应用。
- 当前真机状态：`ttbox-core`、`ttbox-web`、`ttbox-release-manager` 均 active；本阶段仅重启对应 TTBOX 应用服务，没有执行整机重启。
- 当前真实验证边界：无 HDMI 真实目标时，自动标定完整动作链只能验证状态读取、真实拒绝和取消恢复；完整 `completed` 仍需真实目标场景。
- “已保存”根因已复现：普通控件一次操作只产生一个 `PUT /api/config`，自动标定轮询不调用成功 Toast；自动启动使用状态徽标，手动参数保存才显示保存提示。

## 参考资料

历史项目文档存放在 `yu-backend/` 目录，仅用于：
- 功能需求参考
- 设计思路参考
- 参数默认值参考

## 读取顺序

新的 AI 接手后，按以下顺序阅读：

1. `docs/AI_HANDOFF.md`（本文档）
2. `docs/product/TTBOX_PRODUCT_BLUEPRINT.md`（产品蓝图）
3. `docs/product/TTBOX_FEATURE_MATRIX.md`（功能矩阵）
4. `docs/product/TTBOX_DOMAIN_MODEL.md`（领域模型）
5. `docs/product/TTBOX_API_CONTRACT.md`（API 契约）
6. `README.md`（项目概述）
7. `docs/TTBOX_CODE_MAP_CN.md`（代码地图）