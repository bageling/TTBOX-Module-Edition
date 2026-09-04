// synthetic.cpp — TTBOX usb-proxy synthetic 模式
// 自研 synthetic 模式：无物理鼠标，暴露一个 Corsair HID 鼠标 gadget，
// AI MOVE 经 mouse_control 协议直接注入。
#include "synthetic.h"
#include "mouse_control.h"
#include "proxy.h"
#include "misc.h"
#include "host-raw-gadget.h"

#include <fstream>
#include <jsoncpp/json/json.h>
#include <cstring>
#include <linux/hid.h>

using ttbox_usbproxy::g_gadget_config;
using ttbox_usbproxy::g_state;

// RT 调度（定义在 mouse_control.cpp）
void apply_rt_thread_policy();

// host 侧合成描述符（setup_host_usb_desc 的 synthetic 版本）
struct synthetic_desc {
    struct usb_device_descriptor dev;
    struct usb_config_descriptor cfg;
    struct usb_interface_descriptor iface;
    struct usb_endpoint_descriptor ep;
} g_synth;

// 合成 IN 端点号（gadget 侧），由 usb_raw_ep_enable 返回
static int g_synth_ep_num = -1;
// 合成模式 raw-gadget fd（注入线程使用）
static int g_synth_fd = -1;
// HID 描述符类型（部分内核头未导出 USB_DT_HID）
#ifndef USB_DT_HID
#define USB_DT_HID 0x21
#endif
#ifndef HID_DT_REPORT
#define HID_DT_REPORT 0x22
#endif
#ifndef HID_DT_PHYSICAL
#define HID_DT_PHYSICAL 0x23
#endif

// ── 从 gadget-config.json 加载配置 ──
static int load_gadget_config(const char* path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        fprintf(stderr, "gadget-config.json not found: %s\n", path);
        return -1;
    }
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(ifs, root)) {
        fprintf(stderr, "gadget-config.json parse error\n");
        return -1;
    }
    g_gadget_config.usb_vid = root.get("usb_vid", 0x9A80).asInt();
    g_gadget_config.usb_pid = root.get("usb_pid", 0x7072).asInt();
    g_gadget_config.usb_bcd_usb = root.get("usb_bcd_usb", 0x0200).asInt();
    g_gadget_config.usb_bcd_device = root.get("usb_bcd_device", 0x0100).asInt();
    g_gadget_config.usb_device_class = root.get("usb_device_class", 0).asInt();
    g_gadget_config.usb_device_subclass = root.get("usb_device_subclass", 0).asInt();
    g_gadget_config.usb_device_protocol = root.get("usb_device_protocol", 0).asInt();
    g_gadget_config.usb_max_power = root.get("usb_max_power", 250).asInt();
    g_gadget_config.hid_protocol = root.get("hid_protocol", 2).asInt();
    g_gadget_config.hid_subclass = root.get("hid_subclass", 1).asInt();
    g_gadget_config.hid_report_length = root.get("hid_report_length", 4).asInt();
    g_gadget_config.hid_interval = root.get("hid_interval", 1).asInt();
    g_gadget_config.manufacturer = root.get("usb_manufacturer", "Corsair").asString();
    g_gadget_config.product = root.get("usb_product", "Corsair USB Optical Mouse").asString();
    g_gadget_config.serial = root.get("usb_serial", "OPI5P-MOUSE").asString();
    g_gadget_config.configuration = root.get("usb_configuration", "Mouse").asString();
    g_gadget_config.hid_report_desc_hex =
        root.get("hid_report_desc_hex", g_gadget_config.hid_report_desc_hex).asString();
    return 0;
}

