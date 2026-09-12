#pragma once

#include "nativesharp/Basic/ExitCode.h"

#include <filesystem>
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
    std::string& error);
}
