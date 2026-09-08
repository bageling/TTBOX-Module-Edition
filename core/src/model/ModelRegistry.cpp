// ModelRegistry.cpp — 模型仓库实现
#include "model/ModelRegistry.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

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

bool write_file_atomic(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        f.flush();
        if (!f.good()) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tmp, path, ec);
    }
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

bool valid_model_id(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    for (unsigned char c : id) {
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

bool is_hex_sha256(const std::string& value) {
    if (value.size() != 64) return false;
    for (unsigned char c : value) {
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

class Sha256 {
public:
    Sha256() : state_{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u} {}

    void update(const unsigned char* data, size_t length) {
        while (length > 0) {
            const size_t take = std::min(length, sizeof(buffer_) - buffer_size_);
            std::copy(data, data + take, buffer_ + buffer_size_);
            buffer_size_ += take;
            data += take;
            length -= take;
            bit_length_ += static_cast<uint64_t>(take) * 8u;
            if (buffer_size_ == sizeof(buffer_)) {
                transform(buffer_);
                buffer_size_ = 0;
            }
        }
    }

    std::string finish() {
        const uint64_t original_bits = bit_length_;
        buffer_[buffer_size_++] = 0x80;
        if (buffer_size_ > 56) {
            while (buffer_size_ < 64) buffer_[buffer_size_++] = 0;
            transform(buffer_);
            buffer_size_ = 0;
        }
        while (buffer_size_ < 56) buffer_[buffer_size_++] = 0;
        for (int i = 7; i >= 0; --i) buffer_[buffer_size_++] = static_cast<unsigned char>(original_bits >> (i * 8));
        transform(buffer_);

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (uint32_t word : state_) out << std::setw(8) << word;
        return out.str();
    }

private:
    static uint32_t rotr(uint32_t value, unsigned count) {
        return (value >> count) | (value << (32u - count));
    }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t bsig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t bsig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t ssig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t ssig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform(const unsigned char* block) {
        static constexpr uint32_t k[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
        uint32_t w[64] = {};
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(block[i * 4 + 3]);
        }
        for (size_t i = 16; i < 64; ++i) w[i] = ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) + w[i - 16];
        uint32_t a=state_[0], b=state_[1], c=state_[2], d=state_[3], e=state_[4], f=state_[5], g=state_[6], h=state_[7];
        for (size_t i = 0; i < 64; ++i) {
            const uint32_t t1 = h + bsig1(e) + ch(e, f, g) + k[i] + w[i];
            const uint32_t t2 = bsig0(a) + maj(a, b, c);
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
        state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
    }

    uint32_t state_[8];
    unsigned char buffer_[64] = {};
    size_t buffer_size_ = 0;
    uint64_t bit_length_ = 0;
};

bool sha256_file(const std::string& path, std::string* digest, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "无法读取模型文件: " + path;
        return false;
    }
    Sha256 sha;
    std::array<unsigned char, 64 * 1024> buffer{};
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count > 0) sha.update(buffer.data(), static_cast<size_t>(count));
    }
    if (!file.eof()) {
        if (error) *error = "读取模型文件失败: " + path;
        return false;
    }
    if (digest) *digest = sha.finish();
    return true;
}

}  // namespace

