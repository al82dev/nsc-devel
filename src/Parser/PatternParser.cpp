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

class Parser::PatternIterationParser final
{
public:
    PatternIterationParser(Parser& parser, bool implicitBinding)
        : parser_(parser), implicitBinding_(implicitBinding)
    {
    }

    Pattern run();

private:
    enum class WorkKind
    {
        ParseOr,
        FinishOr,
        ParseAnd,
        FinishAnd,
        ParsePrimary,
        FinishNot,
        ContinueList,
        FinishListChild,
        FinishParenthesizedFirst,
        ContinueTuple,
        FinishTupleChild,
        ContinueStruct,
        FinishStructChild,
        ContinueNamedPayload,
        FinishNamedPayloadChild
    };

    struct Work final
    {
        WorkKind kind = WorkKind::ParseOr;
        bool implicitBinding = false;
        bool initial = false;
        std::size_t begin = 0;
        Pattern pattern;
        std::unordered_set<std::string> fields;
    };

    void schedule(WorkKind kind, bool implicitBinding = false, std::size_t begin = 0);
    void schedule(Work work);
    void dispatch(Work work);
    Pattern takeValue();
    void parseOr(bool implicitBinding);
    void finishOr(Work work);
    void parseAnd(bool implicitBinding);
    void finishAnd(Work work);
    void parsePrimary(bool implicitBinding);
    void finishNot(std::size_t begin);
    void continueList(Work work);
    void finishListChild(Work work);
    void finishListElement(Work work, Pattern child);
    void finishParenthesizedFirst(std::size_t begin);
    void continueTuple(Work work);
    void finishTupleChild(Work work);
    void finishTuple(Work work);
    void startStruct(Pattern pattern, std::size_t begin);
    void continueStruct(Work work);
    void finishStructChild(Work work);
    void finishStructElement(Work work, Pattern child);
    void parseNamed(std::size_t begin, bool implicitBinding);
    void continueNamedPayload(Work work);
    void finishNamedPayloadChild(Work work);
    void finishNamedPayload(Work work);

    Parser& parser_;
    bool implicitBinding_ = false;
    std::vector<Work> work_;
    std::vector<Pattern> values_;
};

class Parser::PatternAlternativeExpander final
{
public:
    explicit PatternAlternativeExpander(const Pattern& root) : root_(root) {}

    std::vector<Pattern> run();

private:
    struct Frame final
    {
        const Pattern* source = nullptr;
        std::size_t nextChild = 0;
        std::vector<Pattern> alternatives;
    };

    static Pattern copyWithoutChildren(const Pattern& source);
    static Frame makeFrame(const Pattern& source);
    static void appendOrAlternatives(
        Frame& frame,
        std::vector<Pattern> alternatives);
    static void appendChildAlternatives(
        Frame& frame,
        std::vector<Pattern> alternatives);
    static void integrateCompletedChild(
        Frame& frame,
        std::vector<Pattern> alternatives);

    const Pattern& root_;
};

Parser::Pattern Parser::PatternIterationParser::run()
{
    schedule(WorkKind::ParseOr, implicitBinding_);
    while (!work_.empty())
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        dispatch(std::move(work));
    }
    return takeValue();
}

void Parser::PatternIterationParser::schedule(
    WorkKind kind,
    bool implicitBinding,
    std::size_t begin)
{
    Work work;
    work.kind = kind;
    work.implicitBinding = implicitBinding;
    work.begin = begin;
    work_.push_back(std::move(work));
}

void Parser::PatternIterationParser::schedule(Work work)
{
    work_.push_back(std::move(work));
}

Parser::Pattern Parser::PatternIterationParser::takeValue()
{
    Pattern result = std::move(values_.back());
    values_.pop_back();
    return result;
}

void Parser::PatternIterationParser::dispatch(Work work)
{
    switch (work.kind)
    {
        case WorkKind::ParseOr: return parseOr(work.implicitBinding);
        case WorkKind::FinishOr: return finishOr(std::move(work));
        case WorkKind::ParseAnd: return parseAnd(work.implicitBinding);
        case WorkKind::FinishAnd: return finishAnd(std::move(work));
        case WorkKind::ParsePrimary: return parsePrimary(work.implicitBinding);
        case WorkKind::FinishNot: return finishNot(work.begin);
        case WorkKind::ContinueList: return continueList(std::move(work));
        case WorkKind::FinishListChild: return finishListChild(std::move(work));
        case WorkKind::FinishParenthesizedFirst:
            return finishParenthesizedFirst(work.begin);
        case WorkKind::ContinueTuple: return continueTuple(std::move(work));
        case WorkKind::FinishTupleChild: return finishTupleChild(std::move(work));
        case WorkKind::ContinueStruct: return continueStruct(std::move(work));
        case WorkKind::FinishStructChild: return finishStructChild(std::move(work));
        case WorkKind::ContinueNamedPayload:
            return continueNamedPayload(std::move(work));
        case WorkKind::FinishNamedPayloadChild:
            return finishNamedPayloadChild(std::move(work));
    }
}

