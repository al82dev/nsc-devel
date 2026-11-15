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
ProjectOopHirModel::completeScheduledAssignmentFrames(
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
    if (kind == IterativeExpressionKind::EventSubscription)
        return completeScheduledEventSubscriptionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::PropertyIndexerSetter)
        return completeScheduledPropertyIndexerSetterFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NamespaceExtensionSetter)
        return completeScheduledNamespaceExtensionSetterFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NullConditionalFieldAssignment)
        return completeScheduledNullConditionalFieldAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NullConditionalArrayAssignment)
        return completeScheduledNullConditionalArrayAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::DirectAssignment)
        return completeScheduledDirectAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::StringCompoundAssignment)
        return completeScheduledStringCompoundAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::CompoundAssignment)
        return completeScheduledCompoundAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledEventSubscriptionFrame(
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
        frames.back().kind == IterativeExpressionKind::EventSubscription)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodEventBinding& binding = *frame.eventSubscriptionBinding;
        if (beginEventSubscriptionValue(
                frame, binding, current, currentMemberOrdinal,
                currentAddressContext, lowered))
        {
            descend = true;
            break;
        }
        if (!binding.argumentConversions.empty() &&
            binding.argumentConversions.front().kind !=
                ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered),
                binding.argumentConversions.front(),
                frame.syntax->second());
        }
        finishEventSubscription(frame, binding, lowered);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

// ES: Materializa el receptor del evento y programa el delegado conservando el orden original.
// EN: Materializes the event receiver and schedules the delegate while preserving original order.
// Parámetros / Parameters: frame y binding son la operación; current y lowered transportan su estado.
// Retorno / Return: true sólo al programar el delegado; muta la misma trama; O(1).
bool ProjectOopHirModel::beginEventSubscriptionValue(
    IterativeExpressionFrame& frame,
    const MethodEventBinding& binding,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    if (frame.stage != 0U) return false;
    if (frame.materializeReceiverTemporary)
    {
        auto temporary = makeNode(
            ProjectOopHirNodeKind::TemporaryAddress,
            binding.receiverSyntax,
            binding.receiverTypeKey);
        temporary->elementTypeKey = binding.receiverTypeKey;
        temporary->children.push_back(std::move(lowered));
        lowered = std::move(temporary);
    }
    if (!binding.customAccessorMemberOrdinal.has_value())
    {
        const ClassMemberSymbol& backing =
            memberModel_->members()[binding.backingMemberOrdinal];
        auto addressNode = makeNode(
            ProjectOopHirNodeKind::FieldAddress,
            frame.syntax,
            backing.declaredTypeKey);
        addressNode->memberOrdinal = binding.backingMemberOrdinal;
        addressNode->children.push_back(std::move(lowered));
        lowered = std::move(addressNode);
    }
    frame.first = std::move(lowered);
    frame.stage = 1U;
    current = frame.syntax->second();
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    return true;
}

