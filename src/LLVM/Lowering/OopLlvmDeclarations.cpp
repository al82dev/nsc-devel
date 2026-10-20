#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMTypeRef OopLlvmImplementation::createDeclaredFunctionType(
        const frontend::ProjectOopMirFunction& function,
        bool sretReturn,
        std::string& error)
{
            // Win64 aggregate ABI (ground truth: clang for x86_64-pc-windows-msvc). An indirect
            // struct return becomes `void` plus a leading sret pointer parameter; an indirect
            // struct parameter becomes a plain pointer to a caller-owned per-call copy. For
            // instance methods the sret slot sits AFTER `this` (RCX = this, RDX = sret) --
            // confirmed against clang's own C++ output, where member functions carry
            // (ptr this, ptr sret) in that order.
            LLVMTypeRef returnType = function.referenceReturn
                ? LLVMPointerTypeInContext(context_, 0)
                : sretReturn
                    ? LLVMVoidTypeInContext(context_)
                    : llvmType(function.returnTypeKey, error);
            if (returnType == nullptr)
            {
                return nullptr;
            }
            std::vector<LLVMTypeRef> parameters;
            if (!function.isStatic)
            {
                parameters.push_back(LLVMPointerTypeInContext(context_, 0));
            }
            if (sretReturn)
            {
                parameters.push_back(LLVMPointerTypeInContext(context_, 0));
            }
            for (std::size_t parameterOrdinal = 0;
                 parameterOrdinal < function.parameterTypeKeys.size(); ++parameterOrdinal)
            {
                const std::string& parameter = function.parameterTypeKeys[parameterOrdinal];
                const frontend::ParameterPassKind passKind =
                    parameterOrdinal < function.parameterPassKinds.size()
                    ? function.parameterPassKinds[parameterOrdinal]
                    : frontend::ParameterPassKind::Value;
                LLVMTypeRef type = passKind == frontend::ParameterPassKind::Value
                    ? abiParameterType(parameter, error)
                    : LLVMPointerTypeInContext(context_, 0);
                if (type == nullptr || type == LLVMVoidTypeInContext(context_))
                {
                    if (error.empty()) error = "invalid Stage 34 method parameter type: " + parameter;
                    return nullptr;
                }
                parameters.push_back(type);
            }
            return LLVMFunctionType(
                returnType,
                parameters.empty() ? nullptr : parameters.data(),
                static_cast<unsigned>(parameters.size()),
                function.isVariadic ? 1 : 0);
    }

