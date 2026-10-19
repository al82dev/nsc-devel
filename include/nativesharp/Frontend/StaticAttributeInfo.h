#pragma once

#include "nativesharp/Frontend/StaticMemberKind.h"
#include "nativesharp/Frontend/StaticMemberInfo.h"
#include "nativesharp/Frontend/StaticTypeInfo.h"

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

struct StaticAttributeInfo final
{
    std::string identity;
    std::vector<std::string> constantArguments;
    ComptimeAttributeRetention retention = ComptimeAttributeRetention::CompileTime;
    std::optional<std::size_t> typeOrdinal;
    std::optional<std::size_t> memberOrdinal;
};
}
