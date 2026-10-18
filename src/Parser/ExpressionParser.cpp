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

enum class AssignmentContinuationKind
{
    Assignment,
    ExpressionLambda
};

struct AssignmentContinuation final
{
    AssignmentContinuationKind kind;
    ast::AstNode* node = nullptr;
    TokenKind operation = TokenKind::Invalid;
    bool reference = false;
};

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

ast::AstNode* Parser::parseAssignmentExpression()
{
    std::vector<AssignmentContinuation> continuations;
    ast::AstNode* value = nullptr;
    bool complete = false;
    while (!complete)
    {
        if (looksLikeLambdaExpression())
        {
            ast::AstNode* lambda = parseLambdaExpressionHead();
            if (at(TokenKind::LeftBrace))
            {
                finishLambdaExpression(lambda, parseBlock(), true);
                value = lambda;
                complete = true;
                continue;
            }
            continuations.push_back(
                {AssignmentContinuationKind::ExpressionLambda, lambda});
            continue;
        }

        ast::AstNode* left = parseConditionalExpression();
        if (!lexer::isAssignmentOperator(current().kind()))
        {
            value = left;
            complete = true;
            continue;
        }

        const TokenKind operation = consume().kind();
        const bool referenceAssignment = operation == TokenKind::Equal &&
            atContextualKeyword("ref");
        if (referenceAssignment) consume();
        continuations.push_back(
            {AssignmentContinuationKind::Assignment, left, operation, referenceAssignment});
    }

    while (!continuations.empty())
    {
        const AssignmentContinuation continuation = continuations.back();
        continuations.pop_back();
        if (continuation.kind == AssignmentContinuationKind::ExpressionLambda)
        {
            finishLambdaExpression(continuation.node, value, false);
            value = continuation.node;
            continue;
        }
        ast::AstNode* assignment = arena_.make<ast::AstNode>(
            ast::AstNodeKind::AssignmentExpression,
            join(continuation.node->range(), value->range()));
        assignment->setTokenKind(continuation.operation);
        assignment->setFirst(continuation.node);
        assignment->setSecond(value);
        assignment->setReference(continuation.reference);
        value = assignment;
    }
    return value;
}

// `condition ? whenTrue : whenFalse`, between assignment and logical-or in precedence, and
// RIGHT-associative -- so `a ? b : c ? d : e` is `a ? b : (c ? d : e)`, and the false branch
// accepts an assignment (`c ? x : y = z` is `c ? x : (y = z)`), which is what C# does.
//
// The true branch parses as a full expression because it sits between `?` and `:`, where a
// nested conditional cannot be ambiguous.
ast::AstNode* Parser::parseConditionalExpression()
{
    ast::AstNode* condition = parseNullCoalescingExpression();
    if (!at(TokenKind::Question))
    {
        return condition;
    }

    consume();
    ast::AstNode* whenTrue = parseExpression();
    expect(TokenKind::Colon, "':' in a conditional expression");
    ast::AstNode* whenFalse = parseAssignmentExpression();

    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConditionalExpression,
        join(condition->range(), whenFalse != nullptr ? whenFalse->range() : condition->range()));
    node->setFirst(condition);
    node->setSecond(whenTrue);
    node->setThird(whenFalse);
    return node;
}

