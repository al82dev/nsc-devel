#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::driver
{
struct BuildOptions final
{
    std::optional<std::filesystem::path> project;
    std::optional<std::string> configuration;
    std::optional<std::string> optimization;
    std::optional<std::string> debug;
    std::optional<std::string> linker;
    // `-flto`, `-flto=full`, `-flto=thin` or `-flto=none` on the command line, as none/full/thin.
    // `-flto`, `-flto=full`, `-flto=thin` o `-flto=none` en la linea de comandos, como none/full/thin.
    std::optional<std::string> linkTimeOptimization;
    std::optional<std::filesystem::path> libraryReferenceRoot;
    std::optional<std::size_t> compilerJobs;
    bool noRestore = false;
    bool precompiledOnly = false;
    bool dependencyReport = false;
    bool progress = false;
    std::optional<std::string> progressFormat;
    std::vector<std::string> programArguments;
};
} // namespace nativesharp::driver
