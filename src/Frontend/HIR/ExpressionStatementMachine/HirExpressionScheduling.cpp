#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionKind;
using internal::BoundCallFrameStage;
using internal::ReceiverFrameState;
using internal::BoundCallFrameState;
using internal::NamespaceCallFrameStage;
using internal::NamespaceCallFrameState;
using internal::EventInvokeFrameStage;
using internal::EventInvokeFrameState;
using internal::DelegateInvokeFrameStage;
using internal::DelegateInvokeFrameState;
using internal::CoreOperandCallFrameState;
using internal::AtomicCoreCallFrameStage;
using internal::AtomicCoreCallFrameState;
using internal::MonitorCoreCallFrameStage;
using internal::MonitorCoreCallFrameState;
using internal::ConsoleOutputCallFrameState;
using internal::EnumToStringCallFrameState;
using internal::StructAggregateCallFrameStage;
using internal::StructAggregateCallFrameState;
using internal::OrdinaryCallFrameStage;
using internal::OrdinaryCallFrameState;
using internal::AllocatorConstructionFrameStage;
using internal::AllocatorConstructionFrameState;
using internal::OrdinaryConstructionFrameStage;
using internal::OrdinaryConstructionFrameState;
using internal::AggregateCrossSlot;
using internal::AggregateCrossStructuralFrame;
using internal::AggregateCrossFramePhase;
using internal::AggregateCrossFrameState;
using internal::VariableDeclarationFrameMode;
using internal::VariableDeclarationFrameState;
using internal::DeconstructionDeclarationFrameStage;
using internal::DeconstructionDeclarationFrameState;
using internal::InlineAssemblyFrameState;
using internal::SwitchStatementFrameState;
using internal::ForStatementFrameState;
using internal::ForeachStatementFrameState;
using internal::ExitValueStatementFrameState;
using internal::CoroutineValueStatementFrameState;
using internal::UsingStatementFrameState;
using internal::TryStatementFrameState;
using internal::IterativeExpressionFrame;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;

