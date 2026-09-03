# A9 HID 透传架构

日期：2026-08-14
技术路线：**标准 Linux HID Gadget（configfs + usb_f_hid）+ hidraw 读取 + C++ 用户态转发**
依据：[a9-hid-project-comparison.md](./a9-hid-project-comparison.md)

---

## 1. 架构总览

```
真实鼠标/键盘
      ↓ (USB Host: usbdrd3_1 xhci / EHCI)
Linux HID (hidraw: /dev/hidrawX)     ← 原始 HID Report（不解析，透传）
      ↓
C++ HID Forwarder
   ├─ RX 线程: poll+read(hidraw) → SPSC 队列（lock-free, 无 malloc）
   └─ TX 线程: pop → write(/dev/hidgX)
      ↓
USB HID Gadget (configfs usb_f_hid, UDC=fc000000.usb)
      ↓ (USB-C: usbdrd3_0 device 模式)
PC（识别为标准 HID Keyboard + HID Mouse，无需专用驱动）

AI 旁路观察（不阻塞 HID）：
   HID Report ──→ USB HID Gadget → PC
        └──→ HidParser → MouseState → CoordinateTransform → DetectionResult（仅接口）
```

核心原则：**HID 透传链路完全独立于 AI 推理**；禁止"鼠标→AI→再生成鼠标"。

---

## 2. 模块设计（src/hid/）

| 文件 | 职责 |
|---|---|
| `HidTypes.hpp` | HidReport（原始报告+单调时间戳+seq）、MouseState、KeyboardState、CoordinateTransform、DetectionResult（A9 仅接口，无自动控制） |
| `SpscQueue.hpp` | Vyukov lock-free SPSC 环形队列（RX→TX，固定容量，无锁无分配） |
| `HidForwarder.hpp/.cpp` | RX/TX 双线程转发器：hidraw → 队列 → hidg；统计 latency/drop/queue 深度/回报率；CPU affinity 可选 |
| `HidParser.hpp/.cpp` | 鼠标/键盘报告解析（调试/接口），坐标转换（Screen→ROI→Model→Detection 严格分离） |

## 3. HID Gadget（标准方案）

脚本：`scripts/a9_setup_hid_gadget.sh`（configfs）

```
/sys/kernel/config/usb_gadget/ttbox-hid/
  ├── idVendor=0x1d6b  idProduct=0x0104  bcdUSB=0x0200
  ├── functions/hid.usb0   # Keyboard：protocol=1(boot) report_length=8 desc=63B
  ├── functions/hid.usb1   # Mouse：   protocol=2(boot) report_length=4 desc=52B
  ├── configs/c.1/         # MaxPower=500，两个 function 组合
  └── UDC=fc000000.usb     # dwc3-gadget（USB-C 接 PC）
```

- 主机识别：标准 `USB HID Keyboard` / `USB HID Mouse`，无需 Windows 专用驱动（实测 /dev/hidg0、/dev/hidg1 出现，UDC=configured，write 可接收）

## 4. 数据路径

### RAW（默认优先）
```
hidraw 原始 report（带单调时间戳）→ SPSC 队列 → 原样写 hidg
```
- 键盘 8B boot report：完整透传
- 鼠标 ≤4B boot report：完整透传（buttons+dx+dy+wheel）
- 限制（如实记录）：>4B 鼠标（int16 坐标/扩展按钮/水平滚轮）写入时截断到 4B；
  完整保留非标报告需要 raw-gadget（A9 不启用，记为限制）

### EVDEV（兼容/调试预留）
```
/dev/input/eventX → HidParser → 转发
```
A9 主路径为 RAW；EVDEV 接口已预留（HidParser 可解析）。

## 5. 线程与队列

