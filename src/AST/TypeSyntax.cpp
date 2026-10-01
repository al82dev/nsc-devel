#include "nativesharp/AST/TypeSyntax.h"

#include <utility>

namespace nativesharp::ast
{
std::string_view ownershipQualifierSyntaxKindName(OwnershipQualifierSyntaxKind kind) noexcept
{
    switch (kind)
    {
        case OwnershipQualifierSyntaxKind::None: return "none";
        case OwnershipQualifierSyntaxKind::Owned: return "owned";
        case OwnershipQualifierSyntaxKind::Borrowed: return "borrowed";
    }

    return "none";
}

TypeSyntax::TypeSyntax(SourceRange range, lexer::TokenKind baseTokenKind, std::string baseName)
    : range_(range),
      baseTokenKind_(baseTokenKind),
      baseName_(std::move(baseName))
{
}

SourceRange TypeSyntax::range() const noexcept { return range_; }
void TypeSyntax::setRange(SourceRange range) noexcept { range_ = range; }
lexer::TokenKind TypeSyntax::baseTokenKind() const noexcept { return baseTokenKind_; }
const std::string& TypeSyntax::baseName() const noexcept { return baseName_; }
void TypeSyntax::setBaseName(std::string baseName) { baseName_ = std::move(baseName); }
OwnershipQualifierSyntaxKind TypeSyntax::ownershipQualifier() const noexcept { return ownershipQualifier_; }
void TypeSyntax::setOwnershipQualifier(OwnershipQualifierSyntaxKind qualifier) noexcept
{
    ownershipQualifier_ = qualifier;
}
bool TypeSyntax::isDynamicTrait() const noexcept { return isDynamicTrait_; }
void TypeSyntax::setDynamicTrait(bool value) noexcept { isDynamicTrait_ = value; }
bool TypeSyntax::isPointeeConst() const noexcept { return isPointeeConst_; }
void TypeSyntax::setPointeeConst(bool value) noexcept { isPointeeConst_ = value; }

void TypeSyntax::setBaseTokenKind(lexer::TokenKind kind) noexcept
{
    baseTokenKind_ = kind;
}

void TypeSyntax::prependModifiers(const std::vector<TypeModifierSyntax>& modifiers)
{
    if (modifiers.empty()) return;
    modifiers_.insert(modifiers_.begin(), modifiers.begin(), modifiers.end());
}
void TypeSyntax::addTypeArgument(TypeSyntax* typeArgument) { typeArguments_.push_back(typeArgument); }
const std::vector<TypeSyntax*>& TypeSyntax::typeArguments() const noexcept { return typeArguments_; }
void TypeSyntax::clearTypeArguments() noexcept { typeArguments_.clear(); }
bool TypeSyntax::isTuple() const noexcept { return isTuple_; }
void TypeSyntax::setTuple(bool value) noexcept { isTuple_ = value; }
void TypeSyntax::addTupleElement(TypeSyntax* type, std::string name)
{
    isTuple_ = true;
    typeArguments_.push_back(type);
    tupleElementNames_.push_back(std::move(name));
}
const std::vector<std::string>& TypeSyntax::tupleElementNames() const noexcept
{
    return tupleElementNames_;
}

bool TypeSyntax::isPackExpansion() const noexcept { return isPackExpansion_; }
void TypeSyntax::setPackExpansion(bool value) noexcept { isPackExpansion_ = value; }
void TypeSyntax::addModifier(TypeModifierSyntax modifier) { modifiers_.push_back(modifier); }
const std::vector<TypeModifierSyntax>& TypeSyntax::modifiers() const noexcept { return modifiers_; }
std::optional<std::size_t> TypeSyntax::originProjectOrdinal() const noexcept { return originProjectOrdinal_; }
void TypeSyntax::setOriginProjectOrdinal(std::size_t projectOrdinal) noexcept
{
    originProjectOrdinal_ = projectOrdinal;
}
std::optional<std::string> TypeSyntax::originNamespace() const noexcept { return originNamespace_; }
void TypeSyntax::setOriginNamespace(std::string namespaceName) noexcept
{
    originNamespace_ = std::move(namespaceName);
}
}
