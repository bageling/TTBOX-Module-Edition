#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "rknn/RKNNEngine.hpp"

namespace ttbox::core {

enum class ModelBackendKind {
    kAuto,
    kRknn,
    kOnnx,
};

class IModelBackend {
public:
    virtual ~IModelBackend() = default;
    virtual ModelBackendKind kind() const = 0;
    virtual bool init(const std::string& model_path, std::string* error = nullptr) = 0;
    virtual void destroy() = 0;
    virtual bool initialized() const = 0;
    virtual const RknnModelInfo& info() const = 0;
    virtual bool infer(const void* input, size_t input_size,
                       std::vector<std::vector<float>>& outputs,
                       std::string* error = nullptr) = 0;
};

class ModelBackendFactory {
public:
    static std::unique_ptr<IModelBackend> create(ModelBackendKind kind,
                                                  std::string* error = nullptr);
    static ModelBackendKind detect_kind(const std::string& model_path);
};

}  // namespace ttbox::core
