// mouse_control.cpp — TTBOX usb-proxy mouse_control 协议层（自研 0x4F50 协议）
// 分块编写：part1 全局量 + 报文编解码 + 服务骨架。
#include "mouse_control.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <fstream>
#include <jsoncpp/json/json.h>

extern bool please_stop_ep0;

// RT：realtime=fifo:98 + CPU affinity（大核）
void apply_rt_thread_policy() {
    struct sched_param sp{};
    sp.sched_priority = 98;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        fprintf(stderr, "[rt] pthread_setschedparam(SCHED_FIFO,98) failed: %s\n",
                strerror(errno));
    }
    // CPU affinity：优先绑到最后核（RK3588 大核），失败不致命
    cpu_set_t set;
    CPU_ZERO(&set);
    int ncpu = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    int target = ncpu - 1;  // 板端 run_usb_proxy.sh 用 cpu_affinity=7（8 核的最后核）
    if (target >= 0 && target < CPU_SETSIZE) {
        CPU_SET(target, &set);
        pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    }
}

namespace ttbox_usbproxy {

MouseControlState g_state;
GadgetConfig g_gadget_config;

namespace {

constexpr int kMaxPayload = 4096;

struct ServerThreads {
    pthread_t cmd_thread;
    pthread_t event_thread;
    int cmd_listen_fd = -1;
    int event_listen_fd = -1;
    std::atomic<bool> running{false};
} g_srv;

int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// 编码头部
void encode_header(uint8_t* buf, uint8_t type, uint32_t rid) {
    PacketHeader h{kMagic, kVersion, type, rid};
    std::memcpy(buf, &h, sizeof(h));
}

// 发送完整报文到 fd
bool send_packet(int fd, uint8_t type, uint32_t rid,
                 const void* payload, size_t plen) {
    uint8_t buf[sizeof(PacketHeader) + kMaxPayload];
    if (plen > kMaxPayload) return false;
    encode_header(buf, type, rid);
    if (plen) std::memcpy(buf + sizeof(PacketHeader), payload, plen);
    size_t total = sizeof(PacketHeader) + plen;
    ssize_t sent = ::send(fd, buf, total, MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(total);
}

// 发送错误响应
void send_error(int fd, uint32_t rid, uint16_t code, const char* text) {
    uint8_t payload[512];
    size_t tlen = std::strlen(text);
    if (tlen > 500) tlen = 500;
    std::memcpy(payload, &code, 2);
    uint16_t sz = static_cast<uint16_t>(tlen);
    std::memcpy(payload + 2, &sz, 2);
    std::memcpy(payload + 4, text, tlen);
    send_packet(fd, kErrorResp, rid, payload, 4 + tlen);
}

// ── GET_CONFIG_RESP 编码（字段顺序一致）──
void encode_config_payload(std::vector<uint8_t>& out) {
    const GadgetConfig& c = g_gadget_config;
    out.reserve(64 + c.manufacturer.size() + c.product.size() +
                c.serial.size() + c.configuration.size() +
                c.hid_report_desc_hex.size());
    auto push16 = [&](uint16_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>(v >> 8));
    };
    auto push8 = [&](uint8_t v) { out.push_back(v); };
    auto push_str = [&](const std::string& s) {
        uint16_t n = static_cast<uint16_t>(s.size());
        push16(n);
        out.insert(out.end(), s.begin(), s.end());
    };
    push16(c.usb_vid);
    push16(c.usb_pid);
    push16(c.usb_bcd_usb);
    push16(c.usb_bcd_device);
    push8(c.usb_device_class);
    push8(c.usb_device_subclass);
    push8(c.usb_device_protocol);
    push16(c.usb_max_power);   // 协议 <HHHHBBBHBBBB：max_power 是 u16
    push8(c.hid_protocol);
    push8(c.hid_subclass);
    push8(c.hid_report_length);
    push8(c.hid_interval);
    push_str(c.manufacturer);
    push_str(c.product);
    push_str(c.serial);
    push_str(c.configuration);
    push_str(c.hid_report_desc_hex);
}

// ── SET_CONFIG 解码（协议逐字节对应）──
// 输入：payload 指向固定字段+字符串区（不含 apply_now 字节）
bool decode_config_payload(const uint8_t* payload, size_t plen) {
    auto rd16 = [&](size_t& off) -> uint16_t {
        uint16_t v = 0;
        if (off + 2 > plen) return 0;
        v = static_cast<uint16_t>(payload[off] | (payload[off + 1] << 8));
        off += 2;
        return v;
    };
    auto rd8 = [&](size_t& off) -> uint8_t {
        if (off + 1 > plen) return 0;
        return payload[off++];
    };
    auto rd_str = [&](size_t& off) -> std::string {
        if (off + 2 > plen) return "";
        uint16_t n = static_cast<uint16_t>(payload[off] | (payload[off + 1] << 8));
        off += 2;
        if (off + n > plen) return "";
        std::string s(reinterpret_cast<const char*>(payload + off), n);
        off += n;
        return s;
    };

    size_t off = 0;
    GadgetConfig c;
    c.usb_vid = rd16(off);
    c.usb_pid = rd16(off);
    c.usb_bcd_usb = rd16(off);
    c.usb_bcd_device = rd16(off);
    c.usb_device_class = rd8(off);
    c.usb_device_subclass = rd8(off);
    c.usb_device_protocol = rd8(off);
    c.usb_max_power = rd16(off);   // 协议 <HHHHBBBHBBBB：u16
    c.hid_protocol = rd8(off);
    c.hid_subclass = rd8(off);
    c.hid_report_length = rd8(off);
    c.hid_interval = rd8(off);
    c.manufacturer = rd_str(off);
    c.product = rd_str(off);
    c.serial = rd_str(off);
    c.configuration = rd_str(off);
    c.hid_report_desc_hex = rd_str(off);
    g_gadget_config = c;
    return true;
}

// ── SET_CONFIG 持久化：写回 gadget-config.json（重启后生效）──
int persist_gadget_config() {
    const char* cfg_path = getenv("USB_PROXY_GADGET_CONFIG_FILE");
    if (!cfg_path) cfg_path = "/opt/ttbox/usbproxy/gadget-config.json";
    Json::Value root;
    const GadgetConfig& c = g_gadget_config;
    root["usb_vid"] = c.usb_vid;
    root["usb_pid"] = c.usb_pid;
    root["usb_bcd_usb"] = c.usb_bcd_usb;
    root["usb_bcd_device"] = c.usb_bcd_device;
    root["usb_device_class"] = c.usb_device_class;
    root["usb_device_subclass"] = c.usb_device_subclass;
    root["usb_device_protocol"] = c.usb_device_protocol;
    root["usb_max_power"] = c.usb_max_power;
    root["hid_protocol"] = c.hid_protocol;
    root["hid_subclass"] = c.hid_subclass;
    root["hid_report_length"] = c.hid_report_length;
    root["hid_interval"] = c.hid_interval;
    root["usb_manufacturer"] = c.manufacturer;
    root["usb_product"] = c.product;
    root["usb_serial"] = c.serial;
    root["usb_configuration"] = c.configuration;
    root["hid_report_desc_hex"] = c.hid_report_desc_hex;
    std::ofstream ofs(cfg_path, std::ios::trunc);
    if (!ofs.is_open()) {
        fprintf(stderr, "persist_gadget_config: cannot open %s\n", cfg_path);
        return -1;
    }
    ofs << root.toStyledString();
    ofs.close();
    printf("set-config saved: vid=%04x pid=%04x\n", c.usb_vid, c.usb_pid);
    return 0;
}

// ── 命令分发：处理单个 cmd.sock 连接 ────────────────────────────
void handle_cmd_connection(int fd) {
    uint8_t buf[sizeof(PacketHeader) + kMaxPayload];
    for (;;) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        if (n < static_cast<ssize_t>(sizeof(PacketHeader))) continue;
        PacketHeader h;
        std::memcpy(&h, buf, sizeof(h));
        if (h.magic != kMagic || h.version != kVersion) {
            send_error(fd, h.request_id, 1, "bad magic/version");
            continue;
        }
        const uint8_t* payload = buf + sizeof(PacketHeader);
        size_t plen = static_cast<size_t>(n) - sizeof(PacketHeader);

        switch (h.type) {
        case kPingReq:
            send_packet(fd, kPingResp, h.request_id, nullptr, 0);
            break;

        case kMoveCmd: {
            // payload <iii dx,dy,wheel
            if (plen < 12) { send_error(fd, h.request_id, 2, "short move"); break; }
            int32_t dx, dy, wheel;
            std::memcpy(&dx, payload, 4);
            std::memcpy(&dy, payload + 4, 4);
            std::memcpy(&wheel, payload + 8, 4);
            g_state.pending_dx.fetch_add(dx);
            g_state.pending_dy.fetch_add(dy);
            g_state.pending_wheel.fetch_add(wheel);
            g_state.move_count.fetch_add(1);
            g_state.last_move_ts_us.store(now_us());
            break;
        }

        case kButtonCmd: {
            // payload <BB button, action
            if (plen < 2) { send_error(fd, h.request_id, 3, "short button"); break; }
            uint8_t button = payload[0];
            uint8_t action = payload[1];
            uint8_t bit = static_cast<uint8_t>(1u << (button - 1));
            uint8_t cur = g_state.button_mask.load();
            uint8_t next = cur;
            if (action == kActDown || action == kActClick) next |= bit;
            else if (action == kActUp) next &= static_cast<uint8_t>(~bit);
            g_state.button_mask.store(next);
            break;
        }

        case kGetStateReq: {
            // payload <BQ mask, timestamp_ns
            uint8_t resp[9] = {0};
            resp[0] = g_state.button_mask.load();
            int64_t ts = now_ns();
            std::memcpy(resp + 1, &ts, 8);
            send_packet(fd, kGetStateResp, h.request_id, resp, sizeof(resp));
            break;
        }

        case kGetConfigReq: {
            std::vector<uint8_t> cfg;
            encode_config_payload(cfg);
            send_packet(fd, kGetConfigResp, h.request_id, cfg.data(), cfg.size());
            break;
        }

        case kSetConfigReq: {
            // payload: <B apply_now + encode_config
            if (plen < 1) { send_error(fd, h.request_id, 5, "short set-config"); break; }
            bool apply_now = payload[0] != 0;
            if (decode_config_payload(payload + 1, plen - 1)) {
                // 持久化到 gadget-config.json（重启后生效）
                if (persist_gadget_config() != 0) {
                    send_error(fd, h.request_id, 6, "config save failed");
                    break;
                }
                send_packet(fd, kSetConfigResp, h.request_id, "\x01", 1);
                if (apply_now) {
                    // 设计：usb-proxy 重启 + Windows 重新枚举。
                    // 直接退出进程，由 systemd Restart=always 以新配置拉起。
                    fprintf(stderr, "set-config apply-now: restarting to re-enumerate\n");
                    fflush(stdout);
                    fflush(stderr);
                    _exit(0);
                }
            } else {
                send_error(fd, h.request_id, 5, "bad set-config payload");
            }
            break;
        }

        default:
            send_error(fd, h.request_id, 4, "unsupported command");
            break;
        }
    }
    ::close(fd);
}

// ── event.sock 订阅者处理：SUBSCRIBE → ACK → 推送状态快照 ──────────
void handle_event_connection(int fd) {
    uint8_t buf[sizeof(PacketHeader) + kMaxPayload];
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n < static_cast<ssize_t>(sizeof(PacketHeader))) { ::close(fd); return; }
    PacketHeader h;
    std::memcpy(&h, buf, sizeof(h));
    if (h.magic != kMagic || h.version != kVersion || h.type != kSubscribeReq) {
        ::close(fd);
        return;
    }
    send_packet(fd, kSubscribeAck, h.request_id, nullptr, 0);
    {
        std::lock_guard<std::mutex> lk(g_state.subscribers_mutex);
        g_state.subscribers.push_back(fd);
    }
    // 保持连接；发送失败时移除订阅者。周期性推送 STATE_SNAPSHOT（<BQ mask, timestamp_ns）
    for (;;) {
        uint8_t ev[9] = {0};
        ev[0] = g_state.button_mask.load();
        int64_t ts = now_ns();
        std::memcpy(ev + 1, &ts, 8);
        if (!send_packet(fd, kStateSnapshot, 0, ev, sizeof(ev))) break;
        ::usleep(100000);  // 100ms 周期快照（订阅循环兼容）
    }
    {
        std::lock_guard<std::mutex> lk(g_state.subscribers_mutex);
        auto it = std::find(g_state.subscribers.begin(), g_state.subscribers.end(), fd);
        if (it != g_state.subscribers.end()) g_state.subscribers.erase(it);
    }
    ::close(fd);
}

