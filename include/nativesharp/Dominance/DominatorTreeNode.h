#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nativesharp::cfg
{
class CfgNode;
}

namespace nativesharp::dominance
{
class DominatorTreeNode final
{
public:
    explicit DominatorTreeNode(cfg::CfgNode* cfgNode);

    cfg::CfgNode* cfgNode() const noexcept;

    DominatorTreeNode* immediateDominator() const noexcept;
    void setImmediateDominator(DominatorTreeNode* node) noexcept;

    void addChild(DominatorTreeNode* child);
    const std::vector<DominatorTreeNode*>& children() const noexcept;

    std::size_t reversePostOrderIndex() const noexcept;
    void setReversePostOrderIndex(std::size_t index) noexcept;

    std::uint32_t preorderNumber() const noexcept;
    std::uint32_t postorderNumber() const noexcept;
    void setTraversalNumbers(std::uint32_t preorder, std::uint32_t postorder) noexcept;

    std::uint32_t depth() const noexcept;
    void setDepth(std::uint32_t depth) noexcept;

private:
    cfg::CfgNode* cfgNode_;
    DominatorTreeNode* immediateDominator_ = nullptr;
    std::vector<DominatorTreeNode*> children_;
    std::size_t reversePostOrderIndex_ = 0;
    std::uint32_t preorderNumber_ = 0;
    std::uint32_t postorderNumber_ = 0;
    std::uint32_t depth_ = 0;
};
}
