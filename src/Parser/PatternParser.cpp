#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;
}

bool Parser::patternLiteralStartsHere() const noexcept
{
    const TokenKind kind = current().kind();
    return kind == TokenKind::IntegerLiteral || kind == TokenKind::FloatingLiteral ||
        kind == TokenKind::CharacterLiteral || kind == TokenKind::StringLiteral ||
        kind == TokenKind::KwTrue || kind == TokenKind::KwFalse || kind == TokenKind::KwNull ||
        (kind == TokenKind::Minus && peek().kind() == TokenKind::IntegerLiteral);
}

std::pair<ast::AstNode*, std::optional<std::int64_t>>
Parser::parseSignedPatternIntegerLiteral()
{
    const bool negative = consumeIf(TokenKind::Minus);
    if (!at(TokenKind::IntegerLiteral))
    {
        expect(TokenKind::IntegerLiteral, "integer literal in range pattern");
        return {makeErrorExpression(), std::nullopt};
    }
    ast::AstNode* literal = parsePrimaryExpression();
    const std::uint64_t magnitude = literal->integerValue();
    std::optional<std::int64_t> value;
    if (negative)
    {
        constexpr std::uint64_t minMagnitude =
            std::uint64_t{1} << (std::numeric_limits<std::int64_t>::digits);
        if (magnitude <= minMagnitude)
            value = magnitude == minMagnitude
                ? std::numeric_limits<std::int64_t>::min()
                : -static_cast<std::int64_t>(magnitude);
        ast::AstNode* unary = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression,
            SourceRange(tokens_[index_ - 2].range().begin(), literal->range().end()));
        unary->setTokenKind(TokenKind::Minus);
        unary->setFirst(literal);
        literal = unary;
    }
    else if (magnitude <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        value = static_cast<std::int64_t>(magnitude);
    }
    return {literal, value};
}