bool OopLlvmImplementation::declareFunction(
        const frontend::ProjectOopMirFunction& function,
        std::size_t functionOrdinal,
        std::string& error)
{
            if (function.isTransportedInlineBody &&
                (functionOrdinal >= reachableDefinitions_.size() ||
                 reachableDefinitions_[functionOrdinal] == 0U)) return true;
            if (partitioned_ && restrictFunctionDeclarations_ &&
                !functionDeclarations_[functionOrdinal]) return true;
            const bool sretReturn = function.abi.returnPassing ==
                frontend::ProjectOopMirAbiPassing::Indirect;
            LLVMTypeRef functionType = createDeclaredFunctionType(function, sretReturn, error);
            if (functionType == nullptr) return false;
            // C1.2 "import identity": the same triple (library, symbol, signature) may be
            // DECLARED more than once -- two classes may each wrap `kernel32!GetCurrentProcessId`,
            // and the frontend accepts that because it is ONE import, not a conflict. One import
            // is one LLVM declaration: asking LLVM for the name twice would silently mint
            // `GetCurrentProcessId.1` and the link would end with "undefined symbol:
            // GetCurrentProcessId.1" (measured, by this round's own suite). Every member keeps its
            // own MIR record and simply resolves to the shared declaration.
            if (function.isImportedC || function.isImportedNative)
            {
                const auto existing = functions_.find(function.mangledName);
                if (existing != functions_.end()) return true;
            }
            LLVMValueRef llvmFunction = LLVMAddFunction(module_, function.mangledName.c_str(), functionType);
            if (llvmFunction == nullptr)
            {
                error = "Stage 34 failed to declare LLVM function " + function.mangledName;
                return false;
            }
            const bool ownsDefinition = !function.isImportedC && !function.isImportedNative &&
                (!partitioned_ || (!ownsModuleGlobals_ && functionDefinitions_[functionOrdinal]));
            if (!function.isImportedC && !function.isImportedNative)
            {
                if (ownsDefinition && !function.genericSpecializationKey.empty())
                {
                    applyGenericOdrDefinition(llvmFunction, function.genericSpecializationKey);
                }
                else if (ownsDefinition && function.isSyntheticImplicitConstructor)
                {
                    // An implicit constructor is a compiler-owned ODR definition. The same nominal
                    // type can legitimately reach a final native link through a source dependency
                    // and through a static precompiled producer. Both compilations derive the same
                    // physical symbol and semantics from the type declaration; leaving both copies
                    // strong made the ubiquitous NativeSharp.Core.Object() root collide first.
                    // Written constructors remain strong and therefore still diagnose real duplicate
                    // user implementations.
                    applyOdrDefinition(llvmFunction);
                }
                else if (ownsDefinition &&
                    (function.inliningPolicy == frontend::InliningPolicy::Preferred ||
                     function.inliningPolicy == frontend::InliningPolicy::Required))
                {
                    // A public inline body can be materialized by both its producer and any
                    // NSMETA 1.13 consumer. They share one deterministic physical identity and
                    // therefore require symbol-scoped ODR/COMDAT linkage, never two strong defs.
                    applyOdrDefinition(llvmFunction);
                }
            }
            applyUwtableAttribute(llvmFunction);
            applyDenormalFloatingPointAttribute(llvmFunction);
            if (function.inliningPolicy == frontend::InliningPolicy::Preferred &&
                !function.isImportedC && !function.isImportedNative)
            {
                applyFunctionAttribute(llvmFunction, "inlinehint");
            }
            else if (function.inliningPolicy == frontend::InliningPolicy::Required &&
                !function.isImportedC && !function.isImportedNative)
            {
                applyFunctionAttribute(llvmFunction, "alwaysinline");
            }
            else if (function.inliningPolicy == frontend::InliningPolicy::Forbidden &&
                !function.isImportedC)
            {
                // A precompiled NativeSharp declaration owns no local body, but its producer's
                // noinline contract still constrains every consumer call site. LLVM accepts the
                // attribute on the declaration and preserves the external call boundary.
                applyFunctionAttribute(llvmFunction, "noinline");
            }
            if (sretReturn)
            {
                applySretAttributes(
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                    { LLVMAddAttributeAtIndex(llvmFunction, index, attribute); },
                    static_cast<unsigned>(*function.abi.hiddenReturnParameterOrdinal),
                    function.returnTypeKey);
            }
            LLVMMetadataRef subprogram = ownsDefinition ? createDebugSubprogram(function) : nullptr;
            if (debugBuilder_ != nullptr && ownsDefinition &&
                subprogram == nullptr)
            {
                error = "failed to create LLVM debug subprogram for " + function.mangledName;
                return false;
            }
            if (subprogram != nullptr) LLVMSetSubprogram(llvmFunction, subprogram);
            functions_[function.mangledName] =
                FunctionInfo{llvmFunction, functionType, subprogram, function.asyncFrameSize,
                             function.frameStorage};
        return true;
    }

bool OopLlvmImplementation::declareFunctions(std::string& error)
{
        const std::span<const frontend::ProjectOopMirFunction> mirFunctions = mirModel_.functions();
        for (std::size_t functionOrdinal = 0; functionOrdinal < mirFunctions.size(); ++functionOrdinal)
        {
            if (!declareFunction(mirFunctions[functionOrdinal], functionOrdinal, error)) return false;
        }
        return true;
    }

bool OopLlvmImplementation::declareValueDropEntries(std::string& error)
{
        LLVMTypeRef pointer = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef parameters[] = {pointer};
        valueDropEntryFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameters, 1, 0);
        if (valueDropEntryFunctionType_ == nullptr)
            return fail(error, "NativeSharp 3.0 could not create the value drop-glue ABI");

        for (const frontend::ProjectOopMirStructLayout& layout : mirModel_.structLayouts())
        {
            if (!layout.requiresDrop || layout.dropGlueEntryName.empty()) continue;
            LLVMValueRef function = LLVMAddFunction(
                module_, layout.dropGlueEntryName.c_str(), valueDropEntryFunctionType_);
            if (function == nullptr)
                return fail(error, "NativeSharp 3.0 could not declare value drop glue '" +
                    layout.dropGlueEntryName + "'");
            applyUwtableAttribute(function);
            valueDropEntries_[layout.typeKey] = FunctionInfo{
                function, valueDropEntryFunctionType_, nullptr};
        }
        return true;
    }

