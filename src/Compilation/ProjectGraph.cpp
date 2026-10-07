#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Language/ModuleSemantics.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
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

bool ProjectGraph::addLibraryReferences(
    const std::filesystem::path& ownerProjectFile,
    std::span<const LibraryReference> references,
    std::string& error)
{
    error.clear();
    if (references.empty()) return true;
    const std::string ownerKey = foldedPath(ownerProjectFile);
    bool foundOwner = false;
    std::vector<Project> expanded;
    expanded.reserve(projects_.size());
    for (const Project& project : projects_)
    {
        std::vector<LibraryReference> libraries(
            project.libraryReferences().begin(), project.libraryReferences().end());
        if (foldedPath(project.projectFile()) == ownerKey)
        {
            foundOwner = true;
            for (const LibraryReference& reference : references)
            {
                const std::string key = foldedPath(reference.libraryPath);
                const bool duplicate = std::ranges::any_of(libraries, [&](const auto& existing) {
                    return foldedPath(existing.libraryPath) == key;
                });
                if (!duplicate) libraries.push_back(reference);
            }
            std::ranges::sort(libraries, [](const auto& left, const auto& right) {
                return foldedPath(left.libraryPath) < foldedPath(right.libraryPath);
            });
        }
        expanded.emplace_back(
            project.projectFile(), project.moduleName(), project.outputKind(), project.sourceSet(),
            std::vector<std::filesystem::path>(
                project.projectReferences().begin(), project.projectReferences().end()),
            std::move(libraries),
            std::vector<SdkManifest>(project.sdkReferences().begin(), project.sdkReferences().end()),
            project.panicProfile(),
            std::string(project.panicHandler()), std::string(project.packageName()),
            std::vector<std::string>(
                project.friendModules().begin(), project.friendModules().end()));
    }
    if (!foundOwner)
    {
        error = "cannot add precompiled closure to unknown owner project '" +
            ownerProjectFile.generic_string() + "'";
        return false;
    }
    projects_ = std::move(expanded);
    return true;
}

