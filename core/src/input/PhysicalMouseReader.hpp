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
private:
    void loop();
    void event_socket_loop();
    bool find_device(std::string* out) const;
    bool start_event_socket(std::string* error);
    std::string device_; std::string event_socket_path_;
    int fd_=-1; int event_fd_=-1; std::atomic<bool> running_{false}; std::thread thread_; std::thread event_thread_;
    std::atomic<uint16_t> buttons_{0}; std::atomic<int32_t> rel_x_{0},rel_y_{0};
};
}
