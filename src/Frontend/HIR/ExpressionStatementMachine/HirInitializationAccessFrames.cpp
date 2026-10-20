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
ProjectOopHirModel::initializeScheduledAccessFrame(
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
    else if (currentKind == IterativeExpressionKind::ExtensionPropertyGetter)
        return initializeScheduledExtensionPropertyGetterFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::UserIndexerGetter)
        return initializeScheduledUserIndexerGetterFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::DelegateCreationReceiver)
        return initializeScheduledDelegateCreationReceiverFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::InterpolationHandler)
        return initializeScheduledInterpolationHandlerFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::CollectionBuilder)
        return initializeScheduledCollectionBuilderFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::ArrayConstruction)
        return initializeScheduledArrayConstructionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::InstanceField ||
    currentKind == IterativeExpressionKind::NullConditionalField)
        return initializeScheduledInstanceFieldFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::RangeSlice)
        return initializeScheduledRangeSliceFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledExtensionPropertyGetterFrame(
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
    frame.extensionPropertyBinding = namespaceFunctionCallBinding(current);
    frame.operandAddressContext =
        *extensionPropertyReceiverAddress(*frame.extensionPropertyBinding);

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledUserIndexerGetterFrame(
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
    const MethodCoreBinding* core = coreBinding(current);
    frame.nullConditionalCall = core != nullptr &&
        core->kind == MethodCoreOperationKind::NullConditionalUserIndex;
    if (!current->children().empty())
    {
        frame.operandSyntaxes.assign(
            current->children().begin(), current->children().end());
    }
    else if (current->second() != nullptr)
    {
        frame.operandSyntaxes.push_back(current->second());
    }
    if (!initializeBoundCallFrame(
            frame,
            callBinding(current),
            std::move(frame.operandSyntaxes),
            frame.nullConditionalCall,
            error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledDelegateCreationReceiverFrame(
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
    frame.delegateCreationBinding = delegateBinding(current);
    const std::optional<bool> receiverReady = initializeReceiverFrame(
        frame.receiverFrame,
        frame.delegateCreationBinding->receiverSyntax,
        frame.delegateCreationBinding->receiverTypeKey,
        error);
    if (!receiverReady.has_value())
    {
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledInterpolationHandlerFrame(
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
    frame.core = coreBinding(current);
    frame.syntheticLocalOrdinal = nextSyntheticLocalOrdinal_++;
    std::string parseError;
    if (!language::parseStaticInterpolation(
            current->text(), frame.interpolationParts, parseError))
    {
        error = std::move(parseError);
        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    const std::size_t holeCount = static_cast<std::size_t>(std::count_if(
        frame.interpolationParts.begin(),
        frame.interpolationParts.end(),
        &ProjectOopHirModel::isInterpolationHole));
    const std::size_t available =
        std::min(holeCount, current->children().size());
    frame.operandSyntaxes.assign(
        current->children().begin(), current->children().begin() + available);
    frame.operands.reserve(available);

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledCollectionBuilderFrame(
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
    frame.core = coreBinding(current);
    frame.syntheticLocalOrdinal = nextSyntheticLocalOrdinal_++;
    frame.operandSyntaxes.assign(
        current->children().begin(), current->children().end());
    frame.operands.reserve(frame.operandSyntaxes.size());

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledArrayConstructionFrame(
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
    frame.core = coreBinding(current);
    frame.first = makeNode(
        frame.core->kind == MethodCoreOperationKind::RangeArrayAllocation
            ? ProjectOopHirNodeKind::RangeArrayAllocation
            : ProjectOopHirNodeKind::ArrayAllocation,
        current, frame.core->resultTypeKey);
    frame.first->elementTypeKey = frame.core->elementTypeKey;
    if (frame.core->kind == MethodCoreOperationKind::RangeArrayAllocation)
    {
        frame.first->nativeArrayRank =
            ProjectTypeConversion::nativeArrayRank(frame.core->resultTypeKey);
        frame.operandSyntaxes.push_back(current->first());
        frame.operandSyntaxes.insert(frame.operandSyntaxes.end(),
            current->children().begin(), current->children().end());
    }
    else
    {
        frame.arraySegmented = std::any_of(
            current->children().begin(), current->children().end(),
            &ProjectOopHirModel::isSpreadElement);
        frame.arrayInferred = current->auxiliaryText() ==
            "inferred-array-initializer";
        frame.first->booleanValue = frame.arrayInferred;
        if (frame.arraySegmented)
        {
            for (const ast::AstNode* child : current->children())
            {
                const bool spread = child != nullptr &&
                    child->kind() == ast::AstNodeKind::SpreadElementExpression;
                frame.arraySpreadOperands.push_back(spread);
                frame.operandSyntaxes.push_back(spread ? child->first() : child);
            }
        }
        else
        {
            if (frame.arrayInferred)
            {
                auto count = makeNode(ProjectOopHirNodeKind::IntegerConstant,
                    current, "builtin:int");
                count->integerValue = current->children().size();
                frame.first->children.push_back(std::move(count));
            }
            else frame.operandSyntaxes.push_back(current->first());
            frame.operandSyntaxes.insert(frame.operandSyntaxes.end(),
                current->children().begin(), current->children().end());
        }
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledInstanceFieldFrame(
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
    frame.fieldBinding = nameBinding(current);
    if (currentKind == IterativeExpressionKind::InstanceField)
    {
        frame.materializeReceiverTemporary =
            isStructValueType(frame.fieldBinding->receiverTypeKey) &&
            !isNaturallyAddressable(frame.fieldBinding->receiverSyntax);
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledRangeSliceFrame(
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
    frame.rangeBinding = rangeSliceBinding(current);
    frame.materializeReceiverTemporary =
        isStructValueType(frame.rangeBinding->receiverTypeKey) &&
        !isNaturallyAddressable(frame.rangeBinding->receiverSyntax);

    return LoopBlockAdvance::Complete;
}

}
