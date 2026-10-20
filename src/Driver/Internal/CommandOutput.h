#pragma once

#include "nativesharp/Basic/ExitCode.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
struct CommandOutput final
{
    bool json = false;
    std::string command;
    std::filesystem::path project;

    ExitCode success(std::ostream &output, std::string_view message, std::string_view extra = {}) const;
    ExitCode failure(std::ostream &error, std::string_view message, ExitCode code = ExitCode::CommandLineError) const;
};
} // namespace nativesharp::driver
