#include "nativesharp/Parser/Parser.h"

#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;

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
        FinishDeferBody
    };

    enum class StatementKind : std::uint8_t
    {
        Direct,
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
        Switch,
        Match,
        Try,
        Using,
        Defer
    };

    struct ResultSlot final
    {
        ast::AstNode* value = nullptr;
        bool ready = false;
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
        TokenKind tokenKind = TokenKind::Invalid;
        bool flag = false;
        bool secondaryFlag = false;
        std::string text;
    };

    std::size_t request(ContinuationRequestKind kind);
    std::size_t makeResultSlot();
    void drain(std::size_t resultSlot);
    void execute(Work work);
    void complete(std::size_t resultSlot, ast::AstNode* value);
    ast::AstNode* childValue(const Work& work) const;

    void startExpression(const Work& work);
    void startConditionalExpression(const Work& work);
    void finishLambdaBlock(const Work& work);
    void startBlock(const Work& work);
    void continueBlock(const Work& work);
    void appendBlockStatement(const Work& work);
    void finishBlock(BlockFrame& frame);

    StatementKind classifyStatement() const;
    bool scopedStatementStarts() const noexcept;
    bool controlStatementStarts() const noexcept;
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

    Parser& parser_;
    ParseContinuationCoordinator* previous_ = nullptr;
    std::deque<ResultSlot> resultSlots_;
    std::deque<BlockFrame> blockFrames_;
    std::deque<VariableFrame> variableFrames_;
    std::deque<AggregateFrame> aggregateFrames_;
    std::deque<IfFrame> ifFrames_;
    std::deque<WhileFrame> whileFrames_;
    std::deque<DoWhileFrame> doWhileFrames_;
    std::deque<ForFrame> forFrames_;
    std::deque<ForExpressionFrame> forExpressionFrames_;
    std::deque<ForeachFrame> foreachFrames_;
    std::deque<SwitchFrame> switchFrames_;
    std::deque<MatchFrame> matchFrames_;
    std::deque<TryFrame> tryFrames_;
    std::deque<CatchFrame> catchFrames_;
    std::deque<UsingFrame> usingFrames_;
    std::deque<DeferFrame> deferFrames_;
    std::vector<Work> work_;
};

// Installs one non-owning coordinator for the parser invocation; restoring the previous pointer
// permits bounded fallback reentry without changing Parser ownership or its public ABI.
Parser::ParseContinuationCoordinator::ParseContinuationCoordinator(Parser& parser) noexcept
    : parser_(parser), previous_(parser.activeContinuationCoordinator_)
{
    parser_.activeContinuationCoordinator_ = this;
}

// Restores the prior coordinator when the outer parse request finishes.
Parser::ParseContinuationCoordinator::~ParseContinuationCoordinator() noexcept
{
    parser_.activeContinuationCoordinator_ = previous_;
}

// Schedules one facade request and drains only until its stable result slot is ready.
ast::AstNode* Parser::ParseContinuationCoordinator::parse(ContinuationRequestKind kind)
{
    const std::size_t resultSlot = request(kind);
    drain(resultSlot);
    return resultSlots_[resultSlot].value;
}

// Maps the public facade kind to an internal work item in constant time.
std::size_t Parser::ParseContinuationCoordinator::request(ContinuationRequestKind kind)
{
    const std::size_t resultSlot = makeResultSlot();
    WorkKind workKind = WorkKind::Expression;
    if (kind == ContinuationRequestKind::Block) workKind = WorkKind::Block;
    else if (kind == ContinuationRequestKind::Statement) workKind = WorkKind::Statement;
    else if (kind == ContinuationRequestKind::CatchClause) workKind = WorkKind::CatchClause;
    work_.push_back({workKind, resultSlot});
    return resultSlot;
}

// Appends a stable result cell whose index remains valid across deque growth.
std::size_t Parser::ParseContinuationCoordinator::makeResultSlot()
{
    const std::size_t resultSlot = resultSlots_.size();
    resultSlots_.push_back({});
    return resultSlot;
}

// Executes the explicit LIFO worklist until the requested result is available.
void Parser::ParseContinuationCoordinator::drain(std::size_t resultSlot)
{
    while (!resultSlots_[resultSlot].ready)
    {
        assert(!work_.empty());
        const Work work = work_.back();
        work_.pop_back();
        execute(work);
    }
}

// Dispatches one bounded continuation step without recursive product traversal.
void Parser::ParseContinuationCoordinator::execute(Work work)
{
    switch (work.kind)
    {
    case WorkKind::Expression: startExpression(work); return;
    case WorkKind::ConditionalExpression: startConditionalExpression(work); return;
    case WorkKind::Block: startBlock(work); return;
    case WorkKind::ContinueBlock: continueBlock(work); return;
    case WorkKind::AppendBlockStatement: appendBlockStatement(work); return;
    case WorkKind::Statement: startStatement(work); return;
    case WorkKind::FinishLambdaBlock: finishLambdaBlock(work); return;
    case WorkKind::FinishExpressionStatement: finishExpressionStatement(work); return;
    case WorkKind::FinishReturn: finishReturn(work); return;
    case WorkKind::FinishThrow: finishThrow(work); return;
    case WorkKind::FinishJump: finishJump(work); return;
    case WorkKind::FinishYieldReturn: finishYieldReturn(work); return;
    case WorkKind::FinishStaticAssert: finishStaticAssert(work); return;
    case WorkKind::FinishDeconstruction: finishDeconstruction(work); return;
    case WorkKind::ContinueVariableDeclaration: continueVariableDeclaration(work); return;
    case WorkKind::FinishVariableInitializer: finishVariableInitializer(work); return;
    case WorkKind::Aggregate: startAggregate(work); return;
    case WorkKind::ContinueAggregate: continueAggregate(work); return;
    case WorkKind::FinishAggregateElement: finishAggregateElement(work); return;
    case WorkKind::FinishLocalFunction: finishLocalFunction(work); return;
    case WorkKind::Embedded: startEmbedded(work); return;
    case WorkKind::FinishEmbedded: finishEmbedded(work); return;
    case WorkKind::FinishIfCondition: finishIfCondition(work); return;
    case WorkKind::FinishIfThen: finishIfThen(work); return;
    case WorkKind::FinishIfElse: finishIfElse(work); return;
    case WorkKind::FinishWhileCondition: finishWhileCondition(work); return;
    case WorkKind::FinishWhileBody: finishWhileBody(work); return;
    case WorkKind::FinishDoBody: finishDoBody(work); return;
    case WorkKind::FinishDoCondition: finishDoCondition(work); return;
    case WorkKind::FinishForInitializer: finishForInitializer(work); return;
    case WorkKind::FinishForCondition: finishForCondition(work); return;
    case WorkKind::FinishForIncrement: finishForIncrement(work); return;
    case WorkKind::FinishForBody: finishForBody(work); return;
    case WorkKind::ContinueForExpressionList: continueForExpressionList(work); return;
    case WorkKind::FinishForExpression: finishForExpression(work); return;
    case WorkKind::FinishForeachCollection: finishForeachCollection(work); return;
    case WorkKind::FinishForeachProbe: finishForeachProbe(work); return;
    case WorkKind::FinishForeachBody: finishForeachBody(work); return;
    case WorkKind::FinishSwitchGoverning: finishSwitchGoverning(work); return;
    case WorkKind::ContinueSwitch: continueSwitch(work); return;
    case WorkKind::ContinueSwitchLabels: continueSwitchLabels(work); return;
    case WorkKind::FinishSwitchLabel: finishSwitchLabel(work); return;
    case WorkKind::ContinueSwitchSection: continueSwitchSection(work); return;
    case WorkKind::AppendSwitchStatement: appendSwitchStatement(work); return;
    case WorkKind::FinishMatchGoverning: finishMatchGoverning(work); return;
    case WorkKind::ContinueMatch: continueMatch(work); return;
    case WorkKind::FinishMatchGuard: finishMatchGuard(work); return;
    case WorkKind::FinishMatchArm: finishMatchArm(work); return;
    case WorkKind::Try: startTry(work); return;
    case WorkKind::ContinueTry: continueTry(work); return;
    case WorkKind::FinishTryBlock: finishTryBlock(work); return;
    case WorkKind::FinishTryCatchFilter: finishTryCatchFilter(work); return;
    case WorkKind::FinishTryCatchBlock: finishTryCatchBlock(work); return;
    case WorkKind::FinishTryFinallyBlock: finishTryFinallyBlock(work); return;
    case WorkKind::CatchClause: startStandaloneCatch(work); return;
    case WorkKind::FinishStandaloneCatchFilter: finishStandaloneCatchFilter(work); return;
    case WorkKind::FinishStandaloneCatchBlock: finishStandaloneCatchBlock(work); return;
    case WorkKind::FinishUsingInitializer: finishUsingInitializer(work); return;
    case WorkKind::FinishUsingBody: finishUsingBody(work); return;
    case WorkKind::FinishDeferBody: finishDeferBody(work); return;
    }
}

// Publishes a result exactly once for its waiting continuation.
void Parser::ParseContinuationCoordinator::complete(
    std::size_t resultSlot,
    ast::AstNode* value)
{
    resultSlots_[resultSlot].value = value;
    resultSlots_[resultSlot].ready = true;
}

// Reads an optional completed child; a missing child represents a statement without a value.
ast::AstNode* Parser::ParseContinuationCoordinator::childValue(const Work& work) const
{
    if (work.childSlot == noSlot) return nullptr;
    assert(resultSlots_[work.childSlot].ready);
    return resultSlots_[work.childSlot].value;
}

// Extracts only block-bodied lambdas; all other expressions retain the canonical parser path.
void Parser::ParseContinuationCoordinator::startExpression(const Work& work)
{
    if (!parser_.lambdaBlockBodyStartsHere())
    {
        complete(work.resultSlot, parser_.parseExpressionImpl());
        return;
    }
    ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
    const std::size_t bodySlot = makeResultSlot();
    Work finish{WorkKind::FinishLambdaBlock, work.resultSlot};
    finish.childSlot = bodySlot;
    finish.node = lambda;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, bodySlot});
}

// Preserves the historical guard grammar without widening it to assignment-expression.
void Parser::ParseContinuationCoordinator::startConditionalExpression(const Work& work)
{
    complete(work.resultSlot, parser_.parseConditionalExpression());
}

// Attaches the completed block with the canonical lambda flags and range update.
void Parser::ParseContinuationCoordinator::finishLambdaBlock(const Work& work)
{
    parser_.finishLambdaExpression(work.node, childValue(work), true);
    complete(work.resultSlot, work.node);
}

// Creates one lexical block frame and schedules its first bounded continuation.
void Parser::ParseContinuationCoordinator::startBlock(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    parser_.expect(TokenKind::LeftBrace, "'{'");
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, parser_.tokenRangeFrom(beginIndex));
    const std::size_t frameIndex = blockFrames_.size();
    blockFrames_.push_back({beginIndex, parser_.index_, work.resultSlot, block, block});
    Work next{WorkKind::ContinueBlock, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Either closes the block or schedules exactly one statement plus its append continuation.
void Parser::ParseContinuationCoordinator::continueBlock(const Work& work)
{
    BlockFrame& frame = blockFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        finishBlock(frame);
        return;
    }
    frame.beforeIndex = parser_.index_;
    const std::size_t statementSlot = makeResultSlot();
    Work append{WorkKind::AppendBlockStatement, frame.resultSlot};
    append.frameIndex = work.frameIndex;
    append.childSlot = statementSlot;
    work_.push_back(std::move(append));
    work_.push_back({WorkKind::Statement, statementSlot});
}

