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

void ProjectCompilationDumper::dumpComptime(
    const ProjectCompilation& compilation,
    std::ostream& output) const
{
    std::size_t evaluations = 0;
    std::size_t executedSteps = 0;
    std::size_t cacheHits = 0;
    std::size_t cacheMisses = 0;
    for (const ProjectComptimeRecord& record : compilation.comptimeRecords())
    {
        evaluations += record.evaluationCount;
        executedSteps += record.executedSteps;
        cacheHits += record.cacheHits;
        cacheMisses += record.cacheMisses;
    }

    output << "NSCOMPTIME 1.0\n"
           << "files: " << compilation.comptimeRecords().size() << '\n'
           << "evaluations: " << evaluations << '\n'
           << "executed-steps: " << executedSteps << '\n'
           << "cache-hits: " << cacheHits << '\n'
           << "cache-misses: " << cacheMisses << '\n';
    for (const ProjectComptimeRecord& record : compilation.comptimeRecords())
    {
        output << "file project=" << record.projectOrdinal
               << " source=" << record.sourceOrdinal
               << " module=" << record.moduleName
               << " path=" << record.relativeSourcePath << '\n'
               << "  evaluations=" << record.evaluationCount
               << " steps=" << record.executedSteps
               << " cache-hits=" << record.cacheHits
               << " cache-misses=" << record.cacheMisses << '\n'
               << "  reproducibility-digest=" << record.reproducibilityDigest << '\n';
        for (const ComptimeDependency& dependency : record.dependencies)
        {
            output << "  dependency kind=" << dependency.kind
                   << " identity=" << dependency.identity
                   << " digest=" << dependency.digest << '\n';
        }
    }
}
}
