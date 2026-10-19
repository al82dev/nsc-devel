#pragma once

#include "nativesharp/Parser/Parser.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
class Parser::StructuredControlParser final
{
public:
    explicit StructuredControlParser(Parser& parser) noexcept;

    ast::AstNode* parse();

private:
    void execute();

    enum class WorkKind
    {
        ParseControl,
        ParseIf,
        ParseWhile,
        ParseDoWhile,
        ParseFor,
        ParseUsing,
        ParseDefer,
        ParseForeach,
        ParseConditionalPattern,
        ParseSwitch,
        ParseMatch,
        ParseEmbedded,
        WrapEmbedded,
        ParseBlock,
        ContinueBlock,
        AppendBlock,
        FinishThen,
        FinishElse,
        FinishWhile,
        FinishDoWhile,
        FinishFor,
        FinishUsing,
        FinishDefer,
        FinishForeach,
        FinishConditionalPatternSuccess,
        FinishConditionalPatternFailure,
        FinishConditionalPatternWhile,
        FinishConditionalPatternLetElse,
        ContinueSwitch,
        ContinueSwitchSection,
        AppendSwitchStatement,
        ContinueMatch,
        FinishMatchArm
    };
    struct Work final
    {
        WorkKind kind = WorkKind::ParseIf;
        std::size_t stateIndex = 0;
    };
    struct BlockParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* block = nullptr;
        ast::AstNode* destination = nullptr;
    };
    struct SwitchParseState final
    {
        std::size_t beginIndex = 0;
        std::size_t sectionBegin = 0;
        ast::AstNode* conditionDeclaration = nullptr;
        ast::AstNode* node = nullptr;
        ast::AstNode* section = nullptr;
    };
    struct UsingParseState final
    {
        std::size_t beginIndex = 0;
        ast::AstNode* node = nullptr;
    };
    struct DeferCapture final
    {
        bool byReference = false;
        bool receiver = false;
        std::string name;
        SourceRange range;
    };
    struct DeferParseState final
    {
        std::size_t beginIndex = 0;
        SourceRange deferRange;
        std::vector<DeferCapture> captures;
    };
    enum class ConditionalPatternForm
    {
        If,
        While,
        LetElse
    };
    struct ConditionalPatternParseState final
    {
        std::size_t beginIndex = 0;
        ConditionalPatternForm form = ConditionalPatternForm::LetElse;
        SourceRange featureRange;
        Pattern pattern;
        ast::AstNode* sourceExpression = nullptr;
        ast::AstNode* successBody = nullptr;
        ast::AstNode* failureBody = nullptr;
        bool hasReferenceBinding = false;
        bool hasMoveBinding = false;
    };
    struct ConditionalPatternLoweringContext final
    {
        SourceRange statementRange;
        std::string temporary;
        std::string matched;
    };
    struct MatchArm final
    {
        Pattern pattern;
        Parser::MatchPatternAnalysis analysis;
        SourceRange range;
        ast::AstNode* guard = nullptr;
        ast::AstNode* body = nullptr;
    };
    using MatchIntegerIntervalIndex = std::map<std::int64_t, std::int64_t>;

    struct MatchParseState final
    {
        std::size_t beginIndex = 0;
        std::size_t armBeginIndex = 0;
        SourceRange keywordRange;
        ast::AstNode* governing = nullptr;
        MatchArm pendingArm;
        std::vector<MatchArm> arms;
        bool sawCatchAll = false;
        bool hasReferenceBinding = false;
        bool hasMoveBinding = false;
        MatchIntegerIntervalIndex coveredIntegerRanges;
    };

    bool ordinaryIfAtCursor() const noexcept;

    bool ordinaryWhileAtCursor() const noexcept;

    bool conditionalPatternAtCursor() const noexcept;

    bool structuredControlAtCursor() const noexcept;

    ast::AstNode* wrapEmbedded(ast::AstNode* statement);

    void dispatch(const Work& current);

    void dispatchParse(const Work& current);

    void dispatchFinish(const Work& current);

    void dispatchContinuation(const Work& current);

    void parseControl();

    void parseIf();

    void parseWhile();

    void parseDoWhile();

    void parseFor();

    void parseUsing();

    void finishUsing(std::size_t index, ast::AstNode* body);

    void parseDefer();

    void finishDefer(std::size_t index, ast::AstNode* body);

    void parseForeach();

    void finishForeach(std::size_t index, ast::AstNode* body);

    void parseConditionalPatternPrefix(ConditionalPatternParseState& state);

    void parseConditionalPatternPayload(ConditionalPatternParseState& state);

    void parseConditionalPatternHeader(ConditionalPatternParseState& state);

    void parseConditionalPattern();

    void finishConditionalPatternSuccess(std::size_t index, ast::AstNode* successBody);

    void finishConditionalPatternFailure(std::size_t index, ast::AstNode* failureBody);

    void finishConditionalPatternWhile(std::size_t index, ast::AstNode* successBody);

    void finishConditionalPatternLetElse(std::size_t index, ast::AstNode* failureBody);

    PatternExpressionFactory makeConditionalPatternRoot(
        const ConditionalPatternLoweringContext& context);

    void addConditionalPatternPrelude(
        const ConditionalPatternParseState& state,
        const ConditionalPatternLoweringContext& context,
        ast::AstNode* destination);

    ast::AstNode* makeConditionalPatternSuccess(
        const ConditionalPatternParseState& state,
        const ConditionalPatternLoweringContext& context,
        bool includeBody);

    ast::AstNode* lowerConditionalPatternWhile(
        ConditionalPatternParseState& state,
        const ConditionalPatternLoweringContext& context);

    ast::AstNode* lowerConditionalPatternSelection(
        ConditionalPatternParseState& state,
        const ConditionalPatternLoweringContext& context);

    ast::AstNode* lowerConditionalPattern(ConditionalPatternParseState& state);

    ast::AstNode* makeConditionalPatternNotMatched(
        SourceRange range,
        const std::string& matched);

    void finishConditionalPatternLowering(
        ConditionalPatternParseState& state,
        const PatternExpressionFactory& root,
        ast::AstNode* lowered,
        const ConditionalPatternLoweringContext& context);

    void parseSwitch();

    ast::AstNode* parseSwitchGoverning(SwitchParseState& state);

    void continueSwitch(std::size_t index);

    void startSwitchSection(std::size_t index);

    void appendSwitchLabel(ast::AstNode* section);

    void continueSwitchSection(const Work& current);

    void pushSwitchStatement(std::size_t index, WorkKind child);

    void appendSwitchStatement(std::size_t index);

    static void appendSwitchStatement(SwitchParseState& state, ast::AstNode* statement);

    ast::AstNode* finishSwitch(SwitchParseState& state);

    // Consumes the statement header and schedules arm parsing without retaining this C++ frame.
    void parseMatch();

    // Advances one arm or lowers the completed match; stateIndex remains stable across reallocations.
    void continueMatch(std::size_t index);

    // Parses and validates only the pattern/guard/arrow prefix; the body is a separate task.
    void parseMatchArmHeader(MatchParseState& state);

    // Preserves the historical missing-block diagnostic before statement synchronization.
    void reportMissingMatchArmBody();

    // Returns the stable spelling embedded in binding-mode diagnostics.
    static std::string_view matchBindingModeName(PatternBindingMode mode) noexcept;

    // Publishes the two historical diagnostic families from one source-ordered fact record.
    void reportMatchPatternDiagnostics(const Parser::MatchPatternAnalysis& analysis);

    // Accumulates ownership requirements without rescanning already analyzed arm patterns.
    static void accumulateMatchOwnership(
        MatchParseState& state,
        const Parser::MatchPatternAnalysis& analysis);

    // Updates cross-arm reachability state while preserving diagnostic order.
    void validateMatchArmReachability(MatchParseState& state);

    // Emits the established constant-false guard diagnostic at the guard range.
    void reportConstantFalseMatchGuard(const MatchArm& arm);

    // Emits the established unreachable-arm diagnostic after an unconditional catch-all.
    void reportMatchArmAfterCatchAll(const MatchArm& arm);

    // Collects a complete literal/range OR domain; false means the mixed domain is not comparable.
    static bool collectMatchIntegerIntervals(
        const Pattern& root,
        std::vector<std::pair<std::int64_t, std::int64_t>>& intervals);

    // Appends one half-open integer interval when the pattern has a bounded integral domain.
    static bool appendMatchIntegerInterval(
        const Pattern& pattern,
        std::vector<std::pair<std::int64_t, std::int64_t>>& intervals);

    // Diagnoses overlap and publishes ranges only for arms that are not guard-restricted.
    void validateMatchIntegerIntervals(MatchParseState& state, bool guardAlwaysTrue);

    // Sorts and coalesces one arm, reporting whether strict half-open overlap existed within it.
    static bool normalizeMatchIntegerIntervals(
        std::vector<std::pair<std::int64_t, std::int64_t>>& intervals);

    // Queries the disjoint ordered index in logarithmic time for each normalized interval.
    static bool matchIntervalsOverlap(
        const std::vector<std::pair<std::int64_t, std::int64_t>>& current,
        const MatchIntegerIntervalIndex& covered);

    // Inserts one interval and coalesces overlap or adjacency without changing half-open queries.
    static void insertMatchIntegerInterval(
        MatchIntegerIntervalIndex& covered,
        std::pair<std::int64_t, std::int64_t> interval);

    // Attaches a completed body, closes the source range, and consumes the optional comma.
    void finishMatchArm(std::size_t index, ast::AstNode* body);

    // Materializes the historical early lowering after every arm body has completed.
    ast::AstNode* lowerMatchStatement(MatchParseState& state);

    // Reports an empty statement match while retaining its lowering shell.
    void reportEmptyMatch(const MatchParseState& state);

    // Builds the source-ordered semantic coverage bridge consumed by later analysis.
    std::string buildMatchCoverage(const std::vector<MatchArm>& arms);

    // Encodes one arm as wildcard, list-length, nominal-case, or unknown coverage.
    void appendMatchArmCoverage(std::string& coverage, const MatchArm& arm);

    // Sorts and deduplicates list domains before emitting their stable marker format.
    static void appendMatchListCoverage(
        std::string& coverage,
        std::vector<std::pair<std::size_t, bool>>& lengths,
        bool partial);

    // Sorts and deduplicates nominal cases before emitting their stable marker format.
    static void appendMatchCaseCoverage(
        std::string& coverage,
        std::vector<std::string>& cases,
        bool partial);

    // Creates the governing-value and selected-flag locals with preserved ownership mode.
    void appendMatchGoverningState(
        ast::AstNode& lowered,
        const MatchParseState& state,
        const std::string& temporary);

    // Creates the false-initialized selected flag used by the lowered decision chain.
    ast::AstNode* makeMatchSelectedDeclaration(
        SourceRange range,
        const std::string& temporary);

    // Lowers one arm to the historical not-selected guard and pattern matcher.
    ast::AstNode* lowerMatchArm(
        const MatchArm& arm,
        SourceRange matchRange,
        const std::string& temporary);

    // Prepends selected=true to the original body statements without copying their AST nodes.
    ast::AstNode* makeMatchSelectedBody(const MatchArm& arm, const std::string& temporary);

    // Evaluates a source guard only after pattern bindings have been materialized.
    ast::AstNode* wrapMatchGuard(const MatchArm& arm, ast::AstNode* selectedBody);

    // Builds the outer !selected condition that enforces first-match semantics.
    ast::AstNode* makeMatchNotSelected(SourceRange range, const std::string& temporary);

    void parseEmbedded();

    void finishWrapEmbedded();

    void parseBlock();

    void continueBlock(const Work& current);

    void pushBlockChild(std::size_t blockIndex, WorkKind child);

    void appendBlock(std::size_t index);

    void finishThen(std::size_t index);

    void finishElse(std::size_t index);

    void finishWhile(std::size_t index);

    void finishDoWhile(std::size_t index);

    void finishFor(std::size_t index);

    ast::AstNode* popValue();

    Parser& parser_;
    std::vector<IfParseState> ifStates_;
    std::vector<WhileParseState> whileStates_;
    std::vector<DoWhileParseState> doWhileStates_;
    std::vector<ForParseState> forStates_;
    std::vector<UsingParseState> usingStates_;
    std::vector<DeferParseState> deferStates_;
    std::vector<ForeachParseState> foreachStates_;
    std::vector<ConditionalPatternParseState> conditionalPatternStates_;
    std::vector<SwitchParseState> switchStates_;
    std::vector<MatchParseState> matchStates_;
    std::vector<BlockParseState> blockStates_;
    std::vector<Work> work_;
    std::vector<ast::AstNode*> values_;
};
}
