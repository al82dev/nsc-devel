#include "nativesharp/Parser/Parser.h"

#include "Internal/ParseContinuationCoordinator.h"

#include "nativesharp/Language/AbiSemantics.h"
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
    if (kind == ContinuationRequestKind::LoopExpression) workKind = WorkKind::LoopExpression;
    else if (kind == ContinuationRequestKind::ComprehensionExpression)
        workKind = WorkKind::ComprehensionExpression;
    else if (kind == ContinuationRequestKind::Block) workKind = WorkKind::Block;
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
    if (executeExpressionWork(work)) return;
    if (executeStatementWork(work)) return;
    executeControlWork(work);
}

bool Parser::ParseContinuationCoordinator::executeExpressionWork(const Work& work)
{
    switch (work.kind)
    {
    case WorkKind::Expression: startExpression(work); return true;
    case WorkKind::LoopExpression: startLoopExpression(work); return true;
    case WorkKind::ComprehensionExpression: startComprehensionExpression(work); return true;
    case WorkKind::ResumeExpression: resumeExpression(work); return true;
    case WorkKind::UnaryExpression: startUnaryExpression(work); return true;
    case WorkKind::ConditionalExpression: startConditionalExpression(work); return true;
    case WorkKind::Block: startBlock(work); return true;
    case WorkKind::ContinueBlock: continueBlock(work); return true;
    case WorkKind::AppendBlockStatement: appendBlockStatement(work); return true;
    case WorkKind::Statement: startStatement(work); return true;
    case WorkKind::FinishLambdaBlock: finishLambdaBlock(work); return true;
    case WorkKind::FinishExpressionStatement: finishExpressionStatement(work); return true;
    case WorkKind::FinishReturn: finishReturn(work); return true;
    case WorkKind::FinishThrow: finishThrow(work); return true;
    case WorkKind::FinishJump: finishJump(work); return true;
    case WorkKind::FinishYieldReturn: finishYieldReturn(work); return true;
    case WorkKind::FinishStaticAssert: finishStaticAssert(work); return true;
    case WorkKind::FinishDeconstruction: finishDeconstruction(work); return true;
    case WorkKind::ContinueVariableDeclaration: continueVariableDeclaration(work); return true;
    case WorkKind::FinishVariableInitializer: finishVariableInitializer(work); return true;
    case WorkKind::Aggregate: startAggregate(work); return true;
    case WorkKind::ContinueAggregate: continueAggregate(work); return true;
    case WorkKind::FinishAggregateElement: finishAggregateElement(work); return true;
    case WorkKind::FinishLocalFunction: finishLocalFunction(work); return true;
    default: return false;
    }
}

bool Parser::ParseContinuationCoordinator::executeStatementWork(const Work& work)
{
    switch (work.kind)
    {
    case WorkKind::Embedded: startEmbedded(work); return true;
    case WorkKind::FinishEmbedded: finishEmbedded(work); return true;
    case WorkKind::FinishIfCondition: finishIfCondition(work); return true;
    case WorkKind::FinishIfThen: finishIfThen(work); return true;
    case WorkKind::FinishIfElse: finishIfElse(work); return true;
    case WorkKind::FinishWhileCondition: finishWhileCondition(work); return true;
    case WorkKind::FinishWhileBody: finishWhileBody(work); return true;
    case WorkKind::FinishDoBody: finishDoBody(work); return true;
    case WorkKind::FinishDoCondition: finishDoCondition(work); return true;
    case WorkKind::FinishForInitializer: finishForInitializer(work); return true;
    case WorkKind::FinishForCondition: finishForCondition(work); return true;
    case WorkKind::FinishForIncrement: finishForIncrement(work); return true;
    case WorkKind::FinishForBody: finishForBody(work); return true;
    case WorkKind::ContinueForExpressionList: continueForExpressionList(work); return true;
    case WorkKind::FinishForExpression: finishForExpression(work); return true;
    case WorkKind::FinishForeachCollection: finishForeachCollection(work); return true;
    case WorkKind::FinishForeachProbe: finishForeachProbe(work); return true;
    case WorkKind::FinishForeachBody: finishForeachBody(work); return true;
    case WorkKind::FinishConditionalPatternSource:
        finishConditionalPatternSource(work); return true;
    case WorkKind::FinishConditionalPatternSuccess:
        finishConditionalPatternSuccess(work); return true;
    case WorkKind::FinishConditionalPatternFailure:
        finishConditionalPatternFailure(work); return true;
    case WorkKind::FinishConditionalPatternWhile:
        finishConditionalPatternWhile(work); return true;
    case WorkKind::FinishConditionalPatternLetElse:
        finishConditionalPatternLetElse(work); return true;
    case WorkKind::FinishSwitchGoverning: finishSwitchGoverning(work); return true;
    case WorkKind::ContinueSwitch: continueSwitch(work); return true;
    case WorkKind::ContinueSwitchLabels: continueSwitchLabels(work); return true;
    case WorkKind::FinishSwitchLabel: finishSwitchLabel(work); return true;
    case WorkKind::ContinueSwitchSection: continueSwitchSection(work); return true;
    case WorkKind::AppendSwitchStatement: appendSwitchStatement(work); return true;
    case WorkKind::FinishMatchGoverning: finishMatchGoverning(work); return true;
    case WorkKind::ContinueMatch: continueMatch(work); return true;
    case WorkKind::FinishMatchGuard: finishMatchGuard(work); return true;
    case WorkKind::FinishMatchArm: finishMatchArm(work); return true;
    default: return false;
    }
}

void Parser::ParseContinuationCoordinator::executeControlWork(const Work& work)
{
    switch (work.kind)
    {
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
    case WorkKind::FinishFloatingPointScope: finishFloatingPointScope(work); return;
    case WorkKind::FinishUnsafeScope: finishUnsafeScope(work); return;
    case WorkKind::ContinueInlineAssembly: continueInlineAssembly(work); return;
    case WorkKind::FinishInlineAssemblyOutput: finishInlineAssemblyOutput(work); return;
    case WorkKind::FinishInlineAssemblyInput: finishInlineAssemblyInput(work); return;
    case WorkKind::FinishComprehensionYield: finishComprehensionYield(work); return;
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
}
