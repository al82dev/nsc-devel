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
#include <unordered_map>
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

class Parser::PatternConditionBuilder final
{
public:
    PatternConditionBuilder(
        Parser& parser,
        const Pattern& root,
        PatternExpressionFactory source);

    ast::AstNode* run();

private:
    enum class SourceStepKind
    {
        Root,
        Member,
        VariantPayload,
        Index
    };

    struct SourcePlan final
    {
        std::size_t parent = 0;
        SourceStepKind kind = SourceStepKind::Root;
        std::string text;
        ast::AstNode* index = nullptr;
    };

    enum class WorkKind
    {
        Build,
        FinishNot,
        FinishVariant,
        FinishLogical,
        ContinueStruct,
        FinishStruct,
        ContinueList,
        FinishList
    };

    struct Work final
    {
        WorkKind kind = WorkKind::Build;
        const Pattern* pattern = nullptr;
        std::size_t sourcePlan = 0;
        std::size_t nextChild = 0;
        ast::AstNode* result = nullptr;
        TokenKind operation = TokenKind::AmpAmp;
    };

    void schedule(Work work);
    void scheduleBuild(const Pattern& pattern, std::size_t sourcePlan);
    void dispatch(Work work);
    ast::AstNode* takeValue();
    std::size_t addSourcePlan(
        std::size_t parent,
        SourceStepKind kind,
        std::string text = {},
        ast::AstNode* index = nullptr);
    ast::AstNode* evaluateSource(std::size_t sourcePlan);
    ast::AstNode* buildLeaf(const Pattern& pattern, std::size_t sourcePlan);
    void build(const Pattern& pattern, std::size_t sourcePlan);
    void startNot(const Pattern& pattern, std::size_t sourcePlan);
    void finishNot(Work work);
    void startVariant(const Pattern& pattern, std::size_t sourcePlan);
    void finishVariant(Work work);
    void startLogical(
        const Pattern& pattern,
        std::size_t sourcePlan,
        TokenKind operation);
    void finishLogical(Work work);
    void startStruct(const Pattern& pattern, std::size_t sourcePlan);
    void continueStruct(Work work);
    void finishStruct(Work work);
    void startList(const Pattern& pattern, std::size_t sourcePlan);
    void continueList(Work work);
    void finishList(Work work);

    Parser& parser_;
    const Pattern& root_;
    PatternExpressionFactory rootSource_;
    std::vector<SourcePlan> sources_;
    std::vector<Work> work_;
    std::vector<ast::AstNode*> values_;
};

class Parser::PatternLoweringMachine final
{
public:
    PatternLoweringMachine(
        Parser& parser,
        const Pattern& root,
        PatternExpressionFactory source,
        ast::AstNode* continuation,
        std::string_view temporaryPrefix,
        bool emitBindings);

    ast::AstNode* run();

private:
    enum class SourceKind
    {
        Root,
        Identifier,
        Member
    };

    struct SourcePlan final
    {
        std::size_t parent = 0;
        SourceKind kind = SourceKind::Root;
        SourceRange range;
        std::string text;
    };

    enum class WorkKind
    {
        Build,
        ContinueOr,
        FinishOrChild,
        ContinueSequential,
        FinishSequential,
        FinishVariant
    };

    struct Work final
    {
        WorkKind kind = WorkKind::Build;
        const Pattern* pattern = nullptr;
        std::size_t sourcePlan = 0;
        std::size_t nextChild = 0;
        ast::AstNode* continuation = nullptr;
        ast::AstNode* result = nullptr;
        ast::AstNode* block = nullptr;
        ast::AstNode* decision = nullptr;
        std::string selectedName;
        std::vector<std::string> names;
    };

    struct ClassificationFrame final
    {
        const Pattern* pattern = nullptr;
        std::size_t nextChild = 0;
        bool conditionOnly = true;
    };

