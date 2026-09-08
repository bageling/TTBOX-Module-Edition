# yu-baseline/ — YU 真实抓包原文归档

> 这是从 YU 真实运行实例（192.168.0.53:8080）抓到的**原始数据**，不是分析报告。
> 分析报告放在 `../yu-compatibility/`。

## 目录说明

| 子目录 | 内容 |
|---|---|
| [api/](api/) | YU HTTP API 返回原文（17 个 JSON 文件，含 models/license/state 等） |
| [behavior/](behavior/) | YU 行为观察记录（按钮响应、状态切换等） |
| [config/](config/) | YU 配置文件原文（device.json / preset.json 等） |
| [errors/](errors/) | YU 错误响应原文与触发条件 |
| [pages/](pages/) | YU Web 页面抓包（HTML/资源/接口调用顺序） |
| [restart/](restart/) | YU 重启/恢复行为观察 |
| [runtime/](runtime/) | YU 进程/线程/资源运行时抓包 |

## 何时更新

- 抓新 YU 行为 → 加文件，**不要改旧文件**
- 旧抓包过期 → 移到 `archived/<date>/` 而不是覆盖

## 维护纪律

- 这些是**证据**，不是文档。每次引用都要带文件名+日期。
- 不允许人为改写抓包内容。如果某条抓包发现是错的抓包，加注释说明，不改原文件。
