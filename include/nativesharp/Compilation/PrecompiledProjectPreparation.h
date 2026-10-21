#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"

#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::compilation
{
// Applies the existing metadata-backed dependency closure to an already loaded project graph.
// The caller owns project loading and timing; this operation performs no code generation,
// linking, installation, package restore, or metadata writes.
bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error);
}