// Preserves defer destination rewriting, inline expansions, and the historical progress guard.
void Parser::ParseContinuationCoordinator::appendBlockStatement(const Work& work)
{
    BlockFrame& frame = blockFrames_[work.frameIndex];
    parser_.appendParsedStatement(frame.destination, childValue(work));
    if (parser_.index_ == frame.beforeIndex) parser_.consume();
    Work next{WorkKind::ContinueBlock, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(std::move(next));
}

// Consumes the closing delimiter, fixes the full source range, and publishes the block.
void Parser::ParseContinuationCoordinator::finishBlock(BlockFrame& frame)
{
    parser_.expect(TokenKind::RightBrace, "'}'");
    frame.block->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.block);
}

// Mirrors the side-effect-free prefix test used before the scoped-statement fallback.
bool Parser::ParseContinuationCoordinator::scopedStatementStarts() const noexcept
{
    return (parser_.atContextualKeyword("fp") && parser_.peek().kind() == TokenKind::LeftParen) ||
        (parser_.atContextualKeyword("unsafe") &&
         (parser_.peek().kind() == TokenKind::LeftBrace ||
          parser_.peek().kind() == TokenKind::LeftParen)) ||
        (parser_.atContextualKeyword("asm") &&
         (parser_.peek().kind() == TokenKind::LeftBrace ||
          parser_.peek().kind() == TokenKind::LeftParen));
}

// Recognizes control-family prefixes without consuming tokens or changing their parser.
bool Parser::ParseContinuationCoordinator::controlStatementStarts() const noexcept
{
    if (parser_.atContextualKeyword("let")) return true;
    if ((parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile)) &&
        parser_.peek().kind() == TokenKind::LeftParen &&
        parser_.peek(2).kind() == TokenKind::Identifier &&
        parser_.peek(2).lexeme() == "let") return true;
    return parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile) ||
        parser_.at(TokenKind::KwDo) || parser_.at(TokenKind::KwFor) ||
        parser_.at(TokenKind::KwForeach) || parser_.at(TokenKind::KwSwitch) ||
        parser_.startsParenthesizedBlock("match");
}

// Distinguishes ordinary if from the conditional-pattern form owned by R03C2C2E.
bool Parser::ParseContinuationCoordinator::ordinaryIfStarts() const noexcept
{
    const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
        parser_.peek(2).kind() == TokenKind::Identifier &&
        parser_.peek(2).lexeme() == "let";
    return parser_.at(TokenKind::KwIf) && !conditional;
}

// Distinguishes ordinary while from the conditional-pattern form owned by R03C2C2E.
bool Parser::ParseContinuationCoordinator::ordinaryWhileStarts() const noexcept
{
    const bool conditional = parser_.peek().kind() == TokenKind::LeftParen &&
        parser_.peek(2).kind() == TokenKind::Identifier &&
        parser_.peek(2).lexeme() == "let";
    return parser_.at(TokenKind::KwWhile) && !conditional;
}

// Mirrors variable-declaration disambiguation after C-owned special forms are classified.
bool Parser::ParseContinuationCoordinator::declarationStatementStarts() const noexcept
{
    if (parser_.atContextualKeyword("pin") &&
        (parser_.peek().kind() == TokenKind::KwRaii ||
         (parser_.peek().kind() == TokenKind::Identifier && parser_.peek().lexeme() == "ref")))
        return true;
    if (parser_.at(TokenKind::KwRaii) || parser_.at(TokenKind::KwConst) ||
        parser_.at(TokenKind::KwVolatile) || parser_.at(TokenKind::KwVar)) return true;
    if (parser_.atContextualKeyword("ref") &&
        (parser_.canStartTypeAt(parser_.index_ + 1) ||
         parser_.peek().kind() == TokenKind::KwVar || parser_.peek().kind() == TokenKind::KwConst ||
         parser_.peek().kind() == TokenKind::KwRaii)) return true;
    return parser_.looksLikeVariableDeclaration() && !parser_.looksLikeClosedGenericTypeReference();
}

// Reproduces the canonical dispatcher order while isolating the B and C worklist families.
Parser::ParseContinuationCoordinator::StatementKind
Parser::ParseContinuationCoordinator::classifyStatement() const
{
    if (parser_.at(TokenKind::LeftBrace)) return StatementKind::Block;
    if (ordinaryIfStarts()) return StatementKind::If;
    if (ordinaryWhileStarts()) return StatementKind::While;
    if (parser_.at(TokenKind::KwDo)) return StatementKind::DoWhile;
    if (parser_.at(TokenKind::KwFor)) return StatementKind::For;
    if (parser_.at(TokenKind::KwForeach)) return StatementKind::Foreach;
    if (parser_.at(TokenKind::KwSwitch)) return StatementKind::Switch;
    if (parser_.startsParenthesizedBlock("match")) return StatementKind::Match;
    if (parser_.at(TokenKind::KwTry)) return StatementKind::Try;
    if (parser_.at(TokenKind::KwUsing)) return StatementKind::Using;
    if (scopedStatementStarts() || controlStatementStarts()) return StatementKind::Direct;
    if (parser_.atContextualKeyword("yield"))
        return parser_.comprehensionContexts_.empty() ? StatementKind::Yield : StatementKind::Direct;
    if (parser_.at(TokenKind::KwStatic))
        return parser_.looksLikeLocalFunction()
            ? StatementKind::LocalFunction : StatementKind::StaticLocal;
    if (parser_.at(TokenKind::KwBreak) || parser_.at(TokenKind::KwContinue)) return StatementKind::Jump;
    if (parser_.atContextualKeyword("must_tail")) return StatementKind::MustTailReturn;
    if (parser_.at(TokenKind::KwReturn)) return StatementKind::Return;
    if (parser_.atContextualKeyword("defer")) return StatementKind::Defer;
    if (parser_.at(TokenKind::KwThrow)) return StatementKind::Throw;
    if (parser_.at(TokenKind::KwStaticAssert)) return StatementKind::StaticAssert;
    if (parser_.at(TokenKind::KwVar) && parser_.peek().kind() == TokenKind::LeftParen)
        return StatementKind::Deconstruction;
    if (parser_.looksLikeLocalFunction()) return StatementKind::LocalFunction;
    if (parser_.atContextualKeyword("await")) return StatementKind::AwaitExpression;
    if (declarationStatementStarts()) return StatementKind::VariableDeclaration;
    return StatementKind::Expression;
}

// Schedules one simple statement or delegates an untouched C-F family atomically.
void Parser::ParseContinuationCoordinator::startStatement(const Work& work)
{
    switch (classifyStatement())
    {
    case StatementKind::Direct: complete(work.resultSlot, parser_.parseStatementImpl()); return;
    case StatementKind::Block: work_.push_back({WorkKind::Block, work.resultSlot}); return;
    case StatementKind::Expression: startExpressionStatement(work); return;
    case StatementKind::Return: startReturn(work, false); return;
    case StatementKind::MustTailReturn: startMustTailReturn(work); return;
    case StatementKind::Throw: startThrow(work); return;
    case StatementKind::Jump: startJump(work); return;
    case StatementKind::Yield: startYield(work); return;
    case StatementKind::StaticAssert: startStaticAssert(work); return;
    case StatementKind::AwaitExpression: startAwaitExpression(work); return;
    case StatementKind::Deconstruction: startDeconstruction(work); return;
    case StatementKind::VariableDeclaration: startVariableDeclaration(work, false); return;
    case StatementKind::StaticLocal: startVariableDeclaration(work, true); return;
    case StatementKind::LocalFunction: startLocalFunction(work); return;
    case StatementKind::If: startIf(work); return;
    case StatementKind::While: startWhile(work); return;
    case StatementKind::DoWhile: startDoWhile(work); return;
    case StatementKind::For: startFor(work); return;
    case StatementKind::Foreach: startForeach(work); return;
    case StatementKind::Switch: startSwitch(work); return;
    case StatementKind::Match: startMatch(work); return;
    case StatementKind::Try: startTry(work); return;
    case StatementKind::Using: startUsing(work); return;
    case StatementKind::Defer: startDefer(work); return;
    }
}

// Preserves must_tail validation and then shares the ordinary return continuation.
void Parser::ParseContinuationCoordinator::startMustTailReturn(const Work& work)
{
    const SourceRange attributeRange = parser_.consume().range();
    if (!parser_.at(TokenKind::KwReturn))
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "must_tail must be followed by a return statement");
        parser_.synchronizeStatement();
        complete(work.resultSlot, parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, attributeRange));
        return;
    }
    startReturn(work, true);
}

// Records the statement start and schedules its expression as a stable child result.
void Parser::ParseContinuationCoordinator::startExpressionStatement(const Work& work)
{
    const std::size_t childSlot = makeResultSlot();
    Work finish{WorkKind::FinishExpressionStatement, work.resultSlot};
    finish.childSlot = childSlot;
    finish.beginIndex = parser_.index_;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Expression, childSlot});
}

// Builds the statement before consuming ';' and then extends its range, matching legacy order.
void Parser::ParseContinuationCoordinator::finishExpressionStatement(const Work& work)
{
    ast::AstNode* expression = childValue(work);
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ExpressionStatement,
        expression != nullptr ? expression->range() : parser_.current().range());
    statement->setFirst(expression);
    parser_.expect(TokenKind::Semicolon, work.text.empty() ? "';'" : work.text);
    statement->setRange(parser_.tokenRangeFrom(work.beginIndex));
    complete(work.resultSlot, statement);
}

