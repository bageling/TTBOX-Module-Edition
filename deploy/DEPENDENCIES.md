# TTBOX 依赖清单（依赖管理与部署说明）

> 本文档列出 TTBOX 在 RK3588 目标板（Orange Pi 5 Plus / Ubuntu 22.04 aarch64）上的**全部运行依赖**，
> 以及仓库内已随附的依赖文件。任何新板部署按此清单准备即可，无需联网下载大文件。

## 一、系统包（apt，需 root）

| 包 | 用途 | 安装命令 |
|---|---|---|
| `cmake` `g++` `make` `pkg-config` | 编译工具链 | `apt-get install -y cmake g++ make pkg-config` |
| `librga-dev` `librga2` | RGA 硬件缩放（头文件在 `/usr/include/rga/`） | `apt-get install -y librga-dev librga2` |
| `libjpeg-dev` | JPEG 编解码（预览流） | `apt-get install -y libjpeg-dev` |
| `v4l-utils` | V4L2 调试工具（v4l2-ctl） | `apt-get install -y v4l-utils` |
| `libopencv-dev` | OpenCV 4.x（预览检测框绘制：身体框+头部小框+标签） | `apt-get install -y libopencv-dev` |
| `python3-flask` | Web 后端（ttbox-web 插件） | `apt-get install -y python3-flask` |
| `python3-waitress` | WSGI 服务器（ttbox-web 生产运行） | `apt-get install -y python3-waitress` |
| `python3-numpy` | 数值计算（插件/测试脚本） | `apt-get install -y python3-numpy` |

> 说明：Ubuntu 22.04 (jammy) 的 apt 源自带上述全部包，无需 pip3。OpenCV 版本 4.5.4。

## 二、仓库随附依赖（本仓库已入库，部署时直接复制）

| 文件 | 仓库路径 | 部署目标 | 用途 |
|---|---|---|---|
| librknnrt.so (5.2MB) | `lib/librknnrt.so` | `/usr/local/lib/librknnrt.so` | RKNN NPU 运行时（同时支持 /dev/rknpu 与 /dev/dri/renderD DRM 模式） |
| rknn_api.h | `third_party/rknn/rknn_api.h` | `/usr/local/include/rknn/` | RKNN 推理 API 头文件 |
| rknn_matmul_api.h | `third_party/rknn/rknn_matmul_api.h` | `/usr/local/include/rknn/` | RKNN 矩阵乘 API 头文件 |
| 检测模型 | `models/installed/jwdl_sjzv11/model.rknn` | `/opt/ttbox/models/installed/jwdl_sjzv11/model.rknn` | 256x256 INT8 7类 6输出 YOLO 模型 |

安装命令（板端）：
```bash
sudo cp lib/librknnrt.so /usr/local/lib/
sudo ldconfig
sudo mkdir -p /usr/local/include/rknn
sudo cp third_party/rknn/rknn_api.h third_party/rknn/rknn_matmul_api.h /usr/local/include/rknn/
# 模型复制到部署目录
sudo mkdir -p /opt/ttbox/models/installed/jwdl_sjzv11
sudo cp models/installed/jwdl_sjzv11/model.rknn /opt/ttbox/models/installed/jwdl_sjzv11/
```

## 三、HDMI RX 链路（内核/设备树）

| 项 | 说明 |
|---|---|
| HDMI RX overlay | `/boot/extlinux/extlinux.conf` 追加 `fdtoverlays rk3588-hdmirx.dtbo`，重启后出现 `/dev/video0` |
| 内核模块 | `CONFIG_VIDEO_ROCKCHIP_HDMIRX=y`（已编译进内核，Ubuntu 官方内核自带） |
| NPU 设备 | DRM 模式：`/dev/dri/renderD129`（librknnrt.so 兼容） |
| EDID 注入 | `deploy/inject_edid.sh`（写 sysfs 节点切换 EDID 版本：1=340M/1080p，2=600M/4K） |
| 生产配置模板 | `deploy/config/default.json.prod`（rgb/0.25/640/安全基线，部署时复制到 /opt/ttbox/config/） |

EDID 注入（新板首次部署必须执行，否则 PC 端虚拟屏只能枚举 1080p）：
```bash
sudo bash deploy/inject_edid.sh 2   # 注入 600M EDID，PC 切换 4K 输出
cat /sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/edid  # 应输出 2
```

## 四、systemd 服务（deploy/systemd/）

| 服务文件 | 服务名 | 说明 |
|---|---|---|
| ttbox-core.service | ttbox-core | C++ 核心（V4L2 Capture + RGA + RKNN + DecodeNMS + AimThread） |
| ttbox-web.service | ttbox-web | Web 控制台 0.0.0.0:8080 |
| ttbox-preview.service | ttbox-preview | 预览流 127.0.0.1:8082 |
| ttbox-edid.service | ttbox-edid | 开机 EDID 注入（oneshot） |
| ttbox-usbproxy.service | ttbox-usbproxy | USB 鼠标代理（HID） |

部署命令：
```bash
sudo cp deploy/systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now ttbox-core ttbox-web ttbox-preview ttbox-edid
# usbproxy 按需启用
```

## 五、目录部署结构（/opt/ttbox）

```
/opt/ttbox/
├── bin/ttbox_core_main          # core 编译产物（704KB，aarch64）
├── config/default.json          # 生产配置（安全基线：output_enabled=false 等）
├── models/installed/jwdl_sjzv11/model.rknn
├── plugins/web/                 # Web 插件（Flask，8080）
├── plugins/preview/             # 预览插件（8082）
├── framework/                   # Python 插件框架
├── ttbox_motion/                # 领域包（训练/标定）
├── edid/inject_edid.sh          # EDID 注入脚本
└── runtime/                     # 运行时目录
```

## 六、编译（core）

```bash
cd core
mkdir -p build && cd build
cmake .. && make -j4
# 产物: ttbox_core_main
```

关键 CMake 依赖探测：
- RGA：`find_path(RGA_INCLUDE_DIR im2d.h PATHS /usr/local/include /usr/include /usr/include/rga)`
- RKNN：`find_path(RKNN_INCLUDE_DIR rknn_api.h PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rknn)`
- OpenCV：`find_package(OpenCV COMPONENTS core imgproc)`

## 七、安全基线（部署后必须确认）

```bash
python3 -c "import json;c=json.load(open('/opt/ttbox/config/default.json'));print(c['output_enabled'],c['mouse'])"
# output_enabled=False  mouse.enabled=False  calibrating=False
```
