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
ProjectOopHirModel::completeScheduledSimpleFrames(
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
    const IterativeExpressionKind kind = frames.back().kind;
    if (kind == IterativeExpressionKind::LoopExpression)
        return completeScheduledLoopExpressionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::AwaitExpression)
        return completeScheduledAwaitExpressionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ExpressionStatement)
        return completeScheduledExpressionStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::OverflowPolicy)
        return completeScheduledOverflowPolicyFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::BuiltinUnary)
        return completeScheduledBuiltinUnaryFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::DelegateCreationReceiver)
        return completeScheduledDelegateCreationReceiverFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::Conversion)
        return completeScheduledConversionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::Match)
        return completeScheduledMatchFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledLoopExpressionFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::LoopExpression)
    {
        IterativeExpressionFrame& frame = frames.back();
        frame.first->children.push_back(std::move(lowered));
        lowered = std::move(frame.first);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledAwaitExpressionFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::AwaitExpression)
    {
        IterativeExpressionFrame& frame = frames.back();
        const ast::AstNode* awaitSyntax = frame.syntax;
        lowered = lowerAwaitExpression(
            frame.syntax,
            std::move(lowered),
            frame.awaitMemberOrdinal,
            std::move(frame.awaitCleanupRegionOrdinals),
            frame.awaitExceptionTargetRegionOrdinal,
            error);
        frames.pop_back();
        if (lowered == nullptr)
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid()) lastErrorRange_ = awaitSyntax->range();
            return LoopBlockAdvance::Error;
        }
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledExpressionStatementFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::ExpressionStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        auto statement = makeNode(
            ProjectOopHirNodeKind::ExpressionStatement,
            frame.syntax,
            "builtin:void");
        statement->children.push_back(std::move(lowered));
        lowered = std::move(statement);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledOverflowPolicyFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::OverflowPolicy)
    {
        IterativeExpressionFrame& frame = frames.back();
        auto policy = makeNode(
            ProjectOopHirNodeKind::OverflowPolicy,
            frame.syntax,
            lowered->typeKey);
        policy->stringValue = frame.syntax->auxiliaryText();
        policy->children.push_back(std::move(lowered));
        lowered = std::move(policy);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledBuiltinUnaryFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::BuiltinUnary)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodExpressionTypeBinding* frameType =
            expressionType(frame.syntax);
        const MethodDelegateBinding* frameDelegate =
            delegateBinding(frame.syntax);
        const std::string resultType = frameType != nullptr
            ? frameType->typeKey
            : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
        auto unary = makeNode(
            ProjectOopHirNodeKind::Unary, frame.syntax, resultType);
        unary->operation = frame.syntax->tokenKind();
        unary->children.push_back(std::move(lowered));
        lowered = std::move(unary);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledDelegateCreationReceiverFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::DelegateCreationReceiver)
    {
        IterativeExpressionFrame& frame = frames.back();
        acceptReceiverChild(frame.receiverFrame, std::move(lowered));
        lowered = finishDelegateCreation(
            frame.syntax,
            *frame.delegateCreationBinding,
            std::move(frame.receiverFrame.lowered));
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledConversionFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::Conversion)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodExpressionTypeBinding* frameType =
            expressionType(frame.syntax);
        const MethodDelegateBinding* frameDelegate =
            delegateBinding(frame.syntax);
        const std::string resultType = frameType != nullptr
            ? frameType->typeKey
            : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
        const MethodRuntimeTypeBinding* runtime = runtimeTypeBinding(frame.syntax);
        ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::Conversion;
        if (frame.syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
            frame.syntax->kind() == ast::AstNodeKind::SafeCastExpression)
        {
            nodeKind = runtime != nullptr &&
                    runtime->kind == MethodRuntimeTypeOperationKind::TypeTest
                ? ProjectOopHirNodeKind::RuntimeTypeTest
                : ProjectOopHirNodeKind::SafeReferenceCast;
        }
        else if (runtime != nullptr &&
            runtime->kind == MethodRuntimeTypeOperationKind::CheckedCast)
        {
            nodeKind = ProjectOopHirNodeKind::CheckedReferenceCast;
        }
        auto conversion = makeNode(nodeKind, frame.syntax, resultType);
        if (runtime != nullptr)
        {
            conversion->targetTypeOrdinal = runtime->targetTypeOrdinal;
            conversion->targetIsInterface = runtime->targetIsInterface;
        }
        conversion->children.push_back(std::move(lowered));
        lowered = std::move(conversion);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledMatchFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::Match)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            const LoopBlockAdvance advance = completeInitialMatchFrame(
                frame, frames, current, currentMemberOrdinal,
                currentAddressContext, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
            if (advance == LoopBlockAdvance::Error)
            {
                break;
            }
            if (advance == LoopBlockAdvance::Complete) continue;
            descend = true;
            break;
        }
        if (frame.stage == 1U)
        {
            frame.matchArm->children.push_back(std::move(lowered));
            const ast::AstNode* guard =
                frame.syntax->children()[frame.armIndex]->second();
            if (guard != nullptr)
            {
                frame.stage = 2U;
                current = guard;
                currentMemberOrdinal = frame.memberOrdinal;
                currentAddressContext = false;
                descend = true;
                break;
            }
            auto defaultGuard = makeNode(
                ProjectOopHirNodeKind::BooleanConstant,
                frame.syntax->children()[frame.armIndex],
                "builtin:bool");
            defaultGuard->booleanValue = true;
            frame.matchArm->children.push_back(std::move(defaultGuard));
            if (!beginMatchValue(
                    frame, frames, current, currentMemberOrdinal,
                    currentAddressContext, currentInputKind, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                lowered.reset();
                frames.clear();
                break;
            }
            descend = true;
            break;
        }
        if (frame.stage == 2U)
        {
            frame.matchArm->children.push_back(std::move(lowered));
            if (!beginMatchValue(
                    frame, frames, current, currentMemberOrdinal,
                    currentAddressContext, currentInputKind, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                lowered.reset();
                frames.clear();
                break;
            }
            descend = true;
            break;
        }
        if (frame.stage == 3U)
        {
            activateLocalCleanup(lowered);
            frame.matchArm->children.push_back(std::move(lowered));
            if (!advanceMatchDeclarations(
                    frame, frames, current, currentMemberOrdinal,
                    currentAddressContext, currentInputKind, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                lowered.reset();
                frames.clear();
                break;
            }
            descend = true;
            break;
        }
        const LoopBlockAdvance armAdvance = finishScheduledMatchArm(
            frame, frames, current, currentMemberOrdinal,
            currentAddressContext, lowered, error, rootCleanupDepth,
            rootExceptionHandlerDepth);
        if (armAdvance == LoopBlockAdvance::Error)
        {
            break;
        }
        if (armAdvance == LoopBlockAdvance::Descend)
        {
            descend = true;
            break;
        }
        finishMatch(frame, lowered);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeInitialMatchFrame(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    frame.matchNode->children.push_back(std::move(lowered));
    if (frame.syntax->children().empty())
    {
        finishMatch(frame, lowered);
        frames.pop_back();
        return LoopBlockAdvance::Complete;
    }
    if (beginMatchArm(
            frame, current, currentMemberOrdinal, currentAddressContext, error))
        return LoopBlockAdvance::Descend;
    rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
    lowered.reset();
    frames.clear();
    return LoopBlockAdvance::Error;
}

LoopBlockAdvance ProjectOopHirModel::finishScheduledMatchArm(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    const ast::AstNode* armSyntax =
        frame.syntax->children()[frame.armIndex];
    if (!frame.matchBinding->referenceResult &&
        frame.armIndex < frame.matchBinding->armConversions.size())
    {
        const ProjectImplicitConversion& conversion =
            frame.matchBinding->armConversions[frame.armIndex];
        if (conversion.exists() &&
            conversion.kind != ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered), conversion, armSyntax->third());
        }
    }
    frame.matchArm->children.push_back(std::move(lowered));
    frame.matchArm->cleanupRegionOrdinals.assign(
        activeHirCleanupRegions_.begin() +
            static_cast<std::ptrdiff_t>(frame.cleanupDepth),
        activeHirCleanupRegions_.end());
    activeHirCleanupRegions_.resize(frame.cleanupDepth);
    frame.cleanupActive = false;
    frame.matchNode->children.push_back(std::move(frame.matchArm));
    ++frame.armIndex;
    if (frame.armIndex >= frame.syntax->children().size())
        return LoopBlockAdvance::Complete;
    if (beginMatchArm(
            frame, current, currentMemberOrdinal, currentAddressContext, error))
        return LoopBlockAdvance::Descend;
    rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
    lowered.reset();
    frames.clear();
    return LoopBlockAdvance::Error;
}

}
