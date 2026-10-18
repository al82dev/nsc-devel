#include "nativesharp/Parser/Parser.h"
#include "KnownGenericTypeScope.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/IterationSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
namespace
{
using lexer::TokenKind;

constexpr std::array<TokenKind, 1> kLogicalOr = {TokenKind::PipePipe};
constexpr std::array<TokenKind, 1> kLogicalAnd = {TokenKind::AmpAmp};
constexpr std::array<TokenKind, 1> kBitwiseOr = {TokenKind::Pipe};
constexpr std::array<TokenKind, 1> kBitwiseXor = {TokenKind::Caret};
constexpr std::array<TokenKind, 1> kBitwiseAnd = {TokenKind::Amp};
constexpr std::array<TokenKind, 2> kEquality = {TokenKind::EqualEqual, TokenKind::BangEqual};
constexpr std::array<TokenKind, 4> kRelational = {
    TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater, TokenKind::GreaterEqual};
constexpr std::array<TokenKind, 2> kShift = {TokenKind::LessLess, TokenKind::GreaterGreater};
constexpr std::array<TokenKind, 2> kAdditive = {TokenKind::Plus, TokenKind::Minus};
constexpr std::array<TokenKind, 3> kMultiplicative = {TokenKind::Star, TokenKind::Slash, TokenKind::Percent};

bool containsExpressionToken(
    std::span<const TokenKind> values,
    TokenKind kind) noexcept
{
    return std::find(values.begin(), values.end(), kind) != values.end();
}

std::optional<std::int64_t> signedIntegerLiteral(const ast::AstNode* expression) noexcept
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression)
    {
        if (expression->integerValue() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::nullopt;
        return static_cast<std::int64_t>(expression->integerValue());
    }
    if (expression->kind() != ast::AstNodeKind::UnaryExpression ||
        expression->tokenKind() != TokenKind::Minus || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IntegerLiteralExpression)
        return std::nullopt;
    const std::uint64_t magnitude = expression->first()->integerValue();
    if (magnitude > (std::uint64_t{1} << 63U)) return std::nullopt;
    if (magnitude == (std::uint64_t{1} << 63U))
        return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
}

bool isStaticInterpolationHole(const language::InterpolationPart& part) noexcept
{
    return language::builtinStaticInterpolationHoleSupported(part);
}

bool endsConditionalQuestionScan(TokenKind kind) noexcept
{
    return kind == TokenKind::Comma || kind == TokenKind::Semicolon ||
        kind == TokenKind::EndOfFile;
}

// Identifies delimiters that terminate questions opened at their own nesting level. Runs in O(1).
bool closesConditionalQuestionScope(TokenKind kind) noexcept
{
    return kind == TokenKind::RightParen || kind == TokenKind::RightBracket ||
        kind == TokenKind::RightBrace;
}

// Records one opener in lexical order and in its delimiter-specific LIFO stack. The ordinal table
// lets the later event pass identify an opener without another search. Runs in O(1).
void recordConditionalOpener(
    std::size_t cursor,
    std::vector<std::size_t>& positions,
    std::vector<std::size_t>& ordinals,
    std::vector<std::size_t>& stack)
{
    ordinals[cursor] = positions.size();
    positions.push_back(cursor);
    stack.push_back(cursor);
}

// Pairs one closer with the latest unmatched opener of the same delimiter type. Unmatched closers
// remain unpaired events, matching the previous scanner's recovery behavior. Runs in O(1).
void closeConditionalOpener(
    std::size_t cursor,
    std::vector<std::size_t>& matches,
    std::vector<std::size_t>& stack)
{
    if (stack.empty()) return;
    matches[stack.back()] = cursor;
    stack.pop_back();
}

// Pairs delimiters independently by type, which is exactly how the former per-question counters
// treated crossing and malformed delimiter sequences. Runs in O(T) time and O(T) space.
void pairConditionalDelimiters(
    std::span<const lexer::Token> tokens,
    std::vector<std::size_t>& matches,
    std::vector<std::size_t>& positions,
    std::vector<std::size_t>& ordinals)
{
    const std::size_t end = tokens.size();
    matches.assign(end, end);
    ordinals.assign(end, end);
    std::vector<std::size_t> parentheses;
    std::vector<std::size_t> brackets;
    std::vector<std::size_t> braces;
    for (std::size_t cursor = 0; cursor < end; ++cursor)
    {
        const TokenKind kind = tokens[cursor].kind();
        if (kind == TokenKind::LeftParen)
            recordConditionalOpener(cursor, positions, ordinals, parentheses);
        else if (kind == TokenKind::LeftBracket)
            recordConditionalOpener(cursor, positions, ordinals, brackets);
        else if (kind == TokenKind::LeftBrace)
            recordConditionalOpener(cursor, positions, ordinals, braces);
        else if (kind == TokenKind::RightParen)
            closeConditionalOpener(cursor, matches, parentheses);
        else if (kind == TokenKind::RightBracket)
            closeConditionalOpener(cursor, matches, brackets);
        else if (kind == TokenKind::RightBrace)
            closeConditionalOpener(cursor, matches, braces);
    }
}

// Returns the ordinal of the last opener at or before a token boundary. The caller guarantees a
// non-empty prefix because every queried boundary is at least its originating opener position.
std::size_t lastConditionalOpenerAtOrBefore(
    const std::vector<std::size_t>& positions,
    std::size_t boundary)
{
    return static_cast<std::size_t>(
        std::upper_bound(positions.begin(), positions.end(), boundary) - positions.begin() - 1);
}

// Computes the complete interval closed by each opener, including independently paired intervals
// that cross it. Closure-ordinal jumps make total merging linear; searches add O(T log T).
void buildConditionalScopeClosures(
    const std::vector<std::size_t>& matches,
    const std::vector<std::size_t>& positions,
    std::vector<std::size_t>& closureEnds)
{
    std::vector<std::size_t> closureOrdinals(positions.size());
    closureEnds.resize(positions.size());
    for (std::size_t ordinal = positions.size(); ordinal-- > 0;)
    {
        std::size_t end = matches[positions[ordinal]];
        std::size_t last = lastConditionalOpenerAtOrBefore(positions, end);
        for (std::size_t next = ordinal + 1U; next <= last;)
        {
            end = std::max(end, closureEnds[next]);
            last = lastConditionalOpenerAtOrBefore(positions, end);
            next = closureOrdinals[next] + 1U;
        }
        closureEnds[ordinal] = end;
        closureOrdinals[ordinal] = last;
    }
}

// Identifies events that are observable by a question scanner at depth zero. Opening delimiters
// are handled through their precomputed closures instead. Runs in O(1).
bool isConditionalQuestionEvent(TokenKind kind) noexcept
{
    return kind == TokenKind::Question || kind == TokenKind::Colon ||
        closesConditionalQuestionScope(kind) || endsConditionalQuestionScan(kind);
}

// Builds the next depth-zero event for every token boundary. Crossing scopes are skipped through
// their transitive closure, so every lookup is O(1) after O(T) construction.
void buildConditionalNextEvents(
    std::span<const lexer::Token> tokens,
    const std::vector<std::size_t>& ordinals,
    const std::vector<std::size_t>& closureEnds,
    std::vector<std::size_t>& nextEvents)
{
    const std::size_t end = tokens.size();
    nextEvents.assign(end + 1U, end);
    for (std::size_t cursor = end; cursor-- > 0;)
    {
        if (ordinals[cursor] != end)
        {
            const std::size_t closure = closureEnds[ordinals[cursor]];
            nextEvents[cursor] = closure < end ? nextEvents[closure + 1U] : end;
            continue;
        }
        nextEvents[cursor] = isConditionalQuestionEvent(tokens[cursor].kind())
            ? cursor
            : nextEvents[cursor + 1U];
    }
}

// Matches questions and colons from right to left. Collapsed events skip complete nested ternaries,
// preserving LIFO ownership with O(1) work per question and no native recursion.
void classifyConditionalQuestions(
    std::span<const lexer::Token> tokens,
    const std::vector<std::size_t>& nextEvents,
    std::vector<std::uint8_t>& roles)
{
    const std::size_t end = tokens.size();
    roles.assign(end, std::uint8_t{0});
    std::vector<std::size_t> collapsedEvents(end + 1U, end);
    for (std::size_t cursor = end; cursor-- > 0;)
    {
        const TokenKind kind = tokens[cursor].kind();
        if (kind != TokenKind::Question)
        {
            if (isConditionalQuestionEvent(kind)) collapsedEvents[cursor] = cursor;
            continue;
        }
        std::size_t event = nextEvents[cursor + 1U];
        if (event < end) event = collapsedEvents[event];
        if (event >= end || tokens[event].kind() != TokenKind::Colon)
        {
            collapsedEvents[cursor] = cursor;
            continue;
        }
        roles[cursor] = std::uint8_t{1};
        const std::size_t after = nextEvents[event + 1U];
        collapsedEvents[cursor] = after < end ? collapsedEvents[after] : end;
    }
}

}

class Parser::MatchExpressionPatternAnalyzer final
{
    using BindingKey = std::pair<std::string, PatternBindingMode>;
    struct BindingKeyHash final
    {
        std::size_t operator()(const BindingKey& binding) const noexcept
        {
            const std::size_t name = std::hash<std::string>{}(binding.first);
            const std::size_t mode = std::hash<unsigned>{}(
                static_cast<unsigned>(binding.second));
            return name ^ (mode + 0x9e3779b9U + (name << 6U) + (name >> 2U));
        }
    };
    using BindingCounts = std::unordered_map<BindingKey, std::size_t, BindingKeyHash>;

    struct Summary final
    {
        BindingCounts bindings;
        std::vector<std::string> cases;
        std::vector<std::pair<std::size_t, bool>> listLengths;
        bool catchAll = false;
        bool wildcard = false;
        bool partial = false;
    };

    struct Frame final
    {
        const Pattern* pattern = nullptr;
        std::size_t nextChild = 0;
        std::size_t preorder = 0;
        bool preservesAddress = true;
        std::vector<Summary> children;
    };

public:
    MatchExpressionPatternAnalyzer(Parser& parser, bool reportOrDiagnostics)
        : parser_(parser), reportOrDiagnostics_(reportOrDiagnostics)
    {
    }

    MatchPatternAnalysis analyze(const Pattern& root)
    {
        pushFrame(root, true);
        while (!frames_.empty())
        {
            Frame& frame = frames_.back();
            if (frame.nextChild < frame.pattern->children.size())
            {
                const bool childPreservesAddress = frame.preservesAddress &&
                    frame.pattern->kind != PatternKind::Tuple &&
                    frame.pattern->kind != PatternKind::Variant;
                pushFrame(frame.pattern->children[frame.nextChild++], childPreservesAddress);
                continue;
            }
            Summary summary = finishFrame(frame);
            frames_.pop_back();
            if (frames_.empty()) return finishAnalysis(std::move(summary));
            frames_.back().children.push_back(std::move(summary));
        }
        return {};
    }

private:
    void pushFrame(const Pattern& pattern, bool preservesAddress)
    {
        const std::size_t preorder = patterns_.size();
        patterns_.push_back(&pattern);
        invalidOr_.push_back(false);
        if (pattern.kind == PatternKind::Binding &&
            pattern.bindingMode != PatternBindingMode::Value && !preservesAddress)
        {
            nonAddressableBindingRanges_.push_back(pattern.range);
            nonAddressableBindings_.emplace_back(pattern.range, pattern.bindingMode);
        }
        Frame frame{&pattern, 0, preorder, preservesAddress, {}};
        frame.children.reserve(pattern.children.size());
        frames_.push_back(std::move(frame));
    }

    Summary finishFrame(Frame& frame)
    {
        Summary summary;
        const Pattern& pattern = *frame.pattern;
        summary.catchAll = pattern.kind == PatternKind::Wildcard ||
            pattern.kind == PatternKind::Binding;
        summary.wildcard = summary.catchAll;
        if (pattern.kind == PatternKind::Binding)
            summary.bindings[{pattern.name, pattern.bindingMode}] = 1U;
        if (pattern.kind == PatternKind::Or) summarizeOr(frame, summary);
        if (pattern.kind == PatternKind::Variant) summarizeVariant(frame, summary);
        if (pattern.kind == PatternKind::List) summarizeList(frame, summary);
        for (Summary& child : frame.children)
            mergeBindings(summary.bindings, child.bindings);
        return summary;
    }

    void summarizeOr(Frame& frame, Summary& summary)
    {
        if (!frame.children.empty())
        {
            const BindingCounts& expected = frame.children.front().bindings;
            invalidOr_[frame.preorder] = std::ranges::any_of(
                frame.children.begin() + 1, frame.children.end(),
                [&](const Summary& child) { return child.bindings != expected; });
        }
        for (Summary& child : frame.children)
        {
            summary.catchAll = summary.catchAll || child.catchAll;
            summary.wildcard = summary.wildcard || child.wildcard;
            summary.partial = summary.partial || child.partial;
            mergeVector(summary.cases, child.cases);
            mergeVector(summary.listLengths, child.listLengths);
        }
    }

    static void summarizeVariant(const Frame& frame, Summary& summary)
    {
        summary.cases.push_back(frame.pattern->name);
        if (!frame.children.empty() && !frame.children.front().catchAll)
            summary.partial = true;
    }

    static void summarizeList(const Frame& frame, Summary& summary)
    {
        const Pattern& pattern = *frame.pattern;
        const std::size_t fixedCount = pattern.children.size() -
            (pattern.restIndex.has_value() ? 1U : 0U);
        summary.listLengths.emplace_back(fixedCount, pattern.restIndex.has_value());
        for (std::size_t index = 0; index < frame.children.size(); ++index)
        {
            if (pattern.restIndex.has_value() && *pattern.restIndex == index) continue;
            if (!frame.children[index].catchAll) { summary.partial = true; return; }
        }
    }

    static void mergeBindings(BindingCounts& target, BindingCounts& source)
    {
        if (target.size() < source.size()) target.swap(source);
        for (const auto& [binding, count] : source) target[binding] += count;
        source.clear();
    }

    template <typename Value>
    static void mergeVector(std::vector<Value>& target, std::vector<Value>& source)
    {
        if (target.size() < source.size()) target.swap(source);
        target.insert(
            target.end(),
            std::make_move_iterator(source.begin()),
            std::make_move_iterator(source.end()));
        source.clear();
    }

    MatchPatternAnalysis finishAnalysis(Summary summary)
    {
        reportInvalidOrPatterns();
        MatchPatternAnalysis result;
        for (const auto& [binding, count] : summary.bindings)
            for (std::size_t index = 0; index < count; ++index)
                result.bindings.push_back(binding);
        std::ranges::sort(result.bindings);
        for (std::size_t index = 0; index < invalidOr_.size(); ++index)
            if (invalidOr_[index]) result.invalidOrRanges.push_back(patterns_[index]->range);
        result.nonAddressableBindingRanges = std::move(nonAddressableBindingRanges_);
        result.nonAddressableBindings = std::move(nonAddressableBindings_);
        result.cases = std::move(summary.cases);
        result.listLengths = std::move(summary.listLengths);
        result.wildcard = summary.wildcard;
        result.partial = summary.partial;
        return result;
    }

    void reportInvalidOrPatterns()
    {
        if (!reportOrDiagnostics_) return;
        for (std::size_t index = 0; index < invalidOr_.size(); ++index)
        {
            if (!invalidOr_[index]) continue;
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                patterns_[index]->range,
                "OR-pattern alternatives in a match expression must bind identical names "
                "with identical value/ref/move modes");
        }
    }

    Parser& parser_;
    bool reportOrDiagnostics_ = false;
    std::vector<Frame> frames_;
    std::vector<const Pattern*> patterns_;
    std::vector<bool> invalidOr_;
    std::vector<SourceRange> nonAddressableBindingRanges_;
    std::vector<std::pair<SourceRange, PatternBindingMode>> nonAddressableBindings_;
};

Parser::MatchPatternAnalysis Parser::analyzeMatchExpressionPattern(
    const Pattern& pattern,
    bool reportOrDiagnostics)
{
    return MatchExpressionPatternAnalyzer(*this, reportOrDiagnostics).analyze(pattern);
}

void Parser::appendMatchCoverage(
    MatchPatternAnalysis analysis,
    const ast::AstNode* guard,
    std::string& coverage,
    bool& firstArm) const
{
    if (!firstArm) coverage += ';';
    firstArm = false;
    const bool restricted = guard != nullptr &&
        !(guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
          guard->tokenKind() == TokenKind::KwTrue);
    if (analysis.wildcard)
    {
        if (restricted) coverage += '~';
        coverage += '*';
        return;
    }
    if (!analysis.listLengths.empty())
    {
        std::ranges::sort(analysis.listLengths);
        analysis.listLengths.erase(
            std::unique(analysis.listLengths.begin(), analysis.listLengths.end()),
            analysis.listLengths.end());
        if (analysis.partial || restricted) coverage += '~';
        for (std::size_t index = 0; index < analysis.listLengths.size(); ++index)
        {
            if (index != 0) coverage += '|';
            coverage += '#' + std::to_string(analysis.listLengths[index].first);
            if (analysis.listLengths[index].second) coverage += '+';
        }
        return;
    }
    if (analysis.cases.empty()) { coverage += '?'; return; }
    std::ranges::sort(analysis.cases);
    analysis.cases.erase(
        std::unique(analysis.cases.begin(), analysis.cases.end()), analysis.cases.end());
    if (analysis.partial || restricted) coverage += '~';
    for (std::size_t index = 0; index < analysis.cases.size(); ++index)
    {
        if (index != 0) coverage += '|';
        coverage += analysis.cases[index];
    }
}

