#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectOopHirFunction.h"

#include <string>
#include <vector>

namespace nativesharp::frontend::internal
{
struct RegularFunctionResult final
{
    // A worker can produce more than one callable for a single member: an async method
    // yields the lazy constructor AND its `$poll` function.
    std::vector<ProjectOopHirFunction> functions;
    std::string error;
    SourceRange errorRange;
    bool success = false;
};
}
