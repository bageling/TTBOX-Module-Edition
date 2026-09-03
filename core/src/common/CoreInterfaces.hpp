// CoreInterfaces.hpp — Capture/Detector/Coordinate/Mouse 稳定接口
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "capture/ICapture.hpp"
#include "detector/IDetector.hpp"
#include "coordinate/ICoordinate.hpp"
#include "mouse/IMouse.hpp"

namespace ttbox::core {

using ICapture = capture::ICapture;
using IDetector = detector::IDetector;
using ICoordinate = coordinate::ICoordinate;
using IMouse = mouse::IMouse;

}  // namespace ttbox::core
