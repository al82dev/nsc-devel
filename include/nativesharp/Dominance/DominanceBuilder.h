#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace nativesharp::cfg
{
class CfgFunction;
class CfgModule;
class CfgNode;
}

namespace nativesharp::dominance
{
class DominanceArena;
class DominanceFunction;
class DominanceModule;
class DominatorTree;

class DominanceBuilder final
{
public:
    explicit DominanceBuilder(DominanceArena& arena);

    DominanceModule* build(const cfg::CfgModule* module);
    bool succeeded() const noexcept;
    const std::string& error() const noexcept;

private:
    bool buildFunction(const cfg::CfgFunction* cfgFunction, DominanceModule* module);
    bool computeImmediateDominators(
        const cfg::CfgFunction* cfgFunction,
        std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>& idoms);
    const cfg::CfgNode* intersect(
        const cfg::CfgNode* first,
        const cfg::CfgNode* second,
        const std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>& idoms,
        const std::unordered_map<const cfg::CfgNode*, std::size_t>& rpoIndices) const;
    bool materializeTree(
        const cfg::CfgFunction* cfgFunction,
        DominatorTree* tree,
        const std::unordered_map<const cfg::CfgNode*, const cfg::CfgNode*>& idoms);
    void numberTree(DominatorTree* tree);
    bool buildFrontier(DominanceFunction* function);
    bool fail(std::string message);

    DominanceArena& arena_;
    std::string error_;
};
}
