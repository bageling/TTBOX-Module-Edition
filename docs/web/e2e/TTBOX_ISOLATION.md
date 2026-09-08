# TTBOX 隔离验证报告

> 验证时间：2026-09-05　板子：192.168.0.53（RK3588）
> 结论：**TTBOX 与 YU 同板运行互不污染（ISOLATION: PASS）**

## 一、验证矩阵

| 检查项 | 要求 | 结果 |
|---|---|---|
| 端口隔离 | TTBOX(8000/8001) ≠ YU(8080) | ✅ PASS |
| 配置隔离-正向 | TTBOX 改配置，YU 不变 | ✅ PASS |
| 配置隔离-反向 | YU 改配置，TTBOX 不变 | ✅ PASS |
| 库隔离 | librga/librknnrt md5 不同 | ✅ PASS |
| 代码隔离 | TTBOX 代码无 yu 实际调用 | ✅ PASS |
| 状态文件 | /opt/ttbox/runtime vs /opt/aiassistance/run | ✅ PASS |
| 缓存隔离 | /var/lib/ttbox vs /var/lib/aiassistance | ✅ PASS |
| 进程隔离 | 各自进程独立运行 | ✅ PASS |
| 服务隔离 | ttbox-*.service vs aiassistance-*.service | ✅ PASS |
| 日志隔离 | 无共享日志文件 | ✅ PASS |

## 二、详细证据

### 1. 端口（实测）
```
8080 → aiassistance-web（YU）
8000 → ttbox-web（TTBOX）
8001 → ttbox-preview（TTBOX）
```

### 2. 配置双向隔离（实测）
```
基线:      YU name=XZN-5FE378 | TT name=TTBox-COMPAT
TTBOX PUT: YU name=XZN-5FE378（不变✅）| TT name=TTBOX-ISOLATION-TEST
YU PUT:    YU name=YU-ISOLATION-TEST | TT name=TTBOX-ISOLATION-TEST（不变✅）
恢复后:    YU=XZN-5FE378 | TT=TTBox-COMPAT
```

### 3. 库隔离（md5）
```
TTBOX librga:   475a9f392731ee5494da196c3b15e6ca
YU    librga:   c700c06c1caff8f135ccdbbde3a86cb5
TTBOX librknnrt: 7fdc8beb1dbe792c781695ba95cd352d
YU    librknnrt: a37ee1d5d664c79836bf6e35b7ef6289
```

### 4. 状态/缓存目录
```
YU:    /opt/aiassistance/run/{daemon.pid,daemon.sock,hdmirx_custom_edid.bin}
       /var/lib/aiassistance/hdmirx_boot_image_edid_slots.json
TTBOX: /opt/ttbox/runtime/edid/
       /var/lib/ttbox/{boot_image_backups,boot_image_edid_slots.json}
```

### 5. 进程（实测）
```
YU:    aiassistance_daemon / aiassistance/web/app.py / makcu_mouse_proxy / wifi_bootstrap
TTBOX: ttbox-web.py / ttbox-preview.py
```

## 三、共享硬件资源（允许共享，见 SHARED_HARDWARE_RESOURCES.md）

| 资源 | 状态 |
|---|---|
| HDMI RX 采集 | 独占（不能同时跑 core） |
| HDMI RX EDID | 驱动级共享（谁后写谁生效） |
| USB 鼠标 proxy | 独占（YU 在用，TTBOX disabled） |
| NPU | 可共享（各自 librknnrt） |
| RGA | 可共享（各自 librga） |

## 四、结论

**ISOLATION: PASS**

TTBOX 与 YU 在以下层面完全隔离：端口、配置、库、代码、状态、缓存、进程、服务、日志。
共享的只有硬件资源（HDMI/NPU/RGA/USB），且已明确记录限制与解决方案，无伪造隔离。
