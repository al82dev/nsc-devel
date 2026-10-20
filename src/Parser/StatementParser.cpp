#include "nativesharp/Parser/Parser.h"

#include "Internal/ConditionDeclarationBinding.h"

#include "Internal/StructuredControlParser.h"
#include "Internal/StatementDivergenceWork.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Target/TargetInfo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
}

void Parser::appendParsedStatement(ast::AstNode*& destination, ast::AstNode* statement)
{
    if (statement == nullptr) return;
    if (statement->kind() == ast::AstNodeKind::BlockStatement &&
        statement->auxiliaryText() == "defer-registration")
    {
        ast::AstNode* generatedTry = nullptr;
        for (ast::AstNode* generated : statement->children())
        {
            destination->addChild(generated);
            if (generated != nullptr && generated->kind() == ast::AstNodeKind::TryStatement)
                generatedTry = generated;
        }
        if (generatedTry != nullptr && generatedTry->first() != nullptr)
            destination = generatedTry->first();
        return;
    }
    if (statement->kind() == ast::AstNodeKind::BlockStatement &&
        statement->auxiliaryText() == "conditional-pattern-inline")
    {
        for (ast::AstNode* child : statement->children()) destination->addChild(child);
        return;
    }
    destination->addChild(statement);
}

ast::AstNode* Parser::parseBlockImpl()
{
    const std::size_t beginIndex = index_;
    expect(TokenKind::LeftBrace, "'{'");
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, tokenRangeFrom(beginIndex));
    // A defer protects every statement that follows it in this lexical block. `destination`
    // therefore advances into the generated try body after every registration. A second defer
    // nests another try and naturally produces LIFO cleanup without teaching later stages a
    // second exit model.
    ast::AstNode* destination = block;

    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const std::size_t before = index_;
        ast::AstNode* statement = parseStatement();
        appendParsedStatement(destination, statement);
        if (index_ == before)
        {
            consume();
        }
    }

    expect(TokenKind::RightBrace, "'}'");
    block->setRange(tokenRangeFrom(beginIndex));
    return block;
}

ast::AstNode* Parser::parseStatementImpl()
{
    if (at(TokenKind::LeftBrace)) return parseBlock();
    if (const auto statement = tryParseScopedStatement()) return *statement;
    if (const auto statement = tryParseControlStatement()) return *statement;
    if (atContextualKeyword("yield")) return parseYieldStatement();
    if (at(TokenKind::KwStatic)) return parseStaticLocalStatement();
    if (at(TokenKind::KwBreak) || at(TokenKind::KwContinue)) return parseJumpStatement();
    if (const auto statement = tryParseKeywordStatement()) return *statement;
    if (const auto statement = tryParseDeclarationStatement()) return *statement;
    return parseExpressionStatement();
}

ast::AstNode* Parser::parseFloatingPointScopeStatement()
{
    consume();
    consume();
    ast::FloatingPointModeSyntaxKind mode = ast::FloatingPointModeSyntaxKind::Inherit;
    if (atContextualKeyword("strict")) mode = ast::FloatingPointModeSyntaxKind::Strict;
    else if (atContextualKeyword("precise")) mode = ast::FloatingPointModeSyntaxKind::Precise;
    else if (atContextualKeyword("fast")) mode = ast::FloatingPointModeSyntaxKind::Fast;
    else diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "floating-point scope mode must be strict, precise, or fast");
    std::string modeName;
    if (mode != ast::FloatingPointModeSyntaxKind::Inherit)
    {
        modeName = std::string(current().lexeme());
        consume();
    }
    expect(TokenKind::RightParen, "')' after floating-point scope mode");
    ast::AstNode* block = parseBlock();
    block->setAuxiliaryText("fp-scope:" + modeName);
    applyMemberFloatingPointMode(block, mode);
    return block;
}

ast::AstNode* Parser::parseUnsafeScopeStatement()
{
    consume();
    std::string reason;
    if (consumeIf(TokenKind::LeftParen))
    {
        if (at(TokenKind::StringLiteral)) reason = std::string(consume().lexeme());
        else expect(TokenKind::StringLiteral, "unsafe block reason string");
        expect(TokenKind::RightParen, "')' after unsafe block reason");
    }
    ast::AstNode* block = parseBlock();
    block->setAuxiliaryText("unsafe-block");
    block->setText(reason);
    if (language::unsafeBlockTooBroad(block->children().size(), !reason.empty()))
        diagnostics_.report(
            diagnostics::DiagnosticId::NS3030MissingEffectContext,
            diagnostics::DiagnosticSeverity::Warning,
            block->range(),
            "unsafe block covers more than eight statements; narrow it or write unsafe(\"reason\") { ... }");
    return block;
}

std::optional<ast::AstNode*> Parser::tryParseScopedStatement()
{
    if (atContextualKeyword("fp") && peek().kind() == TokenKind::LeftParen)
        return parseFloatingPointScopeStatement();
    if (atContextualKeyword("unsafe") &&
        (peek().kind() == TokenKind::LeftBrace || peek().kind() == TokenKind::LeftParen))
        return parseUnsafeScopeStatement();
    if (atContextualKeyword("asm") &&
        (peek().kind() == TokenKind::LeftBrace || peek().kind() == TokenKind::LeftParen))
        return parseInlineAssemblyStatement();
    return std::nullopt;
}

std::optional<ast::AstNode*> Parser::tryParseControlStatement()
{
    const auto beginsConditionalPattern = [&]() noexcept
    {
        return peek().kind() == TokenKind::LeftParen &&
            peek(2).kind() == TokenKind::Identifier && peek(2).lexeme() == "let";
    };
    if ((at(TokenKind::KwIf) || at(TokenKind::KwWhile)) && beginsConditionalPattern())
        return parseConditionalPatternStatement();
    if (atContextualKeyword("let")) return parseConditionalPatternStatement();
    if (at(TokenKind::KwIf)) return parseIfStatement();
    if (at(TokenKind::KwWhile)) return parseWhileStatement();
    if (at(TokenKind::KwDo)) return parseDoWhileStatement();
    if (at(TokenKind::KwFor)) return parseForStatement();
    if (at(TokenKind::KwForeach)) return parseForeachStatement();
    if (at(TokenKind::KwSwitch)) return parseSwitchStatement();
    if (startsParenthesizedBlock("match"))
        return parseMatchStatement();
    return std::nullopt;
}

ast::AstNode* Parser::parseYieldStatement()
{
    if (!comprehensionContexts_.empty() && atContextualKeyword("yield"))
        return parseComprehensionYieldStatement();
    if (peek().kind() == TokenKind::KwReturn || peek().kind() == TokenKind::KwBreak)
    {
        // NS304-T06-F01: `yield return expr` / `yield break` ya no se rechazan con NS3049. El
        // lowering productivo a frame/state machine es T06-F03.
        const std::size_t beginIndex = index_;
        consume(); // yield
        ast::AstNode* statement;
        if (at(TokenKind::KwReturn))
        {
            consume(); // return
            ast::AstNode* value = parseExpression();
            statement = arena_.make<ast::AstNode>(
                ast::AstNodeKind::YieldReturnStatement, tokenRangeFrom(beginIndex));
            statement->setFirst(value);
        }
        else
        {
            consume(); // break
            statement = arena_.make<ast::AstNode>(
                ast::AstNodeKind::YieldBreakStatement, tokenRangeFrom(beginIndex));
        }
        expect(TokenKind::Semicolon, "';' after 'yield return'/'yield break'");
        return statement;
    }
    const std::size_t beginIndex = index_;
    consume(); // yield
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::SuspendStatement, tokenRangeFrom(beginIndex));
    expect(TokenKind::Semicolon, "';' after 'yield'");
    return statement;
}

ast::AstNode* Parser::parseStaticLocalStatement()
{
    if (looksLikeLocalFunction()) return parseLocalFunctionAsLambda();
    const SourceRange staticRange = consume().range();
    const bool declaration =
        (atContextualKeyword("pin") &&
         (peek().kind() == TokenKind::KwRaii ||
          (peek().kind() == TokenKind::Identifier && peek().lexeme() == "ref"))) ||
        at(TokenKind::KwRaii) || at(TokenKind::KwConst) ||
        at(TokenKind::KwVolatile) || at(TokenKind::KwVar) ||
        (atContextualKeyword("ref") &&
         (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar ||
          peek().kind() == TokenKind::KwConst || peek().kind() == TokenKind::KwRaii)) ||
        (looksLikeVariableDeclaration() && !looksLikeClosedGenericTypeReference());
    if (declaration) return parseVariableDeclaration(true, true);
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedDeclaration,
        diagnostics::DiagnosticSeverity::Error,
        staticRange,
        "static statement must declare a local variable");
    synchronizeStatement();
    return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, staticRange);
}

ast::AstNode* Parser::parseJumpStatement()
{
    const std::size_t beginIndex = index_;
    const TokenKind kind = consume().kind();
    ast::AstNode* value = nullptr;
    std::string targetLabel;
    if (kind == TokenKind::KwBreak)
    {
        if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Colon)
        {
            targetLabel = std::string(consume().lexeme());
            consume();
        }
        if (!at(TokenKind::Semicolon)) value = parseExpression();
    }
    else if (at(TokenKind::Identifier)) targetLabel = std::string(consume().lexeme());
    expect(TokenKind::Semicolon, "';'");
    ast::AstNode* exit = arena_.make<ast::AstNode>(
        kind == TokenKind::KwBreak
            ? ast::AstNodeKind::BreakStatement
            : ast::AstNodeKind::ContinueStatement,
        tokenRangeFrom(beginIndex));
    exit->setFirst(value);
    exit->setText(std::move(targetLabel));
    return exit;
}

std::optional<ast::AstNode*> Parser::tryParseKeywordStatement()
{

    // NativeSharp 3.0 T08-F06. `must_tail` is a call-site contract rather than a
    // declaration hint: the following return must lower to one ABI-compatible tail call.
    if (atContextualKeyword("must_tail"))
    {
        const SourceRange attributeRange = consume().range();
        if (!at(TokenKind::KwReturn))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "must_tail must be followed by a return statement");
            synchronizeStatement();
            return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, attributeRange);
        }
        ast::AstNode* statement = parseReturnStatement();
        statement->setFlag(true);
        return statement;
    }
    if (at(TokenKind::KwReturn)) return parseReturnStatement();
    if (atContextualKeyword("defer")) return parseDeferStatement();
    if (at(TokenKind::KwTry)) return parseTryStatement();
    if (at(TokenKind::KwThrow)) return parseThrowStatement();
    if (at(TokenKind::KwUsing)) return parseUsingStatement();
    if (at(TokenKind::KwStaticAssert)) return parseStaticAssertStatement();
    return std::nullopt;
}

