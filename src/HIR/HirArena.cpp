#include "nativesharp/HIR/HirArena.h"

namespace nativesharp::hir
{
HirArena::~HirArena()
{
    for (HirNode* node : nodes_)
    {
        delete node;
    }
}

HirNode* HirArena::make(HirNodeKind kind, SourceRange range)
{
    HirNode* node = new HirNode(kind, range);
    nodes_.push_back(node);
    return node;
}

std::size_t HirArena::size() const noexcept
{
    return nodes_.size();
}
}
