# TTBOX Platform V1

平台层骨架。当前阶段只建立产品边界与可验证的最小运行契约，不替换现有 `core/` C++ 链路。

## 目录职责

- `runtime/`：Runtime 生命周期与控制契约；算法保持冻结。
- `model/`：模型上传、验证、安装、激活、回滚契约；复用 `core` 的 ModelRegistry。
- `api/`：面向 Web 的 `/api/v1/*` 资源边界。
- `web/`：API Client 边界；不直接操作 systemd 或 Core 内部文件。
- `config/`：Factory → Device → Runtime → Override 配置层级。
- `supervisor/`：服务编排、依赖和恢复契约。
- `health/`：系统、Runtime、模型和设备健康检查契约。
- `update/`：组件 staging、校验、激活、保留旧版本和回滚契约。
- `data/`：运行数据、日志和指标的持久化边界。
- `services/`：部署到目标设备的 systemd/service 适配层。

## 当前状态

- `IMPLEMENTED`：目录边界、生命周期状态模型、组件更新状态模型、配置层级模型。
- `TESTED`：平台契约单元测试（纯 Python 标准库，无外部依赖）。
- `NOT IMPLEMENTED`：HTTP 服务、Web 页面迁移、systemd Supervisor、RK3588 设备适配、真实更新安装。
- `BLOCKED`：Windows 主机没有 g++/MinGW，C++ Core 主机构建需使用现有 MSYS2 工具链或 RK3588 板端；真实 V4L2/RGA/RKNN/HID 必须在板端验证。

自动瞄准相关实现不在本目录修改范围内，保持冻结。
