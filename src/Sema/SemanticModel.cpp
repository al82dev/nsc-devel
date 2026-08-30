#include "nativesharp/Sema/SemanticModel.h"

namespace nativesharp::sema
{
void SemanticModel::set(const ast::AstNode* node, ExpressionInfo info)
{
    expressions_[node] = info;
}

const ExpressionInfo* SemanticModel::get(const ast::AstNode* node) const noexcept
{
    auto iterator = expressions_.find(node);
    return iterator == expressions_.end() ? nullptr : &iterator->second;
}

void SemanticModel::addConversion(const ast::AstNode* owner, ConversionInfo conversion)
{
    conversions_[owner].push_back(conversion);
}

std::span<const ConversionInfo> SemanticModel::conversions(const ast::AstNode* owner) const noexcept
{
    auto iterator = conversions_.find(owner);
    if (iterator == conversions_.end())
    {
        return {};
    }
    return iterator->second;
}

void SemanticModel::setOperator(const ast::AstNode* node, ResolvedOperatorInfo info)
{
    operators_[node] = info;
}

const ResolvedOperatorInfo* SemanticModel::getOperator(const ast::AstNode* node) const noexcept
{
    auto iterator = operators_.find(node);
    return iterator == operators_.end() ? nullptr : &iterator->second;
}

void SemanticModel::bindSymbol(const ast::AstNode* node, Symbol* symbol)
{
    if (node != nullptr && symbol != nullptr)
    {
        symbols_[node] = symbol;
    }
}

Symbol* SemanticModel::symbol(const ast::AstNode* node) const noexcept
{
    auto iterator = symbols_.find(node);
    return iterator == symbols_.end() ? nullptr : iterator->second;
}
}