// Consumes return/ref syntax and schedules only the optional value expression.
void Parser::ParseContinuationCoordinator::startReturn(const Work& work, bool mustTail)
{
    Work finish{WorkKind::FinishReturn, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    finish.flag = parser_.atContextualKeyword("ref") &&
        parser_.peek().kind() != TokenKind::Semicolon;
    if (finish.flag) parser_.consume();
    finish.secondaryFlag = mustTail;
    if (parser_.at(TokenKind::Semicolon))
    {
        finishReturn(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Completes the return after ';', retaining reference and must-tail flags.
void Parser::ParseContinuationCoordinator::finishReturn(const Work& work)
{
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ReturnStatement, parser_.tokenRangeFrom(work.beginIndex));
    node->setFirst(childValue(work));
    node->setReference(work.flag);
    node->setFlag(work.secondaryFlag);
    complete(work.resultSlot, node);
}

// Consumes throw and schedules its optional value expression without native recursion.
void Parser::ParseContinuationCoordinator::startThrow(const Work& work)
{
    Work finish{WorkKind::FinishThrow, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    if (parser_.at(TokenKind::Semicolon))
    {
        finishThrow(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Completes the throw after its contractual semicolon and full range.
void Parser::ParseContinuationCoordinator::finishThrow(const Work& work)
{
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ThrowStatement, parser_.tokenRangeFrom(work.beginIndex));
    node->setFirst(childValue(work));
    complete(work.resultSlot, node);
}

// Parses jump labels eagerly and schedules only a break value when one exists.
void Parser::ParseContinuationCoordinator::startJump(const Work& work)
{
    Work finish{WorkKind::FinishJump, work.resultSlot};
    finish.beginIndex = parser_.index_;
    finish.tokenKind = parser_.consume().kind();
    if (finish.tokenKind == TokenKind::KwBreak && parser_.at(TokenKind::Identifier) &&
        parser_.peek().kind() == TokenKind::Colon)
    {
        finish.text = std::string(parser_.consume().lexeme());
        parser_.consume();
    }
    else if (finish.tokenKind == TokenKind::KwContinue && parser_.at(TokenKind::Identifier))
        finish.text = std::string(parser_.consume().lexeme());
    if (finish.tokenKind != TokenKind::KwBreak || parser_.at(TokenKind::Semicolon))
    {
        finishJump(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Builds break or continue after ';', preserving label text and optional break value.
void Parser::ParseContinuationCoordinator::finishJump(const Work& work)
{
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* exit = parser_.arena_.make<ast::AstNode>(
        work.tokenKind == TokenKind::KwBreak
            ? ast::AstNodeKind::BreakStatement : ast::AstNodeKind::ContinueStatement,
        parser_.tokenRangeFrom(work.beginIndex));
    exit->setFirst(childValue(work));
    exit->setText(work.text);
    complete(work.resultSlot, exit);
}

// Handles non-comprehension yield forms and schedules only yield-return's expression.
void Parser::ParseContinuationCoordinator::startYield(const Work& work)
{
    Work finish{WorkKind::FinishYieldReturn, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    if (parser_.at(TokenKind::KwReturn))
    {
        parser_.consume();
        finish.childSlot = makeResultSlot();
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, finish.childSlot});
        return;
    }
    ast::AstNodeKind kind = ast::AstNodeKind::SuspendStatement;
    std::string_view semicolonDescription = "';' after 'yield'";
    if (parser_.at(TokenKind::KwBreak))
    {
        parser_.consume();
        kind = ast::AstNodeKind::YieldBreakStatement;
        semicolonDescription = "';' after 'yield return'/'yield break'";
    }
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        kind, parser_.tokenRangeFrom(finish.beginIndex));
    parser_.expect(TokenKind::Semicolon, semicolonDescription);
    complete(work.resultSlot, statement);
}

// Builds yield-return before consuming ';', preserving the legacy range boundary.
void Parser::ParseContinuationCoordinator::finishYieldReturn(const Work& work)
{
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::YieldReturnStatement, parser_.tokenRangeFrom(work.beginIndex));
    statement->setFirst(childValue(work));
    parser_.expect(TokenKind::Semicolon, "';' after 'yield return'/'yield break'");
    complete(work.resultSlot, statement);
}

// Consumes the static_assert prefix and schedules its condition expression.
void Parser::ParseContinuationCoordinator::startStaticAssert(const Work& work)
{
    Work finish{WorkKind::FinishStaticAssert, work.resultSlot};
    finish.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Parses the optional message and delimiters, then publishes the canonical statement node.
void Parser::ParseContinuationCoordinator::finishStaticAssert(const Work& work)
{
    std::string message;
    if (parser_.consumeIf(TokenKind::Comma))
    {
        if (parser_.at(TokenKind::StringLiteral))
            message = std::get<std::string>(parser_.consume().value());
        else
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "static_assert message must be a string literal");
    }
    parser_.expect(TokenKind::RightParen, "')'");
    parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* statement = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::StaticAssertStatement, parser_.tokenRangeFrom(work.beginIndex));
    statement->setFirst(childValue(work));
    statement->setText(std::move(message));
    complete(work.resultSlot, statement);
}

// Routes contextual await through the expression worklist while preserving its diagnostic text.
void Parser::ParseContinuationCoordinator::startAwaitExpression(const Work& work)
{
    const std::size_t childSlot = makeResultSlot();
    Work finish{WorkKind::FinishExpressionStatement, work.resultSlot};
    finish.childSlot = childSlot;
    finish.beginIndex = parser_.index_;
    finish.text = "';' after await expression";
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Expression, childSlot});
}

// Parses only the binding pattern synchronously, then schedules the recursive RHS.
void Parser::ParseContinuationCoordinator::startDeconstruction(const Work& work)
{
    Work finish{WorkKind::FinishDeconstruction, work.resultSlot};
    finish.beginIndex = parser_.index_;
    const SourceRange featureRange = parser_.consume().range();
    ast::AstNode* pattern = parser_.parseDeconstructionPattern();
    finish.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DeconstructionDeclarationStatement, featureRange);
    if (pattern != nullptr && pattern->kind() == ast::AstNodeKind::TupleExpression)
        for (ast::AstNode* binding : pattern->children()) finish.node->addChild(binding);
    parser_.expect(TokenKind::Equal, "'=' after deconstruction bindings");
    finish.childSlot = makeResultSlot();
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, finish.childSlot});
}

// Attaches the completed assignment expression and preserves delimiter/range ordering.
void Parser::ParseContinuationCoordinator::finishDeconstruction(const Work& work)
{
    work.node->setFirst(childValue(work));
    parser_.expect(TokenKind::Semicolon, "';'");
    work.node->setRange(parser_.tokenRangeFrom(work.beginIndex));
    complete(work.resultSlot, work.node);
}

// Validates static-local syntax before sharing the ordinary variable-declaration machine.
void Parser::ParseContinuationCoordinator::startVariableDeclaration(
    const Work& work,
    bool isStaticLocal,
    bool expectSemicolon)
{
    const std::size_t beginIndex = parser_.index_;
    if (isStaticLocal)
    {
        const SourceRange staticRange = parser_.consume().range();
        if (!declarationStatementStarts())
        {
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                staticRange,
                "static statement must declare a local variable");
            parser_.synchronizeStatement();
            complete(work.resultSlot, parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, staticRange));
            return;
        }
    }
    const std::size_t frameIndex = variableFrames_.size();
    variableFrames_.push_back({beginIndex, work.resultSlot});
    variableFrames_.back().isStaticLocal = isStaticLocal;
    variableFrames_.back().expectSemicolon = expectSemicolon;
    parseVariablePrefix(variableFrames_.back());
    Work next{WorkKind::ContinueVariableDeclaration, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Consumes declaration modifiers and type exactly once for every declarator in the frame.
void Parser::ParseContinuationCoordinator::parseVariablePrefix(VariableFrame& frame)
{
    const bool pinnedReference = parser_.atContextualKeyword("pin") &&
        parser_.peek().kind() == TokenKind::Identifier && parser_.peek().lexeme() == "ref";
    frame.isPinned = pinnedReference ||
        (parser_.atContextualKeyword("pin") && parser_.peek().kind() == TokenKind::KwRaii);
    if (frame.isPinned) parser_.consume();
    frame.isReferenceLocal = parser_.atContextualKeyword("ref") &&
        (parser_.canStartTypeAt(parser_.index_ + 1) || parser_.peek().kind() == TokenKind::KwVar ||
         parser_.peek().kind() == TokenKind::KwConst || parser_.peek().kind() == TokenKind::KwRaii);
    if (frame.isReferenceLocal) parser_.consume();
    frame.isRaii = parser_.consumeIf(TokenKind::KwRaii);
    frame.isVolatile = parser_.consumeIf(TokenKind::KwVolatile);
    frame.isConst = parser_.consumeIf(TokenKind::KwConst);
    frame.isScope = parser_.atContextualKeyword("scope") &&
        (parser_.canStartTypeAt(parser_.index_ + 1) || parser_.peek().kind() == TokenKind::KwVar);
    if (frame.isScope) parser_.consume();
    if (frame.isRaii && frame.isConst)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            parser_.tokens_[frame.beginIndex].range(),
            "raii locals cannot also be const");
    if (!parser_.at(TokenKind::KwVar)) { frame.type = parser_.parseType(); return; }
    if (frame.isReferenceLocal)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "reference locals require an explicit element type");
    const lexer::Token& varToken = parser_.consume();
    frame.type = parser_.arena_.make<ast::TypeSyntax>(
        varToken.range(), TokenKind::KwVar, std::string("var"));
}

// Creates one declarator without consuming its optional initializer.
ast::AstNode* Parser::ParseContinuationCoordinator::makeVariableDeclarator(
    VariableFrame& frame,
    bool cloneType)
{
    if (!parser_.at(TokenKind::Identifier))
    {
        parser_.expect(TokenKind::Identifier, "variable name");
        return nullptr;
    }
    const lexer::Token& name = parser_.consume();
    ast::AstNode* variable = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::VariableDeclarationStatement, name.range());
    variable->setTypeSyntax(cloneType ? parser_.cloneDeclarationType(frame.type) : frame.type);
    variable->setText(std::string(name.lexeme()));
    variable->setFlag(frame.isConst);
    if (frame.isRaii) variable->addModifier(TokenKind::KwRaii);
    if (frame.isVolatile) variable->addModifier(TokenKind::KwVolatile);
    if (frame.isReferenceLocal)
        variable->setAuxiliaryText(frame.isPinned ? "local:pinned-ref" : "local:ref");
    else if (frame.isPinned) variable->setAuxiliaryText("local:pinned");
    return variable;
}

// Advances one declarator and schedules only its recursive initializer when present.
void Parser::ParseContinuationCoordinator::continueVariableDeclaration(const Work& work)
{
    VariableFrame& frame = variableFrames_[work.frameIndex];
    const std::size_t beginIndex = parser_.index_;
    ast::AstNode* variable = makeVariableDeclarator(frame, frame.first != nullptr);
    if (variable == nullptr)
    {
        parser_.synchronizeStatement();
        if (frame.first == nullptr) complete(frame.resultSlot, nullptr);
        else finishVariableDeclaration(frame);
        return;
    }
    Work finish{WorkKind::FinishVariableInitializer, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.beginIndex = beginIndex;
    finish.node = variable;
    if (!parser_.consumeIf(TokenKind::Equal))
    {
        if (frame.isReferenceLocal)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "reference-local declaration requires '= ref <storage>'");
        else if (frame.isConst)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::ExpectedToken,
                diagnostics::DiagnosticSeverity::Error,
                parser_.current().range(),
                "const declaration requires an initializer");
        finishVariableInitializer(finish);
        return;
    }
    if (frame.isReferenceLocal)
        parser_.expectContextualKeyword("ref", "'ref' before the reference-local initializer");
    scheduleVariableInitializer(finish);
}

// Selects aggregate or ordinary expression parsing without adding a native frame.
void Parser::ParseContinuationCoordinator::scheduleVariableInitializer(
    const Work& finish)
{
    Work continuation = finish;
    continuation.childSlot = makeResultSlot();
    work_.push_back(continuation);
    work_.push_back({
        parser_.at(TokenKind::LeftBrace) ? WorkKind::Aggregate : WorkKind::Expression,
        continuation.childSlot});
}

// Attaches an optional initializer, fixes the declarator range, and advances the list.
void Parser::ParseContinuationCoordinator::finishVariableInitializer(const Work& work)
{
    work.node->setFirst(childValue(work));
    work.node->setRange(parser_.tokenRangeFrom(work.beginIndex));
    acceptVariableDeclarator(work.frameIndex, work.node);
}

