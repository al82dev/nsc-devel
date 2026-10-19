#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

namespace nativesharp::compilation
{
// Build defaults declared by the root .nsproj. The driver applies explicit
// command-line options after these values, so automation can keep stable
// defaults in the project without losing one-off overrides.
struct ProjectBuildSettings final
{
    std::optional<unsigned> optimizationLevel;
    std::optional<std::filesystem::path> outputPath;
    std::optional<std::filesystem::path> libraryReferenceRoot;
    std::optional<std::size_t> compilerJobs;
    std::optional<std::size_t> genericInstantiationDepth;
    std::optional<std::size_t> genericInstantiationCount;
};
}
