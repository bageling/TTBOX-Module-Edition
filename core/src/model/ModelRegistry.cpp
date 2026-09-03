// ModelRegistry.cpp — 模型仓库实现
#include "model/ModelRegistry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace ttbox::core {

namespace {

std::string now_ms() {
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count());
}

bool read_file(const std::string& path, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out->assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return f.good();
}

}  // namespace

const char* model_status_name(ModelStatus s) {
    switch (s) {
        case ModelStatus::kStaging: return "staging";
        case ModelStatus::kInstalled: return "installed";
        case ModelStatus::kQuarantined: return "quarantined";
        default: return "unknown";
    }
}

// ---------------------------------------------------------------------------
// ModelManifest
// ---------------------------------------------------------------------------

JsonValue ModelManifest::to_json() const {
    JsonValue root = JsonValue::object();
    root.set("model_id", JsonValue::string(model_id));
    root.set("label", JsonValue::string(label));
    root.set("version", JsonValue::string(version));
    root.set("sha256", JsonValue::string(sha256));
    root.set("signature", JsonValue::string(signature));
    root.set("origin", JsonValue::string(origin));
    root.set("converter_version", JsonValue::string(converter_version));
    root.set("runtime_version", JsonValue::string(runtime_version));
    root.set("input_width", JsonValue::number(static_cast<double>(input_width)));
    root.set("input_height", JsonValue::number(static_cast<double>(input_height)));
    root.set("output_count", JsonValue::number(static_cast<double>(output_count)));
    root.set("class_count", JsonValue::number(static_cast<double>(class_count)));
    JsonValue jnames = JsonValue::array();
    for (const auto& n : class_names) jnames.push_back(JsonValue::string(n));
    root.set("class_names", std::move(jnames));
    root.set("rknn_concurrency", JsonValue::number(static_cast<double>(rknn_concurrency)));
    root.set("status", JsonValue::number(static_cast<double>(static_cast<int>(status))));
    root.set("status_name", JsonValue::string(model_status_name(status)));
    root.set("created_at", JsonValue::number(static_cast<double>(created_at)));
    return root;
}

ModelManifest ModelManifest::from_json(const JsonValue& v) {
    ModelManifest m;
    if (!v.is_object()) return m;
    auto get = [&v](const char* key, const char* def) -> std::string {
        const JsonValue* p = v.find(key);
        return (p && p->is_string()) ? p->as_string(def) : def;
    };
    m.model_id = get("model_id", "");
    auto get_int_fn = [&v](const char* key) -> uint32_t {
        const JsonValue* p = v.find(key);
        return (p && p->is_number()) ? static_cast<uint32_t>(p->as_int(0)) : 0u;
    };
    m.label = get("label", "");
    m.version = get("version", "1.0.0");
    m.sha256 = get("sha256", "");
    m.signature = get("signature", "");
    m.origin = get("origin", "local");
    m.converter_version = get("converter_version", "");
    m.runtime_version = get("runtime_version", "");
    if (const JsonValue* s = v.find("status"); s && s->is_number()) {
        m.status = static_cast<ModelStatus>(s->as_int(0));
    }
    if (const JsonValue* t = v.find("created_at"); t && t->is_number()) {
        m.created_at = t->as_int(0);
    m.input_width = get_int_fn("input_width");
    m.input_height = get_int_fn("input_height");
    m.output_count = get_int_fn("output_count");
    m.class_count = get_int_fn("class_count");
    m.rknn_concurrency = get_int_fn("rknn_concurrency");
    if (m.rknn_concurrency == 0) m.rknn_concurrency = 1;
    if (const JsonValue* jn = v.find("class_names"); jn && jn->is_array()) {
        for (const auto& e : jn->as_array()) {
            if (e.is_string()) {
                m.class_names.push_back(e.as_string());
            }
        }
    }
        }
    return m;
}

// ---------------------------------------------------------------------------
// ModelRegistry
// ---------------------------------------------------------------------------