const char* model_status_name(ModelStatus s) {
    switch (s) {
        case ModelStatus::kDiscovered: return "discovered";
        case ModelStatus::kValidating: return "validating";
        case ModelStatus::kReady: return "ready";
        case ModelStatus::kSelected: return "selected";
        case ModelStatus::kLoading: return "loading";
        case ModelStatus::kRunning: return "running";
        case ModelStatus::kFailed: return "failed";
        case ModelStatus::kInvalid: return "invalid";
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
    root.set("name", JsonValue::string(label));
    root.set("version", JsonValue::string(version));
    root.set("format", JsonValue::string(format));
    root.set("source_format", JsonValue::string(source_format));
    root.set("task", JsonValue::string(task));
    root.set("framework", JsonValue::string(framework));
    root.set("input_layout", JsonValue::string(input_layout));
    root.set("input_dtype", JsonValue::string(input_dtype));
    root.set("quantization", JsonValue::string(quantization));
    root.set("output_format", JsonValue::string(output_format));
    root.set("model_family", JsonValue::string(model_family));
    root.set("hardware", JsonValue::string(hardware));
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
    root.set("updated_at", JsonValue::number(static_cast<double>(updated_at)));
    return root;
}

ModelManifest ModelManifest::from_json(const JsonValue& v) {
    ModelManifest m;
    if (!v.is_object()) return m;
    auto get = [&v](const char* key, const char* def) -> std::string {
        const JsonValue* p = v.find(key);
        return (p && p->is_string()) ? p->as_string(def) : def;
    };
    auto get_int_fn = [&v](const char* key) -> uint32_t {
        const JsonValue* p = v.find(key);
        return (p && p->is_number() && p->as_int(0) >= 0) ? static_cast<uint32_t>(p->as_int(0)) : 0u;
    };
    m.model_id = get("model_id", "");
    m.label = get("label", get("name", "").c_str());
    m.version = get("version", "1.0.0");
    m.format = get("format", "rknn");
    m.source_format = get("source_format", "");
    m.task = get("task", "detect");
    m.framework = get("framework", "");
    m.input_layout = get("input_layout", "");
    m.input_dtype = get("input_dtype", "");
    m.quantization = get("quantization", "");
    m.output_format = get("output_format", "");
    m.model_family = get("model_family", "");
    m.hardware = get("hardware", "rk3588");
    m.sha256 = get("sha256", "");
    m.signature = get("signature", "");
    m.origin = get("origin", "local");
    m.converter_version = get("converter_version", "");
    m.runtime_version = get("runtime_version", "");
    if (const JsonValue* s = v.find("status"); s) {
        if (s->is_number()) {
            m.status = static_cast<ModelStatus>(s->as_int(0));
        } else if (s->is_string()) {
            const std::string status = s->as_string();
            if (status == "discovered") m.status = ModelStatus::kDiscovered;
            else if (status == "validating") m.status = ModelStatus::kValidating;
            else if (status == "ready") m.status = ModelStatus::kReady;
            else if (status == "selected") m.status = ModelStatus::kSelected;
            else if (status == "loading") m.status = ModelStatus::kLoading;
            else if (status == "running") m.status = ModelStatus::kRunning;
            else if (status == "failed") m.status = ModelStatus::kFailed;
            else if (status == "invalid") m.status = ModelStatus::kInvalid;
            else if (status == "staging") m.status = ModelStatus::kStaging;
            else if (status == "installed") m.status = ModelStatus::kInstalled;
            else if (status == "quarantined") m.status = ModelStatus::kQuarantined;
        }
    }
    if (const JsonValue* status_name = v.find("status_name"); status_name && status_name->is_string()) {
        const std::string status = status_name->as_string();
        if (status == "installed") m.status = ModelStatus::kInstalled;
        else if (status == "staging") m.status = ModelStatus::kStaging;
    }
    if (const JsonValue* t = v.find("created_at"); t && t->is_number()) m.created_at = t->as_int(0);
    if (const JsonValue* t = v.find("updated_at"); t && t->is_number()) m.updated_at = t->as_int(0);
    m.input_width = get_int_fn("input_width");
    m.input_height = get_int_fn("input_height");
    m.output_count = get_int_fn("output_count");
    m.class_count = get_int_fn("class_count");
    m.rknn_concurrency = get_int_fn("rknn_concurrency");
    if (m.rknn_concurrency == 0) m.rknn_concurrency = 1;
    if (const JsonValue* jn = v.find("class_names"); jn && jn->is_array()) {
        for (const auto& e : jn->as_array()) if (e.is_string()) m.class_names.push_back(e.as_string());
    }
    return m;
}

JsonValue ModelRecord::to_json() const {
    JsonValue root = manifest.to_json();
    root.set("id", JsonValue::string(model_id));
    root.set("name", JsonValue::string(name));
    root.set("path", JsonValue::string(path));
    root.set("checksum", JsonValue::string(checksum));
    root.set("metadata", metadata);
    root.set("record_status", JsonValue::string(model_status_name(status)));
    root.set("status", JsonValue::number(static_cast<double>(static_cast<int>(status))));
    root.set("status_name", JsonValue::string(model_status_name(status)));
    root.set("status_code", JsonValue::number(static_cast<double>(static_cast<int>(status))));
    root.set("failure_code", JsonValue::string(failure_code));
    root.set("failure_message", JsonValue::string(failure_message));
    root.set("selected", JsonValue::boolean(selected));
    root.set("running", JsonValue::boolean(running));
    root.set("created_at", JsonValue::number(static_cast<double>(created_at)));
    root.set("updated_at", JsonValue::number(static_cast<double>(updated_at)));
    return root;
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
        if (error) *error = "MODEL_ROOT_EMPTY";
        return false;
    }
    std::error_code ec;
    if (!fs::create_directories(root_, ec) && ec) {
        if (error) *error = "MODEL_ROOT_CREATE_FAILED: " + ec.message();
        return false;
    }
    if (!ensure_dirs(error)) return false;
    return refresh(error);
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

std::string ModelRegistry::model_dir_locked(const std::string& model_id) const {
    const std::string final_dir = root_ + "/" + model_id;
    std::error_code ec;
    if (fs::is_directory(final_dir, ec)) return final_dir;
    return installed_dir(model_id);
}

std::string ModelRegistry::rknn_path(const std::string& model_id) const {
    return model_dir_locked(model_id) + "/model.rknn";
}
std::string ModelRegistry::metadata_path(const std::string& model_id) const {
    return model_dir_locked(model_id) + "/metadata.json";
}
std::string ModelRegistry::manifest_path(const std::string& model_id) const {
    return model_dir_locked(model_id) + "/manifest.json";
}

bool ModelRegistry::import(const std::string& src_rknn, const std::string& model_id,
                           const ModelManifest& manifest, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!valid_model_id(model_id)) {
        if (error) *error = "MODEL_ID_INVALID";
        return false;
    }
    // 禁止与已安装模型冲突
    if (exists(root_ + "/" + model_id) || exists(installed_dir(model_id))) {
        if (error) *error = "DUPLICATE_MODEL_ID: " + model_id;
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
    if (!sha256_file(staging_dir(model_id) + "/model.rknn", &m.sha256, error)) {
        return false;
    }
    m.updated_at = std::stoll(now_ms());
    if (!write_file_atomic(staging_dir(model_id) + "/manifest.json", m.to_json().dump())) {
        if (error) *error = "写 staging manifest 失败";
        return false;
    }
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::validate(const std::string& model_id, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!validator_) {
        if (error) *error = "VALIDATOR_NOT_CONFIGURED";
        return false;
    }
    const std::string dir = staging_dir(model_id);
    const std::string rknn = dir + "/model.rknn";
    if (!exists(rknn)) {
        if (error) *error = "MODEL_FILE_MISSING";
        return false;
    }
    std::string manifest_text;
    if (!read_file(dir + "/manifest.json", &manifest_text)) {
        if (error) *error = "MANIFEST_MISSING";
        return false;
    }
    const auto parsed = json_parse(manifest_text);
    if (!parsed.ok || !parsed.value.is_object()) {
        if (error) *error = "MANIFEST_INVALID";
        return false;
    }
    ModelManifest manifest = ModelManifest::from_json(parsed.value);
    if (manifest.model_id != model_id || !valid_model_id(model_id)) {
        if (error) *error = "MANIFEST_SCHEMA_INVALID";
        return false;
    }
    std::string actual_sha;
    if (!sha256_file(rknn, &actual_sha, error)) return false;
    if (!is_hex_sha256(manifest.sha256) || manifest.sha256 != actual_sha) {
        if (error) *error = "CHECKSUM_MISMATCH";
        return false;
    }
    JsonValue metadata;
    std::string verr;
    if (!validator_(rknn, &metadata, &verr)) {
        if (error) *error = "LOAD_FAILED: " + verr;
        return false;
    }
    if (!metadata.is_object()) {
        if (error) *error = "METADATA_INVALID";
        return false;
    }
    auto positive = [&metadata](const char* key) {
        const JsonValue* value = metadata.find(key);
        return value && value->is_number() && value->as_int(0) > 0;
    };
    if (!positive("input_width") || !positive("input_height") ||
        !positive("output_count") || !positive("class_count")) {
        if (error) *error = "METADATA_INVALID";
        return false;
    }
    const JsonValue* decode = metadata.find("decode_type");
    if (!decode || (!decode->is_number() && !decode->is_string())) {
        if (error) *error = "METADATA_INVALID";
        return false;
    }
    JsonValue vroot = JsonValue::object();
    vroot.set("ok", JsonValue::boolean(true));
    vroot.set("validated_at", JsonValue::number(static_cast<double>(std::stoll(now_ms()))));
    vroot.set("checksum", JsonValue::string(actual_sha));
    vroot.set("metadata", metadata);
    std::error_code vec;
    fs::create_directories(dir + "/validation", vec);
    if (vec || !write_file_atomic(dir + "/validation/ok.json", vroot.dump()) ||
        !write_file_atomic(dir + "/validation/metadata.json", metadata.dump())) {
        if (error) *error = "VALIDATION_REPORT_WRITE_FAILED";
        return false;
    }
    manifest.sha256 = actual_sha;
    manifest.input_width = static_cast<uint32_t>(metadata.find("input_width")->as_int(0));
    manifest.input_height = static_cast<uint32_t>(metadata.find("input_height")->as_int(0));
    manifest.output_count = static_cast<uint32_t>(metadata.find("output_count")->as_int(0));
    manifest.class_count = static_cast<uint32_t>(metadata.find("class_count")->as_int(0));
    // 模型仍在 staging 目录，尚未 install；状态保持 kStaging（已验证待安装），
    // 只有 install 成功搬到 installed/ 后才写 kInstalled。
    manifest.status = ModelStatus::kStaging;
    manifest.updated_at = std::stoll(now_ms());
    if (!write_file_atomic(dir + "/manifest.json", manifest.to_json().dump())) {
        if (error) *error = "MANIFEST_WRITE_FAILED";
        return false;
    }
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::install(const std::string& model_id, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    if (!write_file_atomic(id + "/manifest.json", m.to_json().dump())) {
        if (error) *error = "写 installed manifest 失败";
        return false;
    }
    if (exists(sd + "/validation/metadata.json")) {
        if (!copy_file(sd + "/validation/metadata.json", id + "/metadata.json", error)) return false;
    }
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::quarantine(const std::string& model_id, const std::string& reason,
                               std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    if (!write_file_atomic(qd + "/reason.json", q.dump())) {
        if (error) *error = "QUARANTINE_REASON_WRITE_FAILED";
        return false;
    }
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::write_active(const std::string& model_id, std::string* error) {
    JsonValue a = JsonValue::object();
    a.set("model_id", JsonValue::string(model_id));
    a.set("activated_at", JsonValue::number(static_cast<double>(std::stoll(now_ms()))));
    if (!write_file_atomic(root_ + "/registry/active.json", a.dump())) {
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
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string model_dir = model_dir_locked(model_id);
    if (!exists(model_dir + "/model.rknn")) {
        if (error) *error = "MODEL_NOT_FOUND";
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
            write_file_atomic(metadata_path(model_id), metadata.dump());
        }
    }
    if (!write_active(model_id, error)) {
        // 写失败：恢复旧值
        if (!old.empty()) write_active(old, nullptr);
        return false;
    }
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::deactivate(std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!write_active("", error)) return false;
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::remove(const std::string& model_id, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 禁止删除正在使用（= active）的模型
    const std::string act = read_active();
    if (!act.empty() && act == model_id) {
        if (error) *error = "模型正在使用（active），禁止删除: " + model_id;
        return false;
    }
    const std::string id = model_dir_locked(model_id);
    if (!exists(id)) {
        if (error) *error = "模型未安装: " + model_id;
        return false;
    }
    std::error_code ec;
    fs::remove_all(id, ec);
    if (ec) {
        if (error) *error = "MODEL_REMOVE_FAILED: " + ec.message();
        return false;
    }
    // install 时 staging 保留副本；remove 时应一并清掉，避免「已删除但 staging 残留」的幽灵模型。
    std::error_code sec;
    fs::remove_all(staging_dir(model_id), sec);
    refresh_locked(nullptr);
    return true;
}

bool ModelRegistry::build_record_locked(const std::string& model_dir, const std::string& model_id,
                                         ModelRecord* out) const {
    if (!out) return false;
    ModelRecord record;
    record.model_id = model_id;
    record.path = model_dir + "/model.rknn";
    record.name = model_id;
    record.format = "rknn";
    record.status = ModelStatus::kDiscovered;
    record.updated_at = std::stoll(now_ms());
    record.selected = (read_active() == model_id);

    const std::string manifest_file = model_dir + "/manifest.json";
    std::string manifest_text;
    if (!read_file(manifest_file, &manifest_text)) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "MANIFEST_MISSING";
        record.failure_message = "模型包缺少 manifest.json";
        *out = std::move(record);
        return true;
    }
    const auto parsed = json_parse(manifest_text);
    if (!parsed.ok || !parsed.value.is_object()) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "MANIFEST_INVALID";
        record.failure_message = parsed.error.empty() ? "manifest 必须是 JSON object" : parsed.error;
        *out = std::move(record);
        return true;
    }
    for (const char* required : {"model_id", "version", "format", "task", "hardware", "sha256"}) {
        const JsonValue* field = parsed.value.find(required);
        if (!field || !field->is_string() || field->as_string().empty()) {
            record.status = ModelStatus::kInvalid;
            record.failure_code = "MANIFEST_SCHEMA_INVALID";
            record.failure_message = std::string("manifest 缺少必填字段: ") + required;
            *out = std::move(record);
            return true;
        }
    }
    record.manifest = ModelManifest::from_json(parsed.value);
    record.name = record.manifest.label.empty() ? model_id : record.manifest.label;
    record.version = record.manifest.version;
    record.format = record.manifest.format;
    record.created_at = record.manifest.created_at;
    if (record.manifest.model_id != model_id || !valid_model_id(record.manifest.model_id)) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "MANIFEST_SCHEMA_INVALID";
        record.failure_message = "manifest.model_id 与目录名不一致或格式非法";
        *out = std::move(record);
        return true;
    }
    if (record.manifest.version.empty() || record.manifest.format != "rknn" ||
        (record.manifest.task != "" && record.manifest.task != "detect")) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "MANIFEST_SCHEMA_INVALID";
        record.failure_message = "manifest 的 version/format/task 不符合要求";
        *out = std::move(record);
        return true;
    }
    if (record.manifest.input_width == 0 || record.manifest.input_height == 0 ||
        record.manifest.output_count == 0 || record.manifest.class_count == 0) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "METADATA_INVALID";
        record.failure_message = "输入尺寸、输出数量和类别数量必须是已知值";
        *out = std::move(record);
        return true;
    }
    if (!record.manifest.class_names.empty() &&
        record.manifest.class_names.size() != record.manifest.class_count) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "METADATA_INVALID";
        record.failure_message = "class_names 数量与 class_count 不一致";
        *out = std::move(record);
        return true;
    }
    std::error_code ec;
    if (!fs::is_regular_file(record.path, ec)) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "MODEL_FILE_MISSING";
        record.failure_message = "model.rknn 不存在或不是普通文件";
        *out = std::move(record);
        return true;
    }
    if (!sha256_file(record.path, &record.checksum, &record.failure_message)) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "MODEL_FILE_READ_FAILED";
        *out = std::move(record);
        return true;
    }
    if (!is_hex_sha256(record.manifest.sha256) || record.manifest.sha256 != record.checksum) {
        record.status = ModelStatus::kInvalid;
        record.failure_code = "CHECKSUM_MISMATCH";
        record.failure_message = "manifest.sha256 与实际模型文件 SHA-256 不一致";
        *out = std::move(record);
        return true;
    }
    const std::string metadata_file = model_dir + "/metadata.json";
    std::string metadata_text;
    if (read_file(metadata_file, &metadata_text)) {
        const auto metadata = json_parse(metadata_text);
        if (!metadata.ok || !metadata.value.is_object()) {
            record.status = ModelStatus::kInvalid;
            record.failure_code = "METADATA_INVALID";
            record.failure_message = "metadata.json 不是合法 object";
            *out = std::move(record);
            return true;
        }
        record.metadata = metadata.value;
    }
    record.status = record.selected ? ModelStatus::kSelected : ModelStatus::kReady;
    if (record.manifest.status == ModelStatus::kFailed || record.manifest.status == ModelStatus::kInvalid) {
        record.status = record.manifest.status;
    }
    *out = std::move(record);
    return true;
}

