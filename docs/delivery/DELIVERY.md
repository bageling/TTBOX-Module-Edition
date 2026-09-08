# TTBOX 当前交付说明

## 已验证链路

```text
HDMI/V4L2 → RGA → RKNN → sjz-XCSH DFL Decode/NMS → AimTargetMailbox → AimThread
```

板端已验证：RK3588、`/dev/video0`、2560x1440、Capture/RGA/RKNN/Decode/目标邮箱/AimThread。

## 模型

模型记录：`models/sjz_xcsh/manifest.json`

- 输入：256x256 BGR
- 输出：6 路，3 组 reg/cls
- DFL：64 通道、16 bins
- stride：8/16/32

## 启动命令

### 安全 Trace

```bash
sudo /home/ubuntu/ttbox-mainline/core/build/hardware_runner_main   --model /home/ubuntu/sjz-XCSH.rknn   --device /dev/video0 --workers 1 --seconds 10 --trace --simulate-hotkey
```

### 默认安全模式

不指定输出参数时使用 `NullHidOutput`，不会写入 HID。

### 真实输出

真实 TTBOX Gadget 输出必须显式指定：

```bash
sudo .../hardware_runner_main --model /home/ubuntu/sjz-XCSH.rknn   --device /dev/video0 --workers 1 --seconds 3 --ttbox-gadget
```

当前输出格式：Report ID 2 + 8 字节鼠标数据，目标 Gadget 为 `/dev/hidg1`。

## 安全与回滚

- 默认 Null/Trace；不要把真实输出设为默认。
- 真实测试使用短时运行并保留 `ttbox-hid-forward.service`。
- 停止 Runner 后确认：`systemctl is-active ttbox-hid-forward.service`。
- 回滚源码：`git checkout <已验证提交>`；回滚板端：重新部署上一份 `core/build/hardware_runner_main`。

## 未完成/未宣称

- 长时间真实自瞄稳定性未宣称完成。
- 自动标定、ONNX 转换、多模型选择未纳入当前交付。
- 目标部位和最终瞄准手感不作为本交付阶段验收项。
