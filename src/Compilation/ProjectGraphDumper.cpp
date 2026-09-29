#include "nativesharp/Compilation/ProjectGraphDumper.h"

#include "nativesharp/Compilation/ProjectGraph.h"

#include <ostream>

namespace nativesharp::compilation
{
void ProjectGraphDumper::dump(const ProjectGraph& graph, std::ostream& output) const
{
    output << "project-graph\n";
    output << "  root: " << graph.rootProjectFile().generic_string() << '\n';
    output << "  projects: " << graph.projects().size() << '\n';

    for (std::size_t index = 0; index < graph.projects().size(); ++index)
    {
        const Project& project = graph.projects()[index];
        output << "    [" << index << "] " << project.moduleName();
        if (index + 1 == graph.projects().size())
        {
            output << " (root)";
        }
        output << '\n';
        output << "      file: " << project.projectFile().generic_string() << '\n';
        output << "      language: " << language::languageEditionName(project.languageEdition()) << '\n';
        output << "      sources: " << project.sourceSet().sources().size() << '\n';
        output << "      references: " << project.projectReferences().size() << '\n';
        output << "      library-references: " << project.libraryReferences().size() << '\n';
    }
}
}