ModelRegistry::ModelRegistry(ModelRegistryOptions opts) : opts_(std::move(opts)) {
    if (!opts_.root.empty()) {
        root_ = opts_.root;
    } else {
        root_ = std::string(TTBOX_PROJECT_ROOT) + "/models";
    }
}

bool ModelRegistry::exists(const std::string& path) const {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool ModelRegistry::ensure_dirs(std::string* error) {
    for (const char* sub : {"registry", "installed", "staging", "cache", "quarantine"}) {
        std::error_code ec;
        const std::string p = root_ + "/" + sub;
        if (!fs::create_directories(p, ec) && ec) {
            if (error) *error = "创建目录失败: " + p + " (" + ec.message() + ")";
            return false;
        }
    }
    return true;
}

bool ModelRegistry::init(std::string* error) {
    if (root_.empty()) {
        if (error) *error = "模型仓库根目录为空";
        return false;
    }
    std::error_code ec;
    if (!fs::create_directories(root_, ec) && ec) {
        if (error) *error = "创建模型仓库失败: " + root_ + " (" + ec.message() + ")";
        return false;
    }
    return ensure_dirs(error);
}

bool ModelRegistry::copy_file(const std::string& src, const std::string& dst,
                              std::string* error) const {
    std::error_code ec;
    if (!fs::exists(src, ec)) {
        if (error) *error = "源文件不存在: " + src;
        return false;
    }
    fs::create_directories(fs::path(dst).parent_path(), ec);
    if (ec) {
        if (error) *error = "创建目标目录失败: " + ec.message();
        return false;
    }
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "复制失败 " + src + " -> " + dst + ": " + ec.message();
        return false;
    }
    return true;
}

std::string ModelRegistry::staging_dir(const std::string& model_id) const {
    return root_ + "/staging/" + model_id;
}
std::string ModelRegistry::installed_dir(const std::string& model_id) const {
    return root_ + "/installed/" + model_id;
}
std::string ModelRegistry::rknn_path(const std::string& model_id) const {
    return installed_dir(model_id) + "/model.rknn";
}
std::string ModelRegistry::metadata_path(const std::string& model_id) const {
    return installed_dir(model_id) + "/metadata.json";
}
std::string ModelRegistry::manifest_path(const std::string& model_id) const {
    return installed_dir(model_id) + "/manifest.json";
}

bool ModelRegistry::import(const std::string& src_rknn, const std::string& model_id,
                           const ModelManifest& manifest, std::string* error) {
    if (model_id.empty()) {
        if (error) *error = "model_id 不能为空";
        return false;
    }
    // 禁止与已安装模型冲突
    if (exists(installed_dir(model_id))) {
        if (error) *error = "模型已安装: " + model_id;
        return false;
    }
    // 覆盖旧 staging
    std::error_code ec;
    fs::remove_all(staging_dir(model_id), ec);
    if (!copy_file(src_rknn, staging_dir(model_id) + "/model.rknn", error)) {
        return false;
    }
    ModelManifest m = manifest;
    m.model_id = model_id;
    m.status = ModelStatus::kStaging;
    if (m.created_at == 0) m.created_at = std::stoll(now_ms());
    if (!write_file(staging_dir(model_id) + "/manifest.json", m.to_json().dump())) {
        if (error) *error = "写 staging manifest 失败";
        return false;
    }
    return true;
}

