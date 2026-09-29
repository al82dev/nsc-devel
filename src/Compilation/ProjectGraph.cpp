#include "nativesharp/Compilation/ProjectGraph.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
std::string foldedPath(const std::filesystem::path& path)
{
    std::error_code filesystemError;
    std::string key = std::filesystem::weakly_canonical(path, filesystemError).generic_string();
    if (filesystemError) key = std::filesystem::absolute(path).lexically_normal().generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return key;
}
}

ProjectGraph::ProjectGraph(std::filesystem::path rootProjectFile, std::vector<Project> projects)
    : rootProjectFile_(std::move(rootProjectFile)),
      projects_(std::move(projects))
{
}

const std::filesystem::path& ProjectGraph::rootProjectFile() const noexcept
{
    return rootProjectFile_;
}

std::span<const Project> ProjectGraph::projects() const noexcept
{
    return projects_;
}

const Project* ProjectGraph::rootProject() const noexcept
{
    if (projects_.empty())
    {
        return nullptr;
    }
    return &projects_.back();
}

bool ProjectGraph::addPrecompiledProjects(
    std::span<const PrecompiledProjectDescriptor> descriptors,
    std::string& error)
{
    error.clear();
    if (descriptors.empty()) return true;
    if (projects_.empty())
    {
        error = "cannot add precompiled modules to an empty project graph";
        return false;
    }

    std::unordered_map<std::string, std::string> moduleOwners;
    for (const Project& project : projects_)
    {
        moduleOwners.emplace(project.moduleName(), project.projectFile().generic_string());
    }

    std::vector<PrecompiledProjectDescriptor> uniqueDescriptors;
    std::unordered_map<std::string, std::size_t> descriptorByPath;
    for (const PrecompiledProjectDescriptor& descriptor : descriptors)
    {
        const std::string metadataKey = foldedPath(descriptor.metadataPath);
        const auto existing = descriptorByPath.find(metadataKey);
        if (existing != descriptorByPath.end())
        {
            if (uniqueDescriptors[existing->second].moduleName != descriptor.moduleName)
            {
                error = "precompiled metadata '" + descriptor.metadataPath.generic_string() +
                    "' was assigned conflicting module identities";
                return false;
            }
            continue;
        }
        const auto moduleOwner = moduleOwners.find(descriptor.moduleName);
        if (moduleOwner != moduleOwners.end())
        {
            error = "duplicate module name '" + descriptor.moduleName + "' in precompiled metadata '" +
                descriptor.metadataPath.generic_string() + "' and '" + moduleOwner->second + "'";
            return false;
        }
        moduleOwners.emplace(descriptor.moduleName, descriptor.metadataPath.generic_string());
        descriptorByPath.emplace(metadataKey, uniqueDescriptors.size());
        uniqueDescriptors.push_back(descriptor);
    }
    std::ranges::sort(uniqueDescriptors, [](const auto& left, const auto& right) {
        if (left.moduleName != right.moduleName) return left.moduleName < right.moduleName;
        return foldedPath(left.metadataPath) < foldedPath(right.metadataPath);
    });

    std::unordered_map<std::string, std::vector<std::filesystem::path>> metadataByOwner;
    for (const PrecompiledProjectDescriptor& descriptor : descriptors)
    {
        std::vector<std::filesystem::path>& dependencies =
            metadataByOwner[foldedPath(descriptor.ownerProjectFile)];
        const std::string metadataKey = foldedPath(descriptor.metadataPath);
        const bool duplicate = std::any_of(
            dependencies.begin(), dependencies.end(), [&](const std::filesystem::path& path) {
                return foldedPath(path) == metadataKey;
            });
        if (!duplicate) dependencies.push_back(descriptor.metadataPath);
    }
    for (auto& [owner, dependencies] : metadataByOwner)
    {
        (void)owner;
        std::ranges::sort(dependencies, {}, [](const std::filesystem::path& path) {
            return foldedPath(path);
        });
    }

    std::vector<Project> expanded;
    expanded.reserve(projects_.size() + uniqueDescriptors.size());
    for (const PrecompiledProjectDescriptor& descriptor : uniqueDescriptors)
    {
        // Metadata-backed declarations are parsed as separate project nodes so their module
        // identity remains stable. A declaration in one NSMETA can nevertheless mention a
        // public type owned by another NSMETA in the same LibraryReference set. Give each
        // synthetic node visibility of its peer metadata modules; ordinary accessibility rules
        // still prevent private/internal declarations from crossing that boundary.
        std::vector<std::filesystem::path> peerMetadata;
        peerMetadata.reserve(uniqueDescriptors.size() - 1U);
        const std::string ownMetadataKey = foldedPath(descriptor.metadataPath);
        for (const PrecompiledProjectDescriptor& peer : uniqueDescriptors)
        {
            if (foldedPath(peer.metadataPath) != ownMetadataKey) peerMetadata.push_back(peer.metadataPath);
        }
        std::ranges::sort(peerMetadata, {}, [](const std::filesystem::path& path) {
            return foldedPath(path);
        });
        ProjectSourceSet emptySources(descriptor.metadataPath.parent_path());
        expanded.emplace_back(
            descriptor.metadataPath,
            descriptor.moduleName,
            ProjectOutputKind::Library,
            std::move(emptySources),
            std::move(peerMetadata),
            std::vector<LibraryReference>{},
            std::vector<SdkManifest>{},
            projects_.back().languageEdition());
    }
    for (const Project& project : projects_)
    {
        std::vector<std::filesystem::path> references(
            project.projectReferences().begin(), project.projectReferences().end());
        const auto extra = metadataByOwner.find(foldedPath(project.projectFile()));
        if (extra != metadataByOwner.end())
        {
            references.insert(references.end(), extra->second.begin(), extra->second.end());
        }
        std::ranges::sort(references, {}, [](const std::filesystem::path& path) {
            return foldedPath(path);
        });
        references.erase(std::unique(references.begin(), references.end(), [](const auto& left, const auto& right) {
            return foldedPath(left) == foldedPath(right);
        }), references.end());
        expanded.emplace_back(
            project.projectFile(),
            project.moduleName(),
            project.outputKind(),
            project.sourceSet(),
            std::move(references),
            std::vector<LibraryReference>(
                project.libraryReferences().begin(), project.libraryReferences().end()),
            std::vector<SdkManifest>(
                project.sdkReferences().begin(), project.sdkReferences().end()),
            project.languageEdition());
    }
    projects_ = std::move(expanded);
    return true;
}
}
