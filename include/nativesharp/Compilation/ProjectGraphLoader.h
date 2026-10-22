#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

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
    ProjectGraphLoader(
        std::size_t workerCount,
        ProjectLoadFunction projectLoader,
        ProjectLoadOptions options,
        driver::CompilationWorkerPool* workerPool);

    std::optional<ProjectGraph> load(const std::filesystem::path& rootProjectFile, std::string& error) const;

private:
    std::size_t workerCount_;
    ProjectLoadFunction projectLoader_;
    std::shared_ptr<driver::CompilationWorkerPool> ownedWorkerPool_;
    driver::CompilationWorkerPool* workerPool_ = nullptr;
};
}
