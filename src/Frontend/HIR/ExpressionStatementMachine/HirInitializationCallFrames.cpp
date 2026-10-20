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
ProjectOopHirModel::initializeScheduledCallFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (currentKind == IterativeExpressionKind::StructCloneCall)
        return initializeScheduledStructCloneCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::StructAggregateCall)
        return initializeScheduledStructAggregateCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::EnvironmentExitCall)
        return initializeScheduledEnvironmentExitCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::OrdinaryCall)
        return initializeScheduledOrdinaryCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::AggregateCrossLowering)
        return initializeScheduledAggregateCrossLoweringFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::OrdinaryConstruction)
        return initializeScheduledOrdinaryConstructionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::AllocatorConstruction)
        return initializeScheduledAllocatorConstructionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::PropertyGetter)
        return initializeScheduledPropertyGetterFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledStructCloneCallFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeStructCloneCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledStructAggregateCallFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeStructAggregateCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledEnvironmentExitCallFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeEnvironmentExitCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledOrdinaryCallFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeOrdinaryCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledAggregateCrossLoweringFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeAggregateCrossFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledOrdinaryConstructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeOrdinaryConstructionFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledAllocatorConstructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (!initializeAllocatorConstructionFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledPropertyGetterFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.nullConditionalCall = current->kind() ==
        ast::AstNodeKind::NullConditionalMemberExpression;
    const MethodExpressionTypeBinding* receiverFlow =
        frame.nullConditionalCall ? expressionType(current->first()) : nullptr;
    frame.flowProvenNonNull = receiverFlow != nullptr &&
        receiverFlow->flowProvenNonNull;
    if (!initializeBoundCallFrame(
            frame,
            accessorGetterBinding(current),
            {},
            frame.nullConditionalCall && !frame.flowProvenNonNull,
            error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

}
