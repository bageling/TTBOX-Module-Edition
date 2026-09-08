# TTBOX 模型转换文档（.pt → ONNX → RKNN → manifest → validation → ModelRegistry）

> 版本：v1.0（设计稿，待用户验收后进入实现）
> 日期：2026-09-06
> 依据：仓库真实情况（无转换脚本、板端 librknnrt 1.5.2）+ rk3588-model-adaptation 技能实测配方（板端 Docker+qemu 转换 yolov8n 全流程已 PASS）+ TTBOX 隔离铁律

---

## 0. 最重要的一行

**转换工具版本必须与 TTBOX 板端运行库配对：`rknn-toolkit2 版本 == /opt/ttbox/lib/librknnrt.so 版本`。**

- TTBOX 板端实测：`librknnrt.so 1.5.2 (c6b7b351a@2023-08-23T15:28:22)`
- 因此转换必须用 **rknn-toolkit2 1.5.x**（推荐 1.5.2）
- ⚠️ **严禁使用 yu 系统的 rknn-toolkit2 2.3.2 转换结果直接喂 TTBOX**（隔离铁律 + 版本不匹配：runtime 1.5.2 加载高版本模型会 `Verify ModelBuffer failed`）
- 判断模型代差：`RKNN_LOG_LEVEL=4` 下 rknn_init 失败看具体原因；`invalid MAGIC`=格式坏，`Verify ModelBuffer failed`=版本不兼容

---

## 1. 全流程总览

```
.pt (Ultralytics YOLOv8/11/26)
  │① step_pt2onnx.py（CPU torch + ultralytics，opset=12，imgsz=模型目标尺寸）
  ▼
.onnx
  │  校验 A：head 无 LFS 指针；onnx.checker 通过；输出名与解码器预期一致
  │② step_onnx2rknn.py（rknn-toolkit2 1.5.x，target_platform=rk3588，INT8 需量化数据集）
  ▼
.rknn
  │  校验 B：magic 头 "RKNN"(52 4b 4e 4e)；转换端模拟推理（rknn.inference）
  │  校验 C：最小 C 探针 rknn_init rc=0（板端 NPU 真加载）
  │  校验 D：TTBOX ModelRegistry validator（结构分析 + 真实冒烟，目标阶段）
  ▼
manifest.json + model.rknn + labels.txt（校验记录落盘）
  ▼
Web 上传 → import → validate → install → select → （重启/热切换）→ 生效
```

---

## 2. 转换环境（板端 Docker + qemu，完全隔离于 TTBOX）

**前置事实**：板端（Orange Pi RK3588，Ubuntu 22.04）无 x86 主机可用；仓库内目前**没有任何转换脚本**（已排查：无 tools/onnx*、无 scripts/convert*、无 rknn-toolkit2 依赖声明）。推荐在板子上用 Docker+qemu 完成转换（技能 2026-08-30 实测全通过），转换产物才需要进入 TTBOX，转换工具本身 **不进入 /opt/ttbox**（隔离铁律：/opt/ttbox 只放运行时）。

### 2.1 开启 amd64 模拟

```bash
# docker 镜像源（失败先配）
cat > /etc/docker/daemon.json <<'EOF'
{"registry-mirrors": ["https://docker.m.daocloud.io", "https://dockerproxy.net", "https://docker.1panel.live"]}
EOF
systemctl restart docker

# 必须拉 arm64 变体 binfmt（默认 manifest 是 amd64 会自我矛盾）
docker pull --platform linux/arm64 tonistiigi/binfmt:latest
docker run --privileged --rm tonistiigi/binfmt:latest --install amd64
ls /proc/sys/fs/binfmt_misc/   # 应出现 qemu-x86_64
docker run --rm --platform linux/amd64 python:3.9-slim python -c "print(42)"  # 验证输出 42
```

### 2.2 下载 wheel（amx--no-deps，避免 CUDA 全家桶）

```bash
docker run --rm -v /opt/conv:/w python:3.9-slim bash -c "
  pip download rknn-toolkit2==1.5.2 --no-deps -d /w/rk
  pip download ultralytics -d /w/ult --no-deps
"
# 模型 .pt：从 ultralytics release 直链拉（注意：HuggingFace .onnx 常是 LFS 指针，别用）
```