// Preserves declaration-list construction, static/scope metadata, and source order.
void Parser::ParseContinuationCoordinator::acceptVariableDeclarator(
    std::size_t frameIndex,
    ast::AstNode* variable)
{
    VariableFrame& frame = variableFrames_[frameIndex];
    if (frame.isStaticLocal) variable->addModifier(TokenKind::KwStatic);
    if (frame.first == nullptr)
    {
        frame.first = variable;
        if (frame.isScope) variable->setAuxiliaryText("local:scope");
    }
    else
    {
        if (frame.declarationList == nullptr)
        {
            frame.declarationList = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::BlockStatement, frame.first->range());
            frame.declarationList->setAuxiliaryText("declaration-list");
            frame.declarationList->addChild(frame.first);
        }
        frame.declarationList->addChild(variable);
    }
    if (!parser_.consumeIf(TokenKind::Comma)) { finishVariableDeclaration(frame); return; }
    Work next{WorkKind::ContinueVariableDeclaration, frame.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Consumes the statement delimiter and publishes either one declarator or its list wrapper.
void Parser::ParseContinuationCoordinator::finishVariableDeclaration(VariableFrame& frame)
{
    if (frame.expectSemicolon) parser_.expect(TokenKind::Semicolon, "';'");
    ast::AstNode* result = frame.declarationList != nullptr
        ? frame.declarationList : frame.first;
    if (result != nullptr) result->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, result);
}

// Opens one aggregate frame and advances it through bounded element continuations.
void Parser::ParseContinuationCoordinator::startAggregate(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    const lexer::Token& open = parser_.consume();
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::AggregateInitializerExpression, open.range());
    const std::size_t frameIndex = aggregateFrames_.size();
    aggregateFrames_.push_back({beginIndex, work.resultSlot, node});
    Work next{WorkKind::ContinueAggregate, work.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(std::move(next));
}

// Schedules exactly one aggregate element while preserving spread/designator precedence.
void Parser::ParseContinuationCoordinator::continueAggregate(const Work& work)
{
    AggregateFrame& frame = aggregateFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        finishAggregate(frame);
        return;
    }
    Work finish{WorkKind::FinishAggregateElement, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    WorkKind childKind = parser_.at(TokenKind::LeftBrace)
        ? WorkKind::Aggregate : WorkKind::Expression;
    if (parser_.at(TokenKind::DotDot))
    {
        const SourceRange spread = parser_.consume().range();
        finish.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SpreadElementExpression, spread);
        finish.node->setTokenKind(TokenKind::DotDot);
        finish.tokenKind = TokenKind::DotDot;
        childKind = WorkKind::Expression;
    }
    else if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Equal)
    {
        finish.text = std::string(parser_.consume().lexeme());
        parser_.consume();
        childKind = parser_.at(TokenKind::LeftBrace)
            ? WorkKind::Aggregate : WorkKind::Expression;
    }
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({childKind, childSlot});
}

// Attaches one completed element and advances or closes the enclosing aggregate.
void Parser::ParseContinuationCoordinator::finishAggregateElement(const Work& work)
{
    AggregateFrame& frame = aggregateFrames_[work.frameIndex];
    ast::AstNode* element = childValue(work);
    if (work.tokenKind == TokenKind::DotDot)
    {
        work.node->setFirst(element);
        work.node->setRange(parser_.join(
            work.node->range(), element != nullptr ? element->range() : work.node->range()));
        element = work.node;
    }
    if (element != nullptr && !work.text.empty()) element->setArgumentName(work.text);
    if (element == nullptr) { finishAggregate(frame); return; }
    frame.node->addChild(element);
    if (!parser_.consumeIf(TokenKind::Comma)) { finishAggregate(frame); return; }
    Work next{WorkKind::ContinueAggregate, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(std::move(next));
}

// Closes one aggregate and publishes its exact token range.
void Parser::ParseContinuationCoordinator::finishAggregate(AggregateFrame& frame)
{
    parser_.expect(TokenKind::RightBrace, "'}'");
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.node);
}

// Parses a local-function header, deferring only its block body to the shared worklist.
void Parser::ParseContinuationCoordinator::startLocalFunction(const Work& work)
{
    const std::size_t beginIndex = parser_.index_;
    const bool isStaticLocal = parser_.at(TokenKind::KwStatic);
    if (isStaticLocal) parser_.consume();
    ast::AstNode* function = parser_.parseFunctionOrGlobalVariable(true);
    if (function == nullptr || function->kind() != ast::AstNodeKind::FunctionDeclaration ||
        !parser_.at(TokenKind::LeftBrace))
    {
        complete(work.resultSlot, parser_.lowerLocalFunctionDeclaration(
            function, isStaticLocal, beginIndex));
        return;
    }
    Work finish{WorkKind::FinishLocalFunction, work.resultSlot};
    finish.beginIndex = beginIndex;
    finish.node = function;
    finish.flag = isStaticLocal;
    finish.childSlot = makeResultSlot();
    const std::size_t bodySlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, bodySlot});
}

// Restores canonical body metadata before lowering the local function to its lambda declaration.
void Parser::ParseContinuationCoordinator::finishLocalFunction(const Work& work)
{
    ast::AstNode* const body = childValue(work);
    work.node->setFirst(body);
    if (body != nullptr && work.node->floatingPointMode() !=
            ast::FloatingPointModeSyntaxKind::Inherit)
        parser_.applyMemberFloatingPointMode(body, work.node->floatingPointMode());
    if (body != nullptr) work.node->setRange(parser_.join(work.node->range(), body->range()));
    complete(work.resultSlot, parser_.lowerLocalFunctionDeclaration(
        work.node, work.flag, work.beginIndex));
}

// Wraps one non-block embedded statement in the canonical block shape and preserves diagnostics.
ast::AstNode* Parser::ParseContinuationCoordinator::wrapEmbedded(ast::AstNode* statement)
{
    if (statement != nullptr &&
        (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement ||
         statement->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement ||
         (statement->kind() == ast::AstNodeKind::BlockStatement &&
          statement->auxiliaryText() == "conditional-pattern-inline")))
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            statement->range(),
            "a declaration used as an if or else body requires an explicit '{ ... }' block");
    const SourceRange range = statement != nullptr
        ? statement->range() : parser_.current().range();
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, range);
    if (statement != nullptr) block->addChild(statement);
    return block;
}

// Schedules one embedded body without invoking a nested facade or nested drain.
void Parser::ParseContinuationCoordinator::startEmbedded(const Work& work)
{
    if (parser_.at(TokenKind::LeftBrace))
    {
        work_.push_back({WorkKind::Block, work.resultSlot});
        return;
    }
    if (parser_.at(TokenKind::Semicolon))
    {
        complete(work.resultSlot, parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, parser_.consume().range()));
        return;
    }
    Work finish{WorkKind::FinishEmbedded, work.resultSlot};
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Statement, childSlot});
}

// Publishes the canonical block wrapper around one completed embedded statement.
void Parser::ParseContinuationCoordinator::finishEmbedded(const Work& work)
{
    complete(work.resultSlot, wrapEmbedded(childValue(work)));
}

// Selects the single declaration binding and reproduces its exact invalid-list diagnostic.
ast::AstNode* Parser::ParseContinuationCoordinator::conditionFromDeclaration(
    ast::AstNode* declaration)
{
    if (declaration == nullptr) return nullptr;
    const ast::AstNode* binding = declaration;
    if (declaration->kind() == ast::AstNodeKind::BlockStatement &&
        declaration->auxiliaryText() == "declaration-list")
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a condition declaration must declare exactly one local");
        if (declaration->children().empty()) return nullptr;
        binding = declaration->children().front();
    }
    return parser_.makeIdentifierExpression(binding->range(), binding->text());
}

// Opens an ordinary if header and schedules its condition through one stable child slot.
void Parser::ParseContinuationCoordinator::startIf(const Work& work)
{
    const std::size_t frameIndex = ifFrames_.size();
    ifFrames_.push_back({{}, work.resultSlot});
    IfFrame& frame = ifFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishIfCondition, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the if header and schedules exactly one embedded then body.
void Parser::ParseContinuationCoordinator::finishIfCondition(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    if (work.flag)
    {
        frame.state.conditionDeclaration = childValue(work);
        frame.state.condition = conditionFromDeclaration(frame.state.conditionDeclaration);
    }
    else frame.state.condition = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    Work finish{WorkKind::FinishIfThen, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Embedded, childSlot});
}

