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
    void dumpNodeHeader(const AstNode* node, std::ostream& output, int depth, const char* edge) const;
    void dumpGenericConstraints(const AstNode* node, std::ostream& output) const;
    void dumpNodeProperties(const AstNode* node, std::ostream& output) const;
    void dumpNodeChildren(const AstNode* node, std::ostream& output, int depth) const;
    void dumpType(const TypeSyntax* typeSyntax, std::ostream& output) const;
    void dumpTypePrefix(const TypeSyntax* typeSyntax, std::ostream& output) const;
    void dumpFunctionType(const TypeSyntax* typeSyntax, std::ostream& output) const;
    void dumpTupleType(const TypeSyntax* typeSyntax, std::ostream& output) const;
    void dumpTypeArguments(const TypeSyntax* typeSyntax, std::ostream& output) const;
    void dumpTypeModifiers(const TypeSyntax* typeSyntax, std::ostream& output) const;
};
}