Parser::Pattern Parser::parseListPattern(std::size_t patternBegin)
{
    consume();
    Pattern result;
    result.kind = PatternKind::List;
    while (!at(TokenKind::RightBracket) && !at(TokenKind::EndOfFile))
    {
        if (consumeIf(TokenKind::DotDot))
        {
            if (result.restIndex.has_value())
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    tokens_[index_ - 1].range(),
                    "a list pattern accepts at most one rest element");
            else
                result.restIndex = result.children.size();

            Pattern rest;
            if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket))
                rest = parsePattern(true);
            else
            {
                rest.kind = PatternKind::Wildcard;
                rest.range = tokens_[index_ - 1].range();
            }
            result.children.push_back(std::move(rest));
        }
        else
        {
            result.children.push_back(parsePattern(true));
        }
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightBracket, "']' after list pattern");
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parseBindingPattern(std::size_t patternBegin)
{
    Pattern result;
    if (at(TokenKind::KwMove))
    {
        result.bindingMode = PatternBindingMode::Move;
        consume();
    }
    else if (atContextualKeyword("copy"))
    {
        result.bindingMode = PatternBindingMode::Copy;
        consume();
    }
    else if (atContextualKeyword("ref"))
    {
        consume();
        if (atContextualKeyword("mut"))
        {
            result.bindingMode = PatternBindingMode::ReferenceMutable;
            consume();
        }
        else
        {
            result.bindingMode = PatternBindingMode::Reference;
        }
    }
    else
    {
        consume();
    }
    result.kind = PatternKind::Binding;
    if (!at(TokenKind::Identifier))
        expect(TokenKind::Identifier, "binding name after pattern binding mode");
    else
        result.name = std::string(consume().lexeme());
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parseRelationalPattern(std::size_t patternBegin)
{
    Pattern result;
    result.kind = PatternKind::Relational;
    result.name = std::string(consume().lexeme());
    if (patternLiteralStartsHere() || at(TokenKind::Identifier))
        result.firstValue = parsePrimaryExpression();
    else
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected a literal or constant operand after a relational pattern operator");
        result.firstValue = makeErrorExpression();
    }
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parseLiteralOrRangePattern(
    std::size_t patternBegin,
    SourceRange start)
{
    Pattern result;
    result.kind = PatternKind::Literal;
    if (at(TokenKind::Minus))
    {
        auto [literal, value] = parseSignedPatternIntegerLiteral();
        result.firstValue = literal;
        result.firstInteger = value;
    }
    else
    {
        const TokenKind literalKind = current().kind();
        result.firstValue = parsePrimaryExpression();
        if (literalKind == TokenKind::IntegerLiteral)
        {
            const std::uint64_t value = result.firstValue->integerValue();
            if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                result.firstInteger = static_cast<std::int64_t>(value);
        }
        if (literalKind == TokenKind::FloatingLiteral)
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                result.firstValue->range(),
                "floating-point literals are not permitted in patterns because NaN and "
                "signed-zero equality do not define a closed pattern domain");
    }
    if (at(TokenKind::DotDot) || at(TokenKind::DotDotEqual))
    {
        result.name = std::string(consume().lexeme());
        result.kind = PatternKind::Range;
        if (!at(TokenKind::IntegerLiteral) &&
            !(at(TokenKind::Minus) && peek().kind() == TokenKind::IntegerLiteral))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "a range pattern requires an integer literal upper bound");
            result.secondValue = makeErrorExpression();
        }
        else
        {
            auto [upper, value] = parseSignedPatternIntegerLiteral();
            result.secondValue = upper;
            result.secondInteger = value;
        }
        if (result.firstInteger.has_value() && result.secondInteger.has_value() &&
            (result.name == "..="
                ? *result.firstInteger > *result.secondInteger
                : *result.firstInteger >= *result.secondInteger))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                start,
                result.name == "..="
                    ? "an inclusive range pattern requires its lower bound not to exceed its upper bound"
                    : "a half-open range pattern requires its lower bound to be less than its upper bound");
        }
    }
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parseParenthesizedPattern(std::size_t patternBegin)
{
    consume();
    Pattern result;
    if (consumeIf(TokenKind::RightParen))
    {
        result.kind = PatternKind::Tuple;
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    Pattern first = parsePattern(true);
    if (!consumeIf(TokenKind::Comma))
    {
        expect(TokenKind::RightParen, "')' after grouped pattern");
        first.range = tokenRangeFrom(patternBegin);
        return first;
    }
    result.kind = PatternKind::Tuple;
    result.children.push_back(std::move(first));
    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        result.children.push_back(parsePattern(true));
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightParen, "')' after tuple pattern");
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

void Parser::parseStructPatternBody(Pattern& pattern)
{
    pattern.kind = PatternKind::Struct;
    expect(TokenKind::LeftBrace, "'{' to open struct pattern");
    std::unordered_set<std::string> fields;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "field name in struct pattern");
            break;
        }
        const lexer::Token& field = consume();
        const std::string fieldName(field.lexeme());
        if (!fields.insert(fieldName).second)
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                field.range(),
                "duplicate field '" + fieldName + "' in struct pattern");
        pattern.fieldNames.push_back(fieldName);
        if (consumeIf(TokenKind::Colon))
            pattern.children.push_back(parsePattern(true));
        else
        {
            Pattern shorthand;
            shorthand.kind = PatternKind::Binding;
            shorthand.name = fieldName;
            shorthand.range = field.range();
            pattern.children.push_back(std::move(shorthand));
        }
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightBrace, "'}' after struct pattern");
}

Parser::Pattern Parser::parseNamedPattern(
    std::size_t patternBegin,
    bool implicitBinding)
{
    Pattern result;
    const lexer::Token& name = consume();
    result.name = std::string(name.lexeme());
    if (at(TokenKind::LeftBrace))
        parseStructPatternBody(result);
    else if (consumeIf(TokenKind::LeftParen))
    {
        std::vector<Pattern> payloads;
        if (!at(TokenKind::RightParen))
        {
            while (true)
            {
                payloads.push_back(parsePattern(true));
                if (!consumeIf(TokenKind::Comma)) break;
            }
        }
        expect(TokenKind::RightParen, "')' after variant/positional payload pattern");
        result.kind = knownTypeNames_.contains(result.name)
            ? PatternKind::Positional
            : PatternKind::Variant;
        result.children = std::move(payloads);
    }
    else
    {
        result.kind = implicitBinding ? PatternKind::Binding : PatternKind::Variant;
    }
    result.range = tokenRangeFrom(patternBegin);
    return result;
}