// Attaches the then body and preserves nearest-if binding for a following else clause.
void Parser::ParseContinuationCoordinator::finishIfThen(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    frame.state.thenBlock = childValue(work);
    if (!parser_.consumeIf(TokenKind::KwElse))
    {
        complete(frame.resultSlot, parser_.lowerIfStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishIfElse, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({parser_.at(TokenKind::KwIf)
        ? WorkKind::Statement : WorkKind::Embedded, childSlot});
}

// Publishes the completed ordinary if after its optional else subtree is ready.
void Parser::ParseContinuationCoordinator::finishIfElse(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerIfStatement(frame.state, childValue(work)));
}

// Opens an ordinary while header and schedules its condition without native reentry.
void Parser::ParseContinuationCoordinator::startWhile(const Work& work)
{
    const std::size_t frameIndex = whileFrames_.size();
    whileFrames_.push_back({{}, work.resultSlot});
    WhileFrame& frame = whileFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishWhileCondition, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the while header and requires the same explicit block body as the legacy path.
void Parser::ParseContinuationCoordinator::finishWhileCondition(const Work& work)
{
    WhileFrame& frame = whileFrames_[work.frameIndex];
    if (work.flag)
    {
        frame.state.conditionDeclaration = childValue(work);
        frame.state.condition = conditionFromDeclaration(frame.state.conditionDeclaration);
    }
    else frame.state.condition = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after while condition");
        complete(frame.resultSlot, parser_.lowerWhileStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishWhileBody, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed body to the canonical while lowering.
void Parser::ParseContinuationCoordinator::finishWhileBody(const Work& work)
{
    WhileFrame& frame = whileFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerWhileStatement(frame.state, childValue(work)));
}

// Opens a do-while and schedules its required block body on the shared worklist.
void Parser::ParseContinuationCoordinator::startDoWhile(const Work& work)
{
    const std::size_t frameIndex = doWhileFrames_.size();
    doWhileFrames_.push_back({{parser_.index_}, work.resultSlot, nullptr});
    parser_.consume();
    Work finish{WorkKind::FinishDoBody, work.resultSlot};
    finish.frameIndex = frameIndex;
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after 'do'");
        work_.push_back(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Consumes the trailing do-while prefix and schedules its condition expression.
void Parser::ParseContinuationCoordinator::finishDoBody(const Work& work)
{
    DoWhileFrame& frame = doWhileFrames_[work.frameIndex];
    frame.body = childValue(work);
    parser_.expect(TokenKind::KwWhile, "'while' after the do-loop body");
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishDoCondition, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Builds the do-while node after its condition and contractual trailing semicolon.
void Parser::ParseContinuationCoordinator::finishDoCondition(const Work& work)
{
    DoWhileFrame& frame = doWhileFrames_[work.frameIndex];
    parser_.expect(TokenKind::RightParen, "')'");
    parser_.expect(TokenKind::Semicolon, "';' after the do-while condition");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DoWhileStatement,
        parser_.tokenRangeFrom(frame.state.beginIndex));
    node->setFirst(childValue(work));
    node->setSecond(frame.body);
    complete(frame.resultSlot, node);
}

// Opens a for header and schedules only the first present clause.
void Parser::ParseContinuationCoordinator::startFor(const Work& work)
{
    const std::size_t frameIndex = forFrames_.size();
    forFrames_.push_back({{}, work.resultSlot});
    ForFrame& frame = forFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    if (parser_.at(TokenKind::Semicolon))
    {
        parser_.consume();
        scheduleForCondition(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForInitializer, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (parser_.at(TokenKind::KwRaii) || parser_.at(TokenKind::KwConst) ||
        parser_.at(TokenKind::KwVolatile) || parser_.at(TokenKind::KwVar) ||
        parser_.canStartTypeAt(parser_.index_))
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        startForExpressionList(childSlot, frame.state.beginIndex);
}

// Stores the completed initializer, consumes its delimiter, and advances to the condition.
void Parser::ParseContinuationCoordinator::finishForInitializer(const Work& work)
{
    ForFrame& frame = forFrames_[work.frameIndex];
    frame.state.initializer = childValue(work);
    parser_.expect(TokenKind::Semicolon, "';'");
    scheduleForCondition(work.frameIndex);
}

// Schedules the optional for condition or advances directly across its delimiter.
void Parser::ParseContinuationCoordinator::scheduleForCondition(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (parser_.at(TokenKind::Semicolon))
    {
        parser_.consume();
        scheduleForIncrement(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForCondition, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores the completed for condition, consumes ';', and advances to the increment clause.
void Parser::ParseContinuationCoordinator::finishForCondition(const Work& work)
{
    forFrames_[work.frameIndex].state.condition = childValue(work);
    parser_.expect(TokenKind::Semicolon, "';'");
    scheduleForIncrement(work.frameIndex);
}

// Schedules the optional increment expression list or advances directly to the body.
void Parser::ParseContinuationCoordinator::scheduleForIncrement(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (parser_.at(TokenKind::RightParen))
    {
        parser_.consume();
        scheduleForBody(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForIncrement, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    startForExpressionList(childSlot, frame.state.beginIndex);
}

// Stores the completed increment list and closes the parenthesized for header.
void Parser::ParseContinuationCoordinator::finishForIncrement(const Work& work)
{
    forFrames_[work.frameIndex].state.increment = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    scheduleForBody(work.frameIndex);
}

// Requires the canonical block body and schedules it without nested parser invocation.
void Parser::ParseContinuationCoordinator::scheduleForBody(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after for clause");
        complete(frame.resultSlot, parser_.lowerForStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishForBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed block to the canonical for lowering.
void Parser::ParseContinuationCoordinator::finishForBody(const Work& work)
{
    ForFrame& frame = forFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerForStatement(frame.state, childValue(work)));
}

// Creates one explicit frame for a comma-separated for-clause expression list.
void Parser::ParseContinuationCoordinator::startForExpressionList(
    std::size_t resultSlot,
    std::size_t beginIndex)
{
    const std::size_t frameIndex = forExpressionFrames_.size();
    forExpressionFrames_.push_back({beginIndex, resultSlot, nullptr, nullptr});
    Work next{WorkKind::ContinueForExpressionList, resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(next);
}

// Schedules exactly one expression from the current for-clause list.
void Parser::ParseContinuationCoordinator::continueForExpressionList(const Work& work)
{
    ForExpressionFrame& frame = forExpressionFrames_[work.frameIndex];
    Work finish{WorkKind::FinishForExpression, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Appends one expression and either publishes the list or schedules its next element.
void Parser::ParseContinuationCoordinator::finishForExpression(const Work& work)
{
    ForExpressionFrame& frame = forExpressionFrames_[work.frameIndex];
    ast::AstNode* expression = childValue(work);
    if (frame.first == nullptr) frame.first = expression;
    if (frame.sequence != nullptr && expression != nullptr)
        frame.sequence->addChild(parser_.makeExpressionStatement(expression));
    if (!parser_.at(TokenKind::Comma))
    {
        ast::AstNode* result = frame.sequence != nullptr ? frame.sequence : frame.first;
        if (frame.sequence != nullptr)
            frame.sequence->setRange(parser_.tokenRangeFrom(frame.beginIndex));
        complete(frame.resultSlot, result);
        return;
    }
    if (frame.sequence == nullptr)
    {
        frame.sequence = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            frame.first != nullptr ? frame.first->range() : parser_.current().range());
        if (frame.first != nullptr)
            frame.sequence->addChild(parser_.makeExpressionStatement(frame.first));
    }
    parser_.consume();
    Work next{WorkKind::ContinueForExpressionList, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Opens the foreach clause and schedules its real collection expression.
void Parser::ParseContinuationCoordinator::startForeach(const Work& work)
{
    const std::size_t frameIndex = foreachFrames_.size();
    foreachFrames_.push_back({{}, work.resultSlot, 0, 0});
    ForeachFrame& frame = foreachFrames_.back();
    if (!parser_.parseForeachHeaderPrefix(frame.state))
    {
        complete(frame.resultSlot, nullptr);
        return;
    }
    frame.collectionStart = parser_.index_;
    Work finish{WorkKind::FinishForeachCollection, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores the executable collection, rewinds, and schedules the distinct semantic probe.
void Parser::ParseContinuationCoordinator::finishForeachCollection(const Work& work)
{
    ForeachFrame& frame = foreachFrames_[work.frameIndex];
    frame.state.collection = childValue(work);
    frame.collectionEnd = parser_.index_;
    parser_.index_ = frame.collectionStart;
    Work finish{WorkKind::FinishForeachProbe, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Restores the real cursor boundary and schedules the required block body.
void Parser::ParseContinuationCoordinator::finishForeachProbe(const Work& work)
{
    ForeachFrame& frame = foreachFrames_[work.frameIndex];
    frame.state.collectionProbe = childValue(work);
    parser_.index_ = frame.collectionEnd;
    parser_.expect(TokenKind::RightParen, "')'");
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after foreach clause");
        complete(frame.resultSlot, parser_.lowerForeachStatement(std::move(frame.state), nullptr));
        return;
    }
    Work finish{WorkKind::FinishForeachBody, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Publishes the canonical lowering after the shared worklist completes the body.
void Parser::ParseContinuationCoordinator::finishForeachBody(const Work& work)
{
    ForeachFrame& frame = foreachFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerForeachStatement(
        std::move(frame.state), childValue(work)));
}

// Opens one switch frame and schedules its governing expression or declaration.
void Parser::ParseContinuationCoordinator::startSwitch(const Work& work)
{
    const std::size_t frameIndex = switchFrames_.size();
    switchFrames_.emplace_back();
    SwitchFrame& frame = switchFrames_.back();
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'(' after 'switch'");
    Work finish{WorkKind::FinishSwitchGoverning, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the switch header and creates the canonical governing node shape.
void Parser::ParseContinuationCoordinator::finishSwitchGoverning(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    ast::AstNode* governing = childValue(work);
    if (work.flag)
    {
        frame.conditionDeclaration = governing;
        governing = conditionFromDeclaration(frame.conditionDeclaration);
    }
    parser_.expect(TokenKind::RightParen, "')' after the switch governing expression");
    parser_.expect(TokenKind::LeftBrace, "'{' to open the switch body");
    frame.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SwitchStatement,
        parser_.tokenRangeFrom(frame.beginIndex));
    frame.node->setFirst(governing);
    Work next{WorkKind::ContinueSwitch, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Selects the next section, performs one-token recovery, or closes the switch.
void Parser::ParseContinuationCoordinator::continueSwitch(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        completeSwitch(frame);
        return;
    }
    if (!parser_.atSwitchLabel())
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "expected 'case' or 'default' at the start of a switch section");
        parser_.consume();
        work_.push_back(work);
        return;
    }
    startSwitchSection(work.frameIndex);
}

// Creates one section and schedules its first label without a nested parser facade.
void Parser::ParseContinuationCoordinator::startSwitchSection(std::size_t frameIndex)
{
    SwitchFrame& frame = switchFrames_[frameIndex];
    frame.sectionBegin = parser_.index_;
    frame.section = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SwitchSection,
        parser_.current().range());
    frame.labelCount = 0;
    Work next{WorkKind::ContinueSwitchLabels, frame.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(next);
}

// Schedules one case value at a time and preserves grouped-label ordering.
void Parser::ParseContinuationCoordinator::continueSwitchLabels(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    if (!parser_.atSwitchLabel())
    {
        frame.section->setIntegerValue(frame.labelCount);
        Work next{WorkKind::ContinueSwitch, frame.resultSlot};
        next.frameIndex = work.frameIndex;
        Work section{WorkKind::ContinueSwitchSection, frame.resultSlot};
        section.frameIndex = work.frameIndex;
        work_.push_back(next);
        work_.push_back(section);
        return;
    }
    Work finish{WorkKind::FinishSwitchLabel, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.beginIndex = parser_.index_;
    finish.flag = parser_.at(TokenKind::KwDefault);
    parser_.consume();
    if (!finish.flag && parser_.at(TokenKind::LeftBracket))
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::NS3049DeferredLanguageSurface,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            language::language3SurfaceDiagnostic(language::Language3Feature::ListPatterns));
    if (!finish.flag)
    {
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, childSlot});
        return;
    }
    work_.push_back(finish);
}

// Materializes one label after its optional value and resumes the label run.
void Parser::ParseContinuationCoordinator::finishSwitchLabel(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    parser_.expect(
        TokenKind::Colon,
        work.flag ? "':' after 'default'" : "':' after the case value");
    ast::AstNode* label = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SwitchLabel,
        parser_.tokenRangeFrom(work.beginIndex));
    label->setFlag(work.flag);
    label->setFirst(childValue(work));
    frame.section->addChild(label);
    ++frame.labelCount;
    Work next{WorkKind::ContinueSwitchLabels, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Schedules one section statement or publishes the completed labels-first section.
void Parser::ParseContinuationCoordinator::continueSwitchSection(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    if (parser_.atSwitchLabel() || parser_.at(TokenKind::RightBrace) ||
        parser_.at(TokenKind::EndOfFile))
    {
        frame.section->setRange(parser_.tokenRangeFrom(frame.sectionBegin));
        frame.node->addChild(frame.section);
        return;
    }
    frame.beforeStatement = parser_.index_;
    Work append{WorkKind::AppendSwitchStatement, frame.resultSlot};
    append.frameIndex = work.frameIndex;
    append.childSlot = makeResultSlot();
    const std::size_t childSlot = append.childSlot;
    work_.push_back(append);
    work_.push_back({WorkKind::Statement, childSlot});
}

// Preserves the historical conditional-pattern flattening and progress guard.
void Parser::ParseContinuationCoordinator::appendSwitchStatement(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    ast::AstNode* statement = childValue(work);
    if (statement != nullptr && statement->kind() == ast::AstNodeKind::BlockStatement &&
        statement->auxiliaryText() == "conditional-pattern-inline")
        for (ast::AstNode* child : statement->children()) frame.section->addChild(child);
    else if (statement != nullptr)
        frame.section->addChild(statement);
    if (parser_.index_ == frame.beforeStatement) parser_.consume();
    Work next{WorkKind::ContinueSwitchSection, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Consumes the closing delimiter and publishes either the switch or its declaration wrapper.
void Parser::ParseContinuationCoordinator::completeSwitch(SwitchFrame& frame)
{
    parser_.expect(TokenKind::RightBrace, "'}' to close the switch");
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    if (frame.conditionDeclaration == nullptr)
    {
        complete(frame.resultSlot, frame.node);
        return;
    }
    ast::AstNode* wrapper = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement,
        frame.node->range());
    wrapper->setAuxiliaryText("condition-declaration");
    wrapper->addChild(frame.conditionDeclaration);
    wrapper->addChild(frame.node);
    complete(frame.resultSlot, wrapper);
}

// Opens one match frame and schedules its governing expression on the shared worklist.
void Parser::ParseContinuationCoordinator::startMatch(const Work& work)
{
    const std::size_t frameIndex = matchFrames_.size();
    matchFrames_.emplace_back();
    MatchFrame& frame = matchFrames_.back();
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    frame.keywordRange = parser_.consume().range();
    parser_.expect(TokenKind::LeftParen, "'(' after 'match'");
    Work finish{WorkKind::FinishMatchGoverning, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the match header and advances to its first arm.
void Parser::ParseContinuationCoordinator::finishMatchGoverning(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    frame.governing = childValue(work);
    parser_.expect(TokenKind::RightParen, "')' after match governing expression");
    parser_.expect(TokenKind::LeftBrace, "'{' to open match arms");
    Work next{WorkKind::ContinueMatch, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Parses one arm prefix or closes and lowers the completed statement match.
void Parser::ParseContinuationCoordinator::continueMatch(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBrace, "'}' to close match arms");
        complete(frame.resultSlot, lowerMatchStatement(frame));
        return;
    }
    parseMatchArmHeader(frame);
    if (parser_.atContextualKeyword("when"))
    {
        parser_.consume();
        Work finish{WorkKind::FinishMatchGuard, frame.resultSlot};
        finish.frameIndex = work.frameIndex;
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::ConditionalExpression, childSlot});
        return;
    }
    validateMatchArmReachability(frame);
    parser_.expect(TokenKind::FatArrow, "'=>' after match pattern");
    scheduleMatchBody(work.frameIndex);
}

// Parses and analyzes one pattern exactly once before any optional guard.
void Parser::ParseContinuationCoordinator::parseMatchArmHeader(MatchFrame& frame)
{
    frame.armBeginIndex = parser_.index_;
    frame.pendingArm = MatchArm{};
    frame.pendingArm.pattern = parser_.parsePattern(false);
    frame.pendingArm.range = frame.pendingArm.pattern.range;
    frame.pendingArm.analysis =
        parser_.analyzeMatchExpressionPattern(frame.pendingArm.pattern, false);
    reportMatchPatternDiagnostics(frame.pendingArm.analysis);
    accumulateMatchOwnership(frame, frame.pendingArm.analysis);
}

// Stores the conditional-expression guard before reachability and arrow validation.
void Parser::ParseContinuationCoordinator::finishMatchGuard(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    frame.pendingArm.guard = childValue(work);
    validateMatchArmReachability(frame);
    parser_.expect(TokenKind::FatArrow, "'=>' after match pattern");
    scheduleMatchBody(work.frameIndex);
}

// Schedules the required block body or performs the historical missing-body recovery.
void Parser::ParseContinuationCoordinator::scheduleMatchBody(std::size_t frameIndex)
{
    MatchFrame& frame = matchFrames_[frameIndex];
    Work finish{WorkKind::FinishMatchArm, frame.resultSlot};
    finish.frameIndex = frameIndex;
    if (parser_.at(TokenKind::LeftBrace))
    {
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Block, childSlot});
        return;
    }
    reportMissingMatchArmBody();
    finish.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, parser_.current().range());
    parser_.synchronizeStatement();
    work_.push_back(finish);
}

// Attaches one completed body, closes its source range, and resumes arm parsing.
void Parser::ParseContinuationCoordinator::finishMatchArm(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    frame.pendingArm.body = work.childSlot == noSlot ? work.node : childValue(work);
    frame.pendingArm.range = parser_.tokenRangeFrom(frame.armBeginIndex);
    frame.arms.push_back(std::move(frame.pendingArm));
    parser_.consumeIf(TokenKind::Comma);
    Work next{WorkKind::ContinueMatch, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Preserves the historical missing-block diagnostic before synchronization.
void Parser::ParseContinuationCoordinator::reportMissingMatchArmBody()
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        parser_.current().range(),
        "a match arm requires a block body");
}

// Returns the stable spelling embedded in binding-mode diagnostics.
std::string_view Parser::ParseContinuationCoordinator::matchBindingModeName(
    PatternBindingMode mode) noexcept
{
    switch (mode)
    {
    case PatternBindingMode::Value: return "value";
    case PatternBindingMode::Reference: return "ref";
    case PatternBindingMode::Move: return "move";
    case PatternBindingMode::Copy: return "copy";
    case PatternBindingMode::ReferenceMutable: return "ref mut";
    }
    return "value";
}

// Publishes the two historical diagnostic families in their original order.
void Parser::ParseContinuationCoordinator::reportMatchPatternDiagnostics(
    const Parser::MatchPatternAnalysis& analysis)
{
    for (const auto& [range, mode] : analysis.nonAddressableBindings)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            std::string(matchBindingModeName(mode)) +
                " pattern binding cannot cross a tuple deconstruction or variant payload "
                "copy; bind the aggregate by ref/move first");
    for (const SourceRange& range : analysis.invalidOrRanges)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "OR-pattern alternatives must bind identical names with identical "
            "value/ref/move modes");
}

// Accumulates governing ownership requirements without rescanning patterns.
void Parser::ParseContinuationCoordinator::accumulateMatchOwnership(
    MatchFrame& frame,
    const Parser::MatchPatternAnalysis& analysis)
{
    for (const auto& [name, mode] : analysis.bindings)
    {
        static_cast<void>(name);
        frame.hasReferenceBinding = frame.hasReferenceBinding ||
            mode == PatternBindingMode::Reference ||
            mode == PatternBindingMode::ReferenceMutable;
        frame.hasMoveBinding = frame.hasMoveBinding || mode == PatternBindingMode::Move;
    }
}

// Updates cross-arm reachability after the optional guard is available.
void Parser::ParseContinuationCoordinator::validateMatchArmReachability(MatchFrame& frame)
{
    MatchArm& arm = frame.pendingArm;
    const bool guardAlwaysTrue = arm.guard == nullptr ||
        (arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
         arm.guard->tokenKind() == TokenKind::KwTrue);
    const bool guardAlwaysFalse = arm.guard != nullptr &&
        arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
        arm.guard->tokenKind() == TokenKind::KwFalse;
    if (guardAlwaysFalse) reportConstantFalseMatchGuard(arm);
    if (frame.sawCatchAll) reportMatchArmAfterCatchAll(arm);
    if (arm.analysis.wildcard && guardAlwaysTrue) frame.sawCatchAll = true;
    validateMatchIntegerIntervals(frame, guardAlwaysTrue);
}

// Emits the established constant-false guard diagnostic at the guard range.
void Parser::ParseContinuationCoordinator::reportConstantFalseMatchGuard(const MatchArm& arm)
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        arm.guard->range(),
        "match arm is unreachable because its guard is the constant false");
}

// Emits the established diagnostic after an unconditional catch-all.
void Parser::ParseContinuationCoordinator::reportMatchArmAfterCatchAll(const MatchArm& arm)
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        arm.range,
        "match arm is unreachable after a catch-all wildcard or binding arm");
}

// Collects one literal/range OR domain iteratively; false marks a mixed domain.
bool Parser::ParseContinuationCoordinator::collectMatchIntegerIntervals(
    const Pattern& root,
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
    bool complete = true;
    std::vector<const Pattern*> pending{&root};
    while (!pending.empty())
    {
        const Pattern* pattern = pending.back();
        pending.pop_back();
        if (appendMatchIntegerInterval(*pattern, intervals)) continue;
        if (pattern->kind != PatternKind::Or) { complete = false; continue; }
        for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
            pending.push_back(&*child);
    }
    return complete;
}

// Appends one bounded integral pattern as a half-open interval.
bool Parser::ParseContinuationCoordinator::appendMatchIntegerInterval(
    const Pattern& pattern,
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
    if (pattern.kind == PatternKind::Literal && pattern.firstInteger.has_value() &&
        *pattern.firstInteger != std::numeric_limits<std::int64_t>::max())
    {
        intervals.emplace_back(*pattern.firstInteger, *pattern.firstInteger + 1);
        return true;
    }
    if (pattern.kind == PatternKind::Range && pattern.firstInteger.has_value() &&
        pattern.secondInteger.has_value() && *pattern.firstInteger < *pattern.secondInteger)
    {
        intervals.emplace_back(*pattern.firstInteger, *pattern.secondInteger);
        return true;
    }
    return false;
}

// Diagnoses overlap and indexes only unrestricted arms.
void Parser::ParseContinuationCoordinator::validateMatchIntegerIntervals(
    MatchFrame& frame,
    bool guardAlwaysTrue)
{
    std::vector<std::pair<std::int64_t, std::int64_t>> current;
    if (!collectMatchIntegerIntervals(frame.pendingArm.pattern, current)) return;
    const bool overlapsWithinArm = normalizeMatchIntegerIntervals(current);
    const bool overlapsEarlierArm = matchIntervalsOverlap(current, frame.coveredIntegerRanges);
    if (overlapsWithinArm || overlapsEarlierArm)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            frame.pendingArm.range,
            "integer literal/range pattern overlaps an earlier alternative");
    if (guardAlwaysTrue)
        for (const auto& interval : current)
            insertMatchIntegerInterval(frame.coveredIntegerRanges, interval);
}

// Sorts and coalesces one arm while recording strict overlap.
bool Parser::ParseContinuationCoordinator::normalizeMatchIntegerIntervals(
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
    std::ranges::sort(intervals);
    std::size_t output = 0;
    bool overlaps = false;
    for (const auto& interval : intervals)
    {
        if (output == 0 || intervals[output - 1].second < interval.first)
        {
            intervals[output++] = interval;
            continue;
        }
        overlaps = overlaps || interval.first < intervals[output - 1].second;
        intervals[output - 1].second =
            std::max(intervals[output - 1].second, interval.second);
    }
    intervals.resize(output);
    return overlaps;
}

// Queries the ordered disjoint interval index in logarithmic time.
bool Parser::ParseContinuationCoordinator::matchIntervalsOverlap(
    const std::vector<std::pair<std::int64_t, std::int64_t>>& current,
    const MatchIntegerIntervalIndex& covered)
{
    for (const auto& interval : current)
    {
        const auto next = covered.lower_bound(interval.first);
        if (next != covered.end() && next->first < interval.second) return true;
        if (next == covered.begin()) continue;
        if (interval.first < std::prev(next)->second) return true;
    }
    return false;
}

// Inserts one interval and coalesces overlap or adjacency.
void Parser::ParseContinuationCoordinator::insertMatchIntegerInterval(
    MatchIntegerIntervalIndex& covered,
    std::pair<std::int64_t, std::int64_t> interval)
{
    auto next = covered.lower_bound(interval.first);
    if (next != covered.begin())
    {
        const auto previous = std::prev(next);
        if (previous->second >= interval.first)
        {
            interval.first = previous->first;
            interval.second = std::max(interval.second, previous->second);
            covered.erase(previous);
        }
    }
    next = covered.lower_bound(interval.first);
    while (next != covered.end() && next->first <= interval.second)
    {
        interval.second = std::max(interval.second, next->second);
        next = covered.erase(next);
    }
    covered.emplace(interval);
}

// Materializes the historical first-match lowering after every arm is complete.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerMatchStatement(MatchFrame& frame)
{
    if (frame.arms.empty()) reportEmptyMatch(frame);
    const SourceRange range = parser_.tokenRangeFrom(frame.beginIndex);
    ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, range);
    const std::string temporary =
        "$__nsc_match_" + std::to_string(parser_.nextMatchTemporary_++);
    lowered->setText(temporary);
    lowered->setAuxiliaryText(buildMatchCoverage(frame.arms));
    appendMatchGoverningState(*lowered, frame, temporary);
    for (const MatchArm& arm : frame.arms)
        lowered->addChild(lowerMatchArm(arm, range, temporary));
    return lowered;
}

// Reports an empty statement match while retaining its lowering shell.
void Parser::ParseContinuationCoordinator::reportEmptyMatch(const MatchFrame& frame)
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        frame.keywordRange,
        "a match statement requires at least one arm");
}

// Builds the source-ordered semantic coverage bridge consumed downstream.
std::string Parser::ParseContinuationCoordinator::buildMatchCoverage(
    const std::vector<MatchArm>& arms)
{
    std::string coverage = "match-coverage:v1:";
    for (std::size_t index = 0; index < arms.size(); ++index)
    {
        if (index != 0) coverage += ';';
        appendMatchArmCoverage(coverage, arms[index]);
    }
    return coverage;
}

// Encodes one arm as wildcard, list-length, nominal-case, or unknown coverage.
void Parser::ParseContinuationCoordinator::appendMatchArmCoverage(
    std::string& coverage,
    const MatchArm& arm)
{
    std::vector<std::string> cases = arm.analysis.cases;
    std::vector<std::pair<std::size_t, bool>> lengths = arm.analysis.listLengths;
    const bool restricted = arm.guard != nullptr &&
        !(arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
          arm.guard->tokenKind() == TokenKind::KwTrue);
    if (arm.analysis.wildcard)
    {
        if (restricted) coverage += '~';
        coverage += '*';
        return;
    }
    if (!lengths.empty())
    {
        appendMatchListCoverage(coverage, lengths, arm.analysis.partial || restricted);
        return;
    }
    appendMatchCaseCoverage(coverage, cases, arm.analysis.partial || restricted);
}

// Sorts and deduplicates list domains before emitting their stable marker format.
void Parser::ParseContinuationCoordinator::appendMatchListCoverage(
    std::string& coverage,
    std::vector<std::pair<std::size_t, bool>>& lengths,
    bool partial)
{
    std::ranges::sort(lengths);
    lengths.erase(std::unique(lengths.begin(), lengths.end()), lengths.end());
    if (partial) coverage += '~';
    for (std::size_t index = 0; index < lengths.size(); ++index)
    {
        if (index != 0) coverage += '|';
        coverage += '#' + std::to_string(lengths[index].first);
        if (lengths[index].second) coverage += '+';
    }
}

// Sorts and deduplicates nominal cases before emitting their stable marker format.
void Parser::ParseContinuationCoordinator::appendMatchCaseCoverage(
    std::string& coverage,
    std::vector<std::string>& cases,
    bool partial)
{
    std::ranges::sort(cases);
    cases.erase(std::unique(cases.begin(), cases.end()), cases.end());
    if (cases.empty()) { coverage += '?'; return; }
    if (partial) coverage += '~';
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        if (index != 0) coverage += '|';
        coverage += cases[index];
    }
}

// Creates governing-value and selected-flag locals with preserved ownership mode.
void Parser::ParseContinuationCoordinator::appendMatchGoverningState(
    ast::AstNode& lowered,
    const MatchFrame& frame,
    const std::string& temporary)
{
    ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
        frame.keywordRange, TokenKind::KwVar, "var");
    ast::AstNode* governing = parser_.makeVariableDeclaration(
        inferred,
        temporary,
        frame.governing,
        false,
        !frame.hasReferenceBinding && !frame.hasMoveBinding);
    if (frame.hasReferenceBinding) governing->setAuxiliaryText("local:ref");
    lowered.addChild(governing);
    lowered.addChild(makeMatchSelectedDeclaration(frame.keywordRange, temporary));
}

// Creates the false-initialized selected flag used by the lowered decision chain.
ast::AstNode* Parser::ParseContinuationCoordinator::makeMatchSelectedDeclaration(
    SourceRange range,
    const std::string& temporary)
{
    ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwBool, "bool");
    return parser_.makeVariableDeclaration(
        boolType,
        temporary + "_selected",
        parser_.makeBooleanLiteral(range, false),
        false,
        false);
}

// Lowers one arm to the historical not-selected guard and pattern matcher.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerMatchArm(
    const MatchArm& arm,
    SourceRange matchRange,
    const std::string& temporary)
{
    ast::AstNode* selectedBody = makeMatchSelectedBody(arm, temporary);
    PatternExpressionFactory root = [this, &temporary, range = arm.range]
    {
        return parser_.makeIdentifierExpression(range, temporary);
    };
    ast::AstNode* continuation = wrapMatchGuard(arm, selectedBody);
    ast::AstNode* matcher = parser_.lowerPattern(
        arm.pattern, root, continuation, temporary);
    ast::AstNode* guardedBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, arm.range);
    guardedBody->addChild(matcher);
    ast::AstNode* guard = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, arm.range);
    guard->setFirst(makeMatchNotSelected(matchRange, temporary));
    guard->setSecond(guardedBody);
    return guard;
}

// Prepends selected=true to original body statements without copying their AST nodes.
ast::AstNode* Parser::ParseContinuationCoordinator::makeMatchSelectedBody(
    const MatchArm& arm,
    const std::string& temporary)
{
    ast::AstNode* selected = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, arm.range);
    selected->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
        parser_.makeIdentifierExpression(arm.range, temporary + "_selected"),
        parser_.makeBooleanLiteral(arm.range, true))));
    for (ast::AstNode* statement : arm.body->children()) selected->addChild(statement);
    return selected;
}

