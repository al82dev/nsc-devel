#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

namespace nativesharp::frontend::internal
{
bool isGenericTypeTemplateNode(const ast::AstNode* node) noexcept;
bool isNamedTypeDeclaration(ast::AstNodeKind kind) noexcept;
bool isStructuralGenericSurface(const ast::TypeSyntax* syntax) noexcept;
GenericTemplateKind genericTypeTemplateKind(const ast::AstNode* node) noexcept;
std::size_t valueParameterCount(const ast::AstNode* node) noexcept;
bool isGenericMethodTemplateNode(const ast::AstNode* node) noexcept;
GenericTemplateAccessibility genericTemplateAccessibility(
    const ast::AstNode* node,
    bool isMethod) noexcept;
}