// ── 监听线程：accept 循环 ────────────────────────────────────────
void* cmd_listen_loop(void* arg) {
    apply_rt_thread_policy();  // RT 线程
    int listen_fd = *static_cast<int*>(arg);
    while (g_srv.running.load()) {
        int cfd = ::accept(listen_fd, nullptr, nullptr);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            if (errno == EBADF) break;
            ::usleep(50000);
            continue;
        }
        handle_cmd_connection(cfd);
    }
    return nullptr;
}

void* event_listen_loop(void* arg) {
    apply_rt_thread_policy();  // RT 线程
    int listen_fd = *static_cast<int*>(arg);
    while (g_srv.running.load()) {
        int cfd = ::accept(listen_fd, nullptr, nullptr);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            if (errno == EBADF) break;
            ::usleep(50000);
            continue;
        }
        handle_event_connection(cfd);
    }
    return nullptr;
}

int create_listen_socket(const char* path) {
    ::unlink(path);
    int fd = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) { perror("socket"); return -1; }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        ::close(fd);
        return -1;
    }
    ::chmod(path, 0666);
    if (::listen(fd, 8) < 0) {
        perror("listen");
        ::close(fd);
        return -1;
    }
    return fd;
}

}  // namespace

// ── 公开接口：启动 / 停止 ────────────────────────────────────────
int mouse_control_start(const std::string& cmd_socket,
                        const std::string& event_socket,
                        bool synthetic) {
    if (g_srv.running.load()) return 0;
    g_state.synthetic_mode.store(synthetic);
    g_state.mouse_control_enabled.store(true);

    g_srv.cmd_listen_fd = create_listen_socket(cmd_socket.c_str());
    if (g_srv.cmd_listen_fd < 0) return -1;
    g_srv.event_listen_fd = create_listen_socket(event_socket.c_str());
    if (g_srv.event_listen_fd < 0) { ::close(g_srv.cmd_listen_fd); return -1; }

    g_srv.running.store(true);
    if (pthread_create(&g_srv.cmd_thread, nullptr, cmd_listen_loop,
                       &g_srv.cmd_listen_fd) != 0) {
        g_srv.running.store(false);
        ::close(g_srv.cmd_listen_fd);
        ::close(g_srv.event_listen_fd);
        return -1;
    }
    if (pthread_create(&g_srv.event_thread, nullptr, event_listen_loop,
                       &g_srv.event_listen_fd) != 0) {
        g_srv.running.store(false);
        ::close(g_srv.cmd_listen_fd);
        ::close(g_srv.event_listen_fd);
        return -1;
    }
    printf("mouse_control: cmd=%s event=%s synthetic=%d\n",
           cmd_socket.c_str(), event_socket.c_str(), synthetic ? 1 : 0);
    return 0;
}

