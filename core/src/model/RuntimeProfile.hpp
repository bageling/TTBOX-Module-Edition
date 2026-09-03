// RuntimeProfile.hpp — 运行时模型配置（阶段 A-8）
//
// 目标：模型本身（ModelMetadata，A-7）与用户参数彻底分离。
//   - RuntimeProfile 只描述"用户/运行时想怎么跑"，绝不写入 RKNN 或 ModelMetadata。
//   - 所有字段都可被用户修改：ROI / FOV / confidence / iou / class_filter / max_detections。
//
// 配置优先级（不变）：
//   Model Default (ModelMetadata) < Runtime Default < User Config (RuntimeProfile)
//
// 热更新：RuntimeConfig 持有 shared_ptr<const RuntimeProfile>，更新 = 原子替换
// 快照（禁逐帧 JSON/IPC）。解码器/worker 每帧取只读快照。
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common/Json.hpp"
#include "mouse/MouseTypes.hpp"
#include "rknn/DetectionGeometryFilter.hpp"

namespace ttbox::core {

// ---------------------------------------------------------------------------
// Capture ROI：屏幕截取区域（像素）
//   width/height = 截取宽高（≠ 模型输入尺寸；0 = 默认全帧）
//   offset_x/offset_y = 相对屏幕中心的偏移（px，负值=左/上；配合 width/height 使用）
// ---------------------------------------------------------------------------
struct CaptureProfile {
    uint32_t width = 0;      // 0 = 默认（全帧）
    uint32_t height = 0;     // 0 = 默认（全帧）
    int32_t offset_x = 0;    // 相对屏幕中心的偏移（px）
    int32_t offset_y = 0;

    bool valid(uint32_t frame_w, uint32_t frame_h, std::string* error = nullptr) const;
};

// ---------------------------------------------------------------------------
// Inference：检测参数（用户可改）
// ---------------------------------------------------------------------------
struct InferenceProfile {
    float confidence = 0.0f;     // 0 = 用 ModelMetadata.default_conf
    float iou = 0.0f;            // 0 = 用 ModelMetadata.default_iou
    std::vector<int> class_filter;  // 空 = 全部保留
    int max_detections = 0;      // 0 = 不限
};

// FOV 形状
enum class FovShape : int { kCircle = 0, kRect = 1 };

// FOV：最终检测过滤（在 NMS 之后应用）
//   center_x/center_y：归一化（0~1，相对全帧）
//   radius：circle = 归一化半径；rect = 归一化半宽/半高
struct FovProfile {
    bool enabled = false;
    FovShape shape = FovShape::kCircle;
    float radius = 0.5f;    // circle: 归一化半径；rect: 归一化半宽半高
    float center_x = 0.5f;  // 0~1
    float center_y = 0.5f;  // 0~1
};

// ---------------------------------------------------------------------------
// Preview：Web 控制台实时画面（用户可选；与模型输入解耦）
//   显示屏幕正中心 roi_w×roi_h 方框 → 平铺到 width×height 预览
//   roi_w/h = 屏幕中心截取尺寸（默认 320x320，模型同款聚焦区域）
// ---------------------------------------------------------------------------
struct PreviewProfile {
    uint32_t width = 320;    // 预览输出宽
    uint32_t height = 320;
    uint32_t roi_w = 320;    // 屏幕中心截取宽
    uint32_t roi_h = 320;
    bool center_crop = true;
    uint32_t fps = 0;        // 预览帧率上限（0 = 用 Application 默认 preview_fps）
};

// ---------------------------------------------------------------------------
// RuntimeProfile：完整用户配置（模型无关）
// ---------------------------------------------------------------------------
struct RuntimeProfile {
    std::string model_id;       // 关联 installed 模型（空 = 未指定，用激活模型）
    CaptureProfile capture;
    InferenceProfile inference;
    FovProfile fov;
    PreviewProfile preview;     // Web 实时画面尺寸
    aim::MouseProfile mouse;    // A10：鼠标 AI 注入配置（与模型彻底分离）
    DetectionGeometryFilterConfig geometry_filter;

    // ---- JSON 序列化（仅配置管理/持久化使用；推理路径禁止逐帧解析）----
    JsonValue to_json() const;
    static RuntimeProfile from_json(const JsonValue& v);
    static RuntimeProfile from_json_file(const std::string& path, std::string* error = nullptr);
    bool save_to_json_file(const std::string& path, std::string* error = nullptr) const;

    // 简单校验：数值范围（错误返回 false + reason）
    bool validate(std::string* error = nullptr) const;
};

// ---------------------------------------------------------------------------
// RuntimeConfig：内存热更新配置（Lock-free 读：读快照 shared_ptr）
//   允许运行时修改：confidence / iou / class_filter / max_detections / FOV / ROI
//   禁止每帧 JSON/IPC；更新通过 update()（线程安全）。
// ---------------------------------------------------------------------------
class RuntimeConfig {
public:
    RuntimeConfig() = default;

    // 原子替换当前配置（线程安全）
    void update(std::shared_ptr<const RuntimeProfile> profile) {
        std::lock_guard<std::mutex> lk(mtx_);
        current_ = std::move(profile);
    }
    void update(const RuntimeProfile& profile) {
        update(std::make_shared<const RuntimeProfile>(profile));
    }

    // 只读快照（每帧调用安全；共享所有权，无拷贝竞争）
    std::shared_ptr<const RuntimeProfile> snapshot() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return current_;
    }

    bool empty() const { return snapshot() == nullptr; }

private:
    mutable std::mutex mtx_;
    std::shared_ptr<const RuntimeProfile> current_;
};

}  // namespace ttbox::core
