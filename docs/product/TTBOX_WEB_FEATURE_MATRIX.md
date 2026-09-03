# TTBOX Web 功能真值表

> 版本：Phase 8 初稿（2026-09-01）
> 诚实原则：'真机'列区分 API 核验（curl 返回真实数据）与 浏览器闭环（读→改→保存→重读→设备生效）。
> 本表初稿基于：代码扫描 + 真机 API 端点核验。页面级浏览器闭环逐页进行，未完成前不标 REAL。
> 状态定义：
> - **REAL** — 真机浏览器实际操作验证通过（读→改→保存→重读→设备生效）
> - **PARTIAL** — 前端/API/后端部分接通，但尚未完成真机闭环
> - **PLANNED** — 界面存在，真实能力尚未实现，保持占位，不伪造
> - **RESERVED** — 未来预留（云端等），本阶段不开发
> - **BROKEN** — 已实现但损坏/未接通
> - **VERIFY** — 代码已接通，缺对应硬件/环境，需后续真机确认

页面 → 功能 → UI → apiClient → Gateway → Core/OS → 真机 → 状态

---

## 01 总览（home-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 设备激活 | licenseKeyInput / activateLicenseButton | GET /api/license / POST /api/license/activate | ✅ | 授权状态文件 | API已核验；浏览器未操作 | VERIFY |
| 刷新激活状态 | refreshLicenseButton | GET /api/license | ✅ | ✅ | API已核验；浏览器未操作 | VERIFY |
| 实时预览 | previewImage | GET /api/preview.mjpg | ✅ | IPC GET_PREVIEW | 浏览器真实加载，320×320 | REAL |
| 链路延迟 | mobileLatency | GET /api/state → latency | ✅ | IPC metrics | 浏览器显示 11.24ms | REAL |
| 采集帧率 | mobileCaptureFps | GET /api/state → capture | ✅ | IPC metrics | 浏览器显示真实值 | REAL |
| 检测帧率 | mobileFps | GET /api/state → detection | ✅ | IPC metrics | 浏览器显示真实值 | REAL |
| 目标状态 | mobileVideoStatus | GET /api/state | ✅ | ✅ | 浏览器显示真实值 | REAL |
| 运行摘要（模型/推理/排队/预处理/错误） | runtimeSummary | GET /api/state | ✅ | IPC metrics | 浏览器显示真实摘要 | REAL |
| 启动/停止 | startButton | POST /api/control/start / stop | ✅ | IPC RUNTIME_CONTROL | 浏览器停止→启动，状态恢复 running | REAL |
| 开机自启动 | data-auto-start-toggle | GET/PUT /api/settings/auto-start | ✅ | systemd | 浏览器读改写成功并恢复；未执行重启 | VERIFY |
| 截取尺寸 | capture_crop_size | GET/PUT /api/config | ✅ | RuntimeProfile | 浏览器 320→416→320 | REAL |
| FOV 半径 | range_factor | GET/PUT /api/config | ✅ | ✅ | 浏览器 0.62→1.0 | REAL |
| 截取位置偏移 X/Y | capture_crop_offset_x/y | GET/PUT /api/config | ✅ | ✅ | 浏览器显示 0；未改写 | VERIFY |
| 检测置信度/IOU | video_detection_confidence/iou | GET/PUT /api/config | ✅ | ✅ | 浏览器 0.55→0.62→0.55；IoU 0.45→0.52→0.45 | REAL |
| 重置默认 | resetOverviewDefaultsButton | PUT /api/config | ✅ | ✅ | 浏览器点击后恢复模型默认 256/0.25/0.45，再恢复工作值 | REAL |
| 重启系统 | rebootSystemButton | POST /api/system/reboot | ✅ | systemctl | 仅确认按钮存在，远程安全模式未点击 | VERIFY |
| 关机 | poweroffSystemButton | POST /api/system/poweroff | ✅ | systemctl | 仅确认按钮存在，远程安全模式未点击 | VERIFY |

---

