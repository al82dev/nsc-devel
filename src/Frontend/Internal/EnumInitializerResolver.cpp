#include "EnumInitializerResolver.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include <utility>

namespace nativesharp::frontend::internal
{
EnumInitializerResolver::EnumInitializerResolver(
    const ProjectMemberModel& members,
    const ProjectNameLookup& names) noexcept
    : members_(members), names_(names)
{
}

std::optional<std::size_t> EnumInitializerResolver::resolveConstantMember(
    const ast::AstNode* syntax,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    if (syntax == nullptr || !declaringTypeOrdinal.has_value() ||
        syntax->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        return std::nullopt;
    }

    const std::span<const std::size_t> candidates =
        members_.findMembers(*declaringTypeOrdinal, syntax->text());
    if (candidates.size() != 1) return std::nullopt;
    return candidates.front();
}

bool EnumInitializerResolver::describeConstantMember(
    std::size_t memberOrdinal,
    bool& isEnumConstant,
    std::int64_t& enumConstantValue,
    std::string& enumUnderlyingTypeKey,
    const ast::AstNode*& initializer,
    std::string& declaredTypeKey,
    std::optional<std::size_t>& declaringTypeOrdinal) const
{
    if (memberOrdinal >= members_.members().size()) return false;
    const ClassMemberSymbol& member = members_.members()[memberOrdinal];
    if (!member.isEnumConstant || member.declaringTypeOrdinal >= names_.namedTypes().size())
    {
        return false;
    }

    isEnumConstant = true;
    enumConstantValue = member.enumConstantValue;
    enumUnderlyingTypeKey = std::string(
        ::nativesharp::frontend::enumUnderlyingTypeKey(
            names_.namedTypes()[member.declaringTypeOrdinal].syntax));
    initializer = nullptr;
    declaredTypeKey = member.declaredTypeKey;
    declaringTypeOrdinal = member.declaringTypeOrdinal;
    return true;
}

std::string EnumInitializerResolver::resolveTypeKey(const ast::TypeSyntax* typeSyntax) const
{
    return members_.canonicalTypeKey(typeSyntax, names_);
}
}
