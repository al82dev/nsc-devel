#pragma once

#include <string>

#include "nativesharp/Language/ImportKind.h"

namespace nativesharp::language
{
struct Import final { ImportKind kind; std::string identity; std::string alias; };
}
