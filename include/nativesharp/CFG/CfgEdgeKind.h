#pragma once

#include <string_view>

namespace nativesharp::cfg
{
enum class CfgEdgeKind
{
    Jump,
    BranchTrue,
    BranchFalse
};

std::string_view cfgEdgeKindName(CfgEdgeKind kind) noexcept;
}
