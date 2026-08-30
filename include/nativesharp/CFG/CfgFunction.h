#pragma once

#include <unordered_map>
#include <vector>

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
}

namespace nativesharp::cfg
{
class CfgEdge;
class CfgNode;

class CfgFunction final
{
public:
    explicit CfgFunction(const mir::MirFunction* function);

    const mir::MirFunction* mirFunction() const noexcept;

    void addNode(CfgNode* node);
    void addEdge(CfgEdge* edge);
    const std::vector<CfgNode*>& nodes() const noexcept;
    const std::vector<CfgEdge*>& edges() const noexcept;
    CfgNode* entryNode() const noexcept;
    CfgNode* findNode(const mir::MirBlock* block) const noexcept;

    void setReversePostOrder(std::vector<CfgNode*> nodes);
    const std::vector<CfgNode*>& reversePostOrder() const noexcept;

    void addExitNode(CfgNode* node);
    void addReachableExitNode(CfgNode* node);
    const std::vector<CfgNode*>& exitNodes() const noexcept;
    const std::vector<CfgNode*>& reachableExitNodes() const noexcept;

private:
    const mir::MirFunction* mirFunction_;
    std::vector<CfgNode*> nodes_;
    std::unordered_map<const mir::MirBlock*, CfgNode*> nodesByBlock_;
    std::vector<CfgEdge*> edges_;
    std::vector<CfgNode*> reversePostOrder_;
    std::vector<CfgNode*> exitNodes_;
    std::vector<CfgNode*> reachableExitNodes_;
};
}
