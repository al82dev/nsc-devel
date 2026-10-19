#pragma once

#include <string>

#include "nativesharp/Language/ConformanceTier.h"

namespace nativesharp::language
{
struct ConformanceCase final { std::string identity; ConformanceTier tier; };
}