bool OopLlvmImplementation::declareDestructionEntries(std::string& error)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef parameters[] = {ptr};
        destructionEntryFunctionType_ = LLVMFunctionType(
            LLVMVoidTypeInContext(context_), parameters, 1, 0);
        if (destructionEntryFunctionType_ == nullptr)
        {
            return fail(error, "Stage 35 failed to create destruction-entry function type");
        }

        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported) continue;
            if (table.destructionEntryName.empty() || table.objectDestructionEntryName.empty())
            {
                return fail(error, "item 319 class descriptor has incomplete destruction-entry names");
            }
            LLVMValueRef function = LLVMAddFunction(
                module_, table.destructionEntryName.c_str(), destructionEntryFunctionType_);
            if (function == nullptr)
            {
                return fail(error, "Stage 35 failed to declare destruction entry " +
                    table.destructionEntryName);
            }
            if (!partitioned_ || ownsModuleGlobals_)
                applyGenericOdrDefinition(function, table.genericSpecializationKey);
            const bool ownsDefinition = !partitioned_ || ownsModuleGlobals_;
            LLVMMetadataRef subprogram = nullptr;
            if (ownsDefinition && table.typeOrdinal < nameLookup_.namedTypes().size())
            {
                const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[table.typeOrdinal];
                const std::vector<std::string> parameters = {table.typeKey};
                const std::string displayName =
                    type.identity.fullyQualifiedName() + ".destruction-entry [generated]";
                subprogram = createSynthesizedDebugSubprogram(
                    type.range, displayName, table.destructionEntryName, "builtin:void",
                    parameters, true, false);
            }
            if (debugBuilder_ != nullptr && ownsDefinition && subprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for destruction entry " +
                    table.destructionEntryName);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);
            destructionFunctions_[table.typeOrdinal] = FunctionInfo{
                function, destructionEntryFunctionType_, subprogram};

            LLVMValueRef objectFunction = LLVMAddFunction(
                module_, table.objectDestructionEntryName.c_str(), destructionEntryFunctionType_);
            if (objectFunction == nullptr)
            {
                return fail(error, "item 319 failed to declare object-only destruction entry " +
                    table.objectDestructionEntryName);
            }
            if (!partitioned_ || ownsModuleGlobals_)
                applyGenericOdrDefinition(objectFunction, table.genericSpecializationKey);
            LLVMMetadataRef objectSubprogram = nullptr;
            if (ownsDefinition && table.typeOrdinal < nameLookup_.namedTypes().size())
            {
                const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[table.typeOrdinal];
                const std::vector<std::string> parameters = {table.typeKey};
                const std::string displayName =
                    type.identity.fullyQualifiedName() + ".object-destruction-entry [generated]";
                objectSubprogram = createSynthesizedDebugSubprogram(
                    type.range, displayName, table.objectDestructionEntryName, "builtin:void",
                    parameters, true, false);
            }
            if (debugBuilder_ != nullptr && ownsDefinition && objectSubprogram == nullptr)
            {
                return fail(error, "failed to create debug subprogram for object-only destruction entry " +
                    table.objectDestructionEntryName);
            }
            if (objectSubprogram != nullptr) LLVMSetSubprogram(objectFunction, objectSubprogram);
            objectDestructionFunctions_[table.typeOrdinal] = FunctionInfo{
                objectFunction, destructionEntryFunctionType_, objectSubprogram};
        }
        return true;
    }

bool OopLlvmImplementation::declarePartitionedDispatchMetadata(std::string& error)
{
        // Function code units need only the externally visible descriptor data and accessor
        // declarations used by construction/casts. Vtables, interface maps, type-name data
        // and accessor bodies have exactly one definition in the coordinator module.
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.descriptorAccessorName.empty())
            {
                return fail(error, "partitioned class has no descriptor accessor");
            }
            LLVMValueRef accessor = LLVMAddFunction(
                module_, table.descriptorAccessorName.c_str(), descriptorAccessorFunctionType_);
            if (accessor == nullptr)
            {
                return fail(error, "failed to declare partitioned class-descriptor accessor " +
                    table.descriptorAccessorName);
            }
            descriptorAccessorFunctions_[table.typeOrdinal] =
                FunctionInfo{accessor, descriptorAccessorFunctionType_};
            if (table.isImported) continue;
            LLVMValueRef descriptor = LLVMAddGlobal(
                module_, descriptorType_, table.descriptorName.c_str());
            if (descriptor == nullptr)
            {
                return fail(error, "failed to declare partitioned class descriptor " +
                    table.descriptorName);
            }
            LLVMSetLinkage(descriptor, LLVMExternalLinkage);
            descriptorGlobals_[table.typeOrdinal] = descriptor;
        }
        return true;
    }

