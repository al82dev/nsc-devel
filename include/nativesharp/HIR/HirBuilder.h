#pragma once

#include "nativesharp/HIR/HirArena.h"
#include "nativesharp/Sema/SemanticModel.h"

#include <string>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
class Symbol;
class TypeContext;
}

namespace nativesharp::hir
{
class HirBuilder final
{
public:
    HirBuilder(
        HirArena& arena,
        sema::TypeContext& types,
        const sema::SemanticModel& semanticModel);

    HirNode* build(ast::AstNode* compilationUnit);
    bool succeeded() const noexcept;
    const std::string& error() const noexcept;

private:
    HirNode* lowerGlobal(ast::AstNode* node);
    HirNode* lowerStruct(ast::AstNode* node);
    HirNode* lowerEnum(ast::AstNode* node);
    HirNode* lowerFunction(ast::AstNode* node);
    HirNode* lowerGlobalVariable(ast::AstNode* node);
    HirNode* lowerBlock(ast::AstNode* node);
    HirNode* lowerStatement(ast::AstNode* node);
    HirNode* lowerLocalVariable(ast::AstNode* node);
    HirNode* lowerExpression(ast::AstNode* node, bool loadLValue = true);
    HirNode* lowerAddress(ast::AstNode* node);
    HirNode* lowerConvertedOperand(
        const ast::AstNode* owner,
        ast::AstNode* operand,
        int operandIndex = -1);
    HirNode* wrapConversion(
        SourceRange range,
        HirNode* value,
        const sema::ConversionInfo& conversion);

    const sema::ExpressionInfo* expressionInfo(const ast::AstNode* node);
    sema::Symbol* boundSymbol(const ast::AstNode* node);
    const sema::ResolvedOperatorInfo* operatorInfo(const ast::AstNode* node);
    const sema::ConversionInfo* operandConversion(
        const ast::AstNode* owner,
        const ast::AstNode* operand) const noexcept;
    const sema::ConversionInfo* resultConversion(const ast::AstNode* owner) const noexcept;
    HirNode* fail(std::string message);

    HirArena& arena_;
    sema::TypeContext& types_;
    const sema::SemanticModel& semanticModel_;
    std::string error_;
};
}