bool ModelRegistry::validate(const std::string& model_id, std::string* error) {
    if (!validator_) {
        if (error) *error = "未设置 validator（生产环境应注入 RKNN+Adapter 校验）";
        return false;
    }
    const std::string rknn = staging_dir(model_id) + "/model.rknn";
    if (!exists(rknn)) {
        if (error) *error = "staging 模型不存在: " + model_id;
        return false;
    }
    JsonValue metadata;
    std::string verr;
    if (!validator_(rknn, &metadata, &verr)) {
        if (error) *error = "模型验证失败(" + model_id + "): " + verr;
        return false;
    }
    // 写 validation 结果（确保 validation/ 目录存在）
    JsonValue vroot = JsonValue::object();
    vroot.set("ok", JsonValue::boolean(true));
    vroot.set("validated_at", JsonValue::number(static_cast<double>(std::stoll(now_ms()))));
    vroot.set("metadata", metadata);
    std::error_code vec;
    fs::create_directories(staging_dir(model_id) + "/validation", vec);
    if (!write_file(staging_dir(model_id) + "/validation/ok.json", vroot.dump())) {
        if (error) *error = "写 validation 结果失败";
        return false;
    }
    // 同步 metadata 快照（供 install 直接搬入）
    if (!metadata.is_null()) {
        write_file(staging_dir(model_id) + "/validation/metadata.json", metadata.dump());
        // 探测结果合并进 manifest（input/output 尺寸随 MODEL_LIST 下发）
        const std::string mf_path = staging_dir(model_id) + "/manifest.json";
        std::string mf_text;
        if (read_file(mf_path, &mf_text)) {
            auto mf_res = json_parse(mf_text);
            if (mf_res.ok) {
                ModelManifest mf = ModelManifest::from_json(mf_res.value);
                if (const JsonValue* iw = metadata.find("input_width"))
                    mf.input_width = static_cast<uint32_t>(std::max<int64_t>(iw->as_int(0), 0));
                if (const JsonValue* ih = metadata.find("input_height"))
                    mf.input_height = static_cast<uint32_t>(std::max<int64_t>(ih->as_int(0), 0));
                if (const JsonValue* oc = metadata.find("output_count"))
                    mf.output_count = static_cast<uint32_t>(std::max<int64_t>(oc->as_int(0), 0));
                if (const JsonValue* cc = metadata.find("class_count"))
                    mf.class_count = static_cast<uint32_t>(std::max<int64_t>(cc->as_int(0), 0));
                write_file(mf_path, mf.to_json().dump());
            }
        }
    }
    return true;
}

bool ModelRegistry::install(const std::string& model_id, std::string* error) {
    const std::string sd = staging_dir(model_id);
    const std::string id = installed_dir(model_id);
    if (!exists(sd + "/model.rknn")) {
        if (error) *error = "staging 模型不存在（先 import+validate）: " + model_id;
        return false;
    }
    if (!exists(sd + "/validation/ok.json")) {
        if (error) *error = "模型未验证（先 validate）: " + model_id;
        return false;
    }
    if (exists(id)) {
        if (error) *error = "目标已存在（需先 remove）: " + model_id;
        return false;
    }
    // 读 staging manifest → 状态改为 installed
    std::string mtext;
    if (!read_file(sd + "/manifest.json", &mtext)) {
        if (error) *error = "读 staging manifest 失败";
        return false;
    }
    auto res = json_parse(mtext);
    if (!res.ok) {
        if (error) *error = "staging manifest 解析失败: " + res.error;
        return false;
    }
    ModelManifest m = ModelManifest::from_json(res.value);
    m.status = ModelStatus::kInstalled;

    std::error_code ec;
    fs::create_directories(id, ec);
    if (ec) {
        if (error) *error = "创建 installed 目录失败: " + ec.message();
        return false;
    }
    // 复制（staging 保留；remove 时清理）
    if (!copy_file(sd + "/model.rknn", id + "/model.rknn", error)) return false;
    if (!copy_file(sd + "/validation/ok.json", id + "/validation/ok.json", error)) return false;
    if (!write_file(id + "/manifest.json", m.to_json().dump())) {
        if (error) *error = "写 installed manifest 失败";
        return false;
    }
    if (exists(sd + "/validation/metadata.json")) {
        copy_file(sd + "/validation/metadata.json", id + "/metadata.json", error);
    }
    return true;
}

