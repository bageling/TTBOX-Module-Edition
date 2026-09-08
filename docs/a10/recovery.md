# AIBox 恢复指南（recovery）

## 1. 何时需要恢复

- 系统无法启动 / 内核损坏
- 配置或文件系统异常无法修复
- 需要回到出厂状态（AIBox v0.0.1）

## 2. 恢复方式：重新烧录镜像

最可靠的方式是使用官方镜像重新烧录（等效恢复出厂）：

1. 准备 TF 卡（≥8G）与烧录工具
2. 烧录 `AIBox-v0.0.1-orangepi5plus.img`（校验 SHA256 后）
3. 插入并开机，等待首次初始化完成

## 3. 系统级修复（可启动但不正常）

### 3.1 重建 systemd 服务

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now ttbox-runtime ttbox-web ttbox-hid ttbox-edid-inject
sudo systemctl status ttbox-runtime ttbox-web ttbox-hid
```

### 3.2 重新初始化 HID Package

```bash
sudo /opt/ttbox/runtime/ttbox-hid-init.sh
sudo /opt/ttbox/runtime/ttbox-hid-health --root /opt/ttbox/hid
```

### 3.3 重新注入 EDID

```bash
sudo bash /opt/ttbox/edid/inject_edid.sh
```

### 3.4 恢复锁频

```bash
sudo bash /opt/ttbox/scripts/setup_freq.sh
```

### 3.5 重建 Model Registry

```bash
sudo mkdir -p /opt/ttbox/models/registry/{installed,staging,cache,quarantine}
sudo cp /opt/ttbox/models/registry/installed/huangwa.rknn /opt/ttbox/models/registry/installed/ 2>/dev/null
echo "huangwa.rknn" | sudo tee /opt/ttbox/models/active_model.txt
```

## 4. 首次启动重新初始化

如需重新执行 firstboot（重新生成 SSH host key / 重新初始化）：

```bash
sudo rm -f /opt/ttbox/.firstboot-done
sudo systemctl restart ttbox-firstboot
```

## 5. 检查硬件

```bash
# HDMI RX
sudo v4l2-ctl -d /dev/video0 --get-dv-timings
# RKNN
ls -l /usr/lib/librknnrt.so
# RGA
ls -l /usr/lib/aarch64-linux-gnu/librga.so.2
# NPU
cat /sys/class/devfreq/fdab0000.npu/cur_freq
# HID gadget
ls -l /dev/hidg0 /dev/hidg1
```

## 6. 日志定位

```bash
journalctl -u ttbox-runtime -u ttbox-web -u ttbox-hid -n 50 --no-pager
journalctl -u ttbox-firstboot -n 50 --no-pager
```

## 7. 恢复出厂（完全重置）

```bash
# 在系统内恢复出厂配置（保留数据）
sudo rm -f /opt/ttbox/.firstboot-done
sudo rm -f /opt/ttbox/models/active_model.txt
sudo reboot
```

> 若系统完全无法启动，请直接重新烧录镜像（见第 2 节）。