void Parser::appendMatchExpressionAlternative(
    ast::AstNode* match,
    const Pattern& pattern,
    ast::AstNode* guard,
    ast::AstNode* value,
    bool referenceResult,
    const std::string& temporary,
    std::size_t armBegin,
    std::size_t alternativeIndex,
    std::string& coverage,
    bool& firstArm)
{
    ast::AstNode* arm = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MatchArmExpression, tokenRangeFrom(armBegin));
    PatternExpressionFactory source = [&, range = pattern.range]
    {
        return arena_.make<ast::AstNode>(ast::AstNodeKind::MatchValueExpression, range);
    };
    arm->setFirst(makePatternCondition(pattern, source));
    arm->setSecond(alternativeIndex == 0U ? guard : cloneDeclarationNode(guard));
    arm->setThird(alternativeIndex == 0U ? value : cloneDeclarationNode(value));
    arm->setFlag(referenceResult);
    emitPatternBindings(pattern, source, arm, temporary);
    match->addChild(arm);
    appendMatchCoverage(
        analyzeMatchExpressionPattern(pattern, false), guard, coverage, firstArm);
}

void Parser::parseMatchExpressionArm(
    ast::AstNode* match,
    const std::string& temporary,
    std::string& coverage,
    bool& firstArm,
    bool& hasReferenceBinding)
{
    const std::size_t armBegin = index_;
    Pattern pattern = parsePattern(false);
    const MatchPatternAnalysis analysis = analyzeMatchExpressionPattern(pattern, true);
    hasReferenceBinding = hasReferenceBinding || std::ranges::any_of(
        analysis.bindings, [](const auto& binding)
        {
            return binding.second == PatternBindingMode::Reference ||
                binding.second == PatternBindingMode::ReferenceMutable;
        });
    ast::AstNode* guard = nullptr;
    if (atContextualKeyword("when")) { consume(); guard = parseConditionalExpression(); }
    expect(TokenKind::FatArrow, "'=>' after match expression pattern");
    const bool referenceResult = atContextualKeyword("ref");
    if (referenceResult) consume();
    ast::AstNode* value = parseExpression();
    const std::vector<Pattern> alternatives = patternHasBindings(pattern)
        ? expandPatternAlternatives(pattern) : std::vector<Pattern>{pattern};
    for (std::size_t index = 0; index < alternatives.size(); ++index)
        appendMatchExpressionAlternative(
            match, alternatives[index], guard, value, referenceResult, temporary,
            armBegin, index, coverage, firstArm);
    if (!consumeIf(TokenKind::Comma) && !at(TokenKind::RightBrace))
        expect(TokenKind::Comma, "',' between match expression arms");
}

ast::AstNode* Parser::parseMatchExpression()
{
    const std::size_t beginIndex = index_;
    const lexer::Token& keyword = consume();
    expect(TokenKind::LeftParen, "'(' after 'match'");
    ast::AstNode* governing = parseExpression();
    expect(TokenKind::RightParen, "')' after match governing expression");
    expect(TokenKind::LeftBrace, "'{' to open match expression arms");
    ast::AstNode* match = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MatchExpression, keyword.range());
    match->setFirst(governing);
    const std::string temporary = "$__nsc_match_expr_" +
        std::to_string(nextMatchTemporary_++);
    match->setText(temporary);
    std::string coverage = "match-coverage:v1:";
    bool firstArm = true;
    bool hasReferenceBinding = false;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        parseMatchExpressionArm(
            match, temporary, coverage, firstArm, hasReferenceBinding);
    expect(TokenKind::RightBrace, "'}' to close match expression arms");
    if (match->children().empty())
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            keyword.range(),
            "a match expression requires at least one arm");
    match->setAuxiliaryText(std::move(coverage));
    match->setFlag(hasReferenceBinding);
    match->setRange(tokenRangeFrom(beginIndex));
    return match;
}

ast::AstNode* Parser::parseLoopExpression()
{
    const std::size_t beginIndex = index_;
    consume();

    std::string label;
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::LeftBrace)
        label = std::string(consume().lexeme());

    if (!at(TokenKind::LeftBrace))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected '{' after loop expression" +
                (label.empty() ? std::string{} : " label '" + label + "'"));
    }
    ast::AstNode* body = parseBlock();
    ast::AstNode* loop = arena_.make<ast::AstNode>(
        ast::AstNodeKind::LoopExpression, tokenRangeFrom(beginIndex));
    loop->setFirst(body);
    loop->setText(std::move(label));
    return loop;
}

ast::AstNode* Parser::parseComprehensionExpression()
{
    const std::size_t beginIndex = index_;
    const SourceRange keywordRange = consume().range();
    expect(TokenKind::LeftParen, "'(' after comprehend");
    ast::AstNode* builder = parseExpression();
    expect(TokenKind::RightParen, "')' after comprehension builder");

    const std::size_t ordinal = nextComprehensionTemporary_++;
    ComprehensionContext context{
        "$__comprehension_builder_" + std::to_string(ordinal),
        "$__comprehension_loop_" + std::to_string(ordinal)};
    comprehensionContexts_.push_back(context);
    ast::AstNode* body = at(TokenKind::LeftBrace) ? parseBlock() : nullptr;
    if (body == nullptr)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            current().range(),
            "expected '{' after comprehension builder");
        body = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, keywordRange);
    }
    comprehensionContexts_.pop_back();

    const SourceRange range = tokenRangeFrom(beginIndex);
    ast::AstNode* loopBody = arena_.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, range);
    ast::TypeSyntax* inferred = arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwVar, std::string("var"));
    ast::AstNode* builderDeclaration = makeVariableDeclaration(
        inferred, context.builderName, builder, false);
    builderDeclaration->setAuxiliaryText("local:comprehension-builder");
    loopBody->addChild(builderDeclaration);
    loopBody->addChild(body);

    ast::AstNode* finish = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, range);
    finish->setText(context.loopLabel);
    ast::AstNode* result = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, range);
    result->setFirst(makeIdentifierExpression(range, context.builderName));
    result->setAuxiliaryText("comprehension-result");
    finish->setFirst(result);
    loopBody->addChild(finish);

    ast::AstNode* loop = arena_.make<ast::AstNode>(ast::AstNodeKind::LoopExpression, range);
    loop->setText(context.loopLabel);
    loop->setAuxiliaryText("comprehension:explicit-builder");
    loop->setFirst(loopBody);
    return loop;
}

ast::AstNode* Parser::makeIdentifierExpression(const SourceRange& range, std::string name)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IdentifierExpression, range);
    node->setText(std::move(name));
    return node;
}

ast::AstNode* Parser::makeIntegerLiteralExpression(const SourceRange& range, std::uint64_t value)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, range);
    node->setTokenKind(TokenKind::IntegerLiteral);
    node->setText(std::to_string(value));
    node->setIntegerValue(value);
    return node;
}

ast::AstNode* Parser::makeMemberExpression(ast::AstNode* receiver, std::string memberName)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::MemberExpression, receiver->range());
    node->setFirst(receiver);
    node->setText(std::move(memberName));
    return node;
}

ast::AstNode* Parser::makeZeroArgCallExpression(ast::AstNode* callee)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::CallExpression, callee->range());
    node->setFirst(callee);
    return node;
}

ast::AstNode* Parser::makeIndexExpression(ast::AstNode* receiver, ast::AstNode* index)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::IndexExpression, receiver->range());
    node->setFirst(receiver);
    node->setSecond(index);
    node->addChild(index);
    return node;
}

ast::AstNode* Parser::makeBinaryExpression(TokenKind operatorKind, ast::AstNode* left, ast::AstNode* right)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BinaryExpression, join(left->range(), right->range()));
    node->setTokenKind(operatorKind);
    node->setFirst(left);
    node->setSecond(right);
    return node;
}

ast::AstNode* Parser::makeAssignmentExpression(ast::AstNode* target, ast::AstNode* value)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression, join(target->range(), value->range()));
    node->setTokenKind(TokenKind::Equal);
    node->setFirst(target);
    node->setSecond(value);
    return node;
}

ast::AstNode* Parser::makeExpressionStatement(ast::AstNode* expression)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::ExpressionStatement, expression->range());
    node->setFirst(expression);
    return node;
}

ast::AstNode* Parser::makeVariableDeclaration(
    ast::TypeSyntax* type, std::string name, ast::AstNode* initializer, bool isRaii, bool isConst)
{
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement, initializer->range());
    node->setTypeSyntax(type);
    node->setText(std::move(name));
    if (isRaii) node->addModifier(TokenKind::KwRaii);
    // `flag()` is the same bit an ordinary `const T x = ...;` local declaration sets via
    // `variable->setFlag(isConst)` (see parseVariableDeclarationStatement) and
    // ProjectMethodBodyModel::analyzeVariable reads back as `local.isConst` -- reusing the exact
    // same mechanism ordinary locals already use for immutability, rather than inventing a
    // foreach-specific "readonly" concept (P0.3 of the closure review).
    node->setFlag(isConst);
    node->setFirst(initializer);
    return node;
}

ast::AstNode* Parser::parseExpressionImpl()
{
    return parseAssignmentExpression();
}

class Parser::ExpressionIterationMachine final
{
public:
    ExpressionIterationMachine(Parser& parser, bool conditionalOnly) noexcept
        : parser_(parser), work_(conditionalOnly ? WorkKind::Conditional : WorkKind::Assignment)
    {
    }

    ast::AstNode* parse();

private:
    enum class WorkKind { Assignment, Conditional, Deliver };
    enum class ContinuationKind {
        AssignmentTail,
        FinishAssignment,
        ExpressionLambda,
        ConditionalTrue,
        ConditionalFalse
    };

    struct Continuation final
    {
        ContinuationKind kind;
        ast::AstNode* first = nullptr;
        ast::AstNode* second = nullptr;
        TokenKind operation = TokenKind::Invalid;
        bool reference = false;
    };

    void parseAssignment();
    void parseConditional();
    void deliver();
    void deliverAssignmentTail(const Continuation& continuation);
    void deliverContinuation(const Continuation& continuation);
    void buildAssignment(const Continuation& continuation);
    void buildConditional(const Continuation& continuation);

    Parser& parser_;
    WorkKind work_;
    std::vector<Continuation> continuations_;
    ast::AstNode* value_ = nullptr;
};

ast::AstNode* Parser::ExpressionIterationMachine::parse()
{
    while (work_ != WorkKind::Deliver || !continuations_.empty())
    {
        if (work_ == WorkKind::Assignment) parseAssignment();
        else if (work_ == WorkKind::Conditional) parseConditional();
        else deliver();
    }
    return value_;
}

void Parser::ExpressionIterationMachine::parseAssignment()
{
    if (!parser_.looksLikeLambdaExpression())
    {
        continuations_.push_back({ContinuationKind::AssignmentTail});
        work_ = WorkKind::Conditional;
        return;
    }

    ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
    if (parser_.at(TokenKind::LeftBrace))
    {
        parser_.finishLambdaExpression(lambda, parser_.parseBlock(), true);
        value_ = lambda;
        work_ = WorkKind::Deliver;
        return;
    }
    continuations_.push_back({ContinuationKind::ExpressionLambda, lambda});
}

void Parser::ExpressionIterationMachine::parseConditional()
{
    ast::AstNode* condition = parser_.parseNullCoalescingExpression();
    if (!parser_.at(TokenKind::Question))
    {
        value_ = condition;
        work_ = WorkKind::Deliver;
        return;
    }

    parser_.consume();
    continuations_.push_back({ContinuationKind::ConditionalTrue, condition});
    work_ = WorkKind::Assignment;
}

void Parser::ExpressionIterationMachine::deliver()
{
    const Continuation continuation = continuations_.back();
    continuations_.pop_back();
    if (continuation.kind == ContinuationKind::AssignmentTail)
    {
        deliverAssignmentTail(continuation);
        return;
    }
    deliverContinuation(continuation);
}

void Parser::ExpressionIterationMachine::deliverAssignmentTail(const Continuation&)
{
    if (!lexer::isAssignmentOperator(parser_.current().kind())) return;

    const TokenKind operation = parser_.consume().kind();
    const bool referenceAssignment = operation == TokenKind::Equal &&
        parser_.atContextualKeyword("ref");
    if (referenceAssignment) parser_.consume();
    continuations_.push_back(
        {ContinuationKind::FinishAssignment, value_, nullptr, operation, referenceAssignment});
    work_ = WorkKind::Assignment;
}

void Parser::ExpressionIterationMachine::deliverContinuation(
    const Continuation& continuation)
{
    switch (continuation.kind)
    {
    case ContinuationKind::FinishAssignment:
        buildAssignment(continuation);
        return;
    case ContinuationKind::ExpressionLambda:
        parser_.finishLambdaExpression(continuation.first, value_, false);
        value_ = continuation.first;
        return;
    case ContinuationKind::ConditionalTrue:
        parser_.expect(TokenKind::Colon, "':' in a conditional expression");
        continuations_.push_back(
            {ContinuationKind::ConditionalFalse, continuation.first, value_});
        work_ = WorkKind::Assignment;
        return;
    case ContinuationKind::ConditionalFalse:
        buildConditional(continuation);
        return;
    case ContinuationKind::AssignmentTail:
        return;
    }
}

void Parser::ExpressionIterationMachine::buildAssignment(const Continuation& continuation)
{
    ast::AstNode* assignment = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression,
        parser_.join(continuation.first->range(), value_->range()));
    assignment->setTokenKind(continuation.operation);
    assignment->setFirst(continuation.first);
    assignment->setSecond(value_);
    assignment->setReference(continuation.reference);
    value_ = assignment;
}

void Parser::ExpressionIterationMachine::buildConditional(const Continuation& continuation)
{
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConditionalExpression,
        parser_.join(continuation.first->range(),
            value_ != nullptr ? value_->range() : continuation.first->range()));
    node->setFirst(continuation.first);
    node->setSecond(continuation.second);
    node->setThird(value_);
    value_ = node;
}

ast::AstNode* Parser::parseAssignmentExpression()
{
    return ExpressionIterationMachine(*this, false).parse();
}

// `condition ? whenTrue : whenFalse`, between assignment and logical-or in precedence, and
// RIGHT-associative -- so `a ? b : c ? d : e` is `a ? b : (c ? d : e)`, and the false branch
// accepts an assignment (`c ? x : y = z` is `c ? x : (y = z)`), which is what C# does.
//
// The true branch parses as a full expression because it sits between `?` and `:`, where a
// nested conditional cannot be ambiguous.
ast::AstNode* Parser::parseConditionalExpression()
{
    return ExpressionIterationMachine(*this, true).parse();
}

ast::AstNode* Parser::parseNullCoalescingExpression()
{
    ast::AstNode* operand = parseRangeExpression();
    if (!at(TokenKind::QuestionQuestion)) return operand;

    std::vector<ast::AstNode*> operands;
    std::vector<SourceRange> operations;
    do
    {
        operands.push_back(operand);
        operations.push_back(consume().range());
        operand = parseRangeExpression();
    } while (at(TokenKind::QuestionQuestion));

    ast::AstNode* right = operand;
    for (std::size_t index = operations.size(); index != 0; --index)
    {
        ast::AstNode* const left = operands[index - 1U];
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::NullCoalescingExpression,
            join(left->range(), right != nullptr ? right->range() : operations[index - 1U]));
        node->setTokenKind(TokenKind::QuestionQuestion);
        node->setFirst(left);
        node->setSecond(right);
        right = node;
    }
    return right;
}

// Integer ranges are ordinary expressions as well as index operands. Keeping the
// operator below null-coalescing but above logical-or lets `foreach (... in begin..end)` reach the
// same RangeExpression node that slicing already uses. Bracket parsing continues to handle the
// fully open `[..]` spelling; this level admits an omitted endpoint only at a delimiter so it
// never consumes a statement or argument that follows the range.
ast::AstNode* Parser::parseRangeExpression()
{
    // A range delimiter may open the expression. Do not ask the primary-expression path to
    // manufacture an operand for `..end`; the missing begin endpoint is part of the range shape.
    ast::AstNode* begin = nullptr;
    if (!at(TokenKind::DotDot) && !at(TokenKind::DotDotEqual))
        begin = parseLogicalOrExpression();
    if (!at(TokenKind::DotDot) && !at(TokenKind::DotDotEqual)) return begin;

    const lexer::Token& delimiter = consume();
    const SourceRange operation = delimiter.range();
    const bool inclusive = delimiter.kind() == TokenKind::DotDotEqual;
    ast::AstNode* end = nullptr;
    if (!at(TokenKind::EndOfFile) && !at(TokenKind::RightParen) && !at(TokenKind::RightBracket) &&
        !at(TokenKind::Comma) && !at(TokenKind::Semicolon))
    {
        end = parseLogicalOrExpression();
    }
    ast::AstNode* step = nullptr;
    if (atContextualKeyword("step"))
    {
        const SourceRange stepKeyword = consume().range();
        if (at(TokenKind::RightParen) || at(TokenKind::RightBracket) ||
            at(TokenKind::Comma) || at(TokenKind::Semicolon))
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedExpression,
                diagnostics::DiagnosticSeverity::Error,
                stepKeyword,
                "expected an integral expression after range step");
        }
        else
        {
            step = parseLogicalOrExpression();
        }
    }
    if (inclusive && end == nullptr)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            operation,
            "an inclusive range requires an end endpoint");
    }
    ast::AstNode* range = arena_.make<ast::AstNode>(
        ast::AstNodeKind::RangeExpression,
        SourceRange(begin != nullptr ? begin->range().begin() : operation.begin(),
            step != nullptr ? step->range().end() :
            (end != nullptr ? end->range().end() : operation.end())));
    range->setTokenKind(delimiter.kind());
    range->setFirst(begin);
    range->setSecond(end);
    range->setThird(step);
    range->setFlag(inclusive);

    const std::optional<std::int64_t> constantBegin = signedIntegerLiteral(begin);
    const std::optional<std::int64_t> constantEnd = signedIntegerLiteral(end);
    const std::optional<std::int64_t> constantStep = step != nullptr
        ? signedIntegerLiteral(step) : std::optional<std::int64_t>(1);
    if (constantBegin.has_value() && constantEnd.has_value() && constantStep.has_value())
    {
        std::string rangeError;
        if (!language::lazyRangeCount(
                {*constantBegin, *constantEnd, *constantStep, inclusive}, rangeError).has_value())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                range->range(),
                rangeError);
        }
    }
    return range;
}