std::optional<ast::AstNode*> Parser::tryParseDeclarationStatement()
{
    if (at(TokenKind::KwVar) && peek().kind() == TokenKind::LeftParen)
    {
        const std::size_t beginIndex = index_;
        const SourceRange featureRange = consume().range();
        ast::AstNode* pattern = parseDeconstructionPattern();
        ast::AstNode* declaration = arena_.make<ast::AstNode>(
            ast::AstNodeKind::DeconstructionDeclarationStatement, featureRange);
        if (pattern != nullptr && pattern->kind() == ast::AstNodeKind::TupleExpression)
        {
            for (ast::AstNode* binding : pattern->children()) declaration->addChild(binding);
        }
        expect(TokenKind::Equal, "'=' after deconstruction bindings");
        declaration->setFirst(parseAssignmentExpression());
        expect(TokenKind::Semicolon, "';'");
        declaration->setRange(tokenRangeFrom(beginIndex));
        return declaration;
    }

    // ID 131, la otra mitad. Al principio de una sentencia, `Caja<int>` puede empezar DOS cosas:
    // una declaracion (`Caja<int> c = ...`) o una expresion (`Caja<int>.Cuenta = 5;`). Las
    // distingue lo que sigue al `>` de cierre: un identificador en la primera, un `.` en la
    // segunda -- y `looksLikeClosedGenericTypeReference` exige exactamente `.` mas identificador,
    // que no puede empezar ninguna declaracion.
    //
    // Sin esto, `canStartTypeAt` decia "declaracion", `parseVariableDeclaration` leia el tipo y
    // pedia un nombre de variable, y el error era "expected variable name, found '.'": un
    // diagnostico que no menciona ni genericos ni miembros estaticos y manda a mirar donde no es.
    if (looksLikeLocalFunction())
    {
        // NS304-T04-F06: funcion local `type name (...)` baja a lambda con tipo natural.
        return parseLocalFunctionAsLambda();
    }
    if (atContextualKeyword("await"))
    {
        // NS304-T07-F06: `await future;` (un Future<void>) es una sentencia de expresion -- un
        // poll bloqueante del future -- y no una declaracion de variable con un tipo `await`.
        const std::size_t beginIndex = index_;
        ast::AstNode* expression = parseExpression();
        ast::AstNode* statement = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ExpressionStatement,
            expression != nullptr ? expression->range() : current().range());
        statement->setFirst(expression);
        expect(TokenKind::Semicolon, "';' after await expression");
        statement->setRange(tokenRangeFrom(beginIndex));
        return statement;
    }
    if ((atContextualKeyword("pin") &&
         (peek().kind() == TokenKind::KwRaii ||
          (peek().kind() == TokenKind::Identifier && peek().lexeme() == "ref"))) ||
        at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVolatile) || at(TokenKind::KwVar) ||
        (atContextualKeyword("ref") &&
         (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar ||
          peek().kind() == TokenKind::KwConst || peek().kind() == TokenKind::KwRaii)) ||
        (looksLikeVariableDeclaration() && !looksLikeClosedGenericTypeReference()))
    {
        return parseVariableDeclaration(true);
    }
    return std::nullopt;
}

ast::AstNode* Parser::parseExpressionStatement()
{
    const std::size_t beginIndex = index_;
    ast::AstNode* expression = parseExpression();
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExpressionStatement,
        expression != nullptr ? expression->range() : current().range());
    statement->setFirst(expression);
    expect(TokenKind::Semicolon, "';'");
    statement->setRange(tokenRangeFrom(beginIndex));
    return statement;
}

ast::AstNode* Parser::parseDeconstructionPattern()
{
    const SourceRange bindingStart = current().range();
    std::string bindingMode = "value";
    if (atContextualKeyword("ref"))
    {
        consume();
        bindingMode = "ref";
    }
    else if (consumeIf(TokenKind::KwMove))
    {
        bindingMode = "move";
    }

    if (at(TokenKind::Identifier))
    {
        const lexer::Token& name = consume();
        ast::AstNode* binding = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IdentifierExpression,
            bindingMode == "value" ? name.range() : join(bindingStart, name.range()));
        binding->setText(std::string(name.lexeme()));
        binding->setAuxiliaryText(std::move(bindingMode));
        return binding;
    }

    if (bindingMode != "value")
    {
        expect(TokenKind::Identifier, "deconstruction binding name after binding mode");
        return makeErrorExpression();
    }

    if (!at(TokenKind::LeftParen))
    {
        expect(TokenKind::LeftParen, "'(' in deconstruction pattern");
        return makeErrorExpression();
    }

    const SourceRange open = consume().range();
    ast::AstNode* tuple = arena_.make<ast::AstNode>(ast::AstNodeKind::TupleExpression, open);
    if (at(TokenKind::RightParen))
    {
        const SourceRange close = consume().range();
        tuple->setRange(join(open, close));
        return tuple;
    }

    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier) && !at(TokenKind::KwMove) && !at(TokenKind::LeftParen))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "expected deconstruction binding name, '_', or nested pattern");
            consume();
        }
        else
        {
            tuple->addChild(parseDeconstructionPattern());
        }
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightParen, "')' after deconstruction pattern");
    if (index_ != 0)
        tuple->setRange(SourceRange(open.begin(), tokens_[index_ - 1].range().end()));
    return tuple;
}

namespace
{
bool isInlineAssemblyRegisterName(std::string_view name) noexcept
{
    if (name.empty()) return false;
    for (const unsigned char byte : name)
    {
        if (std::isalnum(byte) == 0 && byte != '_') return false;
    }
    return true;
}

std::string canonicalInlineAssemblyRegister(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char byte)
    {
        return static_cast<char>(std::tolower(byte));
    });
    return value;
}

std::string_view trimAssemblyText(std::string_view text) noexcept
{
    const std::size_t begin = text.find_first_not_of(" \t\v\f");
    if (begin == std::string_view::npos) return {};
    const std::size_t end = text.find_last_not_of(" \t\v\f");
    return text.substr(begin, end - begin + 1);
}

bool isInlineAssemblyBranchMnemonic(std::string_view mnemonic) noexcept
{
    static const std::unordered_set<std::string_view> branches = {
        "jmp", "je", "jne", "jz", "jnz", "ja", "jae", "jb", "jbe", "jg", "jge",
        "jl", "jle", "jo", "jno", "js", "jns", "jp", "jnp", "jc", "jnc",
        "jcxz", "jecxz", "jrcxz", "loop", "loope", "loopne"};
    return branches.contains(mnemonic);
}

std::string_view fixedInlineAssemblyRegister(std::string_view constraint) noexcept
{
    const std::size_t open = constraint.find('{');
    if (open == std::string_view::npos || constraint.back() != '}') return {};
    return constraint.substr(open + 1, constraint.size() - open - 2);
}

bool isInlineAssemblyStackPointer(std::string_view name) noexcept
{
    return name == "rsp" || name == "esp" || name == "sp" || name == "spl";
}

bool isWindowsX64NonvolatileRegister(std::string_view name) noexcept
{
    static constexpr std::array<std::string_view, 28> names = {
        "rbx", "ebx", "bx", "bl", "bh",
        "rbp", "ebp", "bp", "bpl",
        "rdi", "edi", "di", "dil",
        "rsi", "esi", "si", "sil",
        "r12", "r12d", "r12w", "r12b",
        "r13", "r13d", "r13w", "r13b",
        "r14", "r15", "xmm6"};
    if (std::find(names.begin(), names.end(), name) != names.end()) return true;
    if (name.starts_with("r14") || name.starts_with("r15")) return true;
    if (name.starts_with("xmm"))
    {
        const std::string_view ordinal = name.substr(3);
        return ordinal == "7" || ordinal == "8" || ordinal == "9" || ordinal == "10" ||
            ordinal == "11" || ordinal == "12" || ordinal == "13" || ordinal == "14" ||
            ordinal == "15";
    }
    return false;
}

bool isInlineAssemblyConstraint(std::string_view constraint, bool output) noexcept
{
    if (!output)
    {
        if (constraint == "r" || constraint == "i" || constraint == "m") return true;
        return constraint.size() >= 3 && constraint.front() == '{' &&
            constraint.back() == '}' &&
            isInlineAssemblyRegisterName(constraint.substr(1, constraint.size() - 2));
    }

    if (constraint == "=r" || constraint == "+r" ||
        constraint == "=m" || constraint == "+m") return true;
    if (constraint.size() < 4 || (constraint.front() != '=' && constraint.front() != '+') ||
        constraint[1] != '{' || constraint.back() != '}')
    {
        return false;
    }
    return isInlineAssemblyRegisterName(constraint.substr(2, constraint.size() - 3));
}
}

ast::AstNode* Parser::parseStaticAssertStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();

    std::string message;
    if (consumeIf(TokenKind::Comma))
    {
        if (at(TokenKind::StringLiteral))
        {
            message = std::get<std::string>(consume().value());
        }
        else
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "static_assert message must be a string literal");
        }
    }
    expect(TokenKind::RightParen, "')'");
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StaticAssertStatement,
        tokenRangeFrom(beginIndex));
    statement->setFirst(condition);
    statement->setText(std::move(message));
    return statement;
}

ast::AstNode* Parser::parseInlineAssemblyStatement()
{
    const std::size_t beginIndex = index_;
    consume(); // contextual `asm`
    std::string targetMetadata;
    std::string dialectMetadata;
    parseInlineAssemblyHeader(targetMetadata, dialectMetadata);
    expect(TokenKind::LeftBrace, "'{' after asm");
    std::string exactBody;
    ast::AstNode* statement = arena_.make<ast::AstNode>(
        ast::AstNodeKind::InlineAssemblyStatement,
        tokenRangeFrom(beginIndex));
    if (!targetMetadata.empty() || !dialectMetadata.empty())
        statement->setAuxiliaryText(
            "target:" + targetMetadata + ";dialect:" + dialectMetadata);
    parseInlineAssemblyBody(statement, exactBody);
    expect(TokenKind::RightBrace, "'}' to close asm block");
    statement->setRange(tokenRangeFrom(beginIndex));
    statement->setText(std::move(exactBody));
    validateInlineAssemblyControlFlow(statement);
    parseInlineAssemblyOperandClauses(statement, beginIndex);
    validateInlineAssemblyPlaceholders(statement);
    return statement;
}

