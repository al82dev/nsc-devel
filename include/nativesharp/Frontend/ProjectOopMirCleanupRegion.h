#pragma once

#include "nativesharp/Frontend/ProjectOopMirCleanupKind.h"
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
struct ProjectOopMirCleanupRegion final
{
    std::size_t ordinal = 0;
    ProjectOopMirCleanupKind kind = ProjectOopMirCleanupKind::LocalOwner;
    const ProjectOopMirNode* action = nullptr;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    std::string typeKey;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    bool automaticDestruction = false;
};
}