void Parser::PatternIterationParser::parseOr(bool implicitBinding)
{
    Work finish;
    finish.kind = WorkKind::FinishOr;
    finish.implicitBinding = implicitBinding;
    finish.initial = true;
    schedule(std::move(finish));
    schedule(WorkKind::ParseAnd, implicitBinding);
}

void Parser::PatternIterationParser::finishOr(Work work)
{
    Pattern next = takeValue();
    if (work.initial)
    {
        if (!parser_.at(TokenKind::Pipe))
        {
            values_.push_back(std::move(next));
            return;
        }
        work.pattern.kind = PatternKind::Or;
        work.pattern.range = next.range;
        work.initial = false;
    }
    work.pattern.children.push_back(std::move(next));
    if (parser_.consumeIf(TokenKind::Pipe))
    {
        const bool implicitBinding = work.implicitBinding;
        schedule(std::move(work));
        schedule(WorkKind::ParseAnd, implicitBinding);
        return;
    }
    work.pattern.range = SourceRange(
        work.pattern.children.front().range.begin(), work.pattern.children.back().range.end());
    values_.push_back(std::move(work.pattern));
}

void Parser::PatternIterationParser::parseAnd(bool implicitBinding)
{
    Work finish;
    finish.kind = WorkKind::FinishAnd;
    finish.implicitBinding = implicitBinding;
    finish.initial = true;
    schedule(std::move(finish));
    schedule(WorkKind::ParsePrimary, implicitBinding);
}

void Parser::PatternIterationParser::finishAnd(Work work)
{
    Pattern next = takeValue();
    if (work.initial)
    {
        if (!parser_.at(TokenKind::Amp))
        {
            values_.push_back(std::move(next));
            return;
        }
        work.pattern.kind = PatternKind::And;
        work.pattern.range = next.range;
        work.initial = false;
    }
    work.pattern.children.push_back(std::move(next));
    if (parser_.consumeIf(TokenKind::Amp))
    {
        const bool implicitBinding = work.implicitBinding;
        schedule(std::move(work));
        schedule(WorkKind::ParsePrimary, implicitBinding);
        return;
    }
    work.pattern.range = SourceRange(
        work.pattern.children.front().range.begin(), work.pattern.children.back().range.end());
    values_.push_back(std::move(work.pattern));
}

void Parser::PatternIterationParser::parsePrimary(bool implicitBinding)
{
    const std::size_t begin = parser_.index_;
    const SourceRange start = parser_.current().range();
    if (parser_.at(TokenKind::Identifier) && parser_.current().lexeme() == "_")
    {
        parser_.consume();
        Pattern result;
        result.kind = PatternKind::Wildcard;
        result.range = start;
        values_.push_back(std::move(result));
        return;
    }
    if (parser_.consumeIf(TokenKind::Bang))
    {
        schedule(WorkKind::FinishNot, true, begin);
        schedule(WorkKind::ParsePrimary, true);
        return;
    }
    if (parser_.at(TokenKind::LeftBracket))
    {
        parser_.consume();
        Work list;
        list.kind = WorkKind::ContinueList;
        list.begin = begin;
        list.pattern.kind = PatternKind::List;
        schedule(std::move(list));
        return;
    }
    if (parser_.at(TokenKind::KwVar) || parser_.at(TokenKind::KwMove) ||
        parser_.atContextualKeyword("copy") || parser_.atContextualKeyword("ref"))
    {
        values_.push_back(parser_.parseBindingPattern(begin));
        return;
    }
    if (parser_.at(TokenKind::Less) || parser_.at(TokenKind::LessEqual) ||
        parser_.at(TokenKind::Greater) || parser_.at(TokenKind::GreaterEqual))
    {
        values_.push_back(parser_.parseRelationalPattern(begin));
        return;
    }
    if (parser_.patternLiteralStartsHere())
    {
        values_.push_back(parser_.parseLiteralOrRangePattern(begin, start));
        return;
    }
    if (parser_.at(TokenKind::LeftParen))
    {
        parser_.consume();
        if (parser_.consumeIf(TokenKind::RightParen))
        {
            Pattern tuple;
            tuple.kind = PatternKind::Tuple;
            tuple.range = parser_.tokenRangeFrom(begin);
            values_.push_back(std::move(tuple));
            return;
        }
        schedule(WorkKind::FinishParenthesizedFirst, true, begin);
        schedule(WorkKind::ParseOr, true);
        return;
    }
    if (parser_.at(TokenKind::LeftBrace))
    {
        startStruct(Pattern{}, begin);
        return;
    }
    if (parser_.at(TokenKind::Identifier))
    {
        parseNamed(begin, implicitBinding);
        return;
    }
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        parser_.current().range(),
        "expected wildcard, literal, binding, tuple, struct, variant, range, or list pattern");
    if (!parser_.at(TokenKind::EndOfFile)) parser_.consume();
    Pattern error;
    error.range = start;
    values_.push_back(std::move(error));
}

