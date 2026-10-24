#pragma once

#include "nativesharp/Compilation/PrecompiledSurfaceDemand.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
// Returns the text a compilation will read for one source file, or no value to read the file.
using PrecompiledSurfaceSourceProvider =
    std::function<std::optional<std::string>(const std::filesystem::path&)>;

// Collects the names written by every source of the source projects in `graph`: identifiers and
// keywords, the names inside string literals (an interpolation is code), and the panic handler of
// each project. Comments are ignored. The demand is disabled when a source cannot be read, does not
// tokenize, or may need compile-time expansion, whose output is not visible in the source text.
PrecompiledSurfaceDemand collectPrecompiledSurfaceDemand(
    const ProjectGraph& graph,
    const PrecompiledSurfaceSourceProvider& sourceProvider);
} // namespace nativesharp::compilation