void Parser::parseInlineAssemblyHeader(
    std::string& targetMetadata,
    std::string& dialectMetadata)
{
    if (consumeIf(TokenKind::LeftParen))
    {
        do
        {
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "asm header option name");
                break;
            }
            const lexer::Token& optionToken = consume();
            const std::string option(optionToken.lexeme());
            if (!expect(TokenKind::Equal, "'=' after asm header option")) break;
            if (!at(TokenKind::Identifier))
            {
                expect(TokenKind::Identifier, "asm header option value");
                break;
            }
            const lexer::Token& valueToken = consume();
            const std::string value(valueToken.lexeme());

            if (option == "target" && value == "x64")
            {
                targetMetadata = std::string(target::TargetInfo::WindowsX64().llvmTriple());
            }
            else if (option == "dialect" && value == "intel")
            {
                dialectMetadata = "intel";
            }
            else
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
                    diagnostics::DiagnosticSeverity::Error,
                    valueToken.range(),
                    "unsupported asm " + option + " value '" + value + "'");
            }
        }
        while (consumeIf(TokenKind::Comma));
        expect(TokenKind::RightParen, "')' after asm header");
    }
}

void Parser::parseInlineAssemblyBody(ast::AstNode* statement, std::string& exactBody)
{
    // Item 288: the lexer owns the outer-brace depth and hands the parser exact physical-line
    // records. The parent retains the complete body byte-for-byte. Non-blank records become child
    // nodes whose range starts at the first significant byte, so subsequent operand diagnostics
    // can name the real instruction column without re-scanning the source file.
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        const lexer::Token& token = current();
        exactBody += token.lexeme();
        if (token.kind() == TokenKind::AssemblyText)
        {
            const std::string_view raw = token.lexeme();
            const std::size_t first = raw.find_first_not_of(" \t\v\f");
            if (first != std::string_view::npos)
            {
                const std::size_t last = raw.find_last_not_of(" \t\v\f");
                const SourceRange significantRange(
                    SourceLocation(token.range().begin().fileId(),
                        token.range().begin().offset() + static_cast<std::uint32_t>(first)),
                    SourceLocation(token.range().end().fileId(),
                        token.range().begin().offset() + static_cast<std::uint32_t>(last + 1)));
                ast::AstNode* line = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::InlineAssemblyLine,
                    significantRange);
                line->setText(std::string(raw.substr(first, last - first + 1)));
                statement->addChild(line);
            }
        }
        consume();
    }
}

void Parser::parseInlineAssemblyOperandClauses(
    ast::AstNode* statement,
    std::size_t beginIndex)
{
    bool hasOperandClauses = false;
    if (atContextualKeyword("out"))
    {
        parseInlineAssemblyOutputClause(statement);
        hasOperandClauses = true;
    }
    if (at(TokenKind::KwIn))
    {
        parseInlineAssemblyInputClause(statement);
        hasOperandClauses = true;
    }
    if (atContextualKeyword("clobber"))
    {
        parseInlineAssemblyClobberClause(statement);
        hasOperandClauses = true;
    }
    if (hasOperandClauses)
    {
        expect(TokenKind::Semicolon, "';' after asm operand clauses");
        statement->setRange(tokenRangeFrom(beginIndex));
    }
}

void Parser::validateInlineAssemblyPlaceholders(ast::AstNode* statement)
{
    std::size_t inputCount = 0;
    std::size_t outputCount = 0;
    for (const ast::AstNode* child : statement->children())
    {
        inputCount += child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand ? 1U : 0U;
        outputCount += child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand ? 1U : 0U;
    }

    for (const ast::AstNode* line : statement->children())
    {
        if (line->kind() != ast::AstNodeKind::InlineAssemblyLine) continue;
        std::string_view code = line->text();
        if (const std::size_t comment = code.find(';'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        for (std::size_t index = 0; index < code.size(); ++index)
        {
            if (code[index] != '$') continue;
            if (index + 1 < code.size() && code[index + 1] == '$')
            {
                ++index;
                continue;
            }

            const bool output = code.substr(index + 1, 3) == "out";
            std::size_t cursor = index + (output ? 4 : 1);
            bool resolved = false;
            if (!output && cursor < code.size() &&
                (std::isalpha(static_cast<unsigned char>(code[cursor])) || code[cursor] == '_'))
            {
                const std::size_t nameBegin = cursor++;
                while (cursor < code.size() &&
                    (std::isalnum(static_cast<unsigned char>(code[cursor])) || code[cursor] == '_'))
                {
                    ++cursor;
                }
                const std::string_view name = code.substr(nameBegin, cursor - nameBegin);
                resolved = std::any_of(statement->children().begin(), statement->children().end(),
                    [name](const ast::AstNode* child)
                    {
                        return child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand &&
                            child->first() != nullptr && child->first()->text() == name;
                    });
            }
            else
            {
                std::size_t ordinal = 0;
                const std::size_t digitBegin = cursor;
                while (cursor < code.size() &&
                    std::isdigit(static_cast<unsigned char>(code[cursor])))
                {
                    ordinal = ordinal * 10 + static_cast<std::size_t>(code[cursor] - '0');
                    ++cursor;
                }
                const std::size_t bound = output ? outputCount : inputCount;
                resolved = cursor != digitBegin && ordinal < bound;
            }
            const bool malformedSuffix = cursor < code.size() &&
                (std::isalnum(static_cast<unsigned char>(code[cursor])) || code[cursor] == '_');
            if (!resolved || malformedSuffix)
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidInlineAssemblyPlaceholder,
                    diagnostics::DiagnosticSeverity::Error,
                    line->range(),
                    output
                        ? "asm output placeholder does not name a declared out operand"
                        : "asm input placeholder does not name a declared in operand");
                break;
            }
            index = cursor - 1;
        }
    }
}

void Parser::validateInlineAssemblyControlFlow(ast::AstNode* statement)
{
    std::unordered_set<std::string> labels;
    for (ast::AstNode* line : statement->children())
    {
        if (line->kind() != ast::AstNodeKind::InlineAssemblyLine) continue;
        std::string_view code = line->text();
        if (const std::size_t comment = code.find(';'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        code = trimAssemblyText(code);
        if (code.empty() || code.back() != ':') continue;
        const std::string label = canonicalInlineAssemblyRegister(
            std::string(trimAssemblyText(code.substr(0, code.size() - 1))));
        if (!isInlineAssemblyRegisterName(label) || !labels.insert(label).second)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyControlFlow,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm label must be a unique identifier within its block");
            continue;
        }
        line->setAuxiliaryText("label:" + label);
    }

    for (ast::AstNode* line : statement->children())
    {
        if (line->kind() != ast::AstNodeKind::InlineAssemblyLine ||
            line->auxiliaryText().starts_with("label:")) continue;
        std::string_view code = line->text();
        if (const std::size_t comment = code.find(';'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        code = trimAssemblyText(code);
        if (code.empty()) continue;
        const std::size_t separator = code.find_first_of(" \t");
        const std::string mnemonic = canonicalInlineAssemblyRegister(
            std::string(code.substr(0, separator)));
        std::string_view target = separator == std::string_view::npos
            ? std::string_view{} : trimAssemblyText(code.substr(separator + 1));
        if (mnemonic == "ret" || mnemonic == "retq" || mnemonic.starts_with("iret") ||
            mnemonic == "call" || mnemonic == "callq")
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyControlFlow,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm call/return instructions cannot escape NativeSharp structured control flow");
            continue;
        }
        const target::TargetInfo targetInfo = target::TargetInfo::WindowsX64();
        const std::string_view requiredFeature = targetInfo.requiredAssemblyFeature(mnemonic);
        if (!targetInfo.supportsCpuFeature(requiredFeature))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm instruction '" + mnemonic + "' requires CPU feature '" +
                    std::string(requiredFeature) + "' outside baseline " +
                    std::string(targetInfo.cpuBaselineName()));
            continue;
        }
        if (!isInlineAssemblyBranchMnemonic(mnemonic)) continue;
        const std::string canonicalTarget = canonicalInlineAssemblyRegister(std::string(target));
        if (!isInlineAssemblyRegisterName(canonicalTarget) || !labels.contains(canonicalTarget))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyControlFlow,
                diagnostics::DiagnosticSeverity::Error,
                line->range(),
                "asm branch target must be a label declared in the same block");
            continue;
        }
        line->setAuxiliaryText("branch:" + mnemonic + ":" + canonicalTarget);
    }
}

void Parser::parseInlineAssemblyOutputClause(ast::AstNode* statement)
{
    consume(); // contextual `out`
    if (!expect(TokenKind::LeftParen, "'(' after asm out")) return;
    if (at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm out clause requires at least one output target");
        consume();
        return;
    }

    std::uint64_t ordinal = 0;
    do
    {
        const std::size_t operandBegin = index_;
        ast::AstNode* target = parseUnaryExpression();
        ast::AstNode* operand = arena_.make<ast::AstNode>(
            ast::AstNodeKind::InlineAssemblyOutputOperand,
            tokenRangeFrom(operandBegin));
        operand->setFirst(target);
        operand->setIntegerValue(ordinal++);
        bool explicitConstraint = false;
        operand->setAuxiliaryText(
            parseInlineAssemblyConstraint(true, explicitConstraint));
        operand->setFlag(explicitConstraint);
        operand->setRange(tokenRangeFrom(operandBegin));
        statement->addChild(operand);
    }
    while (consumeIf(TokenKind::Comma));

    expect(TokenKind::RightParen, "')' after asm output targets");
}

void Parser::parseInlineAssemblyInputClause(ast::AstNode* statement)
{
    consume(); // `in`
    if (!expect(TokenKind::LeftParen, "'(' after asm in")) return;
    if (at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm in clause requires at least one input expression");
        consume();
        return;
    }

    std::uint64_t ordinal = 0;
    do
    {
        const std::size_t operandBegin = index_;
        ast::AstNode* expression = parseAssignmentExpression();
        ast::AstNode* operand = arena_.make<ast::AstNode>(
            ast::AstNodeKind::InlineAssemblyInputOperand,
            tokenRangeFrom(operandBegin));
        operand->setFirst(expression);
        operand->setIntegerValue(ordinal++);
        bool explicitConstraint = false;
        operand->setAuxiliaryText(
            parseInlineAssemblyConstraint(false, explicitConstraint));
        operand->setFlag(explicitConstraint);
        operand->setRange(tokenRangeFrom(operandBegin));
        statement->addChild(operand);
    }
    while (consumeIf(TokenKind::Comma));

    expect(TokenKind::RightParen, "')' after asm input operands");
}

void Parser::parseInlineAssemblyClobberClause(ast::AstNode* statement)
{
    consume(); // contextual `clobber`
    if (!expect(TokenKind::LeftParen, "'(' after asm clobber")) return;
    if (at(TokenKind::RightParen))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm clobber clause requires at least one register name");
        consume();
        return;
    }

    std::unordered_set<std::string> seen;
    std::uint64_t ordinal = 0;
    do
    {
        if (!at(TokenKind::StringLiteral))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "asm register clobber must be a string literal");
            if (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile)) consume();
            continue;
        }

        const lexer::Token& registerToken = consume();
        const std::string registerName = canonicalInlineAssemblyRegister(
            std::get<std::string>(registerToken.value()));
        const bool specialClobber = registerName == "cc" || registerName == "memory";
        if (validateInlineAssemblyClobberName(
                statement, registerToken, registerName, specialClobber, seen))
            appendInlineAssemblyClobber(
                statement, registerToken, registerName, specialClobber, ordinal++);
    }
    while (consumeIf(TokenKind::Comma));

    expect(TokenKind::RightParen, "')' after asm clobbers");
}