bool OopLlvmImplementation::declareDispatchMetadataStorage(
        LLVMTypeRef ptr,
        LLVMTypeRef i64,
        std::string& error)
{
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported)
            {
                if (table.descriptorAccessorName.empty())
                {
                    return fail(error, "1.0-E1 imported class has no descriptor accessor");
                }
                LLVMValueRef accessor = LLVMAddFunction(
                    module_, table.descriptorAccessorName.c_str(), descriptorAccessorFunctionType_);
                if (accessor == nullptr)
                {
                    return fail(error, "1.0-E1 failed to declare imported class-descriptor accessor " +
                        table.descriptorAccessorName);
                }
                descriptorAccessorFunctions_[table.typeOrdinal] =
                    FunctionInfo{accessor, descriptorAccessorFunctionType_};
                continue;
            }
            if (!emitVtableGlobal(
                    module_, ptr, table,
                    [&](std::string_view callee) {
                        const auto found = functions_.find(std::string(callee));
                        return found == functions_.end() ? nullptr : found->second.function;
                    },
                    vtableGlobals_, error))
                return false;

            if (!emitInterfaceMapGlobals(
                    module_, ptr, i64, interfaceMapEntryType_, interfaceMapType_, table,
                    [&](std::string_view callee) {
                        const auto found = functions_.find(std::string(callee));
                        return found == functions_.end() ? nullptr : found->second.function;
                    },
                    interfaceMapGlobals_, error))
                return false;

            LLVMValueRef descriptor = LLVMAddGlobal(
                module_, descriptorType_, table.descriptorName.c_str());
            if (descriptor == nullptr)
            {
                return fail(error, "Macro A failed to declare class descriptor " + table.descriptorName);
            }
            descriptorGlobals_[table.typeOrdinal] = descriptor;
            applyGenericOdrDefinition(descriptor, table.genericSpecializationKey);

            if (table.descriptorAccessorName.empty())
            {
                return fail(error, "1.0-E1 local class has no descriptor-accessor name");
            }
            LLVMValueRef accessor = LLVMAddFunction(
                module_, table.descriptorAccessorName.c_str(), descriptorAccessorFunctionType_);
            if (accessor == nullptr)
            {
                return fail(error, "1.0-E1 failed to declare class-descriptor accessor " +
                    table.descriptorAccessorName);
            }
            descriptorAccessorFunctions_[table.typeOrdinal] =
                FunctionInfo{accessor, descriptorAccessorFunctionType_};
            applyGenericOdrDefinition(accessor, table.genericSpecializationKey);
        }
        return true;
    }

bool OopLlvmImplementation::defineDispatchMetadataAccessors(
        LLVMTypeRef ptr,
        LLVMTypeRef i64,
        std::string& error)
{
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            if (table.isImported) continue;
            if (!emitDescriptorInitializer(
                    module_, ptr, i64, descriptorType_, table,
                    runtimeDescriptorAccessorTypes_, descriptorGlobals_, vtableGlobals_,
                    [&](std::size_t ordinal) {
                        const auto found = destructionFunctions_.find(ordinal);
                        return found == destructionFunctions_.end()
                            ? nullptr : found->second.function;
                    },
                    [&](std::size_t ordinal) {
                        const auto found = objectDestructionFunctions_.find(ordinal);
                        return found == objectDestructionFunctions_.end()
                            ? nullptr : found->second.function;
                    },
                    interfaceMapGlobals_,
                    [&](const std::string& name, const std::string& text) {
                        return internTypeNameConstant(name, text);
                    },
                    error))
                return false;

            const auto descriptorFound = descriptorGlobals_.find(table.typeOrdinal);
            const auto accessorFound = descriptorAccessorFunctions_.find(table.typeOrdinal);
            if (accessorFound == descriptorAccessorFunctions_.end())
            {
                return fail(error, "1.0-E1 lost a declared class-descriptor accessor");
            }
            LLVMValueRef accessor = accessorFound->second.function;
            if (!emitDescriptorAccessorBody(
                    context_, descriptorType_, ptr, accessor, descriptorFound->second, table,
                    runtimeDescriptorAccessorTypes_.contains(table.typeOrdinal),
                    [&](std::size_t ordinal, LLVMTypeRef& functionType, LLVMValueRef& function) {
                        const auto found = descriptorAccessorFunctions_.find(ordinal);
                        if (found == descriptorAccessorFunctions_.end()) return false;
                        functionType = found->second.functionType;
                        function = found->second.function;
                        return true;
                    },
                    error))
                return false;
        }
        return true;
    }

