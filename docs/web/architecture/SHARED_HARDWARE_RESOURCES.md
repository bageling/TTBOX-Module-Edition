# TTBOX 板端硬件资源清单

> TTBOX 运行在 RK3588 板子上。部分硬件资源一次只能被一个进程独占，必须明确记录。

## 资源清单

| 资源 | TTBOX 使用方式 | 是否独占 | 实际限制 | 解决方案 |
|---|---|---|---|---|
| HDMI RX (/dev/video0) | ttbox-core 采集 | ❌ 独占 | 同一时刻只有一个进程能打开 /dev/video0 采集 | 采集进程独占启动；web 面板如实显示"未运行"状态 |
| HDMI RX EDID | edid_apply.sh 注入 | ⚠️ 驱动级 | EDID 存在驱动 /sys 节点，谁后写谁生效 | 记录当前生效身份；读取驱动实际值 |
| USB 鼠标物理设备 | ttbox-usbproxy | ❌ 独占 | usb-gadget/raw-gadget 同一设备只能一个 proxy 接管 | 任一时刻只启用一套 usbproxy；service_active=false 如实反映 |
| NPU (/dev/dri/renderD129) | librknnrt（TTBOX 版 /opt/ttbox/lib） | ✅ 可分时 | 同一 NPU 分时；多个进程可同时 open（RKNN 支持） | 使用 TTBOX 自己的 librknnrt.so |
| RGA 硬件 | /opt/ttbox/lib/librga.so（官方版） | ✅ 可分时 | 用户态库独立，内核 RGA 驱动共享 | 使用 TTBOX 自己的 .so |
| DRM 显示器输出 (card0) | loopout（未启用） | ⚠️ | DRM master 只能一个进程 | TTBOX loopout_enabled=false |
| CPU 频率/绑核 | ttbox-core affinity | ⚠️ 潜在 | 绑核可能互相干扰 | 配置独立管理 |
| 风扇 PWM | TTBOX fan_control | ✅ | 系统级 hwmon 可读 | fan_control.enabled=false 时只读状态 |

## 关键结论

1. **HDMI RX 采集是独占资源**：同一时刻只能有一个采集进程。
2. **HDMI RX EDID 是驱动级共享**：EDID 存在驱动节点，后注入者覆盖前注入者；web 面板读取驱动实际值。
3. **USB 鼠标 proxy 独占**：同一物理鼠标只能一套 proxy 接管；未启用时如实显示 inactive。
4. **NPU/RGA 可分时**：用户态库独立，硬件分时。

> 禁止假装独占资源可同时使用。TTBOX 的 service_active/connected 等字段必须诚实反映 TTBOX 自己的服务状态。
