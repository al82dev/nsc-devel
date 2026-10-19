#pragma once

#include <string>

#include "nativesharp/Language/CleanupKind.h"

namespace nativesharp::language
{
struct CleanupAction final { CleanupKind kind; std::string name; };
}
