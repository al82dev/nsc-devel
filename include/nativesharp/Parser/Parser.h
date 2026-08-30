#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Token.h"

#include <cstddef>
#include <span>
#include <vector>
#include <string_view>
#include <unordered_set>

namespace nativesharp::parser
{
class Parser final
{
public:
    Parser(
        std::span<const lexer::Token> tokens,
        ast::AstArena& arena,
        diagnostics::DiagnosticEngine& diagnostics,
        std::span<const std::string> knownTypeNames = {});

    ast::AstNode* parseCompilationUnit();

private:
    const lexer::Token& current() const noexcept;
    const lexer::Token& peek(std::size_t distance = 1) const noexcept;
    bool at(lexer::TokenKind kind) const noexcept;
    bool atContextualKeyword(std::string_view keyword) const noexcept;
    bool consumeIf(lexer::TokenKind kind) noexcept;
    const lexer::Token& consume() noexcept;
    bool expect(lexer::TokenKind kind, std::string_view description);

    SourceRange join(SourceRange begin, SourceRange end) const noexcept;
    SourceRange tokenRangeFrom(std::size_t beginIndex) const noexcept;

    bool canStartType(const lexer::Token& token) const noexcept;
    bool canStartTypeAt(std::size_t tokenIndex) const noexcept;
    bool looksLikeParenthesizedType() const noexcept;
    bool parseQualifiedName(std::string& name, SourceRange& range);
    ast::TypeSyntax* parseType();
    ast::TypeSyntax* parseAllocationElementType();
    bool parseGenericParameterList(ast::AstNode* declaration);
    bool parseTypeArgumentList(std::vector<ast::TypeSyntax*>& arguments);
    bool consumeGenericGreater();
    bool looksLikeGenericCallTypeArguments() const noexcept;

    ast::AstNode* parseNamespaceMember();
    ast::AstNode* parseNamespaceDeclaration();
    ast::AstNode* parseUsingDirective();
    ast::AstNode* parseGlobalDeclaration();
    // C2.2 "union and explicit layout": a union is parsed by the same routine as a struct --
    // they differ only in where their fields land, which is a layout question, not a syntax one.
    enum class StructLayoutForm
    {
        Struct,
        Union
    };
    ast::AstNode* parseStructDeclaration(StructLayoutForm form = StructLayoutForm::Struct);
    ast::AstNode* parseClassDeclaration(bool isPartial, bool isAbstract, bool isSealed);
    ast::AstNode* parseInterfaceDeclaration(bool isPartial);
    ast::AstNode* parseDelegateDeclaration();
    ast::AstNode* parseInterfaceMember();
    ast::AstNode* parseClassMember(std::string_view className, bool allowUnresolvedNamedTypes = false);
    ast::AstNode* parseEventTail(
        lexer::TokenKind accessibility,
        bool isStatic,
        std::size_t beginIndex);
    ast::AstNode* parseDestructorTail(std::string_view className, std::size_t beginIndex);
    ast::AstNode* parsePropertyTail(
        ast::TypeSyntax* propertyType,
        const lexer::Token& name,
        lexer::TokenKind accessibility,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        std::size_t beginIndex,
        bool interfaceContext);
    ast::AstNode* parseIndexerTail(
        ast::TypeSyntax* elementType,
        lexer::TokenKind accessibility,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        std::size_t beginIndex,
        bool interfaceContext);
    ast::AstNode* parseOperatorTail(
        ast::TypeSyntax* returnType,
        lexer::TokenKind accessibility,
        bool isStatic,
        std::size_t beginIndex);
    ast::AstNode* parseConversionOperatorTail(
        bool implicitConversion,
        lexer::TokenKind accessibility,
        bool isStatic,
        std::size_t beginIndex);
    ast::AstNode* makeAccessorMethod(
        std::string name,
        ast::TypeSyntax* returnType,
        lexer::TokenKind accessibility,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        std::string metadata,
        SourceRange range);
    ast::AstNode* makeAutoGetterBody(
        std::string_view backingFieldName,
        SourceRange range);
    ast::AstNode* makeAutoSetterBody(
        std::string_view backingFieldName,
        SourceRange range,
        bool transferOwnership);
    ast::AstNode* parseConstructorTail(
        std::string_view className,
        lexer::TokenKind accessibility,
        std::size_t beginIndex);
    ast::AstNode* parseMethodTail(
        ast::TypeSyntax* returnType,
        const lexer::Token& name,
        lexer::TokenKind accessibility,
        bool isStatic,
        bool isVirtual,
        bool isOverride,
        bool isAbstract,
        bool isSealed,
        bool isExported,
        bool isExternC,
        std::size_t beginIndex);
    ast::AstNode* parseEnumDeclaration();
    ast::AstNode* parseExternCDeclaration();
    ast::AstNode* parseFunctionOrGlobalVariable();
    ast::AstNode* parseFunctionTail(ast::TypeSyntax* returnType, const lexer::Token& name, bool externalOnly);
    ast::AstNode* parseParameter();

