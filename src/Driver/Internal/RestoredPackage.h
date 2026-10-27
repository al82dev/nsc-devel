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
    // weakly_canonical(sourceRoot), resolved by the first appendArtifact; empty until then.
    std::filesystem::path canonicalSourceRoot;
    std::vector<Artifact> artifacts;
};
} // namespace nativesharp::driver