Parser::Pattern Parser::parsePatternPrimary(bool implicitBinding)
{
    Pattern result;
    const std::size_t patternBegin = index_;
    const SourceRange start = current().range();
    if (at(TokenKind::Identifier) && current().lexeme() == "_")
    {
        consume();
        result.kind = PatternKind::Wildcard;
        result.range = start;
        return result;
    }
    if (consumeIf(TokenKind::Bang))
    {
        result.kind = PatternKind::Not;
        result.children.push_back(parsePatternPrimary(true));
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    if (at(TokenKind::LeftBracket)) return parseListPattern(patternBegin);
    if (at(TokenKind::KwVar) || at(TokenKind::KwMove) ||
        atContextualKeyword("copy") || atContextualKeyword("ref"))
        return parseBindingPattern(patternBegin);
    if (at(TokenKind::Less) || at(TokenKind::LessEqual) ||
        at(TokenKind::Greater) || at(TokenKind::GreaterEqual))
        return parseRelationalPattern(patternBegin);
    if (patternLiteralStartsHere())
        return parseLiteralOrRangePattern(patternBegin, start);
    if (at(TokenKind::LeftParen)) return parseParenthesizedPattern(patternBegin);
    if (at(TokenKind::LeftBrace))
    {
        parseStructPatternBody(result);
        result.range = tokenRangeFrom(patternBegin);
        return result;
    }
    if (at(TokenKind::Identifier)) return parseNamedPattern(patternBegin, implicitBinding);
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected wildcard, literal, binding, tuple, struct, variant, range, or list pattern");
    if (!at(TokenKind::EndOfFile)) consume();
    result.range = start;
    return result;
}

Parser::Pattern Parser::parseAndPattern(bool implicitBinding)
{
    Pattern first = parsePatternPrimary(implicitBinding);
    if (!at(TokenKind::Amp)) return first;
    Pattern result;
    result.kind = PatternKind::And;
    result.range = first.range;
    result.children.push_back(std::move(first));
    while (consumeIf(TokenKind::Amp))
        result.children.push_back(parsePatternPrimary(implicitBinding));
    result.range = SourceRange(
        result.children.front().range.begin(), result.children.back().range.end());
    return result;
}

Parser::Pattern Parser::parsePattern(bool implicitBinding)
{
    Pattern first = parseAndPattern(implicitBinding);
    if (!at(TokenKind::Pipe)) return first;
    Pattern result;
    result.kind = PatternKind::Or;
    result.range = first.range;
    result.children.push_back(std::move(first));
    while (consumeIf(TokenKind::Pipe))
        result.children.push_back(parseAndPattern(implicitBinding));
    result.range = SourceRange(
        result.children.front().range.begin(), result.children.back().range.end());
    return result;
}

bool Parser::patternHasBindings(const Pattern& pattern) const
{
    if (pattern.kind == PatternKind::Binding) return true;
    return std::ranges::any_of(pattern.children, [this](const Pattern& child)
    {
        return patternHasBindings(child);
    });
}

std::vector<Parser::Pattern> Parser::expandPatternAlternatives(const Pattern& pattern) const
{
    if (pattern.kind == PatternKind::Or)
    {
        std::vector<Pattern> alternatives;
        for (const Pattern& child : pattern.children)
        {
            std::vector<Pattern> expanded = expandPatternAlternatives(child);
            alternatives.insert(
                alternatives.end(),
                std::make_move_iterator(expanded.begin()),
                std::make_move_iterator(expanded.end()));
        }
        return alternatives;
    }

    std::vector<Pattern> expanded{pattern};
    expanded.front().children.clear();
    for (const Pattern& child : pattern.children)
    {
        const std::vector<Pattern> childAlternatives = expandPatternAlternatives(child);
        std::vector<Pattern> product;
        product.reserve(expanded.size() * childAlternatives.size());
        for (const Pattern& prefix : expanded)
        {
            for (const Pattern& alternative : childAlternatives)
            {
                Pattern copy = prefix;
                copy.children.push_back(alternative);
                product.push_back(std::move(copy));
            }
        }
        expanded = std::move(product);
    }
    return expanded;
}

ast::AstNode* Parser::makeBooleanLiteral(const SourceRange& range, bool value)
{
    ast::AstNode* literal = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BooleanLiteralExpression, range);
    literal->setTokenKind(value ? TokenKind::KwTrue : TokenKind::KwFalse);
    literal->setIntegerValue(value ? 1U : 0U);
    return literal;
}

bool Parser::patternIsConditionOnly(const Pattern& pattern) const
{
    if (pattern.kind == PatternKind::Binding || pattern.kind == PatternKind::Tuple ||
        pattern.kind == PatternKind::List)
        return false;
    return std::ranges::all_of(
        pattern.children,
        [this](const Pattern& child) { return patternIsConditionOnly(child); });
}