    ast::AstNode* parseBlock();
    ast::AstNode* parseStatement();
    ast::AstNode* parseVariableDeclaration(bool expectSemicolon);
    ast::AstNode* parseIfStatement();
    ast::AstNode* parseWhileStatement();
    ast::AstNode* parseForStatement();
    ast::AstNode* parseForeachStatement();
    ast::AstNode* makeIdentifierExpression(const SourceRange& range, std::string name);
    ast::AstNode* makeIntegerLiteralExpression(const SourceRange& range, std::uint64_t value);
    ast::AstNode* makeMemberExpression(ast::AstNode* receiver, std::string memberName);
    ast::AstNode* makeZeroArgCallExpression(ast::AstNode* callee);
    ast::AstNode* makeIndexExpression(ast::AstNode* receiver, ast::AstNode* index);
    ast::AstNode* makeBinaryExpression(lexer::TokenKind operatorKind, ast::AstNode* left, ast::AstNode* right);
    ast::AstNode* makeAssignmentExpression(ast::AstNode* target, ast::AstNode* value);
    ast::AstNode* makeExpressionStatement(ast::AstNode* expression);
    ast::AstNode* makeVariableDeclaration(
        ast::TypeSyntax* type, std::string name, ast::AstNode* initializer, bool isRaii, bool isConst = false);
    ast::AstNode* parseReturnStatement();
    ast::AstNode* parseTryStatement();
    ast::AstNode* parseCatchClause();
    ast::AstNode* parseThrowStatement();
    ast::AstNode* parseUsingStatement();

    ast::AstNode* parseExpression();
    ast::AstNode* parseAssignmentExpression();
    ast::AstNode* parseLambdaExpression();
    bool looksLikeLambdaExpression() const noexcept;
    ast::AstNode* parseLogicalOrExpression();
    ast::AstNode* parseLogicalAndExpression();
    ast::AstNode* parseBitwiseOrExpression();
    ast::AstNode* parseBitwiseXorExpression();
    ast::AstNode* parseBitwiseAndExpression();
    ast::AstNode* parseEqualityExpression();
    ast::AstNode* parseRelationalExpression();
    ast::AstNode* parseShiftExpression();
    ast::AstNode* parseAdditiveExpression();
    ast::AstNode* parseMultiplicativeExpression();
    ast::AstNode* parseUnaryExpression();
    ast::AstNode* parsePostfixExpression();
    ast::AstNode* parsePrimaryExpression();

    ast::AstNode* parseBinaryLevel(
        ast::AstNode* (Parser::*nextLevel)(),
        std::span<const lexer::TokenKind> operators,
        ast::AstNodeKind nodeKind);

    ast::AstNode* makeErrorExpression();
    void synchronizeGlobal();
    void synchronizeStatement();

    std::span<const lexer::Token> tokens_;
    ast::AstArena& arena_;
    diagnostics::DiagnosticEngine& diagnostics_;
    std::size_t index_ = 0;
    std::size_t pendingGenericGreater_ = 0;
    std::unordered_set<std::string> knownTypeNames_;
};
}