## 02 热键（profiles-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 热键守护开关 | hotkey_guard_enabled | 无真实 API（Core未提供全局禁用） | 不接入 | 不存在 | 计划中，控件禁用 | PLANNED |
| 热键守护切换键 | hotkey_guard_toggle_hotkey | 无真实 API（Core未提供全局禁用） | 不接入 | 不存在 | 计划中，控件禁用 | PLANNED |
| 热键与类别编辑 | aimProfilesEditor / addAimProfileButton | GET/PUT /api/config | ✅ | RuntimeProfile.mouse.aim_hotkey/aim_hotkey2/aim_hotkey_mode | 浏览器主/副热键、any/all 读取、修改、刷新保持、恢复 | REAL |
| 目标类别 | .aim-profile-class | GET/PUT /api/config | ✅ | RuntimeProfile.inference.class_filter | 浏览器全选→仅第0类→刷新保持→恢复全选 | REAL |
| 移动倍率 | .aim-profile-sensitivity | GET/PUT /api/config | ✅ | RuntimeProfile.mouse.sensitivity | 浏览器 1.70→1.23→刷新保持→恢复 1.70 | REAL |
| X/Y 偏移 | .aim-profile-offset-x/y | GET/PUT /api/config | ✅ | RuntimeProfile.mouse.aim_point.offset_x/offset_y | 浏览器 X 0.48→0.41、Y 0.35→0.63、刷新保持、恢复 | REAL |
| 热键 FOV 缩放 | .aim-profile-fov-scale | 无 Core 字段/消费 | 不接入 | 不存在 | 页面控件保留，待 Core 能力 | PLANNED |
| 偏移切换（备用 X/Y） | .aim-profile-offset-switch-* / alternate-offset-* | 无 Core 字段/消费 | 不接入 | 不存在 | 页面控件保留，待 Core 能力 | PLANNED |
| 物理按键屏蔽 | physicalButtonBlockButton | GET/PUT /api/config | ✅ | ✅ | 本页未执行实际 HID 输出验证 | VERIFY |

---

## 03 移动控制（control-page）

> 状态更新：Phase 8.3（2026-09-01）浏览器真实闭环验证完成，见 `docs/architecture/TTBOX_PHASE_8_3_CODE_LINEAGE.md`。

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 移动倍率 sens | sens | GET/PUT /api/config | ✅ | AimThread 输出链 ×sensitivity | 浏览器 1.7 显示/恢复 | REAL |
| PID kp/ki/kd | controller_kp_x/y 等 | GET/PUT /api/config | ✅ | AimThread Pid1.configure（ki 为 pid1 内部自适应项） | 浏览器 kp 21.5→22.7→恢复；全部字段回读 | REAL |
| X/Y 预判 predict | controller_predict_x/y | GET/PUT /api/config | ✅ | Pid1 I 通道增益 | 浏览器显示/恢复 | REAL |
| X/Y 跟随 rate | controller_rate_x/y | GET/PUT /api/config | ✅ | Pid1 kp_gain_rate | 浏览器显示/恢复 | REAL |
| 基础死区 output_deadzone | controller_output_deadzone | GET/PUT /api/config | ✅ | AimThread 输出门控（9f30e5f 输出链回归） | 浏览器显示/恢复 | REAL |
| 目标丢失宽限 selector_lost_grace_ms | controller_selector_lost_grace_ms | GET/PUT /api/config | ✅ | AimStateMachine lost_grace_ms | 浏览器显示 45ms | REAL |
| 开火延迟释放Y轴 aim_fire_lock_y | controller_aim_fire_lock_y / y_axis_fire_hotkey / release_delay | GET/PUT /api/config | ✅ | C 桥 fire_lock_y（运行链未启用时配置可存） | 浏览器 true→false→恢复 | REAL（配置链路）/ VERIFY（C 桥运行） |
| 拉枪曲线 pull_curve | controller_pull_curve_* | GET/PUT /api/config | ✅ | PullCurve.hpp 类存在；AimThread 消费接线 PLANNED（29d3622 重构断线） | 浏览器 strength 0.9→1.11→恢复 | REAL（配置链路）/ PLANNED（输出消费） |
| 持续提前量 continuous_lead | controller_continuous_lead_* | GET/PUT /api/config | ✅ | C 桥 continuous_lead_output（运行链未启用时配置可存） | 浏览器 scale 0.6→0.71→恢复 | REAL（配置链路）/ VERIFY（C 桥运行） |
| 屏蔽物理移动 block_physical | controller_block_physical_mouse_x/y | GET/PUT /api/config | ✅ | C 桥 inject_ai block_x/y（运行链未启用时配置可存）；state.mouse_output 真实回填 | 浏览器 X false→true→恢复；支持徽标=可用 | REAL（配置链路）/ VERIFY（C 桥运行） |
| 自动标定 | autoCalibration* / calibration start/status/cancel/apply/reset | GET/POST/PUT/DELETE /api/control/calibration* | ✅ | TTBOX Calibration Domain + Core 目标身份/尺寸遥测 | 浏览器状态读取/无目标真实拒绝；完整目标场景待验收 | VERIFY |
| 手动标定参数 | autoCalibrationGainX/Y、Delay + saveAutoCalibrationValuesButton | PUT /api/control/calibration | ✅ | 写 calibration.json + kp 换算写 RuntimeProfile | 浏览器保存→回读→恢复基线 | REAL |
| 记录移动日志 | recordAimTraceButton | POST /api/diagnostics/aim-trace | ✅（按钮已露出） | Gateway 采样线程 → /opt/ttbox/run/aim_trace.json | 按钮可见；采样链路真实现有 | REAL（按钮/API）/ VERIFY（10s 完整采样需运行时） |
| 恢复本页默认值 | resetControllerDefaultsButton | PUT /api/config | ✅ | ✅ | 分区联动（自动标定区隐藏） | REAL |
| 个性曲线训练 | controlSectionTabMotionTraining | /api/motion-profiles* + /api/motion-training/sessions* | ✅ | TTBOX MotionProfile + PersonalMotion | 浏览器 12样本→训练→激活→刷新→停用；真实 HID 效果待验收 | VERIFY |