    void schedule(Work work);
    void scheduleBuild(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void dispatch(Work work);
    ast::AstNode* takeValue();
    std::size_t addSourcePlan(
        std::size_t parent,
        SourceKind kind,
        SourceRange range,
        std::string text);
    ast::AstNode* evaluateSource(std::size_t sourcePlan);
    PatternExpressionFactory makeSourceFactory(std::size_t sourcePlan);
    bool isConditionOnly(const Pattern& pattern);
    void finishClassification(
        std::vector<ClassificationFrame>& frames,
        bool value);
    void build(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* continuation);
    void buildBinding(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void startOr(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* continuation);
    void continueOr(Work work);
    void finishOrChild(Work work);
    void startSequential(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void initializeDeconstruction(Work& work);
    void continueSequential(Work work);
    void finishSequential(Work work);
    void buildList(const Pattern& pattern, std::size_t sourcePlan, ast::AstNode* continuation);
    void startVariant(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);
    void finishVariant(Work work);
    void buildConditional(
        const Pattern& pattern,
        std::size_t sourcePlan,
        ast::AstNode* continuation);

    Parser& parser_;
    const Pattern& root_;
    PatternExpressionFactory rootSource_;
    ast::AstNode* continuation_ = nullptr;
    std::string temporaryPrefix_;
    bool emitBindings_ = true;
    std::vector<SourcePlan> sources_;
    std::vector<Work> work_;
    std::vector<ast::AstNode*> values_;
    std::unordered_map<const Pattern*, bool> conditionOnly_;
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

Parser::PatternConditionBuilder::PatternConditionBuilder(
    Parser& parser,
    const Pattern& root,
    PatternExpressionFactory source)
    : parser_(parser), root_(root), rootSource_(std::move(source))
{
    sources_.push_back(SourcePlan{});
}

ast::AstNode* Parser::PatternConditionBuilder::run()
{
    scheduleBuild(root_, 0);
    while (!work_.empty())
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        dispatch(std::move(work));
    }
    return takeValue();
}

void Parser::PatternConditionBuilder::schedule(Work work)
{
    work_.push_back(std::move(work));
}

void Parser::PatternConditionBuilder::scheduleBuild(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    Work work;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    schedule(std::move(work));
}

ast::AstNode* Parser::PatternConditionBuilder::takeValue()
{
    ast::AstNode* const value = values_.back();
    values_.pop_back();
    return value;
}

void Parser::PatternConditionBuilder::dispatch(Work work)
{
    switch (work.kind)
    {
        case WorkKind::Build: return build(*work.pattern, work.sourcePlan);
        case WorkKind::FinishNot: return finishNot(std::move(work));
        case WorkKind::FinishVariant: return finishVariant(std::move(work));
        case WorkKind::FinishLogical: return finishLogical(std::move(work));
        case WorkKind::ContinueStruct: return continueStruct(std::move(work));
        case WorkKind::FinishStruct: return finishStruct(std::move(work));
        case WorkKind::ContinueList: return continueList(std::move(work));
        case WorkKind::FinishList: return finishList(std::move(work));
    }
}

std::size_t Parser::PatternConditionBuilder::addSourcePlan(
    std::size_t parent,
    SourceStepKind kind,
    std::string text,
    ast::AstNode* index)
{
    SourcePlan plan;
    plan.parent = parent;
    plan.kind = kind;
    plan.text = std::move(text);
    plan.index = index;
    sources_.push_back(std::move(plan));
    return sources_.size() - 1U;
}

ast::AstNode* Parser::PatternConditionBuilder::evaluateSource(std::size_t sourcePlan)
{
    std::vector<std::size_t> path;
    while (sourcePlan != 0U)
    {
        path.push_back(sourcePlan);
        sourcePlan = sources_[sourcePlan].parent;
    }
    ast::AstNode* result = rootSource_();
    for (auto stepIndex = path.rbegin(); stepIndex != path.rend(); ++stepIndex)
    {
        const SourcePlan& step = sources_[*stepIndex];
        if (step.kind == SourceStepKind::Member)
            result = parser_.makeMemberExpression(result, step.text);
        else if (step.kind == SourceStepKind::VariantPayload)
        {
            result = parser_.makeZeroArgCallExpression(
                parser_.makeMemberExpression(result, "__nsc_variant_extract_" + step.text));
            result->setAuxiliaryText("variant-pattern-extract");
        }
        else if (step.kind == SourceStepKind::Index)
            result = parser_.makeIndexExpression(
                result, parser_.cloneDeclarationNode(step.index));
    }
    return result;
}

ast::AstNode* Parser::PatternConditionBuilder::buildLeaf(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    if (pattern.kind == PatternKind::Wildcard || pattern.kind == PatternKind::Binding ||
        pattern.kind == PatternKind::Positional)
        return parser_.makeBooleanLiteral(pattern.range, true);
    if (pattern.kind == PatternKind::Tuple)
        return parser_.makeBooleanLiteral(pattern.range, false);
    if (pattern.kind == PatternKind::Literal)
        return parser_.makeBinaryExpression(
            TokenKind::EqualEqual, evaluateSource(sourcePlan), pattern.firstValue);
    if (pattern.kind == PatternKind::Range)
    {
        ast::AstNode* lower = parser_.makeBinaryExpression(
            TokenKind::GreaterEqual, evaluateSource(sourcePlan), pattern.firstValue);
        ast::AstNode* upper = parser_.makeBinaryExpression(
            pattern.name == "..=" ? TokenKind::LessEqual : TokenKind::Less,
            evaluateSource(sourcePlan),
            pattern.secondValue);
        return parser_.makeBinaryExpression(TokenKind::AmpAmp, lower, upper);
    }
    TokenKind operation = TokenKind::Less;
    if (pattern.name == "<=") operation = TokenKind::LessEqual;
    else if (pattern.name == ">") operation = TokenKind::Greater;
    else if (pattern.name == ">=") operation = TokenKind::GreaterEqual;
    if (pattern.kind == PatternKind::Relational)
        return parser_.makeBinaryExpression(
            operation, evaluateSource(sourcePlan), pattern.firstValue);
    return parser_.makeBooleanLiteral(pattern.range, false);
}

void Parser::PatternConditionBuilder::build(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    if (pattern.kind == PatternKind::Struct)
        return startStruct(pattern, sourcePlan);
    if (pattern.kind == PatternKind::Variant)
        return startVariant(pattern, sourcePlan);
    if (pattern.kind == PatternKind::Or)
        return startLogical(pattern, sourcePlan, TokenKind::PipePipe);
    if (pattern.kind == PatternKind::List)
        return startList(pattern, sourcePlan);
    if (pattern.kind == PatternKind::Not)
        return startNot(pattern, sourcePlan);
    if (pattern.kind == PatternKind::And)
        return startLogical(pattern, sourcePlan, TokenKind::AmpAmp);
    values_.push_back(buildLeaf(pattern, sourcePlan));
}

void Parser::PatternConditionBuilder::startNot(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    Work finish;
    finish.kind = WorkKind::FinishNot;
    finish.pattern = &pattern;
    schedule(std::move(finish));
    scheduleBuild(pattern.children.front(), sourcePlan);
}

void Parser::PatternConditionBuilder::finishNot(Work work)
{
    ast::AstNode* unary = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, work.pattern->range);
    unary->setTokenKind(TokenKind::Bang);
    unary->setFirst(takeValue());
    values_.push_back(unary);
}

void Parser::PatternConditionBuilder::startVariant(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    ast::AstNode* test = parser_.makeZeroArgCallExpression(
        parser_.makeMemberExpression(evaluateSource(sourcePlan), "Is" + pattern.name));
    if (pattern.children.empty())
    {
        values_.push_back(test);
        return;
    }
    Work finish;
    finish.kind = WorkKind::FinishVariant;
    finish.result = test;
    schedule(std::move(finish));
    const std::size_t payload = addSourcePlan(
        sourcePlan, SourceStepKind::VariantPayload, pattern.name);
    scheduleBuild(pattern.children.front(), payload);
}

void Parser::PatternConditionBuilder::finishVariant(Work work)
{
    values_.push_back(parser_.makeBinaryExpression(
        TokenKind::AmpAmp, work.result, takeValue()));
}

void Parser::PatternConditionBuilder::startLogical(
    const Pattern& pattern,
    std::size_t sourcePlan,
    TokenKind operation)
{
    Work finish;
    finish.kind = WorkKind::FinishLogical;
    finish.pattern = &pattern;
    finish.sourcePlan = sourcePlan;
    finish.nextChild = 1U;
    finish.operation = operation;
    schedule(std::move(finish));
    scheduleBuild(pattern.children.front(), sourcePlan);
}

void Parser::PatternConditionBuilder::finishLogical(Work work)
{
    ast::AstNode* const child = takeValue();
    work.result = work.result == nullptr
        ? child
        : parser_.makeBinaryExpression(work.operation, work.result, child);
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.result);
        return;
    }
    const Pattern& next = work.pattern->children[work.nextChild++];
    const std::size_t sourcePlan = work.sourcePlan;
    schedule(std::move(work));
    scheduleBuild(next, sourcePlan);
}

