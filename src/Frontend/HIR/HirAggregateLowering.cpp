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
                const std::string tempTypeKey = core->resultTypeKey;
                const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;

                auto block = makeNode(
                    ProjectOopHirNodeKind::BlockExpression, expression, core->resultTypeKey);

                // CreateBuilder() static call initializes the compiler-generated temporary.
                auto create = makeNode(ProjectOopHirNodeKind::StaticCall, expression, tempTypeKey);
                create->memberOrdinal = core->builderMemberOrdinals[0];
                auto declaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration, expression, tempTypeKey);
                declaration->localOrdinal = tempOrdinal;
                declaration->stringValue = "builder";
                declaration->children.push_back(std::move(create));
                block->children.push_back(std::move(declaration));

                // Add(element) instance calls over the temporary, preserving source order.
                for (const ast::AstNode* child : expression->children())
                {
                    auto element = lowerExpression(child, memberOrdinal, false, error);
                    if (element == nullptr) return nullptr;
                    auto add = makeNode(ProjectOopHirNodeKind::InstanceCall, child, "builtin:void");
                    add->memberOrdinal = core->builderMemberOrdinals[1];
                    auto addReceiver = makeNode(
                        ProjectOopHirNodeKind::LocalAddress, expression, tempTypeKey + "*");
                    addReceiver->localOrdinal = tempOrdinal;
                    add->children.push_back(std::move(addReceiver));
                    add->children.push_back(std::move(element));
                    auto statement = makeNode(
                        ProjectOopHirNodeKind::ExpressionStatement, child, "builtin:void");
                    statement->children.push_back(std::move(add));
                    block->children.push_back(std::move(statement));
                }

                // ToCollection() instance call yields the result value.
                auto toCollection = makeNode(
                    ProjectOopHirNodeKind::InstanceCall, expression, core->resultTypeKey);
                toCollection->memberOrdinal = core->builderMemberOrdinals[2];
                auto toCollectionReceiver = makeNode(
                    ProjectOopHirNodeKind::LocalAddress, expression, tempTypeKey + "*");
                toCollectionReceiver->localOrdinal = tempOrdinal;
                toCollection->children.push_back(std::move(toCollectionReceiver));
                block->children.push_back(std::move(toCollection));

                return block;
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
                auto node = makeNode(
                    ProjectOopHirNodeKind::RangeArrayAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->nativeArrayRank = ProjectTypeConversion::nativeArrayRank(core->resultTypeKey);
                std::vector<const ast::AstNode*> dimensions;
                dimensions.push_back(expression->first());
                dimensions.insert(dimensions.end(), expression->children().begin(),
                    expression->children().end());
                for (const ast::AstNode* dimension : dimensions)
                {
                    auto extent = lowerExpression(dimension, memberOrdinal, false, error);
                    if (extent == nullptr) return nullptr;
                    node->children.push_back(std::move(extent));
                }
                return node;
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
                auto node = makeNode(
                    ProjectOopHirNodeKind::ArrayAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                for (const ast::AstNode* initializer : expression->children())
                {
                    if (initializer != nullptr &&
                        initializer->kind() == ast::AstNodeKind::SpreadElementExpression)
                    {
                        auto source = lowerExpression(
                            initializer->first(), memberOrdinal, false, error);
                        if (source == nullptr) return nullptr;
                        node->children.push_back(std::move(source));
                        node->collectionSegments.push_back(ProjectCollectionSegmentKind::Spread);
                        continue;
                    }
                    auto value = lowerExpression(initializer, memberOrdinal, false, error);
                    if (value == nullptr) return nullptr;
                    const MethodExpressionTypeBinding* valueType = expressionType(initializer);
                    if (valueType == nullptr)
                    {
                        error = "NSC-LIMIT-017 array spread element has no semantic type";
                        return nullptr;
                    }
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        valueType->typeKey, core->elementTypeKey, valueType->integerConstant);
                    if (!conversion.exists())
                    {
                        error = "NSC-LIMIT-017 array spread element lost its validated conversion";
                        return nullptr;
                    }
                    if (conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(std::move(value), conversion, initializer);
                    }
                    node->children.push_back(std::move(value));
                    node->collectionSegments.push_back(ProjectCollectionSegmentKind::Element);
                }
                return node;
            }
            const bool inferredInitializer =
                expression->auxiliaryText() == "inferred-array-initializer";
            std::unique_ptr<ProjectOopHirNode> count;
            if (inferredInitializer)
            {
                count = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
                count->integerValue = expression->children().size();
            }
            else
            {
                count = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (count == nullptr) return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::ArrayAllocation, expression, core->resultTypeKey);
            node->elementTypeKey = core->elementTypeKey;
            node->booleanValue = inferredInitializer;
            node->children.push_back(std::move(count));
            for (const ast::AstNode* initializer : expression->children())
            {
                auto value = lowerExpression(initializer, memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                const MethodExpressionTypeBinding* valueType = expressionType(initializer);
                if (valueType == nullptr)
                {
                    error = "NSC-LIMIT-017 array initializer element has no semantic type";
                    return nullptr;
                }
                const ProjectImplicitConversion conversion = classifyImplicit(
                    valueType->typeKey, core->elementTypeKey, valueType->integerConstant);
                if (!conversion.exists())
                {
                    error = "NSC-LIMIT-017 array initializer lost its validated conversion";
                    return nullptr;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(std::move(value), conversion, initializer);
                }
                node->children.push_back(std::move(value));
            }
            return node;
        }
        default:
            error = "non-array construction reached HIR array lowering";
            return nullptr;
    }
}
}
