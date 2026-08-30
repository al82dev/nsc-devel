#pragma once

#include <unordered_map>
#include <vector>

namespace nativesharp::cfg
{
class CfgNode;
}

namespace nativesharp::dominance
{
class DominatorTree;

class DominanceFrontier final
{
public:
    explicit DominanceFrontier(const DominatorTree* tree);

    const DominatorTree* tree() const noexcept;
    void setFrontier(const cfg::CfgNode* node, std::vector<const cfg::CfgNode*> frontier);
    const std::vector<const cfg::CfgNode*>& frontier(const cfg::CfgNode* node) const noexcept;
    bool contains(const cfg::CfgNode* node, const cfg::CfgNode* frontierNode) const noexcept;

private:
    const DominatorTree* tree_;
    std::unordered_map<const cfg::CfgNode*, std::vector<const cfg::CfgNode*>> frontiers_;
};
}
