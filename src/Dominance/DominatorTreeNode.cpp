#include "nativesharp/Dominance/DominatorTreeNode.h"

namespace nativesharp::dominance
{
DominatorTreeNode::DominatorTreeNode(cfg::CfgNode* cfgNode) : cfgNode_(cfgNode) {}
cfg::CfgNode* DominatorTreeNode::cfgNode() const noexcept { return cfgNode_; }
DominatorTreeNode* DominatorTreeNode::immediateDominator() const noexcept { return immediateDominator_; }
void DominatorTreeNode::setImmediateDominator(DominatorTreeNode* node) noexcept { immediateDominator_ = node; }
void DominatorTreeNode::addChild(DominatorTreeNode* child) { children_.push_back(child); }
const std::vector<DominatorTreeNode*>& DominatorTreeNode::children() const noexcept { return children_; }
std::size_t DominatorTreeNode::reversePostOrderIndex() const noexcept { return reversePostOrderIndex_; }
void DominatorTreeNode::setReversePostOrderIndex(std::size_t index) noexcept { reversePostOrderIndex_ = index; }
std::uint32_t DominatorTreeNode::preorderNumber() const noexcept { return preorderNumber_; }
std::uint32_t DominatorTreeNode::postorderNumber() const noexcept { return postorderNumber_; }
void DominatorTreeNode::setTraversalNumbers(std::uint32_t preorder, std::uint32_t postorder) noexcept
{
    preorderNumber_ = preorder;
    postorderNumber_ = postorder;
}
std::uint32_t DominatorTreeNode::depth() const noexcept { return depth_; }
void DominatorTreeNode::setDepth(std::uint32_t depth) noexcept { depth_ = depth; }
}