bool OopLlvmImplementation::emitDispatchMetadata(std::string& error, bool defineStorage)
{
        // NSC305-L03. The carrier types live in createDispatchTypes (moved out as a pure move).
        if (!createDispatchTypes(
                context_, interfaceMapEntryType_, interfaceMapType_, descriptorType_,
                descriptorAccessorFunctionType_, error))
            return false;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        // NSC305-L03. The runtime-descriptor closure lives in computeRuntimeDescriptorAccessorTypes.
        computeRuntimeDescriptorAccessorTypes(mirModel_, runtimeDescriptorAccessorTypes_);
        if (!defineStorage) return declarePartitionedDispatchMetadata(error);
        if (!declareDispatchMetadataStorage(ptr, i64, error)) return false;
        return defineDispatchMetadataAccessors(ptr, i64, error);
    }

bool OopLlvmImplementation::defineRttiHelpers(std::string& error)
{
        if (descriptorType_ == nullptr || interfaceMapType_ == nullptr || interfaceMapEntryType_ == nullptr)
        {
            return fail(error, "Stage 39 RTTI helpers require completed descriptor/interface-map LLVM types");
        }

        if (!declareRttiHelpers(error)) return false;
        // NativeSharp 1.0.1 / composable libraries. These helpers inspect an object's public
        // descriptor shape but are implementation details of the module that emits the test.
        // External linkage made every independently compiled .lib define the same two strong
        // COFF symbols; the first real producer/consumer link therefore failed before main.
        // Keeping them internal is also stronger than COMDAT folding: no cross-module identity is
        // promised and each module may inline or optimize its own copy independently.
        LLVMSetLinkage(rttiClassTestFunction_, LLVMInternalLinkage);
        LLVMSetLinkage(rttiInterfaceTestFunction_, LLVMInternalLinkage);

        defineClassRttiHelper();
        defineInterfaceRttiHelper();
        currentFunction_ = nullptr;
        blockCounter_ = 0;
        return true;
    }

bool OopLlvmImplementation::declareRttiHelpers(std::string& error)
{
        LLVMTypeRef parameters[] = {
            LLVMPointerTypeInContext(context_, 0), LLVMInt64TypeInContext(context_)};
        rttiTestFunctionType_ = LLVMFunctionType(
            LLVMInt1TypeInContext(context_), parameters, 2, 0);
        rttiClassTestFunction_ = LLVMAddFunction(
            module_, "__nativesharp_rtti_is_class", rttiTestFunctionType_);
        rttiInterfaceTestFunction_ = LLVMAddFunction(
            module_, "__nativesharp_rtti_is_interface", rttiTestFunctionType_);
        if (rttiTestFunctionType_ == nullptr || rttiClassTestFunction_ == nullptr ||
            rttiInterfaceTestFunction_ == nullptr)
            return fail(error, "Stage 39 failed to declare RTTI helper functions");
        return true;
    }

void OopLlvmImplementation::defineClassRttiHelper()
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);
        currentFunction_ = rttiClassTestFunction_;
        blockCounter_ = 0;
        LLVMBasicBlockRef classEntry = appendBlock("rtti.class.entry");
        LLVMBasicBlockRef classStart = appendBlock("rtti.class.start");
        LLVMBasicBlockRef classLoop = appendBlock("rtti.class.loop");
        LLVMBasicBlockRef classProbe = appendBlock("rtti.class.probe");
        LLVMBasicBlockRef classNext = appendBlock("rtti.class.next");
        LLVMBasicBlockRef classMatch = appendBlock("rtti.class.match");
        LLVMBasicBlockRef classFail = appendBlock("rtti.class.fail");
        LLVMPositionBuilderAtEnd(builder_, classEntry);
        LLVMValueRef classObject = LLVMGetParam(rttiClassTestFunction_, 0);
        LLVMValueRef classTargetId = LLVMGetParam(rttiClassTestFunction_, 1);
        LLVMSetValueName2(classObject, "object", 6);
        LLVMSetValueName2(classTargetId, "target_type_id", 14);
        LLVMValueRef descriptorSlot = LLVMBuildAlloca(builder_, ptr, "descriptor.slot");
        LLVMValueRef classHasObject = LLVMBuildICmp(
            builder_, LLVMIntNE, classObject, LLVMConstNull(ptr), "object.nonnull");
        LLVMBuildCondBr(builder_, classHasObject, classStart, classFail);

        LLVMPositionBuilderAtEnd(builder_, classStart);
        LLVMValueRef firstDescriptor = LLVMBuildLoad2(builder_, ptr, classObject, "descriptor");
        LLVMBuildStore(builder_, firstDescriptor, descriptorSlot);
        LLVMBuildBr(builder_, classLoop);

        LLVMPositionBuilderAtEnd(builder_, classLoop);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, descriptorSlot, "descriptor.current");
        LLVMValueRef hasDescriptor = LLVMBuildICmp(
            builder_, LLVMIntNE, descriptor, LLVMConstNull(ptr), "descriptor.nonnull");
        LLVMBuildCondBr(builder_, hasDescriptor, classProbe, classFail);

        LLVMPositionBuilderAtEnd(builder_, classProbe);
        LLVMValueRef typeIdAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 3, "descriptor.type_id.addr");
        LLVMValueRef typeId = LLVMBuildLoad2(builder_, i64, typeIdAddress, "descriptor.type_id");
        LLVMValueRef classMatches = LLVMBuildICmp(
            builder_, LLVMIntEQ, typeId, classTargetId, "descriptor.matches");
        LLVMBuildCondBr(builder_, classMatches, classMatch, classNext);

        LLVMPositionBuilderAtEnd(builder_, classNext);
        LLVMValueRef baseAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 0, "descriptor.base.addr");
        LLVMValueRef baseDescriptor = LLVMBuildLoad2(builder_, ptr, baseAddress, "descriptor.base");
        LLVMBuildStore(builder_, baseDescriptor, descriptorSlot);
        LLVMBuildBr(builder_, classLoop);

        LLVMPositionBuilderAtEnd(builder_, classMatch);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 1, 0));
        LLVMPositionBuilderAtEnd(builder_, classFail);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 0, 0));
    }

