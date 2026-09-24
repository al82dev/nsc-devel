#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
class ProjectGraphLoader final
{
public:
    using ProjectLoadFunction = std::function<std::optional<Project>(
        const std::filesystem::path&, std::string&)>;

    explicit ProjectGraphLoader(
        std::size_t workerCount = 1,
        ProjectLoadFunction projectLoader = {},
        ProjectLoadOptions options = {});

    std::optional<ProjectGraph> load(const std::filesystem::path& rootProjectFile, std::string& error) const;

private:
    std::size_t workerCount_;
    ProjectLoadFunction projectLoader_;
};
}
