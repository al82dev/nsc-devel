#include "nativesharp/Compilation/Project.h"

#include <utility>

namespace nativesharp::compilation
{
Project::Project(
    std::filesystem::path projectFile,
    std::string moduleName,
    ProjectOutputKind outputKind,
    ProjectSourceSet sourceSet,
    std::vector<std::filesystem::path> projectReferences,
    std::vector<LibraryReference> libraryReferences,
    std::vector<SdkManifest> sdkReferences,
    language::LanguageEdition languageEdition,
    language::PanicProfile panicProfile,
    std::string panicHandler,
    std::string packageName,
    std::vector<std::string> friendModules,
    bool languageEditionExplicit)
    : projectFile_(std::move(projectFile)),
      moduleName_(std::move(moduleName)),
      outputKind_(outputKind),
      sourceSet_(std::move(sourceSet)),
      projectReferences_(std::move(projectReferences)),
      libraryReferences_(std::move(libraryReferences)),
      sdkReferences_(std::move(sdkReferences)),
      languageEdition_(languageEdition),
      languageEditionExplicit_(languageEditionExplicit),
      panicProfile_(panicProfile),
      panicHandler_(std::move(panicHandler)),
      packageName_(packageName.empty() ? moduleName_ : std::move(packageName)),
      friendModules_(std::move(friendModules))
{
}

const std::filesystem::path& Project::projectFile() const noexcept
{
    return projectFile_;
}

const std::string& Project::moduleName() const noexcept
{
    return moduleName_;
}

ProjectOutputKind Project::outputKind() const noexcept
{
    return outputKind_;
}

const ProjectSourceSet& Project::sourceSet() const noexcept
{
    return sourceSet_;
}

std::span<const std::filesystem::path> Project::projectReferences() const noexcept
{
    return projectReferences_;
}

std::span<const LibraryReference> Project::libraryReferences() const noexcept
{
    return libraryReferences_;
}

std::span<const SdkManifest> Project::sdkReferences() const noexcept
{
    return sdkReferences_;
}

language::LanguageEdition Project::languageEdition() const noexcept
{
    return languageEdition_;
}

language::LanguageEdition Project::sourceLanguageEdition() const noexcept
{
    // Graph reconciliation may raise an omitted edition to satisfy dependencies, but it must
    // not rewrite what the author selected. Omission keeps the historical source contract.
    return languageEditionExplicit_ ? languageEdition_ : language::LanguageEdition::V2;
}

language::LanguageEdition Project::recognizedLanguageEdition() const noexcept
{
    // NativeSharp has one cumulative source surface. The legacy project field remains readable
    // while project-file compatibility is retired, but it never hides syntax from the parser.
    return language::LanguageEdition::V3;
}

bool Project::enforcesLanguage3Effects() const noexcept
{
    // Effects are explicit properties of the operation that uses them. A project-wide edition
    // must never turn pointer, ownership, or exception policy into a mandatory writing style.
    return false;
}

bool Project::hasExplicitLanguageEdition() const noexcept
{
    return languageEditionExplicit_;
}

void Project::adoptInferredLanguageEdition(language::LanguageEdition edition) noexcept
{
    if (!languageEditionExplicit_) languageEdition_ = edition;
}

language::PanicProfile Project::panicProfile() const noexcept
{
    return panicProfile_;
}

std::string_view Project::panicHandler() const noexcept
{
    return panicHandler_;
}

std::string_view Project::packageName() const noexcept
{
    return packageName_;
}

std::span<const std::string> Project::friendModules() const noexcept
{
    return friendModules_;
}
}
