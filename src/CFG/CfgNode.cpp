#include "nativesharp/CFG/CfgNode.h"

#include "nativesharp/CFG/CfgEdge.h"

#include <algorithm>

namespace nativesharp::cfg
{
namespace
{
void appendUnique(std::vector<CfgNode*>& nodes, CfgNode* node)
{
    if (node != nullptr && std::find(nodes.begin(), nodes.end(), node) == nodes.end())
    {
        nodes.push_back(node);
    }
}
}

CfgNode::CfgNode(const mir::MirBlock* block) : block_(block) {}

const mir::MirBlock* CfgNode::block() const noexcept { return block_; }
void CfgNode::addIncoming(CfgEdge* edge) { incomingEdges_.push_back(edge); }
void CfgNode::addOutgoing(CfgEdge* edge) { outgoingEdges_.push_back(edge); }
const std::vector<CfgEdge*>& CfgNode::incomingEdges() const noexcept { return incomingEdges_; }
const std::vector<CfgEdge*>& CfgNode::outgoingEdges() const noexcept { return outgoingEdges_; }

std::vector<CfgNode*> CfgNode::predecessors() const
{
    std::vector<CfgNode*> result;
    for (CfgEdge* edge : incomingEdges_)
    {
        if (edge != nullptr)
        {
            appendUnique(result, edge->source());
        }
    }
    return result;
}

std::vector<CfgNode*> CfgNode::successors() const
{
    std::vector<CfgNode*> result;
    for (CfgEdge* edge : outgoingEdges_)
    {
        if (edge != nullptr)
        {
            appendUnique(result, edge->target());
        }
    }
    return result;
}

bool CfgNode::reachable() const noexcept { return reachable_; }
void CfgNode::setReachable(bool value) noexcept { reachable_ = value; }
}
