// CoreInterface.hpp — TTBOX Core 稳定边界（第1步）
#pragma once

#include "common/CoreInterfaces.hpp"

namespace ttbox::core {
using ICapture = capture::ICapture;
using IDetector = detector::IDetector;
using ICoordinate = coordinate::ICoordinate;
using IMouse = mouse::IMouse;
}
