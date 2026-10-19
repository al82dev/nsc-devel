#include "EnumConstResolver.h"

#include "nativesharp/Lexer/TokenKind.h"

#include <utility>

namespace nativesharp::frontend::internal
{
EnumConstResolver::EnumConstResolver(std::string enumName, std::string underlyingType)
    : enumName_(std::move(enumName)), underlyingType_(std::move(underlyingType))
{
}

void EnumConstResolver::add(std::string name, std::uint64_t bits)
{
    const std::size_t ordinal = values_.size();
    names_.emplace(std::move(name), ordinal);
    values_.push_back(bits);
}

std::optional<std::size_t> EnumConstResolver::resolveConstantMember(
    const ast::AstNode* syntax,
    std::optional<std::size_t>) const
{
    if (syntax == nullptr) return std::nullopt;
    if (syntax->kind() == ast::AstNodeKind::MemberExpression)
    {
        if (syntax->first() == nullptr ||
            syntax->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
            syntax->first()->text() != enumName_)
            return std::nullopt;
    }
    else if (syntax->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        return std::nullopt;
    }
    const auto found = names_.find(syntax->text());
    return found == names_.end()
        ? std::nullopt : std::optional<std::size_t>(found->second);
}

bool EnumConstResolver::describeConstantMember(
    std::size_t memberOrdinal,
    bool& isEnumConstant,
    std::int64_t& enumConstantValue,
    std::string& enumUnderlyingTypeKey,
    const ast::AstNode*& initializer,
    std::string& declaredTypeKey,
    std::optional<std::size_t>& declaringTypeOrdinal) const
{
    if (memberOrdinal >= values_.size()) return false;
    isEnumConstant = true;
    enumConstantValue = static_cast<std::int64_t>(values_[memberOrdinal]);
    enumUnderlyingTypeKey = underlyingType_;
    initializer = nullptr;
    declaredTypeKey.clear();
    declaringTypeOrdinal.reset();
    return true;
}

std::string EnumConstResolver::resolveTypeKey(const ast::TypeSyntax* syntax) const
{
    if (syntax == nullptr || !lexer::isBuiltinTypeKeyword(syntax->baseTokenKind())) return {};
    return "builtin:" + syntax->baseName();
}
}