void Parser::PatternConditionBuilder::startStruct(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    Work work;
    work.kind = WorkKind::ContinueStruct;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.result = parser_.makeBooleanLiteral(pattern.range, true);
    schedule(std::move(work));
}

void Parser::PatternConditionBuilder::continueStruct(Work work)
{
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.result);
        return;
    }
    const std::size_t childIndex = work.nextChild++;
    const std::size_t member = addSourcePlan(
        work.sourcePlan,
        SourceStepKind::Member,
        work.pattern->fieldNames[childIndex]);
    const Pattern* const child = &work.pattern->children[childIndex];
    work.kind = WorkKind::FinishStruct;
    schedule(std::move(work));
    scheduleBuild(*child, member);
}

void Parser::PatternConditionBuilder::finishStruct(Work work)
{
    work.result = parser_.makeBinaryExpression(
        TokenKind::AmpAmp, work.result, takeValue());
    work.kind = WorkKind::ContinueStruct;
    schedule(std::move(work));
}

void Parser::PatternConditionBuilder::startList(
    const Pattern& pattern,
    std::size_t sourcePlan)
{
    const std::size_t fixedCount = pattern.children.size() -
        (pattern.restIndex.has_value() ? 1U : 0U);
    ast::AstNode* length = parser_.makeMemberExpression(
        evaluateSource(sourcePlan), "Length");
    length->setAuxiliaryText("list-pattern-length");
    Work work;
    work.kind = WorkKind::ContinueList;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.result = parser_.makeBinaryExpression(
        pattern.restIndex.has_value() ? TokenKind::GreaterEqual : TokenKind::EqualEqual,
        length,
        parser_.makeIntegerLiteralExpression(pattern.range, fixedCount));
    schedule(std::move(work));
}

