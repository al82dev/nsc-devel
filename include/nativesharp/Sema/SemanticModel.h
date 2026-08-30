#pragma once

#include "nativesharp/Sema/ConversionInfo.h"
#include "nativesharp/Sema/ExpressionInfo.h"
#include "nativesharp/Sema/ResolvedOperatorInfo.h"

#include <span>
#include <unordered_map>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
class Symbol;

class SemanticModel final
{
public:
    void set(const ast::AstNode* node, ExpressionInfo info);
    const ExpressionInfo* get(const ast::AstNode* node) const noexcept;

    void addConversion(const ast::AstNode* owner, ConversionInfo conversion);
    std::span<const ConversionInfo> conversions(const ast::AstNode* owner) const noexcept;

    void setOperator(const ast::AstNode* node, ResolvedOperatorInfo info);
    const ResolvedOperatorInfo* getOperator(const ast::AstNode* node) const noexcept;

    void bindSymbol(const ast::AstNode* node, Symbol* symbol);
    Symbol* symbol(const ast::AstNode* node) const noexcept;

private:
    std::unordered_map<const ast::AstNode*, ExpressionInfo> expressions_;
    std::unordered_map<const ast::AstNode*, std::vector<ConversionInfo>> conversions_;
    std::unordered_map<const ast::AstNode*, ResolvedOperatorInfo> operators_;
    std::unordered_map<const ast::AstNode*, Symbol*> symbols_;
};
}
