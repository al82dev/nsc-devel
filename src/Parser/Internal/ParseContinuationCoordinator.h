#pragma once

#include "nativesharp/Parser/Parser.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
class Parser::ParseContinuationCoordinator final
{
public:
    explicit ParseContinuationCoordinator(Parser& parser) noexcept;
    ~ParseContinuationCoordinator() noexcept;

    ast::AstNode* parse(ContinuationRequestKind kind);

private:
    static constexpr std::size_t noSlot = std::numeric_limits<std::size_t>::max();

    enum class WorkKind : std::uint8_t
    {
        Expression,
        LoopExpression,
        ComprehensionExpression,
        ResumeExpression,
        UnaryExpression,
        ConditionalExpression,
        Block,
        ContinueBlock,
        AppendBlockStatement,
        Statement,
        FinishLambdaBlock,
        FinishExpressionStatement,
        FinishReturn,
        FinishThrow,
        FinishJump,
        FinishYieldReturn,
        FinishStaticAssert,
        FinishDeconstruction,
        ContinueVariableDeclaration,
        FinishVariableInitializer,
        Aggregate,
        ContinueAggregate,
        FinishAggregateElement,
        FinishLocalFunction,
        Embedded,
        FinishEmbedded,
        FinishIfCondition,
        FinishIfThen,
        FinishIfElse,
        FinishWhileCondition,
        FinishWhileBody,
        FinishDoBody,
        FinishDoCondition,
        FinishForInitializer,
        FinishForCondition,
        FinishForIncrement,
        FinishForBody,
        ContinueForExpressionList,
        FinishForExpression,
        FinishForeachCollection,
        FinishForeachProbe,
        FinishForeachBody,
        FinishConditionalPatternSource,
        FinishConditionalPatternSuccess,
        FinishConditionalPatternFailure,
        FinishConditionalPatternWhile,
        FinishConditionalPatternLetElse,
        FinishSwitchGoverning,
        ContinueSwitch,
        ContinueSwitchLabels,
        FinishSwitchLabel,
        ContinueSwitchSection,
        AppendSwitchStatement,
        FinishMatchGoverning,
        ContinueMatch,
        FinishMatchGuard,
        FinishMatchArm,
        Try,
        ContinueTry,
        FinishTryBlock,
        FinishTryCatchFilter,
        FinishTryCatchBlock,
        FinishTryFinallyBlock,
        CatchClause,
        FinishStandaloneCatchFilter,
        FinishStandaloneCatchBlock,
        FinishUsingInitializer,
        FinishUsingBody,
        FinishDeferBody,
        FinishFloatingPointScope,
        FinishUnsafeScope,
        ContinueInlineAssembly,
        FinishInlineAssemblyOutput,
        FinishInlineAssemblyInput,
        FinishComprehensionYield
    };

    enum class StatementKind : std::uint8_t
    {
        Block,
        Expression,
        Return,
        MustTailReturn,
        Throw,
        Jump,
        Yield,
        StaticAssert,
        AwaitExpression,
        Deconstruction,
        VariableDeclaration,
        StaticLocal,
        LocalFunction,
        If,
        While,
        DoWhile,
        For,
        Foreach,
        ConditionalPattern,
        Switch,
        Match,
        Try,
        Using,
        Defer,
        FloatingPointScope,
        UnsafeScope,
        InlineAssembly,
        ComprehensionYield
    };

    struct ResultSlot final
    {
        ast::AstNode* value = nullptr;
        bool ready = false;
    };

    struct ExpressionFrame final
    {
        std::size_t resultSlot = 0;
        std::unique_ptr<ExpressionContinuation> continuation;
    };

