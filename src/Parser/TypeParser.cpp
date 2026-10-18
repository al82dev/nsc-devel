#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;

void reportDeferredTypeLanguage3(
    diagnostics::DiagnosticEngine& diagnostics,
    SourceRange range,
    language::Language3Feature feature)
{
    diagnostics.report(
        diagnostics::DiagnosticId::NS3049DeferredLanguageSurface,
        diagnostics::DiagnosticSeverity::Error,
        range,
        language::language3SurfaceDiagnostic(feature));
}
}
bool Parser::canStartType(const lexer::Token& token) const noexcept
{
    if (lexer::isOwnershipQualifierKeyword(token.kind()))
    {
        return true;
    }

    if (lexer::isBuiltinTypeKeyword(token.kind()))
    {
        return true;
    }

    if (token.kind() == TokenKind::Identifier && token.lexeme() == "fn")
    {
        return true;
    }

    if (token.kind() == TokenKind::Identifier &&
        (token.lexeme() == "simd" || token.lexeme() == "mask" || token.lexeme() == "decimal"))
    {
        return true;
    }

    return token.kind() == TokenKind::Identifier && knownTypeNames_.contains(std::string(token.lexeme()));
}

bool Parser::canStartTypeAt(std::size_t tokenIndex) const noexcept
{
    std::size_t afterType = tokenIndex;
    return scanTypeAt(tokenIndex, afterType);
}

bool Parser::scanTypeAt(std::size_t tokenIndex, std::size_t& afterType) const noexcept
{
    if (tokenIndex >= tokens_.size() || !scanOwnershipTypePrefix(tokenIndex))
        return false;

    if (tokenIndex < tokens_.size() &&
        tokens_[tokenIndex].kind() == TokenKind::Identifier &&
        (tokens_[tokenIndex].lexeme() == "dyn" || tokens_[tokenIndex].lexeme() == "some"))
    {
        std::size_t afterDynamicType = tokenIndex + 1U;
        if (scanTypeAt(tokenIndex + 1U, afterDynamicType))
        {
            afterType = afterDynamicType;
            return true;
        }
    }
    if (tokenIndex < tokens_.size() &&
        tokens_[tokenIndex].kind() == TokenKind::Identifier &&
        tokens_[tokenIndex].lexeme() == "fn")
        return scanFunctionTypeAt(tokenIndex, afterType);
    if (tokens_[tokenIndex].kind() == TokenKind::LeftParen)
        return scanTupleTypeAt(tokenIndex, afterType);
    return scanNamedTypeAt(tokenIndex, afterType);
}

bool Parser::scanOwnershipTypePrefix(std::size_t& tokenIndex) const noexcept
{
    if (!lexer::isOwnershipQualifierKeyword(tokens_[tokenIndex].kind()))
        return true;
    const TokenKind ownershipQualifier = tokens_[tokenIndex].kind();
    ++tokenIndex;
    if (tokenIndex >= tokens_.size() ||
        lexer::isOwnershipQualifierKeyword(tokens_[tokenIndex].kind()))
        return false;
    if (ownershipQualifier != TokenKind::KwBorrowed ||
        tokens_[tokenIndex].kind() != TokenKind::LeftBracket)
        return true;
    if (tokenIndex + 2U >= tokens_.size() ||
        (tokens_[tokenIndex + 1U].kind() != TokenKind::Identifier &&
         tokens_[tokenIndex + 1U].kind() != TokenKind::KwThis &&
         tokens_[tokenIndex + 1U].kind() != TokenKind::KwStatic) ||
        tokens_[tokenIndex + 2U].kind() != TokenKind::RightBracket)
        return false;
    tokenIndex += 3U;
    return true;
}

bool Parser::scanFunctionTypeAt(
    std::size_t tokenIndex,
    std::size_t& afterType) const noexcept
{
    std::size_t cursor = tokenIndex + 1U;
    if (!scanFunctionCarrierAt(cursor) || !scanFunctionParametersAt(cursor))
        return false;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::RightParen)
        return false;
    ++cursor;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::Arrow)
        return false;
    ++cursor;
    std::size_t afterResult = cursor;
    if (!scanTypeAt(cursor, afterResult)) return false;
    cursor = afterResult;
    scanFunctionEffectsAt(cursor);
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier &&
        tokens_[cursor].lexeme() == "throws")
    {
        ++cursor;
        std::size_t afterError = cursor;
        if (!scanTypeAt(cursor, afterError)) return false;
        cursor = afterError;
    }
    afterType = cursor;
    return true;
}

