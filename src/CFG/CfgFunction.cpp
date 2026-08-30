#include "nativesharp/CFG/CfgFunction.h"

#include "nativesharp/CFG/CfgNode.h"

#include "nativesharp/MIR/MirBlock.h"

#include <utility>

namespace nativesharp::cfg
{
CfgFunction::CfgFunction(const mir::MirFunction* function) : mirFunction_(function) {}

const mir::MirFunction* CfgFunction::mirFunction() const noexcept { return mirFunction_; }
void CfgFunction::addNode(CfgNode* node)
{
    nodes_.push_back(node);
    if (node != nullptr && node->block() != nullptr)
    {
        nodesByBlock_.emplace(node->block(), node);
    }
}
void CfgFunction::addEdge(CfgEdge* edge) { edges_.push_back(edge); }
const std::vector<CfgNode*>& CfgFunction::nodes() const noexcept { return nodes_; }
const std::vector<CfgEdge*>& CfgFunction::edges() const noexcept { return edges_; }
CfgNode* CfgFunction::entryNode() const noexcept { return nodes_.empty() ? nullptr : nodes_.front(); }

CfgNode* CfgFunction::findNode(const mir::MirBlock* block) const noexcept
{
    const auto found = nodesByBlock_.find(block);
    return found == nodesByBlock_.end() ? nullptr : found->second;
}

void CfgFunction::setReversePostOrder(std::vector<CfgNode*> nodes)
{
    reversePostOrder_ = std::move(nodes);
}

const std::vector<CfgNode*>& CfgFunction::reversePostOrder() const noexcept
{
    return reversePostOrder_;
}

void CfgFunction::addExitNode(CfgNode* node) { exitNodes_.push_back(node); }
void CfgFunction::addReachableExitNode(CfgNode* node) { reachableExitNodes_.push_back(node); }
const std::vector<CfgNode*>& CfgFunction::exitNodes() const noexcept { return exitNodes_; }
const std::vector<CfgNode*>& CfgFunction::reachableExitNodes() const noexcept
{
    return reachableExitNodes_;
}
}
