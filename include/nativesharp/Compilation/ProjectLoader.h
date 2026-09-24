#pragma once

#include "nativesharp/Compilation/Project.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
struct ProjectLoadOptions final
{
    // Relative LibraryReference Include, Metadata, and Runtime paths are
    // resolved from this directory instead of the directory that owns the
    // project file. Absolute paths keep their existing meaning.
    std::optional<std::filesystem::path> libraryReferenceRoot;
};

class ProjectLoader final
{
public:
    std::optional<Project> load(const std::filesystem::path& projectFile, std::string& error) const;
    std::optional<Project> load(
        const std::filesystem::path& projectFile,
        const ProjectLoadOptions& options,
        std::string& error) const;
};
}