void mouse_control_stop() {
    if (!g_srv.running.load()) return;
    g_srv.running.store(false);
    if (g_srv.cmd_listen_fd >= 0) ::close(g_srv.cmd_listen_fd);
    if (g_srv.event_listen_fd >= 0) ::close(g_srv.event_listen_fd);
    pthread_join(g_srv.cmd_thread, nullptr);
    pthread_join(g_srv.event_thread, nullptr);
    g_srv.cmd_listen_fd = -1;
    g_srv.event_listen_fd = -1;
    {
        std::lock_guard<std::mutex> lk(g_state.subscribers_mutex);
        for (int fd : g_state.subscribers) ::close(fd);
        g_state.subscribers.clear();
    }
    g_state.mouse_control_enabled.store(false);
}

// 物理 HID 报告到达时：将挂起 AI 位移合并进 X/Y（int16 LE）。
// 布局: [0]=report_id, [1..2]=buttons u16 LE, [x_offset..+2]=X, [y_offset..+2]=Y
bool mouse_control_merge_report(uint8_t* data, uint32_t len) {
    if (!g_state.mouse_control_enabled.load()) return false;
    int xo = g_state.x_offset.load();
    int yo = g_state.y_offset.load();
    int rl = g_state.report_len.load();
    if (xo < 1 || yo < 1 || xo + 2 > static_cast<int>(len) ||
        yo + 2 > static_cast<int>(len)) {
        return false;
    }
    int32_t dx = g_state.pending_dx.exchange(0);
    int32_t dy = g_state.pending_dy.exchange(0);
    if (dx == 0 && dy == 0) return false;

    auto read_i16 = [&](int off) -> int32_t {
        return static_cast<int16_t>(static_cast<uint16_t>(data[off]) |
                                    (static_cast<uint16_t>(data[off + 1]) << 8));
    };
    auto write_i16 = [&](int off, int32_t v) {
        if (v < -32768) v = -32768;
        if (v > 32767) v = 32767;
        uint16_t uv = static_cast<uint16_t>(static_cast<int16_t>(v));
        data[off] = static_cast<uint8_t>(uv & 0xFF);
        data[off + 1] = static_cast<uint8_t>(uv >> 8);
    };
    write_i16(xo, read_i16(xo) + dx);
    write_i16(yo, read_i16(yo) + dy);
    g_state.merge_count.fetch_add(1);
    g_state.last_move_ts_us.store(now_us());
    (void)rl;
    return true;
}

