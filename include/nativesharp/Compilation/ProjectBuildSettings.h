#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

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
    // EN: T19-036. CpuLevel: `x86-64`, `x86-64-v2`, `x86-64-v3` or `x86-64-v4`.
    // ES: T19-036. CpuLevel: `x86-64`, `x86-64-v2`, `x86-64-v3` o `x86-64-v4`.
    std::optional<std::string> cpuLevel;
};
}
