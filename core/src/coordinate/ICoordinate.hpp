// ICoordinate.hpp — Core 坐标控制边界
#pragma once

#include <string>
#include <vector>
#include "common/CoreContracts.hpp"

namespace ttbox::core::coordinate {

class ICoordinate {
public:
    virtual ~ICoordinate() = default;
    virtual bool calculate(const std::vector<Detection>& detections,
                           MouseCommand* command, std::string* error = nullptr) = 0;
};

}  // namespace ttbox::core::coordinate
