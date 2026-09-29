#pragma once

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
bool writeDiagnosticBundle(
    const std::filesystem::path& projectFile,
    const std::filesystem::path& destination,
    ExitCode result,
    std::string_view standardOutput,
    std::string_view standardError,
    const compilation::ProjectLoadOptions& projectLoadOptions,
    std::span<const std::string_view> arguments,
    std::string& error);
}
