#pragma once

#include <string>

#include "nativesharp/Language/GeneratedCost.h"

namespace nativesharp::language
{
struct CostEvent final
{
    GeneratedCost kind;
    std::string sourceConstruct;
    bool explicitlyRequested = false;
};
}
