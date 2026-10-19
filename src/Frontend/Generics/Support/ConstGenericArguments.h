#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

namespace nativesharp::frontend::internal
{
bool decodeConstGenericArgument(
    const ast::TypeSyntax* syntax,
    std::string& typeKey,
    std::string& value) noexcept;
ast::AstNode* constArgumentExpression(
    ast::AstArena& arena,
    std::string_view typeKey,
    std::string_view value,
    SourceRange range);
bool normalizeConstExpressionArgument(
    ast::TypeSyntax* syntax,
    std::string& typeKey,
    std::string& value);
std::string constGenericParameterTypeKey(const ast::GenericParameterSyntax& parameter);
std::optional<std::size_t> trailingPackIndex(const GenericTemplateRecord& record) noexcept;
}
