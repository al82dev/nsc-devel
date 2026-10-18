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
    struct AggregateFrame final
    {
        const ast::AstNode* syntax = nullptr;
        std::string targetTypeKey;
        std::uint64_t baseOffset = 0;
        std::vector<AggregateSlot> slots;
        const MethodAggregateInitializerBinding* binding = nullptr;
        std::size_t entryIndex = 0;
        bool initialized = false;
    };

    AggregateFrame root;
    root.syntax = syntax;
    root.targetTypeKey = targetTypeKey;
    root.baseOffset = baseOffset;
    std::vector<AggregateFrame> pending;
    pending.push_back(std::move(root));
    while (!pending.empty())
    {
        AggregateFrame& frame = pending.back();
        if (!frame.initialized)
        {
            if (const auto tupleElements =
                    ProjectTypeConversion::tupleElementTypeKeys(frame.targetTypeKey);
                tupleElements.has_value())
            {
                if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                {
                    error = "tuple initializer requires the object layout model";
                    return false;
                }
                const auto layout = objectLayoutModel_->tupleLayoutForTypeKey(
                    *nameLookup_, frame.targetTypeKey);
                if (!layout.has_value() || layout->offsets.size() != tupleElements->size())
                {
                    error = "tuple initializer has no structural layout for '" +
                        frame.targetTypeKey + "'";
                    return false;
                }
                for (std::size_t index = 0; index < tupleElements->size(); ++index)
                {
                    const auto elementLayout = objectLayoutModel_->valueLayoutForTypeKey(
                        *nameLookup_, (*tupleElements)[index]);
                    if (!elementLayout.has_value())
                    {
                        error = "tuple initializer cannot size element " +
                            std::to_string(index);
                        return false;
                    }
                    frame.slots.push_back(AggregateSlot{
                        (*tupleElements)[index],
                        static_cast<std::uint64_t>(layout->offsets[index]),
                        std::nullopt,
                        std::nullopt,
                        0,
                        elementLayout->second,
                        false});
                }
            }
            else if (const auto elementTypeKey =
                    ProjectMemberModel::fixedArrayElementTypeKey(frame.targetTypeKey);
                elementTypeKey.has_value())
            {
                const auto count = ProjectMemberModel::fixedArrayCount(frame.targetTypeKey);
                if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                {
                    error = "C7 aggregate initializer requires the object layout model";
                    return false;
                }
                const auto layout = objectLayoutModel_->valueLayoutForTypeKey(
                    *nameLookup_, *elementTypeKey);
                if (!count.has_value() || !layout.has_value() || layout->first == 0)
                {
                    error = "C7 aggregate initializer cannot size the element type of '" +
                        frame.targetTypeKey + "'";
                    return false;
                }
                for (std::uint64_t index = 0; index < *count; ++index)
                {
                    frame.slots.push_back(AggregateSlot{
                        *elementTypeKey,
                        index * layout->first,
                        std::nullopt,
                        std::nullopt,
                        0,
                        layout->second,
                        false});
                }
            }
            else
            {
                if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
                {
                    error = "C7 aggregate initializer requires the object layout model";
                    return false;
                }
                const auto typeOrdinal =
                    inheritanceModel_->typeOrdinalForTypeKey(frame.targetTypeKey);
                const StructValueLayout* layout = typeOrdinal.has_value()
                    ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
                if (layout == nullptr)
                {
                    error = "C7 aggregate initializer has no value layout for '" +
                        frame.targetTypeKey + "'";
                    return false;
                }
                for (const ObjectFieldLayout& field : layout->fields)
                {
                    const ClassMemberSymbol* member = memberModel_ != nullptr &&
                            field.memberOrdinal < memberModel_->members().size()
                        ? &memberModel_->members()[field.memberOrdinal] : nullptr;
                    if (member != nullptr && member->isUnnamedBitField) continue;
                    frame.slots.push_back(AggregateSlot{
                        field.typeKey,
                        field.offset,
                        field.bitOffset,
                        field.bitWidth,
                        field.bitWidth.has_value() ? field.size : 0,
                        field.alignment,
                        member != nullptr && member->isVolatile});
                    if (layout->isUnion) break;
                }
            }

            frame.binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->aggregateInitializerBinding(frame.syntax) : nullptr;
            if (frame.binding == nullptr ||
                frame.binding->entries.size() != frame.syntax->children().size())
            {
                error = "T01-F05 aggregate initializer has no semantic element mapping";
                return false;
            }
            if (frame.syntax->kind() == ast::AstNodeKind::WithExpression)
            {
                if (frame.syntax->first() == nullptr)
                {
                    error = "T01-F07 with expression has no source value";
                    return false;
                }
                auto source = lowerExpression(
                    frame.syntax->first(), memberOrdinal, false, error);
                if (source == nullptr) return false;
                auto spread = makeNode(
                    ProjectOopHirNodeKind::AggregateSpread,
                    frame.syntax,
                    frame.targetTypeKey);
                spread->children.push_back(std::move(source));
                for (const AggregateSlot& target : frame.slots)
                {
                    if (target.bitWidth.has_value())
                    {
                        error =
                            "T01-F07 with expressions over bit fields require explicit copy semantics";
                        return false;
                    }
                    auto descriptor = makeNode(
                        ProjectOopHirNodeKind::AggregateElement,
                        frame.syntax,
                        target.typeKey);
                    descriptor->integerValue = frame.baseOffset + target.offset;
                    descriptor->secondaryIntegerValue = target.offset;
                    descriptor->bitStorageAlignment = target.storageAlignment;
                    descriptor->volatileAccess = target.volatileAccess;
                    spread->children.push_back(std::move(descriptor));
                }
                elements.push_back(std::move(spread));
            }
            frame.initialized = true;
        }

        if (frame.entryIndex >= frame.binding->entries.size())
        {
            pending.pop_back();
            continue;
        }
        const MethodAggregateInitializerEntryBinding& entry =
            frame.binding->entries[frame.entryIndex++];
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
            auto spread = makeNode(
                ProjectOopHirNodeKind::AggregateSpread,
                element,
                frame.targetTypeKey);
            spread->booleanValue = entry.moveSpread;
            spread->localOrdinal = entry.moveSourceLocalOrdinal;
            spread->parameterOrdinal = entry.moveSourceParameterOrdinal;
            spread->children.push_back(std::move(source));
            for (const std::size_t targetOrdinal : entry.targetElementOrdinals)
            {
                if (targetOrdinal >= frame.slots.size())
                {
                    error = "T01-F05 aggregate spread target is out of range";
                    return false;
                }
                const AggregateSlot& target = frame.slots[targetOrdinal];
                auto descriptor = makeNode(
                    ProjectOopHirNodeKind::AggregateElement,
                    element,
                    target.typeKey);
                descriptor->integerValue = frame.baseOffset + target.offset;
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
            entry.targetElementOrdinals.front() >= frame.slots.size())
        {
            error = "T01-F05 aggregate value has no unique target element";
            return false;
        }
        const AggregateSlot& target = frame.slots[entry.targetElementOrdinals.front()];
        const std::string& slotTypeKey = target.typeKey;
        const std::uint64_t slotOffset = frame.baseOffset + target.offset;
        if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            AggregateFrame child;
            child.syntax = element;
            child.targetTypeKey = slotTypeKey;
            child.baseOffset = slotOffset;
            pending.push_back(std::move(child));
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
    std::vector<std::pair<std::string, std::uint64_t>> pending;
    pending.emplace_back(typeKey, baseOffset);
    while (!pending.empty())
    {
        auto [currentTypeKey, currentBaseOffset] = std::move(pending.back());
        pending.pop_back();
        const std::optional<std::string> elementTypeKey =
            ProjectMemberModel::fixedArrayElementTypeKey(currentTypeKey);
        if (elementTypeKey.has_value())
        {
            const std::optional<std::uint64_t> count =
                ProjectMemberModel::fixedArrayCount(currentTypeKey);
            if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr || !count.has_value())
            {
                error = "C8 cannot expand fixed-array field '" + currentTypeKey + "'";
                return false;
            }
            const auto stride =
                objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, *elementTypeKey);
            if (!stride.has_value() || stride->first == 0)
            {
                error = "C8 cannot size fixed-array element '" + *elementTypeKey + "'";
                return false;
            }
            for (std::uint64_t remaining = *count; remaining > 0; --remaining)
            {
                pending.emplace_back(
                    *elementTypeKey,
                    currentBaseOffset + (remaining - 1U) * stride->first);
            }
            continue;
        }

        if (isStructValueType(currentTypeKey))
        {
            if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
            {
                error = "C8 struct root member requires the object layout model";
                return false;
            }
            const std::optional<std::size_t> typeOrdinal =
                inheritanceModel_->typeOrdinalForTypeKey(currentTypeKey);
            const StructValueLayout* layout = typeOrdinal.has_value()
                ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
            if (layout == nullptr)
            {
                error =
                    "C8 struct root member has no value layout for '" + currentTypeKey + "'";
                return false;
            }
            for (std::size_t remaining = layout->fields.size(); remaining > 0; --remaining)
            {
                const ObjectFieldLayout& field = layout->fields[remaining - 1U];
                pending.emplace_back(
                    field.typeKey, currentBaseOffset + field.offset);
            }
            continue;
        }

        auto leaf = makeNode(
            ProjectOopHirNodeKind::AggregateElement, syntax, currentTypeKey);
        leaf->integerValue = currentBaseOffset;
        leaves.push_back(std::move(leaf));
    }
    return true;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishCollectionBuilderExpression(
    const ast::AstNode* expression,
    const MethodCoreBinding& core,
    std::size_t temporaryOrdinal,
    std::vector<std::unique_ptr<ProjectOopHirNode>> elements)
{
    const std::string& temporaryTypeKey = core.resultTypeKey;
    auto block = makeNode(
        ProjectOopHirNodeKind::BlockExpression, expression, core.resultTypeKey);
    auto create = makeNode(
        ProjectOopHirNodeKind::StaticCall, expression, temporaryTypeKey);
    create->memberOrdinal = core.builderMemberOrdinals[0];
    auto declaration = makeNode(
        ProjectOopHirNodeKind::LocalDeclaration, expression, temporaryTypeKey);
    declaration->localOrdinal = temporaryOrdinal;
    declaration->stringValue = "builder";
    declaration->children.push_back(std::move(create));
    block->children.push_back(std::move(declaration));

    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        const ast::AstNode* child = expression->children()[index];
        auto add = makeNode(ProjectOopHirNodeKind::InstanceCall, child, "builtin:void");
        add->memberOrdinal = core.builderMemberOrdinals[1];
        auto receiver = makeNode(
            ProjectOopHirNodeKind::LocalAddress, expression, temporaryTypeKey + "*");
        receiver->localOrdinal = temporaryOrdinal;
        add->children.push_back(std::move(receiver));
        add->children.push_back(std::move(elements[index]));
        auto statement = makeNode(
            ProjectOopHirNodeKind::ExpressionStatement, child, "builtin:void");
        statement->children.push_back(std::move(add));
        block->children.push_back(std::move(statement));
    }

    auto toCollection = makeNode(
        ProjectOopHirNodeKind::InstanceCall, expression, core.resultTypeKey);
    toCollection->memberOrdinal = core.builderMemberOrdinals[2];
    auto receiver = makeNode(
        ProjectOopHirNodeKind::LocalAddress, expression, temporaryTypeKey + "*");
    receiver->localOrdinal = temporaryOrdinal;
    toCollection->children.push_back(std::move(receiver));
    block->children.push_back(std::move(toCollection));
    return block;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerArrayConstructionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    switch (expression->kind())
    {
        case ast::AstNodeKind::StackallocExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            if (core == nullptr || core->kind != MethodCoreOperationKind::StackAllocation)
            {
                error = "CC1 stackalloc reached OOP lowering without its resolved element type";
                return nullptr;
            }
            if (addressContext)
            {
                error = "CC1 stackalloc yields a pointer value, not addressable storage";
                return nullptr;
            }
            auto count = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (count == nullptr) return nullptr;

            // =================================================================================
            // ID 123 (3/3). QUE LA RESERVA QUEPA.
            //
            // Esta comprobacion vive AQUI y no en el modelo semantico porque necesita el TAMANO
            // DEL ELEMENTO, y aquel corre antes que ProjectObjectLayoutModel -- lo dice el
            // comentario de SizeofConstant en ProjectMethodBodyModel.h. El tamano se lo pregunta
            // al MISMO servicio del que salen `sizeof`, el stride de un arreglo y el offset de un
            // campo: ni una tabla de tamanos nueva.
            //
            // SOLO CUANDO EL CONTADOR ES CONSTANTE. Con un contador en tiempo de ejecucion no hay
            // nada que decidir en compilacion, y este lenguaje no promete comprobar la pila.
            //
            // NO SE MULTIPLICA NUNCA, y esa es la parte que importa. La forma obvia
            //
            //     if (elements * elementSize > kLimit)
            //
            // desborda en 64 bits justo en los casos que quiere atrapar: `stackalloc int[1<<62]`
            // da un producto que envuelve a 0 y pasa la comprobacion. Dividiendo el limite entre
            // el tamano, la comparacion es exacta para todo `elements` de 64 bits y el
            // desbordamiento del producto deja de existir por construccion, en vez de vigilarse
            // aparte. Por eso las dos ultimas comprobaciones del ID 123 son UNA sola.
            //
            // EL LIMITE, y de donde sale. Nada en el arbol le pasa `/STACK` a `lld-link`
            // (comprobado: cero apariciones en src/Linker/), asi que el PE se queda con la
            // reserva por omision del enlazador, que en Windows x64 es 1 MiB. Una sola reserva de
            // 1 MiB no puede caber ahi -- el marco de la funcion tambien ocupa -- asi que el
            // limite rechaza SOLO lo que falla seguro y no opina sobre lo que quiza quepa. Si
            // algun dia el enlazador recibe un `/STACK` mayor, este numero se mueve con el.
            constexpr std::uint64_t kMaximumStackallocBytes = std::uint64_t{1} << 20;

            if (count->kind == ProjectOopHirNodeKind::IntegerConstant)
            {
                if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                {
                    error = "CC1 stackalloc requires the object layout model to size it";
                    return nullptr;
                }
                const std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout =
                    objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, core->elementTypeKey);
                if (!elementLayout.has_value())
                {
                    error = "stackalloc cannot size '" + core->elementTypeKey +
                        "': it has no value layout";
                    return nullptr;
                }
                const std::uint64_t elementSize = elementLayout->first;
                const std::uint64_t elements = count->integerValue;
                if (elementSize != 0 && elements > kMaximumStackallocBytes / elementSize)
                {
                    error = "stackalloc of " + std::to_string(elements) + " x " +
                        std::to_string(elementSize) + " bytes does not fit on the stack; the "
                        "default reserve is " + std::to_string(kMaximumStackallocBytes) +
                        " bytes -- allocate it on the heap instead";
                    return nullptr;
                }
            }

            auto node = makeNode(ProjectOopHirNodeKind::StackAllocation, expression, core->resultTypeKey);
            node->elementTypeKey = core->elementTypeKey;
            node->children.push_back(std::move(count));
            return node;
        }

        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::CollectionExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            // CC1 fase 4C -- `new T` escalar. Hasta esta ronda el mensaje de abajo decia que el
            // raw new "remains outside this pipeline", y era cierto: el modelo semantico lo
            // aceptaba y lo tipaba `T*`, y aqui moria. Ahora trae binding y se baja.
            if (core != nullptr && core->kind == MethodCoreOperationKind::RawAllocation)
            {
                if (addressContext)
                {
                    error = "CC1 raw allocation yields a pointer value, not addressable storage";
                    return nullptr;
                }
                auto node = makeNode(ProjectOopHirNodeKind::RawAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                return node;
            }
            if (core != nullptr && core->kind == MethodCoreOperationKind::CollectionBuilder)
            {
                if (addressContext)
                {
                    error = "collection builder result is a value, not addressable storage";
                    return nullptr;
                }
                const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;
                if (!expression->children().empty())
                {
                    error = "Stage 34 collection builder escaped iterative lowering";
                    return nullptr;
                }
                return finishCollectionBuilderExpression(
                    expression, *core, tempOrdinal, {});
            }
            if (core == nullptr ||
                (core->kind != MethodCoreOperationKind::ArrayAllocation &&
                 core->kind != MethodCoreOperationKind::RangeArrayAllocation))
            {
                error = "Stage 41 OOP HIR only owns dynamic new T[count]; raw new remains outside this pipeline";
                return nullptr;
            }
            if (addressContext)
            {
                error = "Stage 41 array allocation result is a reference value, not addressable storage";
                return nullptr;
            }
            if (core->kind == MethodCoreOperationKind::RangeArrayAllocation)
            {
                error = expression->first() == nullptr && expression->children().empty()
                    ? "null expression reached Stage 34 OOP HIR"
                    : "Stage 34 range array construction escaped iterative lowering";
                return nullptr;
            }
            // NS304-T09-F05: preserve every materialized segment in source order. A parallel
            // kind vector makes spread position explicit through HIR and MIR.
            bool spreadSeen = false;
            for (const ast::AstNode* child : expression->children())
            {
                if (child != nullptr && child->kind() == ast::AstNodeKind::SpreadElementExpression)
                {
                    spreadSeen = true;
                    break;
                }
            }
            if (spreadSeen)
            {
                error = "Stage 34 segmented array construction escaped iterative lowering";
                return nullptr;
            }
            const bool inferredInitializer =
                expression->auxiliaryText() == "inferred-array-initializer";
            if (inferredInitializer)
            {
                if (!expression->children().empty())
                {
                    error = "Stage 34 inferred array construction escaped iterative lowering";
                    return nullptr;
                }
                auto count = makeNode(
                    ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
                count->integerValue = expression->children().size();
                auto node = makeNode(
                    ProjectOopHirNodeKind::ArrayAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->booleanValue = true;
                node->children.push_back(std::move(count));
                return node;
            }
            error = expression->first() == nullptr
                ? "Stage 34 OOP HIR requires an expression"
                : "Stage 34 array construction escaped iterative lowering";
            return nullptr;
        }
        default:
            error = "non-array construction reached HIR array lowering";
            return nullptr;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerObjectConstructionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    const bool placement =
        expression->kind() == ast::AstNodeKind::PlacementConstructionExpression;
    const ObjectConstructionBinding* binding = constructionBinding(expression);
    if (binding == nullptr ||
        (binding->resolution != ConstructionResolutionKind::ResolvedExplicit &&
         binding->resolution != ConstructionResolutionKind::ResolvedImplicitDefault))
    {
        error = "Stage 34 requires a resolved Stage 28 object construction";
        return nullptr;
    }
    const auto ordinal = static_cast<std::size_t>(
        binding - constructionModel_->constructions().data());
    // NS304-T13-F02: allocator selection desugars to `allocator.Allocate(size)` + placement
    // construction on the returned pointer (no direct LLVM call, which would cross code
    // units). The placement node is built below with a local `byte*` as storage.
    if (binding->allocatorSelection && binding->allocatorAllocateMemberOrdinal.has_value())
    {
        if (addressContext)
        {
            error = "allocator construction result is a value, not addressable storage";
            return nullptr;
        }
        std::size_t size = 0;
        if (objectLayoutModel_ != nullptr)
        {
            if (binding->targetIsStruct)
            {
                const auto* layout =
                    objectLayoutModel_->structLayoutForType(binding->targetTypeOrdinal);
                if (layout != nullptr) size = layout->size;
            }
            else
            {
                const auto* layout =
                    objectLayoutModel_->layoutForType(binding->targetTypeOrdinal);
                if (layout != nullptr) size = layout->size;
            }
        }
        // NS304-T13-F06: the allocated extent is the header plus Capacity*sizeof(T). The
        // flexible element size is 1 byte here because `T[*]` only admits trivial elements
        // whose identity size is consulted below via the field's element type.
        if (expression->auxiliaryText() == "trailing" && !expression->children().empty())
        {
            const MethodExpressionTypeBinding* capacityType =
                expressionType(expression->children().back());
            if (capacityType != nullptr && capacityType->integerConstant.has_value())
            {
                size += static_cast<std::size_t>(capacityType->integerConstant->value);
            }
        }
        const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;
        const std::size_t contextOrdinal = nextSyntheticLocalOrdinal_++;
        // NS304-T13-F08 `failure: result`: the block yields the closed Result<T,E>, not the
        // raw target type; the conditional below is the block's last expression.
        auto block = makeNode(
            ProjectOopHirNodeKind::BlockExpression, expression,
            binding->allocatorFailureProfile == ProjectAllocatorFailureProfile::Result
                ? binding->allocatorResultCarrierTypeKey
                : binding->targetTypeKey);

        // A struct allocator is stored by value, so `Allocate` needs its address; a class
        // allocator is a reference and needs the loaded value (the reference pointer).
        bool allocatorIsStruct = false;
        std::optional<std::size_t> allocatorTypeOrdinal;
        if (inheritanceModel_ != nullptr && nameLookup_ != nullptr)
        {
            allocatorTypeOrdinal =
                inheritanceModel_->typeOrdinalForTypeKey(binding->allocatorTypeKey);
            if (allocatorTypeOrdinal.has_value() &&
                *allocatorTypeOrdinal < nameLookup_->namedTypes().size())
            {
                allocatorIsStruct =
                    nameLookup_->namedTypes()[*allocatorTypeOrdinal].kind ==
                    NamedTypeDeclarationKind::Struct;
            }
        }
        auto receiver = lowerExpression(
            expression->first(), memberOrdinal, allocatorIsStruct, error);
        if (receiver == nullptr) return nullptr;
        const std::string allocatorReceiverTypeKey = receiver->typeKey;

        // NS304-T13-F07: store the allocator reference once so `Allocate` (now) and the
        // header provenance (construction, below) share the SAME evaluated value -- no
        // double evaluation of a non-local allocator expression.
        auto contextDecl = makeNode(
            ProjectOopHirNodeKind::LocalDeclaration, expression, allocatorReceiverTypeKey);
        contextDecl->localOrdinal = contextOrdinal;
        contextDecl->stringValue = "allocator.context";
        contextDecl->children.push_back(std::move(receiver));
        block->children.push_back(std::move(contextDecl));

        auto contextValue = makeNode(
            ProjectOopHirNodeKind::LocalValue, expression, allocatorReceiverTypeKey);
        contextValue->localOrdinal = contextOrdinal;

        auto allocate = makeNode(
            ProjectOopHirNodeKind::InstanceCall, expression, "builtin:byte*");
        allocate->memberOrdinal = *binding->allocatorAllocateMemberOrdinal;
        allocate->children.push_back(std::move(contextValue));
        auto sizeArg = makeNode(
            ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
        sizeArg->integerValue = size;
        allocate->children.push_back(std::move(sizeArg));
        auto decl = makeNode(
            ProjectOopHirNodeKind::LocalDeclaration, expression, "builtin:byte*");
        decl->localOrdinal = tempOrdinal;
        decl->stringValue = "allocator.bytes";
        decl->children.push_back(std::move(allocate));
        block->children.push_back(std::move(decl));

        // NS304-T13-F06: the trailing `trailing: capacity` child is the flexible-array
        // extent, not a constructor argument; the result is the header struct BY VALUE.
        const bool trailingConstruction = expression->auxiliaryText() == "trailing";
        auto construction = makeNode(
            ProjectOopHirNodeKind::PlacementConstruction, expression,
            (binding->targetIsStruct && !trailingConstruction)
                ? binding->targetTypeKey + "*"
                : binding->targetTypeKey);
        construction->constructionOrdinal = ordinal;
        construction->objectLifetimeOwnership = sema::OwnershipKind::Owned;
        // Allocator placement owns the returned block even though another object supplies
        // Allocate/Deallocate. Raw placement remains the separate borrowed-storage path.
        construction->storageOwnership = sema::OwnershipKind::Owned;
        construction->targetTypeOrdinal = binding->targetTypeOrdinal;
        construction->memberOrdinal = binding->selectedConstructorMemberOrdinal.has_value()
            ? binding->selectedConstructorMemberOrdinal
            : binding->implicitDefaultBaseConstructorMemberOrdinal;
        // NS304-T13-F07: allocator provenance for the header (context + generated thunk).
        construction->allocatorContextLocalOrdinal = contextOrdinal;
        construction->allocatorDeallocateMemberOrdinal =
            binding->allocatorDeallocateMemberOrdinal;
        construction->allocatorTypeOrdinal = allocatorTypeOrdinal;
        auto storage = makeNode(
            ProjectOopHirNodeKind::LocalValue, expression, "builtin:byte*");
        storage->localOrdinal = tempOrdinal;
        construction->children.push_back(std::move(storage));
        const std::size_t constructorArgumentCount =
            expression->children().size() - (trailingConstruction ? 1U : 0U);
        for (std::size_t index = 0; index < constructorArgumentCount; ++index)
        {
            auto argument = lowerExpression(
                expression->children()[index], memberOrdinal, false, error);
            if (argument == nullptr) return nullptr;
            if (index < binding->selectedConversions.size() &&
                binding->selectedConversions[index].kind != ProjectConversionKind::Identity)
            {
                argument = wrapImplicitConversion(
                    std::move(argument), binding->selectedConversions[index],
                    expression->children()[index]);
            }
            if (index >= binding->selectedArgumentParameterOrdinals.size())
            {
                error = "named construction argument has no selected parameter ordinal";
                return nullptr;
            }
            construction->children.push_back(std::move(argument));
            construction->argumentParameterOrdinals.push_back(
                binding->selectedArgumentParameterOrdinals[index]);
        }
        construction->argumentCount = construction->argumentParameterOrdinals.size();
        // NS304-T13-F08: explicit failure profile. `None` keeps the plain construction; the
        // other profiles wrap it in a null-check so an Allocate returning null takes the
        // failure branch and the constructor never runs.
        if (binding->allocatorFailureProfile == ProjectAllocatorFailureProfile::None)
        {
            block->children.push_back(std::move(construction));
            return block;
        }
        std::string resultType = construction->typeKey;
        auto bytesValue = makeNode(
            ProjectOopHirNodeKind::LocalValue, expression, "builtin:byte*");
        bytesValue->localOrdinal = tempOrdinal;
        auto nullBytes = makeNode(
            ProjectOopHirNodeKind::NullConstant, expression, "builtin:byte*");
        auto compare = makeNode(
            ProjectOopHirNodeKind::Binary, expression, "builtin:bool");
        compare->operation = lexer::TokenKind::BangEqual;
        compare->children.push_back(std::move(bytesValue));
        compare->children.push_back(std::move(nullBytes));

        std::unique_ptr<ProjectOopHirNode> whenFalse;
        if (binding->allocatorFailureProfile == ProjectAllocatorFailureProfile::Null)
        {
            whenFalse = makeNode(
                ProjectOopHirNodeKind::NullConstant, expression, resultType);
        }
        else if (binding->allocatorFailureProfile == ProjectAllocatorFailureProfile::Panic)
        {
            whenFalse = makeNode(
                ProjectOopHirNodeKind::Panic, expression, "builtin:never");
            whenFalse->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > 0; --index)
                whenFalse->cleanupRegionOrdinals.push_back(
                    activeHirCleanupRegions_[index - 1]);
        }
        else if (binding->allocatorFailureProfile == ProjectAllocatorFailureProfile::Throws)
        {
            constexpr std::string_view exceptionTypeKey =
                "type:NativeSharpCore::NativeSharp.Core.AllocationFailureException";
            const std::optional<std::size_t> exceptionType =
                inheritanceModel_ != nullptr
                    ? inheritanceModel_->typeOrdinalForTypeKey(std::string(exceptionTypeKey))
                    : std::nullopt;
            if (!exceptionType.has_value() || memberModel_ == nullptr)
            {
                error = "NS304-T13-F08 failure:throws cannot resolve AllocationFailureException";
                return nullptr;
            }
            std::optional<std::size_t> createMethod;
            for (const std::size_t memberOrdinal :
                 memberModel_->findMembers(*exceptionType, "Create"))
            {
                if (memberOrdinal >= memberModel_->members().size()) continue;
                if (!memberModel_->members()[memberOrdinal].isStatic) continue;
                createMethod = memberOrdinal;
                break;
            }
            if (!createMethod.has_value())
            {
                error = "NS304-T13-F08 failure:throws cannot resolve AllocationFailureException.Create()";
                return nullptr;
            }
            auto createCall = makeNode(
                ProjectOopHirNodeKind::StaticCall, expression, std::string(exceptionTypeKey));
            createCall->memberOrdinal = *createMethod;

            whenFalse = makeNode(
                ProjectOopHirNodeKind::Throw, expression, "builtin:never");
            whenFalse->exceptionTypeOrdinal = *exceptionType;
            whenFalse->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;
            const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
                ? 0 : activeHirExceptionHandlers_.back().retainedCleanupDepth;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > retainedDepth; --index)
                whenFalse->cleanupRegionOrdinals.push_back(
                    activeHirCleanupRegions_[index - 1]);
            if (!activeHirExceptionHandlers_.empty())
                whenFalse->exceptionTargetRegionOrdinal =
                    activeHirExceptionHandlers_.back().hirRegionOrdinal;
            whenFalse->children.push_back(std::move(createCall));
        }
        else if (binding->allocatorFailureProfile == ProjectAllocatorFailureProfile::Result)
        {
            if (!binding->allocatorResultOkFactoryMemberOrdinal.has_value() ||
                !binding->allocatorResultErrorFactoryMemberOrdinal.has_value() ||
                binding->allocatorResultCarrierTypeKey.empty())
            {
                error = "NS304-T13-F08 failure:result cannot resolve Result<T,E> Ok/Error factories";
                return nullptr;
            }
            resultType = binding->allocatorResultCarrierTypeKey;

            // Error(AllocationFailure.OutOfMemory)
            auto errorArg = makeNode(
                ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
            errorArg->integerValue = 0;
            auto errorCall = makeNode(
                ProjectOopHirNodeKind::StaticCall, expression,
                binding->allocatorResultCarrierTypeKey);
            errorCall->memberOrdinal = *binding->allocatorResultErrorFactoryMemberOrdinal;
            errorCall->children.push_back(std::move(errorArg));
            whenFalse = std::move(errorCall);

            // Ok(construction)
            auto okCall = makeNode(
                ProjectOopHirNodeKind::StaticCall, expression,
                binding->allocatorResultCarrierTypeKey);
            okCall->memberOrdinal = *binding->allocatorResultOkFactoryMemberOrdinal;
            okCall->children.push_back(std::move(construction));
            construction = std::move(okCall);
        }
        else
        {
            error = "NS304-T13-F08 allocator failure profile is not implemented yet";
            return nullptr;
        }

        auto conditional = makeNode(
            ProjectOopHirNodeKind::Conditional, expression, resultType);
        conditional->children.push_back(std::move(compare));
        conditional->children.push_back(std::move(construction));
        conditional->children.push_back(std::move(whenFalse));
        block->children.push_back(std::move(conditional));
        return block;
    }
    auto node = makeNode(
        placement
            ? ProjectOopHirNodeKind::PlacementConstruction
            : binding->targetIsStruct
            ? ProjectOopHirNodeKind::ValueConstruction
            : ProjectOopHirNodeKind::ObjectConstruction,
        expression,
        placement && binding->targetIsStruct
            ? binding->targetTypeKey + "*"
            : binding->targetTypeKey);
    node->constructionOrdinal = ordinal;
    if (placement)
    {
        node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
        node->storageOwnership = sema::OwnershipKind::Borrowed;
    }
    else if (!binding->targetIsStruct)
    {
        node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
        node->storageOwnership = sema::OwnershipKind::Owned;
    }
    node->targetTypeOrdinal = binding->targetTypeOrdinal;
    node->memberOrdinal = binding->selectedConstructorMemberOrdinal.has_value()
        ? binding->selectedConstructorMemberOrdinal
        : binding->implicitDefaultBaseConstructorMemberOrdinal;
    node->argumentCount = 0;
    bool hasOwnedTemporary = false;
    ProjectOopHirNode* expandedPack = nullptr;
    if (placement)
    {
        auto storage = lowerExpression(expression->first(), memberOrdinal, false, error);
        if (storage == nullptr) return nullptr;
        node->children.push_back(std::move(storage));
    }
    for (std::size_t index = 0; index < expression->children().size(); ++index)
    {
        auto argument = lowerExpression(expression->children()[index], memberOrdinal, false, error);
        if (argument == nullptr)
        {
            return nullptr;
        }
        if (index < binding->selectedConversions.size() &&
            binding->selectedConversions[index].kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), binding->selectedConversions[index], expression->children()[index]);
        }
        if (index >= binding->selectedArgumentParameterOrdinals.size())
        {
            error = "named construction argument has no selected parameter ordinal";
            return nullptr;
        }
        const std::size_t parameterIndex =
            binding->selectedArgumentParameterOrdinals[index];
        const bool packElement =
            index < binding->selectedParameterPackElements.size() &&
            binding->selectedParameterPackElements[index];
        if (packElement)
        {
            if (!binding->selectedConstructorMemberOrdinal.has_value() ||
                *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
            {
                error = "T01-F02 construction parameter pack has no constructor metadata";
                return nullptr;
            }
            const ClassMemberSymbol& constructor =
                memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
            if (parameterIndex >= constructor.parameters.size())
            {
                error = "T01-F02 construction parameter pack has no parameter";
                return nullptr;
            }
            const std::string& elementType =
                constructor.parameters[parameterIndex].parameterPackElementTypeKey;
            if (elementType.empty())
            {
                error = "T01-F02 construction parameter pack lost its element type";
                return nullptr;
            }
            if (expandedPack == nullptr)
            {
                auto pack = makeNode(
                    ProjectOopHirNodeKind::ParameterPack,
                    expression->children()[index],
                    constructor.parameters[parameterIndex].typeKey);
                pack->elementTypeKey = elementType;
                expandedPack = pack.get();
                node->children.push_back(std::move(pack));
                node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            expandedPack->children.push_back(std::move(argument));
            continue;
        }
        if (std::ranges::find(
                binding->selectedFullExpressionBorrowArgumentOrdinals,
                index) != binding->selectedFullExpressionBorrowArgumentOrdinals.end())
        {
            argument = wrapFreshOwnedTemporaryBorrow(
                std::move(argument), expression->children()[index], hasOwnedTemporary);
        }
        node->children.push_back(std::move(argument));
        node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    if (!binding->selectedDefaultArguments.empty())
    {
        if (!binding->selectedConstructorMemberOrdinal.has_value() ||
            *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
        {
            error = "NSC-LIMIT-020 selected construction has no callable metadata";
            return nullptr;
        }
        const ClassMemberSymbol& selectedConstructor =
            memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
        for (std::size_t index = 0; index < binding->selectedDefaultArguments.size(); ++index)
        {
            if (index >= binding->selectedDefaultArgumentParameterOrdinals.size())
            {
                error = "selected construction default has no parameter ordinal";
                return nullptr;
            }
            const std::size_t parameterIndex =
                binding->selectedDefaultArgumentParameterOrdinals[index];
            if (parameterIndex >= selectedConstructor.parameters.size())
            {
                error = "NSC-LIMIT-020 construction default has no parameter";
                return nullptr;
            }
            auto argument = lowerDefaultArgument(
                binding->selectedDefaultArguments[index],
                selectedConstructor.parameters[parameterIndex].typeKey,
                error);
            if (argument == nullptr) return nullptr;
            node->children.push_back(std::move(argument));
            node->argumentParameterOrdinals.push_back(parameterIndex);
        }
    }
    if (binding->selectedSynthesizesEmptyParameterPack)
    {
        if (!binding->selectedConstructorMemberOrdinal.has_value() ||
            *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size() ||
            !binding->selectedParameterPackOrdinal.has_value())
        {
            error = "T01-F02 empty construction parameter pack has no constructor metadata";
            return nullptr;
        }
        const ClassMemberSymbol& constructor =
            memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
        const std::size_t parameterIndex = *binding->selectedParameterPackOrdinal;
        if (parameterIndex >= constructor.parameters.size())
        {
            error = "T01-F02 empty construction parameter pack has no parameter";
            return nullptr;
        }
        const std::string& elementType =
            constructor.parameters[parameterIndex].parameterPackElementTypeKey;
        if (elementType.empty())
        {
            error = "T01-F02 empty construction parameter pack lost its element type";
            return nullptr;
        }
        auto pack = makeNode(
            ProjectOopHirNodeKind::ParameterPack,
            expression,
            constructor.parameters[parameterIndex].typeKey);
        pack->elementTypeKey = elementType;
        node->children.push_back(std::move(pack));
        node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    node->argumentCount = node->argumentParameterOrdinals.size();
    if (binding->selectedImplicitDefault)
    {
        for (const std::size_t typeOrdinal : implicitInitializerTypes(
                 binding->targetTypeOrdinal,
                 binding->implicitDefaultBaseConstructorMemberOrdinal))
        {
            std::vector<std::unique_ptr<ProjectOopHirNode>> initializers;
            appendInstanceFieldInitializers(
                typeOrdinal, initializers, error, memberOrdinal);
            if (!error.empty())
            {
                return nullptr;
            }
            for (auto& initializer : initializers)
            {
                node->children.push_back(std::move(initializer));
            }
        }
    }
    return wrapFullExpressionCleanup(std::move(node), expression, hasOwnedTemporary);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerAggregateOrLayoutExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error)
{
    switch (expression->kind())
    {
        case ast::AstNodeKind::TupleExpression:
        case ast::AstNodeKind::AggregateInitializerExpression:
        case ast::AstNodeKind::WithExpression:
        {
            const MethodExpressionTypeBinding* targetType = expressionType(expression);
            if (targetType == nullptr)
            {
                error = "aggregate/with initializer has no bound target type";
                return nullptr;
            }
            auto node = makeNode(
                ProjectOopHirNodeKind::AggregateInitializer, expression, targetType->typeKey);
            if (!appendAggregateElements(expression, targetType->typeKey, 0, memberOrdinal, node->children, error))
            {
                return nullptr;
            }
            return node;
        }

        // CC1 fase 4A -- `sizeof`. Las dos formas llegan aqui como una CONSTANTE y nada mas: MIR y
        // LLVM no necesitan enterarse de que en el fuente habia un `sizeof`.
        //
        // El operando de `sizeof(expression)` NO se baja. Es lo que hace que un efecto secundario
        // escrito dentro de el no llegue a ejecutarse: no hay nodo que lo represente. El modelo
        // semantico ya se encargo de que tampoco altere el estado de flujo.
        //
        // El tamano se lo pregunta a `valueLayoutForTypeKey`, que es el MISMO servicio del que
        // salen el stride de un arreglo y el offset de un campo. Ni una tabla de tamanos nueva:
        // esa es toda la diferencia entre cerrar el hueco y crear una segunda regla de tamano que
        // pueda divergir de la primera.
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        {
            const MethodCoreBinding* binding = coreBinding(expression);
            if (binding == nullptr ||
                (binding->kind != MethodCoreOperationKind::SizeofConstant &&
                 binding->kind != MethodCoreOperationKind::AlignofConstant))
            {
                error = "layout query reached OOP lowering without its resolved measured type";
                return nullptr;
            }
            if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
            {
                error = "layout query requires the object layout model";
                return nullptr;
            }
            const std::optional<std::pair<std::uint64_t, std::uint64_t>> layout =
                objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, binding->elementTypeKey);
            if (!layout.has_value())
            {
                error = (binding->kind == MethodCoreOperationKind::AlignofConstant ? "alignof" : "sizeof") +
                    std::string(" cannot measure '") + binding->elementTypeKey + "': it has no value layout";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nuint");
            node->integerValue = binding->kind == MethodCoreOperationKind::AlignofConstant
                ? layout->second
                : layout->first;
            return node;
        }

        case ast::AstNodeKind::OffsetofExpression:
        {
            const MethodCoreBinding* binding = coreBinding(expression);
            if (binding == nullptr || binding->kind != MethodCoreOperationKind::OffsetofConstant ||
                !binding->implicitFieldMemberOrdinal.has_value())
            {
                error = "offsetof reached OOP lowering without its resolved field";
                return nullptr;
            }
            if (objectLayoutModel_ == nullptr)
            {
                error = "offsetof requires the object layout model";
                return nullptr;
            }
            const ObjectFieldLayout* field = objectLayoutModel_->fieldLayoutForMember(
                *binding->implicitFieldMemberOrdinal);
            if (field == nullptr)
            {
                error = "offsetof cannot find the canonical layout for field '" +
                    expression->text() + "'";
                return nullptr;
            }
            if (field->bitWidth.has_value())
            {
                error = "offsetof cannot be applied to a bit-field";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nuint");
            node->integerValue = field->offset;
            return node;
        }
        default:
            error = "non-aggregate/layout expression reached HIR aggregate lowering";
            return nullptr;
    }
}
}
