#pragma once

#include "nativesharp/Language/DeconstructionBindingMode.h"

#include <string>

namespace nativesharp::language
{
struct DeconstructionBinding final
{
    std::string name;
    DeconstructionBindingMode mode = DeconstructionBindingMode::Value;
};
}
