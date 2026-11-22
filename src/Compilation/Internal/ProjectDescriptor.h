#pragma once

#include "LibraryReferenceDescriptor.h"
#include "CppLibraryReferenceDescriptor.h"
#include "SourceIncludeRule.h"

#include "nativesharp/Compilation/GlobPattern.h"
#include "nativesharp/Compilation/CppConsumerVolatileMode.h"
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
    std::optional<CppConsumerVolatileMode> cppConsumerVolatileMode;
    // EN: Item 18.14 (D12). The Include\<version> directory of the Windows SDK installed on this machine; its ucrt,
    //     um and shared directories are system include directories of every C/C++ reference.
    // ES: Item 18.14 (D12). El directorio Include\<version> del Windows SDK instalado; sus directorios ucrt, um y
    //     shared son directorios de sistema de cada referencia C/C++.
    std::optional<std::string> windowsSdkIncludeRoot;
    // Item 18.14: the Visual C++ include directory (vcruntime.h) the UCRT headers include. / El directorio include de
    //     Visual C++ (vcruntime.h) que incluyen los encabezados del UCRT.
    std::optional<std::string> vcToolsIncludeRoot;
    ProjectBuildSettings buildSettings;
    std::vector<SourceIncludeRule> includes;
    std::vector<GlobPattern> removes;
    std::vector<std::string> projectReferences;
    std::vector<LibraryReferenceDescriptor> libraryReferences;
    std::vector<CppLibraryReferenceDescriptor> cppLibraryReferences;
    std::vector<std::string> sdkReferences;
    std::vector<std::string> friendModules;
    // EN: Freeze C-05: an implicit (in-memory) project names its source files literally, so
    //     they are taken directly instead of walking the directory, which may be any folder.
    // ES: Congelamiento C-05: un proyecto implicito (en memoria) nombra sus fuentes
    //     literalmente; se toman directo en lugar de recorrer el directorio, que puede ser cualquiera.
    bool implicitProject = false;
};
}