// Evaluates a source guard only after pattern bindings are materialized.
ast::AstNode* Parser::ParseContinuationCoordinator::wrapMatchGuard(
    const MatchArm& arm,
    ast::AstNode* selectedBody)
{
    if (arm.guard == nullptr) return selectedBody;
    ast::AstNode* guarded = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, arm.guard->range());
    guarded->setFirst(arm.guard);
    guarded->setSecond(selectedBody);
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, arm.guard->range());
    block->addChild(guarded);
    return block;
}

// Builds the outer !selected condition that enforces first-match semantics.
ast::AstNode* Parser::ParseContinuationCoordinator::makeMatchNotSelected(
    SourceRange range,
    const std::string& temporary)
{
    ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, range);
    result->setTokenKind(TokenKind::Bang);
    result->setFirst(parser_.makeIdentifierExpression(range, temporary + "_selected"));
    return result;
}

// Parses the non-recursive catch declaration prefix and leaves any filter/body at the cursor.
ast::AstNode* Parser::ParseContinuationCoordinator::parseCatchHeader(std::size_t beginIndex)
{
    parser_.consume();
    ast::AstNode* clause = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::CatchClause,
        parser_.tokenRangeFrom(beginIndex));
    if (!parser_.consumeIf(TokenKind::LeftParen))
    {
        clause->setFlag(true);
        return clause;
    }
    if (parser_.canStartTypeAt(parser_.index_))
        clause->setTypeSyntax(parser_.parseType());
    else
        parser_.expect(TokenKind::Identifier, "exception type");
    if (parser_.at(TokenKind::Identifier))
        clause->setText(std::string(parser_.consume().lexeme()));
    parser_.expect(TokenKind::RightParen, "')' after catch declaration");
    return clause;
}

