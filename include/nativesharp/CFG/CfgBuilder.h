#pragma once

#include <string>

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirModule;
}

namespace nativesharp::cfg
{
class CfgArena;
class CfgFunction;
class CfgModule;
class CfgNode;

enum class CfgEdgeKind;

class CfgBuilder final
{
public:
    explicit CfgBuilder(CfgArena& arena);

    CfgModule* build(const mir::MirModule* module);
    bool succeeded() const noexcept;
    const std::string& error() const noexcept;

private:
    bool buildFunction(const mir::MirFunction* function, CfgModule* module);
    bool buildEdges(CfgFunction* function);
    bool addEdge(CfgFunction* function, CfgNode* source, const mir::MirBlock* targetBlock, CfgEdgeKind kind);
    void computeTraversal(CfgFunction* function);
    bool fail(std::string message);

    CfgArena& arena_;
    std::string error_;
};
}
