#pragma once

#include "Artifact.h"
#include "PackageRequest.h"

#include <filesystem>
#include <vector>

namespace nativesharp::driver
{
struct RestoredPackage final
{
    PackageRequest request;
    std::filesystem::path sourceRoot;
    std::vector<Artifact> artifacts;
};
} // namespace nativesharp::driver