bool Parser::validateInlineAssemblyClobberName(
    ast::AstNode* statement,
    const lexer::Token& registerToken,
    const std::string& registerName,
    bool specialClobber,
    std::unordered_set<std::string>& seen)
{
    if (!specialClobber && !isInlineAssemblyRegisterName(registerName))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
            diagnostics::DiagnosticSeverity::Error,
            registerToken.range(),
            "asm clobber requires a register atom, 'cc', or 'memory'");
        return false;
    }
    if (!specialClobber && isInlineAssemblyStackPointer(registerName))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
            diagnostics::DiagnosticSeverity::Error,
            registerToken.range(),
            "asm cannot clobber the stack pointer; its value and alignment must survive the block");
        return false;
    }
    if (!specialClobber && !target::TargetInfo::WindowsX64().isAssemblyRegister(registerName))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
            diagnostics::DiagnosticSeverity::Error,
            registerToken.range(),
            "unknown x64 register '" + registerName + "'");
        return false;
    }
    if (!seen.insert(registerName).second)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
            diagnostics::DiagnosticSeverity::Error,
            registerToken.range(),
            "duplicate asm register clobber '" + registerName + "'");
        return false;
    }
    if (specialClobber) return true;
    for (const ast::AstNode* child : statement->children())
    {
        if (child->kind() != ast::AstNodeKind::InlineAssemblyInputOperand &&
            child->kind() != ast::AstNodeKind::InlineAssemblyOutputOperand)
            continue;
        const std::string fixed = canonicalInlineAssemblyRegister(
            std::string(fixedInlineAssemblyRegister(child->auxiliaryText())));
        if (fixed.empty() || fixed != registerName) continue;
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyClobber,
            diagnostics::DiagnosticSeverity::Error,
            registerToken.range(),
            "asm register '" + registerName + "' cannot be both a fixed operand and a clobber");
        return false;
    }
    return true;
}

void Parser::appendInlineAssemblyClobber(
    ast::AstNode* statement,
    const lexer::Token& registerToken,
    const std::string& registerName,
    bool specialClobber,
    std::uint64_t ordinal)
{
    ast::AstNode* clobber = arena_.make<ast::AstNode>(
        ast::AstNodeKind::InlineAssemblyClobber,
        registerToken.range());
    clobber->setText(registerName);
    clobber->setIntegerValue(ordinal);
    clobber->setAuxiliaryText(
        registerName == "cc" ? "flags" :
        registerName == "memory" ? "memory" : "register");
    clobber->setFlag(!specialClobber && isWindowsX64NonvolatileRegister(registerName));
    statement->addChild(clobber);
}

std::string Parser::parseInlineAssemblyConstraint(
    bool output,
    bool& explicitConstraint)
{
    explicitConstraint = consumeIf(TokenKind::Colon);
    const std::string defaultConstraint = output ? "=r" : "r";
    if (!explicitConstraint) return defaultConstraint;

    if (!at(TokenKind::StringLiteral))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "asm operand constraint must be a string literal");
        return defaultConstraint;
    }

    const lexer::Token& constraintToken = consume();
    const std::string constraint = std::get<std::string>(constraintToken.value());
    if (!isInlineAssemblyConstraint(constraint, output))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
            diagnostics::DiagnosticSeverity::Error,
            constraintToken.range(),
            output
                ? "asm output constraint must be '=r', '+r', '=m', '+m', '={register}', or '+{register}'"
                : "asm input constraint must be 'r', 'i', 'm', or '{register}'");
        return defaultConstraint;
    }
    const std::string fixedRegister = canonicalInlineAssemblyRegister(
        std::string(fixedInlineAssemblyRegister(constraint)));
    if (!fixedRegister.empty() && isInlineAssemblyStackPointer(fixedRegister))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint,
            diagnostics::DiagnosticSeverity::Error,
            constraintToken.range(),
            "asm cannot bind an operand to the stack pointer");
        return defaultConstraint;
    }
    if (!fixedRegister.empty() &&
        !target::TargetInfo::WindowsX64().isAssemblyRegister(fixedRegister))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidInlineAssemblyTarget,
            diagnostics::DiagnosticSeverity::Error,
            constraintToken.range(),
            "unknown x64 register '" + fixedRegister + "'");
        return defaultConstraint;
    }
    return constraint;
}

ast::AstNode* Parser::parseVariableDeclarator(
    ast::TypeSyntax* type,
    bool cloneType,
    bool isConst,
    bool isRaii,
    bool isVolatile,
    bool isReferenceLocal,
    bool isPinned)
{
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "variable name");
        return nullptr;
    }

    const std::size_t beginIndex = index_;
    const lexer::Token& name = consume();
    ast::AstNode* variable = arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement,
        name.range());
    variable->setTypeSyntax(cloneType ? cloneDeclarationType(type) : type);
    variable->setText(std::string(name.lexeme()));
    variable->setFlag(isConst);
    if (isRaii) variable->addModifier(TokenKind::KwRaii);
    if (isVolatile) variable->addModifier(TokenKind::KwVolatile);
    if (isReferenceLocal)
        variable->setAuxiliaryText(isPinned ? "local:pinned-ref" : "local:ref");
    else if (isPinned) variable->setAuxiliaryText("local:pinned");

    if (consumeIf(TokenKind::Equal))
    {
        if (isReferenceLocal)
        {
            expectContextualKeyword("ref", "'ref' before the reference-local initializer");
        }
        variable->setFirst(
            at(TokenKind::LeftBrace) ? parseAggregateInitializer() : parseExpression());
    }
    else if (isReferenceLocal)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "reference-local declaration requires '= ref <storage>'");
    }
    else if (isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "const declaration requires an initializer");
    }
    variable->setRange(tokenRangeFrom(beginIndex));
    return variable;
}

ast::AstNode* Parser::parseVariableDeclaration(bool expectSemicolon, bool isStaticLocal)
{
    const std::size_t beginIndex = index_;
    const bool isPinnedReferenceSyntax = atContextualKeyword("pin") &&
        peek().kind() == TokenKind::Identifier && peek().lexeme() == "ref";
    const bool isPinned = isPinnedReferenceSyntax ||
        (atContextualKeyword("pin") && peek().kind() == TokenKind::KwRaii);
    if (isPinned) consume();
    const bool isReferenceLocal = atContextualKeyword("ref") &&
        (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar ||
         peek().kind() == TokenKind::KwConst || peek().kind() == TokenKind::KwRaii);
    if (isReferenceLocal) consume();
    const bool isRaii = consumeIf(TokenKind::KwRaii);
    const bool isVolatile = consumeIf(TokenKind::KwVolatile);
    const bool isConst = consumeIf(TokenKind::KwConst);
    // NS304-T13-F03: `scope Tipo x = ...` — contextual frame-storage modifier (no reserved keyword).
    const bool isScope = atContextualKeyword("scope") &&
        (canStartTypeAt(index_ + 1) || peek().kind() == TokenKind::KwVar);
    if (isScope) consume();
    if (isRaii && isConst)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            tokens_[beginIndex].range(),
            "raii locals cannot also be const");
    }
    // `var` is a full reserved keyword (see TokenKind::KwVar), not contextual like C# -- "var"
    // was unused as an identifier anywhere in the pre-existing codebase, so there is no
    // disambiguation to do. Deliberately bypasses parseType() entirely: a marker TypeSyntax with
    // baseKind == TokenKind::KwVar is constructed directly instead, which is why `var` is
    // syntactically illegal anywhere parseType() is called directly (fields, parameters, return
    // types, casts, generic arguments) -- the correct v1 scope, local-variable type inference
    // only. This also means `owned var`/`borrowed var` are not supported in v1: those qualifiers
    // are consumed by parseType()'s own prefix logic, which this path never reaches -- a
    // deliberate, documented limitation. `raii var`/`const var` DO work, since raii/const are
    // consumed above, before type-parsing begins.
    ast::TypeSyntax* type;
    if (at(TokenKind::KwVar))
    {
        if (isReferenceLocal)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "reference locals require an explicit element type");
        }
        const lexer::Token& varToken = consume();
        type = arena_.make<ast::TypeSyntax>(varToken.range(), TokenKind::KwVar, std::string("var"));
    }
    else
    {
        type = parseType();
    }

    return finishVariableDeclaration(
        beginIndex,
        type,
        expectSemicolon,
        isStaticLocal,
        isConst,
        isRaii,
        isVolatile,
        isReferenceLocal,
        isPinned,
        isScope);
}

ast::AstNode* Parser::finishVariableDeclaration(
    std::size_t beginIndex,
    ast::TypeSyntax* type,
    bool expectSemicolon,
    bool isStaticLocal,
    bool isConst,
    bool isRaii,
    bool isVolatile,
    bool isReferenceLocal,
    bool isPinned,
    bool isScope)
{
    ast::AstNode* variable = parseVariableDeclarator(
        type, false, isConst, isRaii, isVolatile, isReferenceLocal, isPinned);
    if (variable == nullptr)
    {
        synchronizeStatement();
        return nullptr;
    }
    if (isScope) variable->setAuxiliaryText("local:scope");
    if (isStaticLocal) variable->addModifier(TokenKind::KwStatic);
    ast::AstNode* declarationList = nullptr;
    while (consumeIf(TokenKind::Comma))
    {
        ast::AstNode* declarator = parseVariableDeclarator(
            type, true, isConst, isRaii, isVolatile, isReferenceLocal, isPinned);
        if (declarator == nullptr)
        {
            synchronizeStatement();
            break;
        }
        if (isStaticLocal) declarator->addModifier(TokenKind::KwStatic);
        if (declarationList == nullptr)
        {
            declarationList = arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement,
                variable->range());
            declarationList->setAuxiliaryText("declaration-list");
            declarationList->addChild(variable);
        }
        declarationList->addChild(declarator);
    }

    if (expectSemicolon)
    {
        expect(TokenKind::Semicolon, "';'");
    }
    ast::AstNode* const result = declarationList == nullptr ? variable : declarationList;
    result->setRange(tokenRangeFrom(beginIndex));
    return result;
}

