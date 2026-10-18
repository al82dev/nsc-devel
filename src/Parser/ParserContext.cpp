#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <array>
#include <string>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
}

// Seeds the parser cursor and the nominal names required to disambiguate declarations from
// expressions. The supplied token span and referenced services must outlive the parser.
Parser::Parser(
    std::span<const lexer::Token> tokens,
    ast::AstArena& arena,
    diagnostics::DiagnosticEngine& diagnostics,
    std::span<const std::string> knownTypeNames)
    : tokens_(tokens),
      arena_(arena),
      diagnostics_(diagnostics)
{
    static constexpr std::array<std::string_view, 19> builtinNames = {
        "void", "bool", "byte", "char", "short", "ushort", "int", "uint", "long", "ulong",
        "nint", "nuint", "float", "double", "string",
        // C8 "type root": `object` is a KNOWN TYPE NAME here so that `object x = ...;` parses as
        // a declaration, but it is deliberately NOT a lexer keyword and gets no builtin type key:
        // name lookup resolves it, as a last resort, to NativeSharp.Core.Object (see
        // ProjectNameLookup::bindTypeSyntax). A project without the root in view therefore gets
        // "unknown type name 'object'" rather than a parse that quietly means something else.
        "object",
        // NS304-T05-F09: `Future<T>` is the opaque future surface. A known type name here so
        // `Future<int> f = ...;` parses as a declaration (resolved by canonicalTypeKey to future:<T>).
        "Future",
        // NS304-T06-F02: `Iterator<T>` is the opaque generator iterator surface (resolved by
        // canonicalTypeKey to iterator:<T>).
        "Iterator",
        // NS304-T07-F01: `AsyncIterator<T,E>` is the opaque async-stream iterator surface.
        "AsyncIterator"};

    for (const std::string_view name : builtinNames)
    {
        knownTypeNames_.insert(std::string(name));
    }
    for (const std::string& name : knownTypeNames)
    {
        if (!name.empty())
        {
            knownTypeNames_.insert(name);
        }
    }
    buildConditionalQuestionIndex();
    buildComprehensionDelimiterIndex();
}

// Returns the logical token at the cursor, including a pending '=' split from a generic closer.
// The lexer contract guarantees an EOF token, so the returned reference remains in tokens_.
const lexer::Token& Parser::current() const noexcept
{
    if (pendingGenericEqual_)
        return *pendingGenericEqualToken_;
    return tokens_[index_ < tokens_.size() ? index_ : tokens_.size() - 1];
}

// Returns a logical lookahead token without changing parser state and saturates at EOF.
const lexer::Token& Parser::peek(std::size_t distance) const noexcept
{
    if (pendingGenericEqual_ && distance == 0U)
        return *pendingGenericEqualToken_;
    const std::size_t target = pendingGenericEqual_
        ? index_ + distance - 1U
        : index_ + distance;
    return tokens_[target < tokens_.size() ? target : tokens_.size() - 1];
}

// Tests the logical current token without consuming it.
bool Parser::at(TokenKind kind) const noexcept
{
    return current().kind() == kind;
}

// Tests an exact, case-sensitive contextual-keyword spelling without consuming it.
bool Parser::atContextualKeyword(std::string_view keyword) const noexcept
{
    return current().kind() == TokenKind::Identifier && current().lexeme() == keyword;
}

// Looks through a balanced parenthesized header to determine whether a contextual keyword starts
// a braced construct. The scan is read-only and returns false for incomplete delimiters.
bool Parser::startsParenthesizedBlock(std::string_view keyword) const noexcept
{
    if (!atContextualKeyword(keyword) || peek().kind() != TokenKind::LeftParen)
        return false;

    std::size_t depth = 0;
    for (std::size_t cursor = index_ + 1U; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::LeftParen)
        {
            ++depth;
            continue;
        }
        if (kind != TokenKind::RightParen || depth == 0)
            continue;

        --depth;
        if (depth == 0)
            return cursor + 1U < tokens_.size() &&
                tokens_[cursor + 1U].kind() == TokenKind::LeftBrace;
    }
    return false;
}

// Consumes an exact contextual keyword or reports ExpectedToken at the unchanged cursor.
bool Parser::expectContextualKeyword(
    std::string_view keyword,
    std::string_view description)
{
    if (atContextualKeyword(keyword))
    {
        consume();
        return true;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected " + std::string(description) + ", found '" +
            std::string(current().lexeme()) + "'");
    return false;
}

// Consumes the requested kind when present and otherwise leaves the cursor unchanged.
bool Parser::consumeIf(TokenKind kind) noexcept
{
    if (!at(kind))
    {
        return false;
    }
    consume();
    return true;
}

// Consumes one logical token, handling a pending generic '=' before advancing the physical cursor.
// Repeated calls at EOF return the stable final token.
const lexer::Token& Parser::consume() noexcept
{
    const lexer::Token& token = current();
    if (pendingGenericEqual_)
    {
        pendingGenericEqual_ = false;
        return token;
    }
    if (!at(TokenKind::EndOfFile))
    {
        ++index_;
    }
    return token;
}

// Consumes an expected kind or emits the existing ExpectedToken diagnostic without recovery.
bool Parser::expect(TokenKind kind, std::string_view description)
{
    if (consumeIf(kind))
    {
        return true;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected " + std::string(description) + ", found '" + std::string(current().lexeme()) + "'");
    return false;
}

// Joins two source ranges while preserving the first begin and last end positions.
SourceRange Parser::join(SourceRange begin, SourceRange end) const noexcept
{
    return SourceRange(begin.begin(), end.end());
}

// Returns the physical token range consumed since beginIndex. The caller must provide a valid
// starting token and invoke this only after at least one physical token has been considered.
SourceRange Parser::tokenRangeFrom(std::size_t beginIndex) const noexcept
{
    const std::size_t endIndex = index_ == 0 ? 0 : index_ - 1;
    return join(tokens_[beginIndex].range(), tokens_[endIndex].range());
}

// Creates the parser's existing sentinel expression at the current source range.
ast::AstNode* Parser::makeErrorExpression()
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, current().range());
    node->setText("<error>");
    return node;
}

// Advances global-declaration recovery to a stable boundary while leaving a usable starter token
// for the caller. The operation is linear in the number of discarded tokens.
void Parser::synchronizeGlobal()
{
    while (!at(TokenKind::EndOfFile))
    {
        if (consumeIf(TokenKind::Semicolon))
        {
            return;
        }
        if (at(TokenKind::RightBrace) || at(TokenKind::KwStruct) || at(TokenKind::KwEnum) ||
            at(TokenKind::KwNamespace) || at(TokenKind::KwUsing) || at(TokenKind::KwExtern) ||
            at(TokenKind::KwPublic) || at(TokenKind::KwInternal) || at(TokenKind::KwConst) ||
            canStartTypeAt(index_))
        {
            return;
        }
        consume();
    }
}

// Advances statement recovery through a semicolon or stops before the enclosing right brace.
void Parser::synchronizeStatement()
{
    while (!at(TokenKind::EndOfFile) && !at(TokenKind::RightBrace))
    {
        if (consumeIf(TokenKind::Semicolon))
        {
            return;
        }
        consume();
    }
}
}