ast::AstNode* Parser::makeRelationalPatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    TokenKind operation = TokenKind::Less;
    if (pattern.name == "<=") operation = TokenKind::LessEqual;
    else if (pattern.name == ">") operation = TokenKind::Greater;
    else if (pattern.name == ">=") operation = TokenKind::GreaterEqual;
    return makeBinaryExpression(operation, source(), pattern.firstValue);
}

ast::AstNode* Parser::makeStructPatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    ast::AstNode* result = makeBooleanLiteral(pattern.range, true);
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string field = pattern.fieldNames[index];
        PatternExpressionFactory member = [&, source, field]
        {
            return makeMemberExpression(source(), field);
        };
        result = makeBinaryExpression(
            TokenKind::AmpAmp,
            result,
            makePatternCondition(pattern.children[index], member));
    }
    return result;
}

ast::AstNode* Parser::makeVariantPatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    ast::AstNode* test = makeZeroArgCallExpression(
        makeMemberExpression(source(), "Is" + pattern.name));
    if (pattern.children.empty()) return test;
    PatternExpressionFactory payload = [&, source, caseName = pattern.name]
    {
        ast::AstNode* extract = makeZeroArgCallExpression(
            makeMemberExpression(source(), "__nsc_variant_extract_" + caseName));
        extract->setAuxiliaryText("variant-pattern-extract");
        return extract;
    };
    return makeBinaryExpression(
        TokenKind::AmpAmp,
        test,
        makePatternCondition(pattern.children.front(), payload));
}

ast::AstNode* Parser::makeLogicalPatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    TokenKind operation)
{
    ast::AstNode* result = makePatternCondition(pattern.children.front(), source);
    for (std::size_t index = 1; index < pattern.children.size(); ++index)
        result = makeBinaryExpression(
            operation,
            result,
            makePatternCondition(pattern.children[index], source));
    return result;
}

ast::AstNode* Parser::makeListPatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    const std::size_t restOffset = pattern.restIndex.value_or(pattern.children.size());
    const std::size_t fixedCount = pattern.children.size() -
        (pattern.restIndex.has_value() ? 1U : 0U);
    ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
    lengthMember->setAuxiliaryText("list-pattern-length");
    ast::AstNode* condition = makeBinaryExpression(
        pattern.restIndex.has_value() ? TokenKind::GreaterEqual : TokenKind::EqualEqual,
        lengthMember,
        makeIntegerLiteralExpression(pattern.range, fixedCount));
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        if (pattern.restIndex == index) continue;
        ast::AstNode* elementIndex = nullptr;
        if (!pattern.restIndex.has_value() || index < restOffset)
            elementIndex = makeIntegerLiteralExpression(pattern.range, index);
        else
        {
            const std::size_t fromEnd = pattern.children.size() - index;
            ast::AstNode* trailingLength = makeMemberExpression(source(), "Length");
            trailingLength->setAuxiliaryText("list-pattern-length");
            elementIndex = makeBinaryExpression(
                TokenKind::Minus,
                trailingLength,
                makeIntegerLiteralExpression(pattern.range, fromEnd));
        }
        PatternExpressionFactory element = [&, source, elementIndex]
        {
            return makeIndexExpression(source(), cloneDeclarationNode(elementIndex));
        };
        condition = makeBinaryExpression(
            TokenKind::AmpAmp,
            condition,
            makePatternCondition(pattern.children[index], element));
    }
    return condition;
}

ast::AstNode* Parser::makeNotPatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    ast::AstNode* negated = makePatternCondition(pattern.children.front(), source);
    ast::AstNode* unary = arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, pattern.range);
    unary->setTokenKind(TokenKind::Bang);
    unary->setFirst(negated);
    return unary;
}