依赖版本（rknn-toolkit2 1.5.x 配套，参考已实测 2.3.2 配方迁移）：
- numpy==1.26.4
- opencv-python-headless
- torch（`--index-url https://download.pytorch.org/whl/cpu`，CPU 版 ~190MB）
- typing-extensions==4.12.2（先于 torch 装）
- onnx==1.16.1（1.19+ 移除 onnx.mapping 会 AttributeError）
- onnxruntime、ml_dtypes、scipy、tqdm、requests、psutil、ruamel.yaml、pillow

### 2.3 长命容器装依赖 → commit 固化

```bash
docker run -d --name conv --platform linux/amd64 python:3.9-slim sleep 7200
# 全部 pip 放后台 + 轮询（qemu 下 pip 极慢，前台会超时）
docker exec conv pip install --no-deps /w/rk/rknn_toolkit2-1.5.2-*.whl
docker exec conv pip install --no-deps numpy==1.26.4 opencv-python-headless typing-extensions==4.12.2
docker exec conv pip install torch --index-url https://download.pytorch.org/whl/cpu
docker exec conv pip install --no-deps onnx==1.16.1 onnxruntime ml_dtypes scipy tqdm requests psutil ruamel.yaml pillow
docker exec conv python -c "from rknn.api import RKNN; import torch, onnx, cv2, numpy; print('OK')"  # 见一个补一个
docker commit conv ttbox-rknn-1-5-2:latest && docker rm -f conv
```

> ⚠️ 依赖版本执行时以 wheel 实际元数据为准，逐个 import 验证；1.5.2 与 2.3.2 依赖可能略有差异。

---

## 3. 转换脚本

### 3.1 step_pt2onnx.py

```python
# 输入：model.pt（YOLOv8/11/26 均可，ultralytics 加载）
# 输出：model.onnx（opset 12，CPU 导出）
from ultralytics import YOLO
import sys
src, dst_size = sys.argv[1], int(sys.argv[2])   # e.g. yolov11n.pt 320
m = YOLO(src)
m.export(format='onnx', imgsz=dst_size, opset=12)   # opset 12 实测可过
print('ONNX_OK')
```

**注意**：导出尺寸 = 最终模型输入尺寸（256/320/416/640 之一）。TTBOX 的 Preview 与该尺寸**完全无关**（Preview = capture 中心裁剪 640×640，已解耦验收）。

### 3.2 step_onnx2rknn.py

```python
# 输入：model.onnx
# 输出：model_rk3588.rknn（先 FP16 通过，再 INT8 量化）
from rknn.api import RKNN
import sys
rk = RKNN(verbose=False)
# 关键：mean/std 与训练预处理一致。0-255 直通时 mean=[[0,0,0]] std=[[255,255,255]]
rk.config(mean_values=[[0,0,0]], std_values=[[255,255,255]], target_platform='rk3588')
assert rk.load_onnx(model=sys.argv[1]) == 0, 'load_onnx'
assert rk.build(do_quantization=False) == 0, 'build'          # 先 FP16 验证链路
assert rk.export_rknn(sys.argv[2]) == 0, 'export_fp16'
# INT8 量化（生产推荐）：需要量化数据集（推荐 100-500 张代表性图，含检测目标）
# rk.config(mean_values=..., std_values=..., quantized_dtype='asymmetric_quantized-8', target_platform='rk3588')
# rk.build(do_quantization=True, dataset='/opt/conv/quant/dataset.txt')
print('RKNN_OK')
```

> ⚠️ 板端实测结论（RKNNEngine.cpp:357-365）：**INT8/FP16 模型统一喂 UINT8 原始像素 0-255，不做归一化**，量化由 runtime 内部完成。因此转换时 mean/std 必须配 `mean=0 std=255`（直通），否则板端推理结果错误。

### 3.3 INT8 量化（生产路径）

- 量化数据集：收集 100-500 张实际场景图（含目标、含背景变化），缩放到模型输入尺寸，写 `dataset.txt`（每行一个图片路径）
- 量化后必须做**精度对比**：同一张真图，FP16 版 vs INT8 版检测框对比，目标类别/位置/置信度应基本一致（框重叠 IoU>0.8、类别一致、conf 差 <0.15 可接受）
- 若无代表数据集：**宁可用 FP16，不用随便拍的图量化**（INT8 校准集差会毁掉小目标检测）

