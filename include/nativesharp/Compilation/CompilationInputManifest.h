#pragma once

#include "nativesharp/Compilation/CompilationBundleCache.h"
#include "nativesharp/Compilation/LibraryMetadataRepository.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
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
    const std::filesystem::path& rootDirectory() const noexcept;
    ProjectGraph releaseProjectGraph();
    const ProjectLoadOptions& loadOptions() const noexcept;
    std::span<const std::filesystem::path> inputFiles() const noexcept;
    // The initial graph supplies the known physical inputs. Dependency discovery may append
    // more files before the manifest identity is calculated, without rebuilding the graph.
    void addInputFile(std::filesystem::path path);
    void addProjectGraphInputs(const ProjectGraph& graph);
    void addIdentityField(std::string name, std::string value);
    bool calculateIdentity(
        const LibraryMetadataRepository& metadataRepository,
        std::string& error);
    std::string_view identity() const noexcept;
    void setBundleProductKind(std::string productKind);
    std::string_view bundleProductKind() const noexcept;
    void setBundleOutputPath(std::filesystem::path outputPath);
    void addRequiredBundleOutput(std::filesystem::path outputPath);
    const std::filesystem::path& bundleOutputDirectory() const noexcept;
    const std::vector<std::string>& requiredBundleOutputNames() const noexcept;
    void setBundleLookup(CompilationBundleLookup lookup);
    const CompilationBundleLookup& bundleLookup() const noexcept;

private:
    void collectGraphInputs(const ProjectGraph& graph);
    void canonicalizeInputOrder();

    ProjectGraph graph_;
    std::filesystem::path rootDirectory_;
    ProjectLoadOptions loadOptions_;
    std::vector<std::filesystem::path> inputFiles_;
    std::vector<std::pair<std::string, std::string>> identityFields_;
    std::string identity_;
    std::string bundleProductKind_;
    std::filesystem::path bundleOutputDirectory_;
    std::vector<std::string> requiredBundleOutputNames_;
    CompilationBundleLookup bundleLookup_;
};
}
