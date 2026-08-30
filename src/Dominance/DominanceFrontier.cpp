#include "nativesharp/Dominance/DominanceFrontier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::dominance
{
DominanceFrontier::DominanceFrontier(const DominatorTree* tree) : tree_(tree) {}
const DominatorTree* DominanceFrontier::tree() const noexcept { return tree_; }

void DominanceFrontier::setFrontier(
    const cfg::CfgNode* node,
    std::vector<const cfg::CfgNode*> frontier)
{
    frontiers_[node] = std::move(frontier);
}

const std::vector<const cfg::CfgNode*>& DominanceFrontier::frontier(const cfg::CfgNode* node) const noexcept
{
    static const std::vector<const cfg::CfgNode*> empty;
    const auto found = frontiers_.find(node);
    return found == frontiers_.end() ? empty : found->second;
}

bool DominanceFrontier::contains(
    const cfg::CfgNode* node,
    const cfg::CfgNode* frontierNode) const noexcept
{
    const std::vector<const cfg::CfgNode*>& values = frontier(node);
    return std::find(values.begin(), values.end(), frontierNode) != values.end();
}
}
