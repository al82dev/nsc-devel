#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
class ProjectGraphLoader final
{
public:
    std::optional<ProjectGraph> load(const std::filesystem::path& rootProjectFile, std::string& error) const;
};
}
