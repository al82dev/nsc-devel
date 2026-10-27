#pragma once

#include "nativesharp/Compilation/CompilationBundleCache.h"
#include "nativesharp/Compilation/FileDigestCache.h"
#include "nativesharp/Compilation/LibraryMetadataRepository.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
class CompilationInputManifest;
}

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::driver
{
bool configurePrecompiledModules(
    compilation::CompilationInputManifest& manifest,
    frontend::ProjectCompilation& compilation,
    std::string& error,
    bool materializeDemandedSurface);
std::string fingerprintFiles(
    const compilation::CompilationInputManifest& manifest,
    std::vector<std::filesystem::path> files);
}

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
    void addVerifiedInputDigest(
        std::filesystem::path path,
        std::string digest,
        std::uintmax_t size);
    // Inputs that are neither sources nor loaded metadata are hashed through this cache.
    void setInputDigestCacheRoot(std::filesystem::path root);
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
    void setBundleResultOutput(std::string output);
    std::string_view bundleResultOutput() const noexcept;
    void setBundleLookup(CompilationBundleLookup lookup);
    const CompilationBundleLookup& bundleLookup() const noexcept;

private:
    friend bool driver::configurePrecompiledModules(
        CompilationInputManifest& manifest,
        frontend::ProjectCompilation& compilation,
        std::string& error,
        bool materializeDemandedSurface);
    friend std::string driver::fingerprintFiles(
        const CompilationInputManifest& manifest,
        std::vector<std::filesystem::path> files);

    // Returns the exact source bytes used to calculate the manifest identity. The cache is
    // immutable after identity calculation, so syntax workers may query it concurrently.
    std::optional<std::string> sourceBuffer(
        const std::filesystem::path& path) const;
    bool inputDigest(
        const std::filesystem::path& path,
        std::string& digest,
        std::uintmax_t& size) const;
    void collectGraphInputs(const ProjectGraph& graph);
    void canonicalizeInputOrder();

    ProjectGraph graph_;
    std::filesystem::path rootDirectory_;
    ProjectLoadOptions loadOptions_;
    std::vector<std::filesystem::path> inputFiles_;
    std::unordered_set<std::string> sourceInputKeys_;
    std::unordered_map<std::string, std::string> sourceBuffers_;
    FileDigestCache inputDigestCache_;
    std::unordered_map<std::string, std::string> inputDigests_;
    std::unordered_map<std::string, std::uintmax_t> inputSizes_;
    std::vector<std::pair<std::string, std::string>> identityFields_;
    std::string identity_;
    std::string bundleProductKind_;
    std::filesystem::path bundleOutputDirectory_;
    std::vector<std::string> requiredBundleOutputNames_;
    std::string bundleResultOutput_;
    CompilationBundleLookup bundleLookup_;
};
}