// ES: Construye la suscripción después de completar receptor y delegado, sin cambiar su ABI HIR.
// EN: Builds the subscription after receiver and delegate completion without changing its HIR ABI.
// Parámetros / Parameters: frame y binding aportan identidad; lowered contiene y recibe el valor.
// Retorno / Return: no aplica; consume los mismos hijos en el mismo orden; O(1).
void ProjectOopHirModel::finishEventSubscription(
    IterativeExpressionFrame& frame,
    const MethodEventBinding& binding,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    auto subscription = makeNode(
        binding.customAccessorMemberOrdinal.has_value()
            ? ProjectOopHirNodeKind::InstanceCall
            : binding.kind == MethodEventOperationKind::Add
                ? ProjectOopHirNodeKind::EventAdd
                : ProjectOopHirNodeKind::EventRemove,
        frame.syntax,
        "builtin:void");
    if (binding.customAccessorMemberOrdinal.has_value())
    {
        subscription->memberOrdinal = *binding.customAccessorMemberOrdinal;
        if (!binding.staticEvent)
            subscription->children.push_back(std::move(frame.first));
        subscription->argumentParameterOrdinals.push_back(0U);
        subscription->argumentTypeKeys.push_back(binding.delegateTypeKey);
    }
    else
    {
        subscription->elementTypeKey = binding.delegateTypeKey;
        subscription->memberOrdinal = binding.backingMemberOrdinal;
        subscription->children.push_back(std::move(frame.first));
    }
    subscription->children.push_back(std::move(lowered));
    lowered = std::move(subscription);
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledPropertyIndexerSetterFrame(
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
        frames.back().kind == IterativeExpressionKind::PropertyIndexerSetter)
    {
        IterativeExpressionFrame& frame = frames.back();
        const auto materializeReceiver = std::bind_front(
            &ProjectOopHirModel::materializeSetterReceiver,
            this,
            std::ref(frame),
            std::ref(lowered));
        if (frame.compoundSetter && frame.stage == 0U)
        {
            frame.second = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        if (frame.compoundSetter && frame.stage == 1U)
        {
            const LoopBlockAdvance advance = completeCompoundSetterValue(
                frame, frames, current, currentMemberOrdinal,
                currentAddressContext, lowered, error);
            if (advance == LoopBlockAdvance::Error) break;
            if (advance == LoopBlockAdvance::Descend)
            {
                descend = true;
                break;
            }
        }
        else if (frame.compoundSetter && frame.stage == 2U)
        {
            materializeReceiver();
            frame.stage = 3U;
        }
        else if (frame.compoundSetter && frame.stage == 3U)
        {
            frame.operands.push_back(std::move(lowered));
        }
        else if (!frame.compoundSetter && frame.stage == 0U)
        {
            materializeReceiver();
            frame.stage = 1U;
        }
        else if (!frame.compoundSetter)
        {
            frame.operands.push_back(std::move(lowered));
        }
        const std::size_t targetCount = frame.compoundSetter
            ? frame.operandSyntaxes.size() - 1U
            : frame.operandSyntaxes.size();
        if (frame.operands.size() < targetCount)
        {
            current = frame.operandSyntaxes[frame.operands.size()];
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = boundCallArgumentNeedsAddress(
                frame.callTarget.memberOrdinal, frame.operands.size());
            descend = true;
            break;
        }
        if (!finishPropertyIndexerSetterFrame(frame, frames, lowered, error)) break;
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

// ES: Finaliza la llamada al setter cuando todos los operandos ya están completos.
// EN: Finalizes the setter call after every operand has completed.
// Parámetros / Parameters: frame conserva la llamada; frames, lowered y error conservan el flujo existente.
// Retorno / Return: true en éxito; en error limpia exactamente el mismo estado; O(número de operandos).
bool ProjectOopHirModel::finishPropertyIndexerSetterFrame(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error)
{
    std::unique_ptr<ProjectOopHirNode> capturedTarget;
    std::optional<MethodCallBinding> capturedSetterBinding;
    const ast::AstNode* receiverSyntax = frame.propertySetterBinding != nullptr
        ? frame.propertySetterBinding->receiverSyntax : nullptr;
    const MethodExpressionTypeBinding* receiverType = receiverSyntax != nullptr
        ? expressionType(receiverSyntax) : nullptr;
    const bool receiverIsTemporaryOwner = receiverType != nullptr &&
        (receiverType->ownershipFreshOwner || receiverType->ownershipTransfer ||
         (receiverType->ownership == sema::OwnershipKind::Owned &&
          receiverSyntax->kind() != ast::AstNodeKind::IdentifierExpression));
    const bool structReceiver = isStructValueType(frame.callTarget.receiverTypeKey);
    const bool capturedOwnedReceiver = receiverIsTemporaryOwner && !structReceiver &&
        frame.first != nullptr && isFreshDroppableTemporary(frame.first.get());
    const bool structReceiverAddress = frame.first != nullptr &&
        (frame.first->kind == ProjectOopHirNodeKind::LocalAddress ||
         frame.first->kind == ProjectOopHirNodeKind::ParameterAddress ||
         frame.first->kind == ProjectOopHirNodeKind::ThisValue ||
         ((frame.first->kind == ProjectOopHirNodeKind::LocalValue ||
           frame.first->kind == ProjectOopHirNodeKind::ParameterValue) &&
          frame.first->typeKey == std::string(frame.callTarget.receiverTypeKey) + "*"));
    const bool scalarOperands = std::ranges::all_of(frame.operands, [&](const auto& operand)
        { return operand != nullptr && (ProjectTypeConversion::isNumeric(operand->typeKey) ||
                                        operand->typeKey == "builtin:bool" ||
                                        (isStructValueType(operand->typeKey) && memberModel_ != nullptr &&
                                         !memberModel_->typeRequiresValueDrop(operand->typeKey)) ||
                                        isReferenceType(operand->typeKey)); });
    if (frame.compoundSetter && !frame.operandSyntaxes.empty() &&
        frame.first != nullptr && frame.first->kind != ProjectOopHirNodeKind::OwnedTemporaryBorrow &&
        !frame.first->automaticDestruction && (!receiverIsTemporaryOwner || capturedOwnedReceiver) &&
        frame.operands.size() + 1 == frame.operandSyntaxes.size() &&
        scalarOperands && (!structReceiver || structReceiverAddress) &&
        frame.setterCombinedValue != nullptr && !frame.setterCombinedValue->children.empty())
    {
        ProjectOopHirNode* getter = frame.setterCombinedValue->children[0].get();
        if (getter != nullptr && getter->kind == ProjectOopHirNodeKind::InstanceCall &&
            getter->memberOrdinal.has_value() && memberModel_ != nullptr &&
            *getter->memberOrdinal < memberModel_->members().size() &&
            memberModel_->members()[*getter->memberOrdinal].parameters.size() == frame.operands.size() &&
            getter->children.size() == frame.operands.size() + 1 && getter->children[0] != nullptr &&
            getter->children[0]->typeKey == frame.first->typeKey)
        {
            bool compatible = true;
            const auto& parameters = memberModel_->members()[*getter->memberOrdinal].parameters;
            const MethodCallBinding* getterBinding = callBinding(frame.syntax->first());
            for (std::size_t index = 0; compatible && index < frame.operands.size(); ++index)
            {
                const bool matchedConversions = getterBinding != nullptr &&
                    index < getterBinding->selectedConversions.size() &&
                    index < frame.propertySetterBinding->selectedConversions.size() &&
                    getterBinding->selectedConversions[index].kind ==
                        frame.propertySetterBinding->selectedConversions[index].kind &&
                    getterBinding->selectedConversions[index].userDefinedMemberOrdinal ==
                        frame.propertySetterBinding->selectedConversions[index].userDefinedMemberOrdinal;
                bool safeClassConversion = true;
                if (isReferenceType(frame.operands[index]->typeKey))
                {
                    safeClassConversion = matchedConversions;
                    if (safeClassConversion)
                    {
                        const auto& conversion = frame.propertySetterBinding->selectedConversions[index];
                        const bool referenceIndex = isReferenceType(parameters[index].typeKey);
                        if (referenceIndex)
                        {
                            const MethodExpressionTypeBinding* source = expressionType(frame.operandSyntaxes[index]);
                            const bool plainSource = source != nullptr && !source->ownershipTransfer &&
                                !source->ownershipFreshOwner &&
                                (source->ownership != sema::OwnershipKind::Owned ||
                                 frame.operandSyntaxes[index]->kind() == ast::AstNodeKind::IdentifierExpression);
                            const bool plainConversion = conversion.kind == ProjectConversionKind::Identity ||
                                conversion.kind == ProjectConversionKind::ReferenceUpcast ||
                                conversion.kind == ProjectConversionKind::ReferenceInterface;
                            const bool plainSetter = frame.callTarget.memberOrdinal < memberModel_->members().size() &&
                                index < memberModel_->members()[frame.callTarget.memberOrdinal].parameters.size() &&
                                memberModel_->members()[frame.callTarget.memberOrdinal].parameters[index].ownership !=
                                    sema::OwnershipKind::Owned;
                            safeClassConversion = plainSource && plainConversion && plainSetter &&
                                parameters[index].ownership != sema::OwnershipKind::Owned;
                        }
                        else
                        {
                            safeClassConversion = conversion.kind == ProjectConversionKind::UserDefinedImplicit &&
                                conversion.userDefinedMemberOrdinal.has_value() &&
                                *conversion.userDefinedMemberOrdinal < memberModel_->members().size() &&
                                (frame.operands[index]->kind == ProjectOopHirNodeKind::LocalValue ||
                                 frame.operands[index]->kind == ProjectOopHirNodeKind::ParameterValue);
                            if (safeClassConversion)
                            {
                                const auto& converter = memberModel_->members()[*conversion.userDefinedMemberOrdinal];
                                safeClassConversion = converter.parameters.size() == 1 &&
                                    converter.parameters[0].passKind == ParameterPassKind::Value &&
                                    converter.parameters[0].ownership != sema::OwnershipKind::Owned;
                            }
                        }
                    }
                }
                compatible = parameters[index].passKind == ParameterPassKind::Value && matchedConversions &&
                    safeClassConversion &&
                    (ProjectTypeConversion::isNumeric(parameters[index].typeKey) ||
                     parameters[index].typeKey == "builtin:bool" || isReferenceType(parameters[index].typeKey)) &&
                    frame.propertySetterBinding->selectedConversions[index].targetTypeKey == parameters[index].typeKey &&
                    getter->children[index + 1] != nullptr &&
                    getter->children[index + 1]->typeKey == parameters[index].typeKey;
            }
            if (compatible)
            {
                capturedSetterBinding = *frame.propertySetterBinding;
                capturedTarget = makeNode(ProjectOopHirNodeKind::BlockExpression, frame.syntax,
                                          frame.setterCombinedValue->typeKey);
                if (capturedOwnedReceiver) frame.first = ownTemporary(std::move(frame.first));
                const auto capture = [&](std::unique_ptr<ProjectOopHirNode>& setterPart,
                                         std::unique_ptr<ProjectOopHirNode>& getterPart)
                {
                    const std::size_t ordinal = nextSyntheticLocalOrdinal_++;
                    const std::string type = setterPart->typeKey;
                    auto declaration = makeNode(ProjectOopHirNodeKind::LocalDeclaration, frame.syntax, type);
                    declaration->localOrdinal = ordinal;
                    declaration->children.push_back(std::move(setterPart));
                    capturedTarget->children.push_back(std::move(declaration));
                    setterPart = makeNode(ProjectOopHirNodeKind::LocalValue, frame.syntax, type);
                    setterPart->localOrdinal = ordinal;
                    getterPart = makeNode(ProjectOopHirNodeKind::LocalValue, frame.syntax, type);
                    getterPart->localOrdinal = ordinal;
                };
                if (structReceiver)
                {
                    const std::size_t ordinal = nextSyntheticLocalOrdinal_++;
                    auto declaration = makeNode(ProjectOopHirNodeKind::LocalDeclaration, frame.syntax,
                                                std::string(frame.callTarget.receiverTypeKey));
                    declaration->localOrdinal = ordinal;
                    declaration->referenceLocal = true;
                    declaration->referenceMutable = true;
                    declaration->children.push_back(std::move(frame.first));
                    capturedTarget->children.push_back(std::move(declaration));
                    const std::string pointerType = std::string(frame.callTarget.receiverTypeKey) + "*";
                    frame.first = makeNode(ProjectOopHirNodeKind::LocalValue, frame.syntax, pointerType);
                    frame.first->localOrdinal = ordinal;
                    getter->children[0] = makeNode(ProjectOopHirNodeKind::LocalValue, frame.syntax, pointerType);
                    getter->children[0]->localOrdinal = ordinal;
                }
                else capture(frame.first, getter->children[0]);
                for (std::size_t index = 0; index < frame.operands.size(); ++index)
                {
                    const std::size_t ordinal = nextSyntheticLocalOrdinal_++;
                    const std::string type = parameters[index].typeKey;
                    auto declaration = makeNode(ProjectOopHirNodeKind::LocalDeclaration, frame.syntax, type);
                    declaration->localOrdinal = ordinal;
                    declaration->children.push_back(std::move(getter->children[index + 1]));
                    capturedTarget->children.push_back(std::move(declaration));
                    frame.operands[index] = makeNode(ProjectOopHirNodeKind::LocalValue, frame.syntax, type);
                    frame.operands[index]->localOrdinal = ordinal;
                    getter->children[index + 1] = makeNode(ProjectOopHirNodeKind::LocalValue, frame.syntax, type);
                    getter->children[index + 1]->localOrdinal = ordinal;
                    capturedSetterBinding->selectedConversions[index].kind = ProjectConversionKind::Identity;
                    capturedSetterBinding->selectedConversions[index].sourceTypeKey = type;
                }
            }
        }
    }
    if (frame.compoundSetter)
        frame.operands.push_back(std::move(frame.setterCombinedValue));
    auto setterNode = lowerBoundCall(
        frame.syntax,
        frame.memberOrdinal,
        frame.addressContext,
        error,
        expressionType(frame.syntax),
        capturedSetterBinding.has_value() ? &*capturedSetterBinding : frame.propertySetterBinding,
        frame.operandSyntaxes,
        nullptr,
        false,
        std::move(frame.first),
        &frame.operands);
    if (setterNode == nullptr || !validateNoHeapDirectCall(
            frame.memberOrdinal, *frame.propertySetterBinding, error))
    {
        if (!lastErrorRange_.isValid()) lastErrorRange_ = frame.syntax->range();
        lowered.reset();
        frames.clear();
        return false;
    }
    setterNode->returnsAssignedValue = true;
    if (capturedTarget != nullptr)
    {
        capturedTarget->children.push_back(std::move(setterNode));
        lowered = wrapFullExpressionCleanup(std::move(capturedTarget), frame.syntax, capturedOwnedReceiver);
    }
    else lowered = std::move(setterNode);
    frames.pop_back();
    return true;
}

LoopBlockAdvance ProjectOopHirModel::completeCompoundSetterValue(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error)
{
    const MethodExpressionTypeBinding* rhsType =
        expressionType(frame.syntax->second());
    const ProjectImplicitConversion conversion = classifyImplicit(
        lowered->typeKey,
        frame.second->typeKey,
        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
    const bool shift = frame.compoundOperator == lexer::TokenKind::LessLess ||
        frame.compoundOperator == lexer::TokenKind::GreaterGreater;
    if (!shift && !conversion.exists())
    {
        error = "compound assignment to a property/indexer cannot convert its "
            "right-hand side from '" + lowered->typeKey + "' to '" +
            frame.second->typeKey + "'";
        lowered.reset();
        frames.clear();
        return LoopBlockAdvance::Error;
    }
    if (!shift && conversion.kind != ProjectConversionKind::Identity)
    {
        lowered = wrapImplicitConversion(
            std::move(lowered), conversion, frame.syntax->second());
    }
    frame.setterCombinedValue = makeNode(
        ProjectOopHirNodeKind::Binary,
        frame.syntax,
        frame.second->typeKey);
    frame.setterCombinedValue->operation = frame.compoundOperator;
    frame.setterCombinedValue->children.push_back(std::move(frame.second));
    frame.setterCombinedValue->children.push_back(std::move(lowered));
    const std::optional<bool> receiverReady =
        prepareSetterReceiver(frame, error);
    if (!receiverReady.has_value())
    {
        lowered.reset();
        frames.clear();
        return LoopBlockAdvance::Error;
    }
    frame.stage = *receiverReady ? 3U : 2U;
    if (*receiverReady) return LoopBlockAdvance::Complete;
    current = frame.callTarget.receiverSyntax;
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext =
        isStructValueType(frame.callTarget.receiverTypeKey) &&
        !frame.materializeReceiverTemporary;
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNamespaceExtensionSetterFrame(
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
        frames.back().kind == IterativeExpressionKind::NamespaceExtensionSetter)
    {
        IterativeExpressionFrame& frame = frames.back();
        const NamespaceFunctionCallBinding& binding =
            *frame.extensionSetterBinding;
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            if (!binding.selectedConversions.empty() &&
                binding.selectedConversions.front().kind !=
                    ProjectConversionKind::Identity)
            {
                frame.first = wrapImplicitConversion(
                    std::move(frame.first), binding.selectedConversions.front(),
                    binding.receiverSyntax);
            }
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        const NamespaceFunctionSymbol& target = namespaceFunctionModel_->functions()[
            frame.namespaceFunctionOrdinal];
        const std::size_t valueParameterOrdinal =
            binding.forwardedReceiver ? 1U : 0U;
        if (valueParameterOrdinal >= target.parameters.size())
        {
            error = "extension setter has no value parameter";
            lowered.reset();
            frames.clear();
            break;
        }
        if (valueParameterOrdinal < binding.selectedConversions.size() &&
            binding.selectedConversions[valueParameterOrdinal].kind !=
                ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered),
                binding.selectedConversions[valueParameterOrdinal],
                frame.syntax->second());
        }
        auto setter = makeNode(
            ProjectOopHirNodeKind::StaticCall,
            frame.syntax,
            binding.resultTypeKey);
        setter->namespaceFunctionOrdinal = frame.namespaceFunctionOrdinal;
        setter->stringValue = target.mangledName;
        if (binding.forwardedReceiver)
        {
            setter->children.push_back(std::move(frame.first));
            setter->argumentParameterOrdinals.push_back(0U);
            setter->argumentTypeKeys.push_back(target.parameters[0].typeKey);
        }
        setter->children.push_back(std::move(lowered));
        setter->argumentParameterOrdinals.push_back(valueParameterOrdinal);
        setter->argumentTypeKeys.push_back(
            target.parameters[valueParameterOrdinal].typeKey);
        setter->returnsAssignedValue = true;
        lowered = std::move(setter);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNullConditionalFieldAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::NullConditionalFieldAssignment)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        const MethodExpressionTypeBinding* assignmentType = expressionType(frame.syntax);
        const MethodDelegateBinding* assignmentDelegate = delegateBinding(frame.syntax);
        const std::string resultType = assignmentType != nullptr
            ? assignmentType->typeKey
            : assignmentDelegate != nullptr ? assignmentDelegate->resultTypeKey : "<error>";
        auto store = makeNode(
            ProjectOopHirNodeKind::NullConditionalFieldStore,
            frame.syntax,
            resultType);
        store->operation = frame.syntax->tokenKind();
        store->booleanValue = assignmentType != nullptr &&
            assignmentType->ownsStorageDestination;
        store->memberOrdinal = frame.fieldBinding->candidateMemberOrdinals.front();
        store->volatileAccess = memberModel_ != nullptr &&
            *store->memberOrdinal < memberModel_->members().size() &&
            memberModel_->members()[*store->memberOrdinal].isVolatile;
        store->children.push_back(std::move(frame.first));
        store->children.push_back(std::move(lowered));
        lowered = std::move(store);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNullConditionalArrayAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::NullConditionalArrayAssignment)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = !frame.syntax->first()->children().empty()
                ? frame.syntax->first()->children().front()
                : frame.syntax->first()->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        if (frame.stage == 1U)
        {
            frame.second = std::move(lowered);
            frame.stage = 2U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        const MethodExpressionTypeBinding* assignmentType = expressionType(frame.syntax);
        const MethodDelegateBinding* assignmentDelegate = delegateBinding(frame.syntax);
        const std::string resultType = assignmentType != nullptr
            ? assignmentType->typeKey
            : assignmentDelegate != nullptr ? assignmentDelegate->resultTypeKey : "<error>";
        auto store = makeNode(
            ProjectOopHirNodeKind::NullConditionalArrayStore,
            frame.syntax,
            resultType);
        store->elementTypeKey = frame.core->elementTypeKey;
        store->booleanValue = assignmentType != nullptr &&
            assignmentType->ownsStorageDestination;
        store->children.push_back(std::move(frame.first));
        store->children.push_back(std::move(frame.second));
        store->children.push_back(std::move(lowered));
        lowered = std::move(store);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledDirectAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::DirectAssignment)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        const MethodExpressionTypeBinding* rhsType = expressionType(frame.syntax->second());
        const ProjectImplicitConversion conversion = classifyImplicit(
            lowered->typeKey, frame.first->typeKey,
            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
        if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
            lowered = wrapImplicitConversion(std::move(lowered), conversion, frame.syntax->second());
        auto assignment = makeNode(
            ProjectOopHirNodeKind::Assignment, frame.syntax, frame.assignmentResultType);
        assignment->operation = frame.assignmentOperation;
        assignment->booleanValue = frame.assignmentOwnsStorage;
        assignment->localOrdinal = frame.assignmentLocalOrdinal;
        assignment->children.push_back(std::move(frame.first));
        assignment->children.push_back(std::move(lowered));
        lowered = std::move(assignment);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledStringCompoundAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::StringCompoundAssignment)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->first();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        if (frame.stage == 1U)
        {
            frame.second = std::move(lowered);
            frame.stage = 2U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        auto concatenated = makeNode(
            ProjectOopHirNodeKind::StringConcat, frame.syntax, "builtin:string");
        concatenated->operation = lexer::TokenKind::Plus;
        concatenated->children.push_back(std::move(frame.second));
        concatenated->children.push_back(std::move(lowered));
        auto assignment = makeNode(
            ProjectOopHirNodeKind::Assignment, frame.syntax, frame.assignmentResultType);
        assignment->operation = lexer::TokenKind::Equal;
        assignment->booleanValue = true;
        assignment->localOrdinal = frame.assignmentLocalOrdinal;
        assignment->children.push_back(std::move(frame.first));
        assignment->children.push_back(std::move(concatenated));
        lowered = std::move(assignment);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCompoundAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::CompoundAssignment)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            frame.stage = 1U;
            current = frame.syntax->second();
            currentMemberOrdinal = frame.memberOrdinal;
            const MethodCallBinding* binding = callBinding(frame.syntax);
            const ClassMemberSymbol* target = binding != nullptr && binding->selectedMemberOrdinal.has_value()
                ? callableSymbol(*binding->selectedMemberOrdinal) : nullptr;
            currentAddressContext = target != nullptr && target->parameters.size() == 2 &&
                (target->parameters[1].passKind == ParameterPassKind::Ref ||
                 (target->parameters[1].passKind == ParameterPassKind::In &&
                  isNaturallyAddressable(frame.syntax->second())));
            descend = true;
            break;
        }
        auto compound = makeNode(
            ProjectOopHirNodeKind::CompoundStore, frame.syntax, frame.operandType->typeKey);
        compound->operation = lexer::compoundAssignmentBaseOperator(frame.assignmentOperation);
        if (compound->operation == lexer::TokenKind::Invalid)
        {
            error = "unsupported compound assignment operator";
            lowered.reset();
            frames.clear();
            break;
        }
        compound->booleanValue = false;
        bool hasOwnedTemporary = false;
        const MethodCallBinding* overload = callBinding(frame.syntax);
        if (overload != nullptr && overload->resolution == MethodCallResolutionKind::Resolved &&
            overload->selectedMemberOrdinal.has_value())
        {
            const ClassMemberSymbol* target = callableSymbol(*overload->selectedMemberOrdinal);
            if (overload->selectedConversions.size() != 2 ||
                overload->selectedConversions[0].kind != ProjectConversionKind::Identity)
            {
                error = "compound operator requires an identity conversion for its destination";
                lowered.reset();
                frames.clear();
                break;
            }
            compound->memberOrdinal = *overload->selectedMemberOrdinal;
            if (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(frame.operandType->typeKey))
            {
                const MethodNameBinding* targetName = nameBinding(frame.syntax->first());
                const MethodExpressionTypeBinding* receiver = frame.syntax->first() != nullptr &&
                    frame.syntax->first()->kind() == ast::AstNodeKind::IndexExpression
                    ? expressionType(frame.syntax->first()->first()) : nullptr;
                const bool nativeArrayTarget = receiver != nullptr && ProjectTypeConversion::isNativeArray(receiver->typeKey);
                if (!nativeArrayTarget && (targetName == nullptr ||
                    (!targetName->localOrdinal.has_value() && !targetName->parameterOrdinal.has_value())))
                {
                    error = "destructible compound destination must be a local, owned parameter or native array element";
                    lowered.reset();
                    frames.clear();
                    break;
                }
                compound->booleanValue = true;
                if (targetName != nullptr)
                {
                    compound->localOrdinal = targetName->localOrdinal;
                    compound->parameterOrdinal = targetName->parameterOrdinal;
                }
            }
            if (target != nullptr && target->declaredTypeKey != frame.operandType->typeKey)
            {
                const ProjectImplicitConversion resultConversion = classifyImplicit(
                    target->declaredTypeKey, frame.operandType->typeKey, std::nullopt);
                if (!resultConversion.userDefinedMemberOrdinal.has_value())
                {
                    error = "compound operator result has no selected implicit conversion";
                    lowered.reset();
                    frames.clear();
                    break;
                }
                compound->secondaryMemberOrdinal = resultConversion.userDefinedMemberOrdinal;
            }
            lowered = prepareBoundCallArgument(*overload, *overload->selectedMemberOrdinal,
                1, frame.syntax->second(), std::move(lowered), hasOwnedTemporary);
        }
        else if (ProjectTypeConversion::isPointer(frame.operandType->typeKey))
        {
            compound->elementTypeKey = frame.operandType->typeKey.substr(
                0U, frame.operandType->typeKey.size() - 1U);
        }
        else
        {
            const MethodExpressionTypeBinding* rhsType = expressionType(frame.syntax->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                lowered->typeKey, frame.operandType->typeKey,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            const bool shift = compound->operation == lexer::TokenKind::LessLess ||
                compound->operation == lexer::TokenKind::GreaterGreater;
            if (!shift && !conversion.exists())
            {
                error = "compound assignment cannot convert its right-hand side from '" +
                    lowered->typeKey + "' to '" + frame.operandType->typeKey + "'";
                lowered.reset();
                frames.clear();
                break;
            }
            if (!shift && conversion.kind != ProjectConversionKind::Identity)
                lowered = wrapImplicitConversion(std::move(lowered), conversion, frame.syntax->second());
        }
        compound->children.push_back(std::move(frame.first));
        compound->children.push_back(std::move(lowered));
        lowered = wrapFullExpressionCleanup(std::move(compound), frame.syntax, hasOwnedTemporary);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

}
