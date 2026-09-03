// PhysicalMouseReader.cpp — 真实物理鼠标输入读取。
// 优先读取 evdev；完整 USB 透传模式下，usb-proxy 独占鼠标后回退到官方 event.sock。
#include "input/PhysicalMouseReader.hpp"
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <cstring>
#endif

namespace ttbox::core::input {
#if !defined(_WIN32)
namespace {
constexpr uint16_t kMagic = 0x4F50;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kSubscribeReq = 8;
constexpr uint8_t kSubscribeAck = 9;
constexpr uint8_t kButtonEvent = 11;
#pragma pack(push, 1)
struct Header { uint16_t magic; uint8_t version; uint8_t type; uint32_t request_id; };
#pragma pack(pop)
}
#endif

bool PhysicalMouseReader::find_device(std::string* out) const {
#if defined(_WIN32)
 (void)out; return false;
#else
 DIR* d=opendir("/sys/class/input"); if(!d)return false; dirent* e;
 while((e=readdir(d))){
  if(strncmp(e->d_name,"event",5)!=0)continue;
  std::string dev="/dev/input/"+std::string(e->d_name);
  int f=open(dev.c_str(),O_RDONLY|O_NONBLOCK); if(f<0)continue;
  unsigned long ev_bits[(EV_MAX + 64) / 64]{};
  unsigned long key_bits[(KEY_MAX + 64) / 64]{};
  const bool has_rel = ioctl(f, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0 &&
                       (ev_bits[EV_REL/(sizeof(unsigned long)*8)] & (1UL << (EV_REL%(sizeof(unsigned long)*8))));
  const bool has_mouse_key = ioctl(f, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) >= 0 &&
                             ((key_bits[BTN_LEFT/(sizeof(unsigned long)*8)] & (1UL << (BTN_LEFT%(sizeof(unsigned long)*8)))) ||
                              (key_bits[BTN_RIGHT/(sizeof(unsigned long)*8)] & (1UL << (BTN_RIGHT%(sizeof(unsigned long)*8)))));
  close(f);
  if(has_rel && has_mouse_key){*out=dev;closedir(d);return true;}
 }
 closedir(d); return false;
#endif
}

bool PhysicalMouseReader::start_event_socket(std::string* error) {
#if defined(_WIN32)
 (void)error; return false;
#else
 event_socket_path_ = "/run/orangepi-mouse-passthrough/event.sock";
 event_fd_ = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
 if (event_fd_ < 0) { if(error)*error="创建 usb-proxy event socket 失败"; return false; }
 sockaddr_un addr{}; addr.sun_family=AF_UNIX;
 if(event_socket_path_.size() >= sizeof(addr.sun_path)) { if(error)*error="event socket 路径过长"; close(event_fd_); event_fd_=-1; return false; }
 std::memcpy(addr.sun_path,event_socket_path_.c_str(),event_socket_path_.size()+1);
 if(::connect(event_fd_,reinterpret_cast<const sockaddr*>(&addr),sizeof(addr))<0){if(error)*error="连接 usb-proxy event socket 失败: "+std::string(std::strerror(errno));close(event_fd_);event_fd_=-1;return false;}
 Header req{kMagic,kVersion,kSubscribeReq,1};
 if(::send(event_fd_,&req,sizeof(req),MSG_NOSIGNAL)!=static_cast<ssize_t>(sizeof(req))){if(error)*error="订阅 usb-proxy 按键事件失败";close(event_fd_);event_fd_=-1;return false;}
 unsigned char response[64]{}; const ssize_t n=::recv(event_fd_,response,sizeof(response),0);
 if(n<static_cast<ssize_t>(sizeof(Header))){if(error)*error="usb-proxy 按键订阅响应过短";close(event_fd_);event_fd_=-1;return false;}
 Header ack{};std::memcpy(&ack,response,sizeof(ack));
 if(ack.magic!=kMagic||ack.version!=kVersion||ack.type!=kSubscribeAck){if(error)*error="usb-proxy 按键订阅响应不匹配";close(event_fd_);event_fd_=-1;return false;}
 std::fprintf(stderr, "PhysicalMouseReader: usb-proxy event.sock subscribed\\n");
 event_thread_=std::thread(&PhysicalMouseReader::event_socket_loop,this); return true;
#endif
}

bool PhysicalMouseReader::start(const std::string& requested,std::string* error){
#if defined(_WIN32)
 (void)requested; if(error)*error="Windows 不支持 evdev"; return false;
#else
 if(running_.exchange(true))return false;
 device_=requested;
 std::fprintf(stderr, "PhysicalMouseReader: start requested=%s\\n", requested.c_str());
 if (device_.empty()) {
     std::fprintf(stderr, "PhysicalMouseReader: using usb-proxy event.sock\\n");
     if(!start_event_socket(error)){std::fprintf(stderr, "PhysicalMouseReader: event.sock fallback failed: %s\\n", error ? error->c_str() : "unknown");running_=false;return false;}
     return true;
 }
 if (device_.empty()) {
     std::fprintf(stderr, "PhysicalMouseReader: evdev mouse candidate=%s\\n", device_.c_str());
 }
 fd_=open(device_.c_str(),O_RDONLY|O_NONBLOCK);
 if(fd_<0){
    const std::string evdev_error = std::strerror(errno);
    std::fprintf(stderr, "PhysicalMouseReader: evdev open failed (%s), using usb-proxy event.sock\\n", evdev_error.c_str());
    if (start_event_socket(error)) return true;
    running_=false; if(error)*error="无法打开物理鼠标: "+device_+"；event.sock 回退失败"; return false;
 }
 thread_=std::thread(&PhysicalMouseReader::loop,this); return true;
#endif
}

void PhysicalMouseReader::stop(){
#if !defined(_WIN32)
 if(!running_.exchange(false))return;
 if(fd_>=0){close(fd_);fd_=-1;}
 if(event_fd_>=0){shutdown(event_fd_,SHUT_RDWR);close(event_fd_);event_fd_=-1;}
 if(thread_.joinable())thread_.join();
 if(event_thread_.joinable())event_thread_.join();
 buttons_.store(0,std::memory_order_release);
#endif
}

void PhysicalMouseReader::loop(){
#if !defined(_WIN32)
 input_event ev{}; while(running_.load()){ if(read(fd_,&ev,sizeof(ev))!=(ssize_t)sizeof(ev)){usleep(1000);continue;} if(ev.type==EV_REL){if(ev.code==REL_X)rel_x_.fetch_add(ev.value);if(ev.code==REL_Y)rel_y_.fetch_add(ev.value);} if(ev.type==EV_KEY){uint16_t bit=0;if(ev.code==BTN_LEFT)bit=1;if(ev.code==BTN_RIGHT)bit=2;if(ev.code==BTN_MIDDLE)bit=4;if(ev.code==BTN_SIDE)bit=8;if(ev.code==BTN_EXTRA)bit=16;if(bit){if(ev.value)buttons_.fetch_or(bit);else buttons_.fetch_and((uint16_t)~bit);}} }
#endif
}

void PhysicalMouseReader::event_socket_loop(){
#if !defined(_WIN32)
 unsigned char packet[128]{};
 while(running_.load(std::memory_order_acquire) && event_fd_>=0){
   const ssize_t n=::recv(event_fd_,packet,sizeof(packet),0);
   if(n<=0){if(running_.load())usleep(1000);continue;}
   if(n<static_cast<ssize_t>(sizeof(Header)+11))continue;
   Header h{};std::memcpy(&h,packet,sizeof(h));
   if(h.magic!=kMagic||h.version!=kVersion||h.type!=kButtonEvent)continue;
   const unsigned char button=packet[8]; const unsigned char pressed=packet[9]; const unsigned char mask=packet[10];
   (void)button;
   buttons_.store(static_cast<uint16_t>(mask),std::memory_order_release);
   if(!pressed && mask==0) buttons_.store(0,std::memory_order_release);
 }
#endif
}
}