bool Parser::hasLambdaFatArrowBeforeBoundary(std::size_t cursor) const noexcept
{
    for (; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::FatArrow) return true;
        if (kind == TokenKind::Semicolon || kind == TokenKind::LeftBrace ||
            kind == TokenKind::RightBrace || kind == TokenKind::EndOfFile)
            return false;
    }
    return false;
}

std::size_t Parser::skipLambdaGenericHead(std::size_t begin) const noexcept
{
    if (begin >= tokens_.size() || tokens_[begin].kind() != TokenKind::Less) return begin;
    std::size_t depth = 1;
    for (std::size_t cursor = begin + 1; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::Less) ++depth;
        else if (kind == TokenKind::Greater && --depth == 0) return cursor + 1;
        else if (kind == TokenKind::Semicolon || kind == TokenKind::FatArrow) break;
    }
    return tokens_.size();
}

bool Parser::looksLikeParenthesizedLambdaHead(std::size_t begin) const noexcept
{
    std::size_t parenthesisDepth = 1;
    std::size_t bracketDepth = 0;
    for (std::size_t cursor = begin + 1; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::LeftParen) ++parenthesisDepth;
        else if (kind == TokenKind::RightParen && --parenthesisDepth == 0)
        {
            if (cursor + 1 >= tokens_.size()) return false;
            if (tokens_[cursor + 1].kind() == TokenKind::FatArrow) return true;
            if (tokens_[cursor + 1].kind() == TokenKind::Arrow)
                return hasLambdaFatArrowBeforeBoundary(cursor + 2);
            return tokens_[cursor + 1].kind() == TokenKind::Identifier &&
                tokens_[cursor + 1].lexeme() == "where" &&
                hasLambdaFatArrowBeforeBoundary(cursor + 2);
        }
        else if (kind == TokenKind::LeftBracket) ++bracketDepth;
        else if (kind == TokenKind::RightBracket && bracketDepth != 0) --bracketDepth;
        else if (kind == TokenKind::Semicolon && parenthesisDepth == 1 && bracketDepth == 0)
            return false;
    }
    return false;
}

bool Parser::looksLikeLambdaParameterHead(std::size_t begin) const noexcept
{
    if (begin >= tokens_.size()) return false;
    begin = skipLambdaGenericHead(begin);
    if (begin >= tokens_.size()) return false;
    if (tokens_[begin].kind() == TokenKind::Identifier)
    {
        if (begin + 1 >= tokens_.size()) return false;
        if (tokens_[begin + 1].kind() == TokenKind::FatArrow) return true;
        return tokens_[begin + 1].kind() == TokenKind::Arrow &&
            hasLambdaFatArrowBeforeBoundary(begin + 2);
    }
    return tokens_[begin].kind() == TokenKind::LeftParen &&
        looksLikeParenthesizedLambdaHead(begin);
}

bool Parser::looksLikeLambdaCaptureHead() const noexcept
{
    std::size_t depth = 1;
    for (std::size_t cursor = index_ + 1; cursor < tokens_.size(); ++cursor)
    {
        if (tokens_[cursor].kind() == TokenKind::LeftBracket) ++depth;
        else if (tokens_[cursor].kind() == TokenKind::RightBracket && --depth == 0)
            return looksLikeLambdaParameterHead(cursor + 1);
        else if (tokens_[cursor].kind() == TokenKind::Semicolon) return false;
    }
    return false;
}

bool Parser::looksLikeLambdaExpression() const noexcept
{
    if (at(TokenKind::KwStatic) && index_ + 1 < tokens_.size() &&
        looksLikeLambdaParameterHead(index_ + 1))
        return true;
    if ((at(TokenKind::KwNoHeap) || atContextualKeyword("unsafe")) &&
        index_ + 1 < tokens_.size() && looksLikeLambdaParameterHead(index_ + 1))
        return true;
    if (at(TokenKind::KwExtern) && index_ + 2 < tokens_.size() &&
        tokens_[index_ + 1].kind() == TokenKind::KwC &&
        looksLikeLambdaParameterHead(index_ + 2))
        return true;
    if (!at(TokenKind::LeftBracket)) return looksLikeLambdaParameterHead(index_);
    return looksLikeLambdaCaptureHead();
}

// Detects only a lambda whose already-recognized head is followed by a block body. The scan is
// side-effect free, linear in the head length, and leaves expression-bodied lambdas untouched.
bool Parser::lambdaBlockBodyStartsHere() const noexcept
{
    if (!looksLikeLambdaExpression()) return false;
    std::size_t parenthesisDepth = 0;
    std::size_t bracketDepth = 0;
    for (std::size_t cursor = index_; cursor + 1 < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::LeftParen) ++parenthesisDepth;
        else if (kind == TokenKind::RightParen && parenthesisDepth != 0) --parenthesisDepth;
        else if (kind == TokenKind::LeftBracket) ++bracketDepth;
        else if (kind == TokenKind::RightBracket && bracketDepth != 0) --bracketDepth;
        else if (kind == TokenKind::FatArrow && parenthesisDepth == 0 && bracketDepth == 0)
            return tokens_[cursor + 1].kind() == TokenKind::LeftBrace;
        else if (parenthesisDepth == 0 && bracketDepth == 0 &&
            (kind == TokenKind::Semicolon || kind == TokenKind::LeftBrace ||
             kind == TokenKind::RightBrace || kind == TokenKind::EndOfFile))
            return false;
    }
    return false;
}

void Parser::parseLambdaPrefix(
    ast::AstNode* lambda,
    std::string& effects,
    std::string& callingConvention)
{
    if (at(TokenKind::KwStatic))
    {
        consume();
        lambda->addModifier(TokenKind::KwStatic);
    }
    if (at(TokenKind::KwNoHeap)) { effects = "noheap"; consume(); }
    else if (atContextualKeyword("unsafe")) { effects = "unsafe"; consume(); }
    if (at(TokenKind::KwExtern) && peek().kind() == TokenKind::KwC)
    {
        callingConvention = "C";
        consume();
        consume();
    }
}

std::string Parser::parseLambdaCaptureMode()
{
    if (atContextualKeyword("copy")) { consume(); return "copy"; }
    if (at(TokenKind::KwMove) || atContextualKeyword("move"))
    {
        consume();
        return "move";
    }
    if (atContextualKeyword("ref"))
    {
        consume();
        if (atContextualKeyword("mut")) { consume(); return "mutref"; }
        return "ref";
    }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected capture mode 'copy', 'move', 'ref', or 'ref mut'");
    if (!at(TokenKind::RightBracket) && !at(TokenKind::EndOfFile)) consume();
    return "invalid";
}

std::string Parser::parseLambdaCaptureName(SourceRange& range)
{
    range = current().range();
    if (at(TokenKind::Identifier)) return std::string(consume().lexeme());
    if (at(TokenKind::KwThis)) { consume(); return "this"; }
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        current().range(),
        "expected capture name or 'this'");
    if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket) &&
        !at(TokenKind::EndOfFile))
        consume();
    return {};
}

void Parser::parseLambdaCaptureList(std::string& contract)
{
    contract = "captures:";
    std::unordered_set<std::string> names;
    while (!at(TokenKind::RightBracket) && !at(TokenKind::EndOfFile))
    {
        const std::string mode = parseLambdaCaptureMode();
        SourceRange range = current().range();
        const std::string name = parseLambdaCaptureName(range);
        if (!name.empty() && !names.insert(name).second)
            diagnostics_.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                range,
                "duplicate lambda capture '" + name + "'");
        if (!name.empty())
        {
            if (contract.size() != std::string_view("captures:").size()) contract += ';';
            contract += mode + "=" + name;
        }
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightBracket, "']' after lambda capture list");
}

ast::AstNode* Parser::parseLambdaParameter()
{
    const bool modifier = atContextualKeyword("ref") ||
        atContextualKeyword("out") || at(TokenKind::KwIn);
    if (modifier && peek().kind() == TokenKind::Identifier &&
        (peek(2).kind() == TokenKind::Comma || peek(2).kind() == TokenKind::RightParen))
    {
        const lexer::Token& passMode = consume();
        const lexer::Token& parameter = consume();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ParameterDeclaration, passMode.range());
        node->setText(std::string(parameter.lexeme()));
        node->setAuxiliaryText("pass:" + std::string(passMode.lexeme()));
        return node;
    }
    if (at(TokenKind::Identifier) &&
        (peek().kind() == TokenKind::Comma || peek().kind() == TokenKind::RightParen))
    {
        const lexer::Token& parameter = consume();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ParameterDeclaration, parameter.range());
        node->setText(std::string(parameter.lexeme()));
        return node;
    }
    return parseParameter();
}

std::vector<ast::AstNode*> Parser::parseLambdaParameters()
{
    std::vector<ast::AstNode*> parameters;
    if (at(TokenKind::Identifier))
    {
        const lexer::Token& parameter = consume();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ParameterDeclaration, parameter.range());
        node->setText(std::string(parameter.lexeme()));
        parameters.push_back(node);
        return parameters;
    }
    expect(TokenKind::LeftParen, "'('");
    if (!at(TokenKind::RightParen))
    {
        while (true)
        {
            if (ast::AstNode* parameter = parseLambdaParameter()) parameters.push_back(parameter);
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightParen, "')'");
    return parameters;
}

void Parser::materializeLambdaAutoParameters(
    ast::AstNode* lambda,
    const std::vector<ast::AstNode*>& parameters)
{
    std::size_t ordinal = 0;
    for (ast::AstNode* parameter : parameters)
    {
        if (parameter == nullptr || parameter->typeSyntax() == nullptr ||
            parameter->typeSyntax()->baseName() != "auto")
            continue;
        const std::string name = "__lambda_auto_" + std::to_string(ordinal++);
        ast::GenericParameterSyntax inferred;
        inferred.name = name;
        inferred.kind = ast::GenericParameterSyntaxKind::Type;
        inferred.range = parameter->typeSyntax()->range();
        lambda->addGenericParameter(std::move(inferred));
        parameter->typeSyntax()->setBaseName(name);
    }
}

void Parser::applyLambdaContracts(
    ast::AstNode* lambda,
    std::string capture,
    const std::string& effects,
    const std::string& callingConvention,
    const std::string& attribute)
{
    if (!effects.empty())
    {
        if (!capture.empty()) capture += ';';
        capture += "effects:" + effects;
    }
    if (!callingConvention.empty())
    {
        if (!capture.empty()) capture += ';';
        capture += "calling-convention:" + callingConvention;
    }
    if (!attribute.empty())
    {
        if (!capture.empty()) capture += ';';
        capture += attribute;
    }
    lambda->setAuxiliaryText(std::move(capture));
}

ast::AstNode* Parser::parseLambdaExpressionHead()
{
    const std::size_t beginIndex = index_;
    ast::AstNode* lambda = arena_.make<ast::AstNode>(
        ast::AstNodeKind::LambdaExpression, tokens_[beginIndex].range());
    std::string effects;
    std::string callingConvention;
    std::string captureContract;
    std::string attributeContract;
    parseLambdaPrefix(lambda, effects, callingConvention);
    parseLambdaCaptureOrAttribute(captureContract, attributeContract);
    if (at(TokenKind::Less)) parseGenericParameterList(lambda);
    std::vector<ast::AstNode*> parameters = parseLambdaParameters();
    materializeLambdaAutoParameters(lambda, parameters);
    if (!lambda->genericParameters().empty()) parseGenericConstraintClauses(lambda);
    if (consumeIf(TokenKind::Arrow)) lambda->setTypeSyntax(parseType());
    expect(TokenKind::FatArrow, "'=>'");
    lambda->setRange(tokenRangeFrom(beginIndex));
    applyLambdaContracts(
        lambda, std::move(captureContract), effects, callingConvention, attributeContract);
    for (ast::AstNode* parameter : parameters) lambda->addChild(parameter);
    return lambda;
}

void Parser::finishLambdaExpression(
    ast::AstNode* lambda,
    ast::AstNode* body,
    bool blockBody)
{
    lambda->setFlag(blockBody);
    lambda->setFirst(body);
    if (body != nullptr)
        lambda->setRange(SourceRange(lambda->range().begin(), body->range().end()));
}

ast::AstNode* Parser::parseLambdaExpression()
{
    return parseAssignmentExpression();
}