---

## 4. 产物校验（四道，逐级通过才允许上传）

### 4.1 校验 A：ONNX 头 + 结构

```bash
head -c 6 model.onnx | xxd    # 必须是 onnx 魔数或 proto 头；若内容是 "Reposi" = 下载到 LFS 指针，作废
python -c "import onnx; onnx.load('model.onnx'); print('checker_ok')"
```

### 4.2 校验 B：RKNN 魔数 + 转换端推理

```bash
head -c 4 model.rknn | xxd    # 必须是 52 4b 4e 4e = "RKNN"
# 转换端 rknn.inference 跑一张真图，确认输出形状与预期一致（单输出 [1,N,6] 或 (1,C,M) / 多输出成对）
```

### 4.3 校验 C：板端最小探针（NPU 真加载）

在板子（TTBOX 环境外 /opt/conv 或 /tmp 均可，编译产物不要进 /opt/ttbox 依赖树）：

```c
#include <stdio.h>
#include "rknn_api.h"
int main(int argc, char** argv) {
    rknn_context ctx = 0;
    int rc = rknn_init(&ctx, argv[1], 0, 0, NULL);   // 传路径时 size 必须为 0
    printf("rc=%d\n", rc);
    if (rc == 0) { rknn_sdk_version v; rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &v, sizeof(v));
                   printf("api=%s drv=%s\n", v.api_version, v.drv_version); }
    return 0;
}
```

```bash
# 链 TTBOX 自己的运行库（隔离：探针只为验证，不部署）
gcc /tmp/rt.c -o /tmp/rt -I/opt/ttbox/src/third_party/rknn -L/opt/ttbox/lib -lrknnrt -Wl,-rpath,/opt/ttbox/lib
/tmp/rt model.rknn     # rc=0 且 api 版本 = 1.5.2 = 与 TTBOX 运行库配对成功
# 失败时：RKNN_LOG_LEVEL=4 /tmp/rt model.rknn 看原因（MAGIC/代差）
```

### 4.4 校验 D：TTBOX validator（目标阶段增强）

- ModelRegistry validator：RKNNEngine.init（能加载 + 结构分析输入/输出/类型）
- 目标增强：一次真实推理冒烟（feed 真实帧 → 有检测输出、无报错）→ 写 `validation/smoke_result.json`
- 全部通过才进入 installed

---

## 5. manifest.json 生成

上传或转换后，按阶段生成：

**现状（v1 已有流程，仅 3 字段）**：metadata.json `{"input_width":256,"input_height":256,"output_count":6}`（validator 探测写回）。

**目标（完整 manifest）**：由校验 D 的完整结果生成：

```json
{
  "model_id": "jwdl_sjzv11",
  "label": "自定义显示名",
  "version": "1.0.0",
  "sha256": "1592a676…",                    ← import 时计算
  "origin": "local_upload",
  "converter": {"toolkit": "rknn-toolkit2", "version": "1.5.2", "target": "rk3588", "quant": "int8"},
  "runtime": {
    "input_width": 256, "input_height": 256, "color_order": "rgb",
    "decode_type": 3,                        ← ModelAdapter 推断结果持久化
    "class_count": 7,
    "class_names": ["类别0", "…"],
    "rknn_concurrency": 1
  },
  "status": "installed",
  "created_at": 1788613124
}
```

配套 `labels.txt`：每行一个类别名（与 manifest.class_names 一致），UI 可覆盖（现有 /api/models/class-names 写 manifest 的逻辑保留）。

---

## 6. 进入 ModelRegistry（上传 → 生效）

