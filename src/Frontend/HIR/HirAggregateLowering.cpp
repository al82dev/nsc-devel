#include "HirLoweringSupport.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDeconstructionSource(
    const ast::AstNode* bindingSyntax,
    const ast::AstNode* sourceSyntax,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodDeconstructionBinding* binding = methodBodyModel_ != nullptr
        ? methodBodyModel_->deconstructionBinding(bindingSyntax) : nullptr;
    if (binding == nullptr)
    {
        error = "T02-F03 deconstruction has no semantic source binding";
        return nullptr;
    }
    if (!binding->selectedMemberOrdinal.has_value())
        return lowerExpression(sourceSyntax, memberOrdinal, false, error);

    auto receiver = lowerReceiver(
        sourceSyntax, binding->sourceTypeKey, memberOrdinal, error);
    if (receiver == nullptr) return nullptr;
    auto call = makeNode(
        ProjectOopHirNodeKind::InstanceCall, sourceSyntax, binding->tupleTypeKey);
    call->memberOrdinal = *binding->selectedMemberOrdinal;
    call->children.push_back(std::move(receiver));
    return call;
}

// C7 "aggregate initializer". Semantic analysis has already established that the shape is legal
// (see ProjectMethodBodyModel::bindAggregateInitializer); this only has to say WHERE each value
// goes. Nesting is resolved here, against the same StructValueLayout the C header generator emits
// offsets from, so an initializer and a C host cannot disagree about which bytes a field occupies.
bool ProjectOopHirModel::appendAggregateElements(
    const ast::AstNode* syntax,
    const std::string& targetTypeKey,
    std::uint64_t baseOffset,
    std::size_t memberOrdinal,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& elements,
    std::string& error)
{
    // Per-element type and offset, for the two aggregate shapes the language has.
    struct AggregateSlot final
    {
        std::string typeKey;
        std::uint64_t offset = 0;
        std::optional<std::uint64_t> bitOffset;
        std::optional<std::uint64_t> bitWidth;
        std::uint64_t storageSize = 0;
        std::uint64_t storageAlignment = 1;
        bool volatileAccess = false;
    };
    std::vector<AggregateSlot> slots;
    if (const std::optional<std::vector<std::string>> tupleElements =
            ProjectTypeConversion::tupleElementTypeKeys(targetTypeKey);
        tupleElements.has_value())
    {
        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
        {
            error = "tuple initializer requires the object layout model";
            return false;
        }
        const std::optional<language::AggregateLayout> layout =
            objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, targetTypeKey);
        if (!layout.has_value() || layout->offsets.size() != tupleElements->size())
        {
            error = "tuple initializer has no structural layout for '" + targetTypeKey + "'";
            return false;
        }
        for (std::size_t index = 0; index < tupleElements->size(); ++index)
        {
            const std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout =
                objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, (*tupleElements)[index]);
            if (!elementLayout.has_value())
            {
                error = "tuple initializer cannot size element " + std::to_string(index);
                return false;
            }
            slots.push_back(AggregateSlot{
                (*tupleElements)[index],
                static_cast<std::uint64_t>(layout->offsets[index]),
                std::nullopt, std::nullopt, 0, elementLayout->second, false});
        }
    }
    else if (const std::optional<std::string> elementTypeKey =
            ProjectMemberModel::fixedArrayElementTypeKey(targetTypeKey);
        elementTypeKey.has_value())
    {
        const std::optional<std::uint64_t> count =
            ProjectMemberModel::fixedArrayCount(targetTypeKey);
        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
        {
            error = "C7 aggregate initializer requires the object layout model";
            return false;
        }
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> layout =
            objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, *elementTypeKey);
        if (!count.has_value() || !layout.has_value() || layout->first == 0)
        {
            error = "C7 aggregate initializer cannot size the element type of '" + targetTypeKey + "'";
            return false;
        }
        for (std::uint64_t index = 0; index < *count; ++index)
        {
            slots.push_back(AggregateSlot{
                *elementTypeKey, index * layout->first, std::nullopt, std::nullopt, 0,
                layout->second, false});
        }
    }
    else
    {
        if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
        {
            error = "C7 aggregate initializer requires the object layout model";
            return false;
        }
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(targetTypeKey);
        const StructValueLayout* layout = typeOrdinal.has_value()
            ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
        if (layout == nullptr)
        {
            error = "C7 aggregate initializer has no value layout for '" + targetTypeKey + "'";
            return false;
        }
        for (const ObjectFieldLayout& field : layout->fields)
        {
            const ClassMemberSymbol* member = memberModel_ != nullptr &&
                    field.memberOrdinal < memberModel_->members().size()
                ? &memberModel_->members()[field.memberOrdinal] : nullptr;
            if (member != nullptr && member->isUnnamedBitField) continue;
            slots.push_back(AggregateSlot{
                field.typeKey,
                field.offset,
                field.bitOffset,
                field.bitWidth,
                field.bitWidth.has_value() ? field.size : 0,
                field.alignment,
                member != nullptr && member->isVolatile});
            // A union takes exactly one initializer, for its first member -- the same rule C has,
            // and the same one bindAggregateInitializer enforced.
            if (layout->isUnion) break;
        }
    }

    const MethodAggregateInitializerBinding* binding = methodBodyModel_ != nullptr
        ? methodBodyModel_->aggregateInitializerBinding(syntax) : nullptr;
    if (binding == nullptr || binding->entries.size() != syntax->children().size())
    {
        error = "T01-F05 aggregate initializer has no semantic element mapping";
        return false;
    }

    if (syntax->kind() == ast::AstNodeKind::WithExpression)
    {
        if (syntax->first() == nullptr)
        {
            error = "T01-F07 with expression has no source value";
            return false;
        }
        auto source = lowerExpression(syntax->first(), memberOrdinal, false, error);
        if (source == nullptr) return false;
        auto spread = makeNode(ProjectOopHirNodeKind::AggregateSpread, syntax, targetTypeKey);
        spread->children.push_back(std::move(source));
        for (const AggregateSlot& target : slots)
        {
            if (target.bitWidth.has_value())
            {
                error = "T01-F07 with expressions over bit fields require explicit copy semantics";
                return false;
            }
            auto descriptor = makeNode(
                ProjectOopHirNodeKind::AggregateElement, syntax, target.typeKey);
            descriptor->integerValue = baseOffset + target.offset;
            descriptor->secondaryIntegerValue = target.offset;
            descriptor->bitStorageAlignment = target.storageAlignment;
            descriptor->volatileAccess = target.volatileAccess;
            spread->children.push_back(std::move(descriptor));
        }
        elements.push_back(std::move(spread));
    }

    for (std::size_t index = 0; index < binding->entries.size(); ++index)
    {
        const MethodAggregateInitializerEntryBinding& entry = binding->entries[index];
        const ast::AstNode* element = entry.syntax;
        if (entry.spread)
        {
            if (element == nullptr || element->kind() != ast::AstNodeKind::SpreadElementExpression ||
                element->first() == nullptr)
            {
                error = "T01-F05 aggregate spread has no source expression";
                return false;
            }
            const ast::AstNode* sourceSyntax = element->first();
            if (entry.moveSpread && sourceSyntax != nullptr &&
                sourceSyntax->kind() == ast::AstNodeKind::MoveExpression)
            {
                sourceSyntax = sourceSyntax->first();
            }
            auto source = lowerExpression(
                sourceSyntax, memberOrdinal, entry.moveSpread, error);
            if (source == nullptr) return false;
            auto spread = makeNode(ProjectOopHirNodeKind::AggregateSpread, element, targetTypeKey);
            spread->booleanValue = entry.moveSpread;
            spread->localOrdinal = entry.moveSourceLocalOrdinal;
            spread->parameterOrdinal = entry.moveSourceParameterOrdinal;
            spread->children.push_back(std::move(source));
            for (const std::size_t targetOrdinal : entry.targetElementOrdinals)
            {
                if (targetOrdinal >= slots.size())
                {
                    error = "T01-F05 aggregate spread target is out of range";
                    return false;
                }
                const AggregateSlot& target = slots[targetOrdinal];
                auto descriptor = makeNode(
                    ProjectOopHirNodeKind::AggregateElement,
                    element,
                    target.typeKey);
                descriptor->integerValue = baseOffset + target.offset;
                descriptor->secondaryIntegerValue = target.offset;
                descriptor->bitOffset = target.bitOffset;
                descriptor->bitWidth = target.bitWidth;
                descriptor->bitStorageSize = target.storageSize;
                descriptor->bitStorageAlignment = target.storageAlignment;
                descriptor->volatileAccess = target.volatileAccess;
                if (target.bitWidth.has_value())
                {
                    error = "T01-F05 spread initialization of bit fields requires explicit named values";
                    return false;
                }
                spread->children.push_back(std::move(descriptor));
            }
            elements.push_back(std::move(spread));
            continue;
        }
        if (entry.targetElementOrdinals.size() != 1U ||
            entry.targetElementOrdinals.front() >= slots.size())
        {
            error = "T01-F05 aggregate value has no unique target element";
            return false;
        }
        const AggregateSlot& target = slots[entry.targetElementOrdinals.front()];
        const std::string& slotTypeKey = target.typeKey;
        const std::uint64_t slotOffset = baseOffset + target.offset;
        if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            if (!appendAggregateElements(
                    element, slotTypeKey, slotOffset, memberOrdinal, elements, error))
            {
                return false;
            }
            continue;
        }
        auto value = lowerExpression(element, memberOrdinal, false, error);
        if (value == nullptr)
        {
            return false;
        }
        const ProjectImplicitConversion conversion = classifyImplicit(
            value->typeKey,
            slotTypeKey,
            expressionType(element) != nullptr ? expressionType(element)->integerConstant
                                              : std::nullopt);
        if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
        {
            value = wrapImplicitConversion(std::move(value), conversion, element);
        }
        auto slot = makeNode(ProjectOopHirNodeKind::AggregateElement, element, slotTypeKey);
        slot->integerValue = slotOffset;
        slot->bitOffset = target.bitOffset;
        slot->bitWidth = target.bitWidth;
        slot->bitStorageSize = target.storageSize;
        slot->bitStorageAlignment = target.storageAlignment;
        slot->volatileAccess = target.volatileAccess;
        slot->children.push_back(std::move(value));
        elements.push_back(std::move(slot));
    }
    return true;
}

