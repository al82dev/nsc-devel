#pragma once

#include "VisitState.h"

#include "nativesharp/Compilation/Project.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::compilation::detail
{
struct GraphLoadState final
{
    std::unordered_map<std::string, VisitState> visits;
    std::vector<std::filesystem::path> pathStack;
    std::vector<std::string> moduleStack;
    std::vector<Project> orderedProjects;
};
}
