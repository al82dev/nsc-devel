#pragma once

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::ast
{
class TypeSyntax;

enum class GenericParameterSyntaxKind
{
    Type,
    Constant,
    TypePack,
    ConstantPack
};

struct GenericParameterSyntax
{
    std::string name;
    GenericParameterSyntaxKind kind = GenericParameterSyntaxKind::Type;
    TypeSyntax* constantType = nullptr;
    SourceRange range;
};

// NativeSharp 3.0 T06-F04. Lifetime parameters are erased semantic parameters: unlike
// type/const generics they never contribute to ABI identity or request monomorphization.
struct LifetimeParameterSyntax
{
    std::string name;
    SourceRange range;
};

struct LifetimeOutlivesConstraint
{
    std::string longer;
    std::string shorter;
    SourceRange range;
};

// NSC-LIMIT-007. Una clausula `where T : class, Base, IFoo` tal como se escribio.
//
// SE GUARDA SIN RESOLVER, y esa es la decision: los tipos quedan como `TypeSyntax*`, no como
// claves de tipo ni como ordinales. Cuando el parser lee la clausula no existe todavia el name
// lookup --se construye tres fases mas tarde-- asi que resolver aqui exigiria una copia privada
// de la resolucion de nombres, que es justo la clase de duplicado que este proyecto persigue.
// Quien valida es una fase semantica posterior, con la herencia y las interfaces ya construidas.
struct GenericConstraintClause
{
    // El parametro que la clausula restringe. Se comprueba contra genericParameters() en el
    // parser: `where U : class` sobre un tipo que no declara `U` es un error de sintaxis, no una
    // condicion que falle mas tarde con un tipo concreto.
    std::string parameterName;
    SourceRange parameterRange;
    // `class` escrito en primera posicion. Es el unico constraint que NO es un tipo, y por eso
    // viaja como bandera y no dentro de `typeConstraints`.
    bool requiresClass = false;
    // NativeSharp 3 declarative capability constraints. They remain orthogonal flags so their
    // canonical identity is independent of source order and spelling duplication.
    bool requiresValueType = false;
    bool requiresDefaultConstructor = false;
    bool requiresCopy = false;
    bool requiresMove = false;
    bool requiresTrackedLifetime = false;
    bool requiresUnmanaged = false;
    bool requiresNotNull = false;
    // `where T == U` and `where N == 4`. A const value uses the same deterministic TypeSyntax
    // marker as a const-generic argument; a type/parameter uses ordinary type syntax and is
    // substituted by the generic cloner.
    TypeSyntax* equalityConstraint = nullptr;
    // T05-F03. Left side of `where T.Item == Type`. Null for ordinary parameter equality
    // (`where T == U`). The generic cloner normalizes this projection after substituting T and
    // exposes the resulting concrete type through substitutedArgument.
    TypeSyntax* associatedProjection = nullptr;
    // La clase base y las interfaces, en el orden escrito. Cual es cual no se decide aqui: eso
    // necesita saber que declara cada nombre, y el parser no lo sabe.
    std::vector<TypeSyntax*> typeConstraints;
    // La clausula entera, `where` incluido, para el diagnostico.
    SourceRange range;
    // NSC-LIMIT-007. SOLO EN UNA CLAUSULA CLONADA: el tipo con el que se sustituyo el parametro.
    //
    // Existe porque el argumento de una especializacion --el `Valor` de `Caja<Valor>`-- se
    // BORRA del arbol al materializar (`clearTypeArguments`), asi que la resolucion de nombres,
    // que corre despues, no lo ve nunca y nadie puede preguntar de que tipo es. Colgandolo de la
    // clausula clonada --que SI esta en el arbol-- el argumento vuelve a pasar por la resolucion
    // ordinaria, y la validacion no necesita una segunda copia de la resolucion de nombres.
    const TypeSyntax* substitutedArgument = nullptr;
};

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
    std::uint64_t integerValue_ = 0;
    double floatingValue_ = 0.0;
};
}