```
① 浏览器 模型库 → 上传 model.rknn（+ 可选 labels.txt）→ 落 _incoming/
② MODEL_IMPORT → staging/<model_id>/model.rknn（记录 sha256）
③ MODEL_VALIDATE → validator：RKNN 加载 + 结构分析（现状）/ + 冒烟（目标）→ validation/ok.json
④ MODEL_INSTALL → staging → installed/<model_id>/
⑤ /api/models/select → MODEL_ACTIVATE（再次 validator，失败保持旧激活）
⑥ 同步 config（model_label/model_path/尺寸）
⑦ 重启 ttbox-core（现状）→ WorkerPool 加载新模型
   或热切换（目标 v0.4）→ 不重启直接生效
⑧ 验证：/api/models 确认 decode_type/class_count/输入尺寸正确；真实画面出现检测框
```

---

## 7. 模型类型兼容矩阵（转换后自动适配）

| 模型 | 导出尺寸 | RKNN 输出结构 | TTBOX decode_type | 需改 C++？ |
|---|---|---|---|---|
| YOLOv8 黄瓦（已实测） | 320 | 2×2 成对输出 | kDfl | 否 |
| YOLOv11 yolo261n（已实测） | 640 | 单输出 (1,84,8400) | kSingle | 否 |
| YOLOv26 v26m（已实测） | 640 | 单输出 [1,300,6] | kE2e | 否 |
| 当前 256 模型（已运行） | 256 | 6 输出 reg/cls 对 | kDflPairDist | 否 |
| 未来新结构 | 任意 | 未知 | 未知 | 是（DecodeNMS 加分支） |

v8 → v11 → v26 转换链路完全相同（ultralytics 框架统一 .pt → onnx），TTBOX 侧零代码改动。

---

## 8. 验证与回归清单

| 步骤 | 命令/操作 | 预期 |
|---|---|---|
| 转换端 FP16 | step_onnx2rknn FP16 | RKNN_OK |
| magic 校验 | `head -c 4 x.rknn \| xxd` | 52 4b 4e 4e |
| 板端探针 | `/tmp/rt x.rknn` | rc=0, api=1.5.2 |
| 上传导入 | Web 模型库上传 | installed 出现 |
| 激活切换 | select → 重启 core | 模型信息日志 = 新模型 |
| 真实检测 | 看 preview 8001 | 目标出框 |
| 回归 | API 24/24、Overview 11/11、Hotkey 8/8、Movement 9/9、Preview 640/800 | 全 PASS |

---

## 9. 风险与对策

| 风险 | 对策 |
|---|---|
| toolkit 版本与 librknnrt 1.5.2 不匹配（代差） | 版本配对铁律（§0）；探针校验 C 强制验证 |
| INT8 量化掉精度 | 先 FP16 通链路，再量化；量化集 100-500 张；精度对比门槛 |
| HuggingFace .onnx = LFS 指针 | 拒绝下载 .onnx，一律 .pt 自转 |
| /tmp tmpfs 爆（~3.9G） | 大文件放 /opt/conv，转换完 docker prune |
| 板端 Docker 镜像源失败 | daemon.json 配国内镜像（§2.1） |
| 上传了错误代差/损坏模型 | validator 加载失败 → 隔离 quarantine（现状：留 staging 拒绝 install） |
| 激活失败 | active.json 不动 + config 不动 → 旧模型继续（已实现） |

---

## 10. 仓库内待补的实现件

1. `scripts/convert/step_pt2onnx.py` + `step_onnx2rknn.py`（从技能配方落库，默认 opset=12/1.5.2/直通量化）
2. `scripts/convert/verify_rknn.c`（最小探针，编译链指向 /opt/ttbox/lib）
3. `scripts/convert/make_manifest.py`（校验 D 输出 → manifest.json + labels.txt）
4. `docs/web/TTBOX_MODEL_CONVERSION.md`（本文档，已落库）
5. 实现阶段：validator 增强（冒烟）+ ModelRegistry import sha256 + decode_type 持久化

> 所有转换工具/脚本放在仓库 `scripts/convert/`，只部署到板子 `/opt/ttbox/tools/`（或独立目录），**绝不进 /opt/ttbox 运行时依赖树**（隔离铁律：/opt/ttbox 只放 core/plugins/config/models/lib）。

## 11. 模型包标准

模型转换工具的最终输出不是一个孤立的 `.rknn`，而是一个可验证模型包：

