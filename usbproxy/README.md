# TTBOX usbproxy

TTBOX 自研 USB 鼠标代理（raw-gadget + libusb），
让 TTBOX 独立完成 AI → HID 鼠标注入全链路。

```
AI (MOVE_CMD) ──▶ cmd.sock ──▶ usbproxy ──▶ Raw Gadget ──▶ HID ──▶ PC 鼠标
物理鼠标    ──▶ libusb   ──▶ 搭车合并 ──┘
```

## 两种模式

| 模式 | 说明 | 枚举身份 |
|---|---|---|
| `full`（默认） | 克隆物理鼠标（Logitech 046d:c53f），物理报告 + AI 位移搭车合并 | 克隆 Logitech |
| `synthetic` | 无物理鼠标，独立合成鼠标，AI 位移直接注入 | Corsair 9A80:7072 |

## 核心文件

| 文件 | 作用 |
|---|---|
| `usb-proxy.cpp` | 主入口：参数解析、模式分发、mouse_control 启动 |
| `proxy.cpp` | raw-gadget 端点转发 + `mouse_control_merge_report`（AI 搭车合并） |
| `mouse_control.cpp/.hpp` | cmd.sock/event.sock 协议层（0x4F50，15 种消息） |
| `synthetic.cpp/.h` | synthetic 模式：合成描述符 + RT 注入线程（SCHED_FIFO 98 + CPU affinity） |
| `board/run-ttbox-usb-proxy.sh` | 启动脚本（full 模式自动探测物理鼠标） |
| `systemd/ttbox-usbproxy.service` | systemd 托管（Restart=always，自愈） |
| `gadget-config.json` | synthetic 身份配置 |
| `Makefile` | 板端编译 |

## 协议（自研二进制协议）

- 传输：Unix `SOCK_SEQPACKET`，`/run/orangepi-mouse-passthrough/`
- 包头：`<HBBI` = magic 0x4F50 / version 1 / type / request_id
- MOVE_CMD=4：`<iii` dx/dy/wheel（20 字节）
- BUTTON_CMD=5：`<BB` button/action
- GET_STATE 6/7、SUBSCRIBE 8/9、SNAPSHOT 10 `<BQ`、BUTTON_EVENT 11 `<BBBQ`
- CONFIG 12-15：`<HHHHBBBHBBBB`（max_power 为 u16）+ 5 字符串

## 3 项关键实现细节

1. **event 通道逐按钮事件**（BUTTON_EVENT 按单按钮推送，带按下状态与时间戳）
2. **SET_CONFIG apply-now** = `_exit(0)`，由 systemd `Restart=always` 重启重枚举
3. **RT 调度**：注入/event/cmd 线程 SCHED_FIFO 98 + CPU affinity（大核）

## 真机验证（2026-09-03/04）

| 项目 | 结果 |
|---|---|
| synthetic 枚举 | VID_9A80&PID_7072 Corsair，Status=OK |
| full 克隆 | Logitech 046d:c53f 3 接口 OK，物理+AI 搭车一致 |
| AI MOVE | 1:1 精确（正/负/混合方向零误差） |
| 高频 | 100~2000Hz 0 丢包（RT 线程） |
| 热插拔/重枚举 | OK，systemd 自愈 6 秒恢复 |

## 编译（板端）

```bash
cd /opt/ttbox/usbproxy && make
```

## 运行（synthetic 独立测试）

```bash
systemctl start ttbox-usbproxy.service
# 或手动：
./usb-proxy --device=fc000000.usb --driver=dwc3-gadget \
  --synthetic_mouse --enable_mouse_control
```