void Parser::PatternIterationParser::finishNot(std::size_t begin)
{
    Pattern result;
    result.kind = PatternKind::Not;
    result.children.push_back(takeValue());
    result.range = parser_.tokenRangeFrom(begin);
    values_.push_back(std::move(result));
}

void Parser::PatternIterationParser::continueList(Work work)
{
    if (parser_.at(TokenKind::RightBracket) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBracket, "']' after list pattern");
        work.pattern.range = parser_.tokenRangeFrom(work.begin);
        values_.push_back(std::move(work.pattern));
        return;
    }
    if (parser_.consumeIf(TokenKind::DotDot))
    {
        if (work.pattern.restIndex.has_value())
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                parser_.tokens_[parser_.index_ - 1].range(),
                "a list pattern accepts at most one rest element");
        else
            work.pattern.restIndex = work.pattern.children.size();
        if (parser_.at(TokenKind::Comma) || parser_.at(TokenKind::RightBracket))
        {
            Pattern rest;
            rest.kind = PatternKind::Wildcard;
            rest.range = parser_.tokens_[parser_.index_ - 1].range();
            finishListElement(std::move(work), std::move(rest));
            return;
        }
    }
    work.kind = WorkKind::FinishListChild;
    schedule(std::move(work));
    schedule(WorkKind::ParseOr, true);
}

void Parser::PatternIterationParser::finishListChild(Work work)
{
    finishListElement(std::move(work), takeValue());
}

void Parser::PatternIterationParser::finishListElement(Work work, Pattern child)
{
    work.pattern.children.push_back(std::move(child));
    if (parser_.consumeIf(TokenKind::Comma))
    {
        work.kind = WorkKind::ContinueList;
        schedule(std::move(work));
        return;
    }
    parser_.expect(TokenKind::RightBracket, "']' after list pattern");
    work.pattern.range = parser_.tokenRangeFrom(work.begin);
    values_.push_back(std::move(work.pattern));
}

void Parser::PatternIterationParser::finishParenthesizedFirst(std::size_t begin)
{
    Pattern first = takeValue();
    if (!parser_.consumeIf(TokenKind::Comma))
    {
        parser_.expect(TokenKind::RightParen, "')' after grouped pattern");
        first.range = parser_.tokenRangeFrom(begin);
        values_.push_back(std::move(first));
        return;
    }
    Work tuple;
    tuple.kind = WorkKind::ContinueTuple;
    tuple.begin = begin;
    tuple.pattern.kind = PatternKind::Tuple;
    tuple.pattern.children.push_back(std::move(first));
    schedule(std::move(tuple));
}

