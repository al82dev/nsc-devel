#pragma once

#include "nativesharp/LLVM/CodeUnitTelemetry.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
class ProjectOopObjectGenerator;

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
    std::size_t thinLtoSummaryCount() const noexcept;
    const std::string& thinLtoIndexDigest() const noexcept;
    std::span<const CodeUnitTelemetry> codeUnitTelemetry() const noexcept;

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
    std::size_t thinLtoSummaryCount_ = 0;
    std::string thinLtoIndexDigest_;
    std::vector<CodeUnitTelemetry> codeUnitTelemetry_;
};
}
