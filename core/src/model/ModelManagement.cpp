// ModelManagement.cpp — 默认文件级校验器实现。
#include "model/ModelManagement.hpp"

#include <cstdio>

namespace ttbox::core {

bool ModelManagement::file_level_validator(const std::string& rknn_path, JsonValue* meta_out,
                                           std::string* error) {
    FILE* f = std::fopen(rknn_path.c_str(), "rb");
    if (!f) {
        if (error) *error = "无法打开模型文件: " + rknn_path;
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fclose(f);
    if (size <= 0) {
        if (error) *error = "模型文件为空";
        return false;
    }
    if (size < 1024) {
        if (error) *error = "模型文件过小（<1KB），疑似非 RKNN 文件";
        return false;
    }
    // 文件级校验无法解析 RKNN 头（需板端 RKNN runtime）。
    // metadata 输出占位空对象；UI 对缺失字段（输入尺寸/类别数）显示"暂无数据"。
    if (meta_out) *meta_out = JsonValue::object();
    return true;
}

}  // namespace ttbox::core
