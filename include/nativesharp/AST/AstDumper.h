#pragma once

#include "nativesharp/AST/AstNode.h"

#include <iosfwd>

namespace nativesharp::ast
{
class AstDumper final
{
public:
    void dump(const AstNode* node, std::ostream& output) const;

private:
    void dumpNode(const AstNode* node, std::ostream& output, int depth, const char* edge) const;
    void dumpType(const TypeSyntax* typeSyntax, std::ostream& output) const;
};
}
