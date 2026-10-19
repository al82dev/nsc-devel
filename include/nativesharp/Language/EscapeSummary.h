#pragma once

#include <string>
#include <vector>

#include "nativesharp/Language/EscapeClass.h"

namespace nativesharp::language
{
struct EscapeSummary final
{
    std::string functionIdentity;
    std::vector<EscapeClass> parameters;
};
}