bool Parser::scanFunctionCarrierAt(std::size_t& cursor) const noexcept
{
    if (cursor >= tokens_.size() ||
        (tokens_[cursor].kind() != TokenKind::Star &&
         tokens_[cursor].kind() != TokenKind::Amp))
        return false;
    ++cursor;
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::QuestionBracket)
    {
        ++cursor;
        if (cursor + 1U >= tokens_.size() ||
            (tokens_[cursor].kind() != TokenKind::Identifier &&
             tokens_[cursor].kind() != TokenKind::KwThis &&
             tokens_[cursor].kind() != TokenKind::KwStatic) ||
            tokens_[cursor + 1U].kind() != TokenKind::RightBracket)
            return false;
        cursor += 2U;
    }
    else
    {
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Question)
            ++cursor;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::LeftBracket)
        {
            if (cursor + 2U >= tokens_.size() ||
                (tokens_[cursor + 1U].kind() != TokenKind::Identifier &&
                 tokens_[cursor + 1U].kind() != TokenKind::KwThis &&
                 tokens_[cursor + 1U].kind() != TokenKind::KwStatic) ||
                tokens_[cursor + 2U].kind() != TokenKind::RightBracket)
                return false;
            cursor += 3U;
        }
    }
    if (cursor < tokens_.size() &&
        ((tokens_[cursor].kind() == TokenKind::Identifier &&
          tokens_[cursor].lexeme() == "C") ||
         tokens_[cursor].kind() == TokenKind::KwVectorCall ||
         (tokens_[cursor].kind() == TokenKind::Identifier &&
          (tokens_[cursor].lexeme() == "native" ||
           tokens_[cursor].lexeme() == "cdecl" ||
           tokens_[cursor].lexeme() == "stdcall" ||
           tokens_[cursor].lexeme() == "fastcall" ||
           tokens_[cursor].lexeme() == "win64"))))
        ++cursor;
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::LeftParen)
        return false;
    ++cursor;
    return true;
}

bool Parser::scanFunctionParametersAt(std::size_t& cursor) const noexcept
{
    bool sawParameter = false;
    while (cursor < tokens_.size() && tokens_[cursor].kind() != TokenKind::RightParen)
    {
        if (tokens_[cursor].kind() == TokenKind::Ellipsis)
        {
            if (!sawParameter || cursor + 1U >= tokens_.size() ||
                tokens_[cursor + 1U].kind() != TokenKind::RightParen)
                return false;
            ++cursor;
            break;
        }
        std::size_t afterParameter = cursor;
        if (!scanTypeAt(cursor, afterParameter)) return false;
        cursor = afterParameter;
        sawParameter = true;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Comma)
        {
            ++cursor;
            continue;
        }
        break;
    }
    return true;
}

void Parser::scanFunctionEffectsAt(std::size_t& cursor) const noexcept
{
    bool sawUnsafe = false;
    bool sawNoHeap = false;
    bool sawAsync = false;
    while (cursor < tokens_.size())
    {
        const bool unsafeEffect = tokens_[cursor].kind() == TokenKind::Identifier &&
            tokens_[cursor].lexeme() == "unsafe";
        const bool noHeapEffect = tokens_[cursor].kind() == TokenKind::KwNoHeap;
        const bool asyncEffect = tokens_[cursor].kind() == TokenKind::Identifier &&
            tokens_[cursor].lexeme() == "async";
        if ((!unsafeEffect && !noHeapEffect && !asyncEffect) ||
            (unsafeEffect && sawUnsafe) || (noHeapEffect && sawNoHeap) ||
            (asyncEffect && sawAsync))
            break;
        sawUnsafe = sawUnsafe || unsafeEffect;
        sawNoHeap = sawNoHeap || noHeapEffect;
        sawAsync = sawAsync || asyncEffect;
        ++cursor;
    }
}

bool Parser::scanTupleTypeAt(
    std::size_t tokenIndex,
    std::size_t& afterType) const noexcept
{
    std::size_t cursor = tokenIndex + 1U;
    std::size_t elementCount = 0;
    bool sawComma = false;
    while (cursor < tokens_.size() && tokens_[cursor].kind() != TokenKind::RightParen)
    {
        std::size_t afterElement = cursor;
        if (!scanTypeAt(cursor, afterElement)) return false;
        cursor = afterElement;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier &&
            cursor + 1U < tokens_.size() &&
            (tokens_[cursor + 1U].kind() == TokenKind::Comma ||
             tokens_[cursor + 1U].kind() == TokenKind::RightParen))
            ++cursor;
        ++elementCount;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Comma)
        {
            sawComma = true;
            ++cursor;
            if (cursor < tokens_.size() &&
                tokens_[cursor].kind() == TokenKind::RightParen) break;
            continue;
        }
        break;
    }
    if (cursor >= tokens_.size() || tokens_[cursor].kind() != TokenKind::RightParen ||
        (elementCount == 1U && !sawComma))
        return false;
    ++cursor;
    if (!scanTypeSuffixesAt(cursor)) return false;
    afterType = cursor;
    return true;
}

bool Parser::scanNamedTypeAt(
    std::size_t tokenIndex,
    std::size_t& afterType) const noexcept
{
    std::size_t cursor = tokenIndex;
    bool builtin = false;
    if (!scanNamedTypeIdentityAt(tokenIndex, cursor, builtin))
        return false;
    if (!builtin && cursor < tokens_.size() &&
        tokens_[cursor].kind() == TokenKind::Less &&
        !scanGenericTypeArgumentsAt(cursor))
        return false;
    if (!scanTypeSuffixesAt(cursor)) return false;
    afterType = cursor;
    return true;
}

