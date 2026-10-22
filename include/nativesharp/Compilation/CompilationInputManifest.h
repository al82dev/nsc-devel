#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"

#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
class CompilationInputManifest final
{
public:
    CompilationInputManifest(
        ProjectGraph graph,
        ProjectLoadOptions loadOptions);

    const ProjectGraph& projectGraph() const noexcept;
    ProjectGraph releaseProjectGraph();
    const ProjectLoadOptions& loadOptions() const noexcept;
    std::span<const std::filesystem::path> inputFiles() const noexcept;
    // The initial graph supplies the known physical inputs. Dependency discovery may append
    // more files before the manifest identity is calculated, without rebuilding the graph.
    void addInputFile(std::filesystem::path path);

private:
    void collectGraphInputs();
    void canonicalizeInputOrder();

    ProjectGraph graph_;
    ProjectLoadOptions loadOptions_;
    std::vector<std::filesystem::path> inputFiles_;
};
}
