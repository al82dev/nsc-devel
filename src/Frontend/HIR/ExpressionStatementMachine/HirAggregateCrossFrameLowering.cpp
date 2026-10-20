#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <optional>
#include <utility>

namespace nativesharp::frontend
{
using internal::AggregateCrossFramePhase;
using internal::AggregateCrossFrameState;
using internal::AggregateCrossSlot;
using internal::AggregateCrossStructuralFrame;
using internal::IterativeExpressionFrame;

bool ProjectOopHirModel::prepareAggregateCrossStructuralFrame(
    AggregateCrossStructuralFrame& structural,
    [[maybe_unused]] std::string& error)
{
    const auto tupleElements =
        ProjectTypeConversion::tupleElementTypeKeys(structural.targetTypeKey);
    if (tupleElements.has_value())
    {
        if (!prepareTupleAggregateCrossStructuralFrame(
                structural, *tupleElements, error)) return false;
    }
    else if (const auto elementTypeKey =
                 ProjectMemberModel::fixedArrayElementTypeKey(
                     structural.targetTypeKey);
             elementTypeKey.has_value())
    {
        if (!prepareFixedArrayAggregateCrossStructuralFrame(
                structural, *elementTypeKey, error)) return false;
    }
    else if (!prepareStructAggregateCrossStructuralFrame(structural, error))
    {
        return false;
    }
    structural.binding = methodBodyModel_ != nullptr
        ? methodBodyModel_->aggregateInitializerBinding(structural.syntax)
        : nullptr;
    if (structural.binding == nullptr ||
        structural.binding->entries.size() != structural.syntax->children().size())
    {
        error = "T01-F05 aggregate initializer has no semantic element mapping";
        return false;
    }
    structural.initialized = true;
    return true;
}

bool ProjectOopHirModel::prepareTupleAggregateCrossStructuralFrame(
    AggregateCrossStructuralFrame& structural,
    const std::vector<std::string>& tupleElements,
    [[maybe_unused]] std::string& error)
{
    if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
    {
        error = "tuple initializer requires the object layout model";
        return false;
    }
    const auto layout = objectLayoutModel_->tupleLayoutForTypeKey(
        *nameLookup_, structural.targetTypeKey);
    if (!layout.has_value() || layout->offsets.size() != tupleElements.size())
    {
        error = "tuple initializer has no structural layout for '" +
            structural.targetTypeKey + "'";
        return false;
    }
    for (std::size_t index = 0U; index < tupleElements.size(); ++index)
    {
        const auto elementLayout = objectLayoutModel_->valueLayoutForTypeKey(
            *nameLookup_, tupleElements[index]);
        if (!elementLayout.has_value())
        {
            error = "tuple initializer cannot size element " +
                std::to_string(index);
            return false;
        }
        structural.slots.push_back(AggregateCrossSlot{
            tupleElements[index],
            static_cast<std::uint64_t>(layout->offsets[index]),
            std::nullopt,
            std::nullopt,
            0U,
            elementLayout->second,
            false});
    }
    return true;
}

bool ProjectOopHirModel::prepareFixedArrayAggregateCrossStructuralFrame(
    AggregateCrossStructuralFrame& structural,
    std::string_view elementTypeKey,
    [[maybe_unused]] std::string& error)
{
    const auto count = ProjectMemberModel::fixedArrayCount(structural.targetTypeKey);
    if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
    {
        error = "C7 aggregate initializer requires the object layout model";
        return false;
    }
    const auto layout = objectLayoutModel_->valueLayoutForTypeKey(
        *nameLookup_, elementTypeKey);
    if (!count.has_value() || !layout.has_value() || layout->first == 0U)
    {
        error = "C7 aggregate initializer cannot size the element type of '" +
            structural.targetTypeKey + "'";
        return false;
    }
    for (std::uint64_t index = 0U; index < *count; ++index)
    {
        structural.slots.push_back(AggregateCrossSlot{
            std::string(elementTypeKey),
            index * layout->first,
            std::nullopt,
            std::nullopt,
            0U,
            layout->second,
            false});
    }
    return true;
}

bool ProjectOopHirModel::prepareStructAggregateCrossStructuralFrame(
    AggregateCrossStructuralFrame& structural,
    [[maybe_unused]] std::string& error)
{
    if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
    {
        error = "C7 aggregate initializer requires the object layout model";
        return false;
    }
    const auto typeOrdinal =
        inheritanceModel_->typeOrdinalForTypeKey(structural.targetTypeKey);
    const StructValueLayout* layout = typeOrdinal.has_value()
        ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
    if (layout == nullptr)
    {
        error = "C7 aggregate initializer has no value layout for '" +
            structural.targetTypeKey + "'";
        return false;
    }
    for (const ObjectFieldLayout& field : layout->fields)
    {
        const ClassMemberSymbol* member = memberModel_ != nullptr &&
                field.memberOrdinal < memberModel_->members().size()
            ? &memberModel_->members()[field.memberOrdinal] : nullptr;
        if (member != nullptr && member->isUnnamedBitField) continue;
        structural.slots.push_back(AggregateCrossSlot{
            field.typeKey,
            field.offset,
            field.bitOffset,
            field.bitWidth,
            field.bitWidth.has_value() ? field.size : 0U,
            field.alignment,
            member != nullptr && member->isVolatile});
        if (layout->isUnion) break;
    }
    return true;
}

bool ProjectOopHirModel::advanceAggregateCrossFrame(
    AggregateCrossFrameState& aggregate,
    [[maybe_unused]] std::string& error)
{
    while (!aggregate.structuralFrames.empty())
    {
        AggregateCrossStructuralFrame& structural =
            aggregate.structuralFrames.back();
        if (!structural.initialized)
        {
            if (!prepareAggregateCrossStructuralFrame(structural, error)) return false;
            if (structural.syntax->kind() == ast::AstNodeKind::WithExpression)
            {
                if (structural.syntax->first() == nullptr)
                {
                    error = "T01-F07 with expression has no source value";
                    return false;
                }
                aggregate.pendingSyntax = structural.syntax->first();
                aggregate.phase = AggregateCrossFramePhase::AwaitWithSource;
                return true;
            }
        }
        if (structural.entryIndex >= structural.binding->entries.size())
        {
            aggregate.structuralFrames.pop_back();
            continue;
        }
        bool continueStructural = false;
        if (!advanceAggregateCrossEntry(
                aggregate, structural, continueStructural, error)) return false;
        if (!continueStructural) return true;
    }
    aggregate.phase = AggregateCrossFramePhase::Ready;
    aggregate.pendingSyntax = nullptr;
    return true;
}

bool ProjectOopHirModel::advanceAggregateCrossEntry(
    AggregateCrossFrameState& aggregate,
    AggregateCrossStructuralFrame& structural,
    bool& continueStructural,
    [[maybe_unused]] std::string& error)
{
    const MethodAggregateInitializerEntryBinding& entry =
        structural.binding->entries[structural.entryIndex++];
    const ast::AstNode* element = entry.syntax;
    if (entry.spread)
    {
        if (element == nullptr ||
            element->kind() != ast::AstNodeKind::SpreadElementExpression ||
            element->first() == nullptr)
        {
            error = "T01-F05 aggregate spread has no source expression";
            return false;
        }
        const ast::AstNode* sourceSyntax = element->first();
        if (entry.moveSpread &&
            sourceSyntax->kind() == ast::AstNodeKind::MoveExpression)
        {
            sourceSyntax = sourceSyntax->first();
        }
        aggregate.pendingSyntax = sourceSyntax;
        aggregate.pendingTargetOrdinals = entry.targetElementOrdinals;
        aggregate.pendingMoveSpread = entry.moveSpread;
        aggregate.pendingMoveSourceLocalOrdinal = entry.moveSourceLocalOrdinal;
        aggregate.pendingMoveSourceParameterOrdinal =
            entry.moveSourceParameterOrdinal;
        aggregate.phase = AggregateCrossFramePhase::AwaitSpreadSource;
        return true;
    }
    if (entry.targetElementOrdinals.size() != 1U ||
        entry.targetElementOrdinals.front() >= structural.slots.size())
    {
        error = "T01-F05 aggregate value has no unique target element";
        return false;
    }
    const AggregateCrossSlot target =
        structural.slots[entry.targetElementOrdinals.front()];
    const std::uint64_t slotOffset = structural.baseOffset + target.offset;
    if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
    {
        AggregateCrossStructuralFrame child;
        child.syntax = element;
        child.targetTypeKey = target.typeKey;
        child.baseOffset = slotOffset;
        aggregate.structuralFrames.push_back(std::move(child));
        continueStructural = true;
        return true;
    }
    aggregate.pendingSyntax = element;
    aggregate.pendingSlot = target;
    aggregate.pendingSlot.offset = slotOffset;
    aggregate.phase = AggregateCrossFramePhase::AwaitValue;
    return true;
}

bool ProjectOopHirModel::initializeAggregateCrossFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
{
    const MethodExpressionTypeBinding* targetType = expressionType(frame.syntax);
    if (targetType == nullptr)
    {
        error = "aggregate/with initializer has no bound target type";
        return false;
    }
    AggregateCrossFrameState aggregate;
    aggregate.node = makeNode(
        ProjectOopHirNodeKind::AggregateInitializer,
        frame.syntax,
        targetType->typeKey);
    AggregateCrossStructuralFrame root;
    root.syntax = frame.syntax;
    root.targetTypeKey = targetType->typeKey;
    aggregate.structuralFrames.push_back(std::move(root));
    if (!advanceAggregateCrossFrame(aggregate, error)) return false;
    frame.aggregateCross.emplace(std::move(aggregate));
    return true;
}

bool ProjectOopHirModel::nextAggregateCrossChild(
    const AggregateCrossFrameState& aggregate,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    if (aggregate.phase == AggregateCrossFramePhase::AwaitWithSource ||
        aggregate.phase == AggregateCrossFramePhase::AwaitValue)
    {
        syntax = aggregate.pendingSyntax;
        addressContext = false;
        return true;
    }
    if (aggregate.phase == AggregateCrossFramePhase::AwaitSpreadSource)
    {
        syntax = aggregate.pendingSyntax;
        addressContext = aggregate.pendingMoveSpread;
        return true;
    }
    return false;
}

bool ProjectOopHirModel::acceptAggregateCrossChild(
    AggregateCrossFrameState& aggregate,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    if (aggregate.structuralFrames.empty())
    {
        error = "Stage 34 aggregate frame lost its structural owner";
        return false;
    }
    const AggregateCrossStructuralFrame& structural =
        aggregate.structuralFrames.back();
    if (aggregate.phase == AggregateCrossFramePhase::AwaitWithSource)
    {
        if (!acceptAggregateWithSource(
                aggregate, structural, std::move(child), error)) return false;
    }
    else if (aggregate.phase == AggregateCrossFramePhase::AwaitSpreadSource)
    {
        if (!acceptAggregateSpreadSource(
                aggregate, structural, std::move(child), error)) return false;
    }
    else if (aggregate.phase == AggregateCrossFramePhase::AwaitValue)
    {
        acceptAggregateValue(aggregate, std::move(child));
    }
    else
    {
        error = "Stage 34 aggregate frame accepted an unexpected child";
        return false;
    }
    aggregate.phase = AggregateCrossFramePhase::Advance;
    aggregate.pendingSyntax = nullptr;
    aggregate.pendingTargetOrdinals.clear();
    return advanceAggregateCrossFrame(aggregate, error);
}

bool ProjectOopHirModel::acceptAggregateWithSource(
    AggregateCrossFrameState& aggregate,
    const AggregateCrossStructuralFrame& structural,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    auto spread = makeNode(
        ProjectOopHirNodeKind::AggregateSpread,
        structural.syntax,
        structural.targetTypeKey);
    spread->children.push_back(std::move(child));
    for (const AggregateCrossSlot& target : structural.slots)
    {
        if (target.bitWidth.has_value())
        {
            error =
                "T01-F07 with expressions over bit fields require explicit copy semantics";
            return false;
        }
        auto descriptor = makeNode(
            ProjectOopHirNodeKind::AggregateElement,
            structural.syntax,
            target.typeKey);
        descriptor->integerValue = structural.baseOffset + target.offset;
        descriptor->secondaryIntegerValue = target.offset;
        descriptor->bitStorageAlignment = target.storageAlignment;
        descriptor->volatileAccess = target.volatileAccess;
        spread->children.push_back(std::move(descriptor));
    }
    aggregate.node->children.push_back(std::move(spread));
    return true;
}

bool ProjectOopHirModel::acceptAggregateSpreadSource(
    AggregateCrossFrameState& aggregate,
    const AggregateCrossStructuralFrame& structural,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    auto spread = makeNode(
        ProjectOopHirNodeKind::AggregateSpread,
        structural.binding->entries[structural.entryIndex - 1U].syntax,
        structural.targetTypeKey);
    spread->booleanValue = aggregate.pendingMoveSpread;
    spread->localOrdinal = aggregate.pendingMoveSourceLocalOrdinal;
    spread->parameterOrdinal = aggregate.pendingMoveSourceParameterOrdinal;
    spread->children.push_back(std::move(child));
    for (const std::size_t targetOrdinal : aggregate.pendingTargetOrdinals)
    {
        if (targetOrdinal >= structural.slots.size())
        {
            error = "T01-F05 aggregate spread target is out of range";
            return false;
        }
        const AggregateCrossSlot& target = structural.slots[targetOrdinal];
        auto descriptor = makeNode(
            ProjectOopHirNodeKind::AggregateElement,
            structural.binding->entries[structural.entryIndex - 1U].syntax,
            target.typeKey);
        descriptor->integerValue = structural.baseOffset + target.offset;
        descriptor->secondaryIntegerValue = target.offset;
        descriptor->bitOffset = target.bitOffset;
        descriptor->bitWidth = target.bitWidth;
        descriptor->bitStorageSize = target.storageSize;
        descriptor->bitStorageAlignment = target.storageAlignment;
        descriptor->volatileAccess = target.volatileAccess;
        if (target.bitWidth.has_value())
        {
            error =
                "T01-F05 spread initialization of bit fields requires explicit named values";
            return false;
        }
        spread->children.push_back(std::move(descriptor));
    }
    aggregate.node->children.push_back(std::move(spread));
    return true;
}

void ProjectOopHirModel::acceptAggregateValue(
    AggregateCrossFrameState& aggregate,
    std::unique_ptr<ProjectOopHirNode> child)
{
    const MethodExpressionTypeBinding* valueType =
        expressionType(aggregate.pendingSyntax);
    const ProjectImplicitConversion conversion = classifyImplicit(
        child->typeKey,
        aggregate.pendingSlot.typeKey,
        valueType != nullptr ? valueType->integerConstant : std::nullopt);
    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
    {
        child = wrapImplicitConversion(
            std::move(child), conversion, aggregate.pendingSyntax);
    }
    auto slot = makeNode(
        ProjectOopHirNodeKind::AggregateElement,
        aggregate.pendingSyntax,
        aggregate.pendingSlot.typeKey);
    slot->integerValue = aggregate.pendingSlot.offset;
    slot->bitOffset = aggregate.pendingSlot.bitOffset;
    slot->bitWidth = aggregate.pendingSlot.bitWidth;
    slot->bitStorageSize = aggregate.pendingSlot.storageSize;
    slot->bitStorageAlignment = aggregate.pendingSlot.storageAlignment;
    slot->volatileAccess = aggregate.pendingSlot.volatileAccess;
    slot->children.push_back(std::move(child));
    aggregate.node->children.push_back(std::move(slot));
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishAggregateCrossFrame([[maybe_unused]] IterativeExpressionFrame& frame)
{
    return std::move(frame.aggregateCross->node);
}
}
