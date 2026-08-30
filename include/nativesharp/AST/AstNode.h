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

    std::uint64_t integerValue() const noexcept;
    void setIntegerValue(std::uint64_t value) noexcept;

    double floatingValue() const noexcept;
    void setFloatingValue(double value) noexcept;

private:
    AstNodeKind kind_;
    SourceRange range_;
    std::string text_;
    std::string auxiliaryText_;
    lexer::TokenKind tokenKind_ = lexer::TokenKind::Invalid;
    std::vector<lexer::TokenKind> modifiers_;
    TypeSyntax* typeSyntax_ = nullptr;
    std::vector<TypeSyntax*> typeSyntaxes_;
    std::vector<std::string> genericParameters_;
    AstNode* first_ = nullptr;
    AstNode* second_ = nullptr;
    AstNode* third_ = nullptr;
    std::vector<AstNode*> children_;
    bool flag_ = false;
    std::uint64_t integerValue_ = 0;
    double floatingValue_ = 0.0;
};
}
