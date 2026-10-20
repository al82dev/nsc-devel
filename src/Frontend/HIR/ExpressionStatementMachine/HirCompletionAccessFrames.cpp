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
ProjectOopHirModel::completeScheduledAccessFrames(
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
    if (kind == IterativeExpressionKind::CoreMember)
        return completeScheduledCoreMemberFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::CoreIndex)
        return completeScheduledCoreIndexFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::RawIndex)
        return completeScheduledRawIndexFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::NamespaceCall)
        return completeScheduledNamespaceCallFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::EventInvoke)
        return completeScheduledEventInvokeFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    if (kind == IterativeExpressionKind::DelegateInvoke)
        return completeScheduledDelegateInvokeFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCoreMemberFrame(
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
        frames.back().kind == IterativeExpressionKind::CoreMember)
    {
        IterativeExpressionFrame& frame = frames.back();
        const MethodCoreBinding& core = *coreBinding(frame.syntax);
        ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::IteratorCurrent;
        if (core.kind == MethodCoreOperationKind::ArrayLength)
            nodeKind = ProjectOopHirNodeKind::ArrayLength;
        else if (core.kind == MethodCoreOperationKind::StringLength)
            nodeKind = ProjectOopHirNodeKind::StringLength;
        auto member = makeNode(nodeKind, frame.syntax, core.resultTypeKey);
        member->elementTypeKey = core.elementTypeKey;
        member->children.push_back(std::move(lowered));
        lowered = std::move(member);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledCoreIndexFrame(
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
        frames.back().kind == IterativeExpressionKind::CoreIndex)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (frame.stage == 0U)
        {
            frame.first = std::move(lowered);
            frame.stage = 1U;
            const ast::AstNode* syntax = frame.operandSyntaxes.front();
            current = syntax->kind() == ast::AstNodeKind::FromEndExpression
                ? syntax->first() : syntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        frame.operands.push_back(std::move(lowered));
        if (frame.operands.size() < frame.operandSyntaxes.size())
        {
            const ast::AstNode* syntax =
                frame.operandSyntaxes[frame.operands.size()];
            current = syntax->kind() == ast::AstNodeKind::FromEndExpression
                ? syntax->first() : syntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }

        const MethodCoreBinding& core = *frame.core;
        if (core.kind == MethodCoreOperationKind::NullConditionalArrayElement)
        {
            auto element = makeNode(
                ProjectOopHirNodeKind::NullConditionalArrayLoad,
                frame.syntax,
                core.resultTypeKey);
            element->elementTypeKey = core.elementTypeKey;
            element->children.push_back(std::move(frame.first));
            element->children.push_back(std::move(frame.operands.front()));
            lowered = std::move(element);
            frames.pop_back();
            continue;
        }

        ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::StringElementValue;
        if (core.kind == MethodCoreOperationKind::ArrayElement)
        {
            nodeKind = frame.addressContext
                ? ProjectOopHirNodeKind::ArrayElementAddress
                : ProjectOopHirNodeKind::ArrayElementValue;
        }
        else if (core.kind == MethodCoreOperationKind::RangeArrayElement)
        {
            nodeKind = frame.addressContext
                ? ProjectOopHirNodeKind::RangeArrayElementAddress
                : ProjectOopHirNodeKind::RangeArrayElementValue;
        }
        auto element = makeNode(nodeKind, frame.syntax, core.resultTypeKey);
        element->elementTypeKey = core.elementTypeKey;
        element->nativeArrayRank = core.kind == MethodCoreOperationKind::RangeArrayElement
            ? frame.operandSyntaxes.size() : 1U;
        element->children.push_back(std::move(frame.first));
        for (std::size_t dimension = 0;
            dimension < frame.operandSyntaxes.size(); ++dimension)
        {
            const bool fromEnd = frame.operandSyntaxes[dimension]->kind() ==
                ast::AstNodeKind::FromEndExpression;
            if (dimension == 0U) element->booleanValue = fromEnd;
            if (fromEnd && dimension < 64U)
                element->integerValue |= (std::uint64_t{1} << dimension);
            element->children.push_back(std::move(frame.operands[dimension]));
        }
        lowered = std::move(element);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledRawIndexFrame(
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
        frames.back().kind == IterativeExpressionKind::RawIndex)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (scheduleNextRawIndexChild(
                frame, current, currentMemberOrdinal, currentAddressContext,
                lowered))
        {
            descend = true;
            break;
        }
        if (frame.variableArrayIndex != nullptr)
        {
            finishVariableRawIndex(frame, lowered);
            frames.pop_back();
            continue;
        }
        finishArrayRawIndex(frame, lowered);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

bool ProjectOopHirModel::scheduleNextRawIndexChild(
    IterativeExpressionFrame& frame,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    if (frame.stage == 0U)
    {
        frame.first = std::move(lowered);
        frame.stage = 1U;
        current = frame.operandSyntaxes.front();
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        return true;
    }
    frame.operands.push_back(std::move(lowered));
    if (frame.operands.size() >= frame.operandSyntaxes.size()) return false;
    current = frame.operandSyntaxes[frame.operands.size()];
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    return true;
}

void ProjectOopHirModel::finishVariableRawIndex(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    const MethodCoreBinding& core = *frame.core;
    std::unique_ptr<ProjectOopHirNode> linearIndex;
    for (std::size_t dimension = 0;
        dimension < frame.operands.size(); ++dimension)
    {
        auto index = std::move(frame.operands[dimension]);
        if (index->typeKey != "builtin:nuint")
        {
            auto converted = makeNode(
                ProjectOopHirNodeKind::Conversion,
                frame.operandSyntaxes[dimension],
                "builtin:nuint");
            converted->children.push_back(std::move(index));
            index = std::move(converted);
        }
        if (linearIndex == nullptr)
        {
            linearIndex = std::move(index);
            continue;
        }
        const std::size_t extentOrdinal =
            frame.variableArrayIndex->extentLocalOrdinals[dimension];
        auto extent = makeNode(
            ProjectOopHirNodeKind::LocalValue,
            frame.syntax,
            methodBodyModel_->locals()[extentOrdinal].typeKey);
        extent->localOrdinal = extentOrdinal;
        if (extent->typeKey != "builtin:nuint")
        {
            auto convertedExtent = makeNode(
                ProjectOopHirNodeKind::Conversion,
                frame.syntax,
                "builtin:nuint");
            convertedExtent->children.push_back(std::move(extent));
            extent = std::move(convertedExtent);
        }
        auto rowOffset = makeNode(
            ProjectOopHirNodeKind::Binary, frame.syntax, "builtin:nuint");
        rowOffset->operation = lexer::TokenKind::Star;
        rowOffset->children.push_back(std::move(linearIndex));
        rowOffset->children.push_back(std::move(extent));
        auto sum = makeNode(
            ProjectOopHirNodeKind::Binary, frame.syntax, "builtin:nuint");
        sum->operation = lexer::TokenKind::Plus;
        sum->children.push_back(std::move(rowOffset));
        sum->children.push_back(std::move(index));
        linearIndex = std::move(sum);
    }
    auto address = makeNode(
        ProjectOopHirNodeKind::RawElementAddress,
        frame.syntax,
        core.elementTypeKey);
    address->elementTypeKey = core.elementTypeKey;
    address->children.push_back(std::move(frame.first));
    address->children.push_back(std::move(linearIndex));
    lowered = loadRawIndexAddress(frame, std::move(address));
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::loadRawIndexAddress(
    const IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> address)
{
    if (frame.addressContext) return address;
    auto value = makeNode(
        ProjectOopHirNodeKind::PointerLoad,
        frame.syntax,
        frame.core->resultTypeKey);
    value->children.push_back(std::move(address));
    return value;
}

void ProjectOopHirModel::finishArrayRawIndex(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    const MethodExpressionTypeBinding* baseType =
        expressionType(frame.syntax->first());
    std::string indexedType = baseType != nullptr
        ? baseType->typeKey : std::string();
    std::unique_ptr<ProjectOopHirNode> base = std::move(frame.first);
    for (std::size_t dimension = 0;
        dimension < frame.operands.size(); ++dimension)
    {
        const std::optional<std::string> fixedElement = frame.baseIsFixedArray
            ? ProjectMemberModel::fixedArrayElementTypeKey(indexedType)
            : std::optional<std::string>();
        const std::optional<std::string> flexibleElement = frame.baseIsFlexibleArray
            ? ProjectMemberModel::flexibleArrayElementTypeKey(indexedType)
            : std::optional<std::string>();
        const std::string elementType = fixedElement.has_value()
            ? *fixedElement
            : flexibleElement.has_value() ? *flexibleElement
                                          : frame.core->elementTypeKey;
        auto address = makeNode(
            ProjectOopHirNodeKind::RawElementAddress,
            frame.syntax,
            elementType);
        address->elementTypeKey = elementType;
        applyRawIndexTrailingBounds(*address, frame);
        address->children.push_back(std::move(base));
        address->children.push_back(std::move(frame.operands[dimension]));
        base = std::move(address);
        indexedType = elementType;
    }
    lowered = loadRawIndexAddress(frame, std::move(base));
}

void ProjectOopHirModel::applyRawIndexTrailingBounds(
    ProjectOopHirNode& address,
    const IterativeExpressionFrame& frame)
{
    const ast::AstNode* baseSyntax = frame.syntax->first();
    if (!frame.baseIsFlexibleArray || metaprogrammingModel_ == nullptr ||
        baseSyntax == nullptr || baseSyntax->first() == nullptr ||
        inheritanceModel_ == nullptr || memberModel_ == nullptr ||
        objectLayoutModel_ == nullptr)
        return;
    const MethodExpressionTypeBinding* receiverType =
        expressionType(baseSyntax->first());
    if (receiverType == nullptr) return;
    const std::optional<std::size_t> structOrdinal =
        inheritanceModel_->typeOrdinalForTypeKey(receiverType->typeKey);
    if (!structOrdinal.has_value()) return;
    for (const StaticAttributeInfo& attribute :
         metaprogrammingModel_->attributes())
    {
        if (attribute.identity != "TrailingStorage" ||
            attribute.constantArguments.empty() ||
            !attribute.typeOrdinal.has_value() ||
            attribute.typeOrdinal != structOrdinal)
            continue;
        const std::span<const std::size_t> lengthMembers =
            memberModel_->findMembers(
                *structOrdinal, attribute.constantArguments[0]);
        if (!lengthMembers.empty() && attribute.memberOrdinal.has_value())
        {
            const ObjectFieldLayout* lengthLayout =
                objectLayoutModel_->fieldLayoutForMember(lengthMembers.front());
            const ObjectFieldLayout* dataLayout =
                objectLayoutModel_->fieldLayoutForMember(*attribute.memberOrdinal);
            if (lengthLayout != nullptr && dataLayout != nullptr)
            {
                address.hasTrailingLengthBounds = true;
                address.trailingLengthRelativeOffset =
                    static_cast<std::int64_t>(lengthLayout->offset) -
                    static_cast<std::int64_t>(dataLayout->offset);
            }
        }
        break;
    }
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledNamespaceCallFrame(
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
        frames.back().kind == IterativeExpressionKind::NamespaceCall)
    {
        IterativeExpressionFrame& frame = frames.back();
        NamespaceCallFrameState& namespaceCall = *frame.namespaceCall;
        if (!acceptNamespaceCallChild(
                namespaceCall, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextNamespaceCallChild(
                namespaceCall, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        lowered = finishNamespaceCall(
            frame.syntax,
            frame.addressContext,
            error,
            *namespaceCall.binding,
            *namespaceCall.target,
            std::move(namespaceCall.node),
            namespaceCall.hasOwnedTemporary);
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

LoopBlockAdvance ProjectOopHirModel::completeScheduledEventInvokeFrame(
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
        frames.back().kind == IterativeExpressionKind::EventInvoke)
    {
        IterativeExpressionFrame& frame = frames.back();
        if (!acceptEventInvokeChild(frame, std::move(lowered), error))
        {
            if (!lastErrorRange_.isValid())
                lastErrorRange_ = frame.syntax->range();
            lowered.reset();
            frames.clear();
            break;
        }
        EventInvokeFrameState& eventInvoke = *frame.eventInvoke;
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextEventInvokeChild(
                eventInvoke, childSyntax, childAddressContext))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = childAddressContext;
            descend = true;
            break;
        }
        eventInvoke.node->argumentCount = frame.syntax->children().size();
        lowered = std::move(eventInvoke.node);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledDelegateInvokeFrame(
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
        frames.back().kind == IterativeExpressionKind::DelegateInvoke)
    {
        IterativeExpressionFrame& frame = frames.back();
        acceptDelegateInvokeChild(frame, std::move(lowered));
        DelegateInvokeFrameState& delegateInvoke = *frame.delegateInvoke;
        const ast::AstNode* childSyntax = nullptr;
        if (nextDelegateInvokeChild(frame, childSyntax))
        {
            current = childSyntax;
            currentMemberOrdinal = frame.memberOrdinal;
            currentAddressContext = false;
            descend = true;
            break;
        }
        lowered = finishDelegateInvoke(
            frame.syntax,
            error,
            *delegateInvoke.binding,
            std::move(delegateInvoke.node));
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

}
