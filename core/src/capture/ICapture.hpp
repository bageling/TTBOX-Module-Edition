// ICapture.hpp — Core 采集边界
#pragma once

#include <memory>
#include <string>
#include "common/CoreContracts.hpp"

namespace ttbox::core::capture {

class ICapture {
public:
    virtual ~ICapture() = default;
    virtual bool open(std::string* error = nullptr) = 0;
    virtual bool start(std::string* error = nullptr) = 0;
    virtual void stop() = 0;
    virtual void close() = 0;
    virtual std::shared_ptr<Frame> latest_frame() const = 0;
};

}  // namespace ttbox::core::capture
