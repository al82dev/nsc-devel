#pragma once

#include <cstddef>

namespace nativesharp::cfg
{
class CfgNode;
}

namespace nativesharp::cfg::internal
{
struct DfsFrame
{
    CfgNode* node;
    std::size_t nextEdge;
};
}
