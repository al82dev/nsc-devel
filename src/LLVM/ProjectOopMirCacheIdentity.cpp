#include "nativesharp/LLVM/ProjectOopMirCacheIdentity.h"

#include "Internal/IdentityWriter.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <bit>
#include <unordered_set>

namespace nativesharp::llvm_backend
{
namespace
{
void writeNodeIdentity(IdentityWriter& output, const frontend::ProjectOopMirNode& node)
{
    output.scalar(node.kind);
    output.text(node.typeKey);
    output.text(node.elementTypeKey);
    output.text(node.stringValue);
    output.text(node.debugDisplayName);
    output.scalar(node.operation);
}

void writeNodeOrdinals(IdentityWriter& output, const frontend::ProjectOopMirNode& node)
{
    output.optional(node.memberOrdinal);
    output.optional(node.namespaceFunctionOrdinal);
    output.optional(node.secondaryMemberOrdinal);
    output.optional(node.resultSuccessTestMemberOrdinal);
    output.optional(node.resultSuccessExtractorMemberOrdinal);
    output.optional(node.resultErrorExtractorMemberOrdinal);
    output.optional(node.resultErrorFactoryMemberOrdinal);
    output.optional(node.resultErrorConversionMemberOrdinal);
    output.optional(node.localOrdinal);
    output.optional(node.parameterOrdinal);
    output.optional(node.constructionOrdinal);
    output.optional(node.targetTypeOrdinal);
    output.optional(node.exceptionTypeOrdinal);
    output.optional(node.cleanupRegionOrdinal);
    output.optional(node.mirRegionOrdinal);
    output.optional(node.exceptionTargetRegionOrdinal);
    output.optional(node.targetScopeOrdinal);
    output.optional(node.disposeContractMemberOrdinal);
    output.optional(node.disposeImplementationMemberOrdinal);
    output.scalars(node.cleanupRegionOrdinals);
    output.optional(node.cleanupContinuationKind);
    output.optional(node.borrowKind);
    output.optional(node.localBorrowSourceKind);
}

void writeNodeValues(IdentityWriter& output, const frontend::ProjectOopMirNode& node)
{
    output.scalar(node.captureMode);
    output.optional(node.virtualSlot);
    output.optional(node.interfaceSlot);
    output.optional(node.conversionKind);
    output.scalars(node.switchLabels);
    output.scalar(node.switchHasDefault);
    output.scalar(node.immediate);
    output.scalar(node.secondaryImmediate);
    output.scalar(node.resultSuccessTag);
    output.scalar(node.resultErrorTag);
    output.scalar(node.resultReturnErrorTag);
    output.text(node.resultSourceCarrierTypeKey);
    output.text(node.resultSourceErrorTypeKey);
    output.text(node.resultReturnCarrierTypeKey);
    output.text(node.resultReturnErrorTypeKey);
    output.scalar(node.alignment);
    output.optional(node.bitOffset);
    output.optional(node.bitWidth);
    output.optional(node.replaySpillOffset);
    output.scalar(node.bitStorageSize);
    output.scalar(node.integerValue);
    output.scalar(std::bit_cast<std::uint64_t>(node.floatingValue));
    output.scalar(node.booleanValue);
    output.scalar(node.returnsAssignedValue);
    output.scalars(node.collectionSegments);
}

void writeNodeFlags(IdentityWriter& output, const frontend::ProjectOopMirNode& node)
{
    output.scalar(node.referenceLocal);
    output.scalar(node.referenceMutable);
    output.scalar(node.staticLocal);
    output.scalar(node.pinnedStorage);
    output.scalar(node.referenceResult);
    output.scalar(node.governingReference);
    output.scalar(node.mustTailCall);
    output.scalar(node.ownership);
    output.scalar(node.automaticDestruction);
    output.scalar(node.variableLengthArray);
    output.scalar(node.variableLengthArrayRank);
    output.scalar(node.nativeArrayRank);
    output.scalar(node.placementConstructed);
    output.scalar(node.stackPromotedObject);
    output.scalar(node.volatileAccess);
    output.scalar(node.allocationFailureReturnsNull);
    output.scalar(node.zeroInitialize);
    output.scalar(node.baseReceiver);
    output.scalar(node.targetIsInterface);
    output.scalar(node.dynamicDestruction);
    output.scalar(node.valueTypeNoOpDeletion);
    output.scalar(node.objectLifetimeOwnership);
    output.scalar(node.storageOwnership);
    output.scalar(node.catchAll);
    output.scalar(node.catchHasFilter);
    output.scalar(node.catchExhaustive);
    output.texts(node.catchMatchedErrorTypeKeys);
    output.scalar(node.interfaceCleanupDispatch);
    output.scalar(node.inlineAssemblyHasSideEffects);
    output.scalar(node.inlineAssemblyClobbersMemory);
    output.scalar(node.inlineAssemblyClobbersFlags);
    output.scalar(node.argumentCount);
}

void writeNodeCall(IdentityWriter& output, const frontend::ProjectOopMirNode& node)
{
    output.scalars(node.argumentParameterOrdinals);
    output.text(node.calleeName);
    output.text(node.secondaryCalleeName);
    output.text(node.resultSuccessTestCalleeName);
    output.text(node.resultSuccessExtractorCalleeName);
    output.text(node.resultErrorExtractorCalleeName);
    output.text(node.resultErrorFactoryCalleeName);
    output.text(node.resultErrorConversionCalleeName);
    output.texts(node.argumentTypeKeys);
    output.scalar(node.children.size());
}

void writeNode(IdentityWriter& output, const frontend::ProjectOopMirNode* node)
{
    std::vector<const frontend::ProjectOopMirNode*> work;
    work.push_back(node);
    while (!work.empty())
    {
        node = work.back();
        work.pop_back();
        output.scalar(node != nullptr);
        if (node == nullptr) continue;
        writeNodeIdentity(output, *node);
        writeNodeOrdinals(output, *node);
        writeNodeValues(output, *node);
        writeNodeFlags(output, *node);
        writeNodeCall(output, *node);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
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

void writeFunctionIdentity(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function)
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
    output.scalar(function.isTransportedInlineBody);
    output.scalar(function.isSyntheticImplicitConstructor);
    output.text(function.importDllName);
    output.scalar(function.importRequiresRuntimeBinding);
    output.text(function.cAbiName);
}

void writeFunctionClosure(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function)
{
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
}

void writeFunctionSignature(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function)
{
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
}

void writeFunctionCleanupRegions(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function)
{
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
}

void writeFunctionExceptionFlow(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function)
{
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
}

void writeFunction(
    IdentityWriter& output,
    const frontend::ProjectOopMirFunction& function,
    bool definition)
{
    writeFunctionIdentity(output, function);
    writeFunctionClosure(output, function);
    writeFunctionSignature(output, function);
    output.scalar(definition);
    if (!definition) return;
    output.scalars(function.localOrdinals);
    writeFunctionCleanupRegions(output, function);
    writeFunctionExceptionFlow(output, function);
    writeNode(output, function.body.get());
}

void writeStructLayouts(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
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
}

void writeClassLayouts(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
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
}

void writeStaticFields(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
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
}

void writeDispatchTables(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
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
        {
            output.scalar(field.memberOrdinal); output.scalar(field.offset); output.scalar(field.isNativeString);
            output.scalar(field.isNativeArray); output.text(field.arrayTypeKey);
        }
        output.scalar(table.destructionSteps.size());
        for (const auto& step : table.destructionSteps)
        {
            output.text(step.destructorCalleeName); output.scalar(step.ownedField.memberOrdinal);
            output.scalar(step.ownedField.offset); output.scalar(step.ownedField.isNativeString);
            output.scalar(step.ownedField.isNativeArray); output.text(step.ownedField.arrayTypeKey);
            output.scalar(step.isFieldRelease);
            output.scalar(step.isValueFieldDrop); output.text(step.valueFieldTypeKey);
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
}

void writeInterfaceBindings(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
    output.scalar(model.interfaceBindings().size());
    for (const auto& binding : model.interfaceBindings())
    {
        output.scalar(binding.implementingTypeOrdinal); output.scalar(binding.interfaceTypeOrdinal);
        output.text(binding.interfaceTypeKey); output.scalar(binding.stableInterfaceTypeId);
        output.text(binding.methodTableName); output.scalars(binding.interfaceMemberOrdinals);
        output.scalars(binding.implementationMemberOrdinals); output.texts(binding.calleeNames);
    }
}

void writeGenericSpecializations(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
    output.scalar(model.genericSpecializations().size());
    for (const auto& specialization : model.genericSpecializations())
    {
        output.text(specialization.key); output.scalars(specialization.functionIndices);
        output.scalars(specialization.staticFieldIndices);
        output.scalars(specialization.dispatchTableIndices);
    }
}

void writeFieldInitializers(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
    output.scalar(model.fieldInitializers().size());
    for (const auto& initializer : model.fieldInitializers())
    {
        output.scalar(initializer.memberOrdinal); output.scalar(initializer.declaringTypeOrdinal);
        output.scalar(initializer.isStatic); output.scalar(initializer.isConst);
        writeNode(output, initializer.value.get());
    }
}

void writeCommonModel(IdentityWriter& output, const frontend::ProjectOopMirModel& model)
{
    output.scalar(frontend::kProjectOopMirSchemaMajor);
    output.scalar(frontend::kProjectOopMirSchemaMinor);
    writeStructLayouts(output, model);
    writeClassLayouts(output, model);
    writeStaticFields(output, model);
    writeDispatchTables(output, model);
    writeInterfaceBindings(output, model);
    writeGenericSpecializations(output, model);
    writeFieldInitializers(output, model);
}

std::string identityDigest(std::string_view identity)
{
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(identity.data()), identity.size());
    return basic::sha256Hex(basic::sha256(bytes));
}
}

ProjectOopMirCacheIdentityIndex::ProjectOopMirCacheIdentityIndex(
    const frontend::ProjectOopMirModel& model)
    : ProjectOopMirCacheIdentityIndex(model, nullptr)
{
}

ProjectOopMirCacheIdentityIndex::ProjectOopMirCacheIdentityIndex(
    const frontend::ProjectOopMirModel& model,
    driver::CompilationWorkerPool* workerPool)
{
    // EN: Every function owns its two slots, so each digest is computed exactly as in the
    //     sequential loop no matter which worker writes it; the model is only read.
    // ES: Cada función es dueña de sus dos ranuras, así que cada digest se calcula igual que en
    //     el ciclo secuencial sin importar qué worker lo escriba; el modelo solo se lee.
    constexpr std::size_t kFunctionsPerTask = 32;
    const std::size_t functionCount = model.functions().size();
    declarationDigests_.resize(functionCount);
    definitionDigests_.resize(functionCount);
    const bool parallel = workerPool != nullptr && workerPool->workerCount() > 1 &&
        functionCount > kFunctionsPerTask;
    if (parallel)
    {
        for (std::size_t begin = 0; begin < functionCount; begin += kFunctionsPerTask)
        {
            const std::size_t end = std::min(functionCount, begin + kFunctionsPerTask);
            workerPool->submit([this, &model, begin, end](std::size_t) {
                computeFunctionDigests(model, begin, end);
            });
        }
    }

    // EN: The common digest overlaps with the queued tasks; they reference this object, so a
    //     failure here still drains them before the exception leaves the constructor.
    // ES: El digest común se traslapa con las tareas encoladas; estas referencian este objeto,
    //     así que una falla aquí las drena antes de que la excepción salga del constructor.
    try
    {
        IdentityWriter common;
        common.text("NativeSharp.ProjectOopMirCacheIdentity.Common.v1");
        writeCommonModel(common, model);
        commonDigest_ = identityDigest(common.take());
    }
    catch (...)
    {
        if (parallel)
        {
            try { workerPool->wait(); } catch (...) {}
        }
        throw;
    }

    if (parallel) workerPool->wait();
    else computeFunctionDigests(model, 0, functionCount);
}

void ProjectOopMirCacheIdentityIndex::computeFunctionDigests(
    const frontend::ProjectOopMirModel& model, std::size_t begin, std::size_t end)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = model.functions();
    for (std::size_t ordinal = begin; ordinal < end; ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        IdentityWriter declaration;
        declaration.text("NativeSharp.ProjectOopMirCacheIdentity.Declaration.v1");
        writeFunction(declaration, function, false);
        declarationDigests_[ordinal] = identityDigest(declaration.take());

        IdentityWriter definition;
        definition.text("NativeSharp.ProjectOopMirCacheIdentity.Definition.v1");
        writeFunction(definition, function, true);
        definitionDigests_[ordinal] = identityDigest(definition.take());
    }
}

std::string ProjectOopMirCacheIdentityIndex::identity(
    std::span<const std::size_t> definitionOrdinals,
    std::span<const std::size_t> declarationOrdinals) const
{
    IdentityWriter output;
    output.text("NativeSharp.ProjectOopMirCacheIdentity.Indexed.v1");
    output.text(commonDigest_);
    output.scalar(declarationOrdinals.size());
    for (const std::size_t ordinal : declarationOrdinals)
    {
        output.scalar(ordinal);
        if (ordinal >= declarationDigests_.size()) continue;
        const bool definition = std::binary_search(
            definitionOrdinals.begin(), definitionOrdinals.end(), ordinal);
        output.scalar(definition);
        output.text(definition ? definitionDigests_[ordinal] : declarationDigests_[ordinal]);
    }
    return output.take();
}

std::string projectOopMirCacheIdentity(
    const frontend::ProjectOopMirModel& model,
    std::span<const std::size_t> definitionOrdinals,
    std::span<const std::size_t> declarationOrdinals)
{
    return ProjectOopMirCacheIdentityIndex(model).identity(
        definitionOrdinals, declarationOrdinals);
}

std::string projectOopLineTableSourceIdentity(
    const source::SourceManager& sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const SourceRange> ranges)
{
    IdentityWriter output;
    output.text("NativeSharp.ProjectOopLineTableSourceIdentity.v1");
    output.scalar(debugPrefixMaps.size());
    for (const DebugPrefixMap& map : debugPrefixMaps)
    {
        output.text(map.sourcePrefix.lexically_normal().generic_string());
        output.text(map.replacementPrefix.lexically_normal().generic_string());
    }

    std::unordered_set<std::uint32_t> uniqueFileIds;
    output.scalar(ranges.size());
    for (const SourceRange range : ranges)
    {
        output.scalar(range.isValid());
        if (!range.isValid()) continue;
        output.scalar(range.begin().fileId().value());
        output.scalar(range.begin().offset());
        output.scalar(range.end().offset());
        uniqueFileIds.insert(range.begin().fileId().value());
    }

    std::vector<std::uint32_t> fileIds(uniqueFileIds.begin(), uniqueFileIds.end());
    std::ranges::sort(fileIds);
    output.scalar(fileIds.size());
    for (const std::uint32_t fileId : fileIds)
    {
        output.scalar(fileId);
        const source::SourceFile* sourceFile = sourceManager.getFile(FileId(fileId));
        output.scalar(sourceFile != nullptr);
        if (sourceFile == nullptr) continue;
        output.text(sourceFile->path().lexically_normal().generic_string());
        output.scalar(sourceFile->buffer().size());
        output.text(identityDigest(sourceFile->buffer()));
    }
    return output.take();
}
}
