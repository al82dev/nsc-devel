#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

namespace nativesharp::compilation
{
struct ProjectLoadOptions final
{
    // Relative LibraryReference Include, Metadata, and Runtime paths are
    // resolved from this directory instead of the directory that owns the
    // project file. Absolute paths keep their existing meaning.
    std::optional<std::filesystem::path> libraryReferenceRoot;

    // Environment-provided package roots are fallbacks, not project rewrites. A relative
    // LibraryReference that already exists beside its project keeps that local meaning; only a
    // missing project-relative artifact is searched below this directory.
    std::optional<std::filesystem::path> libraryReferenceFallbackRoot;

    // Frontend resource budgets. The driver fills these from explicit CLI options or from the
    // root project's build settings. An absent value selects the language's normative defaults.
    std::optional<std::size_t> genericInstantiationDepth;
    std::optional<std::size_t> genericInstantiationCount;
};
}
