#pragma once

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/AST/FfiUnwindSyntaxKind.h"
#include "nativesharp/AST/FloatingPointModeSyntaxKind.h"
#include "nativesharp/AST/GenericConstraintClause.h"
#include "nativesharp/AST/GenericParameterSyntax.h"
#include "nativesharp/AST/GenericParameterSyntaxKind.h"
#include "nativesharp/AST/LifetimeOutlivesConstraint.h"
#include "nativesharp/AST/LifetimeParameterSyntax.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::ast
{
class TypeSyntax;

class AstNode final
{
public:
    AstNode(AstNodeKind kind, SourceRange range);

    AstNodeKind kind() const noexcept;
    SourceRange range() const noexcept;
    void setRange(SourceRange range) noexcept;

    const std::string& text() const noexcept;
    void setText(std::string text);

    const std::string& auxiliaryText() const noexcept;
    void setAuxiliaryText(std::string text);

    // Set only when this expression is the value of a named call argument (`M(value: expr)`).
    // This cannot reuse auxiliaryText: ref/out/in and other expressions already store semantic
    // information there.
    const std::string& argumentName() const noexcept;
    void setArgumentName(std::string name);

    // NS304-T13-F08. Present only on a PlacementConstructionExpression that spells
    // `failure: panic|null|result|throws`. Stores the profile name ("panic"/"null"/"result"/
    // "throws"); empty means no explicit profile. Independent from auxiliaryText ("trailing")
    // so a construction can carry both a trailing extent and a failure profile.
    const std::string& allocatorFailureProfile() const noexcept;
    void setAllocatorFailureProfile(std::string profile);

    lexer::TokenKind tokenKind() const noexcept;
    void setTokenKind(lexer::TokenKind kind) noexcept;

    void addModifier(lexer::TokenKind kind);
    const std::vector<lexer::TokenKind>& modifiers() const noexcept;
    bool hasModifier(lexer::TokenKind kind) const noexcept;

    TypeSyntax* typeSyntax() const noexcept;
    void setTypeSyntax(TypeSyntax* typeSyntax) noexcept;
    // NativeSharp 3.0 T07-F02. Present only on a callable declaration that spells
    // `throws E`; independent from the ordinary return type and generic/base type lists.
    TypeSyntax* throwsTypeSyntax() const noexcept;
    void setThrowsTypeSyntax(TypeSyntax* typeSyntax) noexcept;
    void addTypeSyntax(TypeSyntax* typeSyntax);
    const std::vector<TypeSyntax*>& typeSyntaxes() const noexcept;
    void clearTypeSyntaxes() noexcept;

    void addGenericParameter(std::string name);
    void addGenericParameter(GenericParameterSyntax parameter);
    const std::vector<std::string>& genericParameters() const noexcept;
    const std::vector<GenericParameterSyntax>& genericParameterSyntaxes() const noexcept;
    void clearGenericParameters() noexcept;

    void addLifetimeParameter(LifetimeParameterSyntax parameter);
    const std::vector<LifetimeParameterSyntax>& lifetimeParameters() const noexcept;
    void addLifetimeOutlivesConstraint(LifetimeOutlivesConstraint constraint);
    const std::vector<LifetimeOutlivesConstraint>& lifetimeOutlivesConstraints() const noexcept;

    // For a parameter this names the lifetime carried by ref/in. For a callable it names the
    // lifetime of its ref return. Empty means that ordinary elision must infer the binding.
    const std::string& lifetimeName() const noexcept;
    void setLifetimeName(std::string name);

    // NSC-LIMIT-007. Las clausulas `where` de esta declaracion, en el orden escrito.
    void addGenericConstraint(GenericConstraintClause clause);
    const std::vector<GenericConstraintClause>& genericConstraints() const noexcept;
    void clearGenericConstraints() noexcept;

    AstNode* first() const noexcept;
    AstNode* second() const noexcept;
    AstNode* third() const noexcept;
    void setFirst(AstNode* node) noexcept;
    void setSecond(AstNode* node) noexcept;
    void setThird(AstNode* node) noexcept;

    void addChild(AstNode* node);
    void prependChild(AstNode* node);
    const std::vector<AstNode*>& children() const noexcept;

    bool flag() const noexcept;
    void setFlag(bool value) noexcept;

    // Item 313. Separate from flag(): function declarations already use that bit for
    // external-only state, while ref-return declarations and `return ref` need an orthogonal
    // contextual marker that survives generic cloning.
    bool isReference() const noexcept;
    void setReference(bool value) noexcept;

    // Item 331. True only for a C declaration whose fixed parameter list ends in `...`.
    // This is separate from flag(): function declarations already use that bit for external-only.
    bool isVariadic() const noexcept;
    void setVariadic(bool value) noexcept;

    // NativeSharp 3.0 T13-F05. On callable declarations this is the caller-side unsafe
    // requirement. It is intentionally independent from flag()/isReference()/isVariadic(),
    // which already carry unrelated ABI state.
    bool isUnsafe() const noexcept;
    void setUnsafe(bool value = true) noexcept;

    FloatingPointModeSyntaxKind floatingPointMode() const noexcept;
    void setFloatingPointMode(FloatingPointModeSyntaxKind mode) noexcept;
    FfiUnwindSyntaxKind ffiUnwindPolicy() const noexcept;
    void setFfiUnwindPolicy(FfiUnwindSyntaxKind policy) noexcept;

    std::uint64_t integerValue() const noexcept;
    void setIntegerValue(std::uint64_t value) noexcept;

    double floatingValue() const noexcept;
    void setFloatingValue(double value) noexcept;

private:
    AstNodeKind kind_;
    SourceRange range_;
    std::string text_;
    std::string auxiliaryText_;
    std::string argumentName_;
    std::string allocatorFailureProfile_;
    lexer::TokenKind tokenKind_ = lexer::TokenKind::Invalid;
    std::vector<lexer::TokenKind> modifiers_;
    TypeSyntax* typeSyntax_ = nullptr;
    TypeSyntax* throwsTypeSyntax_ = nullptr;
    std::vector<TypeSyntax*> typeSyntaxes_;
    std::vector<std::string> genericParameters_;
    std::vector<GenericParameterSyntax> genericParameterSyntaxes_;
    std::vector<LifetimeParameterSyntax> lifetimeParameters_;
    std::vector<LifetimeOutlivesConstraint> lifetimeOutlivesConstraints_;
    std::string lifetimeName_;
    std::vector<GenericConstraintClause> genericConstraints_;
    AstNode* first_ = nullptr;
    AstNode* second_ = nullptr;
    AstNode* third_ = nullptr;
    std::vector<AstNode*> children_;
    bool flag_ = false;
    bool reference_ = false;
    bool variadic_ = false;
    bool unsafe_ = false;
    FloatingPointModeSyntaxKind floatingPointMode_ = FloatingPointModeSyntaxKind::Inherit;
    FfiUnwindSyntaxKind ffiUnwindPolicy_ = FfiUnwindSyntaxKind::Default;
    std::uint64_t integerValue_ = 0;
    double floatingValue_ = 0.0;
};
}
