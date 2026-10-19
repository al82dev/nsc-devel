#pragma once

#include "nativesharp/Compilation/LibraryReference.h"
#include "nativesharp/Compilation/ProjectOutputKind.h"
#include "nativesharp/Compilation/ProjectSourceSet.h"
#include "nativesharp/Compilation/SdkManifest.h"
#include "nativesharp/Language/ErrorSemantics.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
class Project final
{
public:
    Project(
        std::filesystem::path projectFile,
        std::string moduleName,
        ProjectOutputKind outputKind,
        ProjectSourceSet sourceSet,
        std::vector<std::filesystem::path> projectReferences,
        std::vector<LibraryReference> libraryReferences,
        std::vector<SdkManifest> sdkReferences,
        language::PanicProfile panicProfile = language::PanicProfile::Abort,
        std::string panicHandler = {},
        std::string packageName = {},
        std::vector<std::string> friendModules = {});

    const std::filesystem::path& projectFile() const noexcept;
    const std::string& moduleName() const noexcept;
    ProjectOutputKind outputKind() const noexcept;
    const ProjectSourceSet& sourceSet() const noexcept;
    std::span<const std::filesystem::path> projectReferences() const noexcept;
    std::span<const LibraryReference> libraryReferences() const noexcept;
    std::span<const SdkManifest> sdkReferences() const noexcept;
    language::PanicProfile panicProfile() const noexcept;
    std::string_view panicHandler() const noexcept;
    std::string_view packageName() const noexcept;
    std::span<const std::string> friendModules() const noexcept;

private:
    std::filesystem::path projectFile_;
    std::string moduleName_;
    ProjectOutputKind outputKind_;
    ProjectSourceSet sourceSet_;
    std::vector<std::filesystem::path> projectReferences_;
    std::vector<LibraryReference> libraryReferences_;
    std::vector<SdkManifest> sdkReferences_;
    language::PanicProfile panicProfile_ = language::PanicProfile::Abort;
    std::string panicHandler_;
    std::string packageName_;
    std::vector<std::string> friendModules_;
};
}