bool Parser::scanNamedTypeIdentityAt(
    std::size_t tokenIndex,
    std::size_t& cursor,
    bool& builtin) const noexcept
{
    const lexer::Token& token = tokens_[tokenIndex];
    builtin = lexer::isBuiltinTypeKeyword(token.kind());
    const bool portableSimd = token.kind() == TokenKind::Identifier &&
        (token.lexeme() == "simd" || token.lexeme() == "mask" || token.lexeme() == "decimal");
    if (token.kind() != TokenKind::Identifier && !builtin)
        return false;
    std::string candidate(token.lexeme());
    cursor = tokenIndex + 1U;
    bool qualified = false;
    bool moduleQualified = false;
    if (cursor + 2U < tokens_.size() &&
        tokens_[cursor].kind() == TokenKind::Colon &&
        tokens_[cursor + 1U].kind() == TokenKind::Colon &&
        tokens_[cursor + 2U].kind() == TokenKind::Identifier)
    {
        candidate += "::";
        candidate += tokens_[cursor + 2U].lexeme();
        cursor += 3U;
        qualified = true;
        moduleQualified = true;
    }
    while (cursor + 1U < tokens_.size() &&
           tokens_[cursor].kind() == TokenKind::Dot &&
           tokens_[cursor + 1U].kind() == TokenKind::Identifier)
    {
        qualified = true;
        candidate.push_back('.');
        candidate += tokens_[cursor + 1U].lexeme();
        cursor += 2U;
    }
    if (!builtin && qualified)
        return moduleQualified || knownTypeNames_.contains(candidate);
    return builtin || portableSimd || knownTypeNames_.contains(candidate);
}

bool Parser::scanGenericTypeArgumentsAt(std::size_t& cursor) const noexcept
{
    int depth = 0;
    do
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::Less) ++depth;
        else if (kind == TokenKind::Greater) --depth;
        else if (kind == TokenKind::GreaterGreater ||
                 kind == TokenKind::GreaterGreaterEqual) depth -= 2;
        else if (kind == TokenKind::EndOfFile) return false;
        ++cursor;
    }
    while (cursor < tokens_.size() && depth > 0);
    return depth == 0;
}

bool Parser::scanTypeSuffixesAt(std::size_t& cursor) const noexcept
{
    if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::KwConst)
        ++cursor;
    while (cursor < tokens_.size())
    {
        if (tokens_[cursor].kind() == TokenKind::Star)
        {
            cursor = scanPointerQualifiersAt(cursor + 1U);
            continue;
        }
        if (tokens_[cursor].kind() != TokenKind::LeftBracket) break;
        if (!scanArraySuffixAt(cursor)) return false;
    }
    return true;
}

std::size_t Parser::scanPointerQualifiersAt(std::size_t cursor) const noexcept
{
    if (cursor < tokens_.size() &&
        (tokens_[cursor].kind() == TokenKind::Question ||
         tokens_[cursor].kind() == TokenKind::Bang))
        ++cursor;
    while (cursor + 1U < tokens_.size() &&
           tokens_[cursor].kind() == TokenKind::Identifier &&
           (tokens_[cursor].lexeme() == "align" ||
            tokens_[cursor].lexeme() == "addrspace" ||
            tokens_[cursor].lexeme() == "provenance") &&
           tokens_[cursor + 1U].kind() == TokenKind::LeftParen)
    {
        cursor += 2U;
        while (cursor < tokens_.size() &&
               tokens_[cursor].kind() != TokenKind::RightParen &&
               tokens_[cursor].kind() != TokenKind::EndOfFile)
            ++cursor;
        if (cursor >= tokens_.size() ||
            tokens_[cursor].kind() != TokenKind::RightParen)
            return tokens_.size();
        ++cursor;
    }
    return cursor;
}

bool Parser::scanArraySuffixAt(std::size_t& cursor) const noexcept
{
    int depth = 1;
    ++cursor;
    while (cursor < tokens_.size() && depth != 0)
    {
        if (tokens_[cursor].kind() == TokenKind::LeftBracket) ++depth;
        else if (tokens_[cursor].kind() == TokenKind::RightBracket) --depth;
        else if (tokens_[cursor].kind() == TokenKind::EndOfFile) return false;
        ++cursor;
    }
    return depth == 0;
}

bool Parser::parseQualifiedName(std::string& name, SourceRange& range)
{
    name.clear();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "identifier");
        return false;
    }

    const SourceRange start = current().range();
    SourceRange end = start;
    name = std::string(consume().lexeme());
    if (at(TokenKind::Colon) && peek().kind() == TokenKind::Colon)
    {
        consume();
        consume();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "type name after module qualifier '::'");
            range = join(start, end);
            return false;
        }
        const lexer::Token& segment = consume();
        name += "::";
        name += segment.lexeme();
        end = segment.range();
    }
    while (consumeIf(TokenKind::Dot))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "identifier after '.'");
            range = join(start, end);
            return false;
        }
        const lexer::Token& segment = consume();
        name.push_back('.');
        name += segment.lexeme();
        end = segment.range();
    }
    range = join(start, end);
    return true;
}

bool Parser::parseOwnershipTypeQualifier(TypeParseState& state)
{
    if (!lexer::isOwnershipQualifierKeyword(current().kind())) return true;
    const lexer::Token& qualifier = consume();
    state.qualifierRange = qualifier.range();
    state.ownershipQualifier = qualifier.kind() == TokenKind::KwOwned
        ? ast::OwnershipQualifierSyntaxKind::Owned
        : ast::OwnershipQualifierSyntaxKind::Borrowed;
    if (lexer::isOwnershipQualifierKeyword(current().kind()))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "an ownership-qualified type accepts exactly one qualifier");
        return false;
    }
    if (state.ownershipQualifier == ast::OwnershipQualifierSyntaxKind::Borrowed)
        state.lifetimeName = parseOptionalLifetimeAnnotation("borrowed type");
    else if (at(TokenKind::LeftBracket))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "only a borrowed type may carry a named lifetime annotation");
        parseOptionalLifetimeAnnotation("owned type");
    }
    return true;
}