---

## 04 辅助功能（assist-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 自动扳机 | auto_trigger | GET/PUT /api/config | ✅ | ✅ | ✅ | REAL |
| 连发 | rapid_fire_* | GET/PUT /api/config | ✅ | ✅ | ✅ | REAL |
| 后坐力补偿 | recoil_* | GET/PUT /api/config | ✅ | ✅ | ✅ | REAL |
| 回甩 | auto_back_flick_* | GET/PUT /api/config | ✅ | ✅ | ✅ | REAL |
| 准星检测 | crosshair_* | GET/PUT /api/config | ✅ | ✅ | ✅ | REAL |

---

## 05 模型库（model-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 模型列表/当前模型 | modelCardList / modelCurrentName | GET /api/models | ✅ | IPC MODEL_LIST | ✅ | REAL |
| 切换模型 | modelCardList 点击 | POST /api/models/select | ✅ | IPC MODEL_ACTIVATE | ✅ | REAL |
| 导入模型 | modelImportForm / submitModelImportButton | POST /api/models/import | ✅ | IPC MODEL_IMPORT | ✅ | REAL |
| 删除模型 | modelCardList 删除 | POST /api/models/delete | ✅ | IPC MODEL_REMOVE | ✅ | REAL |
| 类别名编辑 | modelClassNamesEditor | POST /api/models/class-names | ✅ | ✅ | ✅ | REAL |
| 绑定预设 | modelGameCombobox | POST /api/models/bind-preset | ✅ | ✅ | ✅ | REAL |
| NPU 并发 | 模型卡并发 | POST /api/models/rknn-concurrency | ✅ | ✅ | ✅ | REAL |
| 远程导入 | remoteConnectForm | POST /api/remote/connect + /api/remote/models | ✅ | ✅ | ✅ | REAL |
| 设备码 | copyModelDeviceCodeButton | POST /api/models/device-code | ✅ | ✅ | ✅ | REAL |

---

## 06 显示与鼠标（hardware-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 显示器信息 | displayHardwareStatus | GET /api/hardware/display | ✅ | edid/sysfs | ✅ | REAL |
| 显示器配置保存 | display 表单 | PUT /api/hardware/display | ✅ | ✅ | ✅ | REAL |
| EDID 模式管理 | addDisplayEdidModeButton | PUT /api/hardware/display | ✅ | ✅ | ✅ | VERIFY |
| 环出叠加 | display_loopout_overlay_* | PUT /api/hardware/display | ✅ | ✅ | ✅ | VERIFY |
| 鼠标信息 | mouse 状态 | GET /api/hardware/mouse | ✅ | HID | ✅ | REAL |
| 鼠标模式 | hardware/mouse/mode | POST /api/hardware/mouse/mode | ✅ | ✅ | ✅ | REAL |
| 鼠标时序 | hardware/mouse/timing | POST /api/hardware/mouse/timing | ✅ | ✅ | ✅ | REAL |
| 圆形输出测试 | mouse-output/test-circle | POST /api/mouse-output/test-circle | ✅ | ✅ | ✅ | REAL |
| USB 诊断下载 | downloadUsbDiagnosticsButton | GET /api/diagnostics/usb-proxy.zip | ✅ | ✅ | ✅ | REAL |

---

