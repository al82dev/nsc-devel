#include "nativesharp/CFG/CfgEdge.h"

namespace nativesharp::cfg
{
CfgEdge::CfgEdge(CfgNode* source, CfgNode* target, CfgEdgeKind kind)
    : source_(source), target_(target), kind_(kind)
{
}

CfgNode* CfgEdge::source() const noexcept { return source_; }
CfgNode* CfgEdge::target() const noexcept { return target_; }
CfgEdgeKind CfgEdge::kind() const noexcept { return kind_; }
}