ast::AstNode* Parser::makePatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    switch (pattern.kind)
    {
        case PatternKind::Wildcard:
        case PatternKind::Binding:
        case PatternKind::Positional:
            return makeBooleanLiteral(pattern.range, true);
        case PatternKind::Tuple:
            return makeBooleanLiteral(pattern.range, false);
        case PatternKind::Literal:
            return makeBinaryExpression(TokenKind::EqualEqual, source(), pattern.firstValue);
        case PatternKind::Range:
            return makeBinaryExpression(
                TokenKind::AmpAmp,
                makeBinaryExpression(TokenKind::GreaterEqual, source(), pattern.firstValue),
                makeBinaryExpression(
                    pattern.name == "..=" ? TokenKind::LessEqual : TokenKind::Less,
                    source(),
                    pattern.secondValue));
        case PatternKind::Relational:
            return makeRelationalPatternCondition(pattern, source);
        case PatternKind::Struct:
            return makeStructPatternCondition(pattern, source);
        case PatternKind::Variant:
            return makeVariantPatternCondition(pattern, source);
        case PatternKind::Or:
            return makeLogicalPatternCondition(pattern, source, TokenKind::PipePipe);
        case PatternKind::List:
            return makeListPatternCondition(pattern, source);
        case PatternKind::Not:
            return makeNotPatternCondition(pattern, source);
        case PatternKind::And:
            return makeLogicalPatternCondition(pattern, source, TokenKind::AmpAmp);
    }
    return makeBooleanLiteral(pattern.range, false);
}

ast::AstNode* Parser::lowerBindingPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    bool emitBindings)
{
    if (!emitBindings) return continuation;
    ast::AstNode* block = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    ast::AstNode* declaration = makeVariableDeclaration(
        type,
        pattern.name,
        source(),
        false,
        pattern.bindingMode == PatternBindingMode::Value ||
            pattern.bindingMode == PatternBindingMode::Copy ||
            pattern.bindingMode == PatternBindingMode::Reference);
    applyPatternBindingMode(declaration, pattern.bindingMode);
    block->addChild(declaration);
    block->addChild(continuation);
    return block;
}

ast::AstNode* Parser::lowerOrPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    ast::AstNode* block = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    const std::string selectedName = std::string(temporaryPrefix) + "_or_selected_" +
        std::to_string(nextMatchTemporary_++);
    ast::TypeSyntax* boolType = arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwBool, "bool");
    block->addChild(makeVariableDeclaration(
        boolType,
        selectedName,
        makeBooleanLiteral(pattern.range, false),
        false,
        false));

    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        ast::AstNode* selectedBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.children[index].range);
        selectedBody->addChild(makeExpressionStatement(makeAssignmentExpression(
            makeIdentifierExpression(pattern.children[index].range, selectedName),
            makeBooleanLiteral(pattern.children[index].range, true))));
        selectedBody->addChild(index + 1U == pattern.children.size()
            ? continuation
            : cloneDeclarationNode(continuation));
        ast::AstNode* matcher = lowerPattern(
            pattern.children[index], source, selectedBody, temporaryPrefix, emitBindings);

        ast::AstNode* notSelected = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, pattern.children[index].range);
        notSelected->setTokenKind(TokenKind::Bang);
        notSelected->setFirst(makeIdentifierExpression(
            pattern.children[index].range, selectedName));
        ast::AstNode* guarded = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, pattern.children[index].range);
        guarded->setFirst(notSelected);
        ast::AstNode* guardedBody = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, pattern.children[index].range);
        guardedBody->addChild(matcher);
        guarded->setSecond(guardedBody);
        block->addChild(guarded);
    }
    return block;
}

ast::AstNode* Parser::lowerTuplePattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    ast::AstNode* block = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
    deconstruction->setFirst(source());
    std::vector<std::string> elementNames;
    elementNames.reserve(pattern.children.size());
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = std::string(temporaryPrefix) + "_tuple_" +
            std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
        elementNames.push_back(name);
        deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
    }
    block->addChild(deconstruction);
    ast::AstNode* next = continuation;
    for (std::size_t index = pattern.children.size(); index > 0; --index)
    {
        const std::string name = elementNames[index - 1];
        PatternExpressionFactory element = [&, name]
        {
            return makeIdentifierExpression(pattern.range, name);
        };
        next = lowerPattern(
            pattern.children[index - 1], element, next, temporaryPrefix, emitBindings);
    }
    block->addChild(next);
    return block;
}

ast::AstNode* Parser::lowerStructPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    ast::AstNode* next = continuation;
    for (std::size_t index = pattern.children.size(); index > 0; --index)
    {
        const std::string field = pattern.fieldNames[index - 1];
        PatternExpressionFactory member = [&, source, field]
        {
            return makeMemberExpression(source(), field);
        };
        next = lowerPattern(
            pattern.children[index - 1], member, next, temporaryPrefix, emitBindings);
    }
    return next;
}