- RX 线程：`poll(hidraw, POLLIN)` → `read` → 打时间戳 → `try_push`（满则计数 drop）
- TX 线程：`try_pop` → `write(hidg)`；空队列 `yield()`；写失败（EAGAIN/ENODEV）计数 drop 不阻塞
- SPSC 队列：固定容量 1024，无锁、无 malloc、无逐事件日志
- HID 线程绝不等待 RKNN/RGA/V4L2/Decode/Web/JSON（完全隔离）

## 6. CPU 调度（实验决定，不凭理论）

- RX/TX 线程 `--cpu N` 可选绑定（pthread_setaffinity_np）
- A9 实验矩阵：默认 / CPU0-7，与 3×Worker(A76 4/5/6) + NPU IRQ(CPU0) 组合实测
- 测量：HID latency/jitter/drop、AI FPS、CPU%、NPU IRQ、温度
- 注意：NPU IRQ 默认集中在 CPU0（/proc/interrupts 实测），HID 线程优先考虑避开

## 7. 时间戳（E2E latency 预备）

- 所有 HID Report 带 `timestamp_us`（monotonic，RX 时刻）
- DetectionResult 带 `timestamp_us` + `frame_id`（与 HID 同单调时钟）
- 可计算：HID→Capture→NPU→Detection→Output 各段延迟（A9 建立接口）

## 8. 坐标系统一

```
物理鼠标(dx/dy 相对) → [不可直接映射] → Screen(全帧像素)
Screen → Capture ROI（平移+裁剪）
Capture ROI → Model Input（缩放）
Model Input → Detection（ROI 偏移，与 DecodeNMS::map_coords 一致）
Screen = Detection（原图坐标，恒等）
```
CoordinateTransform 提供 screen_to_roi / roi_to_model / model_to_detection / screen_to_detection，严格区分坐标系，禁止混用。

## 9. 测试

- `ttbox_core_tests`：SPSC 队列 / HID 解析 / 坐标转换（单元）
- `test_hid_forward_hw`：gadget 检查 + hidraw 枚举 + 转发统计（无设备时如实报 NOT AVAILABLE）
- `ttbox-hid-test`：独立测试程序（--mouse/--keyboard/--rate/--duration/--verbose），显示 device/VID/PID/descriptor/report size/rate/keyboard events/mouse events/latency/drop

## 10. 安全边界

A9 仅实现：本地 USB 设备 → 本机 HID → USB 主机物理透传。
不实现：云端控制、远程键鼠、网络 HID、自动输入、绕过主机安全。

## 11. 实测限制：标准 f_hid 高回报率上限（≈500 Hz）

回环实测（fifo 合成鼠标报告 → HidForwarder → /dev/hidg1，主机已枚举）：

| 注入速率 | 转发成功 | 背压丢弃 | 结果 |
|---|---|---|---|
| 125 / 250 / 500 Hz | 100% | 0 | 无损 |
| 1000 Hz | 1500/3000 | 1500 | ≈50%（上限 ~500 Hz） |
| 2000 / 4000 / 8000 Hz | ~1500 | 其余全部背压 | 上限不变 ~500 Hz |

- 转发器本身：RX 100%、latency avg ≈4.5µs（p50=4-5µs）、CPU 负载可忽略、队列深度 ≤1
- 瓶颈在 **usb_f_hid（f_hid）gadget 端点吞吐**（dwc3 HS 中断 IN 端点 + f_hid request 队列），非 CPU/队列/线程
- 结论（对应对照分析第 3 节判断）：
  - 标准 HID Gadget **满足 ≤500 Hz** 键鼠（覆盖绝大多数办公/普通键鼠）→ A9 采用
  - **>500 Hz 电竞鼠标（1000 Hz+）标准 f_hid 会丢报告** → 该场景需 raw-gadget（usb-proxy 方案），A9 记录为限制与后续备选，不实现（需内核模块 GPL-2.0）
- 真实鼠标为事件型（仅移动/按键产生报告），平均报告率通常低于上限，但持续快速移动时 >500 Hz 段会丢报告——如实记录，不伪造
