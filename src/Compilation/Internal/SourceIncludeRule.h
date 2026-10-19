#pragma once

#include "nativesharp/Compilation/GlobPattern.h"

#include <vector>

namespace nativesharp::compilation
{
struct SourceIncludeRule final
{
    GlobPattern include;
    std::vector<GlobPattern> excludes;
};
}
