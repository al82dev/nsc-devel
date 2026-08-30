#pragma once

#include <vector>

namespace nativesharp::mir
{
class MirBlock;
}

namespace nativesharp::cfg
{
class CfgEdge;

class CfgNode final
{
public:
    explicit CfgNode(const mir::MirBlock* block);

    const mir::MirBlock* block() const noexcept;

    void addIncoming(CfgEdge* edge);
    void addOutgoing(CfgEdge* edge);
    const std::vector<CfgEdge*>& incomingEdges() const noexcept;
    const std::vector<CfgEdge*>& outgoingEdges() const noexcept;

    std::vector<CfgNode*> predecessors() const;
    std::vector<CfgNode*> successors() const;

    bool reachable() const noexcept;
    void setReachable(bool value) noexcept;

private:
    const mir::MirBlock* block_;
    std::vector<CfgEdge*> incomingEdges_;
    std::vector<CfgEdge*> outgoingEdges_;
    bool reachable_ = false;
};
}
