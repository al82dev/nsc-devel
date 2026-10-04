#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"

#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <sstream>

namespace nativesharp::frontend
{
namespace
{
void dumpNode(std::ostringstream& output, const ProjectOopMirNode* node, int depth)
{
    const std::string indent(static_cast<std::size_t>(depth * 2), ' ');
    if (node == nullptr)
    {
        output << indent << "<none>\n";
        return;
    }
    output << indent << projectOopMirNodeKindName(node->kind);
    if (!node->typeKey.empty()) output << " type=" << node->typeKey;
    if (node->memberOrdinal.has_value()) output << " member=#" << *node->memberOrdinal;
    if (node->namespaceFunctionOrdinal.has_value())
        output << " namespace-function=#" << *node->namespaceFunctionOrdinal;
    if (node->namespaceStateOrdinal.has_value())
        output << " namespace-state=#" << *node->namespaceStateOrdinal;
    if (node->localOrdinal.has_value()) output << " local=#" << *node->localOrdinal;
    if (node->mirRegionOrdinal.has_value()) output << " mir-region=#" << *node->mirRegionOrdinal;
    if (node->exceptionTargetRegionOrdinal.has_value()) output << " exception-target=#" << *node->exceptionTargetRegionOrdinal;
    if (node->cleanupRegionOrdinal.has_value()) output << " cleanup=#" << *node->cleanupRegionOrdinal;
    if (node->targetScopeOrdinal.has_value()) output << " target-scope=#" << *node->targetScopeOrdinal;
    if (node->exceptionTypeOrdinal.has_value()) output << " exception-type=#" << *node->exceptionTypeOrdinal;
    if (node->catchAll) output << " catch=all";
    if (node->kind == ProjectOopMirNodeKind::CatchRegion)
    {
        output << " pattern=" << (node->catchAll ? "_" : node->elementTypeKey)
               << " filter=" << (node->catchHasFilter ? "true" : "false")
               << " exhaustive=" << (node->catchExhaustive ? "true" : "false")
               << " matches=[";
        for (std::size_t index = 0; index < node->catchMatchedErrorTypeKeys.size(); ++index)
        {
            if (index != 0) output << ',';
            output << node->catchMatchedErrorTypeKeys[index];
        }
        output << ']';
    }
    if (node->disposeContractMemberOrdinal.has_value()) output << " dispose-contract=#" << *node->disposeContractMemberOrdinal;
    if (node->disposeImplementationMemberOrdinal.has_value()) output << " dispose-implementation=#" << *node->disposeImplementationMemberOrdinal;
    if (node->interfaceCleanupDispatch) output << " cleanup-dispatch=interface";
    if (!node->cleanupRegionOrdinals.empty())
    {
        output << " cleanup-path=";
        for (std::size_t index = 0; index < node->cleanupRegionOrdinals.size(); ++index)
        {
            if (index != 0) output << "->";
            output << '#' << node->cleanupRegionOrdinals[index];
        }
    }
    if (node->cleanupContinuationKind.has_value())
        output << " continuation=" << methodCleanupContinuationKindName(*node->cleanupContinuationKind);
    if (node->borrowKind.has_value())
        output << " borrow=" << methodBorrowKindName(*node->borrowKind);
    if (node->localBorrowSourceKind.has_value())
        output << " borrow-source=" << methodLocalBorrowSourceKindName(*node->localBorrowSourceKind);
    if (node->parameterOrdinal.has_value()) output << " param=#" << *node->parameterOrdinal;
    if (node->constructionOrdinal.has_value()) output << " construction=#" << *node->constructionOrdinal;
    if (node->kind == ProjectOopMirNodeKind::RuntimeTypeTest ||
        node->kind == ProjectOopMirNodeKind::SafeReferenceCast ||
        node->kind == ProjectOopMirNodeKind::CheckedReferenceCast)
        output << " target-type=#" << node->targetTypeOrdinal.value_or(0)
               << " target-kind=" << (node->targetIsInterface ? "interface" : "class")
               << " type-id=" << node->immediate;
    if (node->kind == ProjectOopMirNodeKind::FieldAddress ||
        node->kind == ProjectOopMirNodeKind::FieldLoad ||
        node->kind == ProjectOopMirNodeKind::NullConditionalFieldLoad ||
        node->kind == ProjectOopMirNodeKind::BitFieldAddress ||
        node->kind == ProjectOopMirNodeKind::BitFieldLoad)
        output << " offset=" << node->immediate << " align=" << node->alignment;
    if (node->kind == ProjectOopMirNodeKind::BitFieldAddress ||
        node->kind == ProjectOopMirNodeKind::BitFieldLoad)
        output << " storage=" << node->bitStorageSize
               << " bit-offset=" << node->bitOffset.value_or(0)
               << " bit-width=" << node->bitWidth.value_or(0);
    if (node->kind == ProjectOopMirNodeKind::AggregateElementStore && node->bitWidth.has_value())
        output << " bit-storage=" << node->bitStorageSize
               << " bit-offset=" << node->bitOffset.value_or(0)
               << " bit-width=" << *node->bitWidth;
    if (node->kind == ProjectOopMirNodeKind::AggregateElementStore && !node->bitWidth.has_value())
        output << " offset=" << node->immediate << " bytes=" << node->bitStorageSize;
    if (node->kind == ProjectOopMirNodeKind::AggregateSpreadStore)
        output << " targets=" << (node->children.empty() ? 0U : node->children.size() - 1U)
               << " source-evaluations=1 mode=" << (node->booleanValue ? "move" : "copy");
    if (node->kind == ProjectOopMirNodeKind::ParameterPack)
        output << " element=" << node->elementTypeKey
               << " elements=" << node->children.size()
               << " element-size=" << node->immediate
               << " element-align=" << node->alignment
               << " span-size=" << node->secondaryImmediate
               << " data-offset=" << node->integerValue
               << " length-offset=" << node->bitStorageSize
               << " storage=stack";
    if (node->kind == ProjectOopMirNodeKind::AllocateAndConstruct)
        output << " bytes=" << node->immediate << " align=" << node->alignment
               << " zero=" << (node->zeroInitialize ? "true" : "false")
               << " failure=" << (node->allocationFailureReturnsNull ? "null" : "trap");
    if (node->kind == ProjectOopMirNodeKind::PlacementConstruct)
        output << " bytes=" << node->immediate << " align=" << node->alignment
               << " zero=" << (node->zeroInitialize ? "true" : "false")
               << " storage=supplied";
    if (node->kind == ProjectOopMirNodeKind::ArrayAllocate ||
        node->kind == ProjectOopMirNodeKind::RangeArrayAllocate ||
        node->kind == ProjectOopMirNodeKind::ArrayElementLoad ||
        node->kind == ProjectOopMirNodeKind::ArrayElementAddress ||
        node->kind == ProjectOopMirNodeKind::RangeArrayElementLoad ||
        node->kind == ProjectOopMirNodeKind::RangeArrayElementAddress)
        output << " element=" << node->elementTypeKey
               << " element-size=" << node->immediate
               << " element-align=" << node->alignment
               << ((node->kind != ProjectOopMirNodeKind::ArrayAllocate && node->booleanValue)
                    ? " index=from-end" : "");
    if (node->nativeArrayRank > 1) output << " rank=" << node->nativeArrayRank;
    if (node->kind == ProjectOopMirNodeKind::StringElementLoad && node->booleanValue)
        output << " index=from-end";
    if (node->kind == ProjectOopMirNodeKind::StringLiteral)
        output << " bytes=" << node->stringValue.size();
    if (node->kind == ProjectOopMirNodeKind::InlineAssembly)
        output << " source-bytes=" << node->stringValue.size()
               << " side-effects=" << (node->inlineAssemblyHasSideEffects ? "true" : "false")
               << " memory-barrier=" << (node->inlineAssemblyClobbersMemory ? "true" : "false")
               << " flags=" << (node->inlineAssemblyClobbersFlags ? "true" : "false");
    if (node->kind == ProjectOopMirNodeKind::InlineAssemblyLine)
        output << " text=\"" << node->stringValue << "\" contract=" << node->elementTypeKey;
    if (node->kind == ProjectOopMirNodeKind::InlineAssemblyInput ||
        node->kind == ProjectOopMirNodeKind::InlineAssemblyOutput)
        output << " ordinal=" << node->integerValue << " constraint=" << node->stringValue
               << " explicit=" << (node->booleanValue ? "true" : "false");
    if (node->kind == ProjectOopMirNodeKind::InlineAssemblyClobber)
        output << " ordinal=" << node->integerValue << " name=" << node->stringValue
               << " category=" << node->elementTypeKey
               << " preserve=" << (node->booleanValue ? "true" : "false");
    if (node->referenceLocal) output << " reference-local=true";
    if (node->pinnedStorage) output << " pinned=true";
    if (node->referenceResult) output << " reference-result=true";
    if (node->kind == ProjectOopMirNodeKind::LoopExpression)
        output << " label=" << (node->stringValue.empty() ? "<none>" : node->stringValue)
               << " break-result=" << node->typeKey;
    if ((node->kind == ProjectOopMirNodeKind::Break ||
         node->kind == ProjectOopMirNodeKind::Continue) && !node->stringValue.empty())
        output << " target-label=" << node->stringValue;
    if (node->kind == ProjectOopMirNodeKind::Break)
        output << " value=" << (node->children.empty() ? "none" : node->elementTypeKey);
    if (node->kind == ProjectOopMirNodeKind::MatchExpression)
        output << " dispatch=" << (node->booleanValue ? "tag-switch" : "ordered-branches")
               << " governing-evaluations=1"
               << (node->booleanValue ? " tag-offset=" + std::to_string(node->immediate) : "");
    if (node->kind == ProjectOopMirNodeKind::MatchArm && !node->switchLabels.empty())
    {
        output << " tags=";
        for (std::size_t index = 0; index < node->switchLabels.size(); ++index)
        {
            if (index != 0) output << ',';
            output << node->switchLabels[index];
        }
    }
    if (node->nullConditionalCall) output << " null-conditional=true";
    if (node->mustTailCall) output << " must-tail=true";
    if (node->kind == ProjectOopMirNodeKind::Binary && node->booleanValue &&
        (node->operation == lexer::TokenKind::Slash || node->operation == lexer::TokenKind::Percent))
        output << " check=elided-flow";
    if (node->kind == ProjectOopMirNodeKind::FieldLoad && node->booleanValue)
        output << " null-check=elided-flow";
    if ((node->kind == ProjectOopMirNodeKind::DirectInstanceCall ||
         node->kind == ProjectOopMirNodeKind::DirectStaticCall ||
         node->kind == ProjectOopMirNodeKind::VirtualInstanceCall ||
         node->kind == ProjectOopMirNodeKind::InterfaceInstanceCall) &&
        node->booleanValue)
        output << " variant-case=proven";
    if ((node->kind == ProjectOopMirNodeKind::DirectInstanceCall ||
         node->kind == ProjectOopMirNodeKind::DirectStaticCall ||
         node->kind == ProjectOopMirNodeKind::VirtualInstanceCall ||
         node->kind == ProjectOopMirNodeKind::InterfaceInstanceCall) &&
        node->stringValue == "flow-nonnull")
        output << " null-check=elided-flow";
    if (node->ownership != sema::OwnershipKind::None)
        output << " ownership=" << sema::ownershipKindName(node->ownership);
    if (node->automaticDestruction) output << " destruction=scope-bound";
    if (node->stackPromotedObject) output << " stack-promoted=true";
    if (node->volatileAccess) output << " volatile=true";
    if (node->kind == ProjectOopMirNodeKind::ClosureCaptureLoad)
        output << " capture=#" << node->immediate;
    if (node->kind == ProjectOopMirNodeKind::LambdaCreate)
        output << " lambda=#" << node->immediate;
    if (!node->calleeName.empty()) output << " callee=" << node->calleeName;
    if (!node->secondaryCalleeName.empty())
        output << " secondary-callee=" << node->secondaryCalleeName;
    if (node->kind == ProjectOopMirNodeKind::ResultPropagation)
        output << " source-carrier=" << node->resultSourceCarrierTypeKey
               << " success-tag=" << node->resultSuccessTag
               << " error=" << node->resultSourceErrorTypeKey
               << " error-tag=" << node->resultErrorTag
               << " return-carrier=" << node->resultReturnCarrierTypeKey
               << " return-error=" << node->resultReturnErrorTypeKey
               << " return-error-tag=" << node->resultReturnErrorTag
               << " success-test=" << node->resultSuccessTestCalleeName
               << " success-extractor=" << node->resultSuccessExtractorCalleeName
               << " error-extractor=" << node->resultErrorExtractorCalleeName
               << " error-factory=" << node->resultErrorFactoryCalleeName
               << (node->resultErrorConversionCalleeName.empty()
                    ? " conversion=identity" :
                      " conversion=" + node->resultErrorConversionCalleeName);
    if (node->operation != lexer::TokenKind::Invalid) output << " op=" << lexer::tokenKindName(node->operation);
    if (node->conversionKind.has_value()) output << " conversion=" << projectConversionKindName(*node->conversionKind);
    if (node->kind == ProjectOopMirNodeKind::Deallocate)
        output << " destruction=" << (node->dynamicDestruction ? "dynamic-descriptor" : "direct-free");
    if (node->kind == ProjectOopMirNodeKind::ObjectDestroy)
        output << " destruction=" << (node->dynamicDestruction ? "dynamic-object-only" : "trivial-object");
    if (node->objectLifetimeOwnership != sema::OwnershipKind::None ||
        node->storageOwnership != sema::OwnershipKind::None)
        output << " object-ownership=" << sema::ownershipKindName(node->objectLifetimeOwnership)
               << " storage-ownership=" << sema::ownershipKindName(node->storageOwnership);
    output << '\n';
    for (const auto& child : node->children) dumpNode(output, child.get(), depth + 1);
}
}

std::string ProjectOopMirModelDumper::dump(const ProjectOopMirModel& model)
{
    std::ostringstream output;
    output << "project.oop.mir functions=" << model.functions().size()
           << " schema=" << kProjectOopMirSchemaMajor << '.' << kProjectOopMirSchemaMinor
           << " panic-profile=" << language::panicProfileName(model.panicProfile())
           << " panic-handler=" << (model.panicHandler().empty() ? "none" : model.panicHandler())
           << '\n';
    output << "struct-values=" << model.structLayouts().size() << '\n';
    for (const ProjectOopMirStructLayout& layout : model.structLayouts())
    {
        output << "struct-value type=#" << layout.typeOrdinal
               << " key=" << layout.typeKey
               << " size=" << layout.size
               << " align=" << layout.alignment
               << " min-align=" << layout.minimumAlignment
               << " abi=" << projectOopMirAbiPassingName(layout.abiPassing)
               << " imported=" << (layout.isImported ? "true" : "false")
               << " requires-drop=" << (layout.requiresDrop ? "true" : "false");
        if (!layout.dropGlueEntryName.empty())
            output << " drop-glue=" << layout.dropGlueEntryName;
        output << '\n';
    }
    output << "generic-specializations=" << model.genericSpecializations().size() << '\n';
    for (const ProjectOopMirGenericSpecialization& specialization : model.genericSpecializations())
    {
        output << "generic-specialization key=" << specialization.key
               << " functions=" << specialization.functionIndices.size()
               << " static-fields=" << specialization.staticFieldIndices.size()
               << " dispatch-tables=" << specialization.dispatchTableIndices.size() << '\n';
    }
    for (const ProjectOopMirFunction& function : model.functions())
    {
        output << "function #" << function.memberOrdinal;
        if (function.namespaceFunctionOrdinal.has_value())
            output << " namespace-function=#" << *function.namespaceFunctionOrdinal;
        output << ' ' << function.mangledName
               << (function.isLambda ? " lambda" : "")
               << " inlining=" << inliningPolicyName(function.inliningPolicy)
               << " noheap=" << (function.isNoHeap ? "true" : "false")
               << " effects=" << function.effects.bits()
               << " abi=(" << (function.isLambda && function.hasClosureEnvironment
                    ? "closure-env:first" : function.isStatic ? "static" : "this:first")
               << (function.isExportedC ? " export-c=" + function.cAbiName : "");
        if (function.isVariadic) output << " variadic";
        if (function.isImportedNative) output << " import-native";
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
            output << ",p" << index << ':' << function.parameterTypeKeys[index]
                   << '[' << projectOopMirAbiPassingName(function.abi.parameterPassing[index])
                   << "@" << function.abi.parameterPhysicalOrdinals[index] << ']';
        output << ") -> " << function.returnTypeKey
               << "[" << projectOopMirAbiPassingName(function.abi.returnPassing);
        if (function.abi.hiddenReturnParameterOrdinal.has_value())
            output << "@" << *function.abi.hiddenReturnParameterOrdinal;
        output << "]"
               << " throws=" << (function.errorTypeKey.empty()
                    ? "<none>" : function.errorTypeKey)
               << " ref-return=" << (function.referenceReturn ? "true" : "false")
               << " return-ownership=" << sema::ownershipKindName(function.returnOwnership)
               << (function.constructor ? " constructor" : function.destructor ? " destructor" : function.isLambda ? " lambda-body" : " method");
        if (function.isLambda)
        {
            output << " captures=" << function.captureTypeKeys.size()
                   << " callable-trait=" << methodLambdaCallableTraitName(function.callableTrait)
                   << " closure-layout="
                   << function.closureLayoutSize << ':' << function.closureLayoutAlignment
                   << " closure-allocation="
                   << (function.hasClosureEnvironment ? "managed-heap" : "none");
        }
        if (!function.genericSpecializationKey.empty())
            output << " generic-specialization=" << function.genericSpecializationKey;
        output << '\n';
        for (std::size_t index = 0; index < function.parameterOwnerships.size(); ++index)
        {
            if (index < function.parameterPassKinds.size())
            {
                output << "  parameter-pass[" << index << "]="
                       << parameterPassKindName(function.parameterPassKinds[index]) << '\n';
            }
            output << "  parameter-ownership[" << index << "]="
                   << sema::ownershipKindName(function.parameterOwnerships[index]) << '\n';
        }
        output << "  cleanup-contract=" << function.cleanupRegions.size() << '\n';
        for (std::size_t index = 0; index < function.captureTypeKeys.size(); ++index)
        {
            output << "  capture[" << index << "] name=";
            if (index < function.captureNames.size() && !function.captureNames[index].empty())
                output << function.captureNames[index];
            else
                output << "<unnamed>";
            output << " type=" << function.captureTypeKeys[index]
                   << " source=";
            if (index < function.captureSourceKinds.size())
                output << methodLambdaCaptureSourceKindName(function.captureSourceKinds[index]);
            else
                output << "<missing>";
            output << " mode=";
            if (index < function.captureModes.size())
                output << methodLambdaCaptureModeName(function.captureModes[index]);
            else
                output << "<missing>";
            output << " storage="
                   << (index < function.captureModes.size() &&
                       (function.captureModes[index] == language::CaptureMode::Copy ||
                        function.captureModes[index] == language::CaptureMode::Move)
                       ? "owned-value-cell" : "shared-reference-cell")
                   << " owns-value="
                   << (index < function.captureOwnsValues.size() && function.captureOwnsValues[index]
                       ? "true" : "false") << '\n';
        }
        for (const ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
        {
            output << "    cleanup #" << cleanup.ordinal
                   << " kind=" << projectOopMirCleanupKindName(cleanup.kind)
                   << " type=" << cleanup.typeKey
                   << " ownership=" << sema::ownershipKindName(cleanup.ownership);
            if (cleanup.localOrdinal.has_value()) output << " local=#" << *cleanup.localOrdinal;
            if (cleanup.parameterOrdinal.has_value())
                output << " parameter=#" << *cleanup.parameterOrdinal;
            if (cleanup.automaticDestruction) output << " automatic-destruction";
            output << '\n';
        }
        output << "  exception-flow personality="
               << (function.exceptionFlow.requiresPersonality ? "true" : "false")
               << " protected=" << function.exceptionFlow.protectedRegionOrdinals.size()
               << " catches=" << function.exceptionFlow.catchRegionCount
               << " finally=" << function.exceptionFlow.finallyRegionCount
               << " exits=" << function.exceptionFlow.nonlocalExits.size() << '\n';
        for (const ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
        {
            output << "    nonlocal-exit kind=" << projectOopMirNonlocalExitKindName(exit.kind)
                   << " cleanup-path=";
            if (exit.cleanupRegionOrdinals.empty()) output << "<none>";
            for (std::size_t index = 0; index < exit.cleanupRegionOrdinals.size(); ++index)
            {
                if (index != 0) output << ',';
                output << '#' << exit.cleanupRegionOrdinals[index];
            }
            if (exit.targetScopeOrdinal.has_value()) output << " target-scope=#" << *exit.targetScopeOrdinal;
            if (exit.exceptionTargetRegionOrdinal.has_value())
                output << " exception-target=#" << *exit.exceptionTargetRegionOrdinal;
            output << '\n';
        }
        dumpNode(output, function.body.get(), 1);
    }
    output << "static-fields=" << model.staticFields().size() << '\n';
    for (const ProjectOopMirStaticField& field : model.staticFields())
    {
        output << "static-field member=#" << field.memberOrdinal
               << " type=#" << field.declaringTypeOrdinal;
        if (field.namespaceStateOrdinal.has_value())
            output << " namespace-state=#" << *field.namespaceStateOrdinal;
        output << " name=" << field.name
               << " debug-name=" << field.debugDisplayName
               << " value-type=" << field.typeKey
               << " readonly=" << (field.isReadonly ? "true" : "false")
               << " volatile=" << (field.isVolatile ? "true" : "false")
               << " const=" << (field.isConst ? "true" : "false")
               << " constant=" << (field.hasConstantInitializer ? "true" : "false");
        if (field.hasConstantInitializer)
        {
            if (field.typeKey == "builtin:bool") output << " value=" << (field.constantBooleanValue ? "true" : "false");
            else if (field.typeKey == "builtin:float" || field.typeKey == "builtin:double") output << " value=" << field.constantFloatingValue;
            else if (field.typeKey == "builtin:string") output << " bytes=" << field.constantStringValue.size();
            else output << " value=" << field.constantIntegerValue;
        }
        output << '\n';
    }
    output << "dispatch-tables=" << model.dispatchTables().size() << '\n';
    for (const ProjectOopMirDispatchTable& table : model.dispatchTables())
    {
        output << "dispatch type=#" << table.typeOrdinal
               << " key=" << table.typeKey
               << " descriptor=" << table.descriptorName
               << " vtable=" << (table.vtableName.empty() ? "<none>" : table.vtableName)
               << " type-id=" << table.stableTypeId
               << " destroy=" << table.destructionEntryName
               << " destroy-object=" << table.objectDestructionEntryName;
        if (!table.genericSpecializationKey.empty())
            output << " generic-specialization=" << table.genericSpecializationKey;
        output << '\n';
        for (std::size_t slot = 0; slot < table.slotMemberOrdinals.size(); ++slot)
        {
            output << "  slot[" << slot << "] member=#" << table.slotMemberOrdinals[slot] << " callee=";
            if (slot < table.slotCalleeNames.size() && !table.slotCalleeNames[slot].empty())
                output << table.slotCalleeNames[slot];
            else
                output << "<abstract>";
            output << '\n';
        }
        output << "  destructor-chain=";
        if (table.destructorCalleeNames.empty())
        {
            output << "<free-only>";
        }
        else
        {
            for (std::size_t index = 0; index < table.destructorCalleeNames.size(); ++index)
            {
                if (index != 0) output << " -> ";
                output << table.destructorCalleeNames[index];
            }
        }
        output << '\n';
    }
    output << "interface-bindings=" << model.interfaceBindings().size() << '\n';
    for (const ProjectOopMirInterfaceBinding& binding : model.interfaceBindings())
    {
        output << "interface-binding type=#" << binding.implementingTypeOrdinal
               << " interface=#" << binding.interfaceTypeOrdinal
               << " key=" << binding.interfaceTypeKey
               << " type-id=" << binding.stableInterfaceTypeId
               << " method-table=" << binding.methodTableName
               << " slots=" << binding.interfaceMemberOrdinals.size() << '\n';
    }
    output << "field-initializers=" << model.fieldInitializers().size() << '\n';
    for (const ProjectOopMirFieldInitializer& initializer : model.fieldInitializers())
    {
        output << "field-initializer member=#" << initializer.memberOrdinal
               << " type=#" << initializer.declaringTypeOrdinal
               << " static=" << (initializer.isStatic ? "true" : "false")
               << " const=" << (initializer.isConst ? "true" : "false") << '\n';
        dumpNode(output, initializer.value.get(), 1);
    }
    return output.str();
}
}
