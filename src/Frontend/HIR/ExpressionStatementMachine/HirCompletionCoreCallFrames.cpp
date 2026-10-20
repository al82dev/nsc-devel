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
ProjectOopHirModel::completeScheduledCoreCallFrames(
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
    const IterativeExpressionKind kind = frames.back().kind;
    if (kind == IterativeExpressionKind::AssertionCore)
        return completeScheduledAssertionCoreFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::IteratorCoreCall)
        return completeScheduledIteratorCoreCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::CoreOperandCall)
        return completeScheduledCoreOperandCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::AtomicCoreCall)
        return completeScheduledAtomicCoreCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::MonitorCoreCall)
        return completeScheduledMonitorCoreCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ConsoleOutputCall)
        return completeScheduledConsoleOutputCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ConsoleReadLineCall)
        return completeScheduledConsoleReadLineCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::EnumToStringCall)
        return completeScheduledEnumToStringCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::StructCloneCall)
        return completeScheduledStructCloneCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::StructAggregateCall)
        return completeScheduledStructAggregateCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::EnvironmentExitCall)
        return completeScheduledEnvironmentExitCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledAssertionCoreFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::AssertionCore)
    {
        IterativeExpressionFrame& frame = frames.back();
        const bool hasCondition =
            frame.core->kind == MethodCoreOperationKind::Assert ||
            frame.core->kind == MethodCoreOperationKind::Assume;
        const bool validOperand = hasCondition
            ? lowered->typeKey == "builtin:bool"
            : lowered->typeKey == "builtin:string" ||
                lowered->typeKey == "string-literal";
        if (!validOperand)
        {
            error = hasCondition
                ? "T07-F06 assertion condition lost its bool type"
                : "T07-F06 panic payload lost its string type";
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        frame.first->children.push_back(std::move(lowered));
        lowered = std::move(frame.first);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledIteratorCoreCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::IteratorCoreCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        frame.first->children.push_back(std::move(lowered));
        if (!activeHirExceptionHandlers_.empty())
        {
            frame.first->exceptionTargetRegionOrdinal =
                activeHirExceptionHandlers_.back().hirRegionOrdinal;
        }
        lowered = std::move(frame.first);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCoreOperandCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::CoreOperandCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        acceptCoreOperandCallChild(frame, std::move(lowered));
        CoreOperandCallFrameState& call = *frame.coreOperandCall;
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextCoreOperandCallChild(
                call, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        lowered = std::move(call.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledAtomicCoreCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::AtomicCoreCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (!acceptAtomicCoreCallChild(frame, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        AtomicCoreCallFrameState& call = *frame.atomicCoreCall;
        const ast::AstNode* childSyntax = nullptr;
        if (nextAtomicCoreCallChild(frame, childSyntax))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        lowered = std::move(call.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledMonitorCoreCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::MonitorCoreCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        acceptMonitorCoreCallChild(frame, std::move(lowered));
        MonitorCoreCallFrameState& call = *frame.monitorCoreCall;
        const ast::AstNode* childSyntax = nullptr;
        if (nextMonitorCoreCallChild(call, childSyntax))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        lowered = std::move(call.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledConsoleOutputCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::ConsoleOutputCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        ConsoleOutputCallFrameState& call = *frame.consoleOutputCall;
        const ast::AstNode* argumentSyntax = frame.syntax->children().front();
        if (call.binding != nullptr &&
            !call.binding->selectedConversions.empty() &&
            call.binding->selectedConversions.front().kind !=
                ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered),
                call.binding->selectedConversions.front(),
                argumentSyntax);
        }
        if (call.binding != nullptr &&
            call.binding->selectedMemberOrdinal.has_value())
        {
            lowered = wrapCallArgumentBorrow(
                std::move(lowered),
                argumentSyntax,
                *call.binding->selectedMemberOrdinal,
                0U,
                call.hasOwnedTemporary);
        }
        auto node = makeNode(
            ProjectOopHirNodeKind::CoreConsoleWriteLine,
            frame.syntax,
            "builtin:void");
        node->integerValue = call.mask;
        node->children.push_back(std::move(lowered));
        lowered = wrapFullExpressionCleanup(
            std::move(node), frame.syntax, call.hasOwnedTemporary);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledConsoleReadLineCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::ConsoleReadLineCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        auto node = makeNode(
            ProjectOopHirNodeKind::CoreConsoleReadLine,
            frame.syntax,
            "builtin:string");
        node->children.push_back(std::move(lowered));
        lowered = std::move(node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledEnumToStringCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::EnumToStringCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        EnumToStringCallFrameState& call = *frame.enumToStringCall;
        auto node = makeNode(
            ProjectOopHirNodeKind::EnumName,
            frame.syntax,
            "builtin:string");
        node->children.push_back(std::move(lowered));
        auto fallback = makeNode(
            ProjectOopHirNodeKind::StringLiteral,
            frame.syntax,
            "builtin:string");
        fallback->stringValue = shortTypeName(frame.core->elementTypeKey);
        node->children.push_back(std::move(fallback));
        for (const auto& enumerator : call.enumerators)
        {
            auto constant = makeNode(
                ProjectOopHirNodeKind::IntegerConstant,
                frame.syntax,
                frame.core->elementTypeKey);
            constant->integerValue =
                static_cast<std::uint64_t>(enumerator.first);
            auto name = makeNode(
                ProjectOopHirNodeKind::StringLiteral,
                frame.syntax,
                "builtin:string");
            name->stringValue = enumerator.second;
            node->children.push_back(std::move(constant));
            node->children.push_back(std::move(name));
        }
        lowered = std::move(node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledStructCloneCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::StructCloneCall)
    {
        // Clone is intentionally transparent: return the exact receiver HIR.
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledStructAggregateCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::StructAggregateCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        StructAggregateCallFrameState& call = *frame.structAggregateCall;
        call.node->children.push_back(std::move(lowered));
        if (call.stage == StructAggregateCallFrameStage::Receiver && call.equals)
        {
            call.stage = StructAggregateCallFrameStage::Argument;
            current = frame.syntax->children().front();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        call.node->argumentCount = call.node->children.size();
        if (!appendAggregateLeaves(
                frame.core->elementTypeKey,
                0,
                frame.syntax,
                call.node->children,
                error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        call.stage = StructAggregateCallFrameStage::Ready;
        lowered = std::move(call.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledEnvironmentExitCallFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::EnvironmentExitCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.environmentExitBinding != nullptr &&
            !frame.environmentExitBinding->selectedConversions.empty() &&
            frame.environmentExitBinding->selectedConversions.front().kind !=
                ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered),
                frame.environmentExitBinding->selectedConversions.front(),
                frame.syntax->children().front());
        }
        auto node = makeNode(
            ProjectOopHirNodeKind::CoreEnvironmentExit,
            frame.syntax,
            "builtin:void");
        node->children.push_back(std::move(lowered));
        lowered = std::move(node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

}
