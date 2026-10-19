#pragma once

#include <string>

#include "nativesharp/Language/NumericType.h"

namespace nativesharp::language
{
struct StrongNumericType final { std::string identity; NumericType representation; };
}
