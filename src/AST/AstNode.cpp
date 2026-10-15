#include "nativesharp/AST/AstNode.h"

#include <algorithm>
#include <utility>

namespace nativesharp::ast
{
AstNode::AstNode(AstNodeKind kind, SourceRange range)
    : kind_(kind),
      range_(range)
{
}

AstNodeKind AstNode::kind() const noexcept { return kind_; }
SourceRange AstNode::range() const noexcept { return range_; }
void AstNode::setRange(SourceRange range) noexcept { range_ = range; }
const std::string& AstNode::text() const noexcept { return text_; }
void AstNode::setText(std::string text) { text_ = std::move(text); }
const std::string& AstNode::auxiliaryText() const noexcept { return auxiliaryText_; }
void AstNode::setAuxiliaryText(std::string text) { auxiliaryText_ = std::move(text); }
const std::string& AstNode::argumentName() const noexcept { return argumentName_; }
void AstNode::setArgumentName(std::string name) { argumentName_ = std::move(name); }
const std::string& AstNode::allocatorFailureProfile() const noexcept { return allocatorFailureProfile_; }
void AstNode::setAllocatorFailureProfile(std::string profile) { allocatorFailureProfile_ = std::move(profile); }
lexer::TokenKind AstNode::tokenKind() const noexcept { return tokenKind_; }
void AstNode::setTokenKind(lexer::TokenKind kind) noexcept { tokenKind_ = kind; }
void AstNode::addModifier(lexer::TokenKind kind) { modifiers_.push_back(kind); }
const std::vector<lexer::TokenKind>& AstNode::modifiers() const noexcept { return modifiers_; }
bool AstNode::hasModifier(lexer::TokenKind kind) const noexcept
{
    return std::find(modifiers_.begin(), modifiers_.end(), kind) != modifiers_.end();
}
TypeSyntax* AstNode::typeSyntax() const noexcept { return typeSyntax_; }
void AstNode::setTypeSyntax(TypeSyntax* typeSyntax) noexcept { typeSyntax_ = typeSyntax; }
TypeSyntax* AstNode::throwsTypeSyntax() const noexcept { return throwsTypeSyntax_; }
void AstNode::setThrowsTypeSyntax(TypeSyntax* typeSyntax) noexcept { throwsTypeSyntax_ = typeSyntax; }
void AstNode::addTypeSyntax(TypeSyntax* typeSyntax)
{
    if (typeSyntax != nullptr)
    {
        typeSyntaxes_.push_back(typeSyntax);
    }
}
const std::vector<TypeSyntax*>& AstNode::typeSyntaxes() const noexcept { return typeSyntaxes_; }
void AstNode::clearTypeSyntaxes() noexcept { typeSyntaxes_.clear(); }
void AstNode::addGenericParameter(std::string name)
{
    GenericParameterSyntax parameter;
    parameter.name = std::move(name);
    addGenericParameter(std::move(parameter));
}
void AstNode::addGenericParameter(GenericParameterSyntax parameter)
{
    genericParameters_.push_back(parameter.name);
    genericParameterSyntaxes_.push_back(std::move(parameter));
}
const std::vector<std::string>& AstNode::genericParameters() const noexcept { return genericParameters_; }
const std::vector<GenericParameterSyntax>& AstNode::genericParameterSyntaxes() const noexcept
{
    return genericParameterSyntaxes_;
}
void AstNode::clearGenericParameters() noexcept
{
    genericParameters_.clear();
    genericParameterSyntaxes_.clear();
}
void AstNode::addLifetimeParameter(LifetimeParameterSyntax parameter)
{
    lifetimeParameters_.push_back(std::move(parameter));
}
const std::vector<LifetimeParameterSyntax>& AstNode::lifetimeParameters() const noexcept
{
    return lifetimeParameters_;
}
void AstNode::addLifetimeOutlivesConstraint(LifetimeOutlivesConstraint constraint)
{
    lifetimeOutlivesConstraints_.push_back(std::move(constraint));
}
const std::vector<LifetimeOutlivesConstraint>& AstNode::lifetimeOutlivesConstraints() const noexcept
{
    return lifetimeOutlivesConstraints_;
}
const std::string& AstNode::lifetimeName() const noexcept { return lifetimeName_; }
void AstNode::setLifetimeName(std::string name) { lifetimeName_ = std::move(name); }
void AstNode::addGenericConstraint(GenericConstraintClause clause) { genericConstraints_.push_back(std::move(clause)); }
const std::vector<GenericConstraintClause>& AstNode::genericConstraints() const noexcept { return genericConstraints_; }
void AstNode::clearGenericConstraints() noexcept { genericConstraints_.clear(); }
AstNode* AstNode::first() const noexcept { return first_; }
AstNode* AstNode::second() const noexcept { return second_; }
AstNode* AstNode::third() const noexcept { return third_; }
void AstNode::setFirst(AstNode* node) noexcept { first_ = node; }
void AstNode::setSecond(AstNode* node) noexcept { second_ = node; }
void AstNode::setThird(AstNode* node) noexcept { third_ = node; }
void AstNode::addChild(AstNode* node) { children_.push_back(node); }
void AstNode::prependChild(AstNode* node) { children_.insert(children_.begin(), node); }
const std::vector<AstNode*>& AstNode::children() const noexcept { return children_; }
bool AstNode::flag() const noexcept { return flag_; }
void AstNode::setFlag(bool value) noexcept { flag_ = value; }
bool AstNode::isReference() const noexcept { return reference_; }
void AstNode::setReference(bool value) noexcept { reference_ = value; }
bool AstNode::isVariadic() const noexcept { return variadic_; }
void AstNode::setVariadic(bool value) noexcept { variadic_ = value; }
bool AstNode::isUnsafe() const noexcept { return unsafe_; }
void AstNode::setUnsafe(bool value) noexcept { unsafe_ = value; }
FloatingPointModeSyntaxKind AstNode::floatingPointMode() const noexcept
{
    return floatingPointMode_;
}
void AstNode::setFloatingPointMode(FloatingPointModeSyntaxKind mode) noexcept
{
    floatingPointMode_ = mode;
}
FfiUnwindSyntaxKind AstNode::ffiUnwindPolicy() const noexcept { return ffiUnwindPolicy_; }
void AstNode::setFfiUnwindPolicy(FfiUnwindSyntaxKind policy) noexcept
{
    ffiUnwindPolicy_ = policy;
}
std::uint64_t AstNode::integerValue() const noexcept { return integerValue_; }
void AstNode::setIntegerValue(std::uint64_t value) noexcept { integerValue_ = value; }
double AstNode::floatingValue() const noexcept { return floatingValue_; }
void AstNode::setFloatingValue(double value) noexcept { floatingValue_ = value; }
}
