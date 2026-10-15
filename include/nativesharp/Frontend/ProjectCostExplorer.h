#pragma once

#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectFunctionCosts final
{
    std::string function;
    std::uint64_t staticStackBytesLowerBound = 0;
    std::uint64_t arenaAllocationSites = 0;
    std::uint64_t heapAllocationSites = 0;
    std::uint64_t copySites = 0;
    std::uint64_t moveSites = 0;
    std::uint64_t dropSites = 0;
    std::uint64_t dynamicDispatchSites = 0;
    std::uint64_t lockSites = 0;
    std::uint64_t atomicSites = 0;
    std::uint64_t unwindSites = 0;
    std::uint64_t monomorphizations = 0;
    std::vector<std::string> causes;
};

struct ProjectCostReport final
{
    std::vector<ProjectFunctionCosts> functions;
    ProjectFunctionCosts totals;
};

ProjectCostReport exploreProjectCosts(std::span<const ProjectOopMirFunction> functions);
std::string projectCostReportText(const ProjectCostReport& report);
std::string projectCostReportJson(const ProjectCostReport& report);
}
