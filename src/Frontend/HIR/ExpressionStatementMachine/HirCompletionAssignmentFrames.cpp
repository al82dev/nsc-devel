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
        frames.back().kind == IterativeExpressionKind::EventSubscription)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodEventBinding& binding = *frame.eventSubscriptionBinding;
        if (frame.stage == 0U)
        {
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
            subscription->memberOrdinal =
                *binding.customAccessorMemberOrdinal;
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
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledPropertyIndexerSetterFrame(
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
        if (frame.compoundSetter)
            frame.operands.push_back(std::move(frame.setterCombinedValue));
        auto setterNode = lowerBoundCall(
            frame.syntax,
            frame.memberOrdinal,
            frame.addressContext,
            error,
            expressionType(frame.syntax),
            frame.propertySetterBinding,
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
            break;
        }
        setterNode->returnsAssignedValue = true;
        lowered = std::move(setterNode);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeCompoundSetterValue(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error)
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
        frames.back().kind == IterativeExpressionKind::CompoundAssignment)
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
        if (ProjectTypeConversion::isPointer(frame.operandType->typeKey))
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
        lowered = std::move(compound);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

}
