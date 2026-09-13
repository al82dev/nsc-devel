#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMemberModel;
class ProjectNameLookup;
class ProjectOopMirModel;
}

namespace nativesharp::source
{
class SourceManager;
}

namespace nativesharp::llvm_backend
{
class ProjectOopObjectBundle final
{
public:
    ProjectOopObjectBundle(const ProjectOopObjectBundle&) = delete;
    ProjectOopObjectBundle& operator=(const ProjectOopObjectBundle&) = delete;
    ~ProjectOopObjectBundle();

    std::span<const std::filesystem::path> objectFiles() const noexcept;
    const std::string& combinedLlvmText() const noexcept;
    std::size_t codeUnitCount() const noexcept;
    bool usedParallelOptimization() const noexcept;
    std::size_t cacheHitCount() const noexcept;
    std::size_t cacheMissCount() const noexcept;

private:
    friend class ProjectOopObjectGenerator;
    explicit ProjectOopObjectBundle(std::filesystem::path temporaryDirectory) noexcept;

    std::filesystem::path temporaryDirectory_;
    std::vector<std::filesystem::path> objectFiles_;
    std::string combinedLlvmText_;
    std::size_t codeUnitCount_ = 0;
    bool usedParallelOptimization_ = false;
    std::size_t cacheHitCount_ = 0;
    std::size_t cacheMissCount_ = 0;
};

class ProjectOopObjectGenerator final
{
public:
    std::unique_ptr<ProjectOopObjectBundle> generate(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::size_t workerCount,
        std::span<const DebugPrefixMap> debugPrefixMaps,
        std::string& error) const;
};
}
