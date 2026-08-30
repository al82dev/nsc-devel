#pragma once

#include <iosfwd>

namespace nativesharp::hir
{
class HirNode;

class HirDumper final
{
public:
    void dump(const HirNode* node, std::ostream& output) const;

private:
    void dumpNode(const HirNode* node, std::ostream& output, int depth, const char* edge) const;
};
}