// Opens one try frame and schedules the protected block on the shared worklist.
void Parser::ParseContinuationCoordinator::startTry(const Work& work)
{
    const std::size_t frameIndex = tryFrames_.size();
    const std::size_t beginIndex = parser_.index_;
    parser_.consume();
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::TryStatement,
        parser_.tokenRangeFrom(beginIndex));
    tryFrames_.push_back({beginIndex, work.resultSlot, 0, node, nullptr, false});
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after try");
        work_.push_back({WorkKind::ContinueTry, work.resultSlot});
        work_.back().frameIndex = frameIndex;
        return;
    }
    Work finish{WorkKind::FinishTryBlock, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Selects the next catch/finally clause or closes the completed try statement.
void Parser::ParseContinuationCoordinator::continueTry(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    if (parser_.at(TokenKind::KwCatch))
    {
        frame.pendingCatchBeginIndex = parser_.index_;
        frame.pendingCatch = parseCatchHeader(frame.pendingCatchBeginIndex);
        if (!parser_.atContextualKeyword("when"))
        {
            scheduleTryCatchBody(work.frameIndex);
            return;
        }
        parser_.consume();
        parser_.expect(TokenKind::LeftParen, "'(' after catch filter 'when'");
        Work finish{WorkKind::FinishTryCatchFilter, frame.resultSlot};
        finish.frameIndex = work.frameIndex;
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, childSlot});
        return;
    }
    if (parser_.consumeIf(TokenKind::KwFinally))
    {
        frame.hasFinally = true;
        scheduleTryFinally(work.frameIndex);
        return;
    }
    completeTry(frame);
}