void Parser::PatternConditionBuilder::continueList(Work work)
{
    while (work.nextChild < work.pattern->children.size() &&
           work.pattern->restIndex == work.nextChild)
        ++work.nextChild;
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.result);
        return;
    }
    const std::size_t childIndex = work.nextChild++;
    ast::AstNode* elementIndex = nullptr;
    const std::size_t restOffset =
        work.pattern->restIndex.value_or(work.pattern->children.size());
    if (!work.pattern->restIndex.has_value() || childIndex < restOffset)
        elementIndex = parser_.makeIntegerLiteralExpression(work.pattern->range, childIndex);
    else
    {
        ast::AstNode* length = parser_.makeMemberExpression(
            evaluateSource(work.sourcePlan), "Length");
        length->setAuxiliaryText("list-pattern-length");
        elementIndex = parser_.makeBinaryExpression(
            TokenKind::Minus,
            length,
            parser_.makeIntegerLiteralExpression(
                work.pattern->range, work.pattern->children.size() - childIndex));
    }
    const std::size_t element = addSourcePlan(
        work.sourcePlan, SourceStepKind::Index, {}, elementIndex);
    const Pattern* const child = &work.pattern->children[childIndex];
    work.kind = WorkKind::FinishList;
    schedule(std::move(work));
    scheduleBuild(*child, element);
}

void Parser::PatternConditionBuilder::finishList(Work work)
{
    work.result = parser_.makeBinaryExpression(
        TokenKind::AmpAmp, work.result, takeValue());
    work.kind = WorkKind::ContinueList;
    schedule(std::move(work));
}

Parser::PatternLoweringMachine::PatternLoweringMachine(
    Parser& parser,
    const Pattern& root,
    PatternExpressionFactory source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
    : parser_(parser),
      root_(root),
      rootSource_(std::move(source)),
      continuation_(continuation),
      temporaryPrefix_(temporaryPrefix),
      emitBindings_(emitBindings)
{
    sources_.push_back(SourcePlan{});
}

ast::AstNode* Parser::PatternLoweringMachine::run()
{
    scheduleBuild(root_, 0, continuation_);
    while (!work_.empty())
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        dispatch(std::move(work));
    }
    return takeValue();
}

