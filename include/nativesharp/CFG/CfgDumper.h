#pragma once

#include <iosfwd>

namespace nativesharp::cfg
{
class CfgEdge;
class CfgFunction;
class CfgModule;
class CfgNode;

class CfgDumper final
{
public:
    void dump(const CfgModule* module, std::ostream& output) const;

private:
    void dumpFunction(const CfgFunction* function, std::ostream& output) const;
    void dumpNode(const CfgNode* node, std::ostream& output) const;
    void dumpEdge(const CfgEdge* edge, std::ostream& output) const;
};
}
