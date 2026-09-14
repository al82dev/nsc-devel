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
    void addTypeSyntax(TypeSyntax* typeSyntax);
    const std::vector<TypeSyntax*>& typeSyntaxes() const noexcept;
    void clearTypeSyntaxes() noexcept;

    void addGenericParameter(std::string name);
    const std::vector<std::string>& genericParameters() const noexcept;
    void clearGenericParameters() noexcept;

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
    const std::vector<AstNode*>& children() const noexcept;

    bool flag() const noexcept;
    void setFlag(bool value) noexcept;

    // Item 313. Separate from flag(): function declarations already use that bit for
    // external-only state, while ref-return declarations and `return ref` need an orthogonal
    // contextual marker that survives generic cloning.
    bool isReference() const noexcept;
    void setReference(bool value) noexcept;

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
    std::vector<TypeSyntax*> typeSyntaxes_;
    std::vector<std::string> genericParameters_;
    std::vector<GenericConstraintClause> genericConstraints_;
    AstNode* first_ = nullptr;
    AstNode* second_ = nullptr;
    AstNode* third_ = nullptr;
    std::vector<AstNode*> children_;
    bool flag_ = false;
    bool reference_ = false;
    std::uint64_t integerValue_ = 0;
    double floatingValue_ = 0.0;
};
}
