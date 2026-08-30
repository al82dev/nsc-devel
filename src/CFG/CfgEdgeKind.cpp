#include "nativesharp/CFG/CfgEdgeKind.h"

namespace nativesharp::cfg
{
std::string_view cfgEdgeKindName(CfgEdgeKind kind) noexcept
{
    switch (kind)
    {
        case CfgEdgeKind::Jump: return "jump";
        case CfgEdgeKind::BranchTrue: return "true";
        case CfgEdgeKind::BranchFalse: return "false";
    }
    return "unknown";
}
}
