#pragma once

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/LLVM/CodeUnitTelemetry.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/ProjectOopObjectBundle.h"

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
class ColdLlvmPreambleTelemetry;
class ProjectOopCodePartition;
class ProjectOopFunctionReachabilityAnalysis;

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
    bool finishCodeUnitBundle(
        ProjectOopObjectBundle& result,
        bool usePartitions,
        bool preserveModuleText,
        LinkTimeOptimizationMode linkTimeOptimization,
        ProjectOopCodePartition& partition,
        const CompilationArtifactCache* cache,
        const std::string& partitionPlanKey,
        std::string& partitionPlanPayload,
        std::vector<std::string>& moduleTexts,
        const std::vector<std::string>& moduleErrors,
        const std::vector<std::uint8_t>& cacheHits,
        const std::vector<std::uint8_t>& cacheMisses,
        std::vector<CodeUnitTelemetry>& codeUnitTelemetry,
        ColdLlvmPreambleTelemetry& preambleTelemetry,
        std::string& error) const;
    bool generateCodeUnits(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::size_t workerCount,
        std::span<const DebugPrefixMap> debugPrefixMaps,
        LinkTimeOptimizationMode linkTimeOptimization,
        const CompilationArtifactCache* cache,
        const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
        std::span<const std::uint8_t> reachableDefinitions,
        std::span<const std::size_t> coordinatorFunctionDeclarations,
        ProjectOopCodePartition& partition,
        const std::string& partitionPlanKey,
        std::string& partitionPlanPayload,
        bool requiresWholeModuleRequiredInlining,
        bool usePartitions,
        ProjectOopObjectBundle& result,
        ColdLlvmPreambleTelemetry& preambleTelemetry,
        std::string& error) const;

    std::function<void(const basic::CompilationEvent&)> progressSink_;
};
}
