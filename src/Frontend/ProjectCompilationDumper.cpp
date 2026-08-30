#include "nativesharp/Frontend/ProjectCompilationDumper.h"

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Source/SourceFile.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectCompilationDumper::dump(
    const ProjectCompilation& compilation,
    std::ostream& output) const
{
    const compilation::ProjectGraph* graph = compilation.projectGraph();
    output << "project-compilation\n";
    if (graph == nullptr)
    {
        output << "  graph: unavailable\n";
        return;
    }

    output << "  root: " << graph->rootProjectFile().generic_string() << '\n';
    output << "  modules: " << graph->projects().size() << '\n';
    output << "  syntax-units: " << compilation.units().size() << '\n';
    for (std::size_t index = 0; index < compilation.units().size(); ++index)
    {
        const ProjectSyntaxUnit& unit = *compilation.units()[index];
        const source::SourceFile* sourceFile = compilation.sourceManager().getFile(unit.fileId());
        output << "    [" << index << "] module=" << unit.moduleName();
        output << " project=" << unit.projectOrdinal();
        output << " source=" << unit.sourceOrdinal();
        output << " file-id=" << unit.fileId().value();
        output << " syntax=" << (unit.isSyntaxValid() ? "ok" : "error") << '\n';
        if (sourceFile != nullptr)
        {
            output << "      path: " << sourceFile->path().generic_string() << '\n';
        }
    }
}
}
