# 模块 08：鼠标输出层（Mouse / HID）

## 它是什么？

鼠标输出层是"最后一步"：把控制层的指令，真正变成鼠标的移动。

## 它干什么？

1. **输出后端**：支持多种鼠标输出方式
   - USB 代理（usb_proxy）：通过代理程序转发
   - FIFO：写入命名管道
   - 本地 HID 后端
2. **HID 合成**：把 dx/dy 包装成 HID 报告（电脑认识的鼠标数据格式）
3. **注入开关**：`output_enabled` 为 false 时，**什么都不注入**（安全开关）
4. **合成鼠标读取**：读取物理鼠标事件（用于合流/门控）

## 什么是 HID？

HID = Human Interface Device（人机接口设备），是鼠标/键盘和电脑通信的标准协议。
盒子伪装成鼠标，通过 HID 把移动指令发给电脑。

## 什么是 USB 与 HDMI 的区别？（重要！）

- **HDMI**：传**画面**（电脑 → 盒子）。盒子通过 HDMI 线"看"电脑屏幕。
- **USB**：传**指令**（盒子 → 电脑，或双向）。盒子通过 USB 线"操作"电脑（伪装鼠标）。

两条线各干各的：HDMI 是眼睛，USB 是手。

## 输入是什么？

- 鼠标移动指令（dx, dy，来自模块 07）
- 物理鼠标事件（可选，用于门控）

## 输出是什么？

- HID 报告（真正注入电脑的鼠标数据）

## 谁调用它？

- `AimThread`（瞄准线程输出）
- `OutputBackend`（输出后端管理）

## 它不能干什么？

- 不能识别目标（那是模块 03-05）
- 不能在 `output_enabled=false` 时注入（安全红线）
- 不能绕过注入开关

## 修改它会影响什么？

- 影响鼠标移动的真实效果
- 修改 HID 行为 = 修改注入行为 = **被严格禁止的范围**（当前必须保持 output_enabled=false）

## 关键文件

| 文件 | 作用 |
|---|---|
| `core/src/output/OutputBackend.cpp/.hpp` | 输出后端统一管理 |
| `core/src/output/FifoHidOutput.cpp/.hpp` | FIFO 输出 |
| `core/src/output/AiboxHidOutput.cpp/.hpp` | AIBOX 兼容输出 |
| `core/src/output/MakcuMouseBackend.cpp/.hpp` | MAKCU 鼠标后端 |
| `core/src/hid/HidForwarder.cpp/.hpp` | HID 转发 |
| `core/src/input/PhysicalMouseReader.cpp/.hpp` | 物理鼠标读取 |
