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
ProjectOopHirModel::completeScheduledConstructionFrames(
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
    if (kind == IterativeExpressionKind::OrdinaryCall)
        return completeScheduledOrdinaryCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::AggregateCrossLowering)
        return completeScheduledAggregateCrossLoweringFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::OrdinaryConstruction)
        return completeScheduledOrdinaryConstructionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::AllocatorConstruction)
        return completeScheduledAllocatorConstructionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ExtensionPropertyGetter)
        return completeScheduledExtensionPropertyGetterFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::InterpolationHandler)
        return completeScheduledInterpolationHandlerFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::CollectionBuilder)
        return completeScheduledCollectionBuilderFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::ArrayConstruction)
        return completeScheduledArrayConstructionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledOrdinaryCallFrame(
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
        frames.back().kind == IterativeExpressionKind::OrdinaryCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        OrdinaryCallFrameState& call = *frame.ordinaryCall;
        if (!acceptOrdinaryCallChild(call, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextOrdinaryCallChild(
                call, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        lowered = finishOrdinaryCall(frame, error);
        if (lowered == nullptr)
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            frames.clear();
            break;
        }
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledAggregateCrossLoweringFrame(
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
        frames.back().kind == IterativeExpressionKind::AggregateCrossLowering)
    {
        IterativeExpressionFrame& frame = frames.back();
        AggregateCrossFrameState& aggregate = *frame.aggregateCross;
        if (!acceptAggregateCrossChild(
                aggregate, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextAggregateCrossChild(
                aggregate, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        if (aggregate.phase != AggregateCrossFramePhase::Ready)
        {
            error = "Stage 34 aggregate frame did not reach completion";
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        lowered = finishAggregateCrossFrame(frame);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledOrdinaryConstructionFrame(
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
        frames.back().kind == IterativeExpressionKind::OrdinaryConstruction)
    {
        IterativeExpressionFrame& frame = frames.back();
        OrdinaryConstructionFrameState& construction =
            *frame.ordinaryConstruction;
        if (!acceptOrdinaryConstructionChild(
                construction, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        const ast::AstNode* childSyntax = nullptr;
        if (nextOrdinaryConstructionChild(construction, childSyntax))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        lowered = finishOrdinaryConstructionFrame(frame);
        if (lowered == nullptr)
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            frames.clear();
            break;
        }
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledAllocatorConstructionFrame(
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
        frames.back().kind == IterativeExpressionKind::AllocatorConstruction)
    {
        IterativeExpressionFrame& frame = frames.back();
        AllocatorConstructionFrameState& construction =
            *frame.allocatorConstruction;
        if (!acceptAllocatorConstructionChild(
                construction, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextAllocatorConstructionChild(
                construction, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        lowered = finishAllocatorConstructionFrame(frame, error);
        if (lowered == nullptr)
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            frames.clear();
            break;
        }
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledExtensionPropertyGetterFrame(
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
        frames.back().kind == IterativeExpressionKind::ExtensionPropertyGetter)
    {
        IterativeExpressionFrame& frame = frames.back();
        auto getter = lowerExtensionPropertyGetterWithReceiver(
            frame.syntax,
            frame.memberOrdinal,
            error,
            *frame.extensionPropertyBinding,
            std::move(lowered));
        if (getter == nullptr)
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        lowered = std::move(getter);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledInterpolationHandlerFrame(
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
        frames.back().kind == IterativeExpressionKind::InterpolationHandler)
    {
        IterativeExpressionFrame& frame = frames.back();
        frame.operands.push_back(std::move(lowered));
        if (frame.operands.size() < frame.operandSyntaxes.size())
        {
            current = frame.operandSyntaxes[frame.operands.size()];
            currentAddressContext = false;
            currentMemberOrdinal = frame.core->builderMemberOrdinals[1];
            descend = true;
            break;
        }
        auto interpolation = buildInterpolationHandlerExpression(
            frame.syntax,
            *frame.core,
            frame.syntheticLocalOrdinal,
            frame.interpolationParts,
            std::move(frame.operands),
            error);
        if (interpolation == nullptr)
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        lowered = std::move(interpolation);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCollectionBuilderFrame(
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
        frames.back().kind == IterativeExpressionKind::CollectionBuilder)
    {
        IterativeExpressionFrame& frame = frames.back();
        frame.operands.push_back(std::move(lowered));
        if (frame.operands.size() < frame.operandSyntaxes.size())
        {
            current = frame.operandSyntaxes[frame.operands.size()];
            currentAddressContext = false;
            currentMemberOrdinal = frame.memberOrdinal;
            descend = true;
            break;
        }
        lowered = finishCollectionBuilderExpression(
            frame.syntax,
            *frame.core,
            frame.syntheticLocalOrdinal,
            std::move(frame.operands));
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledArrayConstructionFrame(
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
        frames.back().kind == IterativeExpressionKind::ArrayConstruction)
    {
        IterativeExpressionFrame& frame = frames.back();
        const std::size_t index = frame.armIndex++;
        if (frame.core->kind == MethodCoreOperationKind::RangeArrayAllocation)
        {
            frame.first->children.push_back(std::move(lowered));
        }
        else if (frame.arraySegmented && frame.arraySpreadOperands[index])
        {
            frame.first->children.push_back(std::move(lowered));
            frame.first->collectionSegments.push_back(ProjectCollectionSegmentKind::Spread);
        }
        else
        {
            const bool isCount = !frame.arraySegmented && !frame.arrayInferred && index == 0U;
            if (!isCount)
            {
                const std::size_t initializerIndex = frame.arraySegmented
                    ? index : index - (frame.arrayInferred ? 0U : 1U);
                const ast::AstNode* initializer =
                    frame.syntax->children()[initializerIndex];
                const MethodExpressionTypeBinding* valueType = expressionType(initializer);
                if (valueType == nullptr)
                {
                    error = frame.arraySegmented
                        ? "NSC-LIMIT-017 array spread element has no semantic type"
                        : "NSC-LIMIT-017 array initializer element has no semantic type";
                    if (!lastErrorRange_.isValid())
                        lastErrorRange_ = initializer->range();
                    lowered.reset(); frames.clear(); break;
                }
                const ProjectImplicitConversion conversion = classifyImplicit(
                    valueType->typeKey, frame.core->elementTypeKey, valueType->integerConstant);
                if (!conversion.exists())
                {
                    error = frame.arraySegmented
                        ? "NSC-LIMIT-017 array spread element lost its validated conversion"
                        : "NSC-LIMIT-017 array initializer lost its validated conversion";
                    if (!lastErrorRange_.isValid())
                        lastErrorRange_ = initializer->range();
                    lowered.reset(); frames.clear(); break;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                    lowered = wrapImplicitConversion(std::move(lowered), conversion, initializer);
            }
            frame.first->children.push_back(std::move(lowered));
            if (frame.arraySegmented)
                frame.first->collectionSegments.push_back(ProjectCollectionSegmentKind::Element);
        }
        if (frame.armIndex < frame.operandSyntaxes.size())
        {
            current = frame.operandSyntaxes[frame.armIndex];
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true; break;
        }
        lowered = std::move(frame.first);
        frames.pop_back(); continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

}
