#pragma once

#include "nativesharp/Compilation/Project.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
class ProjectLoader final
{
public:
    std::optional<Project> load(const std::filesystem::path& projectFile, std::string& error) const;
};
}
