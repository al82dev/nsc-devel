#pragma once

#include "nativesharp/Language/CaptureMode.h"

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct Capture final
{
    std::string name;
    std::size_t size = 0;
    std::size_t alignment = 1;
    CaptureMode mode = CaptureMode::Copy;
    bool sourceOutlivesClosure = true;
};
}
