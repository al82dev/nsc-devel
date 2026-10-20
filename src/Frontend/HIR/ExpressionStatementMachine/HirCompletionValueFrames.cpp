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
ProjectOopHirModel::completeScheduledValueFrames(
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
    if (kind == IterativeExpressionKind::InstanceField)
        return completeScheduledInstanceFieldFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NullConditionalField)
        return completeScheduledNullConditionalFieldFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::RangeSlice)
        return completeScheduledRangeSliceFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::BuiltinBinary)
        return completeScheduledBuiltinBinaryFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::CoreBinary)
        return completeScheduledCoreBinaryFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NullCoalescing)
        return completeScheduledNullCoalescingFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NullCoalescingAssignment)
        return completeScheduledNullCoalescingAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::DeconstructionAssignment)
        return completeScheduledDeconstructionAssignmentFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledInstanceFieldFrame(
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
        frames.back().kind == IterativeExpressionKind::InstanceField)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodNameBinding& binding = *frame.fieldBinding;
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
        auto field = makeNode(
            frame.addressContext
                ? ProjectOopHirNodeKind::FieldAddress
                : ProjectOopHirNodeKind::FieldValue,
            frame.syntax,
            binding.typeKey);
        field->memberOrdinal = binding.candidateMemberOrdinals.front();
        field->volatileAccess = memberModel_ != nullptr &&
            *field->memberOrdinal < memberModel_->members().size() &&
            memberModel_->members()[*field->memberOrdinal].isVolatile;
        field->baseReceiver = binding.baseReceiver;
        field->children.push_back(std::move(lowered));
        lowered = std::move(field);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNullConditionalFieldFrame(
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
        frames.back().kind == IterativeExpressionKind::NullConditionalField)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodExpressionTypeBinding* frameType =
            expressionType(frame.syntax);
        const MethodDelegateBinding* frameDelegate =
            delegateBinding(frame.syntax);
        const std::string resultType = frameType != nullptr
            ? frameType->typeKey
            : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
        const MethodExpressionTypeBinding* receiverFlow =
            expressionType(frame.syntax->first());
        const bool provenNonNull =
            receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
        auto field = makeNode(
            provenNonNull
                ? ProjectOopHirNodeKind::FieldValue
                : ProjectOopHirNodeKind::NullConditionalFieldValue,
            frame.syntax,
            resultType);
        field->memberOrdinal = frame.fieldBinding->candidateMemberOrdinals.front();
        field->booleanValue = provenNonNull;
        field->volatileAccess = memberModel_ != nullptr &&
            *field->memberOrdinal < memberModel_->members().size() &&
            memberModel_->members()[*field->memberOrdinal].isVolatile;
        field->children.push_back(std::move(lowered));
        lowered = std::move(field);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledRangeSliceFrame(
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
        frames.back().kind == IterativeExpressionKind::RangeSlice)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodRangeSliceBinding& range = *frame.rangeBinding;
        if (frame.stage == 0U)
        {
            if (frame.materializeReceiverTemporary)
            {
                auto temporary = makeNode(
                    ProjectOopHirNodeKind::TemporaryAddress,
                    range.receiverSyntax,
                    range.receiverTypeKey);
                temporary->elementTypeKey = range.receiverTypeKey;
                temporary->children.push_back(std::move(lowered));
                lowered = std::move(temporary);
            }
            frame.first = std::move(lowered);
            frame.stage = 1U;
            if (range.startSyntax != nullptr)
            {
                current = range.startSyntax;
                currentMemberOrdinal = frame.memberOrdinal;
                currentAddressContext = false;
                descend = true;
                break;
            }
            lowered = makeNode(
                ProjectOopHirNodeKind::IntegerConstant,
                frame.syntax,
                "builtin:int");
            lowered->integerValue = 0;
            continue;
        }
        if (frame.stage == 1U)
        {
            if (range.startSyntax != nullptr && range.startConversion.exists() &&
                range.startConversion.kind != ProjectConversionKind::Identity)
            {
                lowered = wrapImplicitConversion(
                    std::move(lowered), range.startConversion, range.startSyntax);
            }
            frame.second = std::move(lowered);
            frame.stage = 2U;
            if (range.endSyntax != nullptr)
            {
                current = range.endSyntax;
                currentMemberOrdinal = frame.memberOrdinal;
                currentAddressContext = false;
                descend = true;
                break;
            }
            lowered = makeNode(
                ProjectOopHirNodeKind::IntegerConstant,
                frame.syntax,
                "builtin:int");
            lowered->integerValue = 0;
            continue;
        }
        if (range.endSyntax != nullptr && range.endConversion.exists() &&
            range.endConversion.kind != ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered), range.endConversion, range.endSyntax);
        }
        const ClassMemberSymbol& sliceMember =
            memberModel_->members()[range.sliceMemberOrdinal];
        auto slice = makeNode(
            ProjectOopHirNodeKind::RangeSlice,
            frame.syntax,
            range.resultTypeKey);
        slice->elementTypeKey = range.receiverTypeKey;
        slice->memberOrdinal = range.sliceMemberOrdinal;
        slice->secondaryMemberOrdinal = range.lengthMemberOrdinal;
        slice->referenceResult = sliceMember.isReferenceReturn;
        slice->integerValue =
            (range.hasStart ? 1U : 0U) |
            (range.startFromEnd ? 2U : 0U) |
            (range.hasEnd ? 4U : 0U) |
            (range.endFromEnd ? 8U : 0U) |
            (range.boundsStaticallyProven ? 16U : 0U);
        slice->argumentCount = 2U;
        slice->argumentParameterOrdinals = {0U, 1U};
        slice->children.push_back(std::move(frame.first));
        slice->children.push_back(std::move(frame.second));
        slice->children.push_back(std::move(lowered));
        lowered = std::move(slice);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledBuiltinBinaryFrame(
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
        frames.back().kind == IterativeExpressionKind::BuiltinBinary)
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
        lowered = buildBuiltinBinaryExpression(
            frame.syntax, std::move(frame.first), std::move(lowered));
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCoreBinaryFrame(
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
        frames.back().kind == IterativeExpressionKind::CoreBinary)
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
        if (frame.core->kind == MethodCoreOperationKind::StructEquals)
        {
            auto equality = makeNode(
                ProjectOopHirNodeKind::AggregateEquals,
                frame.syntax,
                frame.core->resultTypeKey);
            equality->elementTypeKey = frame.core->elementTypeKey;
            equality->children.push_back(std::move(frame.first));
            equality->children.push_back(std::move(lowered));
            equality->argumentCount = equality->children.size();
            if (!appendAggregateLeaves(
                    frame.core->elementTypeKey,
                    0,
                    frame.syntax,
                    equality->children,
                    error))
            {
                if (!lastErrorRange_.isValid())
                    lastErrorRange_ = frame.syntax->range();
                lowered.reset();
                frames.clear();
                break;
            }
            if (frame.syntax->tokenKind() == lexer::TokenKind::BangEqual)
            {
                auto negated = makeNode(
                    ProjectOopHirNodeKind::Unary,
                    frame.syntax,
                    frame.core->resultTypeKey);
                negated->operation = lexer::TokenKind::Bang;
                negated->children.push_back(std::move(equality));
                lowered = std::move(negated);
            }
            else
            {
                lowered = std::move(equality);
            }
        }
        else
        {
            const ProjectOopHirNodeKind kind =
                frame.core->kind == MethodCoreOperationKind::StringConcat
                    ? ProjectOopHirNodeKind::StringConcat
                    : frame.core->kind == MethodCoreOperationKind::StringEquals
                        ? ProjectOopHirNodeKind::StringEquals
                        : ProjectOopHirNodeKind::PointerArithmetic;
            auto binary = makeNode(
                kind, frame.syntax, frame.core->resultTypeKey);
            binary->operation = frame.syntax->tokenKind();
            if (frame.core->kind == MethodCoreOperationKind::PointerArithmetic)
                binary->elementTypeKey = frame.core->elementTypeKey;
            binary->children.push_back(std::move(frame.first));
            binary->children.push_back(std::move(lowered));
            lowered = std::move(binary);
        }
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNullCoalescingFrame(
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
        frames.back().kind == IterativeExpressionKind::NullCoalescing)
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
        const MethodExpressionTypeBinding* frameType =
            expressionType(frame.syntax);
        const MethodDelegateBinding* frameDelegate =
            delegateBinding(frame.syntax);
        const std::string resultType = frameType != nullptr
            ? frameType->typeKey
            : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
        const MethodExpressionTypeBinding* rhsType =
            expressionType(frame.syntax->second());
        const ProjectImplicitConversion conversion = classifyImplicit(
            lowered->typeKey,
            resultType,
            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
        if (!conversion.exists())
        {
            error = "T01-F03 null-coalescing RHS cannot convert from '" +
                lowered->typeKey + "' to '" + resultType + "'";
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        if (conversion.kind != ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered), conversion, frame.syntax->second());
        }
        const MethodOptionalCarrierBinding* optional =
            optionalCarrierBinding(frame.syntax);
        auto coalescing = makeNode(
            optional != nullptr
                ? ProjectOopHirNodeKind::OptionalCoalescing
                : ProjectOopHirNodeKind::NullCoalescing,
            frame.syntax,
            resultType);
        if (optional != nullptr)
        {
            coalescing->elementTypeKey = optional->carrierTypeKey;
            coalescing->memberOrdinal = optional->hasValueMemberOrdinal;
            coalescing->secondaryMemberOrdinal =
                optional->uncheckedValueMemberOrdinal;
        }
        coalescing->children.push_back(std::move(frame.first));
        coalescing->children.push_back(std::move(lowered));
        lowered = std::move(coalescing);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNullCoalescingAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::NullCoalescingAssignment)
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
        const MethodExpressionTypeBinding* targetType =
            expressionType(frame.syntax->first());
        const MethodExpressionTypeBinding* rhsType =
            expressionType(frame.syntax->second());
        const ProjectImplicitConversion conversion = classifyImplicit(
            lowered->typeKey,
            targetType->typeKey,
            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
        if (!conversion.exists())
        {
            error = "T01-F03 null-coalescing assignment RHS cannot convert from '" +
                lowered->typeKey + "' to '" + targetType->typeKey + "'";
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        if (conversion.kind != ProjectConversionKind::Identity)
        {
            lowered = wrapImplicitConversion(
                std::move(lowered), conversion, frame.syntax->second());
        }
        auto assignment = makeNode(
            ProjectOopHirNodeKind::NullCoalescingAssignment,
            frame.syntax,
            targetType->typeKey);
        assignment->children.push_back(std::move(frame.first));
        assignment->children.push_back(std::move(lowered));
        lowered = std::move(assignment);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledDeconstructionAssignmentFrame(
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
        frames.back().kind == IterativeExpressionKind::DeconstructionAssignment)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            const std::size_t frameCount = frames.size();
            if (!initializeDeconstructionAssignmentCompletion(
                    frame, frames, lowered, error))
                break;
            if (frames.size() != frameCount) continue;
        }
        else
        {
            const HirTuplePatternFrame& leaf =
                frame.deconstructionLeaves[frame.armIndex];
            const ProjectImplicitConversion conversion = classifyImplicit(
                leaf.typeKey, frame.operandType->typeKey, std::nullopt);
            if (!conversion.exists())
            {
                error =
                    "T02-F03 deconstruction lost its validated element conversion";
                if (!lastErrorRange_.isValid())
                    lastErrorRange_ = frame.syntax->range();
                lowered.reset();
                frames.clear();
                break;
            }
            auto element = makeNode(
                ProjectOopHirNodeKind::TupleDeconstructElement,
                leaf.syntax,
                frame.operandType->typeKey);
            element->elementTypeKey = leaf.typeKey;
            element->integerValue = leaf.byteOffset;
            element->conversion = conversion;
            element->children.push_back(std::move(lowered));
            frame.deconstructionNode->children.push_back(std::move(element));
            ++frame.armIndex;
            if (frame.armIndex == frame.deconstructionLeaves.size())
            {
                lowered = std::move(frame.deconstructionNode);
                frames.pop_back();
                continue;
            }
        }
        const ast::AstNode* targetSyntax =
            frame.deconstructionLeaves[frame.armIndex].syntax;
        frame.operandType = expressionType(targetSyntax);
        if (frame.operandType == nullptr)
        {
            error = "T02-F03 deconstruction target has no semantic type";
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        current = targetSyntax;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = true;
        descend = true;
        break;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

bool ProjectOopHirModel::initializeDeconstructionAssignmentCompletion(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error)
{
    if (frame.deconstructionBinding->selectedMemberOrdinal.has_value())
    {
        if (frame.materializeReceiverTemporary)
        {
            auto temporary = makeNode(
                ProjectOopHirNodeKind::TemporaryAddress,
                frame.syntax->second(),
                frame.deconstructionBinding->sourceTypeKey);
            temporary->elementTypeKey =
                frame.deconstructionBinding->sourceTypeKey;
            temporary->children.push_back(std::move(lowered));
            lowered = std::move(temporary);
        }
        auto call = makeNode(
            ProjectOopHirNodeKind::InstanceCall,
            frame.syntax->second(),
            frame.deconstructionBinding->tupleTypeKey);
        call->memberOrdinal =
            *frame.deconstructionBinding->selectedMemberOrdinal;
        call->children.push_back(std::move(lowered));
        lowered = std::move(call);
    }
    const auto sourceElements = ProjectTypeConversion::tupleElementTypeKeys(
        frame.deconstructionBinding->tupleTypeKey);
    if (!sourceElements.has_value() || objectLayoutModel_ == nullptr ||
        nameLookup_ == nullptr)
    {
        error = "T02-F03 deconstruction source has no tuple layout";
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frame.syntax->range();
        lowered.reset();
        frames.clear();
        return false;
    }
    frame.deconstructionNode = makeNode(
        ProjectOopHirNodeKind::TupleDeconstruct,
        frame.syntax,
        frame.deconstructionBinding->tupleTypeKey);
    frame.deconstructionNode->children.push_back(std::move(lowered));
    const auto expandTargetTuple = std::bind_front(
        &ProjectOopHirModel::expandDeconstructionAssignmentTargetTuple,
        this,
        std::ref(error));
    const auto collectTargetLeaf = std::bind_front(
        &ProjectOopHirModel::collectDeconstructionAssignmentTargetLeaf,
        std::ref(frame));
    if (!visitHirTuplePatternLeaves(
            frame.syntax->first(),
            frame.deconstructionBinding->tupleTypeKey,
            0,
            expandTargetTuple,
            collectTargetLeaf))
    {
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frame.syntax->range();
        lowered.reset();
        frames.clear();
        return false;
    }
    if (frame.deconstructionLeaves.empty())
    {
        lowered = std::move(frame.deconstructionNode);
        frames.pop_back();
        return true;
    }
    frame.stage = 1U;
    return true;
}

}
