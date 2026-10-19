#pragma once

#include "nativesharp/Compilation/Project.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation::detail
{
struct LoadedProject final
{
    std::filesystem::path path;
    std::optional<Project> project;
    std::string error;
};
}
