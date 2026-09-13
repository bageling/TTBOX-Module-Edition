// PhysicalMouseReader.hpp — 真实物理鼠标 evdev 输入读取。
// 只读 /dev/input/eventN，不模拟鼠标；输出按钮位图供 AimThread 热键门控使用。
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
namespace ttbox::core::input {
class PhysicalMouseReader {
public:
    ~PhysicalMouseReader(){stop();}
    bool start(const std::string& device="", std::string* error=nullptr);
    void stop();
    uint16_t buttons() const{return buttons_.load(std::memory_order_acquire);}
    int32_t rel_x(){return rel_x_.exchange(0,std::memory_order_acq_rel);}
    int32_t rel_y(){return rel_y_.exchange(0,std::memory_order_acq_rel);}
    bool running() const{return running_.load();}
    std::atomic<uint16_t>* button_source(){return &buttons_;}
    std::string device() const{return device_;}
    // usb-proxy 按键事件通道。默认使用 TTBOX 自己的运行目录，避免读到其它服务的 event.sock。
    void set_event_socket_path(const std::string& path){ if(!path.empty()) event_socket_path_=path; }
private:
    void loop();
    void event_socket_loop();
    bool find_device(std::string* out) const;
    // 只负责建连/订阅/收 ACK；不断开时由 event_socket_loop 持有。
    bool open_event_socket(std::string* error);
    bool start_event_socket(std::string* error);
    std::string device_;
    std::string event_socket_path_="/run/ttbox-mouse-passthrough/event.sock";
    int fd_=-1; int event_fd_=-1; std::atomic<bool> running_{false}; std::thread thread_; std::thread event_thread_;
    std::atomic<uint16_t> buttons_{0};
 std::atomic<int32_t> rel_x_{0},rel_y_{0};
};
}