void OopLlvmImplementation::defineInterfaceRttiHelper()
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);
        currentFunction_ = rttiInterfaceTestFunction_;
        blockCounter_ = 0;
        LLVMBasicBlockRef ifaceEntry = appendBlock("rtti.interface.entry");
        LLVMBasicBlockRef ifaceStart = appendBlock("rtti.interface.start");
        LLVMBasicBlockRef ifaceMap = appendBlock("rtti.interface.map");
        LLVMBasicBlockRef ifaceSearch = appendBlock("rtti.interface.search");
        LLVMBasicBlockRef ifaceProbe = appendBlock("rtti.interface.probe");
        LLVMBasicBlockRef ifaceNext = appendBlock("rtti.interface.next");
        LLVMBasicBlockRef ifaceMatch = appendBlock("rtti.interface.match");
        LLVMBasicBlockRef ifaceFail = appendBlock("rtti.interface.fail");
        LLVMPositionBuilderAtEnd(builder_, ifaceEntry);
        LLVMValueRef ifaceObject = LLVMGetParam(rttiInterfaceTestFunction_, 0);
        LLVMValueRef ifaceTargetId = LLVMGetParam(rttiInterfaceTestFunction_, 1);
        LLVMSetValueName2(ifaceObject, "object", 6);
        LLVMSetValueName2(ifaceTargetId, "target_interface_id", 19);
        LLVMValueRef ifaceHasObject = LLVMBuildICmp(
            builder_, LLVMIntNE, ifaceObject, LLVMConstNull(ptr), "object.nonnull");
        LLVMBuildCondBr(builder_, ifaceHasObject, ifaceStart, ifaceFail);

        LLVMValueRef count = nullptr;
        LLVMValueRef entries = nullptr;
        beginInterfaceRttiSearch(
            ifaceObject, ifaceStart, ifaceMap, ifaceFail, count, entries);
        LLVMBuildBr(builder_, ifaceSearch);

        LLVMPositionBuilderAtEnd(builder_, ifaceSearch);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "interface_map.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {ifaceMap};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef inRange = LLVMBuildICmp(
            builder_, LLVMIntULT, index, count, "interface_map.inrange");
        LLVMBuildCondBr(builder_, inRange, ifaceProbe, ifaceFail);

        LLVMPositionBuilderAtEnd(builder_, ifaceProbe);
        LLVMValueRef entryIndices[] = {index};
        LLVMValueRef entry = LLVMBuildGEP2(
            builder_, interfaceMapEntryType_, entries, entryIndices, 1, "interface_entry");
        LLVMValueRef idAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapEntryType_, entry, 0, "interface_entry.id.addr");
        LLVMValueRef id = LLVMBuildLoad2(builder_, i64, idAddress, "interface_entry.id");
        LLVMValueRef ifaceMatches = LLVMBuildICmp(
            builder_, LLVMIntEQ, id, ifaceTargetId, "interface_entry.matches");
        LLVMBuildCondBr(builder_, ifaceMatches, ifaceMatch, ifaceNext);

        LLVMPositionBuilderAtEnd(builder_, ifaceNext);
        LLVMValueRef one = LLVMConstInt(i64, 1, 0);
        LLVMValueRef nextIndex = LLVMBuildAdd(builder_, index, one, "interface_map.next");
        LLVMBuildBr(builder_, ifaceSearch);
        LLVMValueRef nextValues[] = {nextIndex};
        LLVMBasicBlockRef nextBlocks[] = {ifaceNext};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, ifaceMatch);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 1, 0));
        LLVMPositionBuilderAtEnd(builder_, ifaceFail);
        LLVMBuildRet(builder_, LLVMConstInt(i1, 0, 0));
    }

