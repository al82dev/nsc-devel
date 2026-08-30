#pragma once

#include "nativesharp/CFG/CfgEdgeKind.h"

namespace nativesharp::cfg
{
class CfgNode;

class CfgEdge final
{
public:
    CfgEdge(CfgNode* source, CfgNode* target, CfgEdgeKind kind);

    CfgNode* source() const noexcept;
    CfgNode* target() const noexcept;
    CfgEdgeKind kind() const noexcept;

private:
    CfgNode* source_;
    CfgNode* target_;
    CfgEdgeKind kind_;
};
}
