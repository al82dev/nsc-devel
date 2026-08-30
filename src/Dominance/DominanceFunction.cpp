#include "nativesharp/Dominance/DominanceFunction.h"

namespace nativesharp::dominance
{
DominanceFunction::DominanceFunction(const cfg::CfgFunction* cfgFunction) : cfgFunction_(cfgFunction) {}
const cfg::CfgFunction* DominanceFunction::cfgFunction() const noexcept { return cfgFunction_; }
DominatorTree* DominanceFunction::dominatorTree() const noexcept { return dominatorTree_; }
DominanceFrontier* DominanceFunction::dominanceFrontier() const noexcept { return dominanceFrontier_; }
void DominanceFunction::setDominatorTree(DominatorTree* tree) noexcept { dominatorTree_ = tree; }
void DominanceFunction::setDominanceFrontier(DominanceFrontier* frontier) noexcept
{
    dominanceFrontier_ = frontier;
}
}