ast::AstNode* Parser::lowerListPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    ast::AstNode* thenBlock = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    if (emitBindings)
        emitPatternBindings(pattern, source, thenBlock, temporaryPrefix);
    thenBlock->addChild(continuation);
    ast::AstNode* decision = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(makePatternCondition(pattern, source));
    decision->setSecond(thenBlock);
    return decision;
}

ast::AstNode* Parser::lowerPositionalPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    ast::AstNode* block = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
    deconstruction->setFirst(source());
    deconstruction->setTypeSyntax(arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::Identifier, pattern.name));
    std::vector<std::string> elementNames;
    elementNames.reserve(pattern.children.size());
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = std::string(temporaryPrefix) + "_pos_" +
            std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
        elementNames.push_back(name);
        deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
    }
    block->addChild(deconstruction);

    ast::AstNode* next = continuation;
    for (std::size_t index = pattern.children.size(); index > 0; --index)
    {
        const std::string name = elementNames[index - 1];
        PatternExpressionFactory element = [&, name]
        {
            return makeIdentifierExpression(pattern.range, name);
        };
        next = lowerPattern(
            pattern.children[index - 1], element, next, temporaryPrefix, emitBindings);
    }
    block->addChild(next);
    return block;
}

ast::AstNode* Parser::lowerVariantPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    ast::AstNode* test = makeZeroArgCallExpression(
        makeMemberExpression(source(), "Is" + pattern.name));
    ast::AstNode* thenBlock = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    if (pattern.children.empty() || pattern.children.front().kind == PatternKind::Wildcard)
        thenBlock->addChild(continuation);
    else
    {
        const std::string payloadName = std::string(temporaryPrefix) + "_payload_" +
            std::to_string(nextMatchTemporary_++);
        ast::AstNode* extract = makeZeroArgCallExpression(
            makeMemberExpression(source(), "__nsc_variant_extract_" + pattern.name));
        extract->setAuxiliaryText("variant-pattern-extract");
        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwVar, "var");
        thenBlock->addChild(makeVariableDeclaration(type, payloadName, extract, false, true));
        PatternExpressionFactory payload = [&, payloadName]
        {
            return makeIdentifierExpression(pattern.range, payloadName);
        };
        thenBlock->addChild(lowerPattern(
            pattern.children.front(), payload, continuation, temporaryPrefix, emitBindings));
    }
    ast::AstNode* decision = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(test);
    decision->setSecond(thenBlock);
    return decision;
}

ast::AstNode* Parser::lowerPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    if (pattern.kind == PatternKind::Wildcard) return continuation;
    if (pattern.kind == PatternKind::Binding)
        return lowerBindingPattern(pattern, source, continuation, emitBindings);
    if (pattern.kind == PatternKind::Or && !patternIsConditionOnly(pattern))
        return lowerOrPattern(
            pattern, source, continuation, temporaryPrefix, emitBindings);
    if (pattern.kind == PatternKind::Tuple)
        return lowerTuplePattern(
            pattern, source, continuation, temporaryPrefix, emitBindings);
    if (pattern.kind == PatternKind::Struct)
        return lowerStructPattern(
            pattern, source, continuation, temporaryPrefix, emitBindings);
    if (pattern.kind == PatternKind::List)
        return lowerListPattern(
            pattern, source, continuation, temporaryPrefix, emitBindings);
    if (pattern.kind == PatternKind::Positional)
        return lowerPositionalPattern(
            pattern, source, continuation, temporaryPrefix, emitBindings);
    if (pattern.kind == PatternKind::Variant)
        return lowerVariantPattern(
            pattern, source, continuation, temporaryPrefix, emitBindings);
    ast::AstNode* decision = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(makePatternCondition(pattern, source));
    decision->setSecond(continuation);
    return decision;
}

void Parser::collectUniquePatternBindings(
    const Pattern& pattern,
    std::vector<std::pair<std::string, PatternBindingMode>>& bindings) const
{
    if (pattern.kind == PatternKind::Binding)
    {
        if (std::ranges::none_of(bindings, [&](const auto& binding)
            {
                return binding.first == pattern.name;
            }))
            bindings.emplace_back(pattern.name, pattern.bindingMode);
        return;
    }
    for (const Pattern& child : pattern.children)
        collectUniquePatternBindings(child, bindings);
}

void Parser::applyPatternBindingMode(
    ast::AstNode* declaration,
    PatternBindingMode mode) const
{
    if (mode == PatternBindingMode::Reference)
        declaration->setAuxiliaryText("local:pattern-ref");
    else if (mode == PatternBindingMode::ReferenceMutable)
        declaration->setAuxiliaryText("local:pattern-ref-mut");
}

