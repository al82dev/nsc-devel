#include "nativesharp/Dominance/DominatorTree.h"

#include "nativesharp/Dominance/DominatorTreeNode.h"

#include <algorithm>

namespace nativesharp::dominance
{
DominatorTree::DominatorTree(const cfg::CfgFunction* cfgFunction) : cfgFunction_(cfgFunction) {}
const cfg::CfgFunction* DominatorTree::cfgFunction() const noexcept { return cfgFunction_; }

void DominatorTree::addNode(DominatorTreeNode* node)
{
    nodes_.push_back(node);
    if (node != nullptr && node->cfgNode() != nullptr)
    {
        nodesByCfg_.emplace(node->cfgNode(), node);
    }
}

const std::vector<DominatorTreeNode*>& DominatorTree::nodes() const noexcept { return nodes_; }

DominatorTreeNode* DominatorTree::findNode(const cfg::CfgNode* cfgNode) const noexcept
{
    const auto found = nodesByCfg_.find(cfgNode);
    return found == nodesByCfg_.end() ? nullptr : found->second;
}

DominatorTreeNode* DominatorTree::root() const noexcept { return root_; }
void DominatorTree::setRoot(DominatorTreeNode* root) noexcept { root_ = root; }

bool DominatorTree::dominates(const cfg::CfgNode* dominator, const cfg::CfgNode* node) const noexcept
{
    const DominatorTreeNode* first = findNode(dominator);
    const DominatorTreeNode* second = findNode(node);
    if (first == nullptr || second == nullptr)
    {
        return false;
    }
    return first->preorderNumber() <= second->preorderNumber() &&
        first->postorderNumber() >= second->postorderNumber();
}

bool DominatorTree::strictlyDominates(const cfg::CfgNode* dominator, const cfg::CfgNode* node) const noexcept
{
    return dominator != node && dominates(dominator, node);
}

const cfg::CfgNode* DominatorTree::immediateDominator(const cfg::CfgNode* node) const noexcept
{
    const DominatorTreeNode* treeNode = findNode(node);
    if (treeNode == nullptr || treeNode->immediateDominator() == nullptr)
    {
        return nullptr;
    }
    return treeNode->immediateDominator()->cfgNode();
}

std::vector<const cfg::CfgNode*> DominatorTree::dominators(const cfg::CfgNode* node) const
{
    std::vector<const cfg::CfgNode*> result;
    const DominatorTreeNode* current = findNode(node);
    while (current != nullptr)
    {
        result.push_back(current->cfgNode());
        current = current->immediateDominator();
    }
    std::reverse(result.begin(), result.end());
    return result;
}

const cfg::CfgNode* DominatorTree::nearestCommonDominator(
    const cfg::CfgNode* first,
    const cfg::CfgNode* second) const noexcept
{
    const DominatorTreeNode* left = findNode(first);
    const DominatorTreeNode* right = findNode(second);
    if (left == nullptr || right == nullptr)
    {
        return nullptr;
    }

    while (left->depth() > right->depth())
    {
        left = left->immediateDominator();
    }
    while (right->depth() > left->depth())
    {
        right = right->immediateDominator();
    }
    while (left != right)
    {
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }
        left = left->immediateDominator();
        right = right->immediateDominator();
    }
    return left == nullptr ? nullptr : left->cfgNode();
}
}