// ── 构建合成 host 描述符 ──
// host_device_desc 是 raw-gadget 侧的全局描述符（定义见 host-raw-gadget.h）。
int setup_synthetic_gadget_desc() {
    const char* cfg_path = getenv("USB_PROXY_GADGET_CONFIG_FILE");
    if (!cfg_path) cfg_path = "/opt/ttbox/usbproxy/gadget-config.json";
    if (load_gadget_config(cfg_path) != 0) return -1;

    // 9a80:7072（gadget-config.json 缺省值即 Corsair）
    g_synth.dev.bLength = sizeof(struct usb_device_descriptor);
    g_synth.dev.bDescriptorType = USB_DT_DEVICE;
    g_synth.dev.bcdUSB = g_gadget_config.usb_bcd_usb;
    g_synth.dev.bDeviceClass = g_gadget_config.usb_device_class;
    g_synth.dev.bDeviceSubClass = g_gadget_config.usb_device_subclass;
    g_synth.dev.bDeviceProtocol = g_gadget_config.usb_device_protocol;
    g_synth.dev.bMaxPacketSize0 = 64;
    g_synth.dev.idVendor = g_gadget_config.usb_vid;
    g_synth.dev.idProduct = g_gadget_config.usb_pid;
    g_synth.dev.bcdDevice = g_gadget_config.usb_bcd_device;
    g_synth.dev.iManufacturer = 1;
    g_synth.dev.iProduct = 2;
    g_synth.dev.iSerialNumber = 3;
    g_synth.dev.bNumConfigurations = 1;

    // 配置：1 个 HID 鼠标接口（boot subclass=1, protocol=2），无描述符字符串
    g_synth.cfg.bLength = sizeof(struct usb_config_descriptor);
    g_synth.cfg.bDescriptorType = USB_DT_CONFIG;
    g_synth.cfg.wTotalLength =
        sizeof(struct usb_config_descriptor) +
        sizeof(struct usb_interface_descriptor) +
        9 +  // HID descriptor
        sizeof(struct usb_endpoint_descriptor);
    g_synth.cfg.bNumInterfaces = 1;
    g_synth.cfg.bConfigurationValue = 1;
    g_synth.cfg.iConfiguration = 0;
    g_synth.cfg.bmAttributes = 0x80;  // bus-powered
    g_synth.cfg.bMaxPower = g_gadget_config.usb_max_power / 2;

    g_synth.iface.bLength = sizeof(struct usb_interface_descriptor);
    g_synth.iface.bDescriptorType = USB_DT_INTERFACE;
    g_synth.iface.bInterfaceNumber = 0;
    g_synth.iface.bAlternateSetting = 0;
    g_synth.iface.bNumEndpoints = 1;
    g_synth.iface.bInterfaceClass = USB_CLASS_HID;
    g_synth.iface.bInterfaceSubClass = g_gadget_config.hid_subclass;   // 1 boot
    g_synth.iface.bInterfaceProtocol = g_gadget_config.hid_protocol;   // 2 mouse
    g_synth.iface.iInterface = 0;

    g_synth.ep.bLength = sizeof(struct usb_endpoint_descriptor);
    g_synth.ep.bDescriptorType = USB_DT_ENDPOINT;
    g_synth.ep.bEndpointAddress = 0x81;  // IN
    g_synth.ep.bmAttributes = USB_ENDPOINT_XFER_INT;
    g_synth.ep.wMaxPacketSize = 8;
    g_synth.ep.bInterval = g_gadget_config.hid_interval;  // 1ms
    g_synth.ep.bRefresh = 0;
    g_synth.ep.bSynchAddress = 0;
    return 0;
}

// ── 注入线程：周期性把挂起 AI 位移写入合成端点 ──
// synthetic 设计："pure gadget endpoint; physical overlay is optional"。
// 由 usb-proxy.cpp 在 ep0_loop 前启动，通过 ep 全局写入。
// 为避免与 proxy.cpp 端点线程耦合，注入走独立路径：每 1ms 轮询挂起位移。
void* synthetic_injector_thread(void* arg) {
    (void)arg;
    apply_rt_thread_policy();  // RT：realtime=fifo:98 + CPU affinity
    int ep_num = g_synth_ep_num;
    int fd = g_synth_fd;
    printf("synthetic_injector: ep_num=%d (rt)\n", ep_num);
    while (!please_stop_ep0) {
        uint8_t report[8];
        int len = ttbox_usbproxy::mouse_control_build_synthetic_report(report, sizeof(report));
        if (len > 0) {
            struct usb_raw_transfer_io io;
            io.inner.ep = static_cast<__u16>(ep_num);
            io.inner.flags = 0;
            io.inner.length = static_cast<__u32>(len);
            std::memcpy(io.data, report, len);
            int rv = usb_raw_ep_write(fd, (struct usb_raw_ep_io*)&io);
            if (rv < 0 && errno != ESHUTDOWN && errno != EINTR && errno != EINVAL) {
                fprintf(stderr, "[injector] ep_write rv=%d errno=%d, retrying\n", rv, errno);
            }
        } else {
            // 无挂起位移时休眠，避免空转
            usleep(200);
        }
    }
    return nullptr;
}

