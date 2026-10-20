#pragma once

#include "nativesharp/Driver/ExecutableMigrationStep.h"

#include <string>
#include <vector>

namespace nativesharp::driver
{
struct ExecutableMigrationChapter final
{
    std::string identity;
    int order = 0;
    std::vector<ExecutableMigrationStep> steps;
};
} // namespace nativesharp::driver