bool ProjectGraph::replaceLibraryReferencesWithSourceProjects(
    std::span<const SourceLibraryReferenceReplacement> replacements,
    std::string& error)
{
    error.clear();
    if (replacements.empty()) return true;

    std::unordered_map<std::string, std::size_t> projectByPath;
    projectByPath.reserve(projects_.size());
    for (std::size_t ordinal = 0; ordinal < projects_.size(); ++ordinal)
        projectByPath.emplace(foldedPath(projects_[ordinal].projectFile()), ordinal);

    std::unordered_map<std::string, std::vector<const SourceLibraryReferenceReplacement*>> byOwner;
    for (const SourceLibraryReferenceReplacement& replacement : replacements)
    {
        if (!projectByPath.contains(foldedPath(replacement.ownerProjectFile)))
        {
            error = "cannot replace LibraryReference for unknown owner project '" +
                replacement.ownerProjectFile.generic_string() + "'";
            return false;
        }
        if (!projectByPath.contains(foldedPath(replacement.sourceProjectFile)))
        {
            error = "cannot replace LibraryReference with unknown source project '" +
                replacement.sourceProjectFile.generic_string() + "'";
            return false;
        }
        byOwner[foldedPath(replacement.ownerProjectFile)].push_back(&replacement);
    }

    std::vector<Project> rewired;
    rewired.reserve(projects_.size());
    for (const Project& project : projects_)
    {
        std::vector<std::filesystem::path> projectReferences(
            project.projectReferences().begin(), project.projectReferences().end());
        std::vector<LibraryReference> libraries;
        libraries.reserve(project.libraryReferences().size());
        const auto ownerReplacements = byOwner.find(foldedPath(project.projectFile()));
        for (const LibraryReference& reference : project.libraryReferences())
        {
            bool replaced = false;
            if (ownerReplacements != byOwner.end())
            {
                const std::string metadataKey = foldedPath(reference.metadataPath);
                for (const SourceLibraryReferenceReplacement* replacement : ownerReplacements->second)
                {
                    if (replacement->metadataPath.empty() ||
                        foldedPath(replacement->metadataPath) != metadataKey)
                        continue;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) libraries.push_back(reference);
        }
        if (ownerReplacements != byOwner.end())
        {
            for (const SourceLibraryReferenceReplacement* replacement : ownerReplacements->second)
            {
                const std::string sourceKey = foldedPath(replacement->sourceProjectFile);
                const bool alreadyReferenced = std::ranges::any_of(
                    projectReferences, [&](const std::filesystem::path& path) {
                        return foldedPath(path) == sourceKey;
                    });
                if (!alreadyReferenced) projectReferences.push_back(replacement->sourceProjectFile);
            }
        }
        std::ranges::sort(projectReferences, {}, [](const std::filesystem::path& path) {
            return foldedPath(path);
        });
        rewired.emplace_back(
            project.projectFile(), project.moduleName(), project.outputKind(), project.sourceSet(),
            std::move(projectReferences),
            std::move(libraries),
            std::vector<SdkManifest>(project.sdkReferences().begin(), project.sdkReferences().end()),
            project.panicProfile(),
            std::string(project.panicHandler()), std::string(project.packageName()),
            std::vector<std::string>(
                project.friendModules().begin(), project.friendModules().end()));
    }

    std::vector<std::uint8_t> state(rewired.size(), 0U);
    std::vector<Project> ordered;
    ordered.reserve(rewired.size());
    std::function<bool(std::size_t)> visit = [&](std::size_t ordinal) {
        if (state[ordinal] == 2U) return true;
        if (state[ordinal] == 1U)
        {
            error = "source-preferred project reference introduces a cycle at module '" +
                rewired[ordinal].moduleName() + "'";
            return false;
        }
        state[ordinal] = 1U;
        for (const std::filesystem::path& reference : rewired[ordinal].projectReferences())
        {
            const auto dependency = projectByPath.find(foldedPath(reference));
            if (dependency == projectByPath.end())
            {
                error = "source-preferred project reference names unknown project '" +
                    reference.generic_string() + "'";
                return false;
            }
            if (!visit(dependency->second)) return false;
        }
        state[ordinal] = 2U;
        ordered.push_back(rewired[ordinal]);
        return true;
    };

    const auto root = projectByPath.find(foldedPath(rootProjectFile_));
    if (root == projectByPath.end() || !visit(root->second)) return false;
    if (ordered.size() != rewired.size())
    {
        error = "source-preferred project graph contains unreachable projects";
        return false;
    }
    projects_ = std::move(ordered);
    return true;
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
        moduleOwners.emplace(
            language::canonicalWindowsModuleIdentity({}, project.moduleName()),
            project.projectFile().generic_string());
    }

    std::vector<PrecompiledProjectDescriptor> uniqueDescriptors;
    std::unordered_map<std::string, std::size_t> descriptorByPath;
    for (const PrecompiledProjectDescriptor& descriptor : descriptors)
    {
        const std::string ownerKey = foldedPath(descriptor.ownerProjectFile);
        const auto owner = std::ranges::find_if(projects_, [&](const Project& project) {
            return foldedPath(project.projectFile()) == ownerKey;
        });
        if (owner == projects_.end())
        {
            error = "precompiled metadata '" + descriptor.metadataPath.generic_string() +
                "' names an unknown owner project '" +
                descriptor.ownerProjectFile.generic_string() + "'";
            return false;
        }
        const std::string metadataKey = foldedPath(descriptor.metadataPath);
        const auto existing = descriptorByPath.find(metadataKey);
        if (existing != descriptorByPath.end())
        {
            if (language::canonicalWindowsModuleIdentity(
                    {}, uniqueDescriptors[existing->second].moduleName) !=
                language::canonicalWindowsModuleIdentity({}, descriptor.moduleName))
            {
                error = "precompiled metadata '" + descriptor.metadataPath.generic_string() +
                    "' was assigned conflicting module identities";
                return false;
            }
            continue;
        }
        const std::string moduleKey =
            language::canonicalWindowsModuleIdentity({}, descriptor.moduleName);
        const auto moduleOwner = moduleOwners.find(moduleKey);
        if (moduleOwner != moduleOwners.end())
        {
            error = "duplicate module name '" + descriptor.moduleName + "' in precompiled metadata '" +
                descriptor.metadataPath.generic_string() + "' and '" + moduleOwner->second + "'";
            return false;
        }
        moduleOwners.emplace(moduleKey, descriptor.metadataPath.generic_string());
        descriptorByPath.emplace(metadataKey, uniqueDescriptors.size());
        uniqueDescriptors.push_back(descriptor);
    }
    std::ranges::sort(uniqueDescriptors, [](const auto& left, const auto& right) {
        const std::string leftIdentity =
            language::canonicalWindowsModuleIdentity({}, left.moduleName);
        const std::string rightIdentity =
            language::canonicalWindowsModuleIdentity({}, right.moduleName);
        if (leftIdentity != rightIdentity) return leftIdentity < rightIdentity;
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
            std::vector<SdkManifest>{});
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
            project.panicProfile(),
            std::string(project.panicHandler()), std::string(project.packageName()),
            std::vector<std::string>(
                project.friendModules().begin(), project.friendModules().end()));
    }
    projects_ = std::move(expanded);
    return true;
}
}
