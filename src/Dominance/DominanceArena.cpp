#include "nativesharp/Dominance/DominanceArena.h"

#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"

namespace nativesharp::dominance
{
DominanceArena::~DominanceArena()
{
    for (DominanceFrontier* value : frontiers_) { delete value; }
    for (DominatorTreeNode* value : treeNodes_) { delete value; }
    for (DominatorTree* value : trees_) { delete value; }
    for (DominanceFunction* value : functions_) { delete value; }
    for (DominanceModule* value : modules_) { delete value; }
}

DominanceModule* DominanceArena::makeModule(const cfg::CfgModule* cfgModule)
{
    DominanceModule* value = new DominanceModule(cfgModule);
    modules_.push_back(value);
    return value;
}

DominanceFunction* DominanceArena::makeFunction(const cfg::CfgFunction* cfgFunction)
{
    DominanceFunction* value = new DominanceFunction(cfgFunction);
    functions_.push_back(value);
    return value;
}

DominatorTree* DominanceArena::makeTree(const cfg::CfgFunction* cfgFunction)
{
    DominatorTree* value = new DominatorTree(cfgFunction);
    trees_.push_back(value);
    return value;
}

DominatorTreeNode* DominanceArena::makeTreeNode(cfg::CfgNode* cfgNode)
{
    DominatorTreeNode* value = new DominatorTreeNode(cfgNode);
    treeNodes_.push_back(value);
    return value;
}

DominanceFrontier* DominanceArena::makeFrontier(const DominatorTree* tree)
{
    DominanceFrontier* value = new DominanceFrontier(tree);
    frontiers_.push_back(value);
    return value;
}
}