void Parser::parseReferenceTypeQualifier(TypeParseState& state)
{
    if (!atContextualKeyword("ref")) return;
    const bool refMut = peek().kind() == TokenKind::Identifier && peek().lexeme() == "mut";
    if (!canStartTypeAt(index_ + 1U) && (!refMut || !canStartTypeAt(index_ + 2U))) return;
    const lexer::Token& refToken = consume();
    if (refMut)
    {
        consume();
        state.referenceQualifier = ast::ReferenceQualifierSyntaxKind::RefMut;
    }
    else
    {
        state.referenceQualifier = ast::ReferenceQualifierSyntaxKind::Ref;
    }
    state.qualifierRange = refToken.range();
}

void Parser::parseTraitTypeQualifiers(TypeParseState& state)
{
    if (atContextualKeyword("dyn") && canStartTypeAt(index_ + 1U))
    {
        const lexer::Token& qualifier = consume();
        state.dynamicRange = qualifier.range();
        state.dynamicTrait = true;
    }
    if (!atContextualKeyword("some") || !canStartTypeAt(index_ + 1U)) return;
    const lexer::Token& qualifier = consume();
    state.opaqueRange = qualifier.range();
    state.opaqueResult = true;
    if (state.dynamicTrait)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            join(state.dynamicRange, state.opaqueRange),
            "'dyn' and 'some' are mutually exclusive trait representations");
    }
}

void Parser::parseDeferredTypePrefix(TypeParseState& state)
{
    if (at(TokenKind::KwLong) && peek().kind() == TokenKind::KwDouble)
    {
        state.longDoubleRange = join(current().range(), peek().range());
        reportDeferredTypeLanguage3(
            diagnostics_, state.longDoubleRange, language::Language3Feature::LongDouble);
        consume();
        state.recoverLongDoubleAsDouble = true;
    }
    if (atContextualKeyword("decimal"))
    {
        // NS304-T15-F01: decimal is a builtin 128-bit coefficient+scale+sign value type.
    }
}

bool Parser::parseTypeQualifiers(TypeParseState& state)
{
    state.qualifierRange = current().range();
    if (!parseOwnershipTypeQualifier(state)) return false;
    parseReferenceTypeQualifier(state);
    parseTraitTypeQualifiers(state);
    parseDeferredTypePrefix(state);
    return true;
}

ast::TypeSyntax* Parser::parseTypeImpl()
{
    TypeParseState state;
    if (!parseTypeQualifiers(state)) return nullptr;
    if (atContextualKeyword("fn")) return parseFunctionType(state);
    if (at(TokenKind::LeftParen)) return parseTupleType(state);
    return parseNamedType(state);
}

bool Parser::parseFunctionCarrier(SourceRange start, FunctionTypeParseState& state)
{
    if (consumeIf(TokenKind::Star))
        state.carrier = ast::FunctionTypeCarrierSyntaxKind::Pointer;
    else if (consumeIf(TokenKind::Amp))
        state.carrier = ast::FunctionTypeCarrierSyntaxKind::Reference;
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected '*' (function pointer) or '&' (function reference) after 'fn'");
        return false;
    }
    if (consumeIf(TokenKind::QuestionBracket))
    {
        state.nullable = true;
        if (at(TokenKind::Identifier) || at(TokenKind::KwThis) || at(TokenKind::KwStatic))
            state.lifetimeName = std::string(consume().lexeme());
        else
            expect(TokenKind::Identifier, "lifetime name in function reference");
        expect(TokenKind::RightBracket, "']' after lifetime name");
    }
    else
    {
        state.nullable = consumeIf(TokenKind::Question);
        if (at(TokenKind::LeftBracket))
            state.lifetimeName = parseOptionalLifetimeAnnotation("function reference");
    }
    if (!state.lifetimeName.empty() &&
        state.carrier != ast::FunctionTypeCarrierSyntaxKind::Reference)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            start,
            "only a function reference may carry a named lifetime");
    }
    return true;
}

void Parser::parseFunctionCallingConvention(FunctionTypeParseState& state)
{
    if (atContextualKeyword("C"))
        state.convention = ast::FunctionCallingConventionSyntaxKind::C;
    else if (at(TokenKind::KwVectorCall))
        state.convention = ast::FunctionCallingConventionSyntaxKind::VectorCall;
    else if (atContextualKeyword("native"))
        state.convention = ast::FunctionCallingConventionSyntaxKind::NativeSharp;
    else if (atContextualKeyword("cdecl"))
        state.convention = ast::FunctionCallingConventionSyntaxKind::C;
    else if (atContextualKeyword("stdcall"))
        state.convention = ast::FunctionCallingConventionSyntaxKind::StdCall;
    else if (atContextualKeyword("fastcall"))
        state.convention = ast::FunctionCallingConventionSyntaxKind::FastCall;
    else if (atContextualKeyword("win64"))
        state.convention = ast::FunctionCallingConventionSyntaxKind::Win64;
    else
        return;
    consume();
}