ast::AstNode* Parser::parseLogicalOrExpression()
{
    return parseBinaryLevel(&Parser::parseLogicalAndExpression, kLogicalOr, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseLogicalAndExpression()
{
    return parseBinaryLevel(&Parser::parseBitwiseOrExpression, kLogicalAnd, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseBitwiseOrExpression()
{
    return parseBinaryLevel(&Parser::parseBitwiseXorExpression, kBitwiseOr, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseBitwiseXorExpression()
{
    return parseBinaryLevel(&Parser::parseBitwiseAndExpression, kBitwiseXor, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseBitwiseAndExpression()
{
    return parseBinaryLevel(&Parser::parseEqualityExpression, kBitwiseAnd, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseEqualityExpression()
{
    return parseBinaryLevel(&Parser::parseRelationalExpression, kEquality, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseRelationalExpression()
{
    ast::AstNode* left = parseShiftExpression();
    while (containsExpressionToken(kRelational, current().kind()) || at(TokenKind::KwIs) || at(TokenKind::KwAs))
    {
        if (at(TokenKind::KwIs) || at(TokenKind::KwAs))
        {
            const TokenKind operation = consume().kind();
            ast::TypeSyntax* targetType = parseType();
            if (targetType == nullptr)
            {
                return left;
            }
            ast::AstNode* node = arena_.make<ast::AstNode>(
                operation == TokenKind::KwIs
                    ? ast::AstNodeKind::TypeTestExpression
                    : ast::AstNodeKind::SafeCastExpression,
                join(left->range(), targetType->range()));
            node->setTokenKind(operation);
            node->setTypeSyntax(targetType);
            node->setFirst(left);
            left = node;
            continue;
        }

        const TokenKind operation = consume().kind();
        ast::AstNode* right = parseShiftExpression();
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::BinaryExpression,
            join(left->range(), right->range()));
        node->setTokenKind(operation);
        node->setFirst(left);
        node->setSecond(right);
        left = node;
    }
    return left;
}

ast::AstNode* Parser::parseShiftExpression()
{
    return parseBinaryLevel(&Parser::parseAdditiveExpression, kShift, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseAdditiveExpression()
{
    return parseBinaryLevel(&Parser::parseMultiplicativeExpression, kAdditive, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseMultiplicativeExpression()
{
    return parseBinaryLevel(&Parser::parseUnaryExpression, kMultiplicative, ast::AstNodeKind::BinaryExpression);
}

ast::AstNode* Parser::parseAwaitUnaryExpression()
{
    const SourceRange range = consume().range();
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::AwaitExpression, range);
    node->setFirst(parseUnaryExpression());
    return node;
}

ast::AstNode* Parser::parseFromEndUnaryExpression()
{
    const SourceRange start = consume().range();
    ast::AstNode* operand = parseUnaryExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::FromEndExpression,
        join(start, operand != nullptr ? operand->range() : start));
    node->setTokenKind(TokenKind::Caret);
    node->setFirst(operand);
    return node;
}

ast::AstNode* Parser::parseMoveUnaryExpression()
{
    const SourceRange start = consume().range();
    expect(TokenKind::LeftParen, "'(' after move");
    ast::AstNode* operand = parseExpression();
    expect(TokenKind::RightParen, "')' after move operand");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, join(start, tokens_[index_ - 1].range()));
    node->setFirst(operand);
    return node;
}

ast::AstNode* Parser::parseOrdinaryUnaryExpression()
{
    struct Prefix final { TokenKind kind; SourceRange range; };
    std::vector<Prefix> prefixes;
    while (current().kind() == TokenKind::Plus || current().kind() == TokenKind::Minus ||
           current().kind() == TokenKind::Bang || current().kind() == TokenKind::Tilde ||
           current().kind() == TokenKind::Amp || current().kind() == TokenKind::Star ||
           current().kind() == TokenKind::PlusPlus || current().kind() == TokenKind::MinusMinus)
    {
        const TokenKind kind = current().kind();
        prefixes.push_back(Prefix{kind, consume().range()});
    }
    ast::AstNode* operand = parseUnaryExpression();
    for (auto prefix = prefixes.rbegin(); prefix != prefixes.rend(); ++prefix)
    {
        ast::AstNode* node = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, join(prefix->range, operand->range()));
        node->setTokenKind(prefix->kind);
        node->setFirst(operand);
        operand = node;
    }
    return operand;
}

ast::AstNode* Parser::parseNamedCastExpression()
{
    const lexer::Token& castName = consume();
    expect(TokenKind::Less, "'<' after named cast");
    ast::TypeSyntax* type = parseType();
    expect(TokenKind::Greater, "'>' after named cast target type");
    expect(TokenKind::LeftParen, "'(' after named cast type");
    ast::AstNode* operand = parseExpression();
    expect(TokenKind::RightParen, "')' after named cast operand");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CastExpression, join(castName.range(), tokens_[index_ - 1].range()));
    node->setTypeSyntax(type);
    node->setFirst(operand);
    node->setAuxiliaryText("cast:" + std::string(castName.lexeme()));
    return node;
}

ast::AstNode* Parser::parseParenthesizedCastExpression()
{
    const SourceRange start = consume().range();
    ast::TypeSyntax* type = parseType();
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* operand = parseUnaryExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CastExpression, join(start, operand->range()));
    node->setTypeSyntax(type);
    node->setFirst(operand);
    return node;
}

ast::AstNode* Parser::parseOffsetofExpression()
{
    const SourceRange start = consume().range();
    expect(TokenKind::LeftParen, "'('");
    ast::TypeSyntax* type = parseType();
    expect(TokenKind::Comma, "','");
    std::string fieldName;
    if (at(TokenKind::Identifier)) fieldName = std::string(consume().lexeme());
    else expect(TokenKind::Identifier, "field name");
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::OffsetofExpression, join(start, tokens_[index_ - 1].range()));
    node->setTypeSyntax(type);
    node->setText(std::move(fieldName));
    return node;
}

bool Parser::looksLikeSizeofTypeForm(std::size_t cursor) const noexcept
{
    if (lexer::isOwnershipQualifierKeyword(tokens_[cursor].kind())) ++cursor;
    if (tokens_[cursor].kind() == TokenKind::Identifier)
    {
        ++cursor;
        while (cursor + 1 < tokens_.size() && tokens_[cursor].kind() == TokenKind::Dot &&
               tokens_[cursor + 1].kind() == TokenKind::Identifier)
            cursor += 2;
    }
    else ++cursor;
    while (cursor < tokens_.size())
    {
        if (tokens_[cursor].kind() == TokenKind::Star) { ++cursor; continue; }
        if (tokens_[cursor].kind() != TokenKind::LeftBracket) break;
        int depth = 1;
        for (++cursor; cursor < tokens_.size() && depth != 0; ++cursor)
        {
            if (tokens_[cursor].kind() == TokenKind::LeftBracket) ++depth;
            else if (tokens_[cursor].kind() == TokenKind::RightBracket) --depth;
            else if (tokens_[cursor].kind() == TokenKind::EndOfFile) return false;
        }
        if (depth != 0) return false;
    }
    return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::RightParen;
}

ast::AstNode* Parser::parseSizeofOrAlignofExpression()
{
    const bool alignmentQuery = at(TokenKind::KwAlignof);
    const SourceRange start = consume().range();
    expect(TokenKind::LeftParen, "'('");
    const std::size_t checkpoint = index_;
    if (canStartTypeAt(index_) && looksLikeSizeofTypeForm(index_))
    {
        ast::TypeSyntax* type = parseType();
        expect(TokenKind::RightParen, "')'");
        ast::AstNode* node = arena_.make<ast::AstNode>(
            alignmentQuery ? ast::AstNodeKind::AlignofTypeExpression
                           : ast::AstNodeKind::SizeofTypeExpression,
            tokenRangeFrom(checkpoint));
        node->setTypeSyntax(type);
        node->setRange(join(start, tokens_[index_ - 1].range()));
        return node;
    }
    ast::AstNode* expression = parseExpression();
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        alignmentQuery ? ast::AstNodeKind::AlignofExpression
                       : ast::AstNodeKind::SizeofExpression,
        join(start, tokens_[index_ - 1].range()));
    node->setFirst(expression);
    return node;
}

ast::AstNode* Parser::parseDefaultValueExpression()
{
    const SourceRange start = consume().range();
    expect(TokenKind::LeftParen, "'('");
    ast::TypeSyntax* type = parseType();
    expect(TokenKind::RightParen, "')'");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DefaultValueExpression, join(start, tokens_[index_ - 1].range()));
    node->setTypeSyntax(type);
    return node;
}

bool Parser::parenthesizedNewIsPlacement() const noexcept
{
    std::size_t cursor = index_;
    int depth = 1;
    for (; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::LeftParen) ++depth;
        else if (kind == TokenKind::RightParen && --depth == 0) { ++cursor; break; }
    }
    return cursor < tokens_.size() && canStartTypeAt(cursor);
}

void Parser::appendCallArguments(ast::AstNode* node, TokenKind closingToken)
{
    while (!at(closingToken) && !at(TokenKind::EndOfFile))
    {
        node->addChild(parseArgument());
        if (!consumeIf(TokenKind::Comma)) break;
    }
}

ast::AstNode* Parser::parseTargetTypedNew(const SourceRange& start)
{
    ast::AstNode* creation = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ObjectCreationExpression, start);
    creation->setAuxiliaryText("target-typed");
    appendCallArguments(creation, TokenKind::RightParen);
    expect(TokenKind::RightParen, "')' after target-typed new arguments");
    creation->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
    return creation;
}

void Parser::parsePlacementOptions(
    ast::AstNode*& trailingCapacity,
    std::string& failureProfile)
{
    while (consumeIf(TokenKind::Comma))
    {
        if (atContextualKeyword("trailing") && peek().kind() == TokenKind::Colon)
        {
            consume(); consume();
            trailingCapacity = parseExpression();
            continue;
        }
        if (atContextualKeyword("failure") && peek().kind() == TokenKind::Colon)
        {
            consume(); consume();
            if (atContextualKeyword("panic") || atContextualKeyword("result") ||
                atContextualKeyword("throws") || at(TokenKind::KwNull))
            {
                failureProfile = current().lexeme();
                consume();
            }
            else expect(TokenKind::Colon, "'panic', 'null', 'result' or 'throws' after 'failure:'");
            continue;
        }
        expect(TokenKind::Colon, "':' after placement option");
        break;
    }
}

void Parser::attachPlacementResultType(ast::AstNode* creation, ast::TypeSyntax* type)
{
    ast::TypeSyntax* result = arena_.make<ast::TypeSyntax>(
        type->range(), TokenKind::Identifier, "Result");
    result->addTypeArgument(cloneDeclarationType(type));
    result->addTypeArgument(arena_.make<ast::TypeSyntax>(
        type->range(), TokenKind::Identifier, "AllocationFailure"));
    creation->addTypeSyntax(result);
}

ast::AstNode* Parser::parsePlacementNew(const SourceRange& start)
{
    ast::AstNode* storage = parseExpression();
    ast::AstNode* trailingCapacity = nullptr;
    std::string failureProfile;
    parsePlacementOptions(trailingCapacity, failureProfile);
    expect(TokenKind::RightParen, "')' after placement storage");
    ast::TypeSyntax* type = parseAllocationElementType();
    ast::AstNode* creation = arena_.make<ast::AstNode>(
        ast::AstNodeKind::PlacementConstructionExpression, start);
    creation->setFirst(storage);
    creation->setTypeSyntax(type);
    if (trailingCapacity != nullptr) creation->setAuxiliaryText("trailing");
    if (!failureProfile.empty()) creation->setAllocatorFailureProfile(failureProfile);
    if (failureProfile == "result") attachPlacementResultType(creation, type);
    if (expect(TokenKind::LeftParen, "'(' after placement construction type"))
    {
        appendCallArguments(creation, TokenKind::RightParen);
        expect(TokenKind::RightParen, "')' after placement constructor arguments");
    }
    if (trailingCapacity != nullptr) creation->addChild(trailingCapacity);
    creation->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
    return creation;
}

void Parser::parseArrayAllocationDimensions(
    ast::AstNode* node,
    std::vector<SourceRange>& omittedDimensions,
    std::size_t& countedDimensions)
{
    while (consumeIf(TokenKind::LeftBracket))
    {
        const SourceRange left = tokens_[index_ - 1].range();
        if (at(TokenKind::RightBracket))
        {
            omittedDimensions.push_back(join(left, consume().range()));
            continue;
        }
        std::vector<ast::AstNode*> dimensions{parseExpression()};
        while (consumeIf(TokenKind::Comma)) dimensions.push_back(parseExpression());
        countedDimensions += dimensions.size();
        if (dimensions.size() > 1)
        {
            if (node->first() != nullptr || !omittedDimensions.empty())
                diagnostics_.report(
                    diagnostics::DiagnosticId::UnexpectedToken,
                    diagnostics::DiagnosticSeverity::Error,
                    dimensions.front()->range(),
                    "a rectangular allocation uses one counted bracket group");
            node->setAuxiliaryText("range-array-allocation:" +
                std::to_string(dimensions.size()));
            node->setFirst(dimensions.front());
            for (std::size_t index = 1; index < dimensions.size(); ++index)
                node->addChild(dimensions[index]);
        }
        else if (node->first() == nullptr) node->setFirst(dimensions.front());
        else diagnostics_.report(
            diagnostics::DiagnosticId::UnexpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            dimensions.front()->range(),
            "dynamic jagged allocation accepts exactly one counted dimension");
        expect(TokenKind::RightBracket, "']'");
    }
}

void Parser::finishArrayAllocation(
    ast::AstNode* node,
    ast::TypeSyntax* type,
    const std::vector<SourceRange>& omittedDimensions,
    std::size_t countedDimensions)
{
    const bool inferred = countedDimensions == 0 && !omittedDimensions.empty() &&
        at(TokenKind::LeftBrace);
    if (inferred)
    {
        for (std::size_t index = 1; index < omittedDimensions.size(); ++index)
        {
            const SourceRange& range = omittedDimensions[index - 1];
            type->addModifier(ast::TypeModifierSyntax{
                ast::TypeModifierSyntaxKind::DynamicArray, nullptr, range});
            type->setRange(join(type->range(), range));
        }
        consume();
        node->setAuxiliaryText("inferred-array-initializer");
        while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
        {
            node->addChild(parseAssignmentExpression());
            if (!consumeIf(TokenKind::Comma) || at(TokenKind::RightBrace)) break;
        }
        expect(TokenKind::RightBrace, "'}'");
        return;
    }
    for (const SourceRange& range : omittedDimensions)
    {
        type->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::DynamicArray, nullptr, range});
        type->setRange(join(type->range(), range));
    }
    if (countedDimensions == 0)
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            node->range(),
            "dynamic array allocation requires one count or an aggregate initializer");
}

ast::AstNode* Parser::parseTypedNew(const SourceRange& start)
{
    ast::TypeSyntax* type = parseAllocationElementType();
    if (consumeIf(TokenKind::LeftParen))
    {
        ast::AstNode* creation = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ObjectCreationExpression, start);
        creation->setTypeSyntax(type);
        appendCallArguments(creation, TokenKind::RightParen);
        expect(TokenKind::RightParen, "')'");
        creation->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
        return creation;
    }
    ast::AstNode* node = arena_.make<ast::AstNode>(ast::AstNodeKind::NewExpression, start);
    node->setTypeSyntax(type);
    if (at(TokenKind::LeftBracket))
    {
        node->setFlag(true);
        std::vector<SourceRange> omittedDimensions;
        std::size_t countedDimensions = 0;
        parseArrayAllocationDimensions(node, omittedDimensions, countedDimensions);
        finishArrayAllocation(node, type, omittedDimensions, countedDimensions);
    }
    node->setRange(SourceRange(start.begin(), tokens_[index_ - 1].range().end()));
    return node;
}

ast::AstNode* Parser::parseNewExpression()
{
    const SourceRange start = consume().range();
    if (at(TokenKind::LeftParen) && peek().kind() == TokenKind::RightParen)
    {
        consume(); consume();
        ast::AstNode* creation = arena_.make<ast::AstNode>(
            ast::AstNodeKind::ObjectCreationExpression, start);
        creation->setAuxiliaryText("target-typed");
        creation->setRange(join(start, tokens_[index_ - 1].range()));
        return creation;
    }
    if (!consumeIf(TokenKind::LeftParen)) return parseTypedNew(start);
    if (!parenthesizedNewIsPlacement()) return parseTargetTypedNew(start);
    return parsePlacementNew(start);
}

ast::AstNode* Parser::parseDeleteExpression()
{
    const SourceRange start = consume().range();
    bool arrayForm = false;
    if (consumeIf(TokenKind::LeftBracket))
    {
        arrayForm = true;
        expect(TokenKind::RightBracket, "']'");
    }
    ast::AstNode* operand = parseUnaryExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeleteExpression, join(start, operand->range()));
    node->setFlag(arrayForm);
    node->setFirst(operand);
    return node;
}

ast::AstNode* Parser::parseDestroyExpression()
{
    const SourceRange start = consume().range();
    ast::AstNode* operand = parseUnaryExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::DestroyExpression, join(start, operand->range()));
    node->setFirst(operand);
    return node;
}

ast::AstNode* Parser::parseStackallocExpression()
{
    const SourceRange start = consume().range();
    ast::TypeSyntax* type = parseAllocationElementType();
    expect(TokenKind::LeftBracket, "'['");
    ast::AstNode* count = parseExpression();
    expect(TokenKind::RightBracket, "']'");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StackallocExpression, join(start, tokens_[index_ - 1].range()));
    node->setTypeSyntax(type);
    node->setFirst(count);
    return node;
}

ast::AstNode* Parser::parseUnaryExpression()
{
    if (atContextualKeyword("await") && peek().kind() != TokenKind::Equal &&
        peek().kind() != TokenKind::LeftParen)
        return parseAwaitUnaryExpression();
    if (at(TokenKind::Caret)) return parseFromEndUnaryExpression();
    if (at(TokenKind::KwMove)) return parseMoveUnaryExpression();
    const TokenKind kind = current().kind();
    if (kind == TokenKind::Plus || kind == TokenKind::Minus || kind == TokenKind::Bang ||
        kind == TokenKind::Tilde || kind == TokenKind::Amp || kind == TokenKind::Star ||
        kind == TokenKind::PlusPlus || kind == TokenKind::MinusMinus)
        return parseOrdinaryUnaryExpression();
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Less &&
        (current().lexeme() == "static_cast" || current().lexeme() == "reinterpret_cast" ||
         current().lexeme() == "const_cast"))
        return parseNamedCastExpression();
    if (at(TokenKind::LeftParen) && looksLikeParenthesizedType())
        return parseParenthesizedCastExpression();
    if (at(TokenKind::KwOffsetof)) return parseOffsetofExpression();
    if (at(TokenKind::KwSizeof) || at(TokenKind::KwAlignof))
        return parseSizeofOrAlignofExpression();
    if (at(TokenKind::KwDefault)) return parseDefaultValueExpression();
    if (at(TokenKind::KwNew)) return parseNewExpression();
    if (at(TokenKind::KwDelete)) return parseDeleteExpression();
    if (at(TokenKind::KwDestroy)) return parseDestroyExpression();
    if (at(TokenKind::KwStackalloc)) return parseStackallocExpression();
    return parsePostfixExpression();
}

ast::AstNode* Parser::parseArgument()
{
    std::string argumentName;
    if (at(TokenKind::Identifier) && peek().kind() == TokenKind::Colon)
    {
        argumentName = std::string(consume().lexeme());
        consume();
    }

    // A named by-reference argument is written `name: ref value`, as in C#.
    const bool refArgument = (atContextualKeyword("ref") || atContextualKeyword("out") ||
        at(TokenKind::KwIn)) && peek().kind() != TokenKind::Comma &&
        peek().kind() != TokenKind::RightParen;
    ast::AstNode* argument = nullptr;
    if (refArgument)
    {
        const lexer::Token& keyword = consume();
        const std::string spelling =
            keyword.kind() == TokenKind::KwIn ? "in" : std::string(keyword.lexeme());
        ast::AstNode* operand = parseAssignmentExpression();
        argument = arena_.make<ast::AstNode>(ast::AstNodeKind::UnaryExpression, keyword.range());
        argument->setTokenKind(TokenKind::Amp);
        argument->setFirst(operand);
        argument->setAuxiliaryText("pass:" + spelling);
        if (operand != nullptr) argument->setRange(join(keyword.range(), operand->range()));
    }
    else
    {
        argument = parseAssignmentExpression();
    }
    if (argument != nullptr && !argumentName.empty())
    {
        argument->setArgumentName(std::move(argumentName));
    }
    return argument;
}

ast::AstNode* Parser::parsePostfixBaseExpression()
{
    if (!looksLikeClosedGenericTypeReference()) return nullptr;
    const SourceRange start = current().range();
    ast::TypeSyntax* type = parseType();
    if (type == nullptr) return nullptr;
    ast::AstNode* expression = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IdentifierExpression,
        join(start, tokens_[index_ - 1].range()));
    expression->setText(type->baseName());
    expression->setTypeSyntax(type);
    return expression;
}

class Parser::PostfixIterationMachine final
{
public:
    explicit PostfixIterationMachine(Parser& parser) noexcept : parser_(parser) {}
    ast::AstNode* parse();

private:
    static constexpr std::size_t noSlot = std::numeric_limits<std::size_t>::max();
    enum class WorkKind : std::uint8_t {
        Expression, ExpressionTail, Unary, FinishUnary, FinishBinary,
        FinishAssignment, FinishConditionalTrue, FinishConditionalFalse,
        FinishTypeRelation, FinishRangeEnd, FinishRangeStep,
        Postfix, Primary, Grouping, Collection, FinishNameOf,
        FinishOverflowPolicy, ContinueInterpolation, Call, Index, With
    };
    struct Result final { ast::AstNode* value = nullptr; bool ready = false; };
    struct Work final {
        Work() = default;
        Work(WorkKind workKind, std::size_t result, std::size_t child = noSlot,
            ast::AstNode* current = nullptr, ast::AstNode* prior = nullptr) noexcept
            : kind(workKind), resultSlot(result), childSlot(child), node(current), first(prior)
        {
        }

        WorkKind kind = WorkKind::Postfix;
        std::size_t resultSlot = 0;
        std::size_t childSlot = noSlot;
        ast::AstNode* node = nullptr;
        ast::AstNode* first = nullptr;
        SourceRange range;
        std::string text;
        bool flag = false;
        std::uint8_t precedence = 0;
        TokenKind tokenKind = TokenKind::Invalid;
        std::string auxiliaryText;
        std::vector<ast::AstNode*> children;
        std::size_t frameIndex = noSlot;
        SourceRange outerRange;
    };
    struct InterpolationFrame final {
        std::optional<lexer::Token> token;
        std::vector<language::InterpolationPart> parts;
        std::size_t partIndex = 0;
        ast::AstNode* staticNode = nullptr;
        ast::AstNode* runtimeResult = nullptr;
        bool runtime = false;
        std::unique_ptr<source::SourceFile> fragmentSource;
        std::vector<lexer::Token> fragmentTokens;
        std::span<const lexer::Token> outerTokens;
        std::size_t outerIndex = 0;
        std::vector<std::uint8_t> outerQuestionIndex;
        std::size_t outerPendingGenericGreater = 0;
    };

