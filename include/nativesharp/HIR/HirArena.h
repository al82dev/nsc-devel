#pragma once

#include "nativesharp/HIR/HirNode.h"

#include <vector>

namespace nativesharp::hir
{
class HirArena final
{
public:
    HirArena() = default;
    HirArena(const HirArena&) = delete;
    HirArena& operator=(const HirArena&) = delete;
    ~HirArena();

    HirNode* make(HirNodeKind kind, SourceRange range);
    std::size_t size() const noexcept;

private:
    std::vector<HirNode*> nodes_;
};
}
