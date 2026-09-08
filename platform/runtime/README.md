# Runtime Lifecycle Service

`RuntimeController` 是 Platform 控制/编排层，复用现有 C++ Core 的进程入口，不实现 Capture、RGA、RKNN、WorkerPool、Pipeline、HID 或瞄准算法。

## 适配器

- `SubprocessProcessAdapter`：接收现有 Core 可执行文件或启动脚本命令，负责进程生命周期。
- `MockProcessAdapter`：仅用于自动测试，不能代表真实 Core/RK3588 集成。

## API

`start()`、`stop()`、`restart()`、`status()`、`health()`、`reload()`。

`status()` 返回 `state`、`pid`、`uptime`、`last_error`、`health`、`timestamp`。

真实入口接入方式：将仓库现有 C++ Core 可执行文件/部署脚本命令传给 `SubprocessProcessAdapter`；当前 Windows 没有 C++ 编译器，RK3588 真实进程集成尚未测试。

## 状态转换

允许：`STOPPED→STARTING→READY→RUNNING→STOPPING→STOPPED`；故障路径为 `STARTING/READY/RUNNING→FAILED→RECOVERING→STARTING→READY→RUNNING`，恢复失败可进入 `FAILED`。非法转换由 `transition()` 拒绝。
