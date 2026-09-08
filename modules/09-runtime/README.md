# 模块 09：运行时层（Runtime / Application）

## 它是什么？

运行时层是"总指挥"：把所有模块组装起来，管理它们的生命周期。

## 它干什么？

1. **组装**：创建采集、Worker、瞄准线程、预览、IPC 等所有组件
2. **启动/停止**：统一管理所有组件的启动顺序和停止
3. **配置加载**：读取 config/default.json，把配置分发给各模块
4. **IPC 服务**：提供 /tmp/ttbox_core.sock 接口，Web 页面通过它控制盒子
5. **状态采集**：汇总所有模块指标（帧率、延迟、检测数），供 Web 显示
6. **授权校验**：检查许可证（License），未授权则不允许运行

## 输入是什么？

- 配置文件（config/default.json）
- 命令行参数（--config 路径）

## 输出是什么？

- 运行状态指标（GET_STATUS）
- IPC 服务（Web 控制入口）
- 整个系统的正常运行

## 谁调用它？

- `main.cpp`（程序入口）
- systemd 服务（`ttbox-core.service`，开机自启）

## 它不能干什么？

- 不能自己识别目标、不能自己移动鼠标（它只调度）
- 不能修改模型的检测逻辑

## 修改它会影响什么？

- 影响系统整体行为（启动流程、配置、指标）
- 改配置加载 = 影响所有模块的配置来源

## 关键文件

| 文件 | 作用 |
|---|---|
| `core/src/runtime/CoreRuntime.cpp/.hpp` | 核心运行时（组件组装/生命周期） |
| `core/src/runtime/HardwareRunner.cpp/.hpp` | 硬件测试运行器 |
| `core/src/app/Application.cpp/.hpp` | 应用主类（配置/IPC/授权/启动） |
| `core/src/main.cpp` | 程序入口 |
| `core/src/ipc/IpcServer.cpp/.hpp` | IPC 服务（Web 控制接口） |
| `core/src/config/ConfigManager.cpp/.hpp` | 配置管理 |