    std::size_t makeSlot();
    void complete(std::size_t slot, ast::AstNode* value);
    void scheduleChild(std::size_t slot);
    void startExpression(Work work);
    void continueExpression(Work work);
    void startUnary(Work work);
    void finishUnary(Work work);
    void finishBinary(Work work);
    void finishAssignment(Work work);
    void finishConditionalTrue(Work work);
    void finishConditionalFalse(Work work);
    void finishTypeRelation(Work work);
    void finishRangeEnd(Work work);
    void finishRangeStep(Work work);
    void validateConstantRange(ast::AstNode* range) const;
    void startPostfix(Work work);
    void startPrimary(Work work);
    void continueGrouping(Work work);
    void continueCollection(Work work);
    void finishNameOf(Work work);
    void finishOverflowPolicy(Work work);
    void startInterpolationToken(Work work, const lexer::Token& token);
    void scheduleInterpolation(std::size_t frameIndex, std::size_t resultSlot);
    void finishInterpolationHole(Work work);
    void enterInterpolationFragment(InterpolationFrame& frame);
    void restoreInterpolationParser(InterpolationFrame& frame);
    void schedulePrimaryArgument(Work work, bool tupleElement);
    ast::AstNode* finishPrimaryArgument(Work& work);
    bool startReservedPrimary(Work& work);
    bool startKeywordPrimary(Work& work);
    bool startAggregatePrimary(Work& work);
    void continueCall(Work work);
    void continueIndex(Work work);
    void continueWith(Work work);
    ast::AstNode* finishArgument(Work& work);
    void finishCall(Work& work);
    void appendIndex(Work& work, ast::AstNode* value);
    void finishIndex(Work& work);

    Parser& parser_;
    std::deque<Result> results_;
    std::deque<InterpolationFrame> interpolationFrames_;
    std::vector<Work> work_;
};

std::size_t Parser::PostfixIterationMachine::makeSlot()
{
    const std::size_t slot = results_.size();
    results_.push_back({});
    return slot;
}

void Parser::PostfixIterationMachine::complete(std::size_t slot, ast::AstNode* value)
{
    results_[slot] = {value, true};
}

void Parser::PostfixIterationMachine::scheduleChild(std::size_t slot)
{
    Work expression{WorkKind::Expression, slot};
    expression.precedence = 1;
    work_.push_back(std::move(expression));
}

ast::AstNode* Parser::PostfixIterationMachine::parse()
{
    const std::size_t root = makeSlot();
    work_.push_back({WorkKind::Postfix, root});
    while (!results_[root].ready)
    {
        Work work = std::move(work_.back());
        work_.pop_back();
        switch (work.kind)
        {
        case WorkKind::Expression: startExpression(std::move(work)); break;
        case WorkKind::ExpressionTail: continueExpression(std::move(work)); break;
        case WorkKind::Unary: startUnary(std::move(work)); break;
        case WorkKind::FinishUnary: finishUnary(std::move(work)); break;
        case WorkKind::FinishBinary: finishBinary(std::move(work)); break;
        case WorkKind::FinishAssignment: finishAssignment(std::move(work)); break;
        case WorkKind::FinishConditionalTrue: finishConditionalTrue(std::move(work)); break;
        case WorkKind::FinishConditionalFalse: finishConditionalFalse(std::move(work)); break;
        case WorkKind::FinishTypeRelation: finishTypeRelation(std::move(work)); break;
        case WorkKind::FinishRangeEnd: finishRangeEnd(std::move(work)); break;
        case WorkKind::FinishRangeStep: finishRangeStep(std::move(work)); break;
        case WorkKind::Postfix: startPostfix(std::move(work)); break;
        case WorkKind::Primary: startPrimary(std::move(work)); break;
        case WorkKind::Grouping: continueGrouping(std::move(work)); break;
        case WorkKind::Collection: continueCollection(std::move(work)); break;
        case WorkKind::FinishNameOf: finishNameOf(std::move(work)); break;
        case WorkKind::FinishOverflowPolicy: finishOverflowPolicy(std::move(work)); break;
        case WorkKind::ContinueInterpolation: finishInterpolationHole(std::move(work)); break;
        case WorkKind::Call: continueCall(std::move(work)); break;
        case WorkKind::Index: continueIndex(std::move(work)); break;
        case WorkKind::With: continueWith(std::move(work)); break;
        }
    }
    return results_[root].value;
}

namespace
{
std::uint8_t postfixBridgePrecedence(TokenKind kind) noexcept
{
    if (lexer::isAssignmentOperator(kind)) return 1;
    if (kind == TokenKind::Question) return 2;
    if (kind == TokenKind::QuestionQuestion) return 3;
    if (kind == TokenKind::DotDot || kind == TokenKind::DotDotEqual) return 4;
    if (kind == TokenKind::PipePipe) return 5;
    if (kind == TokenKind::AmpAmp) return 6;
    if (kind == TokenKind::Pipe) return 7;
    if (kind == TokenKind::Caret) return 8;
    if (kind == TokenKind::Amp) return 9;
    if (kind == TokenKind::EqualEqual || kind == TokenKind::BangEqual) return 10;
    if (containsExpressionToken(kRelational, kind) || kind == TokenKind::KwIs || kind == TokenKind::KwAs)
        return 11;
    if (containsExpressionToken(kShift, kind)) return 12;
    if (containsExpressionToken(kAdditive, kind)) return 13;
    if (containsExpressionToken(kMultiplicative, kind)) return 14;
    return 0;
}

bool postfixBridgeRightAssociative(TokenKind kind) noexcept
{
    return lexer::isAssignmentOperator(kind) || kind == TokenKind::QuestionQuestion;
}

bool rangeOperandEnds(TokenKind kind) noexcept
{
    return kind == TokenKind::EndOfFile || kind == TokenKind::RightParen ||
        kind == TokenKind::RightBracket || kind == TokenKind::Comma ||
        kind == TokenKind::Semicolon;
}
}

void Parser::PostfixIterationMachine::startExpression(Work work)
{
    if (work.precedence <= 1 && parser_.looksLikeLambdaExpression())
    {
        ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
        if (parser_.at(TokenKind::LeftBrace))
        {
            parser_.finishLambdaExpression(lambda, parser_.parseBlock(), true);
            complete(work.resultSlot, lambda);
            return;
        }
        const std::size_t child = makeSlot();
        Work finish{WorkKind::FinishAssignment, work.resultSlot, child, lambda};
        finish.text = "lambda";
        work_.push_back(std::move(finish));
        scheduleChild(child);
        return;
    }
    if (work.precedence <= 4 &&
        (parser_.at(TokenKind::DotDot) || parser_.at(TokenKind::DotDotEqual)))
    {
        Work range{WorkKind::FinishRangeEnd, work.resultSlot};
        range.range = parser_.consume().range();
        range.tokenKind = parser_.tokens_[parser_.index_ - 1].kind();
        range.precedence = work.precedence;
        if (rangeOperandEnds(parser_.current().kind())) { finishRangeEnd(std::move(range)); return; }
        range.childSlot = makeSlot();
        work_.push_back(range);
        Work rhs{WorkKind::Expression, range.childSlot};
        rhs.precedence = 5;
        work_.push_back(rhs);
        return;
    }
    const std::size_t child = makeSlot();
    Work tail{WorkKind::ExpressionTail, work.resultSlot, child};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
    work_.push_back({WorkKind::Unary, child});
}

void Parser::PostfixIterationMachine::continueExpression(Work work)
{
    ast::AstNode* left = work.node != nullptr ? work.node : results_[work.childSlot].value;
    const TokenKind operation = parser_.current().kind();
    const std::uint8_t precedence = postfixBridgePrecedence(operation);
    if (precedence == 0 || precedence < work.precedence ||
        (operation == TokenKind::Question && !parser_.questionBeginsConditional()))
    {
        complete(work.resultSlot, left);
        return;
    }
    if (operation == TokenKind::Question)
    {
        parser_.consume();
        const std::size_t child = makeSlot();
        work_.push_back({WorkKind::FinishConditionalTrue, work.resultSlot, child, left});
        scheduleChild(child);
        return;
    }
    if (operation == TokenKind::KwIs || operation == TokenKind::KwAs)
    {
        parser_.consume();
        Work finish{WorkKind::FinishTypeRelation, work.resultSlot, noSlot, left};
        finish.tokenKind = operation;
        finish.precedence = work.precedence;
        work_.push_back(std::move(finish));
        return;
    }
    if (operation == TokenKind::DotDot || operation == TokenKind::DotDotEqual)
    {
        Work range{WorkKind::FinishRangeEnd, work.resultSlot, noSlot, nullptr, left};
        range.range = parser_.consume().range();
        range.tokenKind = operation;
        range.precedence = work.precedence;
        if (rangeOperandEnds(parser_.current().kind())) { finishRangeEnd(std::move(range)); return; }
        range.childSlot = makeSlot();
        work_.push_back(range);
        Work rhs{WorkKind::Expression, range.childSlot};
        rhs.precedence = 5;
        work_.push_back(rhs);
        return;
    }
    parser_.consume();
    const bool reference = lexer::isAssignmentOperator(operation) && operation == TokenKind::Equal &&
        parser_.atContextualKeyword("ref");
    if (reference) parser_.consume();
    const std::size_t child = makeSlot();
    const WorkKind finishKind = lexer::isAssignmentOperator(operation)
        ? WorkKind::FinishAssignment : WorkKind::FinishBinary;
    Work finish{finishKind, work.resultSlot, child, left};
    finish.tokenKind = operation;
    finish.flag = reference;
    finish.precedence = work.precedence;
    work_.push_back(std::move(finish));
    Work rhs{WorkKind::Expression, child};
    rhs.precedence = postfixBridgeRightAssociative(operation) ? precedence : precedence + 1;
    work_.push_back(std::move(rhs));
}

void Parser::PostfixIterationMachine::startUnary(Work work)
{
    const TokenKind kind = parser_.current().kind();
    const bool ordinary = kind == TokenKind::Plus || kind == TokenKind::Minus ||
        kind == TokenKind::Bang || kind == TokenKind::Tilde || kind == TokenKind::Amp ||
        kind == TokenKind::Star || kind == TokenKind::PlusPlus || kind == TokenKind::MinusMinus;
    const bool awaiting = parser_.atContextualKeyword("await") && parser_.peek().kind() != TokenKind::Equal &&
        parser_.peek().kind() != TokenKind::LeftParen;
    if (ordinary || awaiting || kind == TokenKind::Caret)
    {
        const lexer::Token& token = parser_.consume();
        const std::size_t child = makeSlot();
        Work finish{WorkKind::FinishUnary, work.resultSlot, child};
        finish.tokenKind = awaiting ? TokenKind::Invalid : kind;
        finish.range = token.range();
        finish.flag = awaiting;
        work_.push_back(std::move(finish));
        work_.push_back({WorkKind::Unary, child});
        return;
    }
    ast::AstNode* delegated = nullptr;
    if ((parser_.at(TokenKind::KwMove) || parser_.atContextualKeyword("move")) &&
        parser_.peek().kind() == TokenKind::LeftParen)
        delegated = parser_.parseMoveUnaryExpression();
    else if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Less &&
        (parser_.current().lexeme() == "static_cast" ||
         parser_.current().lexeme() == "reinterpret_cast" ||
         parser_.current().lexeme() == "const_cast"))
        delegated = parser_.parseNamedCastExpression();
    else if (parser_.at(TokenKind::LeftParen) && parser_.looksLikeParenthesizedType())
        delegated = parser_.parseParenthesizedCastExpression();
    else if (parser_.at(TokenKind::KwOffsetof)) delegated = parser_.parseOffsetofExpression();
    else if (parser_.at(TokenKind::KwSizeof) || parser_.at(TokenKind::KwAlignof))
        delegated = parser_.parseSizeofOrAlignofExpression();
    else if (parser_.at(TokenKind::KwDefault)) delegated = parser_.parseDefaultValueExpression();
    else if (parser_.at(TokenKind::KwNew)) delegated = parser_.parseNewExpression();
    else if (parser_.at(TokenKind::KwDelete)) delegated = parser_.parseDeleteExpression();
    else if (parser_.at(TokenKind::KwDestroy)) delegated = parser_.parseDestroyExpression();
    else if (parser_.at(TokenKind::KwStackalloc)) delegated = parser_.parseStackallocExpression();
    if (delegated != nullptr) { complete(work.resultSlot, delegated); return; }
    work_.push_back({WorkKind::Postfix, work.resultSlot});
}

void Parser::PostfixIterationMachine::finishUnary(Work work)
{
    ast::AstNode* operand = results_[work.childSlot].value;
    ast::AstNodeKind kind = work.flag
        ? ast::AstNodeKind::AwaitExpression : ast::AstNodeKind::UnaryExpression;
    if (work.tokenKind == TokenKind::Caret) kind = ast::AstNodeKind::FromEndExpression;
    const SourceRange range = work.flag ? work.range :
        parser_.join(work.range, operand != nullptr ? operand->range() : work.range);
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(kind, range);
    if (!work.flag) node->setTokenKind(work.tokenKind);
    node->setFirst(operand);
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::finishBinary(Work work)
{
    ast::AstNode* right = results_[work.childSlot].value;
    const ast::AstNodeKind kind = work.tokenKind == TokenKind::QuestionQuestion
        ? ast::AstNodeKind::NullCoalescingExpression : ast::AstNodeKind::BinaryExpression;
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        kind, parser_.join(work.node->range(), right->range()));
    node->setTokenKind(work.tokenKind);
    node->setFirst(work.node);
    node->setSecond(right);
    Work tail{WorkKind::ExpressionTail, work.resultSlot, noSlot, node};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
}

void Parser::PostfixIterationMachine::finishAssignment(Work work)
{
    ast::AstNode* value = results_[work.childSlot].value;
    if (work.text == "lambda")
    {
        parser_.finishLambdaExpression(work.node, value, false);
        complete(work.resultSlot, work.node);
        return;
    }
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression, parser_.join(work.node->range(), value->range()));
    node->setTokenKind(work.tokenKind);
    node->setFirst(work.node);
    node->setSecond(value);
    node->setReference(work.flag);
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::finishConditionalTrue(Work work)
{
    parser_.expect(TokenKind::Colon, "':' in a conditional expression");
    Work finish{WorkKind::FinishConditionalFalse, work.resultSlot, makeSlot(), work.node,
        results_[work.childSlot].value};
    work_.push_back(finish);
    scheduleChild(finish.childSlot);
}

void Parser::PostfixIterationMachine::finishConditionalFalse(Work work)
{
    ast::AstNode* value = results_[work.childSlot].value;
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::ConditionalExpression,
        parser_.join(work.node->range(), value != nullptr ? value->range() : work.node->range()));
    node->setFirst(work.node);
    node->setSecond(work.first);
    node->setThird(value);
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::finishTypeRelation(Work work)
{
    ast::TypeSyntax* type = parser_.parseType();
    if (type == nullptr) { complete(work.resultSlot, work.node); return; }
    const ast::AstNodeKind kind = work.tokenKind == TokenKind::KwIs
        ? ast::AstNodeKind::TypeTestExpression : ast::AstNodeKind::SafeCastExpression;
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        kind, parser_.join(work.node->range(), type->range()));
    node->setTokenKind(work.tokenKind);
    node->setTypeSyntax(type);
    node->setFirst(work.node);
    Work tail{WorkKind::ExpressionTail, work.resultSlot, noSlot, node};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
}

void Parser::PostfixIterationMachine::finishRangeEnd(Work work)
{
    ast::AstNode* end = work.childSlot == noSlot ? nullptr : results_[work.childSlot].value;
    if (parser_.atContextualKeyword("step"))
    {
        const SourceRange stepKeyword = parser_.consume().range();
        if (rangeOperandEnds(parser_.current().kind()))
            parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
                diagnostics::DiagnosticSeverity::Error, stepKeyword,
                "expected an integral expression after range step");
        else
        {
            Work finish{WorkKind::FinishRangeStep, work.resultSlot, makeSlot(), nullptr, work.first};
            finish.node = end;
            finish.range = work.range;
            finish.tokenKind = work.tokenKind;
            finish.precedence = work.precedence;
            work_.push_back(finish);
            Work step{WorkKind::Expression, finish.childSlot};
            step.precedence = 5;
            work_.push_back(step);
            return;
        }
    }
    Work finish{WorkKind::FinishRangeStep, work.resultSlot, noSlot, end, work.first};
    finish.range = work.range;
    finish.tokenKind = work.tokenKind;
    finish.precedence = work.precedence;
    finishRangeStep(std::move(finish));
}

void Parser::PostfixIterationMachine::finishRangeStep(Work work)
{
    ast::AstNode* step = work.childSlot == noSlot ? nullptr : results_[work.childSlot].value;
    const bool inclusive = work.tokenKind == TokenKind::DotDotEqual;
    if (inclusive && work.node == nullptr)
        parser_.diagnostics_.report(diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error, work.range,
            "an inclusive range requires an end endpoint");
    ast::AstNode* range = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::RangeExpression,
        SourceRange(work.first != nullptr ? work.first->range().begin() : work.range.begin(),
            step != nullptr ? step->range().end() :
            (work.node != nullptr ? work.node->range().end() : work.range.end())));
    range->setTokenKind(work.tokenKind);
    range->setFirst(work.first);
    range->setSecond(work.node);
    range->setThird(step);
    range->setFlag(inclusive);
    validateConstantRange(range);
    Work tail{WorkKind::ExpressionTail, work.resultSlot, noSlot, range};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
}

void Parser::PostfixIterationMachine::validateConstantRange(ast::AstNode* range) const
{
    const std::optional<std::int64_t> begin = signedIntegerLiteral(range->first());
    const std::optional<std::int64_t> end = signedIntegerLiteral(range->second());
    const std::optional<std::int64_t> step = range->third() != nullptr
        ? signedIntegerLiteral(range->third()) : std::optional<std::int64_t>(1);
    if (!begin.has_value() || !end.has_value() || !step.has_value()) return;

    std::string error;
    if (language::lazyRangeCount({*begin, *end, *step, range->flag()}, error).has_value()) return;
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        range->range(),
        error);
}

