#pragma once

#include "BuildPreferences.h"
#include "PackageRequest.h"

#include <string>
#include <vector>

namespace nativesharp::driver
{
struct Sidecar final
{
    std::string project;
    std::string rootNamespace;
    BuildPreferences build;
    std::vector<PackageRequest> packages;
    bool createInitialSource = false;
};
} // namespace nativesharp::driver
