// Json.cpp — 极简 JSON 解析/序列化实现（递归下降解析器）
/*
 * TTBOX 文件说明
 *
 * 文件：Json.cpp
 *
 * 作用：
 *   TTBOX 自用的 JSON 解析和序列化库。
 *   不依赖第三方库，轻量级实现。
 *
 * 小白理解：
 *   JSON 是一种通用的数据格式。
 *   这个模块负责把 JSON 文本转换成 C++ 能用的数据，
 *   也负责把 C++ 数据转换成 JSON 文本。
 *
 * 注意：
 *   本注释仅用于说明代码，不改变程序逻辑。
 */

#include "Json.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ttbox::core {

// ---------------------------------------------------------------------------
// JsonValue 构建/访问
// ---------------------------------------------------------------------------

JsonValue JsonValue::null() {
    return JsonValue();
}

JsonValue JsonValue::boolean(bool v) {
    JsonValue j;
    j.type_ = JsonType::kBool;
    j.bool_ = v;
    return j;
}

JsonValue JsonValue::number(double v) {
    JsonValue j;
    j.type_ = JsonType::kNumber;
    j.num_ = v;
    return j;
}

JsonValue JsonValue::string(std::string v) {
    JsonValue j;
    j.type_ = JsonType::kString;
    j.str_ = std::move(v);
    return j;
}

JsonValue JsonValue::array() {
    JsonValue j;
    j.type_ = JsonType::kArray;
    return j;
}

JsonValue JsonValue::array(std::vector<JsonValue> v) {
    JsonValue j;
    j.type_ = JsonType::kArray;
    j.arr_ = std::move(v);
    return j;
}

JsonValue JsonValue::object() {
    JsonValue j;
    j.type_ = JsonType::kObject;
    return j;
}

bool JsonValue::as_bool(bool def) const {
    return type_ == JsonType::kBool ? bool_ : def;
}

double JsonValue::as_number(double def) const {
    return type_ == JsonType::kNumber ? num_ : def;
}

int64_t JsonValue::as_int(int64_t def) const {
    if (type_ != JsonType::kNumber) return def;
    if (std::isnan(num_) || std::isinf(num_)) return def;
    return static_cast<int64_t>(num_);
}

std::string JsonValue::as_string(const std::string& def) const {
    return type_ == JsonType::kString ? str_ : def;
}

const std::vector<JsonValue>& JsonValue::as_array() const {
    return arr_;
}

const std::map<std::string, JsonValue>& JsonValue::as_object() const {
    return obj_;
}

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type_ != JsonType::kObject) return nullptr;
    auto it = obj_.find(key);
    return it == obj_.end() ? nullptr : &it->second;
}

void JsonValue::set(const std::string& key, JsonValue v) {
    if (type_ != JsonType::kObject) return;
    obj_[key] = std::move(v);
}

void JsonValue::push_back(JsonValue v) {
    if (type_ != JsonType::kArray) return;
    arr_.push_back(std::move(v));
}