LoopBlockAdvance ProjectOopHirModel::scheduleCurrentExpression(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    const std::optional<IterativeExpressionKind> currentKind =
        iterativeExpressionKind(current, currentAddressContext);
    bool completedWithoutChild = false;
    if (currentKind.has_value())
    {
        const LoopBlockAdvance advance = scheduleIterativeExpression(
            *currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        if (advance != LoopBlockAdvance::Complete) return advance;
    }
    return lowerScheduledDirectExpression(
        completedWithoutChild, frames, current, currentMemberOrdinal,
        currentAddressContext, lowered, error, rootCleanupDepth,
        rootExceptionHandlerDepth);
}

LoopBlockAdvance ProjectOopHirModel::lowerScheduledDirectExpression(
    bool completedWithoutChild,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode* current,
    std::size_t currentMemberOrdinal,
    bool currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (completedWithoutChild) return LoopBlockAdvance::Complete;
    lowered = scheduledChildUsesExpressionImpl(frames)
        ? lowerExpressionImpl(
            current, currentMemberOrdinal, currentAddressContext, error)
        : lowerExpression(
            current, currentMemberOrdinal, currentAddressContext, error);
    if (lowered == nullptr)
    {
        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
        setScheduledLoweringErrorRange(frames, current);
        return LoopBlockAdvance::Error;
    }
    return LoopBlockAdvance::Complete;
}

bool ProjectOopHirModel::scheduledChildUsesExpressionImpl(
    const std::vector<IterativeExpressionFrame>& frames) const
{
    const bool isExpressionStatementRoot =
        frames.size() == 1u &&
        frames.back().kind == IterativeExpressionKind::ExpressionStatement;
    if (frames.empty()) return isExpressionStatementRoot;
    const IterativeExpressionKind kind = frames.back().kind;
    const bool isDeclarationChild =
        kind == IterativeExpressionKind::VariableDeclaration ||
        kind == IterativeExpressionKind::DeconstructionDeclaration ||
        kind == IterativeExpressionKind::InlineAssemblyStatement;
    const bool isStatementExpressionChild =
        (kind == IterativeExpressionKind::IfStatement ||
         kind == IterativeExpressionKind::LoopStatement ||
         kind == IterativeExpressionKind::SwitchStatement) &&
        frames.back().stage == 0U;
    const bool isForExpressionChild =
        kind == IterativeExpressionKind::ForStatement &&
        frames.back().stage >= 1U && frames.back().stage <= 3U;
    return isExpressionStatementRoot || isDeclarationChild ||
        kind == IterativeExpressionKind::StaticAssertStatement ||
        isStatementExpressionChild || isForExpressionChild;
}

void ProjectOopHirModel::setScheduledLoweringErrorRange(
    const std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode* current)
{
    if (lastErrorRange_.isValid() || frames.empty()) return;
    const IterativeExpressionKind kind = frames.back().kind;
    const bool usesCurrentRange =
        kind == IterativeExpressionKind::VariableDeclaration ||
        kind == IterativeExpressionKind::DeconstructionDeclaration ||
        kind == IterativeExpressionKind::InlineAssemblyStatement ||
        ((kind == IterativeExpressionKind::IfStatement ||
          kind == IterativeExpressionKind::LoopStatement ||
          kind == IterativeExpressionKind::SwitchStatement) &&
         frames.back().stage == 0U) ||
        (kind == IterativeExpressionKind::ForStatement &&
         frames.back().stage >= 1U && frames.back().stage <= 3U);
    if (usesCurrentRange && current != nullptr)
    {
        lastErrorRange_ = current->range();
    }
    else if (kind != IterativeExpressionKind::ExpressionStatement)
    {
        lastErrorRange_ = frames.back().syntax->range();
    }
}

LoopBlockAdvance ProjectOopHirModel::scheduleIterativeExpression(
    IterativeExpressionKind currentKind,
    bool& completedWithoutChild,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
IterativeExpressionFrame frame;
frame.kind = currentKind;
frame.syntax = current;
frame.addressContext = currentAddressContext;
frame.memberOrdinal = currentMemberOrdinal;
const LoopBlockAdvance initializeAdvance = initializeScheduledExpressionFrame(
        frame, currentKind, frames, current, currentMemberOrdinal,
        currentAddressContext, currentInputKind, lowered, error,
        rootCleanupDepth, rootExceptionHandlerDepth);
if (initializeAdvance != LoopBlockAdvance::Complete)
    return initializeAdvance;

frames.push_back(std::move(frame));
const LoopBlockAdvance selectAdvance = selectScheduledExpressionChild(
        currentKind, completedWithoutChild, frames, current,
        currentMemberOrdinal, currentAddressContext, currentInputKind, lowered,
        error, rootCleanupDepth, rootExceptionHandlerDepth);
if (selectAdvance != LoopBlockAdvance::Complete)
    return selectAdvance;

if (!completedWithoutChild) return LoopBlockAdvance::Descend;

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledExpressionFrame(
    IterativeExpressionFrame& frame,
    IterativeExpressionKind currentKind,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (const auto primaryAdvance = initializeScheduledPrimaryFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error,
            rootCleanupDepth, rootExceptionHandlerDepth);
        primaryAdvance.has_value())
        return *primaryAdvance;
    if (const auto coreAdvance = initializeScheduledCoreFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error,
            rootCleanupDepth, rootExceptionHandlerDepth);
        coreAdvance.has_value())
        return *coreAdvance;
    if (const auto callAdvance = initializeScheduledCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error,
            rootCleanupDepth, rootExceptionHandlerDepth);
        callAdvance.has_value())
        return *callAdvance;
    if (const auto accessAdvance = initializeScheduledAccessFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error,
            rootCleanupDepth, rootExceptionHandlerDepth);
        accessAdvance.has_value())
        return *accessAdvance;
    if (const auto valueAdvance = initializeScheduledValueFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error,
            rootCleanupDepth, rootExceptionHandlerDepth);
        valueAdvance.has_value())
        return *valueAdvance;
    if (const auto assignmentAdvance = initializeScheduledAssignmentFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error,
            rootCleanupDepth, rootExceptionHandlerDepth);
        assignmentAdvance.has_value())
        return *assignmentAdvance;
    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::selectScheduledExpressionChild(
    IterativeExpressionKind currentKind,
    bool& completedWithoutChild,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
const LoopBlockAdvance syntaxAdvance = selectScheduledChildSyntax(
        currentKind, completedWithoutChild, frames, current,
        currentMemberOrdinal, currentAddressContext, currentInputKind,
        lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
if (syntaxAdvance != LoopBlockAdvance::Complete) return syntaxAdvance;

configureScheduledChildContext(
    currentKind, frames, currentMemberOrdinal, currentAddressContext,
    currentInputKind);

if (currentKind != IterativeExpressionKind::LoopExpression)
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Complete;
}


LoopBlockAdvance ProjectOopHirModel::selectScheduledChildSyntax(
    IterativeExpressionKind currentKind,
    bool& completedWithoutChild,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (const auto primaryAdvance = selectScheduledPrimaryChildSyntax(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        primaryAdvance.has_value())
        return *primaryAdvance;
    if (const auto secondaryAdvance = selectScheduledSecondaryChildSyntax(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        secondaryAdvance.has_value())
        return *secondaryAdvance;
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledPrimaryChildSyntax(
    IterativeExpressionKind currentKind,
    bool& completedWithoutChild,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (const auto loopAwaitAdvance = selectScheduledLoopAwaitChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        loopAwaitAdvance.has_value())
        return *loopAwaitAdvance;
    if (const auto namespaceEventAdvance = selectScheduledNamespaceEventChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        namespaceEventAdvance.has_value())
        return *namespaceEventAdvance;
    if (const auto delegateConsoleAdvance = selectScheduledDelegateConsoleChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        delegateConsoleAdvance.has_value())
        return *delegateConsoleAdvance;
    if (const auto coreOperandAdvance = selectScheduledCoreOperandChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        coreOperandAdvance.has_value())
        return *coreOperandAdvance;
    if (const auto atomicMonitorAdvance = selectScheduledAtomicMonitorChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        atomicMonitorAdvance.has_value())
        return *atomicMonitorAdvance;
    if (const auto ordinaryCallAdvance = selectScheduledOrdinaryCallChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        ordinaryCallAdvance.has_value())
        return *ordinaryCallAdvance;
    return std::nullopt;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledSecondaryChildSyntax(
    IterativeExpressionKind currentKind,
    bool& completedWithoutChild,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (const auto aggregateAdvance = selectScheduledAggregateChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        aggregateAdvance.has_value())
        return *aggregateAdvance;
    if (const auto ordinaryConstructionAdvance =
            selectScheduledOrdinaryConstructionChild(
                currentKind, completedWithoutChild, frames, current,
                currentMemberOrdinal, currentAddressContext, currentInputKind,
                lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        ordinaryConstructionAdvance.has_value())
        return *ordinaryConstructionAdvance;
    if (const auto allocatorAdvance = selectScheduledAllocatorChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        allocatorAdvance.has_value())
        return *allocatorAdvance;
    if (const auto receiverAdvance = selectScheduledReceiverChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        receiverAdvance.has_value())
        return *receiverAdvance;
    if (const auto remainingAdvance = selectScheduledRemainingChild(
            currentKind, completedWithoutChild, frames, current,
            currentMemberOrdinal, currentAddressContext, currentInputKind,
            lowered, error, rootCleanupDepth, rootExceptionHandlerDepth);
        remainingAdvance.has_value())
        return *remainingAdvance;
    return std::nullopt;
}

void ProjectOopHirModel::configureScheduledChildContext(
    IterativeExpressionKind currentKind,
    std::vector<IterativeExpressionFrame>& frames,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind)
{
    if (configureScheduledPrimaryChildContext(
            currentKind, frames, currentMemberOrdinal, currentAddressContext,
            currentInputKind))
        return;
    if (configureScheduledValueChildContext(
            currentKind, frames, currentAddressContext))
        return;
    if (configureScheduledCallChildContext(
            currentKind, frames, currentAddressContext))
        return;
    configureScheduledFallbackChildContext(currentKind, currentAddressContext);
}

bool ProjectOopHirModel::configureScheduledPrimaryChildContext(
    IterativeExpressionKind currentKind,
    std::vector<IterativeExpressionFrame>& frames,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind)
{
    if (currentKind == IterativeExpressionKind::LoopExpression)
    {
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
    }
    else if (isBoundCallKind(currentKind))
    {
        // The common frame selected both the child and its address context above.
    }
    else if (currentKind == IterativeExpressionKind::AllocatorConstruction)
    {
        // The allocator frame selected receiver addressability versus argument value.
    }
    else if (currentKind == IterativeExpressionKind::AggregateCrossLowering)
    {
        // The aggregate frame selects value versus move-source address context.
    }
    else if (currentKind == IterativeExpressionKind::OrdinaryConstruction)
    {
        // Storage, arguments, and field initializers are value expressions.
    }
    else if (currentKind == IterativeExpressionKind::ExtensionPropertyGetter)
    {
        currentAddressContext = frames.back().operandAddressContext;
    }
    else if (currentKind == IterativeExpressionKind::InterpolationHandler)
    {
        currentAddressContext = false;
        currentMemberOrdinal = frames.back().core->builderMemberOrdinals[1];
    }
    else
    {
        return false;
    }
    return true;
}

bool ProjectOopHirModel::configureScheduledValueChildContext(
    IterativeExpressionKind currentKind,
    std::vector<IterativeExpressionFrame>& frames,
    bool& currentAddressContext)
{
    if (currentKind == IterativeExpressionKind::InstanceField)
    {
        currentAddressContext =
            isStructValueType(frames.back().fieldBinding->receiverTypeKey) &&
            !frames.back().materializeReceiverTemporary;
    }
    else if (currentKind == IterativeExpressionKind::RangeSlice)
    {
        currentAddressContext =
            isStructValueType(frames.back().rangeBinding->receiverTypeKey) &&
            !frames.back().materializeReceiverTemporary;
    }
    else if (currentKind == IterativeExpressionKind::RawIndex)
    {
        currentAddressContext = frames.back().baseIsFixedArray ||
            frames.back().baseIsFlexibleArray;
    }
    else if (currentKind == IterativeExpressionKind::NullCoalescingAssignment)
    {
        currentAddressContext = true;
    }
    else if (currentKind == IterativeExpressionKind::Destruction)
    {
        currentAddressContext = frames.back().operandAddressContext;
    }
    else if (currentKind == IterativeExpressionKind::StorageUnary)
    {
        currentAddressContext =
            frames.back().syntax->tokenKind() != lexer::TokenKind::Star;
    }
    else if (currentKind == IterativeExpressionKind::MoveProjection)
    {
        currentAddressContext = true;
    }
    else if (currentKind == IterativeExpressionKind::Match)
    {
        currentAddressContext = frames.back().matchBinding->governingReference;
    }
    else if (currentKind == IterativeExpressionKind::DeconstructionAssignment)
    {
        currentAddressContext =
            frames.back().deconstructionBinding->selectedMemberOrdinal.has_value() &&
            isStructValueType(frames.back().deconstructionBinding->sourceTypeKey) &&
            !frames.back().materializeReceiverTemporary;
    }
    else
    {
        return false;
    }
    return true;
}

bool ProjectOopHirModel::configureScheduledCallChildContext(
    IterativeExpressionKind currentKind,
    std::vector<IterativeExpressionFrame>& frames,
    bool& currentAddressContext)
{
    if (currentKind == IterativeExpressionKind::EventSubscription)
    {
        currentAddressContext = frames.back().stage == 0U &&
            isStructValueType(
                frames.back().eventSubscriptionBinding->receiverTypeKey) &&
            !frames.back().materializeReceiverTemporary;
    }
    else if (currentKind == IterativeExpressionKind::PropertyIndexerSetter)
    {
        currentAddressContext = !frames.back().compoundSetter &&
            frames.back().stage == 0U &&
            isStructValueType(frames.back().callTarget.receiverTypeKey) &&
            !frames.back().materializeReceiverTemporary;
        if (!frames.back().compoundSetter && frames.back().stage != 0U)
        {
            currentAddressContext = boundCallArgumentNeedsAddress(
                frames.back().callTarget.memberOrdinal, 0U);
        }
    }
    else if (currentKind == IterativeExpressionKind::NamespaceExtensionSetter)
    {
        currentAddressContext = false;
    }
    else if (currentKind == IterativeExpressionKind::IteratorCoreCall)
    {
        // The iterator frame selected value versus address context above.
    }
    else if (currentKind == IterativeExpressionKind::CoreOperandCall)
    {
        // The core-operand frame selected value versus address context above.
    }
    else if (currentKind == IterativeExpressionKind::OrdinaryCall)
    {
        // The ordinary-call frame selected receiver addressability versus argument value.
    }
    else
    {
        return false;
    }
    return true;
}

void ProjectOopHirModel::configureScheduledFallbackChildContext(
    IterativeExpressionKind currentKind,
    bool& currentAddressContext)
{
    if (currentKind == IterativeExpressionKind::AtomicCoreCall)
    {
        // Atomic locations and values are already resolved value expressions.
    }
    else if (currentKind == IterativeExpressionKind::MonitorCoreCall)
    {
        // Monitor receivers and Wait timeouts are value expressions.
    }
    else if (currentKind == IterativeExpressionKind::ConsoleOutputCall ||
        currentKind == IterativeExpressionKind::ConsoleReadLineCall ||
        currentKind == IterativeExpressionKind::EnumToStringCall ||
        currentKind == IterativeExpressionKind::StructCloneCall ||
        currentKind == IterativeExpressionKind::StructAggregateCall ||
        currentKind == IterativeExpressionKind::EnvironmentExitCall)
    {
        // H02S14K frames finalize or schedule their next value child below.
    }
    else if (currentKind == IterativeExpressionKind::NullConditionalFieldAssignment ||
        currentKind == IterativeExpressionKind::NullConditionalArrayAssignment)
    {
        currentAddressContext = false;
    }
    else if (currentKind == IterativeExpressionKind::DirectAssignment ||
        currentKind == IterativeExpressionKind::CompoundAssignment ||
        currentKind == IterativeExpressionKind::StringCompoundAssignment)
    {
        currentAddressContext = true;
    }
    else if (currentKind != IterativeExpressionKind::OverflowPolicy)
    {
        currentAddressContext = false;
    }
}

}
