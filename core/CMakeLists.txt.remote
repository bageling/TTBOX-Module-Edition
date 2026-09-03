# ttbox/core — TTBox C++ Core 阶段 A-1 地基
#
# 要求：
#   - C++17
#   - ARM64 / RK3588 可编译（板端 g++ 12 / cmake 3.25 已验证）
#   - Debug / Release
#   - warnings（-Wall -Wextra -Wpedantic / MSVC /W4）
#   - 可测试（内置零依赖测试框架）
#   - 不依赖 Python

cmake_minimum_required(VERSION 3.16)
project(ttbox_core VERSION 0.1.0 LANGUAGES CXX)

option(TTBOX_CORE_BUILD_TESTS "构建 C++ Core 单元测试" ON)
option(TTBOX_CORE_BUILD_TOOLS "构建 IPC 命令行工具" ON)
# 硬件测试（需要 /dev/video0，仅板端启用；host 上默认关闭）
option(TTBOX_CORE_BUILD_HW_TESTS "构建 V4L2 硬件验收测试（RK3588）" OFF)
option(TTBOX_CORE_BUILD_AUTH "构建在线授权层（需要 OpenSSL 开发文件）" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# 项目根 (ttbox2/): core 位于 <root>/ttbox/core
get_filename_component(TTBOX_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# ---- 编译警告 ----
if(MSVC)
  # /utf-8：源码为 UTF-8（中文注释），GB 代码页下会被错读导致 #pragma once 失效
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- 核心库 ----
set(CORE_SOURCES
  src/app/Application.cpp
  src/aim/AimThread.cpp
  src/input/PhysicalMouseReader.cpp
  src/runtime/CoreRuntime.cpp
  src/runtime/HardwareRunner.cpp
  src/output/FifoHidOutput.cpp
  src/output/AiboxHidOutput.cpp
  src/output/OutputBackend.cpp
  src/output/LocalHidBackend.cpp
  src/output/MakcuMouseBackend.cpp
  src/output/MouseControlClient.cpp
  src/common/Logger.cpp
  src/common/CpuAffinity.cpp
  src/common/Json.cpp
  src/config/ConfigManager.cpp
  src/ipc/IpcServer.cpp
  src/model/ModelAdapter.cpp
  src/model/ModelRegistry.cpp
  src/model/ModelManagement.cpp
  src/model/RuntimeProfile.cpp
  src/model/IModelSource.cpp
  src/rknn/DecodeNMS.cpp
  src/rknn/DetectionGeometryFilter.cpp
  src/hid/HidForwarder.cpp
  src/hid/HidParser.cpp
  src/hid/HidPackageManifest.cpp
  src/hid/HidPackageConfig.cpp
  src/hid/HidPackageRegistry.cpp
  src/hid/HidRuntime.cpp
  src/hid/IHidPackageSource.cpp
  # A10：AI 鼠标注入（纯 C++ 算法模块，host 可测）
  src/mouse/TargetSelector.cpp
  src/mouse/AimPointProfile.cpp
  src/mouse/AimTracker.cpp
  src/mouse/CoordinateTransform.cpp
  src/mouse/MouseRouter.cpp
  src/mouse/AimStateMachine.cpp
  src/mouse/PersonalMotion.cpp
)
if(TTBOX_CORE_BUILD_AUTH)
  list(APPEND CORE_SOURCES
    src/auth/DeviceFingerprint.cpp
    src/auth/LicenseDaemon.cpp
    src/auth/HttpClient.cpp
    src/auth/AiboxLicenseClient.cpp
  )
else()
  list(APPEND CORE_SOURCES src/auth/DisabledAuth.cpp)
endif()
set(TTBOX_CORE_HAS_RGA FALSE)
set(TTBOX_CORE_HAS_RKNN FALSE)
if(UNIX AND NOT APPLE)
  list(APPEND CORE_SOURCES
    src/capture/DmaBuf.cpp
    src/capture/V4L2Capture.cpp
    src/preview/PreviewModule.cpp
  )
  # RGA（librga，阶段 A-3）：板端已装 /usr/local；缺失则不编译 RGA 源
  find_path(RGA_INCLUDE_DIR im2d.h PATHS /usr/local/include)
  find_library(RGA_LIBRARY rga PATHS /usr/local/lib)
  if(RGA_INCLUDE_DIR AND RGA_LIBRARY)
    list(APPEND CORE_SOURCES src/rga/RgaProcessor.cpp)
    set(TTBOX_CORE_HAS_RGA TRUE)
    message(STATUS "librga found: include=${RGA_INCLUDE_DIR} lib=${RGA_LIBRARY}")
  else()
    message(WARNING "librga 未找到（include=${RGA_INCLUDE_DIR} lib=${RGA_LIBRARY}），RgaProcessor 不编译")
  endif()

  # RKNN（librknnrt + rknn_api.h，阶段 A-4）
  find_library(RKNNRT_LIBRARY rknnrt PATHS /usr/lib /usr/local/lib)
  find_path(RKNN_INCLUDE_DIR rknn_api.h PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rknn)
  if(RKNNRT_LIBRARY AND RKNN_INCLUDE_DIR)
    list(APPEND CORE_SOURCES
      src/rknn/RKNNEngine.cpp
      src/rknn/NpuMonitor.cpp
      src/rknn/WorkerPool.cpp
        )
    set(TTBOX_CORE_HAS_RKNN TRUE)
    message(STATUS "librknnrt found: lib=${RKNNRT_LIBRARY} include=${RKNN_INCLUDE_DIR}")
  else()
    message(WARNING "librknnrt 未找到（lib=${RKNNRT_LIBRARY} include=${RKNN_INCLUDE_DIR}），RKNNEngine 不编译")
  endif()
endif()

add_library(ttbox_core STATIC ${CORE_SOURCES})
target_include_directories(ttbox_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(TTBOX_CORE_HAS_RGA)
  target_include_directories(ttbox_core PUBLIC ${RGA_INCLUDE_DIR})
  target_link_libraries(ttbox_core PUBLIC ${RGA_LIBRARY})
endif()
if(TTBOX_CORE_HAS_RKNN)
  target_include_directories(ttbox_core PUBLIC ${RKNN_INCLUDE_DIR})
  target_link_libraries(ttbox_core PUBLIC ${RKNNRT_LIBRARY})
endif()
# 注入项目根（默认配置文件路径 / 测试定位真实 default.json）
target_compile_definitions(ttbox_core PUBLIC TTBOX_PROJECT_ROOT="${TTBOX_PROJECT_ROOT}")
if(TTBOX_CORE_HAS_RKNN)
  target_compile_definitions(ttbox_core PUBLIC TTBOX_CORE_HAS_RKNN=1)
endif()
if(WIN32)
  target_link_libraries(ttbox_core PUBLIC ws2_32)
endif()
# Phase2：Preview（libjpeg，板端 UNIX）
if(UNIX AND NOT APPLE)
  find_package(JPEG QUIET)
  if(NOT JPEG_FOUND)
    find_library(JPEG_LIB jpeg PATHS /usr/lib /usr/local/lib)
    find_path(JPEG_INC jpeglib.h PATHS /usr/include)
    if(JPEG_LIB AND JPEG_INC)
      set(JPEG_LIBRARIES ${JPEG_LIB})
      set(JPEG_INCLUDE_DIRS ${JPEG_INC})
      set(JPEG_FOUND TRUE)
    endif()
  endif()
  if(JPEG_FOUND)
    target_link_libraries(ttbox_core PUBLIC ${JPEG_LIBRARIES})
    target_include_directories(ttbox_core PUBLIC ${JPEG_INCLUDE_DIRS})
    message(STATUS "libjpeg found: ${JPEG_LIBRARIES}")
  else()
    message(WARNING "libjpeg 未找到，PreviewModule 不编译")
    list(REMOVE_ITEM CORE_SOURCES src/preview/PreviewModule.cpp)
  endif()
endif()

# ---- 新架构硬件入口（默认 NullHidOutput，不接管真实鼠标）----
if(UNIX AND NOT APPLE AND TTBOX_CORE_HAS_RKNN)
  add_executable(hardware_runner_main tools/hardware_runner_main.cpp)
  target_link_libraries(hardware_runner_main PRIVATE ttbox_core)
endif()

# ---- 主程序 ----
# V4L2/RKNN 仅在板端编译；Windows 下 CoreRuntime 会引用未编译符号，
# 因此 ttbox_core_main 仅在 UNIX 且 RKNN 可用时构建。
if(UNIX AND NOT APPLE AND TTBOX_CORE_HAS_RKNN)
  add_executable(ttbox_core_main src/main.cpp)
  target_link_libraries(ttbox_core_main PRIVATE ttbox_core)
endif()

# ---- IPC 命令行工具 ----
if(TTBOX_CORE_BUILD_TOOLS)
  add_executable(ipc_ping tools/ipc_ping.cpp)
  target_link_libraries(ipc_ping PRIVATE ttbox_core)
endif()

# ---- 测试 ----
if(TTBOX_CORE_BUILD_TESTS)
  enable_testing()
  set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_logger.cpp
    tests/test_config.cpp
    tests/test_ipc.cpp
    tests/test_ipc_control.cpp  # Phase 1：SET_CONFIG / RUNTIME_CONTROL 验收
    tests/test_ipc_model.cpp    # v0.3：模型管理 IPC 验收
    # test_application 依赖 Application→CoreRuntime→V4L2/RKNN（仅板端可链接），
    # Windows 下排除，其余纯逻辑测试保留。
    $<$<NOT:$<PLATFORM_ID:Windows>>:tests/test_application.cpp>
    tests/test_decode.cpp
    tests/test_model_metadata.cpp
    tests/test_decoder_dispatch.cpp
    tests/test_runtime_profile.cpp
    tests/test_model_registry.cpp
    tests/test_fov_roi.cpp
    tests/test_hid.cpp
    tests/test_hid_package.cpp
    tests/test_mouse.cpp  # A10：AI 鼠标注入模块测试
  )
  if(UNIX AND NOT APPLE)
    list(APPEND TEST_SOURCES tests/test_latestframe.cpp)
  endif()
  if(TTBOX_CORE_HAS_RGA)
    list(APPEND TEST_SOURCES tests/test_rga.cpp)
  endif()
  add_executable(ttbox_core_tests ${TEST_SOURCES})
  target_include_directories(ttbox_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(ttbox_core_tests PRIVATE ttbox_core)
  add_test(NAME ttbox_core_tests COMMAND ttbox_core_tests)

  # A11：Worker -> AimThread 无锁目标邮箱测试（不需要硬件）
  add_executable(test_aim_target_mailbox tests/test_aim_target_mailbox.cpp)
  target_link_libraries(test_aim_target_mailbox PRIVATE ttbox_core)
  add_test(NAME test_aim_target_mailbox COMMAND test_aim_target_mailbox)

  # A11：AimThread 线程生命周期与邮箱消费测试
  add_executable(test_aim_thread tests/test_aim_thread.cpp)
  target_link_libraries(test_aim_thread PRIVATE ttbox_core)
  add_test(NAME test_aim_thread COMMAND test_aim_thread)

  # pid1.cpp P_PID 1:1 移植对照测试（与参考实现输出逐点一致）
  add_executable(test_pid1 tests/test_pid1.cpp)
  target_link_libraries(test_pid1 PRIVATE ttbox_core)
  add_test(NAME test_pid1 COMMAND test_pid1)

  # Hotkey Gate 最终输出安全边界测试（7 个验收场景）
  add_executable(test_hotkey_gate tests/test_hotkey_gate.cpp)
  target_link_libraries(test_hotkey_gate PRIVATE ttbox_core)
  add_test(NAME test_hotkey_gate COMMAND test_hotkey_gate)

  # 热键完全可配置化验收（默认/换键/侧键/双键/any-all/免重启生效/未按键零输出）
  add_executable(test_hotkey_config tests/test_hotkey_config.cpp)
  target_link_libraries(test_hotkey_config PRIVATE ttbox_core)
  add_test(NAME test_hotkey_config COMMAND test_hotkey_config)

  # C6：TargetSelector 防横跳压力测试（交叉/抖动/消失/排序翻转）
  add_executable(test_selector_stress tests/test_selector_stress.cpp)
  target_link_libraries(test_selector_stress PRIVATE ttbox_core)
  add_test(NAME test_selector_stress COMMAND test_selector_stress)

  # C10/C11/C14：生命周期压力 + RuntimeConfig 并发读写压测
  add_executable(test_lifecycle_stress tests/test_lifecycle_stress.cpp)
  target_link_libraries(test_lifecycle_stress PRIVATE ttbox_core)
  add_test(NAME test_lifecycle_stress COMMAND test_lifecycle_stress)

  # A11：FOV 分辨率与方向换算测试
  add_executable(test_fov_angle tests/test_fov_angle.cpp)
  target_link_libraries(test_fov_angle PRIVATE ttbox_core)
  add_test(NAME test_fov_angle COMMAND test_fov_angle)

  add_executable(test_detection_geometry_filter tests/test_detection_geometry_filter.cpp)
  target_link_libraries(test_detection_geometry_filter PRIVATE ttbox_core)
  add_test(NAME test_detection_geometry_filter COMMAND test_detection_geometry_filter)

  add_executable(test_coordinate_transform tests/test_coordinate_transform.cpp)
  target_link_libraries(test_coordinate_transform PRIVATE ttbox_core)
  add_test(NAME test_coordinate_transform COMMAND test_coordinate_transform)

  # OutputBackend 统一接口测试（纯 C++，host 可测；Gate/选择/生命周期/零移动）
  add_executable(test_output_backend tests/test_output_backend.cpp)
  target_link_libraries(test_output_backend PRIVATE ttbox_core)
  add_test(NAME test_output_backend COMMAND test_output_backend)

  add_executable(test_mouse_control_client tests/test_mouse_control_client.cpp)
  target_link_libraries(test_mouse_control_client PRIVATE ttbox_core)
  add_test(NAME test_mouse_control_client COMMAND test_mouse_control_client)

  add_executable(test_personal_motion tests/test_personal_motion.cpp)
  target_link_libraries(test_personal_motion PRIVATE ttbox_core)
  add_test(NAME test_personal_motion COMMAND test_personal_motion)

  # G1 metrics 测试：实例化 CoreRuntime（依赖 V4L2Capture/WorkerPool，仅板端编译）
  if(UNIX AND NOT APPLE)
    add_executable(test_metrics_g1 tests/test_metrics_g1.cpp)
    target_link_libraries(test_metrics_g1 PRIVATE ttbox_core)
    add_test(NAME test_metrics_g1 COMMAND test_metrics_g1)
  endif()

  # ---- 硬件验收测试（板端：-DTTBOX_CORE_BUILD_HW_TESTS=ON）----
  if(TTBOX_CORE_BUILD_HW_TESTS AND UNIX AND NOT APPLE)
    add_executable(test_capture_hw tests/test_capture_hw.cpp)
    target_include_directories(test_capture_hw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(test_capture_hw PRIVATE ttbox_core)
    add_test(NAME test_capture_hw COMMAND test_capture_hw)

    if(TTBOX_CORE_HAS_RGA)
      add_executable(test_rga_hw tests/test_rga_hw.cpp)
      target_include_directories(test_rga_hw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_rga_hw PRIVATE ttbox_core)
      add_test(NAME test_rga_hw COMMAND test_rga_hw)
    endif()

    if(TTBOX_CORE_HAS_RKNN)
      add_executable(test_rknn_hw tests/test_rknn_hw.cpp)
      target_include_directories(test_rknn_hw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_rknn_hw PRIVATE ttbox_core)
      # ctest 项使用短帧数快速回归；完整吞吐测试见 A-4 验收（--frames 300）
      add_test(NAME test_rknn_hw COMMAND test_rknn_hw --mode raw --model /opt/ttbox2/models/yolo261n-rk3588.rknn --frames 30)

      # A11：旧 test_worker_hw 已弃用。硬件链路由后续 HardwareRunner 独立入口承载。

      # A11：HardwareRunner 只构建验证生命周期，不自动访问设备。
      add_executable(test_hardware_runner tests/test_hardware_runner.cpp)
      target_include_directories(test_hardware_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_hardware_runner PRIVATE ttbox_core)

      add_executable(test_decode_align tests/test_decode_align.cpp)
      target_include_directories(test_decode_align PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_decode_align PRIVATE ttbox_core)

      # A-7 ModelAdapter / Decoder / Runtime 生命周期测试
      add_executable(test_model_adapter tests/test_model_adapter.cpp)
      target_include_directories(test_model_adapter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_model_adapter PRIVATE ttbox_core)
      add_test(NAME test_model_adapter COMMAND test_model_adapter
               --model /opt/ttbox2/models/yolo261n-rk3588.rknn --color bgr --expect single --classes 80)

      add_executable(test_model_runtime tests/test_model_runtime.cpp)
      target_include_directories(test_model_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_model_runtime PRIVATE ttbox_core)

      # A-8 模型仓库生命周期 + 切换 + 热更新
      add_executable(test_model_switch_hw tests/test_model_switch_hw.cpp)
      target_include_directories(test_model_switch_hw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_model_switch_hw PRIVATE ttbox_core)

      # A-8 RGA ROI 硬件裁剪
      add_executable(test_rga_roi_hw tests/test_rga_roi_hw.cpp)
      target_include_directories(test_rga_roi_hw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_rga_roi_hw PRIVATE ttbox_core)

      # A-9 HID 透传（hidraw → hidg）
      add_executable(test_hid_forward_hw tests/test_hid_forward_hw.cpp)
      target_include_directories(test_hid_forward_hw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_hid_forward_hw PRIVATE ttbox_core)

      # A-9 独立测试程序 ttbox-hid-test
      add_executable(ttbox-hid-test tests/ttbox_hid_test.cpp)
      target_include_directories(ttbox-hid-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(ttbox-hid-test PRIVATE ttbox_core)

      # A-9 合成 HID 高频负载模拟（CPU affinity / AI 隔离实验）
      add_executable(test_hid_load_sim tests/test_hid_load_sim.cpp)
      target_include_directories(test_hid_load_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_hid_load_sim PRIVATE ttbox_core)

      # A-9 转发器回环测试（fifo → hidg）
      add_executable(test_hid_loopback tests/test_hid_loopback.cpp)
      target_include_directories(test_hid_loopback PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
      target_link_libraries(test_hid_loopback PRIVATE ttbox_core)

      # A-9-P2 HID Package 健康检查工具
      add_executable(ttbox-hid-health tools/ttbox_hid_health.cpp)
      target_include_directories(ttbox-hid-health PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
      target_link_libraries(ttbox-hid-health PRIVATE ttbox_core)

      # A-9-P2 HID Package Registry CLI
      add_executable(ttbox-hid-pkg tools/ttbox_hid_pkg.cpp)
      target_include_directories(ttbox-hid-pkg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
      target_link_libraries(ttbox-hid-pkg PRIVATE ttbox_core)

      # A-9-P2 常驻 HID 透传 daemon（systemd: ttbox-hid-forward.service）
      add_executable(ttbox-hid-forward tools/ttbox_hid_forward.cpp)
      target_include_directories(ttbox-hid-forward PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
      target_link_libraries(ttbox-hid-forward PRIVATE ttbox_core)
    endif()
  endif()
endif()
