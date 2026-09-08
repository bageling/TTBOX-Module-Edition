# YU 功能总表（行为基准）

> 依据：**真实运行中的 YU**（192.168.0.53:8080）+ `/opt/aiassistance/web/app.py` 路由源码
> 基准日期：2026-09-05　YU 版本：2026.08.03.1
> 规则：所有状态文字/API 返回/HTTP code 以真实 YU 为准，TTBOX 必须原样复刻。

## YU 环境快照

| 项 | 值 |
|---|---|
| IP:Port | 192.168.0.53:8080 |
| app_version | 2026.08.03.1 |
| ui_brand | yu |
| 前端模板 | /opt/aiassistance/web/templates/index.html（12 页面） |
| 核心版本 | 2026.05.16（core.status=loaded） |
| API 总数 | 100（@app 路由计数） |

## YU 页面清单（12 页）

| 页面 id | 页面 | 主要功能 |
|---|---|---|
| home-page | 总览 | 实时画面/运行数据/启动控制/基础检测设置/硬件状态/电源控制 |
| profiles-page | 热键控制 | 热键与类别/新增热键/一键禁用 |
| control-page | 移动控制 | PID 控制器/校准/瞄准行为 |
| assist-page | 辅助功能 | 辅助开关/瞄准辅助 |
| model-page | 模型库 | 本地模型/导入/选择/远端 |
| hardware-page | 显示器与鼠标 | 显示器模式(EDID)/USB鼠标 |
| wifi-page | 网络配置 | 无线网络/热点/网络访问 |
| preset-page | 预设参数 | 预设管理 |
| license-page | 激活 | 激活设备 |
| fan-page | 风扇 | 风扇控制 |
| kmbox-page | kmbox | kmbox 设备 |
| hailo-page | Hailo | Hailo 加速器 |

## 核心 API 清单（100 个路由，按功能域分组）

### 系统/状态
| API | Method | 状态 |
|---|---|---|
| /api/state | GET | ✅ 抓取（8806B） |
| /api/system | GET | ✅ 抓取（1518B） |
| /api/health/frontend | GET | ✅ |
| /api/license | GET | ✅ 抓取（2047B） |

### 激活
| API | Method |
|---|---|
| /api/activation/network/prepare | POST |
| /api/activation/reset-local-identity | POST |
| /api/activation/full-recovery | GET/POST |
| /api/license/activate | POST |
| /api/system/reactivate | POST |
| /api/system/master-reactivate | POST |

### 更新
| API | Method |
|---|---|
| /api/update/check | POST |
| /api/update/versions | POST |
| /api/update/status | GET ✅ 抓取（206B） |
| /api/update/install | POST |
| /api/update/cleanup-stuck | POST |

### 系统设置
| API | Method |
|---|---|
| /api/system/storage | GET |
| /api/system/storage/expand | POST |
| /api/system/hostname | PUT |
| /api/system/web-port | PUT |
| /api/system/lan-blocklist | GET/POST/DELETE |
| /api/system/lan-blocklist/scan | POST |
| /api/system/reboot | POST |
| /api/system/poweroff | POST |
| /api/settings/auto-start | GET ✅ 抓取 / PUT |

### 网络
| API | Method |
|---|---|
| /api/network/wifi | GET ✅ 抓取 |
| /api/network/wifi/scan | POST |
| /api/network/wifi/connect | POST |
| /api/network/wifi/fallback | POST |
| /api/network/wifi/ap/apply | POST |
| /api/network/wifi/client/activate | POST |

### 配置/预设
| API | Method |
|---|---|
| /api/config | PUT |
| /api/presets | GET ✅ 抓取 / POST |
| /api/presets/load | POST |
| /api/presets/import | POST |
| /api/presets/<name>/export | GET |

### 运动档案
| API | Method |
|---|---|
| /api/motion-profiles | GET ✅ 抓取 / POST |
| /api/motion-profiles/<id> | PATCH/DELETE |
| /api/motion-profiles/<id>/export | GET |
| /api/motion-profiles/<id>/train | POST |
| /api/motion-profiles/<id>/activate | POST |
| /api/motion-profiles/active | DELETE |
| /api/motion-profiles/<id>/samples | DELETE |
| /api/motion-training/sessions | POST |
| /api/motion-training/sessions/<id>/heartbeat | PUT |
| /api/motion-training/sessions/<id>/samples | POST |
| /api/motion-training/sessions/<id> | DELETE |

### 控制/校准
| API | Method |
|---|---|
| /api/control/start | POST |
| /api/control/stop | POST |
| /api/control/calibration | GET ✅ 抓取 / PUT / DELETE |
| /api/control/calibration/start | POST |
| /api/control/calibration/cancel | POST |
| /api/diagnostics/aim-trace | POST |
| /api/diagnostics/usb-proxy.zip | GET |

### 硬件（显示器/鼠标）
| API | Method |
|---|---|
| /api/hardware/display | GET ✅ 抓取 / PUT |
| /api/hardware/mouse | GET ✅ 抓取 / PUT |
| /api/hardware/mouse/mode | PUT |
| /api/hardware/mouse/timing | PUT |

### 模型
| API | Method |
|---|---|
| /api/models | GET ✅ 抓取（56B, models=[]） |
| /api/models/device-code | GET |
| /api/models/cloud-encrypted | POST |
| /api/models/import | POST |
| /api/models/delete | POST |
| /api/models/select | POST |
| /api/models/bind-preset | POST |
| /api/models/game-profile | POST |
| /api/models/remote-frame-format | POST |
| /api/models/rknn-concurrency | POST |
| /api/models/hailo-pipeline-depth | POST |
| /api/models/class-names | POST |

### 远端
| API | Method |
|---|---|
| /api/remote/connect | POST |
| /api/remote/models | GET |
| /api/remote/import | POST |
| /api/remote/delete | POST |

### 其他
| API | Method |
|---|---|
| /api/announcement | GET（真实 503） |
| /api/themes | GET / POST |
| /api/themes/<id>/previews/<idx> | GET |
| /api/themes/redeem | POST |
| /api/themes/<id>/install | POST |
| /api/themes/current | PUT |
| /api/xcsh/background | GET/POST/PATCH/DELETE |
| /api/xcsh/background/image | GET |
| /api/hailo/status | GET ✅ 抓取 |
| /api/hailo/install | POST |
| /api/makcu/devices | GET ✅ 抓取（34B） |
| /api/ferrum/devices | GET ✅ 抓取 |
| /api/kmboxb/devices | GET ✅ 抓取 |
| /api/mouse-output/test-circle | POST |
| /api/events | GET |
| /api/preview.jpg | GET |
| /api/preview.mjpg | GET |
