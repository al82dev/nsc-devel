#pragma once

#include "nativesharp/Compilation/Project.h"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
struct PrecompiledProjectDescriptor final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::string moduleName;
    language::LanguageEdition languageEdition = language::LanguageEdition::V2;
};

class ProjectGraph final
{
public:
    ProjectGraph(std::filesystem::path rootProjectFile, std::vector<Project> projects);

    const std::filesystem::path& rootProjectFile() const noexcept;
    std::span<const Project> projects() const noexcept;
    const Project* rootProject() const noexcept;

    // Adds metadata-backed modules as real dependency nodes while preserving the root project as
    // the final node. Frontend units generated from .nsmeta therefore receive their own project
    // ordinal and canonical module identity instead of masquerading as declarations of the
    // consuming project.
    bool addPrecompiledProjects(
        std::span<const PrecompiledProjectDescriptor> descriptors,
        std::string& error);

    // Adds metadata-discovered binary closure edges to an existing source project. This mutates
    // only the in-memory graph; no project file, restore state, or source dependency is created.
    bool addLibraryReferences(
        const std::filesystem::path& ownerProjectFile,
        std::span<const LibraryReference> references,
        std::string& error);

private:
    std::filesystem::path rootProjectFile_;
    std::vector<Project> projects_;
};
}
