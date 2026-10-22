#include "nativesharp/Compilation/CompilationInputManifest.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace nativesharp::compilation
{
namespace
{
std::string inputPathKey(const std::filesystem::path& path)
{
    std::string key = path.lexically_normal().generic_string();
    std::transform(
        key.begin(), key.end(), key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}
}

CompilationInputManifest::CompilationInputManifest(
    ProjectGraph graph,
    ProjectLoadOptions loadOptions)
    : graph_(std::move(graph)),
      loadOptions_(std::move(loadOptions))
{
    collectGraphInputs();
    canonicalizeInputOrder();
}

const ProjectGraph& CompilationInputManifest::projectGraph() const noexcept
{
    return graph_;
}

ProjectGraph CompilationInputManifest::releaseProjectGraph()
{
    return std::move(graph_);
}

const ProjectLoadOptions& CompilationInputManifest::loadOptions() const noexcept
{
    return loadOptions_;
}

std::span<const std::filesystem::path> CompilationInputManifest::inputFiles() const noexcept
{
    return inputFiles_;
}

void CompilationInputManifest::addInputFile(std::filesystem::path path)
{
    if (path.empty())
    {
        return;
    }
    path = path.lexically_normal();
    const std::string key = inputPathKey(path);
    const auto position = std::lower_bound(
        inputFiles_.begin(), inputFiles_.end(), key,
        [](const std::filesystem::path& candidate, const std::string& expected)
        {
            return inputPathKey(candidate) < expected;
        });
    if (position != inputFiles_.end() && inputPathKey(*position) == key)
    {
        return;
    }
    inputFiles_.insert(position, std::move(path));
}

void CompilationInputManifest::collectGraphInputs()
{
    for (const Project& project : graph_.projects())
    {
        inputFiles_.push_back(project.projectFile());
        for (const ProjectSource& source : project.sourceSet().sources())
        {
            inputFiles_.push_back(source.absolutePath);
        }
        for (const LibraryReference& reference : project.libraryReferences())
        {
            if (!reference.libraryPath.empty())
            {
                inputFiles_.push_back(reference.libraryPath);
            }
            if (!reference.metadataPath.empty())
            {
                inputFiles_.push_back(reference.metadataPath);
            }
            if (!reference.runtimePath.empty())
            {
                inputFiles_.push_back(reference.runtimePath);
            }
        }
        for (const SdkManifest& sdk : project.sdkReferences())
        {
            if (!sdk.manifestPath.empty())
            {
                inputFiles_.push_back(sdk.manifestPath);
            }
            for (const SdkLibraryDependency& library : sdk.libraries)
            {
                inputFiles_.push_back(
                    sdk.manifestPath.parent_path() / library.relativePath);
            }
        }
    }
}

void CompilationInputManifest::canonicalizeInputOrder()
{
    std::sort(
        inputFiles_.begin(), inputFiles_.end(),
        [](const std::filesystem::path& left, const std::filesystem::path& right)
        {
            return inputPathKey(left) < inputPathKey(right);
        });
    inputFiles_.erase(
        std::unique(
            inputFiles_.begin(), inputFiles_.end(),
            [](const std::filesystem::path& left, const std::filesystem::path& right)
            {
                return inputPathKey(left) == inputPathKey(right);
            }),
        inputFiles_.end());
}
}