void Parser::PostfixIterationMachine::schedulePrimaryArgument(
    Work work,
    bool tupleElement)
{
    work.childSlot = makeSlot();
    work.flag = false;
    work.text.clear();
    work.auxiliaryText.clear();
    work.tokenKind = TokenKind::Invalid;
    if (tupleElement && parser_.at(TokenKind::DotDot))
    {
        work.tokenKind = TokenKind::DotDot;
        work.range = parser_.consume().range();
    }
    else
    {
        if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Colon)
        {
            work.auxiliaryText = std::string(parser_.consume().lexeme());
            parser_.consume();
        }
        const bool reference = (parser_.atContextualKeyword("ref") ||
            parser_.atContextualKeyword("out") || parser_.at(TokenKind::KwIn)) &&
            parser_.peek().kind() != TokenKind::Comma &&
            parser_.peek().kind() != TokenKind::RightParen;
        if (reference)
        {
            const lexer::Token& keyword = parser_.consume();
            work.range = keyword.range();
            work.text = keyword.kind() == TokenKind::KwIn
                ? "in" : std::string(keyword.lexeme());
            work.flag = true;
        }
    }
    const std::size_t childSlot = work.childSlot;
    work_.push_back(std::move(work));
    scheduleChild(childSlot);
}

ast::AstNode* Parser::PostfixIterationMachine::finishPrimaryArgument(Work& work)
{
    ast::AstNode* argument = finishArgument(work);
    if (work.tokenKind != TokenKind::DotDot) return argument;
    ast::AstNode* spread = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SpreadElementExpression,
        parser_.join(work.range, argument != nullptr ? argument->range() : work.range));
    spread->setTokenKind(TokenKind::DotDot);
    spread->setFirst(argument);
    return spread;
}

void Parser::PostfixIterationMachine::continueGrouping(Work work)
{
    ast::AstNode* argument = finishPrimaryArgument(work);
    if (work.node == nullptr)
    {
        if (!parser_.consumeIf(TokenKind::Comma))
        {
            parser_.expect(TokenKind::RightParen, "')'");
            if (argument != nullptr)
                argument->setRange(SourceRange(
                    work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
            complete(work.resultSlot, argument);
            return;
        }
        work.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::TupleExpression, work.outerRange);
        if (argument != nullptr) work.node->addChild(argument);
        if (parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
        {
            parser_.expect(TokenKind::RightParen, "')' after tuple");
            work.node->setRange(SourceRange(
                work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
            complete(work.resultSlot, work.node);
            return;
        }
        schedulePrimaryArgument(std::move(work), true);
        return;
    }
    if (argument != nullptr) work.node->addChild(argument);
    const bool comma = parser_.consumeIf(TokenKind::Comma);
    if (!comma || parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightParen, "')' after tuple");
        work.node->setRange(SourceRange(
            work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, work.node);
        return;
    }
    schedulePrimaryArgument(std::move(work), true);
}

void Parser::PostfixIterationMachine::continueCollection(Work work)
{
    ast::AstNode* element = results_[work.childSlot].value;
    if (work.tokenKind == TokenKind::DotDot)
    {
        ast::AstNode* spread = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SpreadElementExpression,
            parser_.join(work.range, element != nullptr ? element->range() : work.range));
        spread->setTokenKind(TokenKind::DotDot);
        spread->setFirst(element);
        element = spread;
    }
    if (element != nullptr) work.node->addChild(element);
    if (!parser_.consumeIf(TokenKind::Comma) || parser_.at(TokenKind::RightBracket) ||
        parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBracket, "']' after collection expression");
        work.node->setRange(SourceRange(
            work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, work.node);
        return;
    }
    work.tokenKind = TokenKind::Invalid;
    if (parser_.at(TokenKind::DotDot))
    {
        work.tokenKind = TokenKind::DotDot;
        work.range = parser_.consume().range();
    }
    work.childSlot = makeSlot();
    const std::size_t childSlot = work.childSlot;
    work_.push_back(std::move(work));
    scheduleChild(childSlot);
}

void Parser::PostfixIterationMachine::finishNameOf(Work work)
{
    ast::AstNode* operand = results_[work.childSlot].value;
    const SourceRange end = parser_.current().range();
    parser_.expect(TokenKind::RightParen, "')' after 'nameof' argument");
    ast::AstNode* expression = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::NameOfExpression, parser_.join(work.range, end));
    expression->setFirst(operand);
    const ast::AstNode* named = operand;
    while (named != nullptr && (named->kind() == ast::AstNodeKind::CallExpression ||
        named->kind() == ast::AstNodeKind::IndexExpression)) named = named->first();
    if (named != nullptr && (named->kind() == ast::AstNodeKind::IdentifierExpression ||
        named->kind() == ast::AstNodeKind::MemberExpression ||
        named->kind() == ast::AstNodeKind::PointerMemberExpression ||
        named->kind() == ast::AstNodeKind::NullConditionalMemberExpression))
    {
        expression->setText(named->text());
    }
    complete(work.resultSlot, expression);
}

void Parser::PostfixIterationMachine::finishOverflowPolicy(Work work)
{
    ast::AstNode* operand = results_[work.childSlot].value;
    parser_.expect(TokenKind::RightParen, "')' after '" + work.text + "' operand");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::OverflowPolicyExpression,
        parser_.join(work.range, parser_.tokens_[parser_.index_ - 1U].range()));
    node->setFirst(operand);
    node->setAuxiliaryText(std::move(work.text));
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::enterInterpolationFragment(InterpolationFrame& frame)
{
    const language::InterpolationPart& part = frame.parts[frame.partIndex];
    const lexer::Token& token = *frame.token;
    const std::uint32_t tokenOffset = token.range().begin().offset();
    const std::uint64_t requestedOffset =
        static_cast<std::uint64_t>(tokenOffset) + 2U + part.expressionOffset;
    const std::size_t prefix = requestedOffset <=
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())
        ? static_cast<std::size_t>(requestedOffset) : static_cast<std::size_t>(tokenOffset);
    std::string fragment(prefix, ' ');
    fragment += part.text;
    frame.fragmentSource = std::make_unique<source::SourceFile>(
        token.range().begin().fileId(),
        frame.runtime ? "<runtime-interpolation>" : "<interpolation-hole>",
        std::move(fragment));
    lexer::Lexer lexer(*frame.fragmentSource, parser_.diagnostics_);
    frame.fragmentTokens = lexer.lexAll();
    frame.outerTokens = parser_.tokens_;
    frame.outerIndex = parser_.index_;
    frame.outerQuestionIndex = std::move(parser_.conditionalQuestionIndex_);
    frame.outerPendingGenericGreater = parser_.pendingGenericGreater_;
    parser_.tokens_ = std::span<const lexer::Token>(frame.fragmentTokens);
    parser_.index_ = 0;
    parser_.pendingGenericGreater_ = 0;
    parser_.buildConditionalQuestionIndex();
}

void Parser::PostfixIterationMachine::restoreInterpolationParser(InterpolationFrame& frame)
{
    parser_.tokens_ = frame.outerTokens;
    parser_.index_ = frame.outerIndex;
    parser_.conditionalQuestionIndex_ = std::move(frame.outerQuestionIndex);
    parser_.pendingGenericGreater_ = frame.outerPendingGenericGreater;
    frame.fragmentTokens.clear();
    frame.fragmentSource.reset();
}

void Parser::PostfixIterationMachine::scheduleInterpolation(
    std::size_t frameIndex,
    std::size_t resultSlot)
{
    InterpolationFrame& frame = interpolationFrames_[frameIndex];
    while (frame.partIndex < frame.parts.size())
    {
        const language::InterpolationPart& part = frame.parts[frame.partIndex];
        if (part.kind == language::InterpolationPartKind::Hole)
        {
            Work finish{WorkKind::ContinueInterpolation, resultSlot, makeSlot()};
            finish.frameIndex = frameIndex;
            const std::size_t childSlot = finish.childSlot;
            enterInterpolationFragment(frame);
            work_.push_back(std::move(finish));
            scheduleChild(childSlot);
            return;
        }
        if (frame.runtime && !part.text.empty())
        {
            ast::AstNode* piece = parser_.makeInterpolationStringLiteral(*frame.token, part.text);
            frame.runtimeResult = parser_.makeBinaryExpression(
                TokenKind::Plus, frame.runtimeResult, piece);
            frame.runtimeResult->setRange(frame.token->range());
        }
        ++frame.partIndex;
    }
    complete(resultSlot, frame.runtime ? frame.runtimeResult : frame.staticNode);
}

void Parser::PostfixIterationMachine::finishInterpolationHole(Work work)
{
    InterpolationFrame& frame = interpolationFrames_[work.frameIndex];
    ast::AstNode* hole = results_[work.childSlot].value;
    const bool hasTrailingTokens = !parser_.at(TokenKind::EndOfFile);
    restoreInterpolationParser(frame);
    if (frame.runtime && hasTrailingTokens)
        parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error, frame.token->range(),
            "interpolation hole must contain exactly one expression");
    if (hole == nullptr) hole = parser_.makeErrorExpression();
    if (frame.runtime)
    {
        ast::AstNode* piece = parser_.formatRuntimeInterpolationHole(
            *frame.token, frame.parts[frame.partIndex], hole);
        frame.runtimeResult = parser_.makeBinaryExpression(
            TokenKind::Plus, frame.runtimeResult, piece);
        frame.runtimeResult->setRange(frame.token->range());
    }
    else frame.staticNode->addChild(hole);
    ++frame.partIndex;
    scheduleInterpolation(work.frameIndex, work.resultSlot);
}

void Parser::PostfixIterationMachine::startInterpolationToken(
    Work work,
    const lexer::Token& token)
{
    std::string value;
    if (const auto* decoded = std::get_if<std::string>(&token.value())) value = *decoded;
    parser_.consume();
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::InterpolatedStringExpression, token.range());
    node->setTokenKind(token.kind());
    node->setText(value);
    interpolationFrames_.emplace_back();
    const std::size_t frameIndex = interpolationFrames_.size() - 1U;
    InterpolationFrame& frame = interpolationFrames_.back();
    frame.token = token;
    frame.staticNode = node;
    std::string error;
    if (!language::parseStaticInterpolation(value, frame.parts, error))
    {
        parser_.diagnostics_.report(diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error, token.range(), std::move(error));
        node->setIntegerValue(0U);
        complete(work.resultSlot, node);
        return;
    }
    node->setIntegerValue(frame.parts.size());
    frame.runtime = !std::ranges::all_of(frame.parts, isStaticInterpolationHole);
    if (frame.runtime)
        frame.runtimeResult = parser_.makeInterpolationStringLiteral(token, "");
    scheduleInterpolation(frameIndex, work.resultSlot);
}

bool Parser::PostfixIterationMachine::startReservedPrimary(Work& work)
{
    if (parser_.startsParenthesizedBlock("match"))
        { complete(work.resultSlot, parser_.parseMatchExpression()); return true; }
    if (parser_.atContextualKeyword("loop") &&
        (parser_.peek().kind() == TokenKind::LeftBrace ||
         (parser_.peek().kind() == TokenKind::Identifier &&
          parser_.peek(2).kind() == TokenKind::LeftBrace)))
        { complete(work.resultSlot, parser_.parseLoopExpression()); return true; }
    if (parser_.comprehensionExpressionStartsHere())
        { complete(work.resultSlot, parser_.parseComprehensionExpression()); return true; }
    return false;
}

bool Parser::PostfixIterationMachine::startKeywordPrimary(Work& work)
{
    if (parser_.atContextualKeyword("nameof") && parser_.peek().kind() == TokenKind::LeftParen)
    {
        if (parser_.nameOfHasQualifiedGenericOperand())
            { complete(work.resultSlot, parser_.parseNameOfExpression()); return true; }
        work.range = parser_.consume().range();
        parser_.expect(TokenKind::LeftParen, "'(' after 'nameof'");
        work.kind = WorkKind::FinishNameOf;
        work.childSlot = makeSlot();
        const std::size_t childSlot = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(childSlot); return true;
    }
    if ((parser_.atContextualKeyword("checked") || parser_.atContextualKeyword("saturating") ||
         parser_.atContextualKeyword("unchecked") || parser_.atContextualKeyword("wrapping")) &&
        parser_.peek().kind() == TokenKind::LeftParen)
    {
        const lexer::Token& policy = parser_.consume();
        work.kind = WorkKind::FinishOverflowPolicy;
        work.range = policy.range(); work.text = std::string(policy.lexeme());
        parser_.expect(TokenKind::LeftParen, "'(' after '" + work.text + "'");
        work.childSlot = makeSlot(); const std::size_t childSlot = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(childSlot); return true;
    }
    return false;
}

bool Parser::PostfixIterationMachine::startAggregatePrimary(Work& work)
{
    if (parser_.at(TokenKind::LeftBracket))
    {
        work.kind = WorkKind::Collection; work.outerRange = parser_.consume().range();
        work.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::CollectionExpression, work.outerRange);
        work.node->setAuxiliaryText("inferred-array-initializer");
        if (parser_.at(TokenKind::RightBracket))
        {
            parser_.consume(); work.node->setRange(parser_.join(
                work.outerRange, parser_.tokens_[parser_.index_ - 1U].range()));
            complete(work.resultSlot, work.node); return true;
        }
        work.tokenKind = parser_.at(TokenKind::DotDot) ? parser_.consume().kind() : TokenKind::Invalid;
        if (work.tokenKind == TokenKind::DotDot)
            work.range = parser_.tokens_[parser_.index_ - 1U].range();
        work.childSlot = makeSlot(); const std::size_t childSlot = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(childSlot); return true;
    }
    if (parser_.at(TokenKind::LeftParen))
    {
        work.kind = WorkKind::Grouping; work.outerRange = parser_.consume().range();
        if (parser_.at(TokenKind::RightParen))
        {
            ast::AstNode* tuple = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::TupleExpression,
                parser_.join(work.outerRange, parser_.consume().range()));
            complete(work.resultSlot, tuple); return true;
        }
        schedulePrimaryArgument(std::move(work), false); return true;
    }
    return false;
}

void Parser::PostfixIterationMachine::startPrimary(Work work)
{
    if (startReservedPrimary(work) || startKeywordPrimary(work) || startAggregatePrimary(work))
        return;
    const lexer::Token& token = parser_.current();
    const auto kind = parser_.classifyPrimaryToken(token.kind());
    if (kind == ast::AstNodeKind::InterpolatedStringExpression)
        { startInterpolationToken(std::move(work), token); return; }
    if (kind.has_value())
        { complete(work.resultSlot, parser_.makeTokenPrimaryExpression(token, *kind)); return; }
    parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
        diagnostics::DiagnosticSeverity::Error, token.range(), "expected expression");
    if (!parser_.at(TokenKind::EndOfFile)) parser_.consume();
    complete(work.resultSlot, parser_.makeErrorExpression());
}

void Parser::PostfixIterationMachine::startPostfix(Work work)
{
    if (work.node == nullptr && work.childSlot == noSlot &&
        !parser_.looksLikeClosedGenericTypeReference())
    {
        work.childSlot = makeSlot();
        const std::size_t primarySlot = work.childSlot;
        work_.push_back(std::move(work));
        work_.push_back({WorkKind::Primary, primarySlot});
        return;
    }
    ast::AstNode* expression = work.node;
    if (expression == nullptr && work.childSlot != noSlot)
        expression = results_[work.childSlot].value;
    if (expression == nullptr) expression = parser_.parsePostfixBaseExpression();
    if (expression == nullptr)
    {
        work.childSlot = makeSlot();
        const std::size_t primarySlot = work.childSlot;
        work_.push_back(std::move(work));
        work_.push_back({WorkKind::Primary, primarySlot});
        return;
    }
    std::vector<ast::TypeSyntax*> genericArguments;
    const bool explicitGenerics = parser_.looksLikeGenericCallTypeArguments();
    if (explicitGenerics) parser_.parseTypeArgumentList(genericArguments);
    if (parser_.consumeIf(TokenKind::LeftParen))
    {
        ast::AstNode* call = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::CallExpression, expression->range());
        call->setFirst(expression);
        call->setFlag(explicitGenerics);
        for (ast::TypeSyntax* type : genericArguments) call->addTypeSyntax(type);
        work_.push_back({WorkKind::Call, work.resultSlot, noSlot, call});
        return;
    }
    if (!genericArguments.empty())
    {
        parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error, parser_.current().range(),
            "generic type arguments in an expression must be followed by a call in Stage 44");
        complete(work.resultSlot, expression);
        return;
    }
    work.node = expression;
    if (parser_.consumeIf(TokenKind::LeftBracket))
        work_.push_back(Work{WorkKind::Index, work.resultSlot, noSlot, expression});
    else if (parser_.at(TokenKind::QuestionBracket))
    {
        parser_.consume();
        Work index{WorkKind::Index, work.resultSlot, noSlot, expression};
        index.flag = true;
        work_.push_back(std::move(index));
    }
    else if (parser_.at(TokenKind::QuestionDot))
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parseNullConditionalMemberSuffix(expression)});
    else if (parser_.at(TokenKind::Dot) || parser_.at(TokenKind::Arrow))
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parseMemberSuffix(expression)});
    else if (parser_.at(TokenKind::PlusPlus) || parser_.at(TokenKind::MinusMinus))
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parsePostfixOperatorSuffix(expression)});
    else if (parser_.at(TokenKind::Question) && !parser_.questionBeginsConditional())
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parseResultPropagationSuffix(expression)});
    else if (parser_.atContextualKeyword("with"))
    {
        const SourceRange keyword = parser_.consume().range();
        ast::AstNode* with = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::WithExpression, parser_.join(expression->range(), keyword));
        with->setFirst(expression);
        parser_.expect(TokenKind::LeftBrace, "'{' after with");
        work_.push_back({WorkKind::With, work.resultSlot, noSlot, with});
    }
    else complete(work.resultSlot, expression);
}