void Parser::emitOrPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    const std::vector<Pattern> alternatives = expandPatternAlternatives(pattern);
    std::vector<std::pair<std::string, PatternBindingMode>> bindings;
    if (!alternatives.empty())
        collectUniquePatternBindings(alternatives.front(), bindings);

    for (const auto& [name, mode] : bindings)
    {
        ast::AstNode* selector = arena_.make<ast::AstNode>(
            ast::AstNodeKind::MatchExpression, pattern.range);
        selector->setFirst(source());
        selector->setText(std::string(temporaryPrefix) + "_or_binding_" +
            std::to_string(nextMatchTemporary_++));
        for (const Pattern& alternative : alternatives)
        {
            ast::AstNode* arm = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MatchArmExpression, alternative.range);
            PatternExpressionFactory matchValue = [&, range = alternative.range]
            {
                return arena_.make<ast::AstNode>(
                    ast::AstNodeKind::MatchValueExpression, range);
            };
            arm->setFirst(makePatternCondition(alternative, matchValue));
            emitPatternBindings(alternative, matchValue, arm, temporaryPrefix);
            arm->setThird(makeIdentifierExpression(alternative.range, name));
            arm->setFlag(mode == PatternBindingMode::Reference ||
                mode == PatternBindingMode::ReferenceMutable);
            selector->addChild(arm);
        }

        ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
            pattern.range, TokenKind::KwVar, "var");
        ast::AstNode* declaration = makeVariableDeclaration(
            type, name, selector, false,
            mode == PatternBindingMode::Value || mode == PatternBindingMode::Copy ||
                mode == PatternBindingMode::Reference);
        applyPatternBindingMode(declaration, mode);
        destination->addChild(declaration);
    }
}

void Parser::emitSinglePatternBinding(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination)
{
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    ast::AstNode* initializer = source();
    // A rest value is a borrowed view. Its local cell is compiler-generated, but making that
    // cell const would also make a mutable Span receiver readonly and incorrectly suppress
    // writes to the referenced storage. Readonly sources already produce ReadOnlySpan<T>.
    const bool mutableRestView =
        pattern.bindingMode == PatternBindingMode::Value &&
        initializer != nullptr &&
        initializer->kind() == ast::AstNodeKind::CallExpression &&
        initializer->auxiliaryText() == "list-pattern-rest";
    ast::AstNode* declaration = makeVariableDeclaration(
        type, pattern.name, initializer, false,
        !mutableRestView &&
            (pattern.bindingMode == PatternBindingMode::Value ||
             pattern.bindingMode == PatternBindingMode::Copy ||
             pattern.bindingMode == PatternBindingMode::Reference));
    applyPatternBindingMode(declaration, pattern.bindingMode);
    destination->addChild(declaration);
}

void Parser::emitTuplePatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
    deconstruction->setFirst(source());
    std::vector<std::string> elementNames;
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = std::string(temporaryPrefix) + "_tuple_" +
            std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
        elementNames.push_back(name);
        deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
    }
    destination->addChild(deconstruction);
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = elementNames[index];
        PatternExpressionFactory element = [&, name]
        {
            return makeIdentifierExpression(pattern.range, name);
        };
        emitPatternBindings(
            pattern.children[index], element, destination, temporaryPrefix);
    }
}

void Parser::emitPositionalPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    // Deconstruct once inside the selected arm. The arm guard runs after its declarations, so
    // child predicates can consume these temporaries without repeating a stateful Deconstruct.
    ast::AstNode* deconstruction = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, pattern.range);
    deconstruction->setFirst(source());
    deconstruction->setTypeSyntax(arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::Identifier, pattern.name));
    std::vector<std::string> elementNames;
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = std::string(temporaryPrefix) + "_pos_" +
            std::to_string(nextMatchTemporary_++) + "_" + std::to_string(index);
        elementNames.push_back(name);
        deconstruction->addChild(makeIdentifierExpression(pattern.range, name));
    }
    destination->addChild(deconstruction);
    ast::AstNode* positionalCondition = makeBooleanLiteral(pattern.range, true);
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string name = elementNames[index];
        PatternExpressionFactory element = [&, name]
        {
            return makeIdentifierExpression(pattern.range, name);
        };
        emitPatternBindings(
            pattern.children[index], element, destination, temporaryPrefix);
        positionalCondition = makeBinaryExpression(
            TokenKind::AmpAmp,
            positionalCondition,
            makePatternCondition(pattern.children[index], element));
    }
    if (destination->kind() == ast::AstNodeKind::MatchArmExpression)
    {
        if (ast::AstNode* guard = destination->second(); guard != nullptr)
        {
            positionalCondition = makeBinaryExpression(
                TokenKind::AmpAmp, positionalCondition, guard);
        }
        destination->setSecond(positionalCondition);
    }
}

