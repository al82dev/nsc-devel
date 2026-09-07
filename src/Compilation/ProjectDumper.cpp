#include "nativesharp/Compilation/ProjectDumper.h"

#include "nativesharp/Compilation/Project.h"

#include <ostream>

namespace nativesharp::compilation
{
void ProjectDumper::dump(const Project& project, std::ostream& output) const
{
    output << "project " << project.moduleName() << '\n';
    output << "  file: " << project.projectFile().generic_string() << '\n';
    const char* outputKind = "static-library";
    if (project.outputKind() == ProjectOutputKind::Executable)
    {
        outputKind = "exe";
    }
    else if (project.outputKind() == ProjectOutputKind::SharedLibrary)
    {
        outputKind = "shared-library";
    }
    output << "  output: " << outputKind << '\n';
    output << "  sources: " << project.sourceSet().sources().size() << '\n';
    for (const ProjectSource& source : project.sourceSet().sources())
    {
        output << "    [" << source.ordinal << "] " << source.projectRelativePath.generic_string() << '\n';
    }
    output << "  project-references: " << project.projectReferences().size() << '\n';
    for (const std::filesystem::path& reference : project.projectReferences())
    {
        output << "    " << reference.generic_string() << '\n';
    }
    output << "  library-references: " << project.libraryReferences().size() << '\n';
    for (const LibraryReference& reference : project.libraryReferences())
    {
        output << "    library: " << reference.libraryPath.generic_string() << '\n';
        output << "      metadata: " << reference.metadataPath.generic_string() << '\n';
        if (!reference.runtimePath.empty())
        {
            output << "      runtime: " << reference.runtimePath.generic_string() << '\n';
        }
    }
}
}
