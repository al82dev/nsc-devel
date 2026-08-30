#pragma once

#include <unordered_map>
#include <vector>

namespace nativesharp::cfg
{
class CfgFunction;
class CfgNode;
}

namespace nativesharp::dominance
{
class DominatorTreeNode;

class DominatorTree final
{
public:
    explicit DominatorTree(const cfg::CfgFunction* cfgFunction);

    const cfg::CfgFunction* cfgFunction() const noexcept;

    void addNode(DominatorTreeNode* node);
    const std::vector<DominatorTreeNode*>& nodes() const noexcept;
    DominatorTreeNode* findNode(const cfg::CfgNode* cfgNode) const noexcept;

    DominatorTreeNode* root() const noexcept;
    void setRoot(DominatorTreeNode* root) noexcept;

    bool dominates(const cfg::CfgNode* dominator, const cfg::CfgNode* node) const noexcept;
    bool strictlyDominates(const cfg::CfgNode* dominator, const cfg::CfgNode* node) const noexcept;
    const cfg::CfgNode* immediateDominator(const cfg::CfgNode* node) const noexcept;
    std::vector<const cfg::CfgNode*> dominators(const cfg::CfgNode* node) const;
    const cfg::CfgNode* nearestCommonDominator(
        const cfg::CfgNode* first,
        const cfg::CfgNode* second) const noexcept;

private:
    const cfg::CfgFunction* cfgFunction_;
    std::vector<DominatorTreeNode*> nodes_;
    std::unordered_map<const cfg::CfgNode*, DominatorTreeNode*> nodesByCfg_;
    DominatorTreeNode* root_ = nullptr;
};
}
