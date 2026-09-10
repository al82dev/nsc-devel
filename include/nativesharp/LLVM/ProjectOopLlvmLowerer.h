#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <memory>
#include <span>
#include <string>

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

    const std::string& error() const noexcept;

private:
    std::string error_;
};
}
