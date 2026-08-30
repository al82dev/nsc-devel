#pragma once

#include <string>
#include <unordered_set>

namespace nativesharp::mir
{
class MirFunction;
}

namespace nativesharp::cfg
{
class CfgEdge;
class CfgFunction;
class CfgModule;
class CfgNode;

class CfgVerifier final
{
public:
    bool verify(const CfgModule* module);
    const std::string& error() const noexcept;

private:
    bool verifyFunction(const CfgFunction* function);
    bool verifyNode(const CfgNode* node, const CfgFunction* function);
    bool verifyEdge(const CfgEdge* edge, const CfgFunction* function);
    bool verifyTerminatorEdges(const CfgNode* node);
    bool verifyTraversal(const CfgFunction* function);
    bool verifyExitSets(const CfgFunction* function);
    bool fail(std::string message);

    std::unordered_set<const mir::MirFunction*> seenFunctions_;
    std::unordered_set<const CfgNode*> nodes_;
    std::unordered_set<const CfgEdge*> edges_;
    std::string error_;
};
}