bool Parser::statementAlwaysDiverges(const ast::AstNode* statement) const
{
    using statement_divergence::Work;
    using statement_divergence::WorkKind;

    std::vector<Work> work{{WorkKind::Evaluate, statement}};
    std::vector<bool> values;
    while (!work.empty())
    {
        const Work current = work.back();
        work.pop_back();
        if (current.kind == WorkKind::ContinueIf)
        {
            const bool thenDiverges = values.back();
            values.pop_back();
            if (!thenDiverges)
            {
                values.push_back(false);
                continue;
            }
            work.push_back({WorkKind::Evaluate, current.node->third()});
            continue;
        }

        if (current.node == nullptr)
        {
            values.push_back(false);
            continue;
        }
        switch (current.node->kind())
        {
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::ThrowStatement:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ContinueStatement:
            values.push_back(true);
            break;
        case ast::AstNodeKind::BlockStatement:
            if (current.node->children().empty())
                values.push_back(false);
            else
                work.push_back({WorkKind::Evaluate, current.node->children().back()});
            break;
        case ast::AstNodeKind::IfStatement:
            if (current.node->second() == nullptr || current.node->third() == nullptr)
            {
                values.push_back(false);
                break;
            }
            work.push_back({WorkKind::ContinueIf, current.node});
            work.push_back({WorkKind::Evaluate, current.node->second()});
            break;
        default:
            values.push_back(false);
            break;
        }
    }
    return !values.empty() && values.back();
}

ast::AstNode* Parser::parseConditionalPatternStatement()
{
    return parseStructuredControlStatement();
}

Parser::ConditionalPatternAnalysis Parser::analyzeConditionalPattern(const Pattern& pattern)
{
    MatchPatternAnalysis facts = analyzeMatchExpressionPattern(pattern, false);
    for (const SourceRange& range : facts.nonAddressableBindingRanges)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "ref/move pattern binding cannot cross a tuple deconstruction or variant "
            "payload copy; bind the aggregate first");
    }
    for (const SourceRange& range : facts.invalidOrRanges)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "OR-pattern alternatives must bind identical names with identical "
            "value/ref/move modes");
    }

    ConditionalPatternAnalysis result;
    for (const auto& [name, mode] : facts.bindings)
    {
        static_cast<void>(name);
        result.hasReferenceBinding = result.hasReferenceBinding ||
            mode == PatternBindingMode::Reference ||
            mode == PatternBindingMode::ReferenceMutable;
        result.hasMoveBinding = result.hasMoveBinding || mode == PatternBindingMode::Move;
    }
    return result;
}

void Parser::parseIfHeader(IfParseState& state)
{
    state.beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    if (looksLikeConditionDeclaration())
    {
        state.conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            state.conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            state.condition = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        state.condition = parseExpression();
    }
    expect(TokenKind::RightParen, "')'");
}

ast::AstNode* Parser::lowerIfStatement(const IfParseState& state, ast::AstNode* elseNode)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, tokenRangeFrom(state.beginIndex));
    node->setFirst(state.condition);
    node->setSecond(state.thenBlock);
    node->setThird(elseNode);
    if (state.conditionDeclaration == nullptr) return node;

    ast::AstNode* lowered = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement,
        tokenRangeFrom(state.beginIndex));
    lowered->setAuxiliaryText("condition-declaration");
    lowered->addChild(state.conditionDeclaration);
    lowered->addChild(node);
    return lowered;
}

ast::AstNode* Parser::parseIfStatement()
{
    return parseStructuredControlStatement();
}


ast::AstNode* Parser::parseStructuredControlStatement()
{
    return StructuredControlParser(*this).parse();
}

ast::AstNode* Parser::parseEmbeddedStatementBlock()
{
    if (at(TokenKind::LeftBrace)) return parseBlock();

    // In the embedded-statement grammar a lone semicolon is the empty statement. Represent it
    // as the same empty block downstream already understands; this keeps flow, cleanup, ranges,
    // and dangling-else handling identical without introducing a backend-only AST node.
    if (at(TokenKind::Semicolon))
    {
        const SourceRange range = consume().range();
        return arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    }

    // T29 keeps every downstream invariant intact: the parser accepts one embedded statement,
    // then gives semantic analysis/HIR the same BlockStatement shape produced by explicit braces.
    // Nested `if` is parsed before it is wrapped, so its `else` binds to the nearest open `if`.
    ast::AstNode* statement = parseStatement();
    if (statement != nullptr &&
        (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement ||
         statement->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement ||
         (statement->kind() == ast::AstNodeKind::BlockStatement &&
          statement->auxiliaryText() == "conditional-pattern-inline")))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "a declaration used as an if or else body requires an explicit '{ ... }' block");
    }
    const SourceRange range = statement != nullptr ? statement->range() : current().range();
    ast::AstNode* block = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    if (statement != nullptr) block->addChild(statement);
    return block;
}

void Parser::parseWhileHeader(WhileParseState& state)
{
    state.beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");
    if (looksLikeConditionDeclaration())
    {
        state.conditionDeclaration = parseVariableDeclaration(false);
        const ast::AstNode* binding = conditionDeclarationBinding(
            state.conditionDeclaration, diagnostics_);
        if (binding != nullptr)
        {
            state.condition = makeIdentifierExpression(binding->range(), binding->text());
        }
    }
    else
    {
        state.condition = parseExpression();
    }
    expect(TokenKind::RightParen, "')'");
}

ast::AstNode* Parser::lowerWhileStatement(
    const WhileParseState& state,
    ast::AstNode* body)
{
    const SourceRange statementRange = tokenRangeFrom(state.beginIndex);
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::WhileStatement, statementRange);
    if (state.conditionDeclaration == nullptr)
    {
        node->setFirst(state.condition);
        node->setSecond(body);
        return node;
    }

    // A while condition declaration is evaluated once per iteration. Lowering it outside the
    // loop would retain stale state and delay cleanup until after every iteration. This is the
    // same canonical shape used by `while let`: an infinite loop whose iteration scope owns the
    // declaration, exits when the declared bool is false, and then executes the original body.
    ast::AstNode* iteration = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statementRange);
    iteration->setAuxiliaryText("condition-declaration-iteration");
    iteration->addChild(state.conditionDeclaration);

    ast::AstNode* stopCondition = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, state.condition->range());
    stopCondition->setTokenKind(TokenKind::Bang);
    stopCondition->setFirst(state.condition);
    ast::AstNode* stopBody = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statementRange);
    stopBody->addChild(arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, state.condition->range()));
    ast::AstNode* stop = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, statementRange);
    stop->setFirst(stopCondition);
    stop->setSecond(stopBody);
    iteration->addChild(stop);
    if (body != nullptr)
    {
        iteration->addChild(body);
    }

    node->setFirst(makeBooleanLiteral(statementRange, true));
    node->setSecond(iteration);
    return node;
}

ast::AstNode* Parser::parseWhileStatement()
{
    return parseStructuredControlStatement();
}

// `do { ... } while (condition);`
//
// The node carries its operands in the SAME slots as a while statement -- first() is the
// condition, second() is the body -- even though the source writes them the other way round.
// That is deliberate: everything upstream of the backend (the body model's loop analysis, the
// ownership flow across the back edge, the HIR walk) treats the two loops identically, and the
// only real difference is that the body runs before the first test. Keeping the slots aligned
// means those passes share one implementation instead of two that must be kept in step.
//
// The trailing `;` is required, as in C and C#.
ast::AstNode* Parser::parseDoWhileStatement()
{
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::lowerDoWhileStatement(
    const DoWhileParseState& state,
    ast::AstNode* body)
{
    expect(TokenKind::KwWhile, "'while' after the do-loop body");
    expect(TokenKind::LeftParen, "'('");
    ast::AstNode* condition = parseExpression();
    expect(TokenKind::RightParen, "')'");
    expect(TokenKind::Semicolon, "';' after the do-while condition");

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DoWhileStatement,
        tokenRangeFrom(state.beginIndex));
    node->setFirst(condition);
    node->setSecond(body);
    return node;
}

bool Parser::atSwitchLabel() const noexcept
{
    if (at(TokenKind::KwCase)) return true;
    if (!at(TokenKind::KwDefault)) return false;
    return index_ + 1 < tokens_.size() && tokens_[index_ + 1].kind() == TokenKind::Colon;
}

// `switch (governing) { case a: case b: ...  default: ... }`
//
// SHAPE. The statement node keeps the governing expression in first() and one SwitchSection per
// section in children(), in source order. A section keeps its labels first and its statements
// after, with the label count in integerValue() -- so no consumer ever has to tell the two runs
// apart by inspecting node kinds, which is the sort of thing that silently stops working the day
// somebody adds a statement kind.
//
// NO FALL-THROUGH. A section that reaches its end leaves the switch; it does not slide into the
// next one. This is the C# rule rather than the C one, and it is the whole reason `case 1: case 2:`
// is expressed as two labels on ONE section instead of an empty section falling into the next:
// grouping is what people actually want out of fall-through, and it is the only part of it that
// never produces the accidental-fall-through bug. `break` stays legal and means the same thing --
// leave the switch now -- so existing habits keep working.
//
// A `case` value is parsed as a full expression. Whether it is a CONSTANT, whether it converts to
// the governing type, and whether it duplicates another label are semantic questions, answered in
// the body model where the constant folder and the type table live -- not here, where the answer
// would have to be guessed from syntax.
ast::AstNode* Parser::parseSwitchStatement()
{
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::parseMatchStatement()
{
    return parseStructuredControlStatement();
}
ast::AstNode* Parser::parseComprehensionYieldStatement()
{
    return parseWithContinuation(ContinuationRequestKind::Statement);
}

ast::AstNode* Parser::parseForExpressionList(std::size_t beginIndex)
{
    ast::AstNode* first = parseExpression();
    if (!at(TokenKind::Comma)) return first;

    ast::AstNode* sequence = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement,
        first != nullptr ? first->range() : current().range());
    if (first != nullptr) sequence->addChild(makeExpressionStatement(first));
    while (consumeIf(TokenKind::Comma))
    {
        ast::AstNode* expression = parseExpression();
        if (expression != nullptr) sequence->addChild(makeExpressionStatement(expression));
    }
    sequence->setRange(tokenRangeFrom(beginIndex));
    return sequence;
}

void Parser::parseForHeader(ForParseState& state)
{
    state.beginIndex = index_;
    consume();
    expect(TokenKind::LeftParen, "'('");

    if (!at(TokenKind::Semicolon))
    {
        if (at(TokenKind::KwRaii) || at(TokenKind::KwConst) || at(TokenKind::KwVolatile) ||
            at(TokenKind::KwVar) || canStartTypeAt(index_))
        {
            state.initializer = parseVariableDeclaration(false);
        }
        else
        {
            state.initializer = parseForExpressionList(state.beginIndex);
        }
    }
    expect(TokenKind::Semicolon, "';'");

    if (!at(TokenKind::Semicolon))
    {
        state.condition = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    if (!at(TokenKind::RightParen))
    {
        state.increment = parseForExpressionList(state.beginIndex);
    }
    expect(TokenKind::RightParen, "')'");
}

ast::AstNode* Parser::lowerForStatement(
    const ForParseState& state,
    ast::AstNode* body)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ForStatement,
        tokenRangeFrom(state.beginIndex));
    node->setFirst(state.initializer);
    node->setSecond(state.condition);
    node->setThird(state.increment);
    node->addChild(body);
    return node;
}