void OopLlvmImplementation::beginInterfaceRttiSearch(
        LLVMValueRef object,
        LLVMBasicBlockRef startBlock,
        LLVMBasicBlockRef mapBlock,
        LLVMBasicBlockRef failBlock,
        LLVMValueRef& count,
        LLVMValueRef& entries)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMPositionBuilderAtEnd(builder_, startBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, object, "descriptor");
        LLVMValueRef mapAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 4, "descriptor.interface_map.addr");
        LLVMValueRef map = LLVMBuildLoad2(builder_, ptr, mapAddress, "interface_map");
        LLVMValueRef hasMap = LLVMBuildICmp(
            builder_, LLVMIntNE, map, LLVMConstNull(ptr), "interface_map.nonnull");
        LLVMBuildCondBr(builder_, hasMap, mapBlock, failBlock);
        LLVMPositionBuilderAtEnd(builder_, mapBlock);
        LLVMValueRef countAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 0, "interface_map.count.addr");
        count = LLVMBuildLoad2(builder_, i64, countAddress, "interface_map.count");
        LLVMValueRef entriesAddress = LLVMBuildStructGEP2(
            builder_, interfaceMapType_, map, 1, "interface_map.entries.addr");
        entries = LLVMBuildLoad2(builder_, ptr, entriesAddress, "interface_map.entries");
    }

void OopLlvmImplementation::collectTypedCatchTypeOrdinals(
        const frontend::ProjectOopMirNode* node,
        std::vector<std::size_t>& ordinals) const
{
        if (node == nullptr) return;
        if (node->kind == frontend::ProjectOopMirNodeKind::CatchRegion &&
            !node->catchAll && node->exceptionTypeOrdinal.has_value() &&
            std::find(ordinals.begin(), ordinals.end(), *node->exceptionTypeOrdinal) == ordinals.end())
        {
            ordinals.push_back(*node->exceptionTypeOrdinal);
        }
        for (const auto& child : node->children)
            collectTypedCatchTypeOrdinals(child.get(), ordinals);
    }

bool OopLlvmImplementation::defineTypedCatchFilters(std::string& error)
{
        if (ehFilterFunctionType_ == nullptr || ehFilterFunction_ == nullptr ||
            exceptionCurrentFunctionType_ == nullptr || exceptionCurrentFunction_ == nullptr ||
            rttiTestFunctionType_ == nullptr || rttiClassTestFunction_ == nullptr)
        {
            return fail(error, "Stage 45.11 typed catch filters require runtime and RTTI helpers");
        }

        std::vector<std::size_t> typedCatchOrdinals;
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            const std::size_t ordinal = static_cast<std::size_t>(
                &function - mirModel_.functions().data());
            if (function.isTransportedInlineBody &&
                (ordinal >= reachableDefinitions_.size() || reachableDefinitions_[ordinal] == 0U))
                continue;
            if (function.body == nullptr) continue; // imported C declarations carry no body
            collectTypedCatchTypeOrdinals(function.body.get(), typedCatchOrdinals);
        }

        for (std::size_t typeOrdinal : typedCatchOrdinals)
        {
            const frontend::ProjectOopMirDispatchTable* table = nullptr;
            for (const frontend::ProjectOopMirDispatchTable& candidate : mirModel_.dispatchTables())
            {
                if (candidate.typeOrdinal == typeOrdinal)
                {
                    table = &candidate;
                    break;
                }
            }
            if (table == nullptr)
                return fail(error, "Stage 45.11 typed catch has no class descriptor dispatch table");
            if (!defineTypedCatchFilter(typeOrdinal, *table, error)) return false;
        }

        currentFunction_ = nullptr;
        currentDebugSubprogram_ = nullptr;
        currentDebugScope_ = nullptr;
        currentDebugFileKey_ = 0;
        blockCounter_ = 0;
        return true;
    }