// C8 "type root", struct half: flattens a struct into its SCALAR LEAVES -- one AggregateElement
// per primitive field, with nested structs and fixed arrays expanded in place and their absolute
// byte offsets computed here. Padding bytes are deliberately not leaves: a struct that crossed a C
// boundary carries whatever the other side left in its padding, so comparing or hashing those
// bytes would make two equal values disagree.
//
// The contract these leaves define, and the reason it is stated in exactly one place: the
// synthesized Equals is BIT equality of the fields, and the synthesized GetHashCode mixes the
// same bits. That keeps the two in agreement by construction -- which a "compare floats with
// fcmp, hash their bits" pairing would not, since +0.0 and -0.0 compare equal but hash apart, and
// a NaN would never equal itself while hashing identically. Anything wanting other semantics --
// value comparison of a string field, deep comparison through a reference -- declares its own
// Equals and GetHashCode, and the compiler then steps aside entirely.
bool ProjectOopHirModel::appendAggregateLeaves(
    const std::string& typeKey,
    std::uint64_t baseOffset,
    const ast::AstNode* syntax,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& leaves,
    std::string& error)
{
    if (const std::optional<std::string> elementTypeKey =
            ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        elementTypeKey.has_value())
    {
        const std::optional<std::uint64_t> count = ProjectMemberModel::fixedArrayCount(typeKey);
        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr || !count.has_value())
        {
            error = "C8 cannot expand fixed-array field '" + typeKey + "'";
            return false;
        }
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> stride =
            objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, *elementTypeKey);
        if (!stride.has_value() || stride->first == 0)
        {
            error = "C8 cannot size fixed-array element '" + *elementTypeKey + "'";
            return false;
        }
        for (std::uint64_t index = 0; index < *count; ++index)
        {
            if (!appendAggregateLeaves(
                    *elementTypeKey, baseOffset + index * stride->first, syntax, leaves, error))
            {
                return false;
            }
        }
        return true;
    }

    if (isStructValueType(typeKey))
    {
        if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
        {
            error = "C8 struct root member requires the object layout model";
            return false;
        }
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(typeKey);
        const StructValueLayout* layout = typeOrdinal.has_value()
            ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
        if (layout == nullptr)
        {
            error = "C8 struct root member has no value layout for '" + typeKey + "'";
            return false;
        }
        for (const ObjectFieldLayout& field : layout->fields)
        {
            if (!appendAggregateLeaves(
                    field.typeKey, baseOffset + field.offset, syntax, leaves, error))
            {
                return false;
            }
        }
        return true;
    }

    auto leaf = makeNode(ProjectOopHirNodeKind::AggregateElement, syntax, typeKey);
    leaf->integerValue = baseOffset;
    leaves.push_back(std::move(leaf));
    return true;
}

}