ast::AstNode* Parser::parseForStatement()
{
    return parseStructuredControlStatement();
}

// ---------------------------------------------------------------------------------------------
// foreach -- Stage 52.x, requested by Alberto separately from the Stage 52 closure round.
//
// DESIGN (documented in detail in 52_x_nativesharp_foreach.md; summarized here for anyone
// reading the parser code directly):
//
// NativeSharp deliberately does not require an `IEnumerable<T>`/`IEnumerator<T>` protocol for
// iteration. Generic interfaces are supported, but a universal enumeration contract would also
// need a universal object/value story that this native language does not promise. So `foreach`
// here is implemented exactly the
// way the real C# LANGUAGE SPECIFICATION actually defines it (not the common misconception):
// a compile-time, structural ("duck-typed") pattern -- an accessible zero-argument
// `GetEnumerator()` method returning something with `bool MoveNext()` and a zero-argument
// `Current()` accessor -- resolved entirely through ORDINARY member/overload lookup, the exact
// same lookup that resolves any other `.Method()` call in this compiler. No interfaces, no
// runtime dispatch, no reflection: everything is decided once, at compile time, per call site
// (consistent with `restricciones_arquitectonicas.md`'s "no reflection, no dynamic dispatch by
// name" rule) -- and it composes for free with the array case, which uses indexing/`.Length`
// instead of a method pattern.
//
// IMPLEMENTATION STRATEGY -- desugaring to already-existing, already-tested constructs:
//
// Rather than inventing new HIR/MIR/LLVM node kinds for foreach's control flow (the highest-risk
// part of any loop feature -- see WINE_EXECUTION_FINDINGS_STAGE52.md for how much subtlety
// already lives in this compiler's loop/RAII/cleanup-region lowering), `parseForeachStatement`
// desugars `foreach (T x in expr) body` at PARSE TIME into an ordinary nested-block AST built
// from the exact same node kinds (VariableDeclarationStatement, WhileStatement, MemberExpression,
// CallExpression, IndexExpression, BinaryExpression, AssignmentExpression) any hand-written
// NativeSharp program already produces -- using the SAME arena_.make<...>() calls the rest of
// this parser uses, not a separate synthesis mechanism. This means every later pipeline stage
// (ProjectMethodBodyModel's ownership/RAII-across-back-edge analysis, ProjectOopHirModel's HIR
// lowering, ProjectOopMirModel, ProjectOopLlvmLowerer) needs ZERO new code to handle a foreach
// loop's actual control flow -- it looks, to every one of those passes, exactly like a
// hand-written `while` loop, because it IS one, just synthesized instead of typed by hand.
//
// The one piece parsing genuinely cannot decide is array-mode vs. enumerator-pattern-mode --
// that requires the collection expression's resolved TYPE, which does not exist until semantic
// analysis (ProjectMethodBodyModel) runs. So the parser builds BOTH candidate desugarings
// unconditionally (structurally -- no type information needed to construct an AST node, only to
// analyze one) and stores them as two children of a new, thin `ForeachStatement` AST node:
//   first()  = "var __foreach_source = <original collection expression>;" -- evaluates the
//              collection exactly once (matching real C# foreach semantics), regardless of mode.
//   second() = the array-mode candidate block (index-based loop over __foreach_source[i]).
//   third()  = the enumerator-pattern candidate block (GetEnumerator/MoveNext/Current loop).
// `ProjectMethodBodyModel::analyzeForeach` (semantic analysis) is the ONLY place that ever
// decides which candidate is real: it resolves __foreach_source's type, and if it is an array
// type analyzes ONLY second(), otherwise attempts third() -- the discarded candidate is never
// visited by any analysis pass, so it can never contribute a spurious diagnostic. Both hidden
// locals (`__foreach_source`, and the array/pattern candidate's own hidden index-or-enumerator
// local) are declared with `var`, reusing this session's own `var` type-inference feature, so
// their concrete types (`builtin:int` for the index; whatever `GetEnumerator()` returns for the
// enumerator) never need to be known or spelled out at parse time either.
//
// The enumerator candidate marks its hidden local for type-directed lifetime handling. Semantic
// analysis has the resolved GetEnumerator() return type: reference enumerators become RAII owners
// exactly as before, while trivial struct enumerators remain values in the current stack frame.
// The parser cannot make that choice because it deliberately has no type information.
void Parser::initializeForeachState(ForeachParseState& state)
{
    state.beginIndex = index_;
    // Reserve the identity before parsing the body so a nested foreach receives a different
    // namespace without changing the names already chosen for its enclosing loop.
    state.temporaryPrefix =
        "$__foreach_" + std::to_string(nextMatchTemporary_++);
    state.sourceName = state.temporaryPrefix + "_source";
    state.indexName = state.temporaryPrefix + "_index";
    state.enumeratorName = state.temporaryPrefix + "_enumerator";
    state.rangeBeginName = state.temporaryPrefix + "_range_begin";
    state.rangeEndName = state.temporaryPrefix + "_range_end";
    state.rangeStepName = state.temporaryPrefix + "_range_step";
    state.rangeCursorName = state.temporaryPrefix + "_range_cursor";
    state.elementTemporaryName = state.temporaryPrefix + "_element";
    state.patternTemporaryPrefix = state.temporaryPrefix + "_pattern";
}

void Parser::parseForeachBindingPrefix(ForeachParseState& state)
{
    consume(); // foreach
    expect(TokenKind::LeftParen, "'('");

    // NativeSharp 3.0 T09-F01. The iteration binding mode is part of the foreach clause rather
    // than inferred from Current/indexer shape:
    //
    //   foreach (T value in source)          // value snapshot
    //   foreach (ref T value in source)      // readonly alias
    //   foreach (ref mut T value in source)  // exclusive mutable alias
    //
    // `ref`/`mut` stay contextual. The synthesized declaration below then
    // travels through the ordinary reference-local pipeline, so addressability, exact type,
    // lifetime, exclusivity and LLVM pointer lowering are shared with source ref locals.
    state.elementByReference = atContextualKeyword("ref");
    if (state.elementByReference)
    {
        consume();
        if (atContextualKeyword("mut"))
        {
            state.elementMutableReference = true;
            consume();
        }
    }

    // Element declaration: `raii` (optional, same grammar as an ordinary local), then a real
    // type or `var`, then the element name -- mirrors parseVariableDeclaration's own prefix
    // handling exactly, since this element genuinely IS an ordinary local once desugared.
    //
    // `const` is deliberately no longer accepted here (closure-review fix, P0.4 of
    // STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md: a keyword that parsed successfully and
    // was then silently discarded -- `const bool elementIsConst = consumeIf(...); (void)
    // elementIsConst;` -- is worse than not accepting it at all, since it silently promises a
    // guarantee it never delivers). Real C# does not accept `const` on a foreach iteration
    // variable either -- the variable is simply ALWAYS non-reassignable, unconditionally, which
    // is exactly what NativeSharp now does too (see the element declarations below: both
    // candidates pass `elementIsConst = true` unconditionally to makeVariableDeclaration, P0.3).
    // `foreach (const var x in ...)` is therefore now a plain syntax error ("expected type or
    // 'var'", since `const` is consumed by nothing here and parseType()/the `var` check both
    // reject it) rather than a silently-ignored no-op -- an honest failure instead of a false
    // promise.
    state.elementIsRaii = consumeIf(TokenKind::KwRaii);
    if (state.elementByReference && state.elementIsRaii)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "a foreach reference binding aliases source storage and cannot also be raii");
    }
}

bool Parser::parseForeachElement(ForeachParseState& state)
{
    if (at(TokenKind::KwVar))
    {
        const lexer::Token& varToken = consume();
        state.elementType =
            arena_.make<ast::TypeSyntax>(varToken.range(), TokenKind::KwVar, std::string("var"));
    }
    else
    {
        state.elementType = parseType();
    }

    state.elementDeconstruction = at(TokenKind::LeftParen);
    if (state.elementDeconstruction)
    {
        state.elementPattern = parsePattern(true);
        state.elementName = state.elementTemporaryName;
    }
    else if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "foreach element name");
        synchronizeStatement();
        return false;
    }
    else
    {
        state.elementName = std::string(consume().lexeme());
    }
    expect(TokenKind::KwIn, "'in'");
    return true;
}

bool Parser::parseForeachHeaderPrefix(ForeachParseState& state)
{
    initializeForeachState(state);
    parseForeachBindingPrefix(state);
    return parseForeachElement(state);
}

bool Parser::parseForeachHeader(ForeachParseState& state)
{
    if (!parseForeachHeaderPrefix(state)) return false;
    // P0.2 of the closure review: the collection expression is parsed TWICE from the same token
    // range -- once here (`collection`, embedded in the REAL, semantically-analyzed
    // `sourceDecl` below) and once more, immediately after resetting `index_` back to this
    // point, into a completely separate, structurally-identical-but-distinct AST subtree
    // (`collectionProbe`). `collectionProbe` is never lowered or executed -- it exists only so
    // `ProjectMethodBodyModel::analyzeForeach` can run a throwaway `analyzeExpression` probe on
    // it (with a scratch, discarded DiagnosticEngine) to learn the collection's ownership shape
    // BEFORE deciding whether `sourceDecl` needs to be `raii`. Probing the SAME node twice would
    // double-register that node's bindings in the semantic model (name lookups, call bindings,
    // ...); probing a genuinely separate, unreferenced copy instead has zero effect on the real
    // analysis path. This is the same "parse-time desugaring, decide later once types are known"
    // philosophy the array-vs-enumerator candidate split already uses, applied one layer earlier.
    const std::size_t collectionStart = index_;
    state.collection = parseExpression();
    const std::size_t collectionEnd = index_;
    index_ = collectionStart;
    state.collectionProbe = parseExpression();
    index_ = collectionEnd;
    expect(TokenKind::RightParen, "')'");
    return true;
}

