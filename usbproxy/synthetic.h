// synthetic.h — TTBOX usb-proxy synthetic 模式（复刻 YU --synthetic_mouse）
#pragma once

#include <string>

// 从 gadget-config.json 构造合成 HID 鼠标的 host 描述符（无物理设备）。
// 返回 0 成功。失败时打印错误。
int setup_synthetic_gadget_desc();

// synthetic 模式端点写线程入口：周期性从挂起位移构建 HID 报告并写入 gadget IN EP。
void* synthetic_injector_thread(void* arg);

// synthetic 模式 ep0 事件循环（不依赖 libusb；服务合成描述符 + 启用端点）。
void synthetic_ep0_loop(int fd);

extern bool synthetic_mode;