ast::AstNode* Parser::PostfixIterationMachine::finishArgument(Work& work)
{
    ast::AstNode* argument = results_[work.childSlot].value;
    if (work.flag)
    {
        ast::AstNode* wrapper = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, work.range);
        wrapper->setTokenKind(TokenKind::Amp);
        wrapper->setFirst(argument);
        wrapper->setAuxiliaryText("pass:" + work.text);
        if (argument != nullptr) wrapper->setRange(parser_.join(work.range, argument->range()));
        argument = wrapper;
    }
    if (argument != nullptr && !work.auxiliaryText.empty())
        argument->setArgumentName(work.auxiliaryText);
    return argument;
}

void Parser::PostfixIterationMachine::finishCall(Work& work)
{
    parser_.expect(TokenKind::RightParen, "')'");
    work.node->setRange(SourceRange(work.node->first()->range().begin(),
        parser_.tokens_[parser_.index_ - 1].range().end()));
    ast::AstNode* callee = work.node->first();
    if (callee->kind() == ast::AstNodeKind::IdentifierExpression && callee->typeSyntax() == nullptr)
    {
        const std::string& name = callee->text();
        if (name == "assert" || name == "unreachable" || name == "assume" || name == "panic")
            work.node->setAuxiliaryText("builtin-" + name);
    }
    work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, work.node});
}

void Parser::PostfixIterationMachine::continueCall(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* argument = finishArgument(work);
        if (argument != nullptr) work.node->addChild(argument);
        work.childSlot = noSlot;
        work.text.clear();
        work.auxiliaryText.clear();
        work.flag = false;
        if (!parser_.consumeIf(TokenKind::Comma)) { finishCall(work); return; }
    }
    if (parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
    { finishCall(work); return; }
    if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Colon)
    {
        work.auxiliaryText = std::string(parser_.consume().lexeme());
        parser_.consume();
    }
    const bool reference = (parser_.atContextualKeyword("ref") || parser_.atContextualKeyword("out") ||
        parser_.at(TokenKind::KwIn)) && parser_.peek().kind() != TokenKind::Comma &&
        parser_.peek().kind() != TokenKind::RightParen;
    if (reference)
    {
        const lexer::Token& keyword = parser_.consume();
        work.range = keyword.range();
        work.text = keyword.kind() == TokenKind::KwIn ? "in" : std::string(keyword.lexeme());
        work.flag = true;
    }
    work.childSlot = makeSlot();
    work_.push_back(work);
    scheduleChild(work.childSlot);
}

void Parser::PostfixIterationMachine::appendIndex(Work& work, ast::AstNode* value)
{
    if (value != nullptr) work.children.push_back(value);
    work.childSlot = noSlot;
    work.first = nullptr;
}

void Parser::PostfixIterationMachine::finishIndex(Work& work)
{
    parser_.expect(TokenKind::RightBracket, "']'");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        work.flag ? ast::AstNodeKind::NullConditionalIndexExpression
                  : ast::AstNodeKind::IndexExpression,
        SourceRange(work.node->range().begin(), parser_.tokens_[parser_.index_ - 1].range().end()));
    if (work.flag) node->setTokenKind(TokenKind::QuestionBracket);
    node->setFirst(work.node);
    if (!work.children.empty()) node->setSecond(work.children.front());
    for (ast::AstNode* index : work.children) node->addChild(index);
    work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, node});
}

void Parser::PostfixIterationMachine::continueIndex(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* value = results_[work.childSlot].value;
        if (work.first == nullptr && parser_.at(TokenKind::DotDot))
        {
            work.first = value;
            work.range = parser_.consume().range();
            work.text = "range";
            if (!parser_.at(TokenKind::Comma) && !parser_.at(TokenKind::RightBracket))
            {
                work.childSlot = makeSlot();
                work_.push_back(work);
                scheduleChild(work.childSlot);
                return;
            }
            value = nullptr;
        }
        if (work.text == "range")
        {
            ast::AstNode* range = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::RangeExpression,
                SourceRange(work.first != nullptr ? work.first->range().begin() : work.range.begin(),
                    value != nullptr ? value->range().end() : work.range.end()));
            range->setTokenKind(TokenKind::DotDot);
            range->setFirst(work.first);
            range->setSecond(value);
            value = range;
            work.range = {};
            work.text.clear();
        }
        appendIndex(work, value);
        if (!parser_.consumeIf(TokenKind::Comma)) { finishIndex(work); return; }
    }
    if (parser_.at(TokenKind::RightBracket) || parser_.at(TokenKind::EndOfFile))
    { finishIndex(work); return; }
    if (parser_.at(TokenKind::DotDot))
    {
        work.range = parser_.consume().range();
        work.text = "range";
        if (parser_.at(TokenKind::Comma) || parser_.at(TokenKind::RightBracket))
        {
            work.childSlot = makeSlot();
            complete(work.childSlot, nullptr);
            work_.push_back(work);
            return;
        }
    }
    work.childSlot = makeSlot();
    work_.push_back(work);
    scheduleChild(work.childSlot);
}

void Parser::PostfixIterationMachine::continueWith(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* value = results_[work.childSlot].value;
        if (value != nullptr)
        {
            value->setArgumentName(work.text);
            work.node->addChild(value);
        }
        work.childSlot = noSlot;
        if (!parser_.consumeIf(TokenKind::Comma))
        {
            parser_.expect(TokenKind::RightBrace, "'}' after with expression");
            work.node->setRange(SourceRange(work.node->first()->range().begin(),
                parser_.tokens_[parser_.index_ - 1].range().end()));
            work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, work.node});
            return;
        }
    }
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBrace, "'}' after with expression");
        work.node->setRange(SourceRange(work.node->first()->range().begin(),
            parser_.tokens_[parser_.index_ - 1].range().end()));
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, work.node});
        return;
    }
    if (!parser_.at(TokenKind::Identifier))
    {
        parser_.expect(TokenKind::Identifier, "member name in with expression");
        complete(work.resultSlot, work.node);
        return;
    }
    work.text = std::string(parser_.consume().lexeme());
    parser_.expect(TokenKind::Equal, "'=' after with member name");
    work.childSlot = makeSlot();
    work_.push_back(work);
    scheduleChild(work.childSlot);
}

ast::AstNode* Parser::parseCallSuffix(
    ast::AstNode* expression,
    std::vector<ast::TypeSyntax*> genericArguments,
    bool explicitGenericArguments)
{
    ast::AstNode* call = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CallExpression, expression->range());
    call->setFirst(expression);
    call->setFlag(explicitGenericArguments);
    for (ast::TypeSyntax* argument : genericArguments) call->addTypeSyntax(argument);
    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        call->addChild(parseArgument());
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightParen, "')'");
    call->setRange(SourceRange(
        expression->range().begin(), tokens_[index_ - 1].range().end()));
    if (expression->kind() == ast::AstNodeKind::IdentifierExpression &&
        expression->typeSyntax() == nullptr)
    {
        const std::string& intrinsic = expression->text();
        if (intrinsic == "assert" || intrinsic == "unreachable" ||
            intrinsic == "assume" || intrinsic == "panic")
            call->setAuxiliaryText("builtin-" + intrinsic);
    }
    return call;
}

ast::AstNode* Parser::parseIndexArgument()
{
    ast::AstNode* start = nullptr;
    if (!at(TokenKind::DotDot)) start = parseAssignmentExpression();
    if (!at(TokenKind::DotDot)) return start;
    const SourceRange rangeToken = consume().range();
    ast::AstNode* end = nullptr;
    if (!at(TokenKind::Comma) && !at(TokenKind::RightBracket))
        end = parseAssignmentExpression();
    ast::AstNode* range = arena_.make<ast::AstNode>(
        ast::AstNodeKind::RangeExpression,
        SourceRange(
            start != nullptr ? start->range().begin() : rangeToken.begin(),
            end != nullptr ? end->range().end() : rangeToken.end()));
    range->setTokenKind(TokenKind::DotDot);
    range->setFirst(start);
    range->setSecond(end);
    return range;
}

ast::AstNode* Parser::parseIndexSuffix(ast::AstNode* expression, bool conditional)
{
    std::vector<ast::AstNode*> indices;
    while (!at(TokenKind::RightBracket) && !at(TokenKind::EndOfFile))
    {
        if (ast::AstNode* index = parseIndexArgument()) indices.push_back(index);
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightBracket, "']'");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        conditional ? ast::AstNodeKind::NullConditionalIndexExpression
                    : ast::AstNodeKind::IndexExpression,
        SourceRange(expression->range().begin(), tokens_[index_ - 1].range().end()));
    if (conditional) node->setTokenKind(TokenKind::QuestionBracket);
    node->setFirst(expression);
    if (!indices.empty()) node->setSecond(indices.front());
    for (ast::AstNode* index : indices) node->addChild(index);
    return node;
}

ast::AstNode* Parser::parseNullConditionalMemberSuffix(ast::AstNode* expression)
{
    consume();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "member name after '?.'");
        return expression;
    }
    const lexer::Token& member = consume();
    ast::AstNode* access = arena_.make<ast::AstNode>(
        ast::AstNodeKind::NullConditionalMemberExpression,
        SourceRange(expression->range().begin(), member.range().end()));
    access->setTokenKind(TokenKind::QuestionDot);
    access->setFirst(expression);
    access->setText(std::string(member.lexeme()));
    return access;
}

ast::AstNode* Parser::parseMemberSuffix(ast::AstNode* expression)
{
    const TokenKind operation = consume().kind();
    if (!at(TokenKind::Identifier))
    {
        expect(TokenKind::Identifier, "member name");
        return expression;
    }
    const lexer::Token& member = consume();
    const SourceRange access(expression->range().begin(), member.range().end());
    ast::AstNode* receiver = expression;
    if (operation == TokenKind::Arrow)
    {
        ast::AstNode* dereference = arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, access);
        dereference->setTokenKind(TokenKind::Star);
        dereference->setFirst(expression);
        dereference->setAuxiliaryText("arrow");
        receiver = dereference;
    }
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::MemberExpression, access);
    node->setFirst(receiver);
    node->setText(std::string(member.lexeme()));
    return node;
}

ast::AstNode* Parser::parsePostfixOperatorSuffix(ast::AstNode* expression)
{
    const lexer::Token& operation = consume();
    ast::AstNode* postfix = arena_.make<ast::AstNode>(
        ast::AstNodeKind::PostfixExpression,
        SourceRange(expression->range().begin(), operation.range().end()));
    postfix->setTokenKind(operation.kind());
    postfix->setFirst(expression);
    return postfix;
}

ast::AstNode* Parser::parseResultPropagationSuffix(ast::AstNode* expression)
{
    const lexer::Token& operation = consume();
    ast::AstNode* propagation = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ResultPropagationExpression,
        SourceRange(expression->range().begin(), operation.range().end()));
    propagation->setTokenKind(TokenKind::Question);
    propagation->setFirst(expression);
    return propagation;
}

ast::AstNode* Parser::parsePostfixExpression()
{
    return PostfixIterationMachine(*this).parse();
}

// Builds the immutable question-role cache from delimiter pairs, transitive crossing closures and
// depth-zero events. Total construction is O(T log T), space is O(T), and no recursion is used.
void Parser::buildConditionalQuestionIndex()
{
    std::vector<std::size_t> matches;
    std::vector<std::size_t> positions;
    std::vector<std::size_t> ordinals;
    pairConditionalDelimiters(tokens_, matches, positions, ordinals);
    std::vector<std::size_t> closureEnds;
    buildConditionalScopeClosures(matches, positions, closureEnds);
    std::vector<std::size_t> nextEvents;
    buildConditionalNextEvents(tokens_, ordinals, closureEnds, nextEvents);
    classifyConditionalQuestions(tokens_, nextEvents, conditionalQuestionIndex_);
}

// Returns the preclassified role of the question token at the current cursor in O(1). The index
// is immutable after construction because the token span itself is immutable for the parser.
bool Parser::questionBeginsConditional() const noexcept
{
    return at(TokenKind::Question) && index_ < conditionalQuestionIndex_.size() &&
        conditionalQuestionIndex_[index_] != std::uint8_t{0};
}

ast::AstNode* Parser::parseWithExpression(ast::AstNode* source)
{
    const SourceRange keyword = consume().range();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::WithExpression, join(source->range(), keyword));
    node->setFirst(source);
    if (!expect(TokenKind::LeftBrace, "'{' after with")) return node;
    while (!at(TokenKind::RightBrace) && !at(TokenKind::EndOfFile))
    {
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "member name in with expression");
            break;
        }
        const std::string name(current().lexeme());
        consume();
        expect(TokenKind::Equal, "'=' after with member name");
        ast::AstNode* value = parseAssignmentExpression();
        if (value != nullptr)
        {
            value->setArgumentName(name);
            node->addChild(value);
        }
        if (!consumeIf(TokenKind::Comma)) break;
        if (at(TokenKind::RightBrace)) break;
    }
    expect(TokenKind::RightBrace, "'}' after with expression");
    node->setRange(SourceRange(source->range().begin(), tokens_[index_ - 1].range().end()));
    return node;
}

ast::AstNode* Parser::parseCollectionExpression()
{
    // NS304-T09-F05: `[a, b, c]` — los elementos se bajan como children en orden. El spread
    // (`..x`) y el builder protocol quedan fuera de esta fundación léxica/sintáctica.
    const SourceRange begin = consume().range(); // '['
    ast::AstNode* collection = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CollectionExpression, begin);
    // La cuenta se infiere de los elementos (igual que `new T[] { ... }`), que es la ruta que
    // Sema/HIR ya saben bajar sin contar un `first()`.
    collection->setAuxiliaryText("inferred-array-initializer");
    if (!at(TokenKind::RightBracket))
    {
        while (true)
        {
            if (at(TokenKind::DotDot))
            {
                // NS304-T09-F05: `..x` — spread element (igual que en aggregate initializers).
                const SourceRange spread = consume().range();
                ast::AstNode* value = parseAssignmentExpression();
                ast::AstNode* element = arena_.make<ast::AstNode>(
                    ast::AstNodeKind::SpreadElementExpression,
                    join(spread, value != nullptr ? value->range() : spread));
                element->setTokenKind(TokenKind::DotDot);
                element->setFirst(value);
                collection->addChild(element);
            }
            else
            {
                ast::AstNode* element = parseAssignmentExpression();
                if (element != nullptr) collection->addChild(element);
            }
            if (!consumeIf(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RightBracket, "']' after collection expression");
    collection->setRange(SourceRange(begin.begin(), tokens_[index_ - 1].range().end()));
    return collection;
}

bool Parser::nameOfHasQualifiedGenericOperand() const noexcept
{
    if (!atContextualKeyword("nameof") || peek().kind() != TokenKind::LeftParen)
        return false;
    std::size_t cursor = index_ + 2U;
    while (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier)
    {
        ++cursor;
        if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Dot)
        {
            ++cursor;
            continue;
        }
        break;
    }
    return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Less;
}

ast::AstNode* Parser::parseNameOfExpression()
{
    // Keep the source operand until semantic lookup proves that the selected symbol exists and
    // is accessible. The constant string is produced only after that validation.
    const SourceRange begin = current().range();
    consume(); // `nameof`
    expect(TokenKind::LeftParen, "'(' after 'nameof'");

    ast::AstNode* operand = nullptr;
    std::vector<ast::TypeSyntax*> genericArguments;
    ast::TypeSyntax* genericTarget = nullptr;
    // A generic name without a call cannot use the ordinary postfix parser: that parser rightly
    // requires `<...>` to be followed by `(`. Keep the same identifier/member AST shape here and
    // preserve the complete candidate type plus its explicit arguments on NameOfExpression;
    // Sema decides whether the name denotes a type or a method and validates its arity.
    const auto qualifiedGenericType = [&]()
    {
        std::size_t cursor = index_;
        while (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Identifier)
        {
            ++cursor;
            if (cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Dot)
            {
                ++cursor;
                continue;
            }
            break;
        }
        return cursor < tokens_.size() && tokens_[cursor].kind() == TokenKind::Less;
    };
    if (at(TokenKind::Identifier) && (peek().kind() == TokenKind::Less || qualifiedGenericType()))
    {
        const lexer::Token& identifier = consume();
        SourceRange genericRange = identifier.range();
        std::string qualifiedName(identifier.lexeme());
        operand = arena_.make<ast::AstNode>(
            ast::AstNodeKind::IdentifierExpression, identifier.range());
        operand->setText(std::string(identifier.lexeme()));
        while (consumeIf(TokenKind::Dot))
        {
            const lexer::Token& member = current();
            expect(TokenKind::Identifier, "member name in 'nameof' argument");
            ast::AstNode* access = arena_.make<ast::AstNode>(
                ast::AstNodeKind::MemberExpression,
                SourceRange(operand->range().begin(), member.range().end()));
            access->setFirst(operand);
            access->setText(std::string(member.lexeme()));
            operand = access;
            genericRange = join(genericRange, member.range());
            qualifiedName += ".";
            qualifiedName += member.lexeme();
        }
        parseTypeArgumentList(genericArguments);
        genericTarget = arena_.make<ast::TypeSyntax>(
            genericRange, TokenKind::Identifier, std::move(qualifiedName));
        for (ast::TypeSyntax* argument : genericArguments)
        {
            genericTarget->addTypeArgument(argument);
        }
    }
    else
    {
        operand = parseExpression();
    }

    const SourceRange end = current().range();
    expect(TokenKind::RightParen, "')' after 'nameof' argument");

    ast::AstNode* expression = arena_.make<ast::AstNode>(
        ast::AstNodeKind::NameOfExpression, join(begin, end));
    expression->setFirst(operand);
    expression->setTypeSyntax(genericTarget);
    for (ast::TypeSyntax* argument : genericArguments)
    {
        expression->addTypeSyntax(argument);
    }

    const ast::AstNode* named = operand;
    while (named != nullptr &&
           (named->kind() == ast::AstNodeKind::CallExpression ||
            named->kind() == ast::AstNodeKind::IndexExpression))
    {
        named = named->first();
    }
    if (named != nullptr &&
        (named->kind() == ast::AstNodeKind::IdentifierExpression ||
         named->kind() == ast::AstNodeKind::MemberExpression ||
         named->kind() == ast::AstNodeKind::PointerMemberExpression ||
         named->kind() == ast::AstNodeKind::NullConditionalMemberExpression))
    {
        expression->setText(named->text());
    }
    return expression;
}

// NS304-T15-F05: `checked(expr)` / `saturating(expr)`. The policy is preserved on the node's
// auxiliaryText; the wrapped expression is the single child.
ast::AstNode* Parser::parseOverflowPolicyExpression()
{
    const lexer::Token& policyToken = consume();
    const std::string policy = std::string(policyToken.lexeme());
    expect(TokenKind::LeftParen, "'(' after '" + policy + "'");
    ast::AstNode* operand = parseExpression();
    expect(TokenKind::RightParen, "')' after '" + policy + "' operand");
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::OverflowPolicyExpression,
        join(policyToken.range(), tokens_[index_ - 1U].range()));
    node->setFirst(operand);
    node->setAuxiliaryText(policy);
    return node;
}

