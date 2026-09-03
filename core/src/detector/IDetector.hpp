// IDetector.hpp — Core 识别边界
#pragma once

#include <string>
#include <vector>
#include "common/CoreContracts.hpp"

namespace ttbox::core::detector {

class IDetector {
public:
    virtual ~IDetector() = default;
    virtual bool detect(const Frame& frame, std::vector<Detection>* detections,
                        std::string* error = nullptr) = 0;
};

}  // namespace ttbox::core::detector