void Parser::PatternLoweringMachine::schedule(Work work)
{
    work_.push_back(std::move(work));
}

void Parser::PatternLoweringMachine::scheduleBuild(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    Work work;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.continuation = continuation;
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::dispatch(Work work)
{
    switch (work.kind)
    {
        case WorkKind::Build:
            return build(*work.pattern, work.sourcePlan, work.continuation);
        case WorkKind::ContinueOr: return continueOr(std::move(work));
        case WorkKind::FinishOrChild: return finishOrChild(std::move(work));
        case WorkKind::ContinueSequential: return continueSequential(std::move(work));
        case WorkKind::FinishSequential: return finishSequential(std::move(work));
        case WorkKind::FinishVariant: return finishVariant(std::move(work));
    }
}

ast::AstNode* Parser::PatternLoweringMachine::takeValue()
{
    ast::AstNode* const value = values_.back();
    values_.pop_back();
    return value;
}

std::size_t Parser::PatternLoweringMachine::addSourcePlan(
    std::size_t parent,
    SourceKind kind,
    SourceRange range,
    std::string text)
{
    SourcePlan plan;
    plan.parent = parent;
    plan.kind = kind;
    plan.range = range;
    plan.text = std::move(text);
    sources_.push_back(std::move(plan));
    return sources_.size() - 1U;
}

ast::AstNode* Parser::PatternLoweringMachine::evaluateSource(std::size_t sourcePlan)
{
    std::vector<std::size_t> members;
    while (sources_[sourcePlan].kind == SourceKind::Member)
    {
        members.push_back(sourcePlan);
        sourcePlan = sources_[sourcePlan].parent;
    }
    const SourcePlan& base = sources_[sourcePlan];
    ast::AstNode* result = base.kind == SourceKind::Root
        ? rootSource_()
        : parser_.makeIdentifierExpression(base.range, base.text);
    for (auto member = members.rbegin(); member != members.rend(); ++member)
        result = parser_.makeMemberExpression(result, sources_[*member].text);
    return result;
}

Parser::PatternExpressionFactory Parser::PatternLoweringMachine::makeSourceFactory(
    std::size_t sourcePlan)
{
    return [this, sourcePlan]
    {
        return evaluateSource(sourcePlan);
    };
}

void Parser::PatternLoweringMachine::finishClassification(
    std::vector<ClassificationFrame>& frames,
    bool value)
{
    conditionOnly_[frames.back().pattern] = value;
    frames.pop_back();
    if (!frames.empty() && !value) frames.back().conditionOnly = false;
}

bool Parser::PatternLoweringMachine::isConditionOnly(const Pattern& pattern)
{
    if (const auto found = conditionOnly_.find(&pattern); found != conditionOnly_.end())
        return found->second;
    std::vector<ClassificationFrame> frames{{&pattern}};
    while (!frames.empty())
    {
        ClassificationFrame& frame = frames.back();
        if (!frame.conditionOnly || frame.pattern->kind == PatternKind::Binding ||
            frame.pattern->kind == PatternKind::Tuple || frame.pattern->kind == PatternKind::List)
        {
            finishClassification(frames, false);
            continue;
        }
        if (frame.nextChild == frame.pattern->children.size())
        {
            finishClassification(frames, true);
            continue;
        }
        const Pattern* const child = &frame.pattern->children[frame.nextChild++];
        if (const auto found = conditionOnly_.find(child); found != conditionOnly_.end())
        {
            if (!found->second) frame.conditionOnly = false;
            continue;
        }
        frames.push_back(ClassificationFrame{child});
    }
    return conditionOnly_.at(&pattern);
}

void Parser::PatternLoweringMachine::build(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    if (pattern.kind == PatternKind::Wildcard)
        values_.push_back(continuation);
    else if (pattern.kind == PatternKind::Binding)
        buildBinding(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::Or && !isConditionOnly(pattern))
        startOr(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::Tuple || pattern.kind == PatternKind::Struct ||
            pattern.kind == PatternKind::Positional)
        startSequential(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::List)
        buildList(pattern, sourcePlan, continuation);
    else if (pattern.kind == PatternKind::Variant)
        startVariant(pattern, sourcePlan, continuation);
    else
        buildConditional(pattern, sourcePlan, continuation);
}

void Parser::PatternLoweringMachine::buildBinding(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    if (!emitBindings_)
    {
        values_.push_back(continuation);
        return;
    }
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    ast::AstNode* declaration = parser_.makeVariableDeclaration(
        type,
        pattern.name,
        evaluateSource(sourcePlan),
        false,
        pattern.bindingMode == PatternBindingMode::Value ||
            pattern.bindingMode == PatternBindingMode::Copy ||
            pattern.bindingMode == PatternBindingMode::Reference);
    parser_.applyPatternBindingMode(declaration, pattern.bindingMode);
    block->addChild(declaration);
    block->addChild(continuation);
    values_.push_back(block);
}

void Parser::PatternLoweringMachine::startOr(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    Work work;
    work.kind = WorkKind::ContinueOr;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.continuation = continuation;
    work.block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    work.selectedName = temporaryPrefix_ + "_or_selected_" +
        std::to_string(parser_.nextMatchTemporary_++);
    ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwBool, "bool");
    work.block->addChild(parser_.makeVariableDeclaration(
        boolType, work.selectedName, parser_.makeBooleanLiteral(pattern.range, false),
        false, false));
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::continueOr(Work work)
{
    if (work.nextChild == work.pattern->children.size())
    {
        values_.push_back(work.block);
        return;
    }
    const std::size_t childIndex = work.nextChild++;
    const Pattern* const child = &work.pattern->children[childIndex];
    ast::AstNode* selectedBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, child->range);
    selectedBody->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
        parser_.makeIdentifierExpression(child->range, work.selectedName),
        parser_.makeBooleanLiteral(child->range, true))));
    selectedBody->addChild(childIndex + 1U == work.pattern->children.size()
        ? work.continuation
        : parser_.cloneDeclarationNode(work.continuation));
    work.kind = WorkKind::FinishOrChild;
    work.result = selectedBody;
    const std::size_t sourcePlan = work.sourcePlan;
    schedule(std::move(work));
    scheduleBuild(*child, sourcePlan, selectedBody);
}