void Parser::validateFunctionCarrier(
    SourceRange start,
    const FunctionTypeParseState& state)
{
    if (state.carrier != ast::FunctionTypeCarrierSyntaxKind::Reference ||
        state.convention == ast::FunctionCallingConventionSyntaxKind::NativeSharp)
        return;
    diagnostics_.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        start,
        "a function reference uses the NativeSharp calling convention; foreign conventions require fn*");
}

ast::TypeSyntax* Parser::makeFunctionType(
    SourceRange start,
    TypeParseState& typeState,
    FunctionTypeParseState& functionState)
{
    ast::TypeSyntax* function = arena_.make<ast::TypeSyntax>(
        start, TokenKind::Invalid, "$function");
    function->setFunction();
    function->setOwnershipQualifier(typeState.ownershipQualifier);
    function->setLifetimeName(std::move(typeState.lifetimeName));
    function->setFunctionCarrier(functionState.carrier);
    function->setFunctionCallingConvention(functionState.convention);
    function->setFunctionNullable(functionState.nullable);
    function->setFunctionLifetimeName(std::move(functionState.lifetimeName));
    return function;
}

bool Parser::parseFunctionTypeParameters(ast::TypeSyntax* function)
{
    bool sawParameter = false;
    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        if (consumeIf(TokenKind::Ellipsis))
        {
            function->setFunctionVariadic();
            if (!sawParameter)
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1U].range(),
                    "a variadic function type requires at least one fixed parameter");
            if (!at(TokenKind::RightParen))
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    current().range(),
                    "'...' must be the final item in a function type parameter list");
            break;
        }
        ast::TypeSyntax* parameter = parseType();
        if (parameter == nullptr) return false;
        if (parameter->baseTokenKind() == TokenKind::KwVoid &&
            parameter->modifiers().empty())
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                parameter->range(),
                "a function type parameter cannot have type void");
        function->addTypeArgument(parameter);
        sawParameter = true;
        if (!consumeIf(TokenKind::Comma)) break;
        if (at(TokenKind::RightParen))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "a function type parameter list cannot end with ','; use ', ...' for variadicity");
            break;
        }
    }
    return expect(TokenKind::RightParen, "')' after function type parameters");
}

bool Parser::parseFunctionTypeResult(ast::TypeSyntax* function)
{
    if (!expect(TokenKind::Arrow, "'->' after function type parameters")) return false;
    ast::TypeSyntax* returnType = parseType();
    if (returnType == nullptr) return false;
    function->setFunctionReturnType(returnType);
    return true;
}

void Parser::parseFunctionTypeEffects(ast::TypeSyntax* function)
{
    bool sawUnsafe = false;
    bool sawNoHeap = false;
    bool sawAsync = false;
    while (true)
    {
        const bool unsafeEffect = atContextualKeyword("unsafe");
        const bool noHeapEffect = at(TokenKind::KwNoHeap);
        const bool asyncEffect = atContextualKeyword("async");
        if (!unsafeEffect && !noHeapEffect && !asyncEffect) break;
        const bool duplicate = (unsafeEffect && sawUnsafe) ||
            (noHeapEffect && sawNoHeap) || (asyncEffect && sawAsync);
        const SourceRange effectRange = consume().range();
        if (duplicate)
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                effectRange,
                "duplicate function type effect");
            continue;
        }
        sawUnsafe = sawUnsafe || unsafeEffect;
        sawNoHeap = sawNoHeap || noHeapEffect;
        sawAsync = sawAsync || asyncEffect;
    }
    function->setFunctionUnsafe(sawUnsafe);
    function->setFunctionNoHeap(sawNoHeap);
    function->setFunctionAsync(sawAsync);
}

bool Parser::parseFunctionTypeError(
    ast::TypeSyntax* function,
    const FunctionTypeParseState& state)
{
    if (!atContextualKeyword("throws")) return true;
    const SourceRange throwsRange = consume().range();
    if (state.carrier == ast::FunctionTypeCarrierSyntaxKind::Pointer &&
        state.convention != ast::FunctionCallingConventionSyntaxKind::NativeSharp)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            throwsRange,
            "a foreign function pointer cannot propagate a NativeSharp typed error");
    }
    ast::TypeSyntax* errorType = parseType();
    if (errorType == nullptr) return false;
    function->setFunctionErrorType(errorType);
    return true;
}

void Parser::validateFunctionVariadic(
    ast::TypeSyntax* function,
    SourceRange start,
    const FunctionTypeParseState& state)
{
    if (function->functionIsVariadic() &&
        (state.carrier != ast::FunctionTypeCarrierSyntaxKind::Pointer ||
         state.convention == ast::FunctionCallingConventionSyntaxKind::NativeSharp))
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            start,
            "variadicity requires a foreign function pointer calling convention");
    if (function->functionIsVariadic() &&
        state.convention != ast::FunctionCallingConventionSyntaxKind::C &&
        state.convention != ast::FunctionCallingConventionSyntaxKind::Win64)
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            start,
            "a variadic function pointer requires the cdecl/C or win64 calling convention on Windows x64");
}

