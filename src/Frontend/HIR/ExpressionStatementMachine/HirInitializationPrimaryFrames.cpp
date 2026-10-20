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

std::optional<LoopBlockAdvance>
ProjectOopHirModel::initializeScheduledPrimaryFrame(
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
    if (currentKind == IterativeExpressionKind::LoopExpression)
        return initializeScheduledLoopExpressionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::AwaitExpression)
        return initializeScheduledAwaitExpressionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::UserUnaryOperator)
        return initializeScheduledUserUnaryOperatorFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::UserBinaryOperator)
        return initializeScheduledUserBinaryOperatorFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::UserConversion)
        return initializeScheduledUserConversionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::NamespaceCall)
        return initializeScheduledNamespaceCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::EventInvoke)
        return initializeScheduledEventInvokeFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::DelegateInvoke)
        return initializeScheduledDelegateInvokeFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledLoopExpressionFrame(
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
    if (currentAddressContext)
    {
        error = "a loop expression produces a value, not addressable storage";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
        return LoopBlockAdvance::Error;
    }
    frame.loopExpressionBinding = methodBodyModel_ != nullptr
        ? methodBodyModel_->loopExpressionBinding(current) : nullptr;
    if (frame.loopExpressionBinding == nullptr)
    {
        error = "T07-F05 loop expression has no semantic binding";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
        return LoopBlockAdvance::Error;
    }
    frame.first = makeNode(
        ProjectOopHirNodeKind::LoopExpression,
        current,
        frame.loopExpressionBinding->resultTypeKey);
    frame.first->stringValue = frame.loopExpressionBinding->label;

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledAwaitExpressionFrame(
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
    const MethodCallBinding* binding = callBinding(current->first());
    if (binding != nullptr && binding->selectedMemberOrdinal.has_value() &&
        *binding->selectedMemberOrdinal < memberModel_->members().size())
    {
        frame.awaitMemberOrdinal = *binding->selectedMemberOrdinal;
        if (awaitProducerCanSuspend(*frame.awaitMemberOrdinal))
        {
            error = "await on an async function that can suspend requires the state machine "
                    "lowering (NS304-T05-F03)";
            if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
            return LoopBlockAdvance::Error;
        }
        if (!loweringAsyncBody_ && !activeHirExceptionHandlers_.empty())
        {
            const auto& handler = activeHirExceptionHandlers_.back();
            for (std::size_t index = activeHirCleanupRegions_.size();
                index > handler.retainedCleanupDepth; --index)
            {
                frame.awaitCleanupRegionOrdinals.push_back(
                    activeHirCleanupRegions_[index - 1U]);
            }
            frame.awaitExceptionTargetRegionOrdinal = handler.hirRegionOrdinal;
        }
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledUserUnaryOperatorFrame(
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
    if (!initializeBoundCallFrame(
            frame, callBinding(current), {current->first()}, false, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledUserBinaryOperatorFrame(
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
    if (!initializeBoundCallFrame(
            frame,
            callBinding(current),
            {current->first(), current->second()},
            false,
            error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledUserConversionFrame(
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
    if (!initializeBoundCallFrame(
            frame, callBinding(current), {current->first()}, false, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledNamespaceCallFrame(
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
    if (!initializeNamespaceCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (resumePendingConditionStatementAfterFailure(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error)) return LoopBlockAdvance::Descend;
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledEventInvokeFrame(
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
    if (!initializeEventInvokeFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledDelegateInvokeFrame(
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
    if (!initializeDelegateInvokeFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

}
