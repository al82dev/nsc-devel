#pragma once

#include "nativesharp/LLVM/ProjectOopLlvmLoweringRequest.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
class ProjectNameLookup;
class ProjectMemberModel;
class ProjectOopMirModel;
}

namespace nativesharp::source
{
class SourceManager;
}

namespace nativesharp::llvm_backend
{
class LlvmModule;

class ProjectOopLlvmLowerer final
{
public:
    std::unique_ptr<LlvmModule> lower(const ProjectOopLlvmLoweringRequest& request);

    // Compatibility entry points.  All of them construct a validated request and enter through
    // the same encapsulated backend boundary above.
    std::unique_ptr<LlvmModule> lower(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectOopMirModel& mirModel);

    std::unique_ptr<LlvmModule> lower(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const DebugPrefixMap> debugPrefixMaps = {});

    std::unique_ptr<LlvmModule> lowerPartition(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel& memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager& sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const std::size_t> functionOrdinals,
        bool ownsModuleGlobals,
        std::string_view moduleName,
        std::span<const DebugPrefixMap> debugPrefixMaps = {});

    const std::string& error() const noexcept;

private:
    std::string error_;
};
}
