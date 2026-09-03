# AIBox 首次启动说明（first-boot）

## 1. 首次启动自动完成（ttbox-firstboot.service，一次性）

| 项 | 动作 |
|---|---|
| hostname | 设置为 `ttbox` |
| SSH host key | 重新生成（镜像内已清除，防止镜像克隆共用密钥） |
| Model Registry | 初始化 `registry/{installed,staging,cache,quarantine}` |
| HID | 配置 USB HID Gadget（键盘 + 鼠标） |
| EDID | 注入已验证 1080p240 EDID 到 HDMI RX |
| 锁频 | CPU/GPU/NPU 设为 performance（保持已验证性能配置） |

> 首次启动后会在 `/opt/ttbox/.firstboot-done` 写入标记；再次启动跳过。

## 2. 首次启动后的系统状态

- `ttbox-runtime.service`：运行中（IPC 守护，**不自动开始高负载推理**，避免无人控制持续占用资源）
- `ttbox-web.service`：运行中（`http://<设备IP>:8080`）
- `ttbox-hid.service`：运行中（HID Gadget + Package 0.0.1 激活 + 健康检查）
- `ttbox-infer.service`：**关闭**（由 Web 页面点击"启动推理"开启）

## 3. 使用流程

1. **接好信号源**：HDMI IN 接 1080p240 信号源（显示器/显卡输出）
2. **登录 Web**：浏览器打开 `http://<设备IP>:8080`
3. **验证输入**：页面上方显示温度 / CPU / GPU / NPU 频率；推理前确认 HDMI RX 有信号
4. **启动推理**：点击"启动推理"，3 秒后页面显示实时 FPS（≈240）、错误数、poll_timeout
5. **模型管理**：点击"上传模型"（.rknn），上传到 staging 后可"激活"（成为 active，推理自动使用）；active 模型禁止删除
6. **EDID**：如需重新注入，点击"重新注入 EDID (1080p240)"
7. **HID 健康**：点击"刷新"查看 HID 状态（Package 版本 / 设备 / 键鼠）
8. **停止推理**：点击"停止推理"释放 NPU

## 4. 首次登录

- SSH：`ssh ubuntu@<设备IP>`，密码 `ttbox`
- 首次登录后 `passwd` 修改密码
- Web 无需登录（本机管理工具，仅内网使用）

## 5. 首次启动检查清单

```bash
# 服务状态
systemctl is-active ttbox-firstboot ttbox-runtime ttbox-web ttbox-hid

# 首次初始化日志
journalctl -u ttbox-firstboot -n 30 --no-pager

# HDMI RX 信号
sudo v4l2-ctl -d /dev/video0 --get-dv-timings

# HID 健康
sudo /opt/ttbox/runtime/ttbox-hid-health --root /opt/ttbox/hid

# Web
curl -s http://127.0.0.1:8080/api/state | python3 -m json.tool
```

## 6. 默认配置

- 推理：黄瓦 320 INT8（预置）· 3 Worker · 8 buffers · A76 affinity(4,5,6)
- 模型目录：`/opt/ttbox/models/registry/installed/`
- 推理参数：`/opt/ttbox/config/infer.json`
- Runtime 配置：`/opt/ttbox/config/default.json`

## 7. 注意事项

- 首次启动需 1-2 分钟完成初始化，期间 Web 可能短暂不可用
- 不要删除 `/opt/ttbox/.firstboot-done`（除非需要重新初始化）
- 网络未连接时 Web 无法访问，但其余功能正常（本地化）
