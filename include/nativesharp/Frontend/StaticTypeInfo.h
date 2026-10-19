#pragma once

#include "nativesharp/Frontend/StaticMemberKind.h"
#include "nativesharp/Frontend/StaticMemberInfo.h"

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

struct StaticTypeInfo final
{
    std::size_t typeOrdinal = 0;
    std::size_t ownerProjectOrdinal = 0;
    std::string identity;
    std::string moduleName;
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    bool isPublic = false;
    bool isOpenGeneric = false;
    bool isPrecompiled = false;
    std::optional<std::uint64_t> size;
    std::optional<std::uint64_t> alignment;
    std::vector<StaticMemberInfo> members;
};
}
