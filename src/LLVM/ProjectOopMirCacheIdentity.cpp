#include "nativesharp/LLVM/ProjectOopMirCacheIdentity.h"

#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <algorithm>
#include <bit>
#include <sstream>
#include <type_traits>

namespace nativesharp::llvm_backend
{
namespace
{
class IdentityWriter final
{
public:
    template <typename Value>
    void scalar(Value value)
    {
        if constexpr (std::is_enum_v<Value>)
            output_ << static_cast<std::underlying_type_t<Value>>(value);
        else
            output_ << value;
        output_ << ';';
    }

    void text(std::string_view value)
    {
        output_ << value.size() << ':';
        output_.write(value.data(), static_cast<std::streamsize>(value.size()));
        output_ << ';';
    }

    template <typename Value>
    void optional(const std::optional<Value>& value)
    {
        scalar(value.has_value());
        if (value.has_value()) scalar(*value);
    }

    template <typename Value>
    void scalars(const std::vector<Value>& values)
    {
        scalar(values.size());
        for (const Value& value : values) scalar(value);
    }

    void texts(const std::vector<std::string>& values)
    {
        scalar(values.size());
        for (const std::string& value : values) text(value);
    }

    std::string take() { return std::move(output_).str(); }

private:
    std::ostringstream output_;
};

void writeNode(IdentityWriter& output, const frontend::ProjectOopMirNode* node)
{
    output.scalar(node != nullptr);
    if (node == nullptr) return;
    output.scalar(node->kind);
    output.text(node->typeKey);
    output.text(node->elementTypeKey);
    output.text(node->stringValue);
    output.text(node->debugDisplayName);
    output.scalar(node->operation);
    output.optional(node->memberOrdinal);
    output.optional(node->namespaceFunctionOrdinal);
    output.optional(node->secondaryMemberOrdinal);
    output.optional(node->resultSuccessTestMemberOrdinal);
    output.optional(node->resultSuccessExtractorMemberOrdinal);
    output.optional(node->resultErrorExtractorMemberOrdinal);
    output.optional(node->resultErrorFactoryMemberOrdinal);
    output.optional(node->resultErrorConversionMemberOrdinal);
    output.optional(node->localOrdinal);
    output.optional(node->parameterOrdinal);
    output.optional(node->constructionOrdinal);
    output.optional(node->targetTypeOrdinal);
    output.optional(node->exceptionTypeOrdinal);
    output.optional(node->cleanupRegionOrdinal);
    output.optional(node->mirRegionOrdinal);
    output.optional(node->exceptionTargetRegionOrdinal);
    output.optional(node->targetScopeOrdinal);
    output.optional(node->disposeContractMemberOrdinal);
    output.optional(node->disposeImplementationMemberOrdinal);
    output.scalars(node->cleanupRegionOrdinals);
    output.optional(node->cleanupContinuationKind);
    output.optional(node->borrowKind);
    output.optional(node->localBorrowSourceKind);
    output.scalar(node->captureMode);
    output.optional(node->virtualSlot);
    output.optional(node->interfaceSlot);
    output.optional(node->conversionKind);
    output.scalars(node->switchLabels);
    output.scalar(node->switchHasDefault);
    output.scalar(node->immediate);
    output.scalar(node->secondaryImmediate);
    output.scalar(node->resultSuccessTag);
    output.scalar(node->resultErrorTag);
    output.scalar(node->resultReturnErrorTag);
    output.text(node->resultSourceCarrierTypeKey);
    output.text(node->resultSourceErrorTypeKey);
    output.text(node->resultReturnCarrierTypeKey);
    output.text(node->resultReturnErrorTypeKey);
    output.scalar(node->alignment);
    output.optional(node->bitOffset);
    output.optional(node->bitWidth);
    output.scalar(node->bitStorageSize);
    output.scalar(node->integerValue);
    output.scalar(std::bit_cast<std::uint64_t>(node->floatingValue));
    output.scalar(node->booleanValue);
    output.scalar(node->referenceLocal);
    output.scalar(node->pinnedStorage);
    output.scalar(node->referenceResult);
    output.scalar(node->ownership);
    output.scalar(node->automaticDestruction);
    output.scalar(node->variableLengthArray);
    output.scalar(node->variableLengthArrayRank);
    output.scalar(node->nativeArrayRank);
    output.scalar(node->placementConstructed);
    output.scalar(node->stackPromotedObject);
    output.scalar(node->volatileAccess);
    output.scalar(node->allocationFailureReturnsNull);
    output.scalar(node->zeroInitialize);
    output.scalar(node->baseReceiver);
    output.scalar(node->targetIsInterface);
    output.scalar(node->dynamicDestruction);
    output.scalar(node->valueTypeNoOpDeletion);
    output.scalar(node->objectLifetimeOwnership);
    output.scalar(node->storageOwnership);
    output.scalar(node->catchAll);
    output.scalar(node->catchHasFilter);
    output.scalar(node->catchExhaustive);
    output.texts(node->catchMatchedErrorTypeKeys);
    output.scalar(node->interfaceCleanupDispatch);
    output.scalar(node->inlineAssemblyHasSideEffects);
    output.scalar(node->inlineAssemblyClobbersMemory);
    output.scalar(node->inlineAssemblyClobbersFlags);
    output.scalar(node->argumentCount);
    output.scalars(node->argumentParameterOrdinals);
    output.text(node->calleeName);
    output.text(node->secondaryCalleeName);
    output.text(node->resultSuccessTestCalleeName);
    output.text(node->resultSuccessExtractorCalleeName);
    output.text(node->resultErrorExtractorCalleeName);
    output.text(node->resultErrorFactoryCalleeName);
    output.text(node->resultErrorConversionCalleeName);
    output.texts(node->argumentTypeKeys);
    output.scalar(node->children.size());
    for (const auto& child : node->children) writeNode(output, child.get());
}

void writeFieldLayout(IdentityWriter& output, const frontend::ObjectFieldLayout& field)
{
    output.scalar(field.memberOrdinal);
    output.scalar(field.offset);
    output.scalar(field.size);
    output.scalar(field.alignment);
    output.text(field.typeKey);
    output.optional(field.bitOffset);
    output.optional(field.bitWidth);
}

void writeFunction(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function,
    bool definition)
{
    output.scalar(function.memberOrdinal);
    output.optional(function.namespaceFunctionOrdinal);
    output.scalar(function.declaringTypeOrdinal);
    output.scalar(function.constructor);
    output.scalar(function.destructor);
    output.scalar(function.isStatic);
    output.scalar(function.referenceReturn);
    output.scalar(function.isNoHeap);
    output.scalar(function.effects.bits());
    output.scalar(function.inliningPolicy);
    output.scalar(function.isExportedC);
    output.scalar(function.isImportedC);
    output.scalar(function.isVariadic);
    output.scalar(function.isImportedNative);
    output.scalar(function.isSyntheticImplicitConstructor);
    output.text(function.importDllName);
    output.scalar(function.importRequiresRuntimeBinding);
    output.text(function.cAbiName);
    output.scalar(function.isLambda);
    output.optional(function.lambdaOrdinal);
    output.scalar(function.hasClosureEnvironment);
    output.texts(function.captureTypeKeys);
    output.texts(function.captureNames);
    output.scalars(function.captureSourceKinds);
    output.scalars(function.captureModes);
    output.scalars(function.captureOwnsValues);
    output.scalar(function.callableTrait);
    output.scalar(function.closureLayoutSize);
    output.scalar(function.closureLayoutAlignment);
    output.scalars(function.capturedLocalOrdinals);
    output.scalars(function.capturedParameterOrdinals);
    output.scalar(function.capturesThisCell);
    output.text(function.genericSpecializationKey);
    output.text(function.mangledName);
    output.text(function.debugDisplayName);
    output.text(function.returnTypeKey);
    output.text(function.errorTypeKey);
    output.scalar(function.returnOwnership);
    output.texts(function.parameterNames);
    output.texts(function.parameterTypeKeys);
    output.scalars(function.parameterPassKinds);
    output.scalars(function.parameterOwnerships);
    output.scalar(function.abi.returnPassing);
    output.optional(function.abi.hiddenReturnParameterOrdinal);
    output.scalars(function.abi.parameterPassing);
    output.scalars(function.abi.parameterPhysicalOrdinals);
    output.scalar(function.abi.physicalParameterCount);
    output.scalar(definition);
    if (!definition) return;
    output.scalars(function.localOrdinals);
    output.scalar(function.cleanupRegions.size());
    for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
    {
        output.scalar(cleanup.ordinal);
        output.scalar(cleanup.kind);
        output.optional(cleanup.localOrdinal);
        output.optional(cleanup.parameterOrdinal);
        output.text(cleanup.typeKey);
        output.scalar(cleanup.ownership);
        output.scalar(cleanup.automaticDestruction);
        writeNode(output, cleanup.action);
    }
    output.scalar(function.exceptionFlow.requiresPersonality);
    output.scalars(function.exceptionFlow.protectedRegionOrdinals);
    output.scalar(function.exceptionFlow.catchRegionCount);
    output.scalar(function.exceptionFlow.finallyRegionCount);
    output.scalar(function.exceptionFlow.nonlocalExits.size());
    for (const frontend::ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
    {
        output.scalar(exit.kind);
        output.scalars(exit.cleanupRegionOrdinals);
        output.optional(exit.targetScopeOrdinal);
        output.optional(exit.exceptionTargetRegionOrdinal);
        output.optional(exit.cleanupContinuationKind);
        writeNode(output, exit.node);
    }
    writeNode(output, function.body.get());
}

void writeCommonModel(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
    output.scalar(frontend::kProjectOopMirSchemaMajor);
    output.scalar(frontend::kProjectOopMirSchemaMinor);
    output.scalar(model.structLayouts().size());
    for (const auto& layout : model.structLayouts())
    {
        output.scalar(layout.typeOrdinal); output.text(layout.typeKey);
        output.scalar(layout.size); output.scalar(layout.alignment);
        output.scalar(layout.isUnion); output.scalar(layout.pack);
        output.scalar(layout.minimumAlignment); output.scalar(layout.hasExplicitFieldOffsets);
        output.scalar(layout.abiPassing); output.scalar(layout.isImported);
        output.scalar(layout.requiresDrop); output.text(layout.dropGlueEntryName);
        output.scalar(layout.fields.size());
        for (const auto& field : layout.fields) writeFieldLayout(output, field);
    }
    output.scalar(model.classLayouts().size());
    for (const auto& layout : model.classLayouts())
    {
        output.scalar(layout.typeOrdinal); output.text(layout.typeKey);
        output.scalar(layout.size); output.scalar(layout.alignment); output.scalar(layout.headerSize);
        output.optional(layout.baseTypeOrdinal); output.scalar(layout.baseSubobjectSize);
        output.scalar(layout.referenceSize); output.scalar(layout.referenceAlignment);
        output.scalar(layout.fields.size());
        for (const auto& field : layout.fields) writeFieldLayout(output, field);
    }
    output.scalar(model.staticFields().size());
    for (const auto& field : model.staticFields())
    {
        output.scalar(field.memberOrdinal); output.scalar(field.declaringTypeOrdinal);
        output.text(field.name); output.text(field.typeKey); output.scalar(field.isReadonly);
        output.scalar(field.isVolatile); output.scalar(field.isConst); output.scalar(field.isImported);
        output.text(field.genericSpecializationKey); output.text(field.storageSymbol);
        output.text(field.accessorName); output.scalar(field.hasConstantInitializer);
        output.scalar(field.constantIntegerValue);
        output.scalar(std::bit_cast<std::uint64_t>(field.constantFloatingValue));
        output.scalar(field.constantBooleanValue); output.text(field.constantStringValue);
        output.scalar(field.isEnumConstant);
        output.optional(field.namespaceStateOrdinal);
        output.text(field.debugDisplayName);
    }
    output.scalar(model.dispatchTables().size());
    for (const auto& table : model.dispatchTables())
    {
        output.scalar(table.isImported); output.text(table.genericSpecializationKey);
        output.scalar(table.typeOrdinal); output.optional(table.baseTypeOrdinal);
        output.text(table.typeKey); output.text(table.descriptorName);
        output.text(table.descriptorAccessorName); output.text(table.vtableName);
        output.scalar(table.stableTypeId); output.text(table.typeDisplayName);
        output.text(table.typeNameGlobalName); output.scalars(table.slotMemberOrdinals);
        output.texts(table.slotCalleeNames); output.text(table.destructionEntryName);
        output.text(table.objectDestructionEntryName); output.scalars(table.destructorMemberOrdinals);
        output.texts(table.destructorCalleeNames);
        output.optional(table.delegatedBaseDestructionTypeOrdinal);
        output.scalar(table.ownedFields.size());
        for (const auto& field : table.ownedFields)
        { output.scalar(field.memberOrdinal); output.scalar(field.offset); output.scalar(field.isNativeString); }
        output.scalar(table.destructionSteps.size());
        for (const auto& step : table.destructionSteps)
        {
            output.text(step.destructorCalleeName); output.scalar(step.ownedField.memberOrdinal);
            output.scalar(step.ownedField.offset); output.scalar(step.ownedField.isNativeString);
            output.scalar(step.isFieldRelease);
        }
        output.text(table.interfaceMapName); output.scalar(table.interfaceTables.size());
        for (const auto& interfaceTable : table.interfaceTables)
        {
            output.scalar(interfaceTable.interfaceTypeOrdinal);
            output.text(interfaceTable.interfaceTypeKey);
            output.scalar(interfaceTable.stableInterfaceTypeId);
            output.text(interfaceTable.methodTableName);
            output.scalars(interfaceTable.interfaceMemberOrdinals);
            output.scalars(interfaceTable.implementationMemberOrdinals);
            output.texts(interfaceTable.calleeNames);
        }
    }
    output.scalar(model.interfaceBindings().size());
    for (const auto& binding : model.interfaceBindings())
    {
        output.scalar(binding.implementingTypeOrdinal); output.scalar(binding.interfaceTypeOrdinal);
        output.text(binding.interfaceTypeKey); output.scalar(binding.stableInterfaceTypeId);
        output.text(binding.methodTableName); output.scalars(binding.interfaceMemberOrdinals);
        output.scalars(binding.implementationMemberOrdinals); output.texts(binding.calleeNames);
    }
    output.scalar(model.genericSpecializations().size());
    for (const auto& specialization : model.genericSpecializations())
    {
        output.text(specialization.key); output.scalars(specialization.functionIndices);
        output.scalars(specialization.staticFieldIndices);
        output.scalars(specialization.dispatchTableIndices);
    }
    output.scalar(model.fieldInitializers().size());
    for (const auto& initializer : model.fieldInitializers())
    {
        output.scalar(initializer.memberOrdinal); output.scalar(initializer.declaringTypeOrdinal);
        output.scalar(initializer.isStatic); output.scalar(initializer.isConst);
        writeNode(output, initializer.value.get());
    }
}
}

std::string projectOopMirCacheIdentity(
    const frontend::ProjectOopMirModel& model,
    std::span<const std::size_t> definitionOrdinals,
    std::span<const std::size_t> declarationOrdinals)
{
    IdentityWriter output;
    output.text("NativeSharp.ProjectOopMirCacheIdentity.v3");
    output.text(language::languageEditionFingerprint(model.languageEdition()));
    writeCommonModel(output, model);
    output.scalar(declarationOrdinals.size());
    for (const std::size_t ordinal : declarationOrdinals)
    {
        output.scalar(ordinal);
        if (ordinal >= model.functions().size()) continue;
        const bool definition = std::binary_search(
            definitionOrdinals.begin(), definitionOrdinals.end(), ordinal);
        writeFunction(output, model.functions()[ordinal], definition);
    }
    return output.take();
}
}