ast::AstNode* Parser::parseForeachStatement()
{
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::lowerForeachStatement(ForeachParseState&& state, ast::AstNode* body)
{
    const std::size_t beginIndex = state.beginIndex;
    const std::string& sourceName = state.sourceName;
    const bool elementByReference = state.elementByReference;
    const bool elementMutableReference = state.elementMutableReference;
    ast::AstNode* collection = state.collection;
    ast::AstNode* collectionProbe = state.collectionProbe;
    const SourceRange range = tokenRangeFrom(beginIndex);
    if (ast::AstNode* lowered = lowerForeachRange(state, body, range)) return lowered;

    // first(): `var $__foreach_<id>_source = <collection>;` -- evaluates the collection expression
    // exactly once, regardless of which candidate below ends up chosen. Whether this ends up
    // `raii` is decided by ProjectMethodBodyModel::analyzeForeach (P0.2, see above) using the
    // `collectionProbe` copy stashed as this ForeachStatement node's own extra child below --
    // NOT here, since the parser has no type information yet. Built as plain (non-raii) `var`
    // here; analyzeForeach adds the `raii` modifier onto THIS node in place when the probe finds
    // the source is a genuinely owned-returning expression.
    //
    // The `$` in every synthesized identifier below (P0.1 of the closure review, "higiene de
    // temporales"): NativeSharp's lexer only ever accepts `[A-Za-z_][A-Za-z0-9_]*` for a
    // user-written identifier (see Lexer.cpp's isIdentifierStart/isIdentifierPart) -- `$` can
    // never appear in ANY identifier a real NativeSharp source file could lex. Every name
    // synthesized by this function therefore contains `$`, which makes it categorically
    // impossible for user source (however it's spelled) to ever resolve to, shadow, or reassign
    // one of these compiler-internal locals through ordinary name lookup -- true lexical hygiene,
    // not merely an "unlikely enough" naming convention. `$` is also a legal, unquoted LLVM IR
    // identifier character, so this needs no companion change anywhere in HIR/MIR/LLVM lowering
    // or in the debug names those layers emit.
    ast::AstNode* sourceDecl = makeVariableDeclaration(makeForeachVarMarker(range), sourceName, collection, false);

    ast::AstNode* arrayCandidate = makeForeachArrayCandidate(state, body, range);

    ast::AstNode* spanCandidate = makeForeachSpanCandidate(state, body, range);

    ast::AstNode* patternCandidate = makeForeachPatternCandidate(state, body, range);

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ForeachStatement, range);
    node->setFirst(sourceDecl);
    node->setSecond(arrayCandidate);
    node->setThird(patternCandidate);
    // Fourth association (P0.2): the throwaway, never-lowered probe copy of the collection
    // expression, attached as an ordinary extra child so ProjectMethodBodyModel::analyzeForeach
    // can reach it via node->children()[0] to decide sourceDecl's raii-ness. See the long comment
    // above collectionProbe's construction for why this needs to be a SEPARATE parse rather than
    // a second analysis pass over `collection` itself.
    node->addChild(collectionProbe);
    node->addChild(spanCandidate);
    node->setAuxiliaryText(elementByReference
        ? (elementMutableReference ? "foreach:ref-mut" : "foreach:ref")
        : "foreach:value");
    return node;
}

ast::TypeSyntax* Parser::makeForeachVarMarker(SourceRange range)
{
    return arena_.make<ast::TypeSyntax>(range, TokenKind::KwVar, std::string("var"));
}

void Parser::configureForeachElementBinding(
    const ForeachParseState& state,
    ast::AstNode* declaration)
{
    if (!state.elementByReference || declaration == nullptr) return;
    declaration->setAuxiliaryText(
        state.elementMutableReference ? "local:foreach-ref-mut" : "local:foreach-ref");
}

void Parser::emitForeachElementBindings(
    const ForeachParseState& state,
    SourceRange range,
    ast::AstNode* destination)
{
    if (!state.elementDeconstruction || destination == nullptr) return;
    PatternExpressionFactory source = [&]()
    {
        return makeIdentifierExpression(range, state.elementName);
    };
    emitPatternBindings(
        state.elementPattern,
        source,
        destination,
        state.patternTemporaryPrefix);
}

ast::AstNode* Parser::lowerForeachRange(
    ForeachParseState& state,
    ast::AstNode* body,
    SourceRange range)
{
    ast::AstNode* collection = state.collection;
    // A bounded integer RangeExpression is a storage-free foreach source. Lower it immediately
    // to ordinary scalar locals plus a ForStatement so HIR/MIR/LLVM reuse the verified loop,
    // arithmetic, break and continue paths. Begin, end, and step are bound exactly once from left
    // to right. No array, list, or enumerator is materialized.
    if (collection != nullptr && collection->kind() == ast::AstNodeKind::RangeExpression)
    {
        reportInvalidForeachRangeReference(state, collection);
        ast::AstNode* begin = collection->first();
        ast::AstNode* end = collection->second();
        ast::AstNode* step = collection->third();
        if (begin == nullptr)
            begin = makeIntegerLiteralExpression(range, 0);
        if (end == nullptr)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidIndex,
                diagnostics::DiagnosticSeverity::Error,
                collection->range(),
                "foreach integer range requires a finite end endpoint");
            end = makeIntegerLiteralExpression(range, 0);
        }
        if (begin->kind() == ast::AstNodeKind::FromEndExpression ||
            end->kind() == ast::AstNodeKind::FromEndExpression)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidIndex,
                diagnostics::DiagnosticSeverity::Error,
                collection->range(),
                "a from-end endpoint requires a sized receiver and cannot bound a foreach integer range");
        }
        if (step == nullptr) step = makeIntegerLiteralExpression(range, 1);
        return buildForeachRangeLoop(state, body, range, collection, begin, end, step);
    }

    return nullptr;
}

ast::AstNode* Parser::buildForeachRangeLoop(
    ForeachParseState& state,
    ast::AstNode* body,
    SourceRange range,
    ast::AstNode* collection,
    ast::AstNode* begin,
    ast::AstNode* end,
    ast::AstNode* step)
{
    const std::string& rangeBeginName = state.rangeBeginName;
    const std::string& rangeEndName = state.rangeEndName;
    const std::string& rangeStepName = state.rangeStepName;
    const std::string& rangeCursorName = state.rangeCursorName;
    const std::string& elementName = state.elementName;
    ast::TypeSyntax* elementType = state.elementType;
    const bool elementMutableReference = state.elementMutableReference;
    const bool elementIsRaii = state.elementIsRaii;
    ast::AstNode* beginDecl = makeVariableDeclaration(
        makeForeachVarMarker(range), rangeBeginName, begin, false, true);
    ast::AstNode* endDecl = makeVariableDeclaration(
        makeForeachVarMarker(range), rangeEndName, end, false, true);
    ast::AstNode* stepDecl = makeVariableDeclaration(
        makeForeachVarMarker(range), rangeStepName, step, false, true);
    ast::AstNode* cursorDecl = makeVariableDeclaration(
        makeForeachVarMarker(range), rangeCursorName,
        makeIdentifierExpression(range, rangeBeginName), false);

    ast::AstNode* condition = buildForeachRangeCondition(state, range, collection);
    ast::AstNode* increment = makeAssignmentExpression(
        makeIdentifierExpression(range, rangeCursorName),
        makeBinaryExpression(
            TokenKind::Plus,
            makeIdentifierExpression(range, rangeCursorName),
            makeIdentifierExpression(range, rangeStepName)));
    ast::AstNode* elementDecl = makeVariableDeclaration(
        elementType,
        elementName,
        makeIdentifierExpression(range, rangeCursorName),
        elementIsRaii,
        /*isConst=*/!elementMutableReference);
    configureForeachElementBinding(state, elementDecl);
    ast::AstNode* forBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    forBody->addChild(elementDecl);
    emitForeachElementBindings(state, range, forBody);
    forBody->addChild(body);
    ast::AstNode* loop = arena_.make<ast::AstNode>(ast::AstNodeKind::ForStatement, range);
    loop->setFirst(cursorDecl);
    loop->setSecond(condition);
    loop->setThird(increment);
    loop->addChild(forBody);
    ast::AstNode* lowered = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    lowered->setAuxiliaryText(collection->flag()
        ? "foreach:integer-range-inclusive" : "foreach:integer-range-exclusive");
    lowered->addChild(beginDecl);
    lowered->addChild(endDecl);
    lowered->addChild(stepDecl);

    appendForeachRangeAssertion(state, range, lowered);
    lowered->addChild(loop);
    return lowered;
}

ast::AstNode* Parser::buildForeachRangeCondition(
    const ForeachParseState& state,
    SourceRange range,
    const ast::AstNode* collection)
{
    const auto id = [&](const std::string& name)
    {
        return makeIdentifierExpression(range, name);
    };
    ast::AstNode* ascending = makeBinaryExpression(
        TokenKind::AmpAmp,
        makeBinaryExpression(TokenKind::Greater, id(state.rangeStepName),
            makeIntegerLiteralExpression(range, 0)),
        makeBinaryExpression(collection->flag() ? TokenKind::LessEqual : TokenKind::Less,
            id(state.rangeCursorName), id(state.rangeEndName)));
    ast::AstNode* descending = makeBinaryExpression(
        TokenKind::AmpAmp,
        makeBinaryExpression(TokenKind::Less, id(state.rangeStepName),
            makeIntegerLiteralExpression(range, 0)),
        makeBinaryExpression(collection->flag() ? TokenKind::GreaterEqual : TokenKind::Greater,
            id(state.rangeCursorName), id(state.rangeEndName)));
    return makeBinaryExpression(TokenKind::PipePipe, ascending, descending);
}

void Parser::reportInvalidForeachRangeReference(
    const ForeachParseState& state,
    const ast::AstNode* collection)
{
    if (!state.elementByReference) return;
    diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        collection->range(),
        "foreach reference iteration requires stable source storage; an integer range produces values");
}

void Parser::appendForeachRangeAssertion(
    const ForeachParseState& state,
    SourceRange range,
    ast::AstNode* lowered)
{
    const auto id = [&](const std::string& name)
    {
        return makeIdentifierExpression(range, name);
    };
    ast::AstNode* positive = makeBinaryExpression(
        TokenKind::AmpAmp,
        makeBinaryExpression(TokenKind::Less, id(state.rangeBeginName), id(state.rangeEndName)),
        makeBinaryExpression(TokenKind::Greater, id(state.rangeStepName),
            makeIntegerLiteralExpression(range, 0)));
    ast::AstNode* negative = makeBinaryExpression(
        TokenKind::AmpAmp,
        makeBinaryExpression(TokenKind::Greater, id(state.rangeBeginName), id(state.rangeEndName)),
        makeBinaryExpression(TokenKind::Less, id(state.rangeStepName),
            makeIntegerLiteralExpression(range, 0)));
    ast::AstNode* equal = makeBinaryExpression(
        TokenKind::EqualEqual,
        id(state.rangeBeginName),
        id(state.rangeEndName));
    ast::AstNode* validDirection = makeBinaryExpression(
        TokenKind::PipePipe,
        positive,
        makeBinaryExpression(TokenKind::PipePipe, negative, equal));
    ast::AstNode* nonZero = makeBinaryExpression(
        TokenKind::BangEqual,
        id(state.rangeStepName),
        makeIntegerLiteralExpression(range, 0));
    ast::AstNode* assertion = makeZeroArgCallExpression(id("assert"));
    assertion->setAuxiliaryText("builtin-assert");
    assertion->addChild(makeBinaryExpression(TokenKind::AmpAmp, nonZero, validDirection));
    lowered->addChild(makeExpressionStatement(assertion));
}