bool ModelRegistry::quarantine(const std::string& model_id, const std::string& reason,
                               std::string* error) {
    const std::string sd = staging_dir(model_id);
    const std::string qd = root_ + "/quarantine/" + model_id;
    if (!exists(sd + "/model.rknn")) {
        if (error) *error = "staging 模型不存在: " + model_id;
        return false;
    }
    std::error_code ec;
    fs::remove_all(qd, ec);
    fs::rename(sd, qd, ec);
    if (ec) {
        fs::copy(sd, qd, fs::copy_options::recursive, ec);
        fs::remove_all(sd, ec);
    }
    // 写失败原因
    JsonValue q = JsonValue::object();
    q.set("reason", JsonValue::string(reason));
    q.set("quarantined_at", JsonValue::number(static_cast<double>(std::stoll(now_ms()))));
    write_file(qd + "/reason.json", q.dump());
    return true;
}

bool ModelRegistry::write_active(const std::string& model_id, std::string* error) {
    JsonValue a = JsonValue::object();
    a.set("model_id", JsonValue::string(model_id));
    a.set("activated_at", JsonValue::number(static_cast<double>(std::stoll(now_ms()))));
    if (!write_file(root_ + "/registry/active.json", a.dump())) {
        if (error) *error = "写 active.json 失败";
        return false;
    }
    return true;
}

std::string ModelRegistry::read_active() const {
    std::string text;
    if (!read_file(root_ + "/registry/active.json", &text)) return "";
    auto res = json_parse(text);
    if (!res.ok || !res.value.is_object()) return "";
    const JsonValue* v = res.value.find("model_id");
    if (!v || !v->is_string()) return "";
    return v->as_string();
}

std::string ModelRegistry::active_model() const {
    return read_active();
}

bool ModelRegistry::activate(const std::string& model_id, std::string* error) {
    if (!exists(installed_dir(model_id))) {
        if (error) *error = "模型未安装，无法激活: " + model_id;
        return false;
    }
    const std::string old = read_active();
    // 激活前校验"可用"：validator 加载 installed 模型
    if (validator_) {
        JsonValue metadata;
        std::string verr;
        if (!validator_(rknn_path(model_id), &metadata, &verr)) {
            if (error) *error = "激活校验失败，保持原激活(" + old + "): " + verr;
            return false;  // 未修改 active —— 自动恢复旧模型
        }
        // 同步 metadata 到 installed（激活时刷新）
        if (!metadata.is_null()) {
            write_file(metadata_path(model_id), metadata.dump());
        }
    }
    if (!write_active(model_id, error)) {
        // 写失败：恢复旧值
        if (!old.empty()) write_active(old, nullptr);
        return false;
    }
    return true;
}

bool ModelRegistry::deactivate(std::string* error) {
    if (!write_active("", error)) return false;
    return true;
}

bool ModelRegistry::remove(const std::string& model_id, std::string* error) {
    // 禁止删除正在使用（active）的模型
    const std::string act = read_active();
    if (!act.empty() && act == model_id) {
        if (error) *error = "模型正在使用（active），禁止删除: " + model_id;
        return false;
    }
    const std::string id = installed_dir(model_id);
    if (!exists(id)) {
        if (error) *error = "模型未安装: " + model_id;
        return false;
    }
    std::error_code ec;
    fs::remove_all(id, ec);
    if (ec) {
        if (error) *error = "删除失败: " + ec.message();
        return false;
    }
    return true;
}

std::vector<ModelManifest> ModelRegistry::list() const {
    std::vector<ModelManifest> out;
    const std::string dir = root_ + "/installed";
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_directory()) continue;
        const std::string mid = entry.path().filename().string();
        const std::string mp = entry.path().string() + "/manifest.json";
        std::string text;
        if (!read_file(mp, &text)) continue;
        auto res = json_parse(text);
        if (!res.ok) continue;
        ModelManifest m = ModelManifest::from_json(res.value);
        if (m.model_id.empty()) m.model_id = mid;
        out.push_back(std::move(m));
    }
    std::sort(out.begin(), out.end(),
              [](const ModelManifest& a, const ModelManifest& b) { return a.model_id < b.model_id; });
    return out;
}

}  // namespace ttbox::core
