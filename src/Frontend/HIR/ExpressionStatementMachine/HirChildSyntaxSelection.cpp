#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;
using internal::AggregateCrossFramePhase;

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledLoopAwaitChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::LoopExpression ||
        currentKind == IterativeExpressionKind::AwaitExpression ||
        isBoundCallKind(currentKind)))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::LoopExpression)
    current = current->first();
else if (currentKind == IterativeExpressionKind::AwaitExpression)
    current = current->first();
else if (isBoundCallKind(currentKind))
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextBoundCallChild(
            *frames.back().boundCall,
            childSyntax,
            childAddressContext))
    {
        error = "Stage 34 bound-call frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = childAddressContext;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledNamespaceEventChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::NamespaceCall ||
        currentKind == IterativeExpressionKind::EventInvoke))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::NamespaceCall)
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextNamespaceCallChild(
            *frames.back().namespaceCall,
            childSyntax,
            childAddressContext))
    {
        error = "Stage 34 namespace-call frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = childAddressContext;
}
else if (currentKind == IterativeExpressionKind::EventInvoke)
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextEventInvokeChild(
            *frames.back().eventInvoke,
            childSyntax,
            childAddressContext))
    {
        error = "Stage 34 event-invoke frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = childAddressContext;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledDelegateConsoleChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::DelegateInvoke ||
        currentKind == IterativeExpressionKind::ConsoleOutputCall ||
        currentKind == IterativeExpressionKind::ConsoleReadLineCall ||
        currentKind == IterativeExpressionKind::EnvironmentExitCall ||
        currentKind == IterativeExpressionKind::EnumToStringCall ||
        currentKind == IterativeExpressionKind::StructCloneCall ||
        currentKind == IterativeExpressionKind::StructAggregateCall))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::DelegateInvoke)
{
    const ast::AstNode* childSyntax = nullptr;
    if (!nextDelegateInvokeChild(frames.back(), childSyntax))
    {
        error = "Stage 34 delegate-invoke frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::ConsoleOutputCall ||
    currentKind == IterativeExpressionKind::ConsoleReadLineCall ||
    currentKind == IterativeExpressionKind::EnvironmentExitCall)
{
    current = frames.back().syntax->children().front();
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::EnumToStringCall)
{
    current = frames.back().enumToStringCall->receiverSyntax;
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::StructCloneCall)
{
    current = frames.back().operandSyntaxes.front();
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::StructAggregateCall)
{
    current = frames.back().structAggregateCall->receiverSyntax;
    currentAddressContext = false;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledCoreOperandChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::AssertionCore ||
        currentKind == IterativeExpressionKind::IteratorCoreCall ||
        currentKind == IterativeExpressionKind::CoreOperandCall))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::AssertionCore)
{
    current = current->children().front();
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::IteratorCoreCall)
{
    current = frames.back().operandSyntaxes.front();
    currentAddressContext = frames.back().operandAddressContext;
}
else if (currentKind == IterativeExpressionKind::CoreOperandCall)
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextCoreOperandCallChild(
            *frames.back().coreOperandCall,
            childSyntax,
            childAddressContext))
    {
        error = "Stage 34 core operand-call frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = childAddressContext;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledAtomicMonitorChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::AtomicCoreCall ||
        currentKind == IterativeExpressionKind::MonitorCoreCall))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::AtomicCoreCall)
{
    const ast::AstNode* childSyntax = nullptr;
    if (!nextAtomicCoreCallChild(frames.back(), childSyntax))
    {
        error = "Stage 34 atomic core-call frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::MonitorCoreCall)
{
    const ast::AstNode* childSyntax = nullptr;
    if (!nextMonitorCoreCallChild(
            *frames.back().monitorCoreCall, childSyntax))
    {
        error = "Stage 34 monitor core-call frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = false;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledOrdinaryCallChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::OrdinaryCall))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::OrdinaryCall)
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextOrdinaryCallChild(
            *frames.back().ordinaryCall,
            childSyntax,
            childAddressContext))
    {
        error = "Stage 34 ordinary-call frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = childAddressContext;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledAggregateChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::AggregateCrossLowering))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::AggregateCrossLowering)
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextAggregateCrossChild(
            *frames.back().aggregateCross,
            childSyntax,
            childAddressContext))
    {
        if (frames.back().aggregateCross->phase !=
            AggregateCrossFramePhase::Ready)
        {
            error = "Stage 34 aggregate frame has no child to lower";
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = current->range();
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        lowered = finishAggregateCrossFrame(frames.back());
        frames.pop_back();
        completedWithoutChild = true;
    }
    else
    {
        current = childSyntax;
        currentAddressContext = childAddressContext;
    }
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledOrdinaryConstructionChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::OrdinaryConstruction))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::OrdinaryConstruction)
{
    const ast::AstNode* childSyntax = nullptr;
    if (!nextOrdinaryConstructionChild(
            *frames.back().ordinaryConstruction,
            childSyntax))
    {
        lowered = finishOrdinaryConstructionFrame(frames.back());
        if (lowered == nullptr)
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = current->range();
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        frames.pop_back();
        completedWithoutChild = true;
    }
    else
    {
        current = childSyntax;
        currentAddressContext = false;
    }
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledAllocatorChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::AllocatorConstruction))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::AllocatorConstruction)
{
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextAllocatorConstructionChild(
            *frames.back().allocatorConstruction,
            childSyntax,
            childAddressContext))
    {
        error = "Stage 34 allocator-construction frame has no child to lower";
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    current = childSyntax;
    currentAddressContext = childAddressContext;
}
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledReceiverChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::DelegateCreationReceiver ||
        currentKind == IterativeExpressionKind::ExtensionPropertyGetter ||
        currentKind == IterativeExpressionKind::InterpolationHandler ||
        currentKind == IterativeExpressionKind::CollectionBuilder ||
        currentKind == IterativeExpressionKind::ArrayConstruction ||
        currentKind == IterativeExpressionKind::InstanceField ||
        currentKind == IterativeExpressionKind::RangeSlice))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::DelegateCreationReceiver)
{
    current = frames.back().receiverFrame.syntax;
    currentAddressContext =
        isStructValueType(frames.back().receiverFrame.typeKey) &&
        !frames.back().receiverFrame.materializeTemporary;
}
else if (currentKind == IterativeExpressionKind::ExtensionPropertyGetter)
    current = frames.back().extensionPropertyBinding->receiverSyntax;
