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

    // EN: Internal switch for the precompiled semantic snapshots of `.nsproducts` v2. On by default
    //     and without a command-line option. Off makes every precompiled library take the analysis
    //     path, exactly as a library without a snapshot; tests use it to compare both paths.
    // ES: Interruptor interno de los snapshots semánticos precompilados del `.nsproducts` v2.
    //     Encendido por defecto y sin opción de línea de comandos. Apagado hace que toda biblioteca
    //     precompilada vaya por la ruta de análisis, igual que una sin snapshot; las pruebas lo usan
    //     para comparar ambas rutas.
    bool usePrecompiledSnapshots = true;
};
}
