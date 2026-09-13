# TTBOX usbproxy（鼠标注入代理）

`usbproxy` 是 TTBOX 的"手"：

```text
AI 核心 ──▶ cmd.sock ──▶ usbproxy ──▶ Raw Gadget ──▶ 电脑识别为鼠标
物理鼠标 ──▶ libusb ────┘（full 模式会搭车合并）
```

它不接收 AI 画面，只负责把"鼠标往哪动"的指令，变成电脑 USB 口上的真实鼠标事件。

---

## 一、两种模式

| 模式 | 什么时候用 | 电脑看到的鼠标 |
|---|---|---|
| `full`（默认） | 盒子上插着物理鼠标 | 克隆物理鼠标，物理报告 + AI 位移合并 |
| `synthetic` | 没有物理鼠标 | 独立合成一个鼠标，AI 位移直接注入 |

模式由服务环境变量决定：

```text
USB_PROXY_MODE=full
```

改成 `synthetic` 后重启服务即可。

## 二、核心文件

| 文件 | 干什么 |
|---|---|
| `usb-proxy.cpp` | 主入口：读参数、分发模式、启动协议层 |
| `proxy.cpp` | Raw Gadget 端点转发 + AI 位移合并 |
| `mouse_control.cpp/.hpp` | cmd.sock / event.sock 协议层（0x4F50 协议） |
| `synthetic.cpp/.h` | synthetic 模式：合成鼠标描述符 + 实时注入线程 |
| `device-libusb.cpp` | 物理鼠标采集（full 模式） |
| `host-raw-gadget.cpp` | 主机侧 Raw Gadget 封装 |
| `board/run-ttbox-usb-proxy.sh` | 板端启动脚本 |

## 三、编译和部署

源码在板端 `/opt/ttbox/src/usbproxy`，运行目录是 `/opt/ttbox/usbproxy`。

```bash
cd /opt/ttbox/src/usbproxy
make
cp usb-proxy /opt/ttbox/usbproxy/usb-proxy
systemctl restart ttbox-usbproxy
```

## 四、验证是否正常工作

### 1. 服务状态

```bash
systemctl is-active ttbox-usbproxy
```

### 2. 通信 socket 是否存在

```bash
ls -l /run/ttbox-mouse-passthrough/
```

正常会有：

```text
cmd.sock      AI 核心发送鼠标指令的入口
event.sock    物理鼠标按键事件订阅入口
```

### 3. 按键与移动测试

```bash
python3 /opt/ttbox/src/core/tests/usbproxy_buttontest.py
```

脚本会连接两个 socket，测试订阅按键快照和移动指令注入。

### 4. 单独诊断 socket

```bash
python3 /opt/ttbox/src/core/tools/usb_diag.py
```

## 五、常见问题

**电脑识别不到鼠标？**

检查 Raw Gadget 内核模块：

```bash
lsmod | grep raw_gadget
```

没加载就执行：

```bash
modprobe raw_gadget
```

**AI 动了但鼠标不动？**

按顺序检查：

1. `ttbox-usbproxy` 是否 active。
2. `cmd.sock` 是否存在。
3. AI 核心的 `output_enabled` 和 `mouse.enabled` 是否为 true。
4. 热键是否按住（没有热键不允许注入）。

**换过鼠标/重新插线后不生效？**

重启 usbproxy，让设备重新枚举：

```bash
systemctl restart ttbox-usbproxy
```
