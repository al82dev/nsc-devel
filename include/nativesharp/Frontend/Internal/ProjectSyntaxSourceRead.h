#pragma once

#include "nativesharp/Frontend/ComptimeResult.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::frontend
{
struct ProjectSyntaxSourceRead final
{
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    std::string moduleName;
    std::filesystem::path path;
    std::optional<std::string> buffer;
    std::optional<ComptimeResult> comptime;
    std::string error;
};
}
