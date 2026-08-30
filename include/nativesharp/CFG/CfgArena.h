#pragma once

#include "nativesharp/CFG/CfgEdgeKind.h"

#include <vector>

namespace nativesharp::mir
{
class MirBlock;
class MirFunction;
class MirModule;
}

namespace nativesharp::cfg
{
class CfgEdge;
class CfgFunction;
class CfgModule;
class CfgNode;

class CfgArena final
{
public:
    CfgArena() = default;
    CfgArena(const CfgArena&) = delete;
    CfgArena& operator=(const CfgArena&) = delete;
    ~CfgArena();

    CfgModule* makeModule(const mir::MirModule* mirModule);
    CfgFunction* makeFunction(const mir::MirFunction* function);
    CfgNode* makeNode(const mir::MirBlock* block);
    CfgEdge* makeEdge(CfgNode* source, CfgNode* target, CfgEdgeKind kind);

private:
    std::vector<CfgModule*> modules_;
    std::vector<CfgFunction*> functions_;
    std::vector<CfgNode*> nodes_;
    std::vector<CfgEdge*> edges_;
};
}
