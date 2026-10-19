#pragma once

#include <string>

#include "nativesharp/Language/ModuleDependencyKind.h"

namespace nativesharp::language
{
struct ModuleDependency final { std::string from; std::string to; ModuleDependencyKind kind; };
}