## 07 网络配置（wifi-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| Wi-Fi 状态 | wifiStatusPill | GET /api/network/wifi | ✅ | nmcli | ✅ | REAL |
| 扫描 Wi-Fi | wifiScanButton | POST /api/network/wifi/scan | ✅ | nmcli | ✅ | REAL |
| 连接 Wi-Fi | wifiConnectButton | POST /api/network/wifi/connect | ✅ | nmcli | ✅ | REAL |
| 回退默认 | wifiFallbackButton | POST /api/network/wifi/fallback | ✅ | nmcli | ✅ | REAL |
| 热点模式 | wifiApApplyButton | POST /api/network/wifi/ap/apply | ✅ | nmcli | ✅ | REAL |
| 客户端模式 | wifiClientActivateButton | POST /api/network/wifi/client/activate | ✅ | nmcli | ✅ | REAL |
| 主机名 | lanHostnameInput | PUT /api/system/hostname | ✅ | hostnamectl | ✅ | REAL |
| Web 端口 | webPortInput | PUT /api/system/web-port | ✅ | ✅ | ✅ | VERIFY |
| 局域网屏蔽 | lanBlock* | /api/system/lan-blocklist/* | ✅ | ✅ | ✅ | REAL |
| 网络访问限制 | applyNetworkAccessButton | /api/system/lan-blocklist | ✅ | ✅ | ✅ | REAL |

---

## 08 预设参数（preset-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 预设列表 | presetCardList | GET /api/presets | ✅ | ✅ | ✅ | REAL |
| 保存预设 | savePresetButton | POST /api/presets | ✅ | ✅ | ✅ | REAL |
| 加载预设 | presetCardList 点击 | POST /api/presets/load | ✅ | ✅ | ✅ | REAL |
| 导入预设 | presetImportForm | POST /api/presets/import | ✅ | ✅ | ✅ | REAL |
| 重命名/删除 | presetRenameForm | POST /api/presets | ✅ | ✅ | ✅ | REAL |
| 清理未用预设 | cleanupUnusedPresetsButton | POST /api/presets | ✅ | ✅ | ✅ | REAL |
| 主题商店 | theme-store-page | /api/themes | 部分 | PLANNED | ❌ | PLANNED |

---

## 09 系统设置（license-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 系统版本信息 | systemInfoSummary | GET /api/system/version | ✅ | ✅ | ✅ | REAL |
| 授权状态 | licenseSummary | GET /api/license | ✅ | ✅ | ✅ | REAL |
| 重新激活 | reactivateDeviceButton | POST /api/system/reactivate | ✅ | ✅ | ✅ | REAL |
| 存储信息 | storageExpandSummary | GET /api/system/storage | ✅ | ✅ | ✅ | REAL |
| 扩容 | expandStorageButton | POST /api/system/storage/expand | ✅ | ✅ | ✅ | VERIFY |
| 风扇控制 | fan_control_* | GET/PUT /api/config + 风扇 | ✅ | PWM/温度 | ✅ | REAL |
| 重启/关机 | rebootSystemButton2 / poweroffSystemButton2 | POST /api/system/reboot / poweroff | ✅ | systemctl | ✅ | REAL |

---

## 10 系统更新（update-page）

| 功能 | UI 控件 | API | Gateway | Core/OS | 真机 | 状态 |
|------|---------|-----|---------|---------|------|------|
| 更新状态 | 更新页状态 | GET /api/update/status | ✅ | Update Engine | ✅ | REAL |
| 检查 OTA | 检查按钮 | POST /api/update/check | ✅ | Update Engine | ✅ | REAL |
| USB 扫描 | USB 扫描按钮 | POST /api/update/scan-otg | ✅ | Update Engine | ✅ | REAL |
| 开始更新 | 开始按钮 | POST /api/update/start | ✅ | Update Engine | ✅ | REAL |
| 回滚 | 回滚按钮 | POST /api/update/rollback | ✅ | Update Engine | ✅ | REAL |
| 取消 | 取消按钮 | POST /api/update/cancel | ✅ | Update Engine | ✅ | REAL |
| 更新日志 | 日志视图 | GET /api/update/log | ✅ | Update Engine | ✅ | REAL |

---

## 隐藏/预留页

| 页面 | 功能 | 状态 |
|------|------|------|
| Hailo-8 | hailo-page | PLANNED（无硬件） |
| 键鼠盒子 | kmbox-page | PLANNED（无硬件） |
| 主题商店 | theme-store-page | PARTIAL（本地主题） |
| 风扇控制 | fan-page | REAL（并入 09） |
| 云端（Account/Login/Cloud Config...） | /api/account/*、/api/device/bind 等 | RESERVED |

## 待清理项（历史遗留扫描）

| 位置 | 内容 | 处理 |
|------|------|------|
| app.js | 历史品牌分支（yu/xh 变量残留） | 清理 |
| ttbox_web.py | /api/hailo/*（无硬件） | 标记 PLANNED 或保留 |
| 板端 app.js | 旧版（未含 Phase 6.1） | 部署同步 |
| update-page | update/versions、update/install 旧调用 | 已统一为正式 API，核对 |
| theme-store-page | 主题商店（未做） | PLANNED |