// ── synthetic ep0 辅助 ───────────────────────────────────────────
// hex 字符串 → 字节（HID report descriptor）
static int hex_decode(const std::string& hex, uint8_t* out, int cap) {
    int n = 0;
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int hi = val(hex[i]);
        int lo = val(hex[i + 1]);
        if (hi < 0 || lo < 0) break;
        if (n >= cap) break;
        out[n++] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return n;
}

// 写 ep0 IN 响应（usb_raw_transfer_io 内嵌 4096 数据缓冲，避免 flexible array 溢出）
static void synth_ep0_write(int fd, const void* data, int len) {
    struct usb_raw_transfer_io io;
    io.inner.ep = 0;
    io.inner.flags = 0;
    io.inner.length = static_cast<__u32>(len);
    std::memcpy(io.data, data, len);
    usb_raw_ep0_write(fd, (struct usb_raw_ep_io*)&io);
}

// 按请求 wLength 截断后写 ep0 IN 响应
static void synth_ep0_write_limited(int fd, const void* data, int len, int wlen) {
    if (wlen < len) len = wlen;
    synth_ep0_write(fd, data, len);
}

// 构造 UTF-16LE 字符串描述符
static void synth_string_desc(const std::string& s, uint8_t* out, int cap) {
    int n = 0;
    out[n++] = 0;
    out[n++] = USB_DT_STRING;
    for (char c : s) {
        if (n + 2 >= cap) break;
        out[n++] = static_cast<uint8_t>(c);
        out[n++] = 0;
    }
    out[0] = static_cast<uint8_t>(n);
}

static uint8_t g_synth_hid_report[128];
static int g_synth_hid_report_len = 0;

