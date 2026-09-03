# 2026-09-02 YU 兼容测试记录

## 执行环境

- 本地源码：`C:/Users/Administrator/Desktop/TTbox0831`
- 真机：`192.168.0.53`
- TTBOX Core：active
- TTBOX Web：active
- YU 服务：本轮未启动、不接入 TTBOX

## 已执行验证

| 项目 | 方法 | 结果 | 状态 |
|---|---|---|---|
| TTBOX Web Python 语法 | `python -m py_compile scripts/ttbox_web.py` | 返回码 0 | PASS |
| TTBOX Web JavaScript 语法 | `node --check web/static/app.js` | 返回码 0 | PASS |
| 差异格式检查 | `git diff --check` | 返回码 0 | PASS |
| Core 真机运行 | `systemctl is-active ttbox-core` | active | PASS |
| Web 真机运行 | `systemctl is-active ttbox-web` | active | PASS |
| `/api/state` 真实状态 | 真机 HTTP | running=true、model_loaded=true、capture≈142.58 FPS、inference≈106.79 FPS | PASS |
| 当前检测 | 真机 `/api/state` | detections=1、tracks=1、class=3、框约65×250 | PASS（观测） |
| 远程模型连接 | 真机 POST `/api/remote/connect` | HTTP 501，status=planned，无 mock session | PASS（诚实失败） |
| Core 本地构建 | Windows `core/build` | 目录不存在，且本机无 g++ | BLOCKED |
| 全量 pytest | Windows 根目录 | `platform` 目录遮蔽标准库；另有 Unix IPC 测试误跑 | ENVIRONMENT BLOCKED |

## 本轮修复

原状态：

```text
POST /api/remote/connect → {ok:true, session_id:"mock-remote"}
GET  /api/remote/models  → {ok:true, models:[]}
POST /api/remote/import   → {ok:true, 已导入}
POST /api/remote/delete   → {ok:true, 已删除}
```

现状态：

```text
HTTP 501
ok=false
status=planned
明确提示远程模型协议未接入
```

已同步到真机并重启 `ttbox-web.service`，服务回读 active。

## 尚未完成的自动化测试

以下能力需要独立测试场景或专门设备，不能用配置回读代替：

- Hotkey 真实按键 → injection_allowed → HID 消费者 → 实际位移
- TargetSelector 双目标/交叉/遮挡/短暂消失行为
- FOV 改变后的检测/选择结果变化
- PID 参数改变后的真实输出变化
- 拉枪曲线/持续提前量/个人曲线训练的真实 HID 效果
- 模型切换后 Core 实际加载新模型
- Wi-Fi、主机名、端口、局域网屏蔽（本夜不执行网络修改）
- OTA 安装、取消、回滚（本夜不执行危险副作用）
- 重启/关机（本夜不执行）

## 测试判定规则

- API 返回成功只证明 API 成功，不证明设备行为。
- `systemd active` 只证明服务进程运行，不证明目标选择或 HID 闭环。
- 当前 `class_id=3` 仍是 TTBOX 模型自己的 `enemy_2`，不能映射成 YU 类别。
- 当前只有一个局部 DetectionBox，不能伪造完整人体框。