ast::TypeSyntax* Parser::parseFunctionType(TypeParseState& typeState)
{
    const SourceRange start = current().range();
    consume();
    if (typeState.dynamicTrait || typeState.opaqueResult)
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            typeState.dynamicTrait ? typeState.dynamicRange : typeState.opaqueRange,
            "a function type cannot use 'dyn' or 'some'");
    FunctionTypeParseState functionState;
    if (!parseFunctionCarrier(start, functionState)) return nullptr;
    parseFunctionCallingConvention(functionState);
    validateFunctionCarrier(start, functionState);
    if (!expect(TokenKind::LeftParen, "'(' after function type carrier")) return nullptr;
    ast::TypeSyntax* function = makeFunctionType(start, typeState, functionState);
    if (!parseFunctionTypeParameters(function)) return nullptr;
    if (!parseFunctionTypeResult(function)) return nullptr;
    parseFunctionTypeEffects(function);
    if (!parseFunctionTypeError(function, functionState)) return nullptr;
    validateFunctionVariadic(function, start, functionState);
    const SourceRange complete = index_ == 0
        ? start
        : join(
            typeState.ownershipQualifier == ast::OwnershipQualifierSyntaxKind::None
                ? start : typeState.qualifierRange,
            tokens_[index_ - 1U].range());
    function->setRange(complete);
    return function;
}

ast::TypeSyntax* Parser::parseTupleType(TypeParseState& state)
{
    const SourceRange open = consume().range();
    ast::TypeSyntax* tuple = arena_.make<ast::TypeSyntax>(open, TokenKind::Invalid, "$tuple");
    tuple->setTuple();
    tuple->setOwnershipQualifier(state.ownershipQualifier);
    tuple->setLifetimeName(state.lifetimeName);
    if (state.dynamicTrait)
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.dynamicRange,
            "'dyn' requires a named trait or interface type");
    if (state.opaqueResult)
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.opaqueRange,
            "'some' requires a named trait type");
    if (state.ownershipQualifier != ast::OwnershipQualifierSyntaxKind::None)
        state.qualifierRange = join(state.qualifierRange, open);
    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        ast::TypeSyntax* elementType = parseType();
        if (elementType == nullptr) return nullptr;
        std::string elementName;
        if (at(TokenKind::Identifier) &&
            (peek().kind() == TokenKind::Comma || peek().kind() == TokenKind::RightParen))
            elementName = std::string(consume().lexeme());
        tuple->addTupleElement(elementType, std::move(elementName));
        if (!consumeIf(TokenKind::Comma) || at(TokenKind::RightParen)) break;
    }
    const SourceRange close = current().range();
    if (!expect(TokenKind::RightParen, "')' after tuple type")) return nullptr;
    tuple->setRange(join(
        state.ownershipQualifier == ast::OwnershipQualifierSyntaxKind::None
            ? open : state.qualifierRange,
        close));
    return tuple;
}

void Parser::normalizeBuiltinTypeName(std::string& baseName)
{
    if (baseName == "i8") baseName = "sbyte";
    else if (baseName == "u8") baseName = "byte";
    else if (baseName == "i16") baseName = "short";
    else if (baseName == "u16") baseName = "ushort";
    else if (baseName == "i32") baseName = "int";
    else if (baseName == "u32") baseName = "uint";
    else if (baseName == "i64") baseName = "long";
    else if (baseName == "u64") baseName = "ulong";
    else if (baseName == "isize") baseName = "nint";
    else if (baseName == "usize") baseName = "nuint";
}

ast::TypeSyntax* Parser::parseNamedTypeBase(
    TypeParseState& state,
    SourceRange& completeRange)
{
    if (!lexer::isBuiltinTypeKeyword(current().kind()) && !at(TokenKind::Identifier))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            state.ownershipQualifier == ast::OwnershipQualifierSyntaxKind::None
                ? "expected a NativeSharp type"
                : "expected a NativeSharp type after ownership qualifier");
        return nullptr;
    }
    const lexer::TokenKind baseKind = current().kind();
    completeRange = current().range();
    std::string baseName;
    if (at(TokenKind::Identifier))
    {
        if (!parseQualifiedName(baseName, completeRange)) return nullptr;
    }
    else
    {
        const lexer::Token& base = consume();
        baseName = std::string(base.lexeme());
        normalizeBuiltinTypeName(baseName);
        completeRange = base.range();
    }
    if (state.recoverLongDoubleAsDouble)
        completeRange = join(state.longDoubleRange, completeRange);
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        completeRange, baseKind, std::move(baseName));
    type->setOwnershipQualifier(state.ownershipQualifier);
    type->setReferenceQualifier(state.referenceQualifier);
    type->setLifetimeName(std::move(state.lifetimeName));
    type->setDynamicTrait(state.dynamicTrait);
    type->setOpaqueResult(state.opaqueResult);
    if (state.opaqueResult) type->setOpaqueResultTraitName(type->baseName());
    if (state.ownershipQualifier != ast::OwnershipQualifierSyntaxKind::None)
        completeRange = join(state.qualifierRange, completeRange);
    if (state.dynamicTrait) completeRange = join(state.dynamicRange, completeRange);
    if (state.opaqueResult) completeRange = join(state.opaqueRange, completeRange);
    return type;
}

