#include "PatternIterationParser.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;
Parser::PatternIterationParser::PatternIterationParser(
    Parser& parser,
    bool implicitBinding)
    : parser_(parser), implicitBinding_(implicitBinding)
{
}

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
    work.pattern.kind = parser_.isKnownTypeName(work.pattern.name)
        ? PatternKind::Positional
        : PatternKind::Variant;
    work.pattern.range = parser_.tokenRangeFrom(work.begin);
    values_.push_back(std::move(work.pattern));
}
}