ast::AstNode* Parser::parseNullCoalescingExpression()
{
    ast::AstNode* left = parseRangeExpression();
    if (!at(TokenKind::QuestionQuestion)) return left;
    const SourceRange operation = consume().range();
    ast::AstNode* right = parseNullCoalescingExpression();
    ast::AstNode* node = arena_.make<ast::AstNode>(
        ast::AstNodeKind::NullCoalescingExpression,
        join(left->range(), right != nullptr ? right->range() : operation));
    node->setTokenKind(TokenKind::QuestionQuestion);
    node->setFirst(left);
    node->setSecond(right);
    return node;
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
    if (!looksLikeClosedGenericTypeReference()) return parsePrimaryExpression();
    const SourceRange start = current().range();
    ast::TypeSyntax* type = parseType();
    if (type == nullptr) return parsePrimaryExpression();
    ast::AstNode* expression = arena_.make<ast::AstNode>(
        ast::AstNodeKind::IdentifierExpression,
        join(start, tokens_[index_ - 1].range()));
    expression->setText(type->baseName());
    expression->setTypeSyntax(type);
    return expression;
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
    ast::AstNode* expression = parsePostfixBaseExpression();
    while (true)
    {
        std::vector<ast::TypeSyntax*> genericArguments;
        const bool explicitGenericArguments = looksLikeGenericCallTypeArguments();
        if (explicitGenericArguments) parseTypeArgumentList(genericArguments);
        if (consumeIf(TokenKind::LeftParen))
        {
            expression = parseCallSuffix(
                expression, std::move(genericArguments), explicitGenericArguments);
            continue;
        }
        if (!genericArguments.empty())
        {
            diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                current().range(),
                "generic type arguments in an expression must be followed by a call in Stage 44");
            return expression;
        }
        if (consumeIf(TokenKind::LeftBracket))
        {
            expression = parseIndexSuffix(expression, false);
            continue;
        }
        if (at(TokenKind::QuestionBracket))
        {
            consume();
            expression = parseIndexSuffix(expression, true);
            continue;
        }
        if (at(TokenKind::QuestionDot))
        {
            expression = parseNullConditionalMemberSuffix(expression);
            continue;
        }
        if (at(TokenKind::Dot) || at(TokenKind::Arrow))
        {
            expression = parseMemberSuffix(expression);
            continue;
        }
        if (at(TokenKind::PlusPlus) || at(TokenKind::MinusMinus))
        {
            expression = parsePostfixOperatorSuffix(expression);
            continue;
        }
        if (at(TokenKind::Question) && !questionBeginsConditional())
        {
            expression = parseResultPropagationSuffix(expression);
            continue;
        }
        if (atContextualKeyword("with"))
        {
            expression = parseWithExpression(expression);
            continue;
        }
        break;
    }
    return expression;
}

bool Parser::questionBeginsConditional() const noexcept
{
    if (!at(TokenKind::Question)) return false;

    // `?` is both a postfix Result operator and the opener of `condition ? a : b`.  Looking only
    // at the following token is insufficient (`result? + 1` and `condition ? +1 : 0` share it),
    // so find the colon owned by this exact nesting level. Nested ternaries consume their own
    // colons; delimiters end the current expression. This keeps the existing conditional grammar
    // intact while allowing propagation to participate in every higher-precedence expression.
    std::size_t parentheses = 0;
    std::size_t brackets = 0;
    std::size_t braces = 0;
    std::size_t nestedQuestions = 0;
    for (std::size_t cursor = index_ + 1U; cursor < tokens_.size(); ++cursor)
    {
        const TokenKind kind = tokens_[cursor].kind();
        if (parentheses == 0 && brackets == 0 && braces == 0 &&
            endsConditionalQuestionScan(kind))
            return false;
        switch (kind)
        {
            case TokenKind::LeftParen: ++parentheses; break;
            case TokenKind::LeftBracket: ++brackets; break;
            case TokenKind::LeftBrace: ++braces; break;
            case TokenKind::RightParen:
                if (parentheses != 0) { --parentheses; break; }
                if (brackets == 0 && braces == 0) return false;
                break;
            case TokenKind::RightBracket:
                if (brackets != 0) { --brackets; break; }
                if (parentheses == 0 && braces == 0) return false;
                break;
            case TokenKind::RightBrace:
                if (braces != 0) { --braces; break; }
                if (parentheses == 0 && brackets == 0) return false;
                break;
            case TokenKind::Question:
                if (parentheses == 0 && brackets == 0 && braces == 0) ++nestedQuestions;
                break;
            case TokenKind::Colon:
                if (parentheses == 0 && brackets == 0 && braces == 0)
                {
                    if (nestedQuestions == 0) return true;
                    --nestedQuestions;
                }
                break;
            default: break;
        }
    }
    return false;
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