LLVMMetadataRef OopLlvmImplementation::createTypedCatchFilterDebugSubprogram(
        std::size_t typeOrdinal,
        std::string_view name,
        SourceRange& debugRange)
{
        if (typeOrdinal >= nameLookup_.namedTypes().size()) return nullptr;
        const frontend::NamedTypeDeclaration& type = nameLookup_.namedTypes()[typeOrdinal];
        debugRange = type.range;
        const std::vector<std::string> parameters = {"builtin:void*", "builtin:void*"};
        const std::string displayName =
            type.identity.fullyQualifiedName() + ".catch-filter [generated]";
        return createSynthesizedDebugSubprogram(
            debugRange, displayName, name, "builtin:int", parameters, true, false);
    }

bool OopLlvmImplementation::defineTypedCatchFilter(
        std::size_t typeOrdinal,
        const frontend::ProjectOopMirDispatchTable& table,
        std::string& error)
{
        const std::string name = "__nativesharp_eh_filter_type_" +
            std::to_string(table.stableTypeId);
        LLVMValueRef function = LLVMAddFunction(module_, name.c_str(), ehFilterFunctionType_);
        if (function == nullptr)
            return fail(error, "Stage 45.11 failed to declare typed catch filter " + name);
        // A typed filter is referenced only by this module's own Win64 EH tables. It is not a
        // runtime ABI entry and must not compete with the identical implementation detail
        // emitted by another precompiled producer or consumer for the same caught type.
        LLVMSetLinkage(function, LLVMInternalLinkage);
        SourceRange debugRange;
        LLVMMetadataRef subprogram = createTypedCatchFilterDebugSubprogram(
            typeOrdinal, name, debugRange);
        if (debugBuilder_ != nullptr && subprogram == nullptr)
            return fail(error, "failed to create debug subprogram for typed catch filter " + name);
        if (subprogram != nullptr) LLVMSetSubprogram(function, subprogram);
        typedCatchFilters_[typeOrdinal] = function;
        // Multi-catch fix (see lowerTryRegion's catchswitch comment): the parent-side clause
        // dispatch tests the caught object with __nativesharp_rtti_is_class, which needs the
        // same stable type id this filter compiles in, so remember it here rather than
        // re-scanning the dispatch tables at every catch site.
        typedCatchTypeIds_[typeOrdinal] = table.stableTypeId;
        currentFunction_ = function;
        currentDebugSubprogram_ = subprogram;
        currentDebugScope_ = currentDebugSubprogram_;
        currentDebugFileKey_ = debugRange.isValid() ? debugRange.begin().fileId().value() : 0;
        blockCounter_ = 0;
        LLVMBasicBlockRef entry = appendBlock("eh.typed.filter.entry");
        LLVMBasicBlockRef typeCheck = appendBlock("eh.typed.filter.type");
        LLVMBasicBlockRef reject = appendBlock("eh.typed.filter.reject");
        LLVMPositionBuilderAtEnd(builder_, entry);
        ScopedDebugLocation generatedLocation(
            builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef domainArguments[] = {LLVMGetParam(function, 0), LLVMGetParam(function, 1)};
        LLVMValueRef domainResult = LLVMBuildCall2(
            builder_, ehFilterFunctionType_, ehFilterFunction_, domainArguments, 2, "eh.domain.match");
        LLVMValueRef domainMatches = LLVMBuildICmp(
            builder_, LLVMIntNE, domainResult, LLVMConstInt(i32, 0, 0), "eh.domain.matches");
        LLVMBuildCondBr(builder_, domainMatches, typeCheck, reject);
        LLVMPositionBuilderAtEnd(builder_, typeCheck);
        LLVMValueRef exception = LLVMBuildCall2(
            builder_, exceptionCurrentFunctionType_, exceptionCurrentFunction_,
            nullptr, 0, "eh.filter.exception");
        LLVMValueRef rttiArguments[] = {
            exception,
            LLVMConstInt(LLVMInt64TypeInContext(context_), table.stableTypeId, 0)};
        LLVMValueRef classMatches = LLVMBuildCall2(
            builder_, rttiTestFunctionType_, rttiClassTestFunction_,
            rttiArguments, 2, "eh.type.matches");
        LLVMValueRef filterResult = LLVMBuildZExt(
            builder_, classMatches, i32, "eh.type.filter.result");
        LLVMBuildRet(builder_, filterResult);
        LLVMPositionBuilderAtEnd(builder_, reject);
        LLVMBuildRet(builder_, LLVMConstInt(i32, 0, 0));
        return true;
    }

}
