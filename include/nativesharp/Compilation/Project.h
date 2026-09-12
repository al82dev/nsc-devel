#pragma once

#include "nativesharp/Compilation/ProjectSourceSet.h"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
enum class ProjectOutputKind
{
    Executable,
    Library,
    SharedLibrary
};

// NativeSharp 1.0.1: a precompiled library is a three-part contract. `libraryPath` is either a
// static COFF archive or the import library of `runtimePath`; metadata is mandatory because a
// bare .lib cannot describe NativeSharp types, ownership, defaults, or ABI identity.
struct LibraryReference final
{
    std::filesystem::path libraryPath;
    std::filesystem::path metadataPath;
    std::filesystem::path runtimePath;
};

class Project final
{
public:
    Project(
        std::filesystem::path projectFile,
        std::string moduleName,
        ProjectOutputKind outputKind,
        ProjectSourceSet sourceSet,
        std::vector<std::filesystem::path> projectReferences,
        std::vector<LibraryReference> libraryReferences);

    const std::filesystem::path& projectFile() const noexcept;
    const std::string& moduleName() const noexcept;
    ProjectOutputKind outputKind() const noexcept;
    const ProjectSourceSet& sourceSet() const noexcept;
    std::span<const std::filesystem::path> projectReferences() const noexcept;
    std::span<const LibraryReference> libraryReferences() const noexcept;

private:
    std::filesystem::path projectFile_;
    std::string moduleName_;
    ProjectOutputKind outputKind_;
    ProjectSourceSet sourceSet_;
    std::vector<std::filesystem::path> projectReferences_;
    std::vector<LibraryReference> libraryReferences_;
};
}