// Stores the protected block and advances to the ordered catch/finally sequence.
void Parser::ParseContinuationCoordinator::finishTryBlock(const Work& work)
{
    tryFrames_[work.frameIndex].node->setFirst(childValue(work));
    Work next{WorkKind::ContinueTry, work.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Attaches the filter after the expression continuation consumes its exact closing boundary.
void Parser::ParseContinuationCoordinator::finishTryCatchFilter(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    frame.pendingCatch->setSecond(childValue(work));
    parser_.expect(TokenKind::RightParen, "')' after catch filter expression");
    scheduleTryCatchBody(work.frameIndex);
}

// Schedules one catch body or preserves the canonical missing-block recovery.
void Parser::ParseContinuationCoordinator::scheduleTryCatchBody(std::size_t frameIndex)
{
    TryFrame& frame = tryFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after catch");
        attachPendingCatch(frame, nullptr);
        Work next{WorkKind::ContinueTry, frame.resultSlot};
        next.frameIndex = frameIndex;
        work_.push_back(next);
        return;
    }
    Work finish{WorkKind::FinishTryCatchBlock, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches one completed catch body and resumes clause discovery in source order.
void Parser::ParseContinuationCoordinator::finishTryCatchBlock(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    attachPendingCatch(frame, childValue(work));
    Work next{WorkKind::ContinueTry, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Finalizes one pending catch without changing child ordering or range construction.
void Parser::ParseContinuationCoordinator::attachPendingCatch(
    TryFrame& frame,
    ast::AstNode* body)
{
    frame.pendingCatch->setFirst(body);
    frame.pendingCatch->setRange(parser_.tokenRangeFrom(frame.pendingCatchBeginIndex));
    frame.node->addChild(frame.pendingCatch);
    frame.pendingCatch = nullptr;
}

// Schedules the optional finally block or preserves its canonical recovery diagnostic.
void Parser::ParseContinuationCoordinator::scheduleTryFinally(std::size_t frameIndex)
{
    TryFrame& frame = tryFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after finally");
        completeTry(frame);
        return;
    }
    Work finish{WorkKind::FinishTryFinallyBlock, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the final cleanup block and publishes the completed try statement.
void Parser::ParseContinuationCoordinator::finishTryFinallyBlock(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    frame.node->setSecond(childValue(work));
    completeTry(frame);
}

// Preserves the missing-clause diagnostic and exact full-statement source range.
void Parser::ParseContinuationCoordinator::completeTry(TryFrame& frame)
{
    if (frame.node->children().empty() && !frame.hasFinally)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "expected 'catch' or 'finally' after try block");
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.node);
}

// Opens one standalone catch frame for the parser's internal catch-clause facade.
void Parser::ParseContinuationCoordinator::startStandaloneCatch(const Work& work)
{
    const std::size_t frameIndex = catchFrames_.size();
    const std::size_t beginIndex = parser_.index_;
    catchFrames_.push_back({beginIndex, work.resultSlot, parseCatchHeader(beginIndex)});
    if (!parser_.atContextualKeyword("when"))
    {
        scheduleStandaloneCatchBody(frameIndex);
        return;
    }
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'(' after catch filter 'when'");
    Work finish{WorkKind::FinishStandaloneCatchFilter, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores a standalone filter before scheduling the clause body.
void Parser::ParseContinuationCoordinator::finishStandaloneCatchFilter(const Work& work)
{
    catchFrames_[work.frameIndex].clause->setSecond(childValue(work));
    parser_.expect(TokenKind::RightParen, "')' after catch filter expression");
    scheduleStandaloneCatchBody(work.frameIndex);
}

// Schedules a standalone catch body or completes the same malformed-clause recovery shape.
void Parser::ParseContinuationCoordinator::scheduleStandaloneCatchBody(std::size_t frameIndex)
{
    CatchFrame& frame = catchFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after catch");
        frame.clause->setRange(parser_.tokenRangeFrom(frame.beginIndex));
        complete(frame.resultSlot, frame.clause);
        return;
    }
    Work finish{WorkKind::FinishStandaloneCatchBlock, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches and publishes one standalone catch body with its canonical range.
void Parser::ParseContinuationCoordinator::finishStandaloneCatchBlock(const Work& work)
{
    CatchFrame& frame = catchFrames_[work.frameIndex];
    frame.clause->setFirst(childValue(work));
    frame.clause->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.clause);
}

// Parses the resource header and schedules its initializer on the shared expression worklist.
void Parser::ParseContinuationCoordinator::startUsing(const Work& work)
{
    const std::size_t frameIndex = usingFrames_.size();
    usingFrames_.emplace_back();
    UsingFrame& frame = usingFrames_.back();
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'(' after using");
    if (parser_.canStartTypeAt(parser_.index_))
        frame.resourceType = parser_.parseType();
    else
        parser_.expect(TokenKind::Identifier, "resource type");
    if (parser_.at(TokenKind::Identifier))
        frame.resourceName = std::string(parser_.consume().lexeme());
    else
        parser_.expect(TokenKind::Identifier, "resource variable name");

    Work finish{WorkKind::FinishUsingInitializer, work.resultSlot};
    finish.frameIndex = frameIndex;
    if (parser_.expect(TokenKind::Equal, "'=' in using resource declaration"))
    {
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, childSlot});
        return;
    }
    work_.push_back(finish);
}

// Materializes the canonical UsingStatement after the initializer and before its body.
void Parser::ParseContinuationCoordinator::finishUsingInitializer(const Work& work)
{
    UsingFrame& frame = usingFrames_[work.frameIndex];
    parser_.expect(TokenKind::RightParen, "')' after using resource declaration");
    frame.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UsingStatement,
        parser_.tokenRangeFrom(frame.beginIndex));
    frame.node->setTypeSyntax(frame.resourceType);
    frame.node->setText(std::move(frame.resourceName));
    frame.node->setFirst(childValue(work));
    scheduleUsingBody(work.frameIndex);
}

// Requires the historical braced body and publishes malformed headers without blocking drain.
void Parser::ParseContinuationCoordinator::scheduleUsingBody(std::size_t frameIndex)
{
    UsingFrame& frame = usingFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after using declaration");
        completeUsing(frame, nullptr);
        return;
    }
    Work finish{WorkKind::FinishUsingBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed body and publishes the resource statement.
void Parser::ParseContinuationCoordinator::finishUsingBody(const Work& work)
{
    completeUsing(usingFrames_[work.frameIndex], childValue(work));
}

// Preserves the full historical range and AST field layout for using.
void Parser::ParseContinuationCoordinator::completeUsing(
    UsingFrame& frame,
    ast::AstNode* body)
{
    frame.node->setSecond(body);
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.node);
}

// Opens one defer frame and parses its bounded capture list without nested parser reentry.
void Parser::ParseContinuationCoordinator::startDefer(const Work& work)
{
    const std::size_t frameIndex = deferFrames_.size();
    deferFrames_.push_back({parser_.index_, work.resultSlot, parser_.current().range(), {}});
    DeferFrame& frame = deferFrames_.back();
    parser_.consume();
    parser_.expect(TokenKind::LeftBracket, "'[' after defer");
    parseDeferCaptures(frame);
    parser_.expect(TokenKind::RightBracket, "']' after defer captures");
    scheduleDeferBody(frameIndex);
}

// Preserves source-order capture parsing and the historical duplicate-name diagnostic.
void Parser::ParseContinuationCoordinator::parseDeferCaptures(DeferFrame& frame)
{
    std::unordered_set<std::string> names;
    if (parser_.at(TokenKind::RightBracket)) return;
    while (true)
    {
        parseDeferCapture(frame, names);
        if (!parser_.consumeIf(TokenKind::Comma)) return;
    }
}

// Parses one explicit ref/move capture, including receiver and malformed-name recovery.
void Parser::ParseContinuationCoordinator::parseDeferCapture(
    DeferFrame& frame,
    std::unordered_set<std::string>& names)
{
    DeferCapture capture;
    if (parser_.atContextualKeyword("ref"))
    {
        capture.byReference = true;
        parser_.consume();
    }
    else if (parser_.at(TokenKind::KwMove) || parser_.atContextualKeyword("move"))
        parser_.consume();
    else
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "defer capture requires explicit 'ref' or 'move'");

    capture.range = parser_.current().range();
    if (parser_.at(TokenKind::KwThis))
    {
        capture.receiver = true;
        capture.name = "this";
        parser_.consume();
        if (!capture.byReference)
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                capture.range,
                "defer can capture this only by reference");
    }
    else if (parser_.at(TokenKind::Identifier))
        capture.name = std::string(parser_.consume().lexeme());
    else
    {
        parser_.expect(TokenKind::Identifier, "defer capture name");
        capture.name = "__invalid_defer_capture";
    }
    if (!names.insert(capture.name).second)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            capture.range,
            "duplicate defer capture '" + capture.name + "'");
    frame.captures.push_back(std::move(capture));
}

// Schedules the braced cleanup body or completes the canonical malformed-body shape.
void Parser::ParseContinuationCoordinator::scheduleDeferBody(std::size_t frameIndex)
{
    DeferFrame& frame = deferFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after defer capture list");
        ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, frame.deferRange);
        complete(frame.resultSlot, lowerDeferRegistration(frame, body));
        return;
    }
    Work finish{WorkKind::FinishDeferBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Lowers the completed cleanup block and publishes it to the enclosing block continuation.
void Parser::ParseContinuationCoordinator::finishDeferBody(const Work& work)
{
    DeferFrame& frame = deferFrames_[work.frameIndex];
    complete(frame.resultSlot, lowerDeferRegistration(frame, childValue(work)));
}

// Records the exact capture boundary contract consumed by body binding.
void Parser::ParseContinuationCoordinator::configureDeferBody(
    const DeferFrame& frame,
    ast::AstNode* body)
{
    std::string allowedNames;
    for (std::size_t index = 0; index < frame.captures.size(); ++index)
    {
        if (index != 0) allowedNames.push_back(',');
        allowedNames += frame.captures[index].name;
    }
    body->setAuxiliaryText("defer-capture-boundary");
    body->setText(std::move(allowedNames));
}

// Builds the canonical inferred local type used by defer capture temporaries.
ast::TypeSyntax* Parser::ParseContinuationCoordinator::makeDeferVarType(SourceRange range)
{
    return parser_.arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwVar, std::string("var"));
}

// Builds the canonical move expression for source and hidden defer values.
ast::AstNode* Parser::ParseContinuationCoordinator::makeDeferMoveExpression(
    SourceRange range,
    std::string name)
{
    ast::AstNode* move = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, range);
    move->setFirst(parser_.makeIdentifierExpression(range, std::move(name)));
    move->setAuxiliaryText("defer-value-capture");
    return move;
}

// Materializes one capture's outer snapshot and cleanup-local binding in source order.
void Parser::ParseContinuationCoordinator::addDeferCaptureLowering(
    const DeferCapture& capture,
    std::size_t deferOrdinal,
    ast::AstNode* registration,
    ast::AstNode* cleanup)
{
    if (capture.receiver) return;
    const std::string hidden = "__nsc_defer_capture_" +
        std::to_string(deferOrdinal) + "_" + capture.name;
    if (!capture.byReference)
    {
        ast::AstNode* outer = parser_.makeVariableDeclaration(
            makeDeferVarType(capture.range), hidden,
            makeDeferMoveExpression(capture.range, capture.name), false);
        outer->setAuxiliaryText("local:defer-value-capture");
        registration->addChild(outer);
    }
    ast::AstNode* initializer = capture.byReference
        ? parser_.makeIdentifierExpression(capture.range, capture.name)
        : makeDeferMoveExpression(capture.range, hidden);
    ast::AstNode* inner = parser_.makeVariableDeclaration(
        makeDeferVarType(capture.range), capture.name, initializer, false);
    inner->setAuxiliaryText(
        capture.byReference ? "local:ref" : "local:defer-value-capture");
    cleanup->addChild(inner);
}

// Recreates the historical registration block and nested try/finally lowering exactly once.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerDeferRegistration(
    DeferFrame& frame,
    ast::AstNode* body)
{
    configureDeferBody(frame, body);
    const std::size_t deferOrdinal = parser_.nextDeferTemporary_++;
    ast::AstNode* registration = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, frame.deferRange);
    registration->setAuxiliaryText("defer-registration");
    ast::AstNode* cleanup = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, body->range());
    for (const DeferCapture& capture : frame.captures)
        addDeferCaptureLowering(capture, deferOrdinal, registration, cleanup);
    cleanup->addChild(body);

    ast::AstNode* protectedBlock = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, frame.deferRange);
    ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::TryStatement, parser_.tokenRangeFrom(frame.beginIndex));
    lowered->setFirst(protectedBlock);
    lowered->setSecond(cleanup);
    lowered->setAuxiliaryText("defer-lowered");
    registration->addChild(lowered);
    registration->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    return registration;
}

ast::AstNode* Parser::parseWithContinuation(ContinuationRequestKind kind)
{
    if (activeContinuationCoordinator_ != nullptr)
        return activeContinuationCoordinator_->parse(kind);
    ParseContinuationCoordinator coordinator(*this);
    return coordinator.parse(kind);
}
}
