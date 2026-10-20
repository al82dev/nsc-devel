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
ProjectOopHirModel::initializeScheduledAssignmentFrame(
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
    if (currentKind == IterativeExpressionKind::DeconstructionAssignment)
        return initializeScheduledDeconstructionAssignmentFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::EventSubscription)
        return initializeScheduledEventSubscriptionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::PropertyIndexerSetter)
        return initializeScheduledPropertyIndexerSetterFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::NamespaceExtensionSetter)
        return initializeScheduledNamespaceExtensionSetterFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::NullConditionalFieldAssignment)
        return initializeScheduledNullConditionalFieldAssignmentFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::NullConditionalArrayAssignment)
        return initializeScheduledNullConditionalArrayAssignmentFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::DirectAssignment ||
    currentKind == IterativeExpressionKind::CompoundAssignment ||
    currentKind == IterativeExpressionKind::StringCompoundAssignment)
        return initializeScheduledDirectAssignmentFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledDeconstructionAssignmentFrame(
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
    frame.deconstructionBinding =
        methodBodyModel_->deconstructionBinding(current);
    frame.materializeReceiverTemporary =
        frame.deconstructionBinding->selectedMemberOrdinal.has_value() &&
        isStructValueType(frame.deconstructionBinding->sourceTypeKey) &&
        !isNaturallyAddressable(current->second());

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledEventSubscriptionFrame(
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
    frame.eventSubscriptionBinding = eventBinding(current);
    const MethodEventBinding& binding = *frame.eventSubscriptionBinding;
    if (!binding.customAccessorMemberOrdinal.has_value() &&
        (memberModel_ == nullptr ||
         binding.backingMemberOrdinal >= memberModel_->members().size()))
    {
        error = "Stage 42 event binding has invalid backing member";
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    if (!binding.staticEvent && binding.receiverTypeKey.empty())
    {
        error = "Stage 40 receiver has no semantic class/interface/struct type";
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    if (!binding.staticEvent &&
        (binding.receiverSyntax == nullptr ||
         binding.receiverSyntax->kind() == ast::AstNodeKind::BaseExpression))
    {
        if (currentLambda_ != nullptr)
        {
            const std::optional<std::size_t> capture =
                currentLambdaThisCapture();
            if (!capture.has_value())
            {
                error = "Stage 43 implicit instance receiver has no captured this";
                rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            frame.first = makeNode(
                isStructValueType(binding.receiverTypeKey)
                    ? ProjectOopHirNodeKind::ClosureCaptureAddress
                    : ProjectOopHirNodeKind::ClosureCaptureValue,
                binding.receiverSyntax,
                binding.receiverTypeKey);
            frame.first->integerValue = *capture;
        }
        else
        {
            frame.first = makeNode(
                ProjectOopHirNodeKind::ThisValue,
                binding.receiverSyntax,
                binding.receiverTypeKey);
        }
        frame.stage = 1U;
    }
    else if (!binding.staticEvent)
    {
        frame.materializeReceiverTemporary =
            isStructValueType(binding.receiverTypeKey) &&
            !isNaturallyAddressable(binding.receiverSyntax);
    }
    if (binding.staticEvent) frame.stage = 1U;
    if (frame.stage == 1U &&
        !binding.customAccessorMemberOrdinal.has_value())
    {
        const ClassMemberSymbol& backing =
            memberModel_->members()[binding.backingMemberOrdinal];
        auto addressNode = makeNode(
            binding.staticEvent
                ? ProjectOopHirNodeKind::StaticFieldAddress
                : ProjectOopHirNodeKind::FieldAddress,
            current,
            backing.declaredTypeKey);
        addressNode->memberOrdinal = binding.backingMemberOrdinal;
        if (!binding.staticEvent)
            addressNode->children.push_back(std::move(frame.first));
        frame.first = std::move(addressNode);
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledPropertyIndexerSetterFrame(
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
    frame.propertySetterBinding = callBinding(current);
    frame.callTarget = resolveBoundCallTarget(*frame.propertySetterBinding);
    const ClassMemberSymbol& setter = memberModel_->members()[
        *frame.propertySetterBinding->selectedMemberOrdinal];
    if (setter.isIndexerAccessor && current->first() != nullptr &&
        current->first()->kind() == ast::AstNodeKind::IndexExpression)
    {
        const ast::AstNode* indexExpression = current->first();
        if (!indexExpression->children().empty())
        {
            frame.operandSyntaxes.assign(
                indexExpression->children().begin(),
                indexExpression->children().end());
        }
        else if (indexExpression->second() != nullptr)
        {
            frame.operandSyntaxes.push_back(indexExpression->second());
        }
    }
    frame.operandSyntaxes.push_back(current->second());
    frame.operands.reserve(frame.operandSyntaxes.size());
    frame.compoundSetter = current->tokenKind() != lexer::TokenKind::Equal;
    if (frame.compoundSetter)
    {
        if (currentAddressContext)
        {
            error = "the result of a compound assignment is a value, not addressable storage";
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        frame.compoundOperator =
            lexer::compoundAssignmentBaseOperator(current->tokenKind());
        if (frame.compoundOperator == lexer::TokenKind::Invalid)
        {
            error = "unsupported compound assignment operator on a property/indexer";
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
    }
    else
    {
        const std::optional<bool> receiverReady =
            prepareSetterReceiver(frame, error);
        if (!receiverReady.has_value())
        {
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        frame.stage = *receiverReady ? 1U : 0U;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledNamespaceExtensionSetterFrame(
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
    frame.extensionSetterBinding = namespaceFunctionCallBinding(current);
    frame.namespaceFunctionOrdinal =
        *frame.extensionSetterBinding->selectedFunctionOrdinal;
    const NamespaceFunctionSymbol& target = namespaceFunctionModel_->functions()[
        frame.namespaceFunctionOrdinal];
    if (currentAddressContext)
    {
        error = "the result of an extension-property assignment is a value, not addressable storage";
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    if (frame.extensionSetterBinding->forwardedReceiver &&
        (frame.extensionSetterBinding->receiverSyntax == nullptr ||
         target.parameters.empty()))
    {
        error = "extension setter lost its synthetic receiver contract";
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    frame.stage = frame.extensionSetterBinding->forwardedReceiver ? 0U : 1U;

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledNullConditionalFieldAssignmentFrame(
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
    frame.fieldBinding = nameBinding(current->first());
    frame.stage = 0U;

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledNullConditionalArrayAssignmentFrame(
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
    frame.core = coreBinding(current->first());
    frame.stage = 0U;

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledDirectAssignmentFrame(
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
    const MethodExpressionTypeBinding* assignmentType = expressionType(current);
    const MethodDelegateBinding* assignmentDelegate = delegateBinding(current);
    frame.assignmentResultType = assignmentType != nullptr
        ? assignmentType->typeKey
        : assignmentDelegate != nullptr ? assignmentDelegate->resultTypeKey : "<error>";
    frame.assignmentOperation = current->tokenKind();
    frame.assignmentOwnsStorage = assignmentType != nullptr &&
        assignmentType->ownsStorageDestination;
    if (currentKind == IterativeExpressionKind::DirectAssignment)
    {
        const MethodNameBinding* targetBinding = nameBinding(current->first());
        if (current->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            targetBinding != nullptr && targetBinding->localOrdinal.has_value() &&
            methodBodyModel_ != nullptr &&
            *targetBinding->localOrdinal < methodBodyModel_->locals().size() &&
            (methodBodyModel_->locals()[*targetBinding->localOrdinal].isRaii ||
             methodBodyModel_->locals()[*targetBinding->localOrdinal].requiresValueDrop))
        {
            frame.assignmentOwnsStorage = true;
            frame.assignmentLocalOrdinal = targetBinding->localOrdinal;
        }
        if (!frame.assignmentOwnsStorage && memberModel_ != nullptr &&
            memberModel_->typeRequiresValueDrop(frame.assignmentResultType))
            frame.assignmentOwnsStorage = true;
    }
    else
    {
        if (currentAddressContext)
        {
            error = "the result of a compound assignment is a value, not addressable storage";
            rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
            return LoopBlockAdvance::Error;
        }
        if (currentKind == IterativeExpressionKind::StringCompoundAssignment)
        {
            const MethodNameBinding* targetBinding = nameBinding(current->first());
            if (targetBinding == nullptr || !targetBinding->localOrdinal.has_value())
            {
                error = "string += requires an identifier-backed RAII string destination";
                rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
            frame.assignmentLocalOrdinal = targetBinding->localOrdinal;
        }
        else
        {
            frame.operandType = expressionType(current->first());
            if (frame.operandType == nullptr)
            {
                error = "Stage 34 compound assignment target has no resolved semantic type";
                rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
                return LoopBlockAdvance::Error;
            }
        }
    }
    frame.stage = 0U;

    return LoopBlockAdvance::Complete;
}

}
