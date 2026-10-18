#pragma once

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
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
// NSC305-P02. Per-code-unit timing and cache-lookup outcome. `lookup` includes the restore
// (byte copy) performed by `CompilationArtifactCache::restoreObject`; the remaining phases map
// one-to-one to the object generator steps. Populated for every code unit of a generate() call.
struct CodeUnitTelemetry final
{
    std::string name;
    std::size_t definitionCount = 0;
    std::size_t declarationCount = 0;
    std::size_t staticFieldCount = 0;
    std::uint64_t staticFieldMaskMicros = 0;
    std::uint64_t fingerprintMicros = 0;
    std::uint64_t lookupMicros = 0;
    std::uint64_t loweringMicros = 0;
    std::uint64_t targetMicros = 0;
    std::uint64_t optimizationMicros = 0;
    std::uint64_t emissionMicros = 0;
    std::uint64_t verificationMicros = 0;
    std::uint64_t storeMicros = 0;
    CacheMissReason missReason = CacheMissReason::Hit;
};

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

class ProjectOopObjectGenerator final
{
public:
    void setProgressSink(std::function<void(const basic::CompilationEvent&)> sink);

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

    std::unique_ptr<ProjectOopObjectBundle> generate(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::size_t workerCount,
        std::span<const DebugPrefixMap> debugPrefixMaps,
        LinkTimeOptimizationMode linkTimeOptimization,
        std::string& error) const;

private:
    std::function<void(const basic::CompilationEvent&)> progressSink_;
};
}