void Parser::emitStructPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        const std::string field = pattern.fieldNames[index];
        PatternExpressionFactory member = [&, source, field]
        {
            return makeMemberExpression(source(), field);
        };
        emitPatternBindings(
            pattern.children[index], member, destination, temporaryPrefix);
    }
}

void Parser::emitListPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    const std::size_t restOffset = pattern.restIndex.value_or(pattern.children.size());
    for (std::size_t index = 0; index < pattern.children.size(); ++index)
    {
        if (pattern.restIndex == index)
        {
            PatternExpressionFactory rest = [&, source]
            {
                const std::size_t fixedCount = pattern.children.size() - 1U;
                ast::AstNode* call = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::CallExpression, pattern.children[index].range);
                call->setFirst(makeMemberExpression(source(), "Slice"));
                call->setAuxiliaryText("list-pattern-rest");
                call->addChild(makeIntegerLiteralExpression(pattern.range, restOffset));
                ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
                lengthMember->setAuxiliaryText("list-pattern-length");
                call->addChild(makeBinaryExpression(
                    TokenKind::Minus, lengthMember,
                    makeIntegerLiteralExpression(pattern.range, fixedCount)));
                return call;
            };
            emitPatternBindings(
                pattern.children[index], rest, destination, temporaryPrefix);
            continue;
        }
        PatternExpressionFactory element = [&, source, index, restOffset]
        {
            ast::AstNode* elementIndex = nullptr;
            if (!pattern.restIndex.has_value() || index < restOffset)
                elementIndex = makeIntegerLiteralExpression(pattern.range, index);
            else
            {
                const std::size_t fromEnd = pattern.children.size() - index;
                ast::AstNode* lengthMember = makeMemberExpression(source(), "Length");
                lengthMember->setAuxiliaryText("list-pattern-length");
                elementIndex = makeBinaryExpression(
                    TokenKind::Minus, lengthMember,
                    makeIntegerLiteralExpression(pattern.range, fromEnd));
            }
            return makeIndexExpression(source(), elementIndex);
        };
        emitPatternBindings(
            pattern.children[index], element, destination, temporaryPrefix);
    }
}

void Parser::emitVariantPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    const std::string payloadName = std::string(temporaryPrefix) + "_payload_" +
        std::to_string(nextMatchTemporary_++);
    ast::AstNode* extract = makeZeroArgCallExpression(
        makeMemberExpression(source(), "__nsc_variant_extract_" + pattern.name));
    extract->setAuxiliaryText("variant-pattern-extract");
    ast::TypeSyntax* type = arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    destination->addChild(makeVariableDeclaration(type, payloadName, extract, false, true));
    PatternExpressionFactory payload = [&, payloadName]
    {
        return makeIdentifierExpression(pattern.range, payloadName);
    };
    emitPatternBindings(
        pattern.children.front(), payload, destination, temporaryPrefix);
}

void Parser::emitPatternBindings(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* destination,
    std::string_view temporaryPrefix)
{
    if (pattern.kind == PatternKind::Or && patternHasBindings(pattern))
        return emitOrPatternBindings(pattern, source, destination, temporaryPrefix);
    if (pattern.kind == PatternKind::Binding)
        return emitSinglePatternBinding(pattern, source, destination);
    if (pattern.kind == PatternKind::Tuple)
        return emitTuplePatternBindings(pattern, source, destination, temporaryPrefix);
    if (pattern.kind == PatternKind::Positional)
        return emitPositionalPatternBindings(pattern, source, destination, temporaryPrefix);
    if (pattern.kind == PatternKind::Struct)
        return emitStructPatternBindings(pattern, source, destination, temporaryPrefix);
    if (pattern.kind == PatternKind::List)
        return emitListPatternBindings(pattern, source, destination, temporaryPrefix);
    if (pattern.kind == PatternKind::Variant && !pattern.children.empty())
        emitVariantPatternBindings(pattern, source, destination, temporaryPrefix);
}
}
