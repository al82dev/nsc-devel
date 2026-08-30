#pragma once

#include <vector>

namespace nativesharp::cfg
{
class CfgFunction;
class CfgModule;
class CfgNode;
}

namespace nativesharp::dominance
{
class DominanceFrontier;
class DominanceFunction;
class DominanceModule;
class DominatorTree;
class DominatorTreeNode;

class DominanceArena final
{
public:
    DominanceArena() = default;
    DominanceArena(const DominanceArena&) = delete;
    DominanceArena& operator=(const DominanceArena&) = delete;
    ~DominanceArena();

    DominanceModule* makeModule(const cfg::CfgModule* cfgModule);
    DominanceFunction* makeFunction(const cfg::CfgFunction* cfgFunction);
    DominatorTree* makeTree(const cfg::CfgFunction* cfgFunction);
    DominatorTreeNode* makeTreeNode(cfg::CfgNode* cfgNode);
    DominanceFrontier* makeFrontier(const DominatorTree* tree);

private:
    std::vector<DominanceModule*> modules_;
    std::vector<DominanceFunction*> functions_;
    std::vector<DominatorTree*> trees_;
    std::vector<DominatorTreeNode*> treeNodes_;
    std::vector<DominanceFrontier*> frontiers_;
};
}
