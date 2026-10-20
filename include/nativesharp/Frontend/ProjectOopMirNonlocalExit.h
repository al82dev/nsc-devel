#pragma once

#include "nativesharp/Frontend/ProjectOopMirNonlocalExitKind.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
struct ProjectOopMirNonlocalExit final
{
    ProjectOopMirNonlocalExitKind kind = ProjectOopMirNonlocalExitKind::Return;
    const ProjectOopMirNode* node = nullptr;
    std::vector<std::size_t> cleanupRegionOrdinals;
    std::optional<std::size_t> targetScopeOrdinal;
    std::optional<std::size_t> exceptionTargetRegionOrdinal;
    std::optional<MethodCleanupContinuationKind> cleanupContinuationKind;
};
}