// ── synthetic ep0 事件循环 ───────────────────────────────────────
// 不依赖 libusb；直接服务合成描述符，处理 SET_CONFIGURATION。
void synthetic_ep0_loop(int fd) {
    // 预解码 HID report descriptor
    g_synth_hid_report_len = hex_decode(g_gadget_config.hid_report_desc_hex,
                                        g_synth_hid_report,
                                        sizeof(g_synth_hid_report));

    // 字符串描述符缓存
    uint8_t str_manu[64], str_prod[64], str_ser[64], str_cfg[64];
    synth_string_desc(g_gadget_config.manufacturer, str_manu, sizeof(str_manu));
    synth_string_desc(g_gadget_config.product, str_prod, sizeof(str_prod));
    synth_string_desc(g_gadget_config.serial, str_ser, sizeof(str_ser));
    synth_string_desc(g_gadget_config.configuration, str_cfg, sizeof(str_cfg));
    bool ep_enabled = false;

    while (!please_stop_ep0) {
        struct usb_raw_control_event event;
        event.inner.type = 0;
        event.inner.length = sizeof(event.ctrl);
        usb_raw_event_fetch(fd, (struct usb_raw_event*)&event);

        if (event.inner.length == 4294967295) break;

        if (event.inner.type == USB_RAW_EVENT_RESET ||
            event.inner.type == USB_RAW_EVENT_DISCONNECT) {
            // 禁用端点，等待重新枚举
            if (ep_enabled) {
                usb_raw_ep_disable(fd, 0x81);
                ep_enabled = false;
            }
            continue;
        }
        if (event.inner.type != USB_RAW_EVENT_CONTROL) continue;

        const struct usb_ctrlrequest& ctrl = event.ctrl;
        uint8_t bmRequestType = ctrl.bRequestType;
        uint8_t bRequest = ctrl.bRequest;
        bool in = (bmRequestType & USB_DIR_IN) != 0;

        // 标准设备请求
        if ((bmRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
            if (in && bRequest == USB_REQ_GET_DESCRIPTOR) {
                uint8_t type = static_cast<uint8_t>(ctrl.wValue >> 8);
                uint8_t index = static_cast<uint8_t>(ctrl.wValue & 0xFF);
                switch (type) {
                case USB_DT_DEVICE:
                    synth_ep0_write_limited(fd, &g_synth.dev, sizeof(g_synth.dev),
                                            ctrl.wLength);
                    break;
                case USB_DT_CONFIG: {
                    // config(9) + interface(9) + HID desc(9) + endpoint(9) = 36 字节
                    uint8_t cfg[36];
                    std::memset(cfg, 0, sizeof(cfg));
                    std::memcpy(cfg, &g_synth.cfg, sizeof(g_synth.cfg));
                    std::memcpy(cfg + 9, &g_synth.iface, sizeof(g_synth.iface));
                    cfg[18] = 9; cfg[19] = USB_DT_HID;
                    cfg[20] = 0x11; cfg[21] = 0x01; cfg[22] = 0x00;
                    cfg[23] = 1;                      // bNumDescriptors
                    cfg[24] = HID_DT_REPORT;          // 报告描述符类型 0x22
                    cfg[25] = static_cast<uint8_t>(g_synth_hid_report_len & 0xFF);
                    cfg[26] = static_cast<uint8_t>((g_synth_hid_report_len >> 8) & 0xFF);
                    std::memcpy(cfg + 27, &g_synth.ep, sizeof(g_synth.ep));  // 9 字节
                    synth_ep0_write_limited(fd, cfg, sizeof(cfg), ctrl.wLength);
                    break;
                }
                case USB_DT_STRING:
                    if (index == 0) {
                        uint8_t lang[] = {4, USB_DT_STRING, 0x09, 0x04};
                        synth_ep0_write_limited(fd, lang, sizeof(lang), ctrl.wLength);
                    } else if (index == 1) {
                        synth_ep0_write_limited(fd, str_manu, str_manu[0], ctrl.wLength);
                    } else if (index == 2) {
                        synth_ep0_write_limited(fd, str_prod, str_prod[0], ctrl.wLength);
                    } else if (index == 3) {
                        synth_ep0_write_limited(fd, str_ser, str_ser[0], ctrl.wLength);
                    } else {
                        usb_raw_ep0_stall(fd);
                    }
                    break;
                case USB_DT_HID: {
                    // 接口 HID 描述符（class=0x21）：报告描述符长度等
                    uint8_t hid_desc[9] = {
                        9, USB_DT_HID, 0x11, 0x01, 0x00,
                        1, static_cast<uint8_t>(g_synth_hid_report_len & 0xFF),
                        static_cast<uint8_t>((g_synth_hid_report_len >> 8) & 0xFF),
                        0};
                    synth_ep0_write_limited(fd, hid_desc, sizeof(hid_desc), ctrl.wLength);
                    break;
                }
                case HID_DT_REPORT:
                    synth_ep0_write_limited(fd, g_synth_hid_report,
                                            g_synth_hid_report_len, ctrl.wLength);
                    break;
                default:
                    usb_raw_ep0_stall(fd);
                    break;
                }
                continue;
            }
            if (bRequest == USB_REQ_SET_CONFIGURATION) {
                printf("synthetic_ep0: SET_CONFIGURATION(%d)\n", ctrl.wValue);
                // status stage ACK（OUT 请求需读取零长度包完成握手）
                struct usb_raw_ep_io ack;
                ack.ep = 0;
                ack.flags = 0;
                ack.length = 0;
                usb_raw_ep0_read(fd, &ack);
                if (ctrl.wValue != 0) {
                    usb_raw_configure(fd);
                    // 启用 IN 中断端点 0x81（仅首次）
                    if (!ep_enabled) {
                        struct usb_endpoint_descriptor ep = g_synth.ep;
                        g_synth_ep_num = usb_raw_ep_enable(fd, &ep);
                        g_synth_fd = fd;
                        ep_enabled = true;
                        // 启动注入线程
                        pthread_t tid;
                        pthread_create(&tid, nullptr, synthetic_injector_thread, nullptr);
                        pthread_detach(tid);
                    }
                } else {
                    // SET_CONFIGURATION(0) = 解除配置
                    if (ep_enabled) {
                        usb_raw_ep_disable(fd, 0x81);
                        ep_enabled = false;
                    }
                }
                continue;
            }
            // 其余标准请求：ACK
            continue;
        }

        // HID 类请求：SET_IDLE/SET_PROTOCOL 是 OUT（读零长包 ACK）；
        // GET_REPORT/GET_IDLE 是 IN（回 1 字节）
        if ((bmRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
            if (in) {
                uint8_t zero = 0;
                synth_ep0_write(fd, &zero, 1);
            } else {
                struct usb_raw_ep_io ack;
                ack.ep = 0;
                ack.flags = 0;
                ack.length = 0;
                usb_raw_ep0_read(fd, &ack);
            }
            continue;
        }

        // 其他请求：STALL
        usb_raw_ep0_stall(fd);
    }
    printf("synthetic_ep0: exit\n");
}
