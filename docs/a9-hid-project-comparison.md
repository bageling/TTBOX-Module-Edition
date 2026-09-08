# A9 HID 透传项目对照分析

日期：2026-08-14
目标：USB 键鼠透传（Host → Proxy → USB Gadget → PC）技术选型
原则：优先复用成熟开源方案，不自研 HID 架构；先判断标准 HID Gadget 是否满足。

---

## 1. 四项目总览

| 项目 | 许可证 | 语言 | 核心定位 | 硬件要求 |
|---|---|---|---|---|
| [Gadgetoid/pi400kb](https://github.com/Gadgetoid/pi400kb) | **MIT** | C | 把 Pi 400/500 变成 USB 键盘+鼠标 | 树莓派（dwc2/OTG），/dev/hidg0 |
| [AristoChen/usb-proxy](https://github.com/AristoChen/usb-proxy) | **Apache-2.0** | C++ | 通用 USB 代理（raw-gadget + libusb） | OTG 口 + raw-gadget 内核模块 |
| [xairy/raw-gadget](https://github.com/xairy/raw-gadget) | 内核模块 **GPL-2.0** | C | 用户态任意 USB 设备模拟/代理 | UDC（USB Device Controller）+ 内核 5.7+ |
| [jfedor2/hid-remapper](https://github.com/jfedor2/hid-remapper) | **MIT** | C/C++ | 低延迟 HID 重映射/转发（RP2040 固件） | RP2040/Feather（独立硬件） |

---

## 2. 逐项对照

### 2.1 Gadgetoid/pi400kb

**核心技术**
- 读取：`hidraw`（`HIDIOCGRAWINFO` 按 VID/PID 定位设备）+ `EVIOCGRAB` 独占抓取
- Gadget：`libcomposite`（内核模块）/ `libusbgx` 配置标准 HID gadget
- 转发：单线程 `poll()` 键盘/鼠标两个 fd → 读 report → 写 `/dev/hidg0`（带 report_id 前缀）
- 键盘/鼠标合并到一个 hidg0，用 report_id（1=键盘, 2=鼠标）区分

**线程模型**：单线程 poll 循环（无独立 RX/TX），写后 `usleep(1000)` 限速

**适合部分**：
- 标准 HID Gadget（libcomposite/configfs）配置方法
- hidraw 按 VID/PID 自动查找设备
- 键盘+鼠标 composite 设计

**不适合部分**：
- 单线程 + usleep 限速 → 高回报率（>1000Hz）受限
- report_id 前缀方案依赖其自定义 gadget 描述符，与标准 boot 模式不同

**License**：MIT，可直接复用（保留版权声明）

**RK3588 适配难度**：低。dwc3 dual-role 等价于树莓派 dwc2；configfs 已是标准接口

### 2.2 AristoChen/usb-proxy

**核心技术**
- Host 端（模拟设备给 PC）：`raw-gadget`（用户态控制 UDC）
- Device 端（读真实设备）：`libusb`（bulk/interrupt 传输）
- 转发：中断传输原样透传 + 可选的 injection（控制/中断/bulk 的 pattern 替换、字节运算、Lua 脚本）
- 多线程：raw-gadget 主线程处理枚举 + 每端点一个线程

**适合部分**：
- **HID Report 原样转发思路**（中断 IN 传输透传，不解析）
- 高回报率：libusb interrupt 传输异步 + 独立线程
- 特殊 HID / Vendor-specific / 需要保留原始描述符

**不适合部分**：
- 依赖 raw-gadget 内核模块（GPL-2.0，需编译/加载，且 raw-gadget README 明确"生产环境标准类设备不建议用"）
- 无 SuperSpeed（raw-gadget 限制，USB 2.0 足够 HID）
- 相对复杂（枚举/复位/config 切换处理）

**License**：Apache-2.0，可直接复用（保留 NOTICE + 声明修改）

**RK3588 适配难度**：中。需编译 raw-gadget 模块 + 处理 dwc3 端点；对标准 HID 属过度设计

### 2.3 xairy/raw-gadget

**核心技术**
- 内核模块暴露 `/dev/raw-gadget`：`USB_RAW_IOCTL_INIT/RUN/EVENT_FETCH/EP_ENABLE/EP_READ/EP_WRITE`
- 用户态完全控制 USB 设备枚举与传输（任意类、任意描述符、最小校验）

**适合部分**：
- 完整保留 VID/PID/Report Descriptor/Report Size/Report ID（用户态原样回复描述符请求）
- 特殊/Vendor HID、fuzzing、USB 设备代理
- 参考 usb-proxy 已基于它实现完整 proxy

**不适合部分**：
- **README 明确："不推荐生产环境用 Raw Gadget 模拟标准类设备，用 Composite Framework"**
- 无 SuperSpeed（HID 无需）
- GPL-2.0 内核模块：加载不影响用户态代码许可，但若复制其用户态示例代码需遵守 GPL

**License**：内核模块 GPL-2.0（Linux 内核集成；用户态 API 使用不受限）

**RK3588 适配难度**：中高（编译模块 + dwc3 端点映射 + 完整枚举处理）

### 2.4 jfedor2/hid-remapper

**核心技术**
- RP2040 固件：USB Host 读键鼠 + USB Device（gadget）出给 PC
- 低延迟：硬件原生转发，报告在内存中直通
- 支持 1000Hz 回报率（甚至超频）
- 配置：WebHID/CLI（JSON 规则）

**适合部分**：
- **低延迟转发的架构思想**（设备 → 中间层 → HID 输出）
- 独立转发线程、不与上层业务（AI）互相阻塞的理念

**不适合部分**：
- 固件是 RP2040 专用（不可直接用于 RK3588）
- 我们不需要重映射逻辑（A9 只透传），且禁止自动控制逻辑

**License**：MIT

**RK3588 适配难度**：不直接可移植；仅借鉴线程/队列思想

---

## 3. 决策：标准 HID Gadget 是否满足

用户要求第 3 节：先判断"标准 HID Gadget"是否满足"真实键鼠 → Host → HID Report → Gadget → PC"。

**结论：满足 A9 目标，优先采用标准 HID Gadget（configfs / usb_f_hid）。**

| 需求 | 标准 configfs f_hid gadget | 评估 |
|---|---|---|
| 键盘（boot 8B 标准） | ✓ 自定义 report_desc 可配 | 满足 |
| 鼠标（boot 4B） | ✓ 自定义 report_desc 可配 | 满足 |
| Composite（键盘+鼠标同 gadget） | ✓ 多函数组合 | 满足 |
| 高回报率（1000Hz） | ✓ 中断 IN 端点（bInterval 可配） | 满足 |
| 原始 Report Descriptor | 部分：可复制真实设备 desc 到 f_hid | 满足（A9 用标准 boot；非标记录限制） |
| 原始 VID/PID | 部分：f_hid 可设 idVendor/idProduct | 可设为真实设备值 |
| 特殊/Vendor HID / 任意 USB 类 | ✗（f_hid 仅 HID 类） | 不需要（A9 只 HID） |
| 完整保留非标 Report（int16 鼠标/扩展按钮） | ✗（report_length 固定） | 记录限制，需 raw-gadget 才能完整保留 |

raw-gadget README 亦明确建议标准类设备使用 Composite Framework。因此：

> **技术路线：标准 Linux HID Gadget（configfs + usb_f_hid）+ hidraw 读取 + C++ 用户态转发。**
> Raw Gadget 仅作为"完整保留非标 HID"的未来备选（A9 不启用）。

---

## 4. 四项目采用点（A9 实际使用）

| 项目 | 采用点 | 复用什么 | 说明 |
|---|---|---|---|
| pi400kb | **主线** | hidraw 按 VID/PID 查找设备；标准 HID Gadget 配置；键盘+鼠标 composite | 我们改为 configfs（等价 libcomposite） |
| usb-proxy | 转发思想 | 原始 HID Report 透传（中断传输不做重编码）；injection 仅预留 | A9 纯透传，不修改报告 |
| raw-gadget | 备选 | 仅在标准 gadget 无法保留非标 HID 时启用（A9 不启用） | README 不建议生产用 |
| hid-remapper | 线程模型 | 独立 RX/TX 线程 + 队列，避免阻塞 | 我们用 SPSC 队列（比单线程 poll 更优） |

**License 使用声明**：
- pi400kb（MIT）：参考其 hidraw 查找与 gadget 配置思路，代码为独立实现（不直接复制文件），保留版权声明
- usb-proxy（Apache-2.0）：仅借鉴"原始报告透传"概念，未复制代码
- hid-remapper（MIT）：仅借鉴线程/队列思想
- raw-gadget（GPL-2.0）：不加载模块、不复制其代码（A9 不启用）

---

## 5. RK3588 硬件适配评估

已实测（A9 硬件调查）：
- **USB Host**：`usbdrd3_1`（xhci）+ 2× EHCI/OHCI ✓
- **USB Device/OTG**：`usbdrd3_0`（dwc3-gadget UDC 已注册，`/sys/class/udc/fc000000.usb`）✓
- **DRD/Role Switch**：`CONFIG_USB_DWC3_DUAL_ROLE=y` + typec port0 ✓
- **内核**：`CONFIG_USB_GADGET=y`、`CONFIG_USB_CONFIGFS=y`、`CONFIG_USB_F_HID=y`、`CONFIG_HIDRAW=y`、`CONFIG_USB_HID=y` ✓（全部内置，无需改内核）
- **HID Gadget 实测**：ttbox-hid（键盘 63B desc / 鼠标 52B desc）已绑定 UDC，`/dev/hidg0`+`/dev/hidg1` 已出现，UDC state=configured，主机侧可接收报告 ✓

结论：**Orange Pi 5 Plus 当前内核即支持 Host → Proxy → Device 完整链路，无需修改内核。**

## 6. 结论

- 采用：**标准 HID Gadget（configfs usb_f_hid）+ hidraw + C++ 转发器**
- 主参考：pi400kb（gadget 配置 + hidraw 查找）
- 转发模型：hid-remapper 思想（独立 RX/TX + 队列）+ usb-proxy 原始报告透传
- 备选：raw-gadget（非标 HID 完整保留时才需要，A9 不启用）
- 全部许可允许本使用方式（MIT/Apache-2.0；GPL 模块不加载）