void Parser::PatternLoweringMachine::finishOrChild(Work work)
{
    ast::AstNode* notSelected = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, work.result->range());
    notSelected->setTokenKind(TokenKind::Bang);
    notSelected->setFirst(parser_.makeIdentifierExpression(
        work.result->range(), work.selectedName));
    ast::AstNode* guarded = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, work.result->range());
    guarded->setFirst(notSelected);
    ast::AstNode* guardedBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, work.result->range());
    guardedBody->addChild(takeValue());
    guarded->setSecond(guardedBody);
    work.block->addChild(guarded);
    work.kind = WorkKind::ContinueOr;
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::initializeDeconstruction(Work& work)
{
    work.block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, work.pattern->range);
    ast::AstNode* deconstruction = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, work.pattern->range);
    deconstruction->setFirst(evaluateSource(work.sourcePlan));
    if (work.pattern->kind == PatternKind::Positional)
        deconstruction->setTypeSyntax(parser_.arena_.make<ast::TypeSyntax>(
            work.pattern->range, TokenKind::Identifier, work.pattern->name));
    work.names.reserve(work.pattern->children.size());
    const std::string marker = work.pattern->kind == PatternKind::Tuple ? "_tuple_" : "_pos_";
    for (std::size_t index = 0; index < work.pattern->children.size(); ++index)
    {
        const std::string name = temporaryPrefix_ + marker +
            std::to_string(parser_.nextMatchTemporary_++) + "_" + std::to_string(index);
        work.names.push_back(name);
        deconstruction->addChild(
            parser_.makeIdentifierExpression(work.pattern->range, name));
    }
    work.block->addChild(deconstruction);
}

