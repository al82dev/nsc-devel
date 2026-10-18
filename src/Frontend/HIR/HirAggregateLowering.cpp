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
                error = "Stage 34 aggregate with source escaped iterative lowering";
                return false;
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
            (void)sourceSyntax;
            error = "Stage 34 aggregate spread source escaped iterative lowering";
            return false;
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
        (void)slotOffset;
        error = "Stage 34 aggregate value escaped iterative lowering";
        return false;
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

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishAllocatorConstructionExpression(
    const ast::AstNode* expression,
    const ObjectConstructionBinding& binding,
    std::size_t constructionOrdinal,
    std::size_t computedSize,
    std::size_t temporaryOrdinal,
    std::size_t contextOrdinal,
    std::optional<std::size_t> allocatorTypeOrdinal,
    std::unique_ptr<ProjectOopHirNode> receiver,
    std::vector<std::unique_ptr<ProjectOopHirNode>> arguments,
    std::vector<std::size_t> argumentParameterOrdinals,
    std::string& error)
{
    auto block = makeNode(
        ProjectOopHirNodeKind::BlockExpression,
        expression,
        binding.allocatorFailureProfile == ProjectAllocatorFailureProfile::Result
            ? binding.allocatorResultCarrierTypeKey
            : binding.targetTypeKey);

    const std::string allocatorReceiverTypeKey = receiver->typeKey;
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
    allocate->memberOrdinal = *binding.allocatorAllocateMemberOrdinal;
    allocate->children.push_back(std::move(contextValue));
    auto sizeArg = makeNode(
        ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
    sizeArg->integerValue = computedSize;
    allocate->children.push_back(std::move(sizeArg));
    auto allocationDecl = makeNode(
        ProjectOopHirNodeKind::LocalDeclaration, expression, "builtin:byte*");
    allocationDecl->localOrdinal = temporaryOrdinal;
    allocationDecl->stringValue = "allocator.bytes";
    allocationDecl->children.push_back(std::move(allocate));
    block->children.push_back(std::move(allocationDecl));

    const bool trailingConstruction = expression->auxiliaryText() == "trailing";
    auto construction = makeNode(
        ProjectOopHirNodeKind::PlacementConstruction,
        expression,
        (binding.targetIsStruct && !trailingConstruction)
            ? binding.targetTypeKey + "*"
            : binding.targetTypeKey);
    construction->constructionOrdinal = constructionOrdinal;
    construction->objectLifetimeOwnership = sema::OwnershipKind::Owned;
    construction->storageOwnership = sema::OwnershipKind::Owned;
    construction->targetTypeOrdinal = binding.targetTypeOrdinal;
    construction->memberOrdinal = binding.selectedConstructorMemberOrdinal.has_value()
        ? binding.selectedConstructorMemberOrdinal
        : binding.implicitDefaultBaseConstructorMemberOrdinal;
    construction->allocatorContextLocalOrdinal = contextOrdinal;
    construction->allocatorDeallocateMemberOrdinal =
        binding.allocatorDeallocateMemberOrdinal;
    construction->allocatorTypeOrdinal = allocatorTypeOrdinal;
    auto storage = makeNode(
        ProjectOopHirNodeKind::LocalValue, expression, "builtin:byte*");
    storage->localOrdinal = temporaryOrdinal;
    construction->children.push_back(std::move(storage));
    for (auto& argument : arguments)
        construction->children.push_back(std::move(argument));
    construction->argumentParameterOrdinals = std::move(argumentParameterOrdinals);
    construction->argumentCount = construction->argumentParameterOrdinals.size();

    if (binding.allocatorFailureProfile == ProjectAllocatorFailureProfile::None)
    {
        block->children.push_back(std::move(construction));
        return block;
    }

    std::string resultType = construction->typeKey;
    auto bytesValue = makeNode(
        ProjectOopHirNodeKind::LocalValue, expression, "builtin:byte*");
    bytesValue->localOrdinal = temporaryOrdinal;
    auto nullBytes = makeNode(
        ProjectOopHirNodeKind::NullConstant, expression, "builtin:byte*");
    auto compare = makeNode(
        ProjectOopHirNodeKind::Binary, expression, "builtin:bool");
    compare->operation = lexer::TokenKind::BangEqual;
    compare->children.push_back(std::move(bytesValue));
    compare->children.push_back(std::move(nullBytes));

    std::unique_ptr<ProjectOopHirNode> whenFalse;
    if (binding.allocatorFailureProfile == ProjectAllocatorFailureProfile::Null)
    {
        whenFalse = makeNode(
            ProjectOopHirNodeKind::NullConstant, expression, resultType);
    }
    else if (binding.allocatorFailureProfile == ProjectAllocatorFailureProfile::Panic)
    {
        whenFalse = makeNode(
            ProjectOopHirNodeKind::Panic, expression, "builtin:never");
        whenFalse->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;
        for (std::size_t index = activeHirCleanupRegions_.size(); index > 0; --index)
            whenFalse->cleanupRegionOrdinals.push_back(
                activeHirCleanupRegions_[index - 1U]);
    }
    else if (binding.allocatorFailureProfile == ProjectAllocatorFailureProfile::Throws)
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
            ? 0U : activeHirExceptionHandlers_.back().retainedCleanupDepth;
        for (std::size_t index = activeHirCleanupRegions_.size();
             index > retainedDepth; --index)
        {
            whenFalse->cleanupRegionOrdinals.push_back(
                activeHirCleanupRegions_[index - 1U]);
        }
        if (!activeHirExceptionHandlers_.empty())
            whenFalse->exceptionTargetRegionOrdinal =
                activeHirExceptionHandlers_.back().hirRegionOrdinal;
        whenFalse->children.push_back(std::move(createCall));
    }
    else if (binding.allocatorFailureProfile == ProjectAllocatorFailureProfile::Result)
    {
        if (!binding.allocatorResultOkFactoryMemberOrdinal.has_value() ||
            !binding.allocatorResultErrorFactoryMemberOrdinal.has_value() ||
            binding.allocatorResultCarrierTypeKey.empty())
        {
            error = "NS304-T13-F08 failure:result cannot resolve Result<T,E> Ok/Error factories";
            return nullptr;
        }
        resultType = binding.allocatorResultCarrierTypeKey;

        auto errorArg = makeNode(
            ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
        errorArg->integerValue = 0U;
        auto errorCall = makeNode(
            ProjectOopHirNodeKind::StaticCall,
            expression,
            binding.allocatorResultCarrierTypeKey);
        errorCall->memberOrdinal = *binding.allocatorResultErrorFactoryMemberOrdinal;
        errorCall->children.push_back(std::move(errorArg));
        whenFalse = std::move(errorCall);

        auto okCall = makeNode(
            ProjectOopHirNodeKind::StaticCall,
            expression,
            binding.allocatorResultCarrierTypeKey);
        okCall->memberOrdinal = *binding.allocatorResultOkFactoryMemberOrdinal;
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

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerObjectConstructionExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    (void)memberOrdinal;
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
    if (binding->allocatorSelection && binding->allocatorAllocateMemberOrdinal.has_value())
    {
        if (addressContext)
        {
            error = "allocator construction result is a value, not addressable storage";
            return nullptr;
        }
        error = "Stage 34 allocator construction escaped iterative lowering";
        return nullptr;
    }
    (void)ordinal;
    error = placement
        ? "Stage 34 ordinary placement construction escaped iterative lowering"
        : "Stage 34 ordinary construction escaped iterative lowering";
    return nullptr;
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
            (void)memberOrdinal;
            error = "Stage 34 aggregate/tuple/with escaped iterative lowering";
            return nullptr;
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
