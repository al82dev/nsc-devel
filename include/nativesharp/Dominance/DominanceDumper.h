#pragma once

#include <cstddef>
#include <iosfwd>

namespace nativesharp::cfg
{
class CfgNode;
}

namespace nativesharp::dominance
{
class DominanceFunction;
class DominanceModule;
class DominatorTreeNode;

class DominanceDumper final
{
public:
    void dump(const DominanceModule* module, std::ostream& output) const;

private:
    void dumpFunction(const DominanceFunction* function, std::ostream& output) const;
    void dumpTree(const DominatorTreeNode* root, std::ostream& output) const;
    static void dumpBlockRef(const cfg::CfgNode* node, std::ostream& output);
    static void dumpIndent(std::size_t depth, std::ostream& output);
};
}