void Parser::parseNamedTypeArguments(ast::TypeSyntax* type, SourceRange& completeRange)
{
    if (type->baseTokenKind() == TokenKind::Identifier && at(TokenKind::Less))
    {
        std::vector<ast::TypeSyntax*> arguments;
        if (parseTypeArgumentList(arguments))
        {
            for (ast::TypeSyntax* argument : arguments) type->addTypeArgument(argument);
            if (index_ > 0) completeRange = join(completeRange, tokens_[index_ - 1U].range());
        }
    }
    if (at(TokenKind::Ellipsis))
    {
        const SourceRange expansion = consume().range();
        type->setPackExpansion();
        completeRange = join(completeRange, expansion);
    }
}

void Parser::parsePointeeConst(
    ast::TypeSyntax* type,
    SourceRange& completeRange,
    bool& pointeeConst,
    SourceRange& pointeeConstRange)
{
    pointeeConst = at(TokenKind::KwConst);
    if (!pointeeConst) return;
    pointeeConstRange = consume().range();
    type->setPointeeConst();
    completeRange = join(completeRange, pointeeConstRange);
}

void Parser::parsePointerProvenanceQualifier(ast::TypeModifierSyntax& pointer)
{
    if (atContextualKeyword("none"))
    {
        pointer.pointerHasProvenance = false;
        consume();
    }
    else if (atContextualKeyword("tracked"))
    {
        pointer.pointerHasProvenance = true;
        consume();
    }
    else
    {
        expectContextualKeyword("tracked", "'tracked' or 'none' provenance mode");
        if (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile)) consume();
    }
}

void Parser::parsePointerNumericQualifier(
    ast::TypeModifierSyntax& pointer,
    std::string_view qualifier,
    SourceRange qualifierStart)
{
    std::optional<std::uint64_t> value;
    if (at(TokenKind::IntegerLiteral) &&
        std::holds_alternative<std::uint64_t>(current().value()))
    {
        value = std::get<std::uint64_t>(current().value());
        consume();
    }
    else
    {
        expect(TokenKind::IntegerLiteral, "pointer qualifier value");
    }
    if (qualifier == "align" && value.has_value())
    {
        if (*value == 0 || (*value & (*value - 1U)) != 0 || *value > 4096U)
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                qualifierStart,
                "pointer alignment knowledge must be a power of two no larger than 4096");
        else
            pointer.pointerKnownAlignment = static_cast<std::size_t>(*value);
    }
    else if (qualifier == "addrspace" && value.has_value())
    {
        if (*value > 255U)
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                qualifierStart,
                "reserved pointer address space must be in range 0..255");
        else
            pointer.pointerAddressSpace = static_cast<unsigned>(*value);
    }
}

void Parser::parsePointerTypeQualifier(ast::TypeModifierSyntax& pointer)
{
    const std::string qualifier(current().lexeme());
    const SourceRange qualifierStart = consume().range();
    consume();
    if (qualifier == "provenance")
        parsePointerProvenanceQualifier(pointer);
    else
        parsePointerNumericQualifier(pointer, qualifier, qualifierStart);
    expect(TokenKind::RightParen, "')' after pointer qualifier");
    pointer.range = join(pointer.range, tokens_[index_ - 1U].range());
}

void Parser::parsePointerTypeQualifiers(ast::TypeModifierSyntax& pointer)
{
    while ((atContextualKeyword("align") || atContextualKeyword("addrspace") ||
            atContextualKeyword("provenance")) &&
           peek().kind() == TokenKind::LeftParen)
        parsePointerTypeQualifier(pointer);
}

void Parser::parsePointerTypeModifier(ast::TypeSyntax* type, SourceRange& completeRange)
{
    const lexer::Token& star = consume();
    ast::TypeModifierSyntax pointer{
        ast::TypeModifierSyntaxKind::Pointer,
        nullptr,
        star.range()};
    if (consumeIf(TokenKind::Question))
    {
        pointer.pointerNullability = ast::PointerNullabilitySyntaxKind::Nullable;
        pointer.range = join(pointer.range, tokens_[index_ - 1U].range());
    }
    else if (consumeIf(TokenKind::Bang))
    {
        pointer.pointerNullability = ast::PointerNullabilitySyntaxKind::NonNull;
        pointer.range = join(pointer.range, tokens_[index_ - 1U].range());
    }
    parsePointerTypeQualifiers(pointer);
    type->addModifier(pointer);
    completeRange = join(completeRange, pointer.range);
}

bool Parser::parseSpecialArrayTypeModifier(
    ast::TypeSyntax* type,
    SourceRange left,
    SourceRange& completeRange)
{
    if (at(TokenKind::Star) && peek().kind() == TokenKind::RightBracket)
    {
        consume();
        const SourceRange right = consume().range();
        const SourceRange modifierRange = join(left, right);
        type->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::FlexibleArray,
            nullptr,
            modifierRange});
        completeRange = join(completeRange, modifierRange);
        return true;
    }
    if (!at(TokenKind::RightBracket)) return false;
    const SourceRange right = consume().range();
    const SourceRange modifierRange = join(left, right);
    type->addModifier(ast::TypeModifierSyntax{
        ast::TypeModifierSyntaxKind::DynamicArray,
        nullptr,
        modifierRange});
    completeRange = join(completeRange, modifierRange);
    return true;
}

