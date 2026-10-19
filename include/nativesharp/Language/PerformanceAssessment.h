#pragma once

#include <string>
#include <vector>

namespace nativesharp::language
{
struct PerformanceAssessment final
{
    bool passed = false;
    bool backendAndToolchainExcluded = true;
    std::vector<std::string> violations;
};
}
