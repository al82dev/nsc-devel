#pragma once

#include "nativesharp/Frontend/ProjectFunctionCosts.h"

#include <vector>

namespace nativesharp::frontend
{
struct ProjectCostReport final
{
    std::vector<ProjectFunctionCosts> functions;
    ProjectFunctionCosts totals;
};
}
