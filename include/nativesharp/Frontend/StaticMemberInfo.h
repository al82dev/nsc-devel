#pragma once

#include "nativesharp/Frontend/StaticMemberKind.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ComptimeEngine.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMethodBodyModel;

struct StaticMemberInfo final
{
    std::size_t memberOrdinal = 0;
    StaticMemberKind kind = StaticMemberKind::Field;
    std::string name;
    std::string typeKey;
    std::string signatureKey;
    bool isPublic = false;
    bool isStatic = false;
    // NS304-T14-F01: typed descriptors (modifiers / ownership / generics / source origin).
    bool isReadonly = false;
    bool isConst = false;
    bool isVolatile = false;
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    std::string genericTemplateKey;
    std::string sourcePath;
    language::FrameStorage frameStorage = language::FrameStorage::CallerStack;
};
}
