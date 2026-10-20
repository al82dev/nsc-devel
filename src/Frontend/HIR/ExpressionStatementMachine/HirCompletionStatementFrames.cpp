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
ProjectOopHirModel::completeScheduledStatementFrames(
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
    if (kind == IterativeExpressionKind::VariableDeclaration)
        return completeScheduledVariableDeclarationFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::InlineAssemblyStatement)
        return completeScheduledInlineAssemblyStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::StaticAssertStatement)
        return completeScheduledStaticAssertStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::IfStatement)
        return completeScheduledIfStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::LoopStatement)
        return completeScheduledLoopStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::SwitchStatement)
        return completeScheduledSwitchStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ForStatement)
        return completeScheduledForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ExitValueStatement)
        return completeScheduledExitValueStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::CoroutineValueStatement)
        return completeScheduledCoroutineValueStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::UsingStatement)
        return completeScheduledUsingStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::TryStatement)
        return completeScheduledTryStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ForeachStatement)
        return completeScheduledForeachStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::LoopBlockExpressionStatement)
        return completeScheduledLoopBlockExpressionStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::LoopBlock)
        return completeScheduledLoopBlockFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledVariableDeclarationFrame(
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
        frames.back().kind == IterativeExpressionKind::VariableDeclaration)
    {
        IterativeExpressionFrame& frame = frames.back();
        const ast::AstNode* pendingChildSyntax = nullptr;
        bool pendingChildAddressContext = false;
        if (!nextVariableDeclarationChild(
                frame,
                pendingChildSyntax,
                pendingChildAddressContext) ||
            !applyExpressionBoundaryPostconditions(
                lowered,
                pendingChildSyntax,
                frame.memberOrdinal,
                error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            return LoopBlockAdvance::Error;
        }
        if (!acceptVariableDeclarationChild(
                frame, std::move(lowered), error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            return LoopBlockAdvance::Error;
        }
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextVariableDeclarationChild(
                frame, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        lowered = finishVariableDeclarationFrame(frame);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledInlineAssemblyStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::InlineAssemblyStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        InlineAssemblyFrameState& state = *frame.inlineAssembly;
        const ast::AstNode* record = state.pendingOperand;
        const ast::AstNode* operandSyntax =
            record != nullptr ? record->first() : nullptr;
        if (record == nullptr || operandSyntax == nullptr ||
            !applyExpressionBoundaryPostconditions(
                lowered, operandSyntax, frame.memberOrdinal, error) ||
            !acceptInlineAssemblyOperand(
                frame, std::move(lowered), error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid())
            {
                lastErrorRange_ = operandSyntax != nullptr
                    ? operandSyntax->range()
                    : frame.syntax->range();
            }
            return LoopBlockAdvance::Error;
        }
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        const bool hasChild = advanceInlineAssemblyFrame(
            frame, childSyntax, childAddressContext, error);
        if (!error.empty())
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            return LoopBlockAdvance::Error;
        }
        if (hasChild)
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        lowered = finishInlineAssemblyFrame(frame);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledStaticAssertStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::StaticAssertStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        const ast::AstNode* assertionSyntax = frame.syntax;
        if (!applyExpressionBoundaryPostconditions(
                lowered,
                assertionSyntax->first(),
                frame.memberOrdinal,
                error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        lowered = finishStaticAssertStatement(
            assertionSyntax, std::move(lowered), error);
        frames.pop_back();
        if (lowered == nullptr)
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = assertionSyntax->range();
            return LoopBlockAdvance::Error;
        }
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledIfStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::IfStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            if (!applyExpressionBoundaryPostconditions(
                    lowered, frame.syntax->first(), frame.memberOrdinal, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            currentInputKind = IterativeInputKind::LoopBlock;
            descend = true;
            break;
        }
        if (frame.stage == 1U)
        {
            frame.second = std::move(lowered);
            if (frame.statementConditionFailed)
            {
                error = std::move(frame.statementConditionError);
                lowered.reset();
                frames.pop_back();
                if (resumePendingConditionStatementAfterFailure(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error))
                    return LoopBlockAdvance::Descend;
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            const ast::AstNode* alternative = frame.syntax->third();
            if (alternative == nullptr)
            {
                frame.loopBlockNode->children.push_back(std::move(frame.first));
                frame.loopBlockNode->children.push_back(std::move(frame.second));
                lowered = std::move(frame.loopBlockNode);
                frames.pop_back();
                continue;
            }
            frame.stage = 2U;
            std::optional<LoopBlockAdvance> alternativeAdvance =
                beginIfControlAlternative(
                    frame, frames, alternative, current, currentMemberOrdinal,
                    currentAddressContext, currentInputKind, error,
                    rootCleanupDepth, rootExceptionHandlerDepth);
            if (!alternativeAdvance.has_value())
            {
                alternativeAdvance = beginIfIterationAlternative(
                    frame, frames, alternative, current, currentMemberOrdinal,
                    currentAddressContext, currentInputKind, error,
                    rootCleanupDepth, rootExceptionHandlerDepth);
            }
            if (!alternativeAdvance.has_value())
            {
                alternativeAdvance = beginIfValueAlternative(
                    frame, frames, alternative, current, currentMemberOrdinal,
                    currentAddressContext, currentInputKind, error,
                    rootCleanupDepth, rootExceptionHandlerDepth);
            }
            if (alternativeAdvance.has_value())
            {
                if (*alternativeAdvance == LoopBlockAdvance::Error)
                    return LoopBlockAdvance::Error;
                descend = true;
                break;
            }
            lowered = lowerStatement(
                alternative, frame.memberOrdinal, error);
            if (lowered == nullptr)
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                if (!lastErrorRange_.isValid())
                    lastErrorRange_ = alternative->range();
                return LoopBlockAdvance::Error;
            }
        }
        frame.loopBlockNode->children.push_back(std::move(frame.first));
        frame.loopBlockNode->children.push_back(std::move(frame.second));
        frame.loopBlockNode->children.push_back(std::move(lowered));
        lowered = std::move(frame.loopBlockNode);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::beginIfControlAlternative(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode* alternative,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (alternative->kind() == ast::AstNodeKind::BlockStatement)
    {
        current = alternative;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        return LoopBlockAdvance::Descend;
    }
    IterativeExpressionFrame nested;
    nested.syntax = alternative;
    nested.memberOrdinal = frame.memberOrdinal;
    if (alternative->kind() == ast::AstNodeKind::IfStatement)
    {
        nested.kind = IterativeExpressionKind::IfStatement;
        nested.loopBlockNode = makeNode(
            ProjectOopHirNodeKind::If, alternative, "builtin:void");
    }
    else if (alternative->kind() == ast::AstNodeKind::WhileStatement ||
        alternative->kind() == ast::AstNodeKind::DoWhileStatement)
    {
        nested.kind = IterativeExpressionKind::LoopStatement;
        nested.loopBlockNode = makeNode(
            alternative->kind() == ast::AstNodeKind::DoWhileStatement
                ? ProjectOopHirNodeKind::DoWhile
                : ProjectOopHirNodeKind::While,
            alternative,
            "builtin:void");
    }
    else if (alternative->kind() == ast::AstNodeKind::SwitchStatement)
    {
        if (!initializeSwitchStatementFrame(nested, alternative, error))
        {
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
    }
    else
    {
        return std::nullopt;
    }
    frames.push_back(std::move(nested));
    current = alternative->first();
    currentMemberOrdinal = frames.back().memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::beginIfIterationAlternative(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode* alternative,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    IterativeExpressionFrame nested;
    nested.memberOrdinal = frame.memberOrdinal;
    LoopBlockAdvance advance = LoopBlockAdvance::Complete;
    if (alternative->kind() == ast::AstNodeKind::ForStatement)
    {
        initializeForStatementFrame(nested, alternative);
        frames.push_back(std::move(nested));
        advance = advanceForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
    }
    else if (alternative->kind() == ast::AstNodeKind::ForeachStatement)
    {
        if (!initializeForeachStatementFrame(nested, alternative, error))
        {
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        frames.push_back(std::move(nested));
        advance = advanceForeachStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
    }
    else
    {
        return std::nullopt;
    }
    if (advance != LoopBlockAdvance::Error) return LoopBlockAdvance::Descend;
    rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
    return LoopBlockAdvance::Error;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::beginIfValueAlternative(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode* alternative,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (isExitValueStatement(alternative))
    {
        if (!beginExitValueStatement(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, error, alternative, frame.memberOrdinal))
        {
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
    }
    else if (isCoroutineValueStatement(alternative))
    {
        beginCoroutineValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, alternative, frame.memberOrdinal);
    }
    else if (alternative->kind() == ast::AstNodeKind::UsingStatement)
    {
        if (!beginUsingStatement(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, error, alternative, frame.memberOrdinal))
        {
            rollbackMatchCleanups(
                frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
    }
    else if (alternative->kind() == ast::AstNodeKind::TryStatement)
    {
        beginTryStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, alternative, frame.memberOrdinal);
    }
    else
    {
        return std::nullopt;
    }
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledLoopStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::LoopStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            if (!applyExpressionBoundaryPostconditions(
                    lowered, frame.syntax->first(), frame.memberOrdinal, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            currentInputKind = IterativeInputKind::LoopBlock;
            descend = true;
            break;
        }
        frame.second = std::move(lowered);
        if (frame.statementConditionFailed)
        {
            error = std::move(frame.statementConditionError);
            lowered.reset();
            frames.pop_back();
            if (resumePendingConditionStatementAfterFailure(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error))
                    return LoopBlockAdvance::Descend;
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        frame.loopBlockNode->children.push_back(std::move(frame.first));
        frame.loopBlockNode->children.push_back(std::move(frame.second));
        lowered = std::move(frame.loopBlockNode);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledSwitchStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::SwitchStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            if (!applyExpressionBoundaryPostconditions(
                    lowered, frame.syntax->first(), frame.memberOrdinal, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            frame.switchStatement->node->children.push_back(
                std::move(lowered));
            frame.stage = 1U;
        }
        else
        {
            activateDirectStatementCleanup(lowered);
            frame.switchStatement->sectionBody->children.push_back(
                std::move(lowered));
            frame.stage = 1U;
        }
        const LoopBlockAdvance advance =
                            advanceSwitchStatementFrame(
                                frames,
                                current,
                                currentMemberOrdinal,
                                currentAddressContext,
                                currentInputKind,
                                lowered,
                                error,
                                frames.size() - 1U);
        if (advance == LoopBlockAdvance::Error)
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        if (advance == LoopBlockAdvance::Descend)
        {
            descend = true;
            break;
        }
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledForStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::ForStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        const ast::AstNode* childSyntax = nullptr;
        bool expressionBoundary = false;
        if (frame.stage == 1U)
        {
            childSyntax = frame.syntax->first();
            expressionBoundary = childSyntax != nullptr &&
                childSyntax->kind() != ast::AstNodeKind::BlockStatement &&
                childSyntax->kind() !=
                    ast::AstNodeKind::VariableDeclarationStatement;
        }
        else if (frame.stage == 2U)
        {
            childSyntax = frame.syntax->second();
            expressionBoundary = childSyntax != nullptr;
        }
        else if (frame.stage == 3U)
        {
            childSyntax = frame.syntax->third();
            expressionBoundary = childSyntax != nullptr &&
                childSyntax->kind() != ast::AstNodeKind::BlockStatement;
        }
        if (expressionBoundary &&
            !applyExpressionBoundaryPostconditions(
                lowered, childSyntax, frame.memberOrdinal, error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        if (frame.stage == 1U)
            activateDirectStatementCleanup(lowered);
        frame.forStatement->node->children.push_back(std::move(lowered));
        if (frame.stage == 4U)
        {
            activeHirCleanupRegions_.resize(frame.cleanupDepth);
            frame.cleanupActive = false;
            lowered = std::move(frame.forStatement->node);
            frames.pop_back();
            continue;
        }
        const LoopBlockAdvance advance =
                            advanceForStatementFrame(
                                frames,
                                current,
                                currentMemberOrdinal,
                                currentAddressContext,
                                currentInputKind,
                                error,
                                frames.size() - 1U);
        if (advance == LoopBlockAdvance::Error)
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        if (advance == LoopBlockAdvance::Descend)
        {
            descend = true;
            break;
        }
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledExitValueStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::ExitValueStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        const ast::AstNode* valueSyntax = frame.syntax->first();
        if (!applyExpressionBoundaryPostconditions(
                lowered, valueSyntax, frame.memberOrdinal, error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        ExitValueStatementFrameState& state = *frame.exitValueStatement;
        switch (frame.syntax->kind())
        {
            case ast::AstNodeKind::BreakStatement:
                if (state.loopBreakBinding->conversion.exists() &&
                    state.loopBreakBinding->conversion.kind !=
                        ProjectConversionKind::Identity)
                {
                    lowered = wrapImplicitConversion(
                        std::move(lowered),
                        state.loopBreakBinding->conversion,
                        valueSyntax);
                }
                state.node->elementTypeKey =
                    state.loopBreakBinding->resultTypeKey;
                break;

            case ast::AstNodeKind::ReturnStatement:
                if (state.valueReturningCallable &&
                    state.declaredTypeKey != "builtin:void" &&
                    !state.referenceReturn)
                {
                    const MethodExpressionTypeBinding* type =
                        expressionType(valueSyntax);
                    const ProjectImplicitConversion conversion =
                        classifyImplicit(
                            lowered->typeKey,
                            state.declaredTypeKey,
                            type != nullptr
                                ? type->integerConstant : std::nullopt);
                    if (conversion.exists() &&
                        conversion.kind != ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered), conversion, valueSyntax);
                    }
                    if (state.declaredOwnership ==
                        sema::OwnershipKind::Borrowed)
                    {
                        const MethodBorrowBinding* borrow =
                            borrowBinding(valueSyntax);
                        if (borrow == nullptr)
                        {
                            error = "Stage 50.8 borrowed return has no frozen borrow binding";
                            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                            return LoopBlockAdvance::Error;
                        }
                        auto borrowed = makeNode(
                            ProjectOopHirNodeKind::BorrowValue,
                            valueSyntax,
                            state.declaredTypeKey);
                        borrowed->borrowKind = borrow->kind;
                        borrowed->localOrdinal =
                            borrow->sourceLocalOrdinal;
                        borrowed->parameterOrdinal =
                            borrow->sourceParameterOrdinal;
                        borrowed->children.push_back(std::move(lowered));
                        lowered = std::move(borrowed);
                    }
                }
                if (frame.syntax->flag())
                {
                    if (lowered->kind != ProjectOopHirNodeKind::InstanceCall &&
                        lowered->kind != ProjectOopHirNodeKind::StaticCall)
                    {
                        error = "T08-F06 validated must_tail return did not lower to a direct call";
                        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                        return LoopBlockAdvance::Error;
                    }
                    lowered->mustTailCall = true;
                }
                break;

            case ast::AstNodeKind::ThrowStatement:
                break;

            default:
                error = "non-value exit statement reached iterative HIR exit completion";
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
        }
        state.node->children.push_back(std::move(lowered));
        lowered = std::move(state.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCoroutineValueStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::CoroutineValueStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (!applyExpressionBoundaryPostconditions(
                lowered, frame.syntax->first(), frame.memberOrdinal, error))
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        CoroutineValueStatementFrameState& state =
            *frame.coroutineValueStatement;
        state.node->children.push_back(std::move(lowered));
        lowered = std::move(state.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledUsingStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::UsingStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        UsingStatementFrameState& state = *frame.usingStatement;
        if (frame.stage == 0U)
        {
            const ast::AstNode* initializerSyntax = frame.syntax->first();
            if (!applyExpressionBoundaryPostconditions(
                    lowered, initializerSyntax, frame.memberOrdinal, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            const MethodExpressionTypeBinding* sourceType =
                expressionType(initializerSyntax);
            const ProjectImplicitConversion conversion = classifyImplicit(
                lowered->typeKey,
                state.binding->resourceTypeKey,
                sourceType != nullptr
                    ? sourceType->integerConstant : std::nullopt);
            if (conversion.exists() &&
                conversion.kind != ProjectConversionKind::Identity)
            {
                lowered = wrapImplicitConversion(
                    std::move(lowered), conversion, initializerSyntax);
            }
            state.node->children.push_back(std::move(lowered));
            frame.cleanupDepth = activeHirCleanupRegions_.size();
            activeHirCleanupRegions_.push_back(state.cleanupOrdinal);
            frame.cleanupActive = true;
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            currentInputKind = IterativeInputKind::LoopBlock;
            descend = true;
            break;
        }

        state.node->children.push_back(std::move(lowered));
        activeHirCleanupRegions_.resize(frame.cleanupDepth);
        frame.cleanupActive = false;
        auto continuation = makeNode(
            ProjectOopHirNodeKind::CleanupContinuation,
            frame.syntax,
            "builtin:void");
        continuation->cleanupRegionOrdinal = state.cleanupOrdinal;
        continuation->cleanupContinuationKind =
            MethodCleanupContinuationKind::Normal;
        state.node->children.push_back(std::move(continuation));
        lowered = std::move(state.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledTryStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::TryStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        TryStatementFrameState& state = *frame.tryStatement;
        if (frame.stage == 0U)
        {
            if (state.handlerActive)
            {
                activeHirExceptionHandlers_.resize(
                    state.exceptionHandlerDepth);
                state.handlerActive = false;
            }
            state.node->children.push_back(std::move(lowered));
        }
        else if (frame.stage == 1U)
        {
            const ast::AstNode* filterSyntax =
                state.catchSyntax != nullptr
                ? state.catchSyntax->second()
                : nullptr;
            if (!applyExpressionBoundaryPostconditions(
                    lowered, filterSyntax, frame.memberOrdinal, error))
            {
                rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            state.regionNode->children.push_back(std::move(lowered));
            frame.stage = 2U;
            current = state.catchSyntax != nullptr
                ? state.catchSyntax->first()
                : nullptr;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            currentInputKind = IterativeInputKind::LoopBlock;
            descend = true;
            break;
        }
        else if (frame.stage == 2U)
        {
            state.regionNode->children.push_back(std::move(lowered));
            state.node->children.push_back(std::move(state.regionNode));
            state.catchSyntax = nullptr;
        }
        else
        {
            state.regionNode->children.push_back(std::move(lowered));
            auto continuation = makeNode(
                ProjectOopHirNodeKind::CleanupContinuation,
                frame.syntax,
                "builtin:void");
            continuation->cleanupRegionOrdinal = state.cleanupOrdinal;
            continuation->cleanupContinuationKind =
                MethodCleanupContinuationKind::Normal;
            state.regionNode->children.push_back(
                std::move(continuation));
            state.node->children.push_back(std::move(state.regionNode));
            lowered = std::move(state.node);
            frames.pop_back();
            continue;
        }

        if (const auto catchAdvance = beginNextTryCatch(
                frame, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
            catchAdvance.has_value())
        {
            if (*catchAdvance == LoopBlockAdvance::Error)
                return LoopBlockAdvance::Error;
            descend = true;
            break;
        }
        if (const auto finallyAdvance = beginTryFinally(
                frame, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
            finallyAdvance.has_value())
        {
            if (*finallyAdvance == LoopBlockAdvance::Error)
                return LoopBlockAdvance::Error;
            descend = true;
            break;
        }

        lowered = std::move(state.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::beginNextTryCatch(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    TryStatementFrameState& state = *frame.tryStatement;
    if (state.nextCatch >= frame.syntax->children().size())
        return std::nullopt;
    state.catchSyntax = frame.syntax->children()[state.nextCatch++];
    const MethodExceptionBinding* catchBinding =
        exceptionBinding(state.catchSyntax);
    if (catchBinding == nullptr ||
        catchBinding->kind != MethodExceptionOperationKind::Catch)
    {
        error = "Stage 45.6 catch clause has no Stage 45.2 catch binding";
        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    state.regionNode = makeNode(
        ProjectOopHirNodeKind::CatchRegion,
        state.catchSyntax,
        "builtin:void");
    state.regionNode->elementTypeKey = catchBinding->exceptionTypeKey;
    state.regionNode->exceptionTypeOrdinal = catchBinding->exceptionTypeOrdinal;
    state.regionNode->catchAll = catchBinding->catchAll;
    state.regionNode->catchHasFilter = catchBinding->hasFilter;
    state.regionNode->catchExhaustive = catchBinding->exhaustive;
    state.regionNode->catchMatchedErrorTypeKeys =
        catchBinding->matchedErrorTypeKeys;
    if (const MethodLocalSymbol* local = localSymbol(state.catchSyntax);
        local != nullptr)
    {
        state.regionNode->localOrdinal = static_cast<std::size_t>(
            local - methodBodyModel_->locals().data());
        state.regionNode->stringValue = local->name;
    }
    frame.stage = catchBinding->hasFilter ? 1U : 2U;
    current = catchBinding->hasFilter
        ? state.catchSyntax->second() : state.catchSyntax->first();
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    currentInputKind = catchBinding->hasFilter
        ? IterativeInputKind::Expression : IterativeInputKind::LoopBlock;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::beginTryFinally(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    TryStatementFrameState& state = *frame.tryStatement;
    if (frame.cleanupActive)
    {
        activeHirCleanupRegions_.resize(frame.cleanupDepth);
        frame.cleanupActive = false;
    }
    if (frame.syntax->second() == nullptr) return std::nullopt;
    if (!state.cleanupOrdinal.has_value())
    {
        error = "Stage 45.6 finally block has no cleanup region";
        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    state.regionNode = makeNode(
        ProjectOopHirNodeKind::FinallyRegion,
        frame.syntax->second(),
        "builtin:void");
    state.regionNode->cleanupRegionOrdinal = state.cleanupOrdinal;
    frame.stage = 3U;
    current = frame.syntax->second();
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::LoopBlock;
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledForeachStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::ForeachStatement)
    {
        IterativeExpressionFrame& frame = frames.back();
        ForeachStatementFrameState& state = *frame.foreachStatement;
        if (frame.stage == 1U)
        {
            state.node->children.push_back(std::move(lowered));
            frame.stage = 2U;
            current = state.chosenCandidate;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            currentInputKind = IterativeInputKind::LoopBlock;
            descend = true;
            break;
        }
        state.node->children.push_back(std::move(lowered));
        lowered = std::move(state.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledLoopBlockExpressionStatementFrame(
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
        frames.back().kind == IterativeExpressionKind::LoopBlockExpressionStatement)
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

LoopBlockAdvance ProjectOopHirModel::completeScheduledLoopBlockFrame(
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
        frames.back().kind == IterativeExpressionKind::LoopBlock)
    {
        IterativeExpressionFrame& frame = frames.back();
        activateLocalCleanup(lowered);
        frame.loopBlockNode->children.push_back(std::move(lowered));
        const ast::AstNode* blockSyntax = frame.syntax;
        const LoopBlockAdvance advance = advanceLoopBlockFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, lowered, error, frames.size() - 1U);
        if (advance == LoopBlockAdvance::Error)
        {
            rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
            if (!lastErrorRange_.isValid()) lastErrorRange_ = blockSyntax->range();
            return LoopBlockAdvance::Error;
        }
        if (advance == LoopBlockAdvance::Descend)
        {
            descend = true;
            break;
        }
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

}