bool JsonValue::operator==(const JsonValue& o) const {
    if (type_ != o.type_) return false;
    switch (type_) {
        case JsonType::kNull: return true;
        case JsonType::kBool: return bool_ == o.bool_;
        case JsonType::kNumber: return num_ == o.num_;
        case JsonType::kString: return str_ == o.str_;
        case JsonType::kArray: return arr_ == o.arr_;
        case JsonType::kObject: return obj_ == o.obj_;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 序列化（dump）
// ---------------------------------------------------------------------------

namespace {

void dump_string(std::string& out, const std::string& s) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

void dump_value(std::string& out, const JsonValue& v) {
    switch (v.type()) {
        case JsonType::kNull: out += "null"; break;
        case JsonType::kBool: out += v.as_bool() ? "true" : "false"; break;
        case JsonType::kNumber: {
            double d = v.as_number();
            if (std::isnan(d) || std::isinf(d)) {
                out += "null";
            } else if (d == static_cast<int64_t>(d) && std::fabs(d) < 1e15) {
                out += std::to_string(static_cast<int64_t>(d));
            } else {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.9g", d);
                out += buf;
            }
            break;
        }
        case JsonType::kString: dump_string(out, v.as_string()); break;
        case JsonType::kArray: {
            out.push_back('[');
            bool first = true;
            for (const auto& e : v.as_array()) {
                if (!first) out.push_back(',');
                first = false;
                dump_value(out, e);
            }
            out.push_back(']');
            break;
        }
        case JsonType::kObject: {
            out.push_back('{');
            bool first = true;
            for (const auto& [k, val] : v.as_object()) {
                if (!first) out.push_back(',');
                first = false;
                dump_string(out, k);
                out.push_back(':');
                dump_value(out, val);
            }
            out.push_back('}');
            break;
        }
    }
}

}  // namespace

std::string JsonValue::dump() const {
    std::string out;
    dump_value(out, *this);
    return out;
}

// ---------------------------------------------------------------------------
// 解析器
// ---------------------------------------------------------------------------

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    JsonParseResult parse() {
        JsonParseResult result;
        skip_ws();
        JsonValue v;
        if (!parse_value(v)) {
            result.ok = false;
            result.error = "JSON 语法错误 @ 位置 " + std::to_string(pos_) + ": " + err_;
            return result;
        }
        skip_ws();
        if (pos_ != text_.size()) {
            result.ok = false;
            result.error = "JSON 尾部存在多余内容 @ 位置 " + std::to_string(pos_);
            return result;
        }
        result.ok = true;
        result.value = std::move(v);
        return result;
    }

private:
    const std::string& text_;
    size_t pos_ = 0;
    std::string err_;

    void skip_ws() {
        while (pos_ < text_.size() &&
               (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                text_[pos_] == '\n' || text_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool fail(const std::string& msg) {
        if (err_.empty()) err_ = msg;
        return false;
    }

    bool parse_value(JsonValue& out) {
        if (pos_ >= text_.size()) return fail("意外的文件结尾");
        char c = text_[pos_];
        switch (c) {
            case '{': return parse_object(out);
            case '[': return parse_array(out);
            case '"': return parse_string(out);
            case 't':
            case 'f': return parse_bool(out);
            case 'n': return parse_null(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
                return fail(std::string("无法识别的字符 '") + c + "'");
        }
    }

    bool parse_object(JsonValue& out) {
        ++pos_;  // {
        out = JsonValue::object();
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return fail("对象成员名必须是字符串");
            }
            JsonValue key_v;
            if (!parse_string(key_v)) return false;
            std::string key = key_v.as_string();
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                return fail("对象成员名后缺少 ':'");
            }
            ++pos_;
            skip_ws();
            JsonValue val;
            if (!parse_value(val)) return false;
            out.set(key, std::move(val));
            skip_ws();
            if (pos_ >= text_.size()) return fail("对象未闭合");
            char c = text_[pos_];
            if (c == ',') {
                ++pos_;
                continue;
            }
            if (c == '}') {
                ++pos_;
                return true;
            }
            return fail("对象成员之间缺少 ',' 或 '}'");
        }
    }

    bool parse_array(JsonValue& out) {
        ++pos_;  // [
        out = JsonValue::array();
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_ws();
            JsonValue val;
            if (!parse_value(val)) return false;
            out.push_back(std::move(val));
            skip_ws();
            if (pos_ >= text_.size()) return fail("数组未闭合");
            char c = text_[pos_];
            if (c == ',') {
                ++pos_;
                continue;
            }
            if (c == ']') {
                ++pos_;
                return true;
            }
            return fail("数组元素之间缺少 ',' 或 ']'");
        }
    }

    bool parse_string(JsonValue& out) {
        ++pos_;  // "
        std::string s;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') {
                out = JsonValue::string(std::move(s));
                return true;
            }
            if (c == '\\') {
                if (pos_ >= text_.size()) return fail("字符串转义不完整");
                char e = text_[pos_++];
                switch (e) {
                    case '"': s.push_back('"'); break;
                    case '\\': s.push_back('\\'); break;
                    case '/': s.push_back('/'); break;
                    case 'b': s.push_back('\b'); break;
                    case 'f': s.push_back('\f'); break;
                    case 'n': s.push_back('\n'); break;
                    case 'r': s.push_back('\r'); break;
                    case 't': s.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) return fail("\\uXXXX 转义不完整");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = text_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                            else return fail("\\uXXXX 含非法十六进制字符");
                        }
                        // 仅支持 BMP（U+0000~U+FFFF）；代理对按原始码元追加（UTF-8 简化）
                        if (cp < 0x80) {
                            s.push_back(static_cast<char>(cp));
                        } else if (cp < 0x800) {
                            s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: return fail("未知转义字符");
                }
            } else {
                s.push_back(c);
            }
        }
        return fail("字符串未闭合");
    }

    bool parse_number(JsonValue& out) {
        size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
        bool any = false;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
            any = true;
        }
        if (!any) return fail("数字格式错误（缺少整数部分）");
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            bool frac = false;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
                frac = true;
            }
            if (!frac) return fail("数字格式错误（小数部分为空）");
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            bool exp = false;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
                exp = true;
            }
            if (!exp) return fail("数字格式错误（指数部分为空）");
        }
        std::string tok = text_.substr(start, pos_ - start);
        errno = 0;
        char* end = nullptr;
        double d = std::strtod(tok.c_str(), &end);
        if (errno == ERANGE || end == nullptr || *end != '\0') {
            return fail("数字解析失败（超出范围或非法）");
        }
        out = JsonValue::number(d);
        return true;
    }

    bool parse_bool(JsonValue& out) {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            out = JsonValue::boolean(true);
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            out = JsonValue::boolean(false);
            return true;
        }
        return fail("无法识别的字面量（应为 true/false）");
    }

    bool parse_null(JsonValue& out) {
        if (text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            out = JsonValue::null();
            return true;
        }
        return fail("无法识别的字面量（应为 null）");
    }
};

}  // namespace

JsonParseResult json_parse(const std::string& text) {
    Parser p(text);
    return p.parse();
}

JsonParseResult json_parse_file(const std::string& path) {
    JsonParseResult result;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        result.ok = false;
        result.error = "无法打开配置文件: " + path;
        return result;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (f.bad()) {
        result.ok = false;
        result.error = "读取配置文件失败: " + path;
        return result;
    }
    return json_parse(ss.str());
}

}  // namespace ttbox::core