```text
<model_id>/
├── model.rknn
├── manifest.json
├── labels.txt                  # 可选，但存在时必须匹配 class_count
├── validation/                 # 板端生成，转换端不要伪造 PASS
├── source/                     # 可选：pt/onnx 和转换记录
├── calibration/                # 可选：INT8 校准集和 checksum
└── README.md                   # 可选：人工说明
```

转换端只能生成：`model.rknn`、manifest、source、calibration 和转换报告。`validation/report.json` 的板端字段必须由真实 RKNN Runtime 和真实推理写入，不能由 PC 转换脚本伪造。

## 12. manifest 入库前检查

转换工具生成 `status=staging` 的 manifest；Registry 入库前重新检查：

1. `model_id` 与目录名一致。
2. `model.rknn` 存在、可读、非空。
3. manifest JSON 合法，`schema_version` 在支持范围内。
4. 输入宽高、layout、dtype、quantization 合法。
5. `class_count > 0`，类别名数量为 0 或等于 class_count。
6. `output_count > 0`，`output_format` 属于已注册 Decoder。
7. checksum 使用 SHA256，且重新计算结果一致。
8. `hardware` 包含 `rk3588`。

任何一项失败都只进入 `INVALID` 或 `UNSUPPORTED`，不允许安装为 READY。

## 13. 转换工具输出目录

PC 转换工具建议使用临时工作目录，完成后只把完整包复制到 staging：

```text
work/<model_id>/
├── export/model.onnx
├── inspect/onnx-report.json
├── build/model.rknn
├── convert-report.json
└── package/<model_id>/
    ├── model.rknn
    ├── manifest.json
    ├── source/
    └── calibration/
```

工具执行失败时保留 `convert-report.json` 和错误日志，但不创建 READY 包。文件复制到板端后，必须再次计算 model.rknn checksum，不能信任 PC 端传输前的结果。

## 14. 板端入库命令契约

实现阶段提供一个只负责模型生命周期的板端入口，内部仍调用唯一 C++ ModelRegistry：

```bash
ttbox-model import --package /opt/ttbox/incoming/<model_id>
ttbox-model validate --model-id <model_id> --real-inference
ttbox-model install --model-id <model_id>
ttbox-model switch --model-id <model_id>
ttbox-model status --model-id <model_id>
```

这些命令不是第二套 ModelManager；它们只是 Registry/IPC 的操作入口。命令输出必须包含：`model_id`、阶段、状态、failure_code、failure_message 和 validation report 路径。

## 15. 真实验证报告

`validation/report.json` 至少记录：

```json
{
  "model_id": "MODEL_ID",
  "model_sha256": "SHA256",
  "device": "rk3588",
  "runtime_version": "1.5.2",
  "input": {"width": 256, "height": 256, "layout": "NHWC", "dtype": "INT8"},
  "outputs": [],
  "stages": {
    "file_read": "pass",
    "manifest": "pass",
    "checksum": "pass",
    "rknn_init": "pass",
    "rknn_query": "pass",
    "smoke_inference": "pass",
    "decode": "pass"
  },
  "first_inference_ms": 0,
  "detections": 0,
  "failure_code": "",
  "validated_at": "ISO-8601"
}
```

`detections=0` 本身不是失败；必须同时记录输入样本类型和 Decode 合法性。失败时 `failure_code` 必须非空，`stages` 保留失败阶段。

## 16. 当前 jwdl_sjzv11 迁移样例

当前板端真实目录只有：

```text
/opt/ttbox/models/installed/jwdl_sjzv11/model.rknn
/opt/ttbox/models/installed/jwdl_sjzv11/metadata.json
```

迁移到最终目录时不能直接移动后宣称完成，必须：

1. 创建 `/opt/ttbox/models/jwdl_sjzv11/`。
2. 复制 `model.rknn`，重新计算 SHA256。
3. 根据真实 RKNN query 生成 manifest v2。
4. 在 `192.168.0.53` 上完成 init、query、一次真实推理和 Decode 验证。
5. 写入 validation/report.json。
6. Registry install/scan 通过后，才允许把新目录标为 READY。
7. 切换成功且首帧完成后，再处理旧 `installed/` 目录。

当前 `/api/models` 已实测返回 `selected_model_id=jwdl_sjzv11`，但输入尺寸、输出数量、类别数量为 0；这一步必须在迁移验收中修正。