bool ModelRegistry::scan_locked(std::vector<ModelRecord>* out, std::string* error) const {
    if (!out) return false;
    out->clear();
    std::set<std::string> ids;
    auto scan_dir = [&](const std::string& parent, bool legacy, ModelStatus force_status) {
        std::error_code ec;
        if (!fs::is_directory(parent, ec)) return;
        for (const auto& entry : fs::directory_iterator(parent, ec)) {
            if (ec || !entry.is_directory()) continue;
            const std::string id = entry.path().filename().string();
            if (!valid_model_id(id)) continue;
            if (!legacy && (id == "registry" || id == "installed" || id == "staging" ||
                            id == "cache" || id == "quarantine" || id == "_incoming")) continue;
            if (!ids.insert(id).second) continue;
            ModelRecord record;
            build_record_locked(entry.path().string(), id, &record);
            if (force_status != ModelStatus::kUnknown &&
                record.status != ModelStatus::kInvalid && record.status != ModelStatus::kFailed) {
                // staging 目录：已导入/已验证但未 install 的模型标 kStaging，且不可被选中激活
                record.status = force_status;
                record.selected = false;
            }
            if (legacy && record.failure_code.empty() && record.status == ModelStatus::kReady) {
                record.failure_code = "LEGACY_LAYOUT";
                record.failure_message = "模型仍位于兼容目录 installed/";
            }
            out->push_back(std::move(record));
        }
    };
    scan_dir(root_, false, ModelStatus::kUnknown);
    scan_dir(root_ + "/installed", true, ModelStatus::kUnknown);
    scan_dir(root_ + "/staging", false, ModelStatus::kStaging);
    std::sort(out->begin(), out->end(), [](const ModelRecord& a, const ModelRecord& b) {
        return a.model_id < b.model_id;
    });
    if (error) error->clear();
    return true;
}

bool ModelRegistry::refresh_locked(std::string* error) const {
    std::vector<ModelRecord> next;
    if (!scan_locked(&next, error)) return false;
    snapshot_ = std::make_shared<const std::vector<ModelRecord>>(std::move(next));
    return true;
}

bool ModelRegistry::refresh(std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    return refresh_locked(error);
}

std::vector<ModelRecord> ModelRegistry::records() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!snapshot_) return {};
    return *snapshot_;
}

std::vector<ModelManifest> ModelRegistry::list() const {
    std::vector<ModelManifest> out;
    for (const auto& record : records()) out.push_back(record.manifest);
    return out;
}

}  // namespace ttbox::core
