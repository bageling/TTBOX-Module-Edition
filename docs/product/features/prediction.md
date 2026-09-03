# TTBOX 预测功能 (Prediction)

## 产品目标

在目标跟踪的基础上，预测目标的下一个位置，减少瞄准延迟。

## 用户体验

用户开启预测功能后，AI 瞄准不再仅跟踪目标的当前位置，而是根据历史轨迹预判目标下一步位置，实现"提前量"瞄准。

## 当前状态

- 核心已有 AlphaBetaGammaFilter 滤波算法（`core/src/aim/AlphaBetaGammaFilter.hpp`）
- 该算法在 AimThread 中已使用（`AimThread.cpp` 的 `AimStateMachine` 中）
- 缺少独立的预测参数配置和 Web UI 控制

## 依赖

- AlphaBetaGammaFilter（核心已实现）
- AimThread（核心已实现）

## 完成标准

1. 预测参数可在 Web 页面配置
2. 预测行为可启用/禁用
3. 预测效果可观测（延迟降低）

## 参考

历史产品设计中存在 Prediction 相关参数，可作为参考默认值。