void Parser::PatternIterationParser::continueTuple(Work work)
{
    if (parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
    {
        finishTuple(std::move(work));
        return;
    }
    work.kind = WorkKind::FinishTupleChild;
    schedule(std::move(work));
    schedule(WorkKind::ParseOr, true);
}

void Parser::PatternIterationParser::finishTupleChild(Work work)
{
    work.pattern.children.push_back(takeValue());
    if (parser_.consumeIf(TokenKind::Comma))
    {
        work.kind = WorkKind::ContinueTuple;
        schedule(std::move(work));
        return;
    }
    finishTuple(std::move(work));
}

void Parser::PatternIterationParser::finishTuple(Work work)
{
    parser_.expect(TokenKind::RightParen, "')' after tuple pattern");
    work.pattern.range = parser_.tokenRangeFrom(work.begin);
    values_.push_back(std::move(work.pattern));
}

void Parser::PatternIterationParser::startStruct(Pattern pattern, std::size_t begin)
{
    pattern.kind = PatternKind::Struct;
    parser_.expect(TokenKind::LeftBrace, "'{' to open struct pattern");
    Work work;
    work.kind = WorkKind::ContinueStruct;
    work.begin = begin;
    work.pattern = std::move(pattern);
    schedule(std::move(work));
}

void Parser::PatternIterationParser::continueStruct(Work work)
{
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBrace, "'}' after struct pattern");
        work.pattern.range = parser_.tokenRangeFrom(work.begin);
        values_.push_back(std::move(work.pattern));
        return;
    }
    if (!parser_.at(TokenKind::Identifier))
    {
        parser_.expect(TokenKind::Identifier, "field name in struct pattern");
        parser_.expect(TokenKind::RightBrace, "'}' after struct pattern");
        work.pattern.range = parser_.tokenRangeFrom(work.begin);
        values_.push_back(std::move(work.pattern));
        return;
    }
    const lexer::Token& field = parser_.consume();
    const std::string fieldName(field.lexeme());
    if (!work.fields.insert(fieldName).second)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            field.range(),
            "duplicate field '" + fieldName + "' in struct pattern");
    work.pattern.fieldNames.push_back(fieldName);
    if (parser_.consumeIf(TokenKind::Colon))
    {
        work.kind = WorkKind::FinishStructChild;
        schedule(std::move(work));
        schedule(WorkKind::ParseOr, true);
        return;
    }
    Pattern shorthand;
    shorthand.kind = PatternKind::Binding;
    shorthand.name = fieldName;
    shorthand.range = field.range();
    finishStructElement(std::move(work), std::move(shorthand));
}

void Parser::PatternIterationParser::finishStructChild(Work work)
{
    finishStructElement(std::move(work), takeValue());
}

void Parser::PatternIterationParser::finishStructElement(Work work, Pattern child)
{
    work.pattern.children.push_back(std::move(child));
    if (parser_.consumeIf(TokenKind::Comma))
    {
        work.kind = WorkKind::ContinueStruct;
        schedule(std::move(work));
        return;
    }
    parser_.expect(TokenKind::RightBrace, "'}' after struct pattern");
    work.pattern.range = parser_.tokenRangeFrom(work.begin);
    values_.push_back(std::move(work.pattern));
}

void Parser::PatternIterationParser::parseNamed(
    std::size_t begin,
    bool implicitBinding)
{
    Pattern result;
    const lexer::Token& name = parser_.consume();
    result.name = std::string(name.lexeme());
    if (parser_.at(TokenKind::LeftBrace))
    {
        startStruct(std::move(result), begin);
        return;
    }
    if (parser_.consumeIf(TokenKind::LeftParen))
    {
        Work payload;
        payload.kind = WorkKind::ContinueNamedPayload;
        payload.begin = begin;
        payload.pattern = std::move(result);
        if (parser_.at(TokenKind::RightParen))
        {
            finishNamedPayload(std::move(payload));
            return;
        }
        schedule(std::move(payload));
        return;
    }
    result.kind = implicitBinding ? PatternKind::Binding : PatternKind::Variant;
    result.range = parser_.tokenRangeFrom(begin);
    values_.push_back(std::move(result));
}

void Parser::PatternIterationParser::continueNamedPayload(Work work)
{
    work.kind = WorkKind::FinishNamedPayloadChild;
    schedule(std::move(work));
    schedule(WorkKind::ParseOr, true);
}

void Parser::PatternIterationParser::finishNamedPayloadChild(Work work)
{
    work.pattern.children.push_back(takeValue());
    if (parser_.consumeIf(TokenKind::Comma))
    {
        work.kind = WorkKind::ContinueNamedPayload;
        schedule(std::move(work));
        return;
    }
    finishNamedPayload(std::move(work));
}

void Parser::PatternIterationParser::finishNamedPayload(Work work)
{
    parser_.expect(TokenKind::RightParen, "')' after variant/positional payload pattern");
    work.pattern.kind = parser_.knownTypeNames_.contains(work.pattern.name)
        ? PatternKind::Positional
        : PatternKind::Variant;
    work.pattern.range = parser_.tokenRangeFrom(work.begin);
    values_.push_back(std::move(work.pattern));
}

Parser::Pattern Parser::PatternAlternativeExpander::copyWithoutChildren(
    const Pattern& source)
{
    Pattern copy;
    copy.kind = source.kind;
    copy.bindingMode = source.bindingMode;
    copy.name = source.name;
    copy.range = source.range;
    copy.firstValue = source.firstValue;
    copy.secondValue = source.secondValue;
    copy.firstInteger = source.firstInteger;
    copy.secondInteger = source.secondInteger;
    copy.restIndex = source.restIndex;
    copy.fieldNames = source.fieldNames;
    return copy;
}

Parser::PatternAlternativeExpander::Frame
Parser::PatternAlternativeExpander::makeFrame(const Pattern& source)
{
    Frame frame;
    frame.source = &source;
    if (source.kind != PatternKind::Or)
        frame.alternatives.push_back(copyWithoutChildren(source));
    return frame;
}

void Parser::PatternAlternativeExpander::appendOrAlternatives(
    Frame& frame,
    std::vector<Pattern> alternatives)
{
    frame.alternatives.insert(
        frame.alternatives.end(),
        std::make_move_iterator(alternatives.begin()),
        std::make_move_iterator(alternatives.end()));
}

void Parser::PatternAlternativeExpander::appendChildAlternatives(
    Frame& frame,
    std::vector<Pattern> alternatives)
{
    if (frame.alternatives.empty() || alternatives.empty())
    {
        frame.alternatives.clear();
        return;
    }
    if (frame.alternatives.size() == 1 && alternatives.size() == 1)
    {
        frame.alternatives.front().children.push_back(std::move(alternatives.front()));
        return;
    }
    std::vector<Pattern> product;
    product.reserve(frame.alternatives.size() * alternatives.size());
    for (const Pattern& prefix : frame.alternatives)
    {
        for (const Pattern& alternative : alternatives)
        {
            Pattern copy = prefix;
            copy.children.push_back(alternative);
            product.push_back(std::move(copy));
        }
    }
    frame.alternatives = std::move(product);
}

void Parser::PatternAlternativeExpander::integrateCompletedChild(
    Frame& frame,
    std::vector<Pattern> alternatives)
{
    if (frame.source->kind == PatternKind::Or)
        appendOrAlternatives(frame, std::move(alternatives));
    else
        appendChildAlternatives(frame, std::move(alternatives));
}

std::vector<Parser::Pattern> Parser::PatternAlternativeExpander::run()
{
    std::vector<Frame> frames;
    frames.push_back(makeFrame(root_));
    while (!frames.empty())
    {
        Frame& frame = frames.back();
        if (frame.nextChild < frame.source->children.size())
        {
            const Pattern& child = frame.source->children[frame.nextChild++];
            frames.push_back(makeFrame(child));
            continue;
        }
        std::vector<Pattern> completed = std::move(frame.alternatives);
        frames.pop_back();
        if (frames.empty()) return completed;
        integrateCompletedChild(frames.back(), std::move(completed));
    }
    return {};
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

Parser::Pattern Parser::parsePattern(bool implicitBinding)
{
    return PatternIterationParser(*this, implicitBinding).run();
}

bool Parser::patternHasBindings(const Pattern& pattern) const
{
    std::vector<const Pattern*> pending{&pattern};
    while (!pending.empty())
    {
        const Pattern& currentPattern = *pending.back();
        pending.pop_back();
        if (currentPattern.kind == PatternKind::Binding) return true;
        for (auto child = currentPattern.children.rbegin();
             child != currentPattern.children.rend(); ++child)
            pending.push_back(&*child);
    }
    return false;
}

std::vector<Parser::Pattern> Parser::expandPatternAlternatives(const Pattern& pattern) const
{
    return PatternAlternativeExpander(pattern).run();
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
    std::vector<const Pattern*> pending{&pattern};
    while (!pending.empty())
    {
        const Pattern& currentPattern = *pending.back();
        pending.pop_back();
        if (currentPattern.kind == PatternKind::Binding ||
            currentPattern.kind == PatternKind::Tuple ||
            currentPattern.kind == PatternKind::List)
            return false;
        for (auto child = currentPattern.children.rbegin();
             child != currentPattern.children.rend(); ++child)
            pending.push_back(&*child);
    }
    return true;
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
    std::unordered_set<std::string> names;
    names.reserve(bindings.size());
    for (const auto& binding : bindings) names.insert(binding.first);
    std::vector<const Pattern*> pending{&pattern};
    while (!pending.empty())
    {
        const Pattern& currentPattern = *pending.back();
        pending.pop_back();
        if (currentPattern.kind == PatternKind::Binding)
        {
            if (names.insert(currentPattern.name).second)
                bindings.emplace_back(currentPattern.name, currentPattern.bindingMode);
            continue;
        }
        for (auto child = currentPattern.children.rbegin();
             child != currentPattern.children.rend(); ++child)
            pending.push_back(&*child);
    }
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
