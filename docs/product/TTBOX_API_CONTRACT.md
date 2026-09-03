# TTBOX API 契约

## 架构

```
TTBOX Web UI
    ↓ HTTP API
TTBOX Gateway (scripts/ttbox_gateway.py)
    ↓ Unix Socket IPC
TTBOX IPC (core/src/ipc/IpcServer.cpp)
    ↓
TTBOX Core (C++)

TTBOX Web UI
    ↓ HTTP API
TTBOX Update Engine (/var/run/ttbox/update.sock)
    ↓ Unix Socket IPC
TTBOX Update Engine (C++/Python)
```

## API 命名规范

- 所有 API 路径以 `/api/` 开头
- 资源名称使用复数形式：`/api/models`、`/api/presets`
- 操作通过 HTTP 方法区分：GET（查询）、POST（创建）、PUT（更新）、DELETE（删除）
- 响应格式统一：`{ ok: bool, data: object, error?: string }`

## API 分组

### Runtime（运行时）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/state` | 获取完整运行状态 | 🟢 |
| POST | `/api/control/start` | 启动 AI 流水线 | 🟢 |
| POST | `/api/control/stop` | 停止 AI 流水线 | 🟢 |

### Models（模型管理）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/models` | 列出所有模型 | 🟢 |
| POST | `/api/models/import` | 导入模型 | 🟢 |
| POST | `/api/models/delete` | 删除模型 | 🟢 |
| POST | `/api/models/select` | 切换模型（自动重启 AI） | 🟢 |
| POST | `/api/models/class-names` | 编辑类别名 | 🟢 |
| POST | `/api/models/rknn-concurrency` | 设置 NPU 并发 | 🟢 |
| POST | `/api/models/bind-preset` | 绑定预设 | 🟢 |
| POST | `/api/models/game-profile` | 设置游戏档案 | 🟢 |

### Config（配置）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/config` | 获取当前配置 | 🟢 |
| PUT | `/api/config` | 更新配置 | 🟢 |

### Presets（预设）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/presets` | 列出所有预设 | 🟢 |
| POST | `/api/presets` | 保存/删除/重命名预设 | 🟢 |
| POST | `/api/presets/load` | 加载预设 | 🟢 |

### Preview（预览）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/preview.jpg` | 获取单帧预览 | 🟢 |
| GET | `/api/preview.mjpg` | 获取 MJPEG 预览流 | 🟢 |

### Display（显示器）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/hardware/display` | 获取显示器信息 | 🟢 |
| PUT | `/api/hardware/display` | 更新显示器配置 | 🟢 |

### Mouse（鼠标）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/hardware/mouse` | 获取鼠标硬件信息 | 🟢 |
| PUT | `/api/hardware/mouse` | 更新鼠标配置 | 🟢 |

### Network（网络）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/network/wifi` | 获取 Wi-Fi 状态 | 🟢 |
| POST | `/api/network/wifi/scan` | 扫描 Wi-Fi | 🟢 |
| POST | `/api/network/wifi/connect` | 连接 Wi-Fi | 🟢 |
| POST | `/api/network/wifi/fallback` | 回退默认 Wi-Fi | 🟢 |
| POST | `/api/network/wifi/ap/apply` | 启动热点模式 | 🟢 |
| POST | `/api/network/wifi/client/activate` | 切回客户端模式 | 🟢 |

### System（系统）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/system` | 获取系统状态 | 🟢 |
| GET | `/api/system/version` | 获取版本信息 | 🟢 |
| GET | `/api/system/storage` | 获取存储信息 | 🟢 |
| PUT | `/api/system/hostname` | 修改主机名 | 🟢 |
| POST | `/api/system/reboot` | 重启系统 | 🟢 |
| POST | `/api/system/poweroff` | 关机 | 🟢 |
| POST | `/api/system/lan-blocklist/scan` | 扫描局域网设备 | 🟢 |
| GET | `/api/settings/auto-start` | 获取自启设置 | 🟢 |
| PUT | `/api/settings/auto-start` | 更新自启设置 | 🟢 |

### Update（更新）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/update/status` | 获取更新状态 | 🟢 |
| POST | `/api/update/check` | 检查 OTA 更新 | 🟢 |
| POST | `/api/update/scan-otg` | 扫描 USB 更新 | 🟢 |
| POST | `/api/update/start` | 开始更新指定版本 | 🟢 |
| POST | `/api/update/rollback` | 回滚到上一版本 | 🟢 |
| POST | `/api/update/cancel` | 取消更新 | 🟢 |
| GET | `/api/update/log` | 获取更新日志 | 🟢 |

