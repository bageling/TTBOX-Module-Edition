// IMouse.hpp — Core 鼠标输出边界
#pragma once

#include <string>
#include "common/CoreContracts.hpp"

namespace ttbox::core::mouse {

class IMouse {
public:
    virtual ~IMouse() = default;
    virtual bool send(const MouseCommand& command, std::string* error = nullptr) = 0;
};

}  // namespace ttbox::core::mouse
