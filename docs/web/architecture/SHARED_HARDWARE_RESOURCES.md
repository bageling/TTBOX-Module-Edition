# 共享硬件资源清单

> TTBOX 和 YU 在同一块 RK3588 板子上运行。某些硬件资源无法同时独占，必须明确记录。

## 资源清单

| 资源 | YU 使用方式 | TTBOX 使用方式 | 可共享 | 实际限制 | 解决方案 | 验证方式 |
|---|---|---|---|---|---|---|
| HDMI RX (/dev/video0) | aiassistance-daemon 采集 | ttbox-core 采集 | ❌ 独占 | 同一时刻只有一个进程能打开 /dev/video0 采集 | YU 与 TTBOX 的 core 不能同时启动采集；web 面板均需显示"未运行"状态 | 实测：TTBOX core 启动时 YU 必须停止，反之亦然 |
| HDMI RX EDID | hdmirx_edid 注入 | edid_apply.sh 注入 | ⚠️ 共享（驱动级） | EDID 存在驱动 /sys 节点，谁后写谁生效 | 记录当前生效身份；两系统都读驱动实际值 | 实测：TTBOX 注入 TTBox-COMPAT 后，YU 的 --status 也读到 TTBox-COMPAT |
| USB 鼠标物理设备 | aiassistance-usbproxy (3-1 Logitech) | ttbox-usbproxy（未启用） | ❌ 独占 | usb-gadget/raw-gadget 同一设备只能一个 proxy 接管 | 任一时刻只启用一套 usbproxy；当前 TTBOX usbproxy disabled（service_active=false 诚实反映） | service_active_text: TTBOX=inactive（未启用） |
| NPU (/dev/dri/renderD129) | librknnrt（yu 自带版） | librknnrt（TTBOX 版 /opt/ttbox/lib） | ✅ 可共享 | 同一 NPU 分时；两个进程可同时 open（RKNN 支持） | 各自用自己的 librknnrt.so | 实测两个进程同时运行推理 |
| RGA 硬件 | /opt/aiassistance/lib/librga.so | /opt/ttbox/lib/librga.so（官方版） | ✅ 可共享 | 用户态库各自独立，内核 RGA 驱动共享 | 各自用自己的 .so | md5 不同，运行时各自加载 |
| DRM 显示器输出 (card0) | loopout 环出 | loopout（未启用） | ⚠️ | DRM master 只能一个进程 | TTBOX loopout_enabled=false | status=disabled |
| CPU 频率/绑核 | aiassistance-performance | ttbox-core affinity | ⚠️ 潜在 | 绑核冲突 | 各自配置独立 | 待验证 |
| 风扇 PWM | aiassistance 温控 | TTBOX 未接管 | ✅ | 系统级 hwmon 可读 | fan_control.enabled=false | status 一致 |

## 关键结论

1. **HDMI RX 采集是独占资源**：TTBOX 与 YU 的 core 不能同时启动。这是 HARDWARE_RESOURCE_CONFLICT（采集层面）。
2. **HDMI RX EDID 是驱动级共享**：EDID 存在驱动节点，两系统都注入时后者覆盖前者。TTBOX/YU 的 web 面板都读驱动实际值（诚实反映）。
3. **USB 鼠标 proxy 独占**：同一物理鼠标只能一套 proxy。当前 YU 的 usb-proxy 在跑，TTBOX usbproxy disabled（诚实反映 inactive）。
4. **NPU/RGA 可共享**：用户态库独立，硬件分时。

> 禁止假装独占资源可同时使用。TTBOX 的 service_active/connected 等字段必须诚实反映 TTBOX 自己的服务状态。