ast::AstNode* Parser::makeForeachArrayCandidate(
    const ForeachParseState& state,
    ast::AstNode* body,
    SourceRange range)
{
    const std::string& sourceName = state.sourceName;
    const std::string& indexName = state.indexName;
    const std::string& elementName = state.elementName;
    ast::TypeSyntax* elementType = state.elementType;
    const bool elementIsRaii = state.elementIsRaii;
    const bool elementMutableReference = state.elementMutableReference;
    // second(): the array-mode candidate.
    //   for (int __foreach_index = 0; __foreach_index < __foreach_source.Length;
    //        __foreach_index = __foreach_index + 1) {
    //       <ElementType> <name> = __foreach_source[__foreach_index];
    //       { body }
    //   }
    //
    // This is built as a REAL ForStatement (first()=init, second()=condition, third()=increment,
    // body=the single child block) rather than a hand-spliced `while` with the increment tacked on
    // as a trailing statement inside the body. That distinction is not cosmetic: a `continue`
    // inside a WhileStatement's body jumps straight to the condition check, so a trailing
    // increment statement placed after the (possibly `continue`-skipped) loop body would simply
    // never run whenever the caller's body used `continue` -- an infinite loop on the same index,
    // caught by the Wine execution smoke test in 52_x_nativesharp_foreach.md. A ForStatement's
    // `continue` already jumps to the DEDICATED increment slot instead (see
    // ProjectOopHirModel::lowerStatement's ForStatement case and
    // ProjectOopLlvmLowerer::lowerFor's `incBlock`, both pre-existing and unmodified by foreach),
    // so building this candidate out of the SAME node kind real `for` loops use gets correct
    // continue semantics for free, with no foreach-specific control-flow code anywhere.
    ast::AstNode* arrayCandidate;
    {
        ast::TypeSyntax* intType = arena_.make<ast::TypeSyntax>(range, TokenKind::KwInt, std::string("int"));
        ast::AstNode* indexDecl = makeVariableDeclaration(
            intType, indexName, makeIntegerLiteralExpression(range, 0), false);

        ast::AstNode* condition = makeBinaryExpression(
            TokenKind::Less,
            makeIdentifierExpression(range, indexName),
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "Length"));

        ast::AstNode* increment = makeAssignmentExpression(
            makeIdentifierExpression(range, indexName),
            makeBinaryExpression(
                TokenKind::Plus,
                makeIdentifierExpression(range, indexName),
                makeIntegerLiteralExpression(range, 1)));

        // isConst=true (P0.3 of the closure review): the foreach element is unconditionally
        // read-only, exactly like real C#'s iteration variable. Reuses the SAME enforcement
        // mechanism an ordinary `const T x = ...;` local already relies on (setFlag(isConst) at
        // parse time here, read back as local.isConst in ProjectMethodBodyModel::analyzeVariable,
        // which later gates `result.assignable`) -- no new machinery, just a different call site
        // feeding the existing flag.
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeIndexExpression(
                makeIdentifierExpression(range, sourceName),
                makeIdentifierExpression(range, indexName)),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureForeachElementBinding(state, elementDecl);

        ast::AstNode* forBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        forBody->addChild(elementDecl);
        emitForeachElementBindings(state, range, forBody);
        forBody->addChild(body);

        ast::AstNode* forNode = arena_.make<ast::AstNode>(ast::AstNodeKind::ForStatement, range);
        forNode->setFirst(indexDecl);
        forNode->setSecond(condition);
        forNode->setThird(increment);
        forNode->addChild(forBody);

        arrayCandidate = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        arrayCandidate->addChild(forNode);
    }

    return arrayCandidate;
}

ast::AstNode* Parser::makeForeachSpanCandidate(
    const ForeachParseState& state,
    ast::AstNode* body,
    SourceRange range)
{
    const std::string& sourceName = state.sourceName;
    const std::string& indexName = state.indexName;
    const std::string& elementName = state.elementName;
    ast::TypeSyntax* elementType = state.elementType;
    const bool elementIsRaii = state.elementIsRaii;
    const bool elementMutableReference = state.elementMutableReference;
    // NativeSharp.Core Span<T>/ReadOnlySpan<T> are compiler-recognized borrowed views. Their
    // iteration uses Length() once per loop condition and Data()[index] for the element place.
    // Data() is a raw pointer, so the loop proof is the only bounds contract: no redundant
    // managed-array/indexer guard is emitted for every element. ReadOnlySpan mutation is rejected
    // semantically once the resolved source type is known.
    ast::AstNode* spanCandidate;
    {
        ast::TypeSyntax* intType = arena_.make<ast::TypeSyntax>(
            range, TokenKind::KwInt, std::string("int"));
        ast::AstNode* indexDecl = makeVariableDeclaration(
            intType, indexName, makeIntegerLiteralExpression(range, 0), false);
        ast::AstNode* lengthMember = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "Length"));
        ast::AstNode* condition = makeBinaryExpression(
            TokenKind::Less,
            makeIdentifierExpression(range, indexName),
            lengthMember);
        ast::AstNode* increment = makeAssignmentExpression(
            makeIdentifierExpression(range, indexName),
            makeBinaryExpression(
                TokenKind::Plus,
                makeIdentifierExpression(range, indexName),
                makeIntegerLiteralExpression(range, 1)));
        ast::AstNode* dataCall = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "Data"));
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeIndexExpression(
                dataCall,
                makeIdentifierExpression(range, indexName)),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureForeachElementBinding(state, elementDecl);
        ast::AstNode* forBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, range);
        forBody->addChild(elementDecl);
        emitForeachElementBindings(state, range, forBody);
        forBody->addChild(body);
        ast::AstNode* forNode = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ForStatement, range);
        forNode->setFirst(indexDecl);
        forNode->setSecond(condition);
        forNode->setThird(increment);
        forNode->addChild(forBody);
        spanCandidate = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, range);
        spanCandidate->addChild(forNode);
    }

    return spanCandidate;
}

ast::AstNode* Parser::makeForeachPatternCandidate(
    const ForeachParseState& state,
    ast::AstNode* body,
    SourceRange range)
{
    const std::string& sourceName = state.sourceName;
    const std::string& enumeratorName = state.enumeratorName;
    const std::string& elementName = state.elementName;
    ast::TypeSyntax* elementType = state.elementType;
    const bool elementIsRaii = state.elementIsRaii;
    const bool elementMutableReference = state.elementMutableReference;
    // third(): the enumerator-pattern candidate.
    //   raii var $__foreach_<id>_enumerator = $__foreach_<id>_source.GetEnumerator();
    //   while ($__foreach_<id>_enumerator.MoveNext()) {
    //       <ElementType> <name> = $__foreach_<id>_enumerator.Current;
    //       { body }
    //   }
    //
    // `Current` is read here as a plain property access (P1.1 of the closure review), not a
    // zero-arg method call: real C#'s enumerator pattern (`IEnumerator<T>.Current`) is a
    // get-only PROPERTY, and NativeSharp has supported properties since Stage 38, so the
    // previous `Current()`-as-method desugaring was an unnecessary deviation from the structural
    // convention this feature is supposed to mirror. Every enumerator class this compiler ships
    // or documents (ListEnumerator<T>, examples/project_stage52x_foreach's CountUpToEnumerator)
    // is updated alongside this change to declare `Current` as a property instead of a method --
    // see lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns.
    ast::AstNode* patternCandidate;
    {
        ast::AstNode* getEnumeratorCall = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, sourceName), "GetEnumerator"));
        ast::AstNode* enumeratorDecl = makeVariableDeclaration(
            makeForeachVarMarker(range), enumeratorName, getEnumeratorCall, false);
        // This spelling cannot come from source: it is compiler-owned metadata on the synthetic
        // declaration. ProjectMethodBodyModel turns it into RAII only when the inferred type is
        // reference-shaped, avoiding a heap-only contract for value enumerators.
        enumeratorDecl->setAuxiliaryText("local:foreach-enumerator");

        ast::AstNode* condition = makeZeroArgCallExpression(
            makeMemberExpression(makeIdentifierExpression(range, enumeratorName), "MoveNext"));

        // isConst=true here too (P0.3) -- both candidates must agree on the element's
        // read-only-ness, since exactly one of them is the one actually analyzed/lowered and the
        // other is discarded; the user-visible semantics must not depend on which was chosen.
        ast::AstNode* elementDecl = makeVariableDeclaration(
            elementType,
            elementName,
            makeMemberExpression(makeIdentifierExpression(range, enumeratorName), "Current"),
            elementIsRaii,
            /*isConst=*/!elementMutableReference);
        configureForeachElementBinding(state, elementDecl);

        ast::AstNode* whileBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        whileBody->addChild(elementDecl);
        emitForeachElementBindings(state, range, whileBody);
        whileBody->addChild(body);

        ast::AstNode* whileNode = arena_.make<ast::AstNode>(ast::AstNodeKind::WhileStatement, range);
        whileNode->setFirst(condition);
        whileNode->setSecond(whileBody);

        patternCandidate = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
        patternCandidate->addChild(enumeratorDecl);
        patternCandidate->addChild(whileNode);
    }

    return patternCandidate;
}
ast::AstNode* Parser::parseReturnStatement()
{
    const std::size_t beginIndex = index_;
    consume();
    const bool isReferenceReturn = atContextualKeyword("ref") && peek().kind() != TokenKind::Semicolon;
    if (isReferenceReturn) consume();
    ast::AstNode* value = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        value = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ReturnStatement, tokenRangeFrom(beginIndex));
    node->setFirst(value);
    node->setReference(isReferenceReturn);
    return node;
}

ast::AstNode* Parser::parseDeferStatement()
{
    return parseStructuredControlStatement();
}

ast::AstNode* Parser::parseTryStatement()
{
    return parseWithContinuation(ContinuationRequestKind::Statement);
}

ast::AstNode* Parser::parseCatchClause()
{
    return parseWithContinuation(ContinuationRequestKind::CatchClause);
}

ast::AstNode* Parser::parseThrowStatement()
{
    const std::size_t beginIndex = index_;
    consume();

    ast::AstNode* value = nullptr;
    if (!at(TokenKind::Semicolon))
    {
        value = parseExpression();
    }
    expect(TokenKind::Semicolon, "';'");

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ThrowStatement,
        tokenRangeFrom(beginIndex));
    node->setFirst(value);
    return node;
}

ast::AstNode* Parser::parseUsingStatement()
{
    return parseStructuredControlStatement();
}

}
