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
ReferenceQualifierSyntaxKind TypeSyntax::referenceQualifier() const noexcept { return referenceQualifier_; }
void TypeSyntax::setReferenceQualifier(ReferenceQualifierSyntaxKind qualifier) noexcept
{
    referenceQualifier_ = qualifier;
}
const std::string& TypeSyntax::lifetimeName() const noexcept { return lifetimeName_; }
void TypeSyntax::setLifetimeName(std::string name) { lifetimeName_ = std::move(name); }
bool TypeSyntax::isDynamicTrait() const noexcept { return isDynamicTrait_; }
void TypeSyntax::setDynamicTrait(bool value) noexcept { isDynamicTrait_ = value; }
bool TypeSyntax::isOpaqueResult() const noexcept { return isOpaqueResult_; }
void TypeSyntax::setOpaqueResult(bool value) noexcept { isOpaqueResult_ = value; }
const std::string& TypeSyntax::opaqueResultTraitName() const noexcept
{
    return opaqueResultTraitName_;
}
void TypeSyntax::setOpaqueResultTraitName(std::string name)
{
    opaqueResultTraitName_ = std::move(name);
}
bool TypeSyntax::isExplicitTraitImplementation() const noexcept
{
    return isExplicitTraitImplementation_;
}
void TypeSyntax::setExplicitTraitImplementation(bool value) noexcept
{
    isExplicitTraitImplementation_ = value;
}
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
bool TypeSyntax::isFunction() const noexcept { return isFunction_; }
void TypeSyntax::setFunction(bool value) noexcept { isFunction_ = value; }
FunctionTypeCarrierSyntaxKind TypeSyntax::functionCarrier() const noexcept
{
    return functionCarrier_;
}
void TypeSyntax::setFunctionCarrier(FunctionTypeCarrierSyntaxKind carrier) noexcept
{
    functionCarrier_ = carrier;
}
FunctionCallingConventionSyntaxKind TypeSyntax::functionCallingConvention() const noexcept
{
    return functionCallingConvention_;
}
void TypeSyntax::setFunctionCallingConvention(
    FunctionCallingConventionSyntaxKind convention) noexcept
{
    functionCallingConvention_ = convention;
}
TypeSyntax* TypeSyntax::functionReturnType() const noexcept { return functionReturnType_; }
void TypeSyntax::setFunctionReturnType(TypeSyntax* type) noexcept { functionReturnType_ = type; }
TypeSyntax* TypeSyntax::functionErrorType() const noexcept { return functionErrorType_; }
void TypeSyntax::setFunctionErrorType(TypeSyntax* type) noexcept { functionErrorType_ = type; }
bool TypeSyntax::functionIsVariadic() const noexcept { return functionIsVariadic_; }
void TypeSyntax::setFunctionVariadic(bool value) noexcept { functionIsVariadic_ = value; }
bool TypeSyntax::functionIsUnsafe() const noexcept { return functionIsUnsafe_; }
void TypeSyntax::setFunctionUnsafe(bool value) noexcept { functionIsUnsafe_ = value; }
bool TypeSyntax::functionIsNoHeap() const noexcept { return functionIsNoHeap_; }
void TypeSyntax::setFunctionNoHeap(bool value) noexcept { functionIsNoHeap_ = value; }
bool TypeSyntax::functionIsAsync() const noexcept { return functionIsAsync_; }
void TypeSyntax::setFunctionAsync(bool value) noexcept { functionIsAsync_ = value; }
bool TypeSyntax::functionIsNullable() const noexcept { return functionIsNullable_; }
void TypeSyntax::setFunctionNullable(bool value) noexcept { functionIsNullable_ = value; }
const std::string& TypeSyntax::functionLifetimeName() const noexcept
{
    return functionLifetimeName_;
}
void TypeSyntax::setFunctionLifetimeName(std::string name)
{
    functionLifetimeName_ = std::move(name);
}
AstNode* TypeSyntax::constExpression() const noexcept { return constExpression_; }
void TypeSyntax::setConstExpression(AstNode* expression) noexcept
{
    constExpression_ = expression;
}

bool TypeSyntax::isPackExpansion() const noexcept { return isPackExpansion_; }
void TypeSyntax::setPackExpansion(bool value) noexcept { isPackExpansion_ = value; }
void TypeSyntax::addModifier(TypeModifierSyntax modifier) { modifiers_.push_back(modifier); }
void TypeSyntax::addArrayModifierGroup(const std::vector<TypeModifierSyntax>& modifiers)
{
    auto insertion = modifiers_.end();
    while (insertion != modifiers_.begin() &&
           (insertion - 1)->kind == TypeModifierSyntaxKind::Array)
    {
        --insertion;
    }
    modifiers_.insert(insertion, modifiers.begin(), modifiers.end());
}
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
