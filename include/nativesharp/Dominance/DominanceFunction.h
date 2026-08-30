#pragma once

namespace nativesharp::cfg
{
class CfgFunction;
}

namespace nativesharp::dominance
{
class DominanceFrontier;
class DominatorTree;

class DominanceFunction final
{
public:
    explicit DominanceFunction(const cfg::CfgFunction* cfgFunction);

    const cfg::CfgFunction* cfgFunction() const noexcept;
    DominatorTree* dominatorTree() const noexcept;
    DominanceFrontier* dominanceFrontier() const noexcept;

    void setDominatorTree(DominatorTree* tree) noexcept;
    void setDominanceFrontier(DominanceFrontier* frontier) noexcept;

private:
    const cfg::CfgFunction* cfgFunction_;
    DominatorTree* dominatorTree_ = nullptr;
    DominanceFrontier* dominanceFrontier_ = nullptr;
};
}
