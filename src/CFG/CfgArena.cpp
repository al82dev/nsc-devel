#include "nativesharp/CFG/CfgArena.h"

#include "nativesharp/CFG/CfgEdge.h"
#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"

namespace nativesharp::cfg
{
CfgArena::~CfgArena()
{
    for (CfgEdge* value : edges_) { delete value; }
    for (CfgNode* value : nodes_) { delete value; }
    for (CfgFunction* value : functions_) { delete value; }
    for (CfgModule* value : modules_) { delete value; }
}

CfgModule* CfgArena::makeModule(const mir::MirModule* mirModule)
{
    CfgModule* value = new CfgModule(mirModule);
    modules_.push_back(value);
    return value;
}

CfgFunction* CfgArena::makeFunction(const mir::MirFunction* function)
{
    CfgFunction* value = new CfgFunction(function);
    functions_.push_back(value);
    return value;
}

CfgNode* CfgArena::makeNode(const mir::MirBlock* block)
{
    CfgNode* value = new CfgNode(block);
    nodes_.push_back(value);
    return value;
}

CfgEdge* CfgArena::makeEdge(CfgNode* source, CfgNode* target, CfgEdgeKind kind)
{
    CfgEdge* value = new CfgEdge(source, target, kind);
    edges_.push_back(value);
    return value;
}
}