void Parser::parseRankArrayTypeModifier(
    ast::TypeSyntax* type,
    SourceRange left,
    SourceRange& completeRange)
{
    std::size_t rank = 1;
    while (consumeIf(TokenKind::Comma)) ++rank;
    const SourceRange beforeRight = current().range();
    expect(TokenKind::RightBracket, "']'");
    SourceRange modifierRange = join(left, beforeRight);
    if (index_ > 0 && tokens_[index_ - 1U].kind() == TokenKind::RightBracket)
        modifierRange = join(left, tokens_[index_ - 1U].range());
    type->addModifier(ast::TypeModifierSyntax{
        ast::TypeModifierSyntaxKind::RangeArray, nullptr, modifierRange, rank});
    completeRange = join(completeRange, modifierRange);
}

void Parser::parseFixedArrayTypeModifier(
    ast::TypeSyntax* type,
    SourceRange left,
    SourceRange& completeRange)
{
    std::vector<ast::AstNode*> dimensions;
    dimensions.push_back(parseExpression());
    while (consumeIf(TokenKind::Comma))
    {
        if (at(TokenKind::RightBracket))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedExpression,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "fixed multidimensional array requires an expression after ','");
            break;
        }
        dimensions.push_back(parseExpression());
    }
    const SourceRange beforeRight = current().range();
    expect(TokenKind::RightBracket, "']'");
    SourceRange modifierRange = join(left, beforeRight);
    if (index_ > 0 && tokens_[index_ - 1U].kind() == TokenKind::RightBracket)
        modifierRange = join(left, tokens_[index_ - 1U].range());
    std::vector<ast::TypeModifierSyntax> modifiers;
    modifiers.reserve(dimensions.size());
    for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
        modifiers.push_back(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::Array,
            *dimension,
            modifierRange});
    type->addArrayModifierGroup(modifiers);
    completeRange = join(completeRange, modifierRange);
}

void Parser::parseArrayTypeModifier(ast::TypeSyntax* type, SourceRange& completeRange)
{
    const SourceRange left = consume().range();
    if (parseSpecialArrayTypeModifier(type, left, completeRange)) return;
    if (at(TokenKind::Comma))
    {
        parseRankArrayTypeModifier(type, left, completeRange);
        return;
    }
    parseFixedArrayTypeModifier(type, left, completeRange);
}

void Parser::parseTypePostfixModifiers(ast::TypeSyntax* type, SourceRange& completeRange)
{
    SourceRange pointeeConstRange{};
    bool pointeeConst = false;
    parsePointeeConst(type, completeRange, pointeeConst, pointeeConstRange);
    while (true)
    {
        if (at(TokenKind::Star))
        {
            parsePointerTypeModifier(type, completeRange);
            continue;
        }
        if (at(TokenKind::LeftBracket))
        {
            parseArrayTypeModifier(type, completeRange);
            continue;
        }
        break;
    }
    if (pointeeConst && std::ranges::none_of(
            type->modifiers(), [](const ast::TypeModifierSyntax& modifier)
            {
                return modifier.kind == ast::TypeModifierSyntaxKind::Pointer;
            }))
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            pointeeConstRange,
            "postfix const qualifies a pointer pointee and therefore requires '*' in the type");
}

ast::TypeSyntax* Parser::parseNamedType(TypeParseState& state)
{
    SourceRange completeRange{};
    ast::TypeSyntax* type = parseNamedTypeBase(state, completeRange);
    if (type == nullptr) return nullptr;
    parseNamedTypeArguments(type, completeRange);
    parseTypePostfixModifiers(type, completeRange);
    type->setRange(completeRange);
    return type;
}


std::string Parser::parseOptionalLifetimeAnnotation(std::string_view context)
{
    if (!consumeIf(TokenKind::LeftBracket)) return {};
    std::string result;
    if (at(TokenKind::Identifier) || at(TokenKind::KwThis) || at(TokenKind::KwStatic))
        result = std::string(consume().lexeme());
    else
        expect(TokenKind::Identifier, ("lifetime name in " + std::string(context)).c_str());
    expect(TokenKind::RightBracket, "']' after lifetime name");
    return result;
}

ast::TypeSyntax* Parser::parseAllocationElementType()
{
    if (!lexer::isBuiltinTypeKeyword(current().kind()) && !at(TokenKind::Identifier))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedType,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected allocation element type");
        return nullptr;
    }

    const lexer::TokenKind baseKind = current().kind();
    SourceRange completeRange = current().range();
    std::string baseName;
    if (at(TokenKind::Identifier))
    {
        if (!parseQualifiedName(baseName, completeRange))
        {
            return nullptr;
        }
    }
    else
    {
        const lexer::Token& base = consume();
        baseName = std::string(base.lexeme());
        completeRange = base.range();
    }

    if (baseKind == TokenKind::KwVoid)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            completeRange,
            "void cannot be used as an allocation element type");
    }

    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        completeRange, baseKind, std::move(baseName));

    if (baseKind == TokenKind::Identifier && at(TokenKind::Less))
    {
        std::vector<ast::TypeSyntax*> arguments;
        if (parseTypeArgumentList(arguments))
        {
            for (ast::TypeSyntax* argument : arguments) type->addTypeArgument(argument);
            if (index_ > 0) completeRange = join(completeRange, tokens_[index_ - 1].range());
        }
    }

    while (at(TokenKind::Star))
    {
        const lexer::Token& star = consume();
        type->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::Pointer,
            nullptr,
            star.range()});
        completeRange = join(completeRange, star.range());
    }

    type->setRange(completeRange);
    return type;
}
}
