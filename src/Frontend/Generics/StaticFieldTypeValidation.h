#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend::internal
{
// Validates the supported type projection before cloning. Const-expression
// carriers are rejected until they can be rewritten without hidden AST edges.
inline bool validateStaticFieldTemplateTypes(
    const ast::AstNode* body, std::string_view descriptor, std::size_t& nodeCount,
    bool& projectsFieldType, std::string& error)
{
    projectsFieldType = false;
    constexpr std::size_t maximumItems = 100000;
    std::vector<const ast::AstNode*> nodes{body};
    std::vector<const ast::TypeSyntax*> types;
    std::unordered_set<const ast::AstNode*> visitedNodes;
    std::unordered_set<const ast::TypeSyntax*> visitedTypes;
    while (!nodes.empty())
    {
        const ast::AstNode* node = nodes.back();
        nodes.pop_back();
        if (node == nullptr || !visitedNodes.insert(node).second) continue;
        bool supportedGenericLambda = node->kind() == ast::AstNodeKind::LambdaExpression &&
            node->genericParameterSyntaxes().size() == node->genericParameters().size();
        for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
            if (parameter.kind != ast::GenericParameterSyntaxKind::Type || parameter.constantType != nullptr)
                supportedGenericLambda = false;
        if (visitedNodes.size() > maximumItems ||
            (!node->genericParameters().empty() && !supportedGenericLambda) ||
            !node->genericConstraints().empty() || node->throwsTypeSyntax() != nullptr)
        {
            error = "static field body has unsupported generic or type syntax";
            return false;
        }
        if (node->typeSyntax() != nullptr) types.push_back(node->typeSyntax());
        for (const ast::TypeSyntax* type : node->typeSyntaxes()) types.push_back(type);
        const ast::AstNode* hiddenBody = node->kind() == ast::AstNodeKind::StaticFieldIterationStatement &&
            node->text() == descriptor ? node->first() : nullptr;
        if (node->first() != hiddenBody) nodes.push_back(node->first());
        if (node->second() != hiddenBody) nodes.push_back(node->second());
        if (node->third() != hiddenBody) nodes.push_back(node->third());
        for (const ast::AstNode* child : node->children()) if (child != hiddenBody) nodes.push_back(child);
    }
    const std::string prefix = std::string(descriptor) + ".";
    while (!types.empty())
    {
        const ast::TypeSyntax* type = types.back();
        types.pop_back();
        if (type == nullptr || !visitedTypes.insert(type).second) continue;
        if (type->baseName() == prefix + "Type") projectsFieldType = true;
        if (visitedTypes.size() > maximumItems || type->constExpression() != nullptr ||
            type->baseName() == descriptor ||
            (type->baseName().starts_with(prefix) && type->baseName() != prefix + "Type"))
        {
            error = "static field body has unsupported descriptor type syntax";
            return false;
        }
        for (const ast::TypeModifierSyntax& modifier : type->modifiers())
            if (modifier.arrayCount != nullptr)
            {
                error = "static field body has an unsupported array count expression";
                return false;
            }
        for (const ast::TypeSyntax* argument : type->typeArguments()) types.push_back(argument);
        if (type->functionReturnType() != nullptr) types.push_back(type->functionReturnType());
        if (type->functionErrorType() != nullptr) types.push_back(type->functionErrorType());
    }
    nodeCount = visitedNodes.size();
    return true;
}
}
