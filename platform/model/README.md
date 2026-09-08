# Model Manager

实现与 TTBOX `/usr/lib/ttbox/model` + 白狼模型安装能力等价的最小控制面：`upload → staging → validate → install → activate → rollback`。当前验证只确认文件存在且非空，不解析 RKNN tensor；真实 RKNN 校验留给 RK3588 adapter。