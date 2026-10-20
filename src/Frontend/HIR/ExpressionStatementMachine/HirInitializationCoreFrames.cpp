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
ProjectOopHirModel::initializeScheduledCoreFrame(
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
    else if (currentKind == IterativeExpressionKind::AssertionCore)
        return initializeScheduledAssertionCoreFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::IteratorCoreCall)
        return initializeScheduledIteratorCoreCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::CoreOperandCall)
        return initializeScheduledCoreOperandCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::AtomicCoreCall)
        return initializeScheduledAtomicCoreCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::MonitorCoreCall)
        return initializeScheduledMonitorCoreCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::ConsoleOutputCall)
        return initializeScheduledConsoleOutputCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::ConsoleReadLineCall)
        return initializeScheduledConsoleReadLineCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::EnumToStringCall)
        return initializeScheduledEnumToStringCallFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledAssertionCoreFrame(
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
    if (!initializeAssertionCoreFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledIteratorCoreCallFrame(
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
    if (!initializeIteratorCoreCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledCoreOperandCallFrame(
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
    if (!initializeCoreOperandCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledAtomicCoreCallFrame(
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
    if (!initializeAtomicCoreCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledMonitorCoreCallFrame(
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
    if (!initializeMonitorCoreCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledConsoleOutputCallFrame(
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
    if (!initializeConsoleOutputCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledConsoleReadLineCallFrame(
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
    if (!initializeConsoleReadLineCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledEnumToStringCallFrame(
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
    if (!initializeEnumToStringCallFrame(frame, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

}