    struct BlockFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t beforeIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* block = nullptr;
        ast::AstNode* destination = nullptr;
    };

    struct VariableFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::TypeSyntax* type = nullptr;
        ast::AstNode* first = nullptr;
        ast::AstNode* declarationList = nullptr;
        bool isStaticLocal = false;
        bool isConst = false;
        bool isRaii = false;
        bool isVolatile = false;
        bool isReferenceLocal = false;
        bool isPinned = false;
        bool isScope = false;
        bool expectSemicolon = true;
    };

    struct AggregateFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* node = nullptr;
    };

    struct IfFrame final
    {
        IfParseState state;
        std::size_t resultSlot = 0;
    };

    struct WhileFrame final
    {
        WhileParseState state;
        std::size_t resultSlot = 0;
    };

    struct DoWhileFrame final
    {
        DoWhileParseState state;
        std::size_t resultSlot = 0;
        ast::AstNode* body = nullptr;
    };

    struct ForFrame final
    {
        ForParseState state;
        std::size_t resultSlot = 0;
    };

    struct ForExpressionFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* first = nullptr;
        ast::AstNode* sequence = nullptr;
    };

    struct ForeachFrame final
    {
        ForeachParseState state;
        std::size_t resultSlot = 0;
        std::size_t collectionStart = 0;
        std::size_t collectionEnd = 0;
    };

    enum class ConditionalPatternForm : std::uint8_t
    {
        If,
        While,
        LetElse
    };

    struct ConditionalPatternFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
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

    struct SwitchFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        std::size_t sectionBegin = 0;
        std::size_t beforeStatement = 0;
        ast::AstNode* conditionDeclaration = nullptr;
        ast::AstNode* node = nullptr;
        ast::AstNode* section = nullptr;
        std::uint64_t labelCount = 0;
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

    struct MatchFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
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

    struct TryFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        std::size_t pendingCatchBeginIndex = 0;
        ast::AstNode* node = nullptr;
        ast::AstNode* pendingCatch = nullptr;
        bool hasFinally = false;
    };

    struct CatchFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* clause = nullptr;
    };

    struct UsingFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::TypeSyntax* resourceType = nullptr;
        std::string resourceName;
        ast::AstNode* node = nullptr;
    };

    struct DeferCapture final
    {
        bool byReference = false;
        bool receiver = false;
        std::string name;
        SourceRange range;
    };

    struct DeferFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        SourceRange deferRange;
        std::vector<DeferCapture> captures;
    };

    struct FloatingPointScopeFrame final
    {
        std::size_t resultSlot = 0;
        ast::FloatingPointModeSyntaxKind mode = ast::FloatingPointModeSyntaxKind::Inherit;
        std::string modeName;
    };

    struct UnsafeScopeFrame final
    {
        std::size_t resultSlot = 0;
        std::string reason;
    };

    struct InlineAssemblyFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        ast::AstNode* statement = nullptr;
        std::uint64_t outputOrdinal = 0;
        std::uint64_t inputOrdinal = 0;
        bool hasOperandClauses = false;
    };

    struct ComprehensionYieldFrame final
    {
        std::size_t beginIndex = 0;
        std::size_t resultSlot = 0;
        SourceRange yieldRange;
        std::string builderName;
        std::string loopLabel;
    };

    struct Work final
    {
        Work(WorkKind requestedKind, std::size_t requestedResultSlot) noexcept
            : kind(requestedKind), resultSlot(requestedResultSlot)
        {
        }

        WorkKind kind = WorkKind::Expression;
        std::size_t resultSlot = 0;
        std::size_t frameIndex = noSlot;
        std::size_t childSlot = noSlot;
        std::size_t beginIndex = 0;
        ast::AstNode* node = nullptr;
        lexer::TokenKind tokenKind = lexer::TokenKind::Invalid;
        bool flag = false;
        bool secondaryFlag = false;
        std::string text;
    };

    std::size_t request(ContinuationRequestKind kind);
    std::size_t makeResultSlot();
    void drain(std::size_t resultSlot);
    void execute(Work work);
    bool executeExpressionWork(const Work& work);
    bool executeStatementWork(const Work& work);
    void executeControlWork(const Work& work);
    void complete(std::size_t resultSlot, ast::AstNode* value);
    ast::AstNode* childValue(const Work& work) const;

    void startExpression(const Work& work);
    void startLoopExpression(const Work& work);
    void startComprehensionExpression(const Work& work);
    void startExpressionContinuation(
        const Work& work,
        ContinuationRequestKind rootKind);
    void advanceExpression(std::size_t frameIndex, ast::AstNode* completedBlock);
    void resumeExpression(const Work& work);
    void startUnaryExpression(const Work& work);
    void startConditionalExpression(const Work& work);
    void finishLambdaBlock(const Work& work);
    void startBlock(const Work& work);
    void continueBlock(const Work& work);
    void appendBlockStatement(const Work& work);
    void finishBlock(BlockFrame& frame);

    StatementKind classifyStatement() const;
    bool ordinaryIfStarts() const noexcept;
    bool ordinaryWhileStarts() const noexcept;
    bool declarationStatementStarts() const noexcept;
    void startStatement(const Work& work);
    void startMustTailReturn(const Work& work);
    void startExpressionStatement(const Work& work);
    void finishExpressionStatement(const Work& work);
    void startReturn(const Work& work, bool mustTail);
    void finishReturn(const Work& work);
    void startThrow(const Work& work);
    void finishThrow(const Work& work);
    void startJump(const Work& work);
    void finishJump(const Work& work);
    void startYield(const Work& work);
    void finishYieldReturn(const Work& work);
    void startStaticAssert(const Work& work);
    void finishStaticAssert(const Work& work);
    void startAwaitExpression(const Work& work);
    void startDeconstruction(const Work& work);
    void finishDeconstruction(const Work& work);
    void startVariableDeclaration(
        const Work& work,
        bool isStaticLocal,
        bool expectSemicolon = true);
    void parseVariablePrefix(VariableFrame& frame);
    void continueVariableDeclaration(const Work& work);
    ast::AstNode* makeVariableDeclarator(VariableFrame& frame, bool cloneType);
    void scheduleVariableInitializer(const Work& work);
    void finishVariableInitializer(const Work& work);
    void acceptVariableDeclarator(std::size_t frameIndex, ast::AstNode* variable);
    void finishVariableDeclaration(VariableFrame& frame);
    void startAggregate(const Work& work);
    void continueAggregate(const Work& work);
    void finishAggregateElement(const Work& work);
    void finishAggregate(AggregateFrame& frame);
    void startLocalFunction(const Work& work);
    void finishLocalFunction(const Work& work);
    void startEmbedded(const Work& work);
    void finishEmbedded(const Work& work);
    ast::AstNode* wrapEmbedded(ast::AstNode* statement);
    ast::AstNode* conditionFromDeclaration(ast::AstNode* declaration);
    void startIf(const Work& work);
    void finishIfCondition(const Work& work);
    void finishIfThen(const Work& work);
    void finishIfElse(const Work& work);
    void startWhile(const Work& work);
    void finishWhileCondition(const Work& work);
    void finishWhileBody(const Work& work);
    void startDoWhile(const Work& work);
    void finishDoBody(const Work& work);
    void finishDoCondition(const Work& work);
    void startFor(const Work& work);
    void finishForInitializer(const Work& work);
    void scheduleForCondition(std::size_t frameIndex);
    void finishForCondition(const Work& work);
    void scheduleForIncrement(std::size_t frameIndex);
    void finishForIncrement(const Work& work);
    void scheduleForBody(std::size_t frameIndex);
    void finishForBody(const Work& work);
    void startForExpressionList(std::size_t resultSlot, std::size_t beginIndex);
    void continueForExpressionList(const Work& work);
    void finishForExpression(const Work& work);
    void startForeach(const Work& work);
    void finishForeachCollection(const Work& work);
    void finishForeachProbe(const Work& work);
    void finishForeachBody(const Work& work);
    void startConditionalPattern(const Work& work);
    void parseConditionalPatternPrefix(ConditionalPatternFrame& frame);
    void analyzeConditionalPattern(ConditionalPatternFrame& frame);
    void finishConditionalPatternSource(const Work& work);
    void scheduleConditionalPatternBody(
        std::size_t frameIndex,
        WorkKind finishKind,
        WorkKind bodyKind);
    void finishConditionalPatternSuccess(const Work& work);
    void finishConditionalPatternFailure(const Work& work);
    void finishConditionalPatternWhile(const Work& work);
    void finishConditionalPatternLetElse(const Work& work);
    PatternExpressionFactory makeConditionalPatternRoot(
        const ConditionalPatternLoweringContext& context);
    void addConditionalPatternPrelude(
        const ConditionalPatternFrame& frame,
        const ConditionalPatternLoweringContext& context,
        ast::AstNode* destination);
    ast::AstNode* makeConditionalPatternSuccess(
        const ConditionalPatternFrame& frame,
        const ConditionalPatternLoweringContext& context,
        bool includeBody);
    ast::AstNode* lowerConditionalPatternWhile(
        ConditionalPatternFrame& frame,
        const ConditionalPatternLoweringContext& context);
    ast::AstNode* lowerConditionalPatternSelection(
        ConditionalPatternFrame& frame,
        const ConditionalPatternLoweringContext& context);
    ast::AstNode* lowerConditionalPattern(ConditionalPatternFrame& frame);
    ast::AstNode* makeConditionalPatternNotMatched(
        SourceRange range,
        const std::string& matched);
    void finishConditionalPatternLowering(
        ConditionalPatternFrame& frame,
        const PatternExpressionFactory& root,
        ast::AstNode* lowered,
        const ConditionalPatternLoweringContext& context);
    void startSwitch(const Work& work);
    void finishSwitchGoverning(const Work& work);
    void continueSwitch(const Work& work);
    void startSwitchSection(std::size_t frameIndex);
    void continueSwitchLabels(const Work& work);
    void finishSwitchLabel(const Work& work);
    void continueSwitchSection(const Work& work);
    void appendSwitchStatement(const Work& work);
    void completeSwitch(SwitchFrame& frame);
    void startMatch(const Work& work);
    void finishMatchGoverning(const Work& work);
    void continueMatch(const Work& work);
    void parseMatchArmHeader(MatchFrame& frame);
    void finishMatchGuard(const Work& work);
    void scheduleMatchBody(std::size_t frameIndex);
    void finishMatchArm(const Work& work);
    void reportMissingMatchArmBody();
    static std::string_view matchBindingModeName(PatternBindingMode mode) noexcept;
    void reportMatchPatternDiagnostics(const Parser::MatchPatternAnalysis& analysis);
    static void accumulateMatchOwnership(
        MatchFrame& frame,
        const Parser::MatchPatternAnalysis& analysis);
    void validateMatchArmReachability(MatchFrame& frame);
    void reportConstantFalseMatchGuard(const MatchArm& arm);
    void reportMatchArmAfterCatchAll(const MatchArm& arm);
    static bool collectMatchIntegerIntervals(
        const Pattern& root,
        std::vector<std::pair<std::int64_t, std::int64_t>>& intervals);
    static bool appendMatchIntegerInterval(
        const Pattern& pattern,
        std::vector<std::pair<std::int64_t, std::int64_t>>& intervals);
    void validateMatchIntegerIntervals(MatchFrame& frame, bool guardAlwaysTrue);
    static bool normalizeMatchIntegerIntervals(
        std::vector<std::pair<std::int64_t, std::int64_t>>& intervals);
    static bool matchIntervalsOverlap(
        const std::vector<std::pair<std::int64_t, std::int64_t>>& current,
        const MatchIntegerIntervalIndex& covered);
    static void insertMatchIntegerInterval(
        MatchIntegerIntervalIndex& covered,
        std::pair<std::int64_t, std::int64_t> interval);
    ast::AstNode* lowerMatchStatement(MatchFrame& frame);
    void reportEmptyMatch(const MatchFrame& frame);
    std::string buildMatchCoverage(const std::vector<MatchArm>& arms);
    void appendMatchArmCoverage(std::string& coverage, const MatchArm& arm);
    static void appendMatchListCoverage(
        std::string& coverage,
        std::vector<std::pair<std::size_t, bool>>& lengths,
        bool partial);
    static void appendMatchCaseCoverage(
        std::string& coverage,
        std::vector<std::string>& cases,
        bool partial);
    void appendMatchGoverningState(
        ast::AstNode& lowered,
        const MatchFrame& frame,
        const std::string& temporary);
    ast::AstNode* makeMatchSelectedDeclaration(
        SourceRange range,
        const std::string& temporary);
    ast::AstNode* lowerMatchArm(
        const MatchArm& arm,
        SourceRange matchRange,
        const std::string& temporary);
    ast::AstNode* makeMatchSelectedBody(
        const MatchArm& arm,
        const std::string& temporary);
    ast::AstNode* wrapMatchGuard(const MatchArm& arm, ast::AstNode* selectedBody);
    ast::AstNode* makeMatchNotSelected(
        SourceRange range,
        const std::string& temporary);
    ast::AstNode* parseCatchHeader(std::size_t beginIndex);
    void startTry(const Work& work);
    void continueTry(const Work& work);
    void finishTryBlock(const Work& work);
    void finishTryCatchFilter(const Work& work);
    void scheduleTryCatchBody(std::size_t frameIndex);
    void finishTryCatchBlock(const Work& work);
    void attachPendingCatch(TryFrame& frame, ast::AstNode* body);
    void scheduleTryFinally(std::size_t frameIndex);
    void finishTryFinallyBlock(const Work& work);
    void completeTry(TryFrame& frame);
    void startStandaloneCatch(const Work& work);
    void finishStandaloneCatchFilter(const Work& work);
    void scheduleStandaloneCatchBody(std::size_t frameIndex);
    void finishStandaloneCatchBlock(const Work& work);
    void startUsing(const Work& work);
    void finishUsingInitializer(const Work& work);
    void scheduleUsingBody(std::size_t frameIndex);
    void finishUsingBody(const Work& work);
    void completeUsing(UsingFrame& frame, ast::AstNode* body);
    void startDefer(const Work& work);
    void parseDeferCaptures(DeferFrame& frame);
    void parseDeferCapture(
        DeferFrame& frame,
        std::unordered_set<std::string>& names);
    void scheduleDeferBody(std::size_t frameIndex);
    void finishDeferBody(const Work& work);
    void configureDeferBody(const DeferFrame& frame, ast::AstNode* body);
    ast::TypeSyntax* makeDeferVarType(SourceRange range);
    ast::AstNode* makeDeferMoveExpression(SourceRange range, std::string name);
    void addDeferCaptureLowering(
        const DeferCapture& capture,
        std::size_t deferOrdinal,
        ast::AstNode* registration,
        ast::AstNode* cleanup);
    ast::AstNode* lowerDeferRegistration(DeferFrame& frame, ast::AstNode* body);
    void startFloatingPointScope(const Work& work);
    void finishFloatingPointScope(const Work& work);
    void startUnsafeScope(const Work& work);
    void finishUnsafeScope(const Work& work);
    void startInlineAssembly(const Work& work);
    void continueInlineAssembly(const Work& work);
    void scheduleInlineAssemblyOperand(
        std::size_t frameIndex,
        bool output,
        std::size_t operandBegin);
    void finishInlineAssemblyOutput(const Work& work);
    void finishInlineAssemblyInput(const Work& work);
    void appendInlineAssemblyOperand(
        InlineAssemblyFrame& frame,
        ast::AstNode* value,
        bool output,
        std::size_t operandBegin);
    void completeInlineAssembly(InlineAssemblyFrame& frame);
    void startComprehensionYield(const Work& work);
    void finishComprehensionYield(const Work& work);

    Parser& parser_;
    ParseContinuationCoordinator* previous_ = nullptr;
    std::deque<ResultSlot> resultSlots_;
    std::deque<ExpressionFrame> expressionFrames_;
    std::deque<BlockFrame> blockFrames_;
    std::deque<VariableFrame> variableFrames_;
    std::deque<AggregateFrame> aggregateFrames_;
    std::deque<IfFrame> ifFrames_;
    std::deque<WhileFrame> whileFrames_;
    std::deque<DoWhileFrame> doWhileFrames_;
    std::deque<ForFrame> forFrames_;
    std::deque<ForExpressionFrame> forExpressionFrames_;
    std::deque<ForeachFrame> foreachFrames_;
    std::deque<ConditionalPatternFrame> conditionalPatternFrames_;
    std::deque<SwitchFrame> switchFrames_;
    std::deque<MatchFrame> matchFrames_;
    std::deque<TryFrame> tryFrames_;
    std::deque<CatchFrame> catchFrames_;
    std::deque<UsingFrame> usingFrames_;
    std::deque<DeferFrame> deferFrames_;
    std::deque<FloatingPointScopeFrame> floatingPointScopeFrames_;
    std::deque<UnsafeScopeFrame> unsafeScopeFrames_;
    std::deque<InlineAssemblyFrame> inlineAssemblyFrames_;
    std::deque<ComprehensionYieldFrame> comprehensionYieldFrames_;
    std::vector<Work> work_;
};
}
