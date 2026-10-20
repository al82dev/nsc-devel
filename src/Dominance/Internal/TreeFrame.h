#pragma once

#include <cstddef>

namespace nativesharp::dominance
{
class DominatorTreeNode;
}

namespace nativesharp::dominance::internal
{
struct TreeFrame
{
    DominatorTreeNode* node;
    std::size_t nextChild;
    bool entered;
};
}
