# AIBox-v0.0.1-orangepi5plus 镜像安装指南

## 1. 镜像文件

| 文件 | 说明 |
|---|---|
| `AIBox-v0.0.1-orangepi5plus.img` | 完整烧录镜像（8.8G，烧录至 ≥8G TF 卡 / eMMC） |
| `AIBox-v0.0.1-orangepi5plus.img.sha256` | 镜像 SHA256 校验 |
| `AIBox-v0.0.1-orangepi5plus.img.xz` | xz 压缩版（分发用，约 2G） |
| `AIBox-v0.0.1-orangepi5plus.img.xz.sha256` | 压缩版 SHA256 校验 |
| `AIBox-v0.0.1-orangepi5plus-manifest.json` | 镜像版本清单（版本/SHA256/构建时间） |

> 使用前必须校验 SHA256，与 `image-manifest.json` 中记录的哈希一致。

## 2. 环境要求

- 目标板：Orange Pi 5 Plus（RK3588）
- 烧录介质：TF 卡（≥8G，推荐 16G 或以上）或 eMMC
- 烧录工具：balenaEtcher / Rufus / `dd`

## 3. 烧录步骤

### 3.1 校验文件（以 Windows 为例）

```powershell
Get-FileHash .\AIBox-v0.0.1-orangepi5plus.img -Algorithm SHA256
# 与 manifest.json 中 "sha256" 对比，必须一致
```

### 3.2 Linux / macOS 解压 + 烧录

```bash
# 解压（若使用 xz 版）
xz -dk AIBox-v0.0.1-orangepi5plus.img.xz

# 烧录（注意替换 /dev/sdX 为 TF 卡设备，务必确认）
sudo dd if=AIBox-v0.0.1-orangepi5plus.img of=/dev/sdX bs=4M conv=fsync status=progress
```

### 3.3 Windows（balenaEtcher / Rufus）

1. 选择 `AIBox-v0.0.1-orangepi5plus.img.xz`（Etcher 直接支持 xz）或先解压 `.img`
2. 选择目标 TF 卡
3. 点击 Flash

## 4. 首次启动

1. 烧录完成后插入 Orange Pi 5 Plus
2. 连接：
   - HDMI IN（接 1080p240 信号源）
   - HDMI OUT（接显示器，可选）
   - USB Host：鼠标、键盘
   - 网线（DHCP）或跳过（Web 需网络）
3. 上电，等待约 1-2 分钟完成首次初始化（hostname、SSH host key、HID Gadget、EDID、锁频）
4. 首次启动会自动执行，无需人工干预

## 5. 登录

- SSH：`ssh ubuntu@<设备IP>`
- 默认账号：`ubuntu` / 密码：`ttbox`
- 首次登录后建议立即修改密码：`passwd`

## 6. 打开 Mini Web

浏览器访问：`http://<设备IP>:8080`

> IP 不写死，设备通过 DHCP 获取。可用 `ip a` 或路由器后台查询。

Web 功能：推理启停 / 实时 FPS / 模型上传/切换/删除 / 硬件监控（温度/频率）/ EDID 重注入 / HID 健康状态。

## 7. 系统布局

```
/opt/ttbox/
├── runtime/    # C++ runtime 二进制 + 推理包装
├── models/     # registry/{installed,staging,cache,quarantine}
├── hid/        # HID Package 0.0.1
├── edid/       # 1080p240 EDID + 注入脚本
├── web/        # Mini Web (ttbox_web.py, :8080)
├── config/     # default.json / infer.json
├── scripts/    # firstboot / HID gadget / 锁频 / ONNX→RKNN 转换
└── tests/      # 硬件验收测试工具
```

## 8. 服务

| 服务 | 说明 | 状态 |
|---|---|---|
| `ttbox-firstboot.service` | 首次初始化（hostname/SSH key/EDID/gadget/锁频） | 自动，一次性 |
| `ttbox-runtime.service` | C++ Runtime 守护（IPC，不自动推理） | 自动 |
| `ttbox-web.service` | Mini Web :8080 | 自动 |
| `ttbox-hid.service` | HID Gadget + Package 激活 + 健康检查 | 自动 |
| `ttbox-infer.service` | 推理（Web 控制启停） | 默认关闭 |
| `ttbox-edid-inject.service` | EDID 注入 | 自动 |

## 9. 常见问题

- 烧录后无法启动：检查 TF 卡容量 ≥8G；重新烧录并校验 SHA256
- Web 打不开：确认设备 IP；`systemctl status ttbox-web`
- 推理 FPS 为 0：确认 HDMI IN 有 1080p240 信号（`sudo v4l2-ctl -d /dev/video0 --get-dv-timings`）
- 键鼠无透传：确认 USB Host 口插入键鼠；`systemctl status ttbox-hid`；`/opt/ttbox/runtime/ttbox-hid-health --root /opt/ttbox/hid`