### Calibration（自动标定）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/control/calibration` | 获取 TTBOX 标定状态、候选目标、分轴拟合结果和最终参数 | 🟡 VERIFY（真实目标场景待验收） |
| POST | `/api/control/calibration/start` | 启动真实自动标定状态机 | 🟡 VERIFY |
| POST | `/api/control/calibration/cancel` | 取消当前标定并恢复临时配置 | 🟢 |
| PUT | `/api/control/calibration` | 用户明确保存手动 X/Y 响应和延迟 | 🟢 |
| DELETE | `/api/control/calibration` | 清除已保存标定结果 | 🟢 |

自动标定状态：`idle → preparing → stabilize_x → sampling_x → analyzing_x → stabilize_y → sampling_y → analyzing_y → validating → applying → completed`；任意阶段可进入 `cancelled/failed`。

### Motion Training（个人移动曲线）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET/POST/PATCH/DELETE | `/api/motion-profiles*` | TTBOX 本地档案查询、创建、重命名、删除、导出 | 🟢 |
| POST/PUT/DELETE | `/api/motion-training/sessions*` | 训练租约、心跳、样本上传、结束会话 | 🟢 |
| POST | `/api/motion-profiles/<id>/train` | 根据真实样本生成 TTBOX 模型 | 🟢 |
| POST/DELETE | `/api/motion-profiles/<id>/activate`、`/active` | 启用/停用个人模型并写入 Core RuntimeProfile | 🟡 VERIFY（真实 HID 效果待验收） |

### Devices（设备枚举）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/makcu/devices` | 枚举 MAKCU 设备 | 🟢 |
| GET | `/api/ferrum/devices` | 枚举 Ferrum 设备 | 🟢 |
| GET | `/api/kmboxb/devices` | 枚举 KmboxB 设备 | 🟢 |

### License（授权）

| 方法 | 路径 | 描述 | 状态 |
|------|------|------|------|
| GET | `/api/license` | 获取授权信息 | 🟢 |

## IPC 协议

### 通信方式

- Unix Domain Socket：`/tmp/ttbox_core.sock`（Core）
- Unix Domain Socket：`/var/run/ttbox/update.sock`（Update Engine）
- 请求格式：JSON 文本行，以 `\n` 结尾
- 响应格式：JSON 文本行，以 `\n` 结尾

### 请求格式

```json
{
  "type": "COMMAND",
  "params": { ... }
}
```

### 响应格式

```json
{
  "type": "COMMAND",
  "status": 0,
  "data": { ... },
  "error": ""
}
```

### 命令列表

#### Core IPC

| 命令 | 描述 | 状态 |
|------|------|------|
| GET_STATUS | 获取运行状态 | 🟢 |
| GET_CONFIG | 获取配置 | 🟢 |
| SET_CONFIG | 更新配置 | 🟢 |
| RUNTIME_CONTROL | 启动/停止 AI | 🟢 |
| GET_PREVIEW | 获取预览帧 | 🟢 |
| MODEL_LIST | 列出所有模型 | 🟢 |
| MODEL_IMPORT | 导入模型 | 🟢 |
| MODEL_VALIDATE | 校验模型 | 🟢 |
| MODEL_INSTALL | 安装模型 | 🟢 |
| MODEL_ACTIVATE | 激活模型 | 🟢 |
| MODEL_REMOVE | 删除模型 | 🟢 |
| PING | 心跳检测 | 🟢 |

#### Update Engine IPC — 新增

| 命令 | 描述 | 状态 |
|------|------|------|
| GET_STATUS | 获取更新状态 | 🔵 PLANNED |
| CHECK_UPDATE | 检查 OTA 更新 | 🔵 PLANNED |
| SCAN_OTG | 扫描 USB 更新 | 🔵 PLANNED |
| START_UPDATE | 开始更新 | 🔵 PLANNED |
| CANCEL_UPDATE | 取消更新 | 🔵 PLANNED |
| ROLLBACK | 回滚 | 🔵 PLANNED |
| GET_LOG | 获取更新日志 | 🔵 PLANNED |

## 响应格式规范

### 成功响应

```json
{
  "ok": true,
  "data": { ... }
}
```

### 错误响应

```json
{
  "ok": false,
  "error": "错误描述"
}
```

## 状态码规范

| 状态码 | 含义 |
|--------|------|
| 0 | 成功 |
| 1 | 参数错误 |
| 2 | 资源不存在 |
| 3 | 内部错误 |
| 4 | 更新进行中 |
| 5 | 更新不可用 |

## 版本控制

- API 版本通过 `app_version` 字段标识
- 当前版本：`0.1.0`
- 不兼容变更时升级主版本号