void Parser::PatternLoweringMachine::startSequential(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    Work work;
    work.kind = WorkKind::ContinueSequential;
    work.pattern = &pattern;
    work.sourcePlan = sourcePlan;
    work.nextChild = pattern.children.size();
    work.result = continuation;
    if (pattern.kind != PatternKind::Struct) initializeDeconstruction(work);
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::continueSequential(Work work)
{
    if (work.nextChild == 0U)
    {
        if (work.block != nullptr)
        {
            work.block->addChild(work.result);
            values_.push_back(work.block);
        }
        else
            values_.push_back(work.result);
        return;
    }
    const std::size_t childIndex = --work.nextChild;
    const std::size_t childSource = work.pattern->kind == PatternKind::Struct
        ? addSourcePlan(
              work.sourcePlan,
              SourceKind::Member,
              work.pattern->range,
              work.pattern->fieldNames[childIndex])
        : addSourcePlan(0, SourceKind::Identifier, work.pattern->range, work.names[childIndex]);
    const Pattern* const child = &work.pattern->children[childIndex];
    ast::AstNode* const continuation = work.result;
    work.kind = WorkKind::FinishSequential;
    schedule(std::move(work));
    scheduleBuild(*child, childSource, continuation);
}

void Parser::PatternLoweringMachine::finishSequential(Work work)
{
    work.result = takeValue();
    work.kind = WorkKind::ContinueSequential;
    schedule(std::move(work));
}

void Parser::PatternLoweringMachine::buildList(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    ast::AstNode* thenBlock = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    PatternExpressionFactory source = makeSourceFactory(sourcePlan);
    if (emitBindings_)
        parser_.emitPatternBindings(pattern, source, thenBlock, temporaryPrefix_);
    thenBlock->addChild(continuation);
    ast::AstNode* decision = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(parser_.makePatternCondition(pattern, source));
    decision->setSecond(thenBlock);
    values_.push_back(decision);
}

void Parser::PatternLoweringMachine::startVariant(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    ast::AstNode* test = parser_.makeZeroArgCallExpression(
        parser_.makeMemberExpression(evaluateSource(sourcePlan), "Is" + pattern.name));
    ast::AstNode* thenBlock = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, pattern.range);
    ast::AstNode* decision = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(test);
    decision->setSecond(thenBlock);
    if (pattern.children.empty() || pattern.children.front().kind == PatternKind::Wildcard)
    {
        thenBlock->addChild(continuation);
        values_.push_back(decision);
        return;
    }
    const std::string payloadName = temporaryPrefix_ + "_payload_" +
        std::to_string(parser_.nextMatchTemporary_++);
    ast::AstNode* extract = parser_.makeZeroArgCallExpression(parser_.makeMemberExpression(
        evaluateSource(sourcePlan), "__nsc_variant_extract_" + pattern.name));
    extract->setAuxiliaryText("variant-pattern-extract");
    ast::TypeSyntax* type = parser_.arena_.make<ast::TypeSyntax>(
        pattern.range, TokenKind::KwVar, "var");
    thenBlock->addChild(parser_.makeVariableDeclaration(type, payloadName, extract, false, true));
    Work finish;
    finish.kind = WorkKind::FinishVariant;
    finish.block = thenBlock;
    finish.decision = decision;
    schedule(std::move(finish));
    const std::size_t payload = addSourcePlan(
        0, SourceKind::Identifier, pattern.range, payloadName);
    scheduleBuild(pattern.children.front(), payload, continuation);
}

void Parser::PatternLoweringMachine::finishVariant(Work work)
{
    work.block->addChild(takeValue());
    values_.push_back(work.decision);
}

void Parser::PatternLoweringMachine::buildConditional(
    const Pattern& pattern,
    std::size_t sourcePlan,
    ast::AstNode* continuation)
{
    PatternExpressionFactory source = makeSourceFactory(sourcePlan);
    ast::AstNode* decision = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, pattern.range);
    decision->setFirst(parser_.makePatternCondition(pattern, source));
    decision->setSecond(continuation);
    values_.push_back(decision);
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

ast::AstNode* Parser::makePatternCondition(
    const Pattern& pattern,
    const PatternExpressionFactory& source)
{
    return PatternConditionBuilder(*this, pattern, source).run();
}

ast::AstNode* Parser::lowerPattern(
    const Pattern& pattern,
    const PatternExpressionFactory& source,
    ast::AstNode* continuation,
    std::string_view temporaryPrefix,
    bool emitBindings)
{
    return PatternLoweringMachine(
        *this,
        pattern,
        source,
        continuation,
        temporaryPrefix,
        emitBindings).run();
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
