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

bool ProjectOopHirModel::runExpressionScheduler(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    bool& worklistComplete,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    while (!worklistComplete)
    {
iterative_restart:
    if (currentInputKind == IterativeInputKind::LoopBlock)
    {
        if (current == nullptr || current->kind() != ast::AstNodeKind::BlockStatement)
        {
            error = "Stage 34 OOP HIR requires block statement body";
            if (!lastErrorRange_.isValid() && current != nullptr)
                lastErrorRange_ = current->range();
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (resumePendingConditionStatementAfterFailure(
                    frames, current, currentMemberOrdinal, currentAddressContext,
                    currentInputKind, lowered, error)) goto iterative_restart;
            return false;
        }
        IterativeExpressionFrame blockFrame;
        blockFrame.kind = IterativeExpressionKind::LoopBlock;
        blockFrame.syntax = current;
        blockFrame.memberOrdinal = currentMemberOrdinal;
        blockFrame.cleanupDepth = activeHirCleanupRegions_.size();
        blockFrame.loopBlockNode = makeNode(
            ProjectOopHirNodeKind::Block, current, "builtin:void");
        frames.push_back(std::move(blockFrame));
        const LoopBlockAdvance advance = advanceLoopBlockFrame(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, frames.size() - 1U);
        if (advance == LoopBlockAdvance::Descend) continue;
        if (advance == LoopBlockAdvance::Error)
        {
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
            if (resumePendingConditionStatementAfterFailure(
                    frames, current, currentMemberOrdinal, currentAddressContext,
                    currentInputKind, lowered, error)) goto iterative_restart;
            return false;
        }
    }
    else
    {
        const LoopBlockAdvance scheduleAdvance = scheduleCurrentExpression(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
        if (scheduleAdvance == LoopBlockAdvance::Error) return false;
        if (scheduleAdvance == LoopBlockAdvance::Descend) continue;
    }

    const LoopBlockAdvance advance = completeExpressionFrames(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, lowered, error, rootCleanupDepth,
        rootExceptionHandlerDepth);
    if (advance == LoopBlockAdvance::Error) return false;
    if (advance == LoopBlockAdvance::Complete) break;
    }
    return true;
}

LoopBlockAdvance ProjectOopHirModel::completeExpressionFrames(
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

    currentInputKind = IterativeInputKind::Expression;
    bool descend = false;
    while (!frames.empty())
    {
        if (const auto assignmentAdvance = completeScheduledAssignmentFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            assignmentAdvance.has_value())
        {
            if (*assignmentAdvance != LoopBlockAdvance::Complete)
                return *assignmentAdvance;
            continue;
        }
        if (const auto valueAdvance = completeScheduledValueFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            valueAdvance.has_value())
        {
            if (*valueAdvance != LoopBlockAdvance::Complete)
                return *valueAdvance;
            continue;
        }
        if (const auto constructionAdvance = completeScheduledConstructionFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            constructionAdvance.has_value())
        {
            if (*constructionAdvance != LoopBlockAdvance::Complete)
                return *constructionAdvance;
            continue;
        }
        if (const auto coreCallAdvance = completeScheduledCoreCallFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            coreCallAdvance.has_value())
        {
            if (*coreCallAdvance != LoopBlockAdvance::Complete)
                return *coreCallAdvance;
            continue;
        }
        if (const auto accessAdvance = completeScheduledAccessFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            accessAdvance.has_value())
        {
            if (*accessAdvance != LoopBlockAdvance::Complete)
                return *accessAdvance;
            continue;
        }
        if (const auto simpleAdvance = completeScheduledSimpleFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            simpleAdvance.has_value())
        {
            if (*simpleAdvance != LoopBlockAdvance::Complete)
                return *simpleAdvance;
            continue;
        }
        if (const auto statementAdvance = completeScheduledStatementFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            statementAdvance.has_value())
        {
            if (*statementAdvance != LoopBlockAdvance::Complete)
                return *statementAdvance;
            continue;
        }
        if (const auto terminalAdvance = completeScheduledTerminalFrames(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            terminalAdvance.has_value())
        {
            if (*terminalAdvance != LoopBlockAdvance::Complete)
                return *terminalAdvance;
            continue;
        }
        std::optional<LoopBlockAdvance> residualAdvance =
            completeScheduledDeconstructionDeclarationFrame(
                frames, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
        if (!residualAdvance.has_value())
        {
            residualAdvance = completeScheduledBoundCallFrame(
                frames, current, currentMemberOrdinal, currentAddressContext,
                lowered, error);
        }
        if (!residualAdvance.has_value())
        {
            residualAdvance = completeScheduledConditionalFrame(
                frames, current, currentMemberOrdinal, currentAddressContext,
                lowered);
        }
        if (!residualAdvance.has_value()) return LoopBlockAdvance::Error;
        if (*residualAdvance != LoopBlockAdvance::Complete)
            return *residualAdvance;
    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::completeScheduledDeconstructionDeclarationFrame(
    std::vector<IterativeExpressionFrame>& frames,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    IterativeExpressionFrame& frame = frames.back();
    if (frame.kind != IterativeExpressionKind::DeconstructionDeclaration)
        return std::nullopt;
    if (!acceptDeconstructionDeclarationChild(
            frame, std::move(lowered), error))
    {
        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frame.syntax->range();
        return LoopBlockAdvance::Error;
    }
    lowered = finishDeconstructionDeclarationFrame(frame, error);
    if (lowered == nullptr)
    {
        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frame.syntax->range();
        return LoopBlockAdvance::Error;
    }
    frames.pop_back();
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::completeScheduledBoundCallFrame(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error)
{
    IterativeExpressionFrame& frame = frames.back();
    if (!isBoundCallKind(frame.kind)) return std::nullopt;
    BoundCallFrameState& boundCall = *frame.boundCall;
    acceptBoundCallChild(boundCall, std::move(lowered));
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (nextBoundCallChild(boundCall, childSyntax, childAddressContext))
    {
        current = childSyntax;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = childAddressContext;
        return LoopBlockAdvance::Descend;
    }
    auto call = finishBoundCall(
        frame.syntax,
        frame.addressContext,
        error,
        boundCall.typeBinding,
        *boundCall.binding,
        boundCall.target,
        boundCall.argumentSyntaxes.size(),
        boundCall.nullConditional,
        std::move(boundCall.receiver.lowered),
        std::move(boundCall.arguments),
        boundCall.hasOwnedTemporary);
    const bool accessor =
        frame.kind == IterativeExpressionKind::PropertyGetter ||
        frame.kind == IterativeExpressionKind::UserIndexerGetter;
    if (call != nullptr && accessor &&
        !validateNoHeapDirectCall(
            frame.memberOrdinal, *boundCall.binding, error))
    {
        call.reset();
    }
    if (call == nullptr)
    {
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frame.syntax->range();
        lowered.reset();
        frames.clear();
        return LoopBlockAdvance::Error;
    }
    if (frame.kind == IterativeExpressionKind::PropertyGetter &&
        frame.flowProvenNonNull)
    {
        call->stringValue = "flow-nonnull";
    }
    lowered = std::move(call);
    frames.pop_back();
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::completeScheduledConditionalFrame(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    IterativeExpressionFrame& frame = frames.back();
    if (frame.kind != IterativeExpressionKind::Conditional)
        return std::nullopt;
    if (frame.stage == 0U)
    {
        frame.first = std::move(lowered);
        frame.stage = 1U;
        current = frame.syntax->second();
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        return LoopBlockAdvance::Descend;
    }
    if (frame.stage == 1U)
    {
        frame.second = std::move(lowered);
        frame.stage = 2U;
        current = frame.syntax->third();
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        return LoopBlockAdvance::Descend;
    }
    lowered = buildConditionalExpression(
        frame.syntax,
        std::move(frame.first),
        std::move(frame.second),
        std::move(lowered));
    frames.pop_back();
    return LoopBlockAdvance::Complete;
}
}
