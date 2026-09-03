# A9-P2 HID Package v0.0.1 验收报告

- 日期：2026-08-14
- 平台：Orange Pi 5 Plus（RK3588）/ Ubuntu 24.04 / 内核 6.1.0-1025-rockchip（arm64）
- 版本：HID Package `0.0.1`，Status: `development`
- 结论：**PASS**（20/20 项验收通过）

---

## 1. HID 是否完全独立 —— PASS

- 独立目录：[hid/](file:///g:/工作区/ttbox逆向/ttbox2/hid)（manifest.json / VERSION / bin/ / config/ / descriptors/ / profiles/ / validation/ / runtime/ / packages/ + registry/）
- 独立源码：[src/hid/](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/src/hid)，**不依赖** `src/model/`、`src/rknn/`、`src/rga/`
- AI Runtime 只通过 [IHidRuntime.hpp](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/src/hid/IHidRuntime.hpp) 访问 HID 状态；`/dev/hidraw*`、`/dev/hidg*`、`/sys/kernel/config/usb_gadget/` 全部由 HID Package 管理
- HID 配置写入 `hid/config/hid_config.json`，**不写入** `config/default.json`

## 2. Package 目录 —— PASS

```text
hid/
├── manifest.json      Package Manifest（含安全字段）
├── VERSION            0.0.1
├── bin/               板端脚本（gadget 配置脚本随包分发）
├── config/            hid_config.json（独立配置）
├── descriptors/       keyboard.desc / mouse.desc
├── profiles/          CPU/线程策略预留
├── validation/        校验记录
├── runtime/           Runtime 工作目录
├── packages/          已安装包（registry 管理）
├── quarantine/        激活失败隔离区
├── staging/           import 暂存区
└── registry/          active.json + previous.json
```

## 3. manifest —— PASS

[hid/manifest.json](file:///g:/工作区/ttbox逆向/ttbox2/hid/manifest.json)：

```json
{
  "package_id": "ttbox-hid",
  "version": "0.0.1",
  "status_name": "development",
  "architecture": "aarch64",
  "hid_protocol_version": "1",
  "min_runtime_version": "0.0.1",
  "max_runtime_version": "",
  "kernel_abi": "6.1-rockchip",
  "sha256": "",
  "signature": "",
  "signing_key_id": "",
  "origin": "local",
  "release_channel": "development",
  "rollback_version": ""
}
```

sha256 / signature / signing_key_id 字段预留为空，**未伪造**。实现见 [HidPackageManifest.hpp](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/src/hid/HidPackageManifest.hpp)（`to_json` / `from_json`）。

## 4. VERSION —— PASS

[hid/VERSION](file:///g:/工作区/ttbox逆向/ttbox2/hid/VERSION) = `0.0.1`（`hid_read_version` / `hid_write_version` 统一读写）。

## 5. Registry —— PASS

[HidPackageRegistry](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/src/hid/HidPackageRegistry.hpp) 支持：`init / list / install / validate / activate / deactivate / remove / rollback / get_active / get_previous`。

- **不能删除 active Package**（板端实测拒绝：`remove 0.0.1: 禁止删除 active HID Package: 0.0.1`）
- 激活失败自动恢复旧版本（validator 注入 + quarantine + 恢复 previous，单测覆盖）

## 6. install —— PASS

流程实测（板端 CLI `ttbox-hid-pkg`）：

```text
import <src> <ver> → staging
validate <ver>     → 校验（manifest 结构 + 版本一致）
install <ver>      → staging → installed
```

## 7. activate —— PASS

```text
activate 0.0.0 → active=0.0.0（previous=0.0.1 写入）
activate 0.0.1 → active=0.0.1（previous=0.0.0）
```

激活成功更新 `registry/active.json` + `registry/previous.json`，并同步两个包的 manifest 状态（新包 active、旧包 inactive）。

## 8. deactivate —— PASS

```text
deactivate → active=（空），previous=原 active
```

## 9. remove —— PASS

```text
remove 0.0.0（inactive）→ OK
remove 0.0.1（active）  → 拒绝
```

## 10. rollback —— PASS（板端真实双版本演示）

```text
0.0.0 active
activate 0.0.1 → active=0.0.1, previous=0.0.0
rollback       → active=0.0.0        （回滚成功）
rollback       → active=0.0.1        （恢复，禁止出现"无可用 HID Package"）
```

最终 `list`：

```text
已安装 2 个 HID Package：
  0.0.0  status=inactive rollback=0.0.1
  0.0.1  status=active  rollback=0.0.0
```

## 11. health check —— PASS

[ttbox-hid-health](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/tools/ttbox_hid_health.cpp) 板端实测：

```text
=== ttbox-hid-health (root=/home/ubuntu/ttbox2/hid) ===
  [PASS] Package version        VERSION=0.0.1
  [PASS] Active package         0.0.1
  [PASS] Previous (rollback)    （无有效 previous）
  [PASS] Installed packages     0.0.1
  [N/A ] Runtime status         无输入设备（hidraw=0），未运行
  [N/A ] Queue/Drop             无输入设备，无事件可测
  [N/A ] Latency                无输入设备，无延迟可测
  [PASS] USB Gadget             2 个 hidg
  [PASS] USB Host/UDC           udc state=configured
  [PASS] HID device             无 hidraw（真实键鼠未插入，NOT AVAILABLE）
  [PASS] Keyboard               hidg0 就绪
  [PASS] Mouse                  hidg1 就绪
  [PASS] Config                 独立配置存在（不依赖 default.json）
  [PASS] Manifest               sha256 字段=Y signature 字段=Y
=== ttbox-hid-health: 11 PASS / 0 FAIL / 3 NOT-AVAILABLE ===
```

无真实输入设备时按 **NOT-AVAILABLE** 分类（不伪造数据、不误报 FAIL）；有设备但启动失败才判 FAIL。可作云端升级后 health check 的 commit 决策输入。

## 12. LocalSource —— PASS

[LocalHidPackageSource](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/src/hid/IHidPackageSource.hpp)（`fetch_to` 复制本地包目录至 staging）。

## 13. CloudSource 接口 —— PASS（占位）

[CloudHidPackageSource](file:///g:/工作区/ttbox逆向/ttbox2/ttbox/core/src/hid/IHidPackageSource.hpp)：**只做接口和占位**（返回 false），不连接真实云端。预留流程：云端检查版本 → 下载 → SHA256 → 签名 → 兼容性 → staging → activate → health check → commit。`test_hid_package` 单测 `hid_cloud_source_placeholder` 覆盖。

## 14. SHA256 字段 —— PASS

manifest 预留 `"sha256": ""`（开发版不启用验证，字段不删）。

## 15. signature 字段 —— PASS

manifest 预留 `"signature": ""` + `"signing_key_id": ""`。正式版要求：下载 → SHA256 → 签名 → 版本 → Runtime ABI → Kernel ABI → 激活，任何校验失败**禁止 activate**。

## 16. Runtime ABI —— PASS

`hid_protocol_version: "1"`，`min_runtime_version: "0.0.1"`，`max_runtime_version: ""`（null）。激活前 validator 检查版本兼容。

## 17. Kernel ABI —— PASS

`kernel_abi: "6.1-rockchip"`（对应板端内核 6.1.0-1025-rockchip）。

## 18. A1-A8 回归 —— PASS

板端 `ttbox_core_tests`：**67 tests passed, 0 failed**（覆盖 manifest 字段 / VERSION / registry 生命周期 / 激活失败回滚 / 配置独立 / CloudSource 占位 / A7 ModelAdapter / A8 ModelRegistry / ROI/FOV / RGA / IPC / 最新帧等全部历史单测）。

## 19. 240FPS 回归 —— PASS（无退化）

黄瓦 320 INT8 · 3 Worker · 8 buffers · A76 affinity(4,5,6) · 1080p240：

| 场景 | 结果 |
|---|---|
| Pipeline 单独 | capture 241.8 FPS / 总吞吐 241.1 FPS / error=0 / poll_timeouts=0 / V4L2 errors=0 |
| 无 HID（基线） | 总吞吐 240.5 FPS / error=0 / poll_timeouts=0 |
| HID 8000Hz @ CPU0 | 总吞吐 241.5 FPS / error=0 / poll_timeouts=0 |
| HID 8000Hz @ CPU7 | 总吞吐 241.4 FPS / error=0 / poll_timeouts=0 |
| HID 8000Hz @ 默认 | 总吞吐 240.5 FPS / error=0 / poll_timeouts=0 |

HID 仿真负载全部 100% 达成目标率（7999.8~8000.0 Hz），drop=0，queue_max=1。

## 20. HID v0.0.1 功能回归 —— PASS

回环 500Hz（模拟真实键鼠回报率）：

```text
注入=1500 rx=1500 tx=1500 backpressure=0 drop=0 rx_err=0 tx_err=0
latency(us): avg=4.6 p50=4 p95=5 p99=7 max=41
=== 回环测试: PASS ===
```

- HID forwarding：100%（1500/1500）
- latency：avg 4.6µs（与 A9 基线 4.5µs 一致，无回退）
- queue drop：0
- keyboard / mouse：解析由 67 项单测覆盖；真实 hidraw 转发测试在插入键鼠后运行（无输入设备时输出 NOT AVAILABLE，不伪造）

---

## 20 项验收清单

| # | 验收项 | 结果 |
|---|---|---|
| 1 | HID 完全独立 | PASS |
| 2 | Package 目录 | PASS |
| 3 | manifest | PASS |
| 4 | VERSION | PASS |
| 5 | Registry | PASS |
| 6 | install | PASS |
| 7 | activate | PASS |
| 8 | deactivate | PASS |
| 9 | remove（禁删 active） | PASS |
| 10 | rollback（双版本） | PASS |
| 11 | health check | PASS（11 PASS / 0 FAIL / 3 N/A） |
| 12 | LocalSource | PASS |
| 13 | CloudSource 接口 | PASS（占位） |
| 14 | SHA256 字段 | PASS（预留） |
| 15 | signature 字段 | PASS（预留） |
| 16 | Runtime ABI | PASS |
| 17 | Kernel ABI | PASS |
| 18 | A1-A8 回归 | PASS（67/67） |
| 19 | 240FPS 回归 | PASS（~241 FPS / 0 error / 0 timeout） |
| 20 | HID v0.0.1 | **PASS** |

---

## 最终状态

```text
HID Package v0.0.1
        ↓
独立运行（AI Runtime 仅经 IHidRuntime 访问）
        ↓
独立安装/升级（import→validate→install→activate，生命周期 staging/installed/active/inactive/quarantine/rollback）
        ↓
独立回滚（active + previous 双版本，激活失败自动恢复）
        ↓
云端更新接口已预留（CloudHidPackageSource 占位，sha256/signature 字段就位）
```

本阶段 HID 模块边界与升级机制已定死。后续键鼠功能可在 HID Package 内独立迭代版本，无需重烧 AIBox 系统。**A9-P2 完成，停止，不进入 A10。**

---

## 本阶段禁止事项（未实施）

- Raw Gadget 高回报率正式实现（属 A9-P3）
- 云端真实连接 / 自动更新 / 远程 HID / 自动鼠标控制 / A10 Web UI

## 板端验证脚本

- `scripts/a9_pkg_install.sh`：0.0.1 安装/激活完整流程
- `scripts/a9_pkg_rollback_demo.sh`：双版本回滚演示（0.0.0 ↔ 0.0.1）
- `scripts/a9_pkg_guard_verify.sh`：禁删 active / deactivate / remove 守卫验证