// 物理报告解析：更新按钮掩码 + 通知订阅者（逐按钮事件）
// BUTTON_EVENT payload = <BBBQ button, pressed(1=down/0=up), mask, timestamp_ns
void mouse_control_notify_physical_report(const uint8_t* data, uint32_t len) {
    if (!g_state.mouse_control_enabled.load()) return;
    // Logitech 布局: [1..2] buttons u16 LE；其他布局退化读取 [1]
    uint8_t mask = 0;
    if (len >= 3) {
        mask = static_cast<uint8_t>(data[1] | (data[2] << 8));
    } else if (len >= 2) {
        mask = data[1];
    }
    uint8_t old = g_state.button_mask.exchange(mask);
    uint8_t changed = static_cast<uint8_t>(old ^ mask);
    if (!changed) return;

    int64_t ts_ns = now_ns();
    uint32_t rid = 0;
    std::lock_guard<std::mutex> lk(g_state.subscribers_mutex);
    // 每个变化的按钮发一条 BUTTON_EVENT：button=1..5, pressed, mask, ts
    for (int b = 1; b <= 5; b++) {
        uint8_t bit = static_cast<uint8_t>(1u << (b - 1));
        if (!(changed & bit)) continue;
        uint8_t ev[11] = {0};
        ev[0] = static_cast<uint8_t>(b);
        ev[1] = (mask & bit) ? 1 : 0;
        ev[2] = mask;
        std::memcpy(ev + 3, &ts_ns, 8);
        for (int fd : g_state.subscribers) {
            send_packet(fd, kButtonEvent, rid, ev, sizeof(ev));
        }
    }
}

// synthetic 模式：从挂起位移构造合成 HID 报告
// boot 鼠标布局(与 gadget-config.json 描述符一致, 无 report_id):
// [0]=buttons u8, [1]=X int8, [2]=Y int8, [3]=wheel int8
int mouse_control_build_synthetic_report(uint8_t* out, uint32_t cap) {
    int32_t dx = g_state.pending_dx.exchange(0);
    int32_t dy = g_state.pending_dy.exchange(0);
    int32_t wheel = g_state.pending_wheel.exchange(0);
    if (dx == 0 && dy == 0 && wheel == 0) return 0;
    if (cap < 4) return 0;
    std::memset(out, 0, 4);
    out[0] = g_state.button_mask.load();
    auto put_i8 = [&](int off, int32_t v) {
        if (v < -128) v = -128;
        if (v > 127) v = 127;
        out[off] = static_cast<uint8_t>(static_cast<int8_t>(v));
    };
    put_i8(1, dx);
    put_i8(2, dy);
    put_i8(3, wheel);
    g_state.merge_count.fetch_add(1);
    g_state.last_move_ts_us.store(now_us());
    return 4;
}

}  // namespace ttbox_usbproxy
