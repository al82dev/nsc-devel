#pragma once

#include "LibraryReferenceDescriptor.h"
#include "SourceIncludeRule.h"

#include "nativesharp/Compilation/GlobPattern.h"
#include "nativesharp/Compilation/ProjectBuildSettings.h"
#include "nativesharp/Compilation/ProjectOutputKind.h"
#include "nativesharp/Language/ErrorSemantics.h"

#include <optional>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
struct ProjectDescriptor final
{
    std::optional<std::string> name;
    std::optional<std::string> packageName;
    std::optional<ProjectOutputKind> outputKind;
    std::optional<language::PanicProfile> panicProfile;
    std::optional<std::string> panicHandler;
    ProjectBuildSettings buildSettings;
    std::vector<SourceIncludeRule> includes;
    std::vector<GlobPattern> removes;
    std::vector<std::string> projectReferences;
    std::vector<LibraryReferenceDescriptor> libraryReferences;
    std::vector<std::string> sdkReferences;
    std::vector<std::string> friendModules;
    // EN: Freeze 3.0 C-05: an implicit (in-memory) project names its source files literally, so
    //     they are taken directly instead of walking the directory, which may be any folder.
    // ES: Congelamiento 3.0 C-05: un proyecto implicito (en memoria) nombra sus fuentes
    //     literalmente; se toman directo en lugar de recorrer el directorio, que puede ser cualquiera.
    bool implicitProject = false;
};
}
