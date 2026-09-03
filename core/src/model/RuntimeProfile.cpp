// RuntimeProfile.cpp — RuntimeProfile JSON 序列化/校验
/*
 * TTBOX 文件说明
 *
 * 文件：RuntimeProfile.cpp
 *
 * 作用：
 *   运行时配置文件的定义和翻译。
 *   定义所有可调参数，并在 YU 格式和 TTBOX 内部格式之间转换。
 *
 * 小白理解：
 *   你在 Web 页面上看到的每个参数（置信度、截取尺寸、PID 参数等），
 *   都在这里定义。它还负责把 YU 格式的参数翻译成 TTBOX 内部格式。
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "model/RuntimeProfile.hpp"

namespace ttbox::core {

// ---------------------------------------------------------------------------
// CaptureProfile
// ---------------------------------------------------------------------------

bool CaptureProfile::valid(uint32_t frame_w, uint32_t frame_h,
                           std::string* error) const {
    // 0 表示"用全帧"，等价于合法
    const uint32_t w = (width == 0) ? frame_w : width;
    const uint32_t h = (height == 0) ? frame_h : height;
    if (w == 0 || h == 0) {
        if (error) *error = "ROI 尺寸不能为 0";
        return false;
    }
    if (w > frame_w || h > frame_h) {
        if (error) *error = "ROI 尺寸超全帧: " + std::to_string(w) + "x" +
                            std::to_string(h) + " > " + std::to_string(frame_w) +
                            "x" + std::to_string(frame_h);
        return false;
    }
    // offset 为相对屏幕中心的偏移；计算左上角起点并 clamp 后必然界内
    const int32_t cx = static_cast<int32_t>(frame_w / 2) + offset_x;
    const int32_t cy = static_cast<int32_t>(frame_h / 2) + offset_y;
    const int32_t rx = std::max<int32_t>(0, std::min<int32_t>(
        cx - static_cast<int32_t>(w / 2), static_cast<int32_t>(frame_w - w)));
    const int32_t ry = std::max<int32_t>(0, std::min<int32_t>(
        cy - static_cast<int32_t>(h / 2), static_cast<int32_t>(frame_h - h)));
    (void)rx; (void)ry;  // 计算即校验（clamp 后必界内）；实际起点由 apply_runtime_profile 计算
    return true;
}

// ---------------------------------------------------------------------------
// 工具：读 object 成员（key 不存在返回默认值）
// ---------------------------------------------------------------------------
namespace {

int64_t obj_int(const JsonValue& o, const char* key, int64_t def) {
    const JsonValue* v = o.find(key);
    return (v && v->is_number()) ? v->as_int(def) : def;
}
double obj_num(const JsonValue& o, const char* key, double def) {
    const JsonValue* v = o.find(key);
    return (v && v->is_number()) ? v->as_number(def) : def;
}
bool obj_bool(const JsonValue& o, const char* key, bool def) {
    const JsonValue* v = o.find(key);
    return (v && v->is_bool()) ? v->as_bool(def) : def;
}
std::string obj_str(const JsonValue& o, const char* key, const std::string& def) {
    const JsonValue* v = o.find(key);
    return (v && v->is_string()) ? v->as_string(def) : def;
}

}  // namespace

// ---------------------------------------------------------------------------
// RuntimeProfile
// ---------------------------------------------------------------------------

bool RuntimeProfile::validate(std::string* error) const {
    // 非有限值总闸：JSON 1e999 等可产生 inf，NaN/inf 进入 PID 会输出乱飞（fail-closed 防线）。
    const float mouse_nums[] = {
        mouse.kp_x, mouse.kp_y, mouse.ki_x, mouse.ki_y, mouse.kd_x, mouse.kd_y,
        mouse.predict_x, mouse.predict_y, mouse.rate_x, mouse.rate_y,
        mouse.smooth_x, mouse.smooth_y, mouse.smooth,
        mouse.fov_range, mouse.confidence, mouse.sensitivity, mouse.output_scale,
        mouse.deadzone_x, mouse.deadzone_y, mouse.output_deadzone,
        mouse.selector_search_radius, mouse.lost_grace_ms,
        mouse.hfov, mouse.vfov, mouse.move_speed_x, mouse.move_speed_y,
        mouse.aim_point.aim_offset_x, mouse.aim_point.aim_offset_y,
        mouse.aim_point.offset_x, mouse.aim_point.offset_y,
        mouse.personal_motion.curve_blend, mouse.personal_motion.speed_blend,
        mouse.personal_motion.reaction_blend, mouse.personal_motion.max_reaction_delay_ms,
        inference.confidence, inference.iou,
        fov.center_x, fov.center_y, fov.radius,
    };
    for (const float v : mouse_nums) {
        if (!std::isfinite(v)) {
            if (error) *error = "配置含非有限数值（NaN/Infinity），已拒绝";
            return false;
        }
    }
    if (inference.confidence < 0.0f || inference.confidence > 1.0f) {
        if (error) *error = "confidence 必须在 [0,1]";
        return false;
    }
    if (inference.iou < 0.0f || inference.iou > 1.0f) {
        if (error) *error = "iou 必须在 [0,1]";
        return false;
    }
    for (const int c : inference.class_filter) {
        if (c < 0) {
            if (error) *error = "class_filter 含负类别";
            return false;
        }
    }
    if (inference.max_detections < 0) {
        if (error) *error = "max_detections 不能为负";
        return false;
    }
    if (fov.enabled) {
        if (fov.center_x < 0.0f || fov.center_x > 1.0f ||
            fov.center_y < 0.0f || fov.center_y > 1.0f) {
            if (error) *error = "FOV 中心必须在 [0,1]";
            return false;
        }
        if (fov.radius <= 0.0f || fov.radius > 1.0f) {
            if (error) *error = "FOV 半径必须在 (0,1]";
            return false;
        }
    }
    if (mouse.fov_range < 0.0f || mouse.fov_range > 1.0f) {
        if (error) *error = "mouse.fov_range 必须在 [0,1]";
        return false;
    }
    if (mouse.confidence < 0.0f || mouse.confidence > 1.0f) {
        if (error) *error = "mouse.confidence 必须在 [0,1]";
        return false;
    }
    if (mouse.kp_x < 0.0f || mouse.kp_y < 0.0f) {
        if (error) *error = "mouse.kp 不能为负";
        return false;
    }
    if (mouse.hfov <= 0.0f || mouse.hfov >= 180.0f ||
        mouse.vfov <= 0.0f || mouse.vfov >= 180.0f) {
        if (error) *error = "mouse.hfov/vfov 必须在 (0,180)";
        return false;
    }
    if (mouse.move_speed_x < 0.0f || mouse.move_speed_y < 0.0f) {
        if (error) *error = "mouse.move_speed 不能为负";
        return false;
    }
    if (mouse.aim_part < 0 || mouse.aim_part > 10) {
        if (error) *error = "mouse.aim_part 必须在 [0,10]";
        return false;
    }
    if (mouse.rate_x < 0.0f || mouse.rate_y < 0.0f ||
        mouse.sensitivity < 0.0f || mouse.output_scale < 0.0f) {
        if (error) *error = "mouse 输出系数不能为负";
        return false;
    }
    if (mouse.smooth < 0.0f || mouse.smooth > 1.0f) {
        if (error) *error = "mouse.smooth 必须在 [0,1]";
        return false;
    }
    if (mouse.lost_grace_ms < 0.0f) {
        if (error) *error = "mouse.lost_grace_ms 不能为负";
        return false;
    }
    if (mouse.personal_motion.curve_blend < 0.0f || mouse.personal_motion.curve_blend > 1.0f ||
        mouse.personal_motion.speed_blend < 0.0f || mouse.personal_motion.speed_blend > 1.0f ||
        mouse.personal_motion.reaction_blend < 0.0f || mouse.personal_motion.reaction_blend > 1.0f ||
        mouse.personal_motion.max_reaction_delay_ms < 0.0f ||
        mouse.personal_motion.max_reaction_delay_ms > 1000.0f) {
        if (error) *error = "personal_motion 混合参数超出范围";
        return false;
    }
    for (const float knot : mouse.personal_motion.knots) {
        if (!std::isfinite(knot) || knot < 0.0f || knot > 1.0f) {
            if (error) *error = "personal_motion knots 必须在 [0,1]";
            return false;
        }
    }
    if (mouse.personal_motion.knots.size() > 32) {
        if (error) *error = "personal_motion knots 最多 32 个";
        return false;
    }
    if (preview.width == 0 || preview.height == 0 ||
        preview.width > 3840 || preview.height > 2160 ||
        preview.roi_w == 0 || preview.roi_h == 0) {
        if (error) *error = "preview 尺寸/ROI 必须为正";
        return false;
    }
    return true;
}

JsonValue RuntimeProfile::to_json() const {
    JsonValue root = JsonValue::object();
    root.set("model_id", JsonValue::string(model_id));

    JsonValue cap = JsonValue::object();
    cap.set("width", JsonValue::number(static_cast<double>(capture.width)));
    cap.set("height", JsonValue::number(static_cast<double>(capture.height)));
    cap.set("offset_x", JsonValue::number(static_cast<double>(capture.offset_x)));
    cap.set("offset_y", JsonValue::number(static_cast<double>(capture.offset_y)));
    root.set("capture", std::move(cap));

    JsonValue inf = JsonValue::object();
    inf.set("confidence", JsonValue::number(static_cast<double>(inference.confidence)));
    inf.set("iou", JsonValue::number(static_cast<double>(inference.iou)));
    JsonValue cf = JsonValue::array();
    for (const int c : inference.class_filter) cf.push_back(JsonValue::number(static_cast<double>(c)));
    inf.set("class_filter", std::move(cf));
    inf.set("max_detections", JsonValue::number(static_cast<double>(inference.max_detections)));
    root.set("inference", std::move(inf));

    JsonValue gf = JsonValue::object();
    gf.set("enabled", JsonValue::boolean(geometry_filter.enabled));
    gf.set("min_head_conf", JsonValue::number(geometry_filter.min_head_conf));
    gf.set("min_body_conf", JsonValue::number(geometry_filter.min_body_conf));
    gf.set("paired_head_min_conf", JsonValue::number(geometry_filter.paired_head_min_conf));
    gf.set("head_only_min_conf", JsonValue::number(geometry_filter.head_only_min_conf));
    gf.set("head_only_center_max_px", JsonValue::number(geometry_filter.head_only_center_max_px));
    gf.set("min_body_width_px", JsonValue::number(geometry_filter.min_body_width_px));
    gf.set("min_body_height_px", JsonValue::number(geometry_filter.min_body_height_px));
    gf.set("border_reject_enabled", JsonValue::boolean(geometry_filter.reject_border));
    root.set("geometry_filter", std::move(gf));

    JsonValue fobj = JsonValue::object();
    fobj.set("enabled", JsonValue::boolean(fov.enabled));
    fobj.set("shape", JsonValue::number(static_cast<double>(fov.shape == FovShape::kRect ? 1 : 0)));
    fobj.set("radius", JsonValue::number(static_cast<double>(fov.radius)));
    fobj.set("center_x", JsonValue::number(static_cast<double>(fov.center_x)));
    fobj.set("center_y", JsonValue::number(static_cast<double>(fov.center_y)));
    root.set("fov", std::move(fobj));

    // A10：鼠标 AI 注入配置
    JsonValue m = JsonValue::object();
    m.set("enabled", JsonValue::boolean(mouse.enabled));
    m.set("proxy_mode", JsonValue::string(aim::mouse_proxy_mode_name(mouse.proxy_mode)));
    m.set("aim_hotkey", JsonValue::number(static_cast<double>(mouse.aim_hotkey)));
    m.set("aim_hotkey2", JsonValue::number(static_cast<double>(mouse.aim_hotkey2)));
    m.set("aim_hotkey_mode", JsonValue::string(aim::mouse_hotkey_mode_name(mouse.aim_hotkey_mode)));
    m.set("fov_range", JsonValue::number(static_cast<double>(mouse.fov_range)));
    m.set("confidence", JsonValue::number(static_cast<double>(mouse.confidence)));
    m.set("prediction_s", JsonValue::number(static_cast<double>(mouse.prediction_s)));
    m.set("kp_x", JsonValue::number(static_cast<double>(mouse.kp_x)));
    m.set("kp_y", JsonValue::number(static_cast<double>(mouse.kp_y)));
    m.set("ki_x", JsonValue::number(static_cast<double>(mouse.ki_x)));
    m.set("ki_y", JsonValue::number(static_cast<double>(mouse.ki_y)));
    m.set("kd_x", JsonValue::number(static_cast<double>(mouse.kd_x)));
    m.set("kd_y", JsonValue::number(static_cast<double>(mouse.kd_y)));
    m.set("fov_mode", JsonValue::boolean(mouse.fov_mode));
    m.set("hfov", JsonValue::number(static_cast<double>(mouse.hfov)));
    m.set("vfov", JsonValue::number(static_cast<double>(mouse.vfov)));
    m.set("move_speed_x", JsonValue::number(static_cast<double>(mouse.move_speed_x)));
    m.set("move_speed_y", JsonValue::number(static_cast<double>(mouse.move_speed_y)));
    m.set("aim_part", JsonValue::number(static_cast<double>(mouse.aim_part)));
    m.set("rate_x", JsonValue::number(static_cast<double>(mouse.rate_x)));
    m.set("rate_y", JsonValue::number(static_cast<double>(mouse.rate_y)));
    m.set("sensitivity", JsonValue::number(static_cast<double>(mouse.sensitivity)));
    m.set("output_scale", JsonValue::number(static_cast<double>(mouse.output_scale)));
    m.set("deadzone_x", JsonValue::number(static_cast<double>(mouse.deadzone_x)));
    m.set("deadzone_y", JsonValue::number(static_cast<double>(mouse.deadzone_y)));
    m.set("smooth", JsonValue::number(static_cast<double>(mouse.smooth)));
    // YU 对齐参数
    m.set("predict_x", JsonValue::number(static_cast<double>(mouse.predict_x)));
    m.set("predict_y", JsonValue::number(static_cast<double>(mouse.predict_y)));
    m.set("smooth_x", JsonValue::number(static_cast<double>(mouse.smooth_x)));
    m.set("smooth_y", JsonValue::number(static_cast<double>(mouse.smooth_y)));
    m.set("output_deadzone", JsonValue::number(static_cast<double>(mouse.output_deadzone)));
    m.set("selector_search_radius", JsonValue::number(static_cast<double>(mouse.selector_search_radius)));
    m.set("aim_fire_lock_y", JsonValue::boolean(mouse.aim_fire_lock_y));
    m.set("y_axis_fire_hotkey", JsonValue::number(static_cast<double>(mouse.y_axis_fire_hotkey)));
    m.set("y_axis_fire_release_delay_sec", JsonValue::number(static_cast<double>(mouse.y_axis_fire_release_delay_sec)));
    // 插件配置（pull_curve / continuous_lead / humanize）
    JsonValue pc = JsonValue::object();
    pc.set("enabled", JsonValue::boolean(mouse.pull_curve.enabled));
    pc.set("strength", JsonValue::number(static_cast<double>(mouse.pull_curve.strength)));
    pc.set("jitter_px", JsonValue::number(static_cast<double>(mouse.pull_curve.jitter_px)));
    pc.set("min_distance", JsonValue::number(static_cast<double>(mouse.pull_curve.min_distance)));
    m.set("pull_curve", std::move(pc));
    JsonValue cl = JsonValue::object();
    cl.set("enabled", JsonValue::boolean(mouse.continuous_lead.enabled));
    cl.set("enter_distance", JsonValue::number(static_cast<double>(mouse.continuous_lead.enter_distance)));
    cl.set("scale", JsonValue::number(static_cast<double>(mouse.continuous_lead.scale)));
    cl.set("fade_in_ms", JsonValue::number(static_cast<double>(mouse.continuous_lead.fade_in_ms)));
    cl.set("fade_out_ms", JsonValue::number(static_cast<double>(mouse.continuous_lead.fade_out_ms)));
    cl.set("near_disable_ratio", JsonValue::number(static_cast<double>(mouse.continuous_lead.near_disable_ratio)));
    m.set("continuous_lead", std::move(cl));
    JsonValue hz = JsonValue::object();
    hz.set("enabled", JsonValue::boolean(mouse.humanize.enabled));
    hz.set("curve_strength", JsonValue::number(static_cast<double>(mouse.humanize.curve_strength)));
    hz.set("jitter_px", JsonValue::number(static_cast<double>(mouse.humanize.jitter_px)));
    hz.set("jitter_frequency", JsonValue::number(static_cast<double>(mouse.humanize.jitter_frequency)));
    m.set("humanize", std::move(hz));
    JsonValue pm = JsonValue::object();
    pm.set("enabled", JsonValue::boolean(mouse.personal_motion.enabled));
    pm.set("curve_blend", JsonValue::number(static_cast<double>(mouse.personal_motion.curve_blend)));
    pm.set("speed_blend", JsonValue::number(static_cast<double>(mouse.personal_motion.speed_blend)));
    pm.set("reaction_blend", JsonValue::number(static_cast<double>(mouse.personal_motion.reaction_blend)));
    pm.set("max_reaction_delay_ms", JsonValue::number(static_cast<double>(mouse.personal_motion.max_reaction_delay_ms)));
    JsonValue knots = JsonValue::array();
    for (const float knot : mouse.personal_motion.knots) {
        knots.push_back(JsonValue::number(static_cast<double>(knot)));
    }
    pm.set("knots", std::move(knots));
    m.set("personal_motion", std::move(pm));
    m.set("aim_offset_x", JsonValue::number(static_cast<double>(mouse.aim_point.aim_offset_x)));
    m.set("aim_offset_y", JsonValue::number(static_cast<double>(mouse.aim_point.aim_offset_y)));
    m.set("offset_x", JsonValue::number(static_cast<double>(mouse.aim_point.offset_x)));
    m.set("offset_y", JsonValue::number(static_cast<double>(mouse.aim_point.offset_y)));
    m.set("switch_delay_ms", JsonValue::number(static_cast<double>(mouse.aim_point.switch_delay_ms)));
    m.set("lost_grace_ms", JsonValue::number(static_cast<double>(mouse.lost_grace_ms)));
    m.set("calibrating", JsonValue::boolean(mouse.calibrating));
    m.set("calibration_bias_x", JsonValue::number(static_cast<double>(mouse.calibration_bias_x)));
    m.set("calibration_bias_y", JsonValue::number(static_cast<double>(mouse.calibration_bias_y)));
    m.set("block_physical_x", JsonValue::boolean(mouse.block_physical_x));
    m.set("block_physical_y", JsonValue::boolean(mouse.block_physical_y));
    JsonValue cos = JsonValue::array();
    for (const auto& c : mouse.aim_point.class_offsets) {
        JsonValue o = JsonValue::object();
        o.set("class_id", JsonValue::number(static_cast<double>(c.class_id)));
        o.set("offset_x", JsonValue::number(static_cast<double>(c.offset_x)));
        o.set("offset_y", JsonValue::number(static_cast<double>(c.offset_y)));
        o.set("priority", JsonValue::number(static_cast<double>(c.priority)));
        cos.push_back(std::move(o));
    }
    m.set("class_offsets", std::move(cos));
    root.set("mouse", std::move(m));

    JsonValue pv = JsonValue::object();
    pv.set("width", JsonValue::number(static_cast<double>(preview.width)));
    pv.set("height", JsonValue::number(static_cast<double>(preview.height)));
    pv.set("roi_w", JsonValue::number(static_cast<double>(preview.roi_w)));
    pv.set("roi_h", JsonValue::number(static_cast<double>(preview.roi_h)));
    pv.set("center_crop", JsonValue::boolean(preview.center_crop));
    pv.set("fps", JsonValue::number(static_cast<double>(preview.fps)));
    root.set("preview", std::move(pv));

    return root;
}

RuntimeProfile RuntimeProfile::from_json(const JsonValue& v) {
    RuntimeProfile p;
    if (!v.is_object()) return p;

    p.model_id = obj_str(v, "model_id", "");

    if (const JsonValue* c = v.find("capture"); c && c->is_object()) {
        p.capture.width = static_cast<uint32_t>(std::max<int64_t>(obj_int(*c, "width", 0), 0));
        p.capture.height = static_cast<uint32_t>(std::max<int64_t>(obj_int(*c, "height", 0), 0));
        // offset 相对屏幕中心，允许负值
        p.capture.offset_x = static_cast<int32_t>(std::max<int64_t>(-100000, std::min<int64_t>(obj_int(*c, "offset_x", 0), 100000)));
        p.capture.offset_y = static_cast<int32_t>(std::max<int64_t>(-100000, std::min<int64_t>(obj_int(*c, "offset_y", 0), 100000)));
    }
    if (const JsonValue* i = v.find("inference"); i && i->is_object()) {
        p.inference.confidence = static_cast<float>(obj_num(*i, "confidence", 0.0));
        p.inference.iou = static_cast<float>(obj_num(*i, "iou", 0.0));
        p.inference.max_detections = static_cast<int>(obj_int(*i, "max_detections", 0));
        if (const JsonValue* cf = i->find("class_filter"); cf && cf->is_array()) {
            for (const auto& e : cf->as_array()) {
                if (e.is_number()) p.inference.class_filter.push_back(static_cast<int>(e.as_int()));
            }
        }
    }
    if (const JsonValue* gf = v.find("geometry_filter"); gf && gf->is_object()) {
        p.geometry_filter.enabled = obj_bool(*gf, "enabled", false);
        p.geometry_filter.min_head_conf = static_cast<float>(obj_num(*gf, "min_head_conf", 0.18));
        p.geometry_filter.min_body_conf = static_cast<float>(obj_num(*gf, "min_body_conf", 0.26));
        p.geometry_filter.paired_head_min_conf = static_cast<float>(obj_num(*gf, "paired_head_min_conf", 0.20));
        p.geometry_filter.head_only_min_conf = static_cast<float>(obj_num(*gf, "head_only_min_conf", 0.75));
        p.geometry_filter.head_only_center_max_px = static_cast<float>(obj_num(*gf, "head_only_center_max_px", 175.0));
        p.geometry_filter.min_body_width_px = static_cast<float>(obj_num(*gf, "min_body_width_px", 8.0));
        p.geometry_filter.min_body_height_px = static_cast<float>(obj_num(*gf, "min_body_height_px", 26.0));
        p.geometry_filter.reject_border = obj_bool(*gf, "border_reject_enabled", true);
    }
    if (const JsonValue* f = v.find("fov"); f && f->is_object()) {
        p.fov.enabled = obj_bool(*f, "enabled", false);
        p.fov.shape = (obj_int(*f, "shape", 0) == 1) ? FovShape::kRect : FovShape::kCircle;
        p.fov.radius = static_cast<float>(obj_num(*f, "radius", 0.5));
        p.fov.center_x = static_cast<float>(obj_num(*f, "center_x", 0.5));
        p.fov.center_y = static_cast<float>(obj_num(*f, "center_y", 0.5));
    }
    // A10：鼠标 AI 注入配置
    if (const JsonValue* m = v.find("mouse"); m && m->is_object()) {
        p.mouse.enabled = obj_bool(*m, "enabled", false);
        p.mouse.proxy_mode = aim::mouse_proxy_mode_from_string(obj_str(*m, "proxy_mode", "full_passthrough"));
        p.mouse.aim_hotkey = static_cast<uint8_t>(obj_int(*m, "aim_hotkey", 2));
        p.mouse.aim_hotkey2 = static_cast<uint8_t>(obj_int(*m, "aim_hotkey2", 0));
        p.mouse.aim_hotkey_mode = aim::mouse_hotkey_mode_from_string(obj_str(*m, "aim_hotkey_mode", "any").c_str());
        p.mouse.fov_range = static_cast<float>(obj_num(*m, "fov_range", 1.0));
        p.mouse.confidence = static_cast<float>(obj_num(*m, "confidence", 0.25));
        p.mouse.prediction_s = static_cast<float>(obj_num(*m, "prediction_s", 0.0));
        p.mouse.kp_x = static_cast<float>(obj_num(*m, "kp_x", 17.0));
        p.mouse.kp_y = static_cast<float>(obj_num(*m, "kp_y", 10.0));
        p.mouse.ki_x = static_cast<float>(obj_num(*m, "ki_x", 0.0));
        p.mouse.ki_y = static_cast<float>(obj_num(*m, "ki_y", 0.0));
        p.mouse.kd_x = static_cast<float>(obj_num(*m, "kd_x", 0.0));
        p.mouse.kd_y = static_cast<float>(obj_num(*m, "kd_y", 0.0));
        p.mouse.fov_mode = obj_bool(*m, "fov_mode", false);
        p.mouse.hfov = static_cast<float>(obj_num(*m, "hfov", 83.105));
        p.mouse.vfov = static_cast<float>(obj_num(*m, "vfov", 53.0));
        p.mouse.move_speed_x = static_cast<float>(obj_num(*m, "move_speed_x", 500.0));
        p.mouse.move_speed_y = static_cast<float>(obj_num(*m, "move_speed_y", 500.0));
        p.mouse.aim_part = static_cast<int>(obj_int(*m, "aim_part", 0));
        p.mouse.rate_x = static_cast<float>(obj_num(*m, "rate_x", 1.0));
        p.mouse.rate_y = static_cast<float>(obj_num(*m, "rate_y", 1.0));
        p.mouse.sensitivity = static_cast<float>(obj_num(*m, "sensitivity", 1.0));
        p.mouse.output_scale = static_cast<float>(obj_num(*m, "output_scale", 1.0));
        p.mouse.deadzone_x = static_cast<float>(obj_num(*m, "deadzone_x", 1.0));
        p.mouse.deadzone_y = static_cast<float>(obj_num(*m, "deadzone_y", 1.0));
        p.mouse.smooth = static_cast<float>(obj_num(*m, "smooth", 0.0));
        // YU 对齐参数
        p.mouse.predict_x = static_cast<float>(obj_num(*m, "predict_x", 0.008));
        p.mouse.predict_y = static_cast<float>(obj_num(*m, "predict_y", 0.008));
        p.mouse.smooth_x = static_cast<float>(obj_num(*m, "smooth_x", 9900.0));
        p.mouse.smooth_y = static_cast<float>(obj_num(*m, "smooth_y", 9900.0));
        p.mouse.output_deadzone = static_cast<float>(obj_num(*m, "output_deadzone", 1.0));
        p.mouse.selector_search_radius = static_cast<float>(obj_num(*m, "selector_search_radius", 170.0));
        p.mouse.aim_fire_lock_y = obj_bool(*m, "aim_fire_lock_y", false);
        p.mouse.y_axis_fire_hotkey = static_cast<int>(obj_int(*m, "y_axis_fire_hotkey", 1));
        p.mouse.y_axis_fire_release_delay_sec = static_cast<float>(obj_num(*m, "y_axis_fire_release_delay_sec", 0.3));
        // 插件配置（pull_curve / continuous_lead / humanize）
        if (const JsonValue* pc = m->find("pull_curve"); pc && pc->is_object()) {
            p.mouse.pull_curve.enabled = obj_bool(*pc, "enabled", true);
            p.mouse.pull_curve.strength = static_cast<float>(obj_num(*pc, "strength", 0.8));
            p.mouse.pull_curve.jitter_px = static_cast<float>(obj_num(*pc, "jitter_px", 3.0));
            p.mouse.pull_curve.min_distance = static_cast<float>(obj_num(*pc, "min_distance", 80.0));
        }
        if (const JsonValue* cl = m->find("continuous_lead"); cl && cl->is_object()) {
            p.mouse.continuous_lead.enabled = obj_bool(*cl, "enabled", false);
            p.mouse.continuous_lead.enter_distance = static_cast<float>(obj_num(*cl, "enter_distance", 150.0));
            p.mouse.continuous_lead.scale = static_cast<float>(obj_num(*cl, "scale", 0.5));
            p.mouse.continuous_lead.fade_in_ms = static_cast<float>(obj_num(*cl, "fade_in_ms", 300.0));
            p.mouse.continuous_lead.fade_out_ms = static_cast<float>(obj_num(*cl, "fade_out_ms", 300.0));
            p.mouse.continuous_lead.near_disable_ratio = static_cast<float>(obj_num(*cl, "near_disable_ratio", 0.66));
        }
        if (const JsonValue* hz = m->find("humanize"); hz && hz->is_object()) {
            p.mouse.humanize.enabled = obj_bool(*hz, "enabled", true);
            p.mouse.humanize.curve_strength = static_cast<float>(obj_num(*hz, "curve_strength", 0.45));
            p.mouse.humanize.jitter_px = static_cast<float>(obj_num(*hz, "jitter_px", 0.25));
            p.mouse.humanize.jitter_frequency = static_cast<float>(obj_num(*hz, "jitter_frequency", 8.0));
        }
        if (const JsonValue* pm = m->find("personal_motion"); pm && pm->is_object()) {
            p.mouse.personal_motion.enabled = obj_bool(*pm, "enabled", false);
            p.mouse.personal_motion.curve_blend = static_cast<float>(obj_num(*pm, "curve_blend", 1.0));
            p.mouse.personal_motion.speed_blend = static_cast<float>(obj_num(*pm, "speed_blend", 1.0));
            p.mouse.personal_motion.reaction_blend = static_cast<float>(obj_num(*pm, "reaction_blend", 0.7));
            p.mouse.personal_motion.max_reaction_delay_ms = static_cast<float>(obj_num(*pm, "max_reaction_delay_ms", 250.0));
            if (const JsonValue* knots = pm->find("knots"); knots && knots->is_array()) {
                for (const auto& item : knots->as_array()) {
                    if (item.is_number() && p.mouse.personal_motion.knots.size() < 32) {
                        p.mouse.personal_motion.knots.push_back(static_cast<float>(item.as_number()));
                    }
                }
            }
        }
        p.mouse.aim_point.aim_offset_x = static_cast<float>(obj_num(*m, "aim_offset_x", 0.0));
        p.mouse.aim_point.aim_offset_y = static_cast<float>(obj_num(*m, "aim_offset_y", 0.0));
        p.mouse.aim_point.offset_x = static_cast<float>(obj_num(*m, "offset_x", 0.5));
        p.mouse.aim_point.offset_y = static_cast<float>(obj_num(*m, "offset_y", 0.5));
        p.mouse.aim_point.switch_delay_ms = static_cast<int>(obj_int(*m, "switch_delay_ms", 30));
        p.mouse.lost_grace_ms = static_cast<float>(obj_num(*m, "lost_grace_ms", 78.0));
        p.mouse.calibrating = obj_bool(*m, "calibrating", false);
        p.mouse.calibration_bias_x = static_cast<float>(obj_num(*m, "calibration_bias_x", 0.0));
        p.mouse.calibration_bias_y = static_cast<float>(obj_num(*m, "calibration_bias_y", 0.0));
        p.mouse.block_physical_x = obj_bool(*m, "block_physical_x", false);
        p.mouse.block_physical_y = obj_bool(*m, "block_physical_y", false);
        if (const JsonValue* co = m->find("class_offsets"); co && co->is_array()) {
            for (const auto& e : co->as_array()) {
                if (!e.is_object()) continue;
                aim::ClassOffset c;
                c.class_id = static_cast<int>(obj_int(e, "class_id", 0));
                c.offset_x = static_cast<float>(obj_num(e, "offset_x", 0.5));
                c.offset_y = static_cast<float>(obj_num(e, "offset_y", 0.5));
                c.priority = static_cast<int>(obj_int(e, "priority", 0));
                p.mouse.aim_point.class_offsets.push_back(c);
            }
        }
    }
    if (const JsonValue* pv = v.find("preview"); pv && pv->is_object()) {
        p.preview.width = static_cast<uint32_t>(std::max<int64_t>(obj_int(*pv, "width", 320), 1));
        p.preview.height = static_cast<uint32_t>(std::max<int64_t>(obj_int(*pv, "height", 320), 1));
        p.preview.roi_w = static_cast<uint32_t>(std::max<int64_t>(obj_int(*pv, "roi_w", 320), 1));
        p.preview.roi_h = static_cast<uint32_t>(std::max<int64_t>(obj_int(*pv, "roi_h", 320), 1));
        p.preview.center_crop = obj_bool(*pv, "center_crop", true);
        p.preview.fps = static_cast<uint32_t>(std::max<int64_t>(obj_int(*pv, "fps", 0), 0));
    }
    return p;
}

RuntimeProfile RuntimeProfile::from_json_file(const std::string& path,
                                              std::string* error) {
    auto res = json_parse_file(path);
    if (!res.ok) {
        if (error) *error = "解析失败(" + path + "): " + res.error;
        return {};
    }
    return from_json(res.value);
}

bool RuntimeProfile::save_to_json_file(const std::string& path,
                                       std::string* error) const {
    std::string text = to_json().dump();
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        if (error) *error = "无法写入: " + path;
        return false;
    }
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 && ok) {
        if (error) *error = "写文件失败: " + path;
        return false;
    }
    return ok;
}

}  // namespace ttbox::core