bool Parser::comprehensionExpressionStartsHere() const noexcept
{
    if (!atContextualKeyword("comprehend") || peek().kind() != TokenKind::LeftParen)
        return false;
    std::size_t depth = 0;
    for (std::size_t cursor = index_ + 1U; cursor < tokens_.size(); ++cursor)
    {
        if (tokens_[cursor].kind() == TokenKind::LeftParen) ++depth;
        else if (tokens_[cursor].kind() == TokenKind::RightParen && --depth == 0)
            return cursor + 1U < tokens_.size() &&
                tokens_[cursor + 1U].kind() == TokenKind::LeftBrace;
    }
    return false;
}

std::size_t Parser::pureGroupingDepth() const noexcept
{
    std::size_t groupingDepth = 1;
    while (index_ + groupingDepth < tokens_.size() &&
           tokens_[index_ + groupingDepth].kind() == TokenKind::LeftParen)
        ++groupingDepth;
    if (groupingDepth == 1 || index_ + groupingDepth >= tokens_.size() ||
        tokens_[index_ + groupingDepth].kind() == TokenKind::RightParen)
        return 0;
    std::size_t parentheses = 0;
    std::size_t brackets = 0;
    std::size_t braces = 0;
    std::size_t closing = tokens_.size();
    for (std::size_t cursor = index_ + groupingDepth; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (kind == TokenKind::LeftParen) ++parentheses;
        else if (kind == TokenKind::RightParen)
        {
            if (parentheses != 0) --parentheses;
            else { closing = cursor; break; }
        }
        else if (kind == TokenKind::LeftBracket) ++brackets;
        else if (kind == TokenKind::RightBracket && brackets != 0) --brackets;
        else if (kind == TokenKind::LeftBrace) ++braces;
        else if (kind == TokenKind::RightBrace && braces != 0) --braces;
        else if (kind == TokenKind::Comma && parentheses == 0 && brackets == 0 && braces == 0)
            return 0;
    }
    if (closing == tokens_.size()) return 0;
    for (std::size_t offset = 1; offset < groupingDepth; ++offset)
        if (closing + offset >= tokens_.size() ||
            tokens_[closing + offset].kind() != TokenKind::RightParen)
            return 0;
    return groupingDepth;
}

ast::AstNode* Parser::parseTupleElement()
{
    if (!at(TokenKind::DotDot)) return parseArgument();
    const SourceRange spread = consume().range();
    ast::AstNode* value = parseAssignmentExpression();
    ast::AstNode* element = arena_.make<ast::AstNode>(
        ast::AstNodeKind::SpreadElementExpression,
        join(spread, value != nullptr ? value->range() : spread));
    element->setTokenKind(TokenKind::DotDot);
    element->setFirst(value);
    return element;
}

ast::AstNode* Parser::parseParenthesizedPrimaryExpression()
{
    const lexer::Token& token = current();
    const std::size_t groupingDepth = pureGroupingDepth();
    if (groupingDepth != 0)
    {
        const SourceRange open = current().range();
        for (std::size_t depth = 0; depth < groupingDepth; ++depth) consume();
        ast::AstNode* expression = parseArgument();
        SourceRange close = current().range();
        for (std::size_t depth = 0; depth < groupingDepth; ++depth)
        {
            close = current().range();
            expect(TokenKind::RightParen, "')'");
        }
        if (expression != nullptr) expression->setRange(SourceRange(open.begin(), close.end()));
        return expression;
    }
    const SourceRange open = consume().range();
    if (at(TokenKind::RightParen))
        return arena_.make<ast::AstNode>(
            ast::AstNodeKind::TupleExpression, join(open, consume().range()));
    ast::AstNode* expression = parseArgument();
    if (!consumeIf(TokenKind::Comma))
    {
        expect(TokenKind::RightParen, "')'");
        if (expression != nullptr)
            expression->setRange(SourceRange(
                token.range().begin(), tokens_[index_ - 1].range().end()));
        return expression;
    }
    ast::AstNode* tuple = arena_.make<ast::AstNode>(
        ast::AstNodeKind::TupleExpression, open);
    tuple->addChild(expression);
    while (!at(TokenKind::RightParen) && !at(TokenKind::EndOfFile))
    {
        if (ast::AstNode* element = parseTupleElement()) tuple->addChild(element);
        if (!consumeIf(TokenKind::Comma)) break;
    }
    expect(TokenKind::RightParen, "')' after tuple");
    tuple->setRange(SourceRange(open.begin(), tokens_[index_ - 1].range().end()));
    return tuple;
}

std::optional<ast::AstNodeKind> Parser::classifyPrimaryToken(TokenKind kind) const noexcept
{
    switch (kind)
    {
        case TokenKind::Identifier: return ast::AstNodeKind::IdentifierExpression;
        case TokenKind::IntegerLiteral: return ast::AstNodeKind::IntegerLiteralExpression;
        case TokenKind::FloatingLiteral: return ast::AstNodeKind::FloatingLiteralExpression;
        case TokenKind::CharacterLiteral: return ast::AstNodeKind::CharacterLiteralExpression;
        case TokenKind::StringLiteral:
        case TokenKind::Utf8StringLiteral: return ast::AstNodeKind::StringLiteralExpression;
        case TokenKind::InterpolatedStringLiteral:
            return ast::AstNodeKind::InterpolatedStringExpression;
        case TokenKind::KwTrue:
        case TokenKind::KwFalse: return ast::AstNodeKind::BooleanLiteralExpression;
        case TokenKind::KwNull: return ast::AstNodeKind::NullLiteralExpression;
        case TokenKind::KwThis:
            return parsingExtensionBody_ ? ast::AstNodeKind::IdentifierExpression
                                         : ast::AstNodeKind::ThisExpression;
        case TokenKind::KwBase: return ast::AstNodeKind::BaseExpression;
        default: return std::nullopt;
    }
}

ast::AstNode* Parser::makeInterpolationStringLiteral(
    const lexer::Token& token,
    std::string value)
{
    ast::AstNode* literal = arena_.make<ast::AstNode>(
        ast::AstNodeKind::StringLiteralExpression, token.range());
    literal->setTokenKind(TokenKind::StringLiteral);
    literal->setText(std::move(value));
    return literal;
}

ast::AstNode* Parser::makeInterpolationBooleanLiteral(
    const lexer::Token& token,
    bool value)
{
    ast::AstNode* literal = arena_.make<ast::AstNode>(
        ast::AstNodeKind::BooleanLiteralExpression, token.range());
    literal->setTokenKind(value ? TokenKind::KwTrue : TokenKind::KwFalse);
    literal->setIntegerValue(value ? 1U : 0U);
    return literal;
}

ast::AstNode* Parser::makeInterpolationCall(
    const lexer::Token& token,
    std::string method,
    std::vector<ast::AstNode*> arguments)
{
    ast::AstNode* receiver = makeIdentifierExpression(token.range(), "String");
    ast::AstNode* member = makeMemberExpression(receiver, std::move(method));
    ast::AstNode* call = arena_.make<ast::AstNode>(
        ast::AstNodeKind::CallExpression, token.range());
    call->setFirst(member);
    call->setAuxiliaryText("runtime-interpolation");
    for (ast::AstNode* argument : arguments) call->addChild(argument);
    return call;
}

std::optional<std::uint64_t> Parser::parseInterpolationWidth(std::string_view format) const noexcept
{
    std::uint64_t value = 0;
    for (std::size_t index = 1; index < format.size(); ++index)
    {
        const unsigned char byte = static_cast<unsigned char>(format[index]);
        if (!std::isdigit(byte)) return std::nullopt;
        value = value * 10U + static_cast<std::uint64_t>(byte - '0');
        if (value > 64U) return std::nullopt;
    }
    return value;
}

ast::AstNode* Parser::parseInterpolationFragment(
    const lexer::Token& token,
    const language::InterpolationPart& part,
    std::string_view label,
    const std::vector<std::string>& knownTypes,
    bool requireEnd)
{
    const std::uint32_t tokenOffset = token.range().begin().offset();
    const std::uint64_t requestedOffset =
        static_cast<std::uint64_t>(tokenOffset) + 2U + part.expressionOffset;
    const std::size_t prefix = requestedOffset <=
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())
        ? static_cast<std::size_t>(requestedOffset) : static_cast<std::size_t>(tokenOffset);
    std::string fragment(prefix, ' ');
    fragment += part.text;
    source::SourceFile fragmentSource(
        token.range().begin().fileId(), std::string(label), std::move(fragment));
    lexer::Lexer fragmentLexer(fragmentSource, diagnostics_);
    const std::vector<lexer::Token> fragmentTokens = fragmentLexer.lexAll();
    Parser fragmentParser(fragmentTokens, arena_, diagnostics_, knownTypes);
    ast::AstNode* hole = fragmentParser.parseExpression();
    if (requireEnd && !fragmentParser.at(TokenKind::EndOfFile))
        diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error,
            token.range(),
            "interpolation hole must contain exactly one expression");
    return hole != nullptr ? hole : makeErrorExpression();
}

void Parser::appendInterpolationHoles(
    ast::AstNode* node,
    const lexer::Token& token,
    const std::vector<language::InterpolationPart>& parts)
{
    std::vector<std::string> knownTypes;
    knownTypes.reserve(knownTypeNames_.size());
    for (const std::string& name : knownTypeNames_) knownTypes.push_back(name);
    for (const language::InterpolationPart& part : parts)
    {
        if (part.kind != language::InterpolationPartKind::Hole) continue;
        node->addChild(parseInterpolationFragment(
            token, part, "<interpolation-hole>", knownTypes, false));
    }
}

ast::AstNode* Parser::formatRuntimeInterpolationHole(
    const lexer::Token& token,
    const language::InterpolationPart& part,
    ast::AstNode* hole)
{
    if (part.format.empty()) return makeInterpolationCall(token, "ToString", {hole});
    const char mode = part.format.front();
    const std::optional<std::uint64_t> width = parseInterpolationWidth(part.format);
    const bool validMode = mode == 'D' || mode == 'd' || mode == 'X' || mode == 'x' ||
        mode == 'F' || mode == 'f' || mode == 'G' || mode == 'g';
    if (!width.has_value() || !validMode ||
        ((mode == 'G' || mode == 'g') && part.format.size() != 1))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error,
            token.range(),
            "runtime interpolation format must be D or X with optional width, F with optional "
            "precision, or G without precision; numeric arguments are limited to 64");
        return makeInterpolationCall(token, "ToString", {hole});
    }
    if (mode == 'F' || mode == 'f')
    {
        const std::uint64_t precision = part.format.size() == 1 ? 2U : *width;
        return makeInterpolationCall(token, "ToStringFixed", {
            hole, makeIntegerLiteralExpression(token.range(), precision)});
    }
    if (mode == 'G' || mode == 'g') return makeInterpolationCall(token, "ToString", {hole});
    if (mode == 'X' || mode == 'x')
        return makeInterpolationCall(token, "ToHexString", {
            hole, makeIntegerLiteralExpression(token.range(), *width),
            makeInterpolationBooleanLiteral(token, mode == 'X')});
    ast::AstNode* piece = makeInterpolationCall(token, "ToString", {hole});
    if (*width != 0)
        piece = makeInterpolationCall(token, "ZFill", {
            piece, makeIntegerLiteralExpression(token.range(), *width)});
    return piece;
}

ast::AstNode* Parser::lowerRuntimeInterpolation(
    const lexer::Token& token,
    const std::vector<language::InterpolationPart>& parts)
{
    std::vector<std::string> knownTypes;
    knownTypes.reserve(knownTypeNames_.size());
    for (const std::string& name : knownTypeNames_) knownTypes.push_back(name);
    ast::AstNode* result = makeInterpolationStringLiteral(token, "");
    for (const language::InterpolationPart& part : parts)
    {
        ast::AstNode* piece = nullptr;
        if (part.kind == language::InterpolationPartKind::Literal)
        {
            if (part.text.empty()) continue;
            piece = makeInterpolationStringLiteral(token, part.text);
        }
        else
        {
            ast::AstNode* hole = parseInterpolationFragment(
                token, part, "<runtime-interpolation>", knownTypes, true);
            piece = formatRuntimeInterpolationHole(token, part, hole);
        }
        result = makeBinaryExpression(TokenKind::Plus, result, piece);
        result->setRange(token.range());
    }
    return result;
}

ast::AstNode* Parser::finishInterpolatedString(
    ast::AstNode* node,
    const lexer::Token& token,
    const std::string& value)
{
    std::vector<language::InterpolationPart> parts;
    std::string error;
    if (!language::parseStaticInterpolation(value, parts, error))
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error,
            token.range(),
            std::move(error));
        node->setIntegerValue(0);
        return node;
    }
    node->setIntegerValue(parts.size());
    appendInterpolationHoles(node, token, parts);
    if (std::ranges::all_of(parts, isStaticInterpolationHole)) return node;
    return lowerRuntimeInterpolation(token, parts);
}

ast::AstNode* Parser::makeTokenPrimaryExpression(
    const lexer::Token& token,
    ast::AstNodeKind kind)
{
    consume();
    SourceRange range = token.range();
    std::string text(token.lexeme());
    if (token.kind() == TokenKind::KwThis && parsingExtensionBody_)
        text = "__nsc_extension_this";
    if (kind == ast::AstNodeKind::IdentifierExpression &&
        at(TokenKind::Colon) && peek().kind() == TokenKind::Colon)
    {
        consume(); consume();
        if (!at(TokenKind::Identifier))
        {
            expect(TokenKind::Identifier, "type name after module qualifier '::'");
            return makeErrorExpression();
        }
        const lexer::Token& qualified = consume();
        text += "::";
        text += qualified.lexeme();
        range = join(token.range(), qualified.range());
    }
    ast::AstNode* node = arena_.make<ast::AstNode>(kind, range);
    node->setTokenKind(token.kind());
    node->setText(std::move(text));
    if (const auto* integer = std::get_if<std::uint64_t>(&token.value()))
        node->setIntegerValue(*integer);
    if (const auto* floating = std::get_if<double>(&token.value()))
        node->setFloatingValue(*floating);
    if (const auto* character = std::get_if<std::uint8_t>(&token.value()))
        node->setIntegerValue(*character);
    if (const auto* string = std::get_if<std::string>(&token.value()))
    {
        node->setText(*string);
        if (kind == ast::AstNodeKind::InterpolatedStringExpression)
            return finishInterpolatedString(node, token, *string);
    }
    return node;
}

ast::AstNode* Parser::parsePrimaryExpression()
{
    if (startsParenthesizedBlock("match")) return parseMatchExpression();
    if (atContextualKeyword("loop") &&
        (peek().kind() == TokenKind::LeftBrace ||
         (peek().kind() == TokenKind::Identifier && peek(2).kind() == TokenKind::LeftBrace)))
        return parseLoopExpression();
    if (comprehensionExpressionStartsHere()) return parseComprehensionExpression();
    if (atContextualKeyword("nameof") && peek().kind() == TokenKind::LeftParen)
        return parseNameOfExpression();
    if ((atContextualKeyword("checked") || atContextualKeyword("saturating") ||
         atContextualKeyword("unchecked") || atContextualKeyword("wrapping")) &&
        peek().kind() == TokenKind::LeftParen)
        return parseOverflowPolicyExpression();
    if (at(TokenKind::LeftBracket)) return parseCollectionExpression();
    if (at(TokenKind::LeftParen)) return parseParenthesizedPrimaryExpression();
    const lexer::Token& token = current();
    const std::optional<ast::AstNodeKind> kind = classifyPrimaryToken(token.kind());
    if (kind.has_value()) return makeTokenPrimaryExpression(token, *kind);
    diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedExpression,
        diagnostics::DiagnosticSeverity::Error,
        token.range(),
        "expected expression");
    if (!at(TokenKind::EndOfFile)) consume();
    return makeErrorExpression();
}

ast::AstNode* Parser::parseBinaryLevel(
    ast::AstNode* (Parser::*nextLevel)(),
    std::span<const TokenKind> operators,
    ast::AstNodeKind nodeKind)
{
    ast::AstNode* left = (this->*nextLevel)();
    while (containsExpressionToken(operators, current().kind()))
    {
        const TokenKind operation = consume().kind();
        ast::AstNode* right = (this->*nextLevel)();
        ast::AstNode* node = arena_.make<ast::AstNode>(nodeKind, join(left->range(), right->range()));
        node->setTokenKind(operation);
        node->setFirst(left);
        node->setSecond(right);
        left = node;
    }
    return left;
}

}
