#pragma once

#include <string>

#include "nativesharp/Language/PatternBindingMode.h"

namespace nativesharp::language
{
struct PatternBinding final
{
    std::string name;
    std::string typeKey;
    PatternBindingMode mode = PatternBindingMode::Value;
};
}