else if (currentKind == IterativeExpressionKind::InterpolationHandler)
    current = frames.back().operandSyntaxes.front();
else if (currentKind == IterativeExpressionKind::CollectionBuilder)
{
    current = frames.back().operandSyntaxes.front();
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::ArrayConstruction)
{
    current = frames.back().operandSyntaxes.front();
    currentAddressContext = false;
}
else if (currentKind == IterativeExpressionKind::InstanceField)
    current = frames.back().fieldBinding->receiverSyntax;
else if (currentKind == IterativeExpressionKind::RangeSlice)
{
    current = frames.back().rangeBinding->receiverSyntax;
}
return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::selectScheduledRemainingChild(
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] bool& completedWithoutChild,
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
    if (!(currentKind == IterativeExpressionKind::CoreIndex ||
        currentKind == IterativeExpressionKind::RawIndex ||
        currentKind == IterativeExpressionKind::ResultPropagation ||
        currentKind == IterativeExpressionKind::Match ||
        currentKind == IterativeExpressionKind::DeconstructionAssignment ||
        currentKind == IterativeExpressionKind::EventSubscription ||
        currentKind == IterativeExpressionKind::PropertyIndexerSetter ||
        currentKind == IterativeExpressionKind::NamespaceExtensionSetter ||
        currentKind == IterativeExpressionKind::NullConditionalFieldAssignment ||
        currentKind == IterativeExpressionKind::NullConditionalArrayAssignment ||
        currentKind == IterativeExpressionKind::DirectAssignment ||
        currentKind == IterativeExpressionKind::CompoundAssignment ||
        currentKind == IterativeExpressionKind::StringCompoundAssignment ||
        currentKind == IterativeExpressionKind::MoveProjection))
        return std::nullopt;
if (currentKind == IterativeExpressionKind::CoreIndex ||
    currentKind == IterativeExpressionKind::RawIndex)
    current = current->first();
else if (currentKind == IterativeExpressionKind::ResultPropagation)
    current = frames.back().resultPropagation->operandSyntax;
else if (currentKind == IterativeExpressionKind::Match)
    current = current->first();
else if (currentKind == IterativeExpressionKind::DeconstructionAssignment)
    current = current->second();
else if (currentKind == IterativeExpressionKind::EventSubscription)
    current = frames.back().stage == 0U
        ? frames.back().eventSubscriptionBinding->receiverSyntax
        : current->second();
else if (currentKind == IterativeExpressionKind::PropertyIndexerSetter)
    current = frames.back().compoundSetter
        ? current->first()
        : frames.back().stage == 0U
            ? frames.back().callTarget.receiverSyntax
            : frames.back().operandSyntaxes.front();
else if (currentKind == IterativeExpressionKind::NamespaceExtensionSetter)
    current = frames.back().stage == 0U
        ? frames.back().extensionSetterBinding->receiverSyntax
        : current->second();
else if (currentKind == IterativeExpressionKind::NullConditionalFieldAssignment ||
    currentKind == IterativeExpressionKind::NullConditionalArrayAssignment)
    current = current->first()->first();
else if (currentKind == IterativeExpressionKind::DirectAssignment ||
    currentKind == IterativeExpressionKind::CompoundAssignment ||
    currentKind == IterativeExpressionKind::StringCompoundAssignment)
    current = current->first();
else if (currentKind == IterativeExpressionKind::MoveProjection &&
    !frames.back().move->sourceIsArrayElement &&
    !frames.back().move->sourceIsOwnedField &&
    frames.back().move->sourceIsIteratorCurrent)
    current = current->first()->first();
else
{
    current = current->first();
}
return LoopBlockAdvance::Complete;
}

}
