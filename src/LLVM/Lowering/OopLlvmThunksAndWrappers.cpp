#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::defineFunctions(std::string& error)
{
        const std::span<const frontend::ProjectOopMirFunction> mirFunctions = mirModel_.functions();
        for (std::size_t functionOrdinal = 0; functionOrdinal < mirFunctions.size(); ++functionOrdinal)
        {
            const frontend::ProjectOopMirFunction& function = mirFunctions[functionOrdinal];
            if (function.isTransportedInlineBody &&
                (functionOrdinal >= reachableDefinitions_.size() ||
                 reachableDefinitions_[functionOrdinal] == 0U)) continue;
            // Interop Foundation: imported C functions stay DECLARATIONS -- declareFunctions gave
            // them their (ABI-mapped) prototype and the definition lives in the external DLL.
            // C1.2 "import identity" is the one exception: an import whose C symbol name is
            // claimed by two libraries has no import-library slot to be declared against, so it
            // gets a real body that binds the external symbol at first call.
            if (function.isImportedC || function.isImportedNative)
            {
                if (partitioned_ && !ownsModuleGlobals_) continue;
                // Same deduplication as declareFunctions: one identity, one body. The set is what
                // keeps a second declaration of the same import from redefining it.
                if (function.importRequiresRuntimeBinding &&
                    definedRuntimeBoundImports_.insert(function.mangledName).second &&
                    !defineRuntimeBoundImport(function, error))
                {
                    return false;
                }
                continue;
            }
            if (partitioned_ &&
                (ownsModuleGlobals_ || !functionDefinitions_[functionOrdinal]))
            {
                continue;
            }
            if (!defineFunction(function, error))
            {
                return false;
            }
        }
        return true;
    }

bool OopLlvmImplementation::defineCallbackGuardThunks(std::string& error, bool defineBodies)
{
        std::vector<std::pair<std::string, bool>> targets;
        const auto visit = [&](const frontend::ProjectOopMirNode* node, auto&& self) -> void
        {
            if (node == nullptr) return;
            if ((node->kind == frontend::ProjectOopMirNodeKind::DelegateCreate ||
                 node->kind == frontend::ProjectOopMirNodeKind::LambdaCreate) &&
                isExternCDelegateType(node->typeKey) && !node->calleeName.empty())
            {
                const std::pair<std::string, bool> target = {
                    node->calleeName, isVectorCallDelegateType(node->typeKey)};
                if (std::find(targets.begin(), targets.end(), target) == targets.end())
                    targets.push_back(target);
            }
            for (const auto& child : node->children) self(child.get(), self);
        };
        const std::span<const frontend::ProjectOopMirFunction> functions = mirModel_.functions();
        for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
        {
            if (functions[ordinal].isTransportedInlineBody &&
                (ordinal >= reachableDefinitions_.size() || reachableDefinitions_[ordinal] == 0U))
                continue;
            // A non-coordinator partition only needs declarations for callback thunks referenced
            // by bodies that it owns. Scanning every MIR body here used to be harmless because
            // every partition declared every function; after declaration pruning it asks this
            // module for unrelated callback targets that are intentionally absent.
            if (partitioned_ && !ownsModuleGlobals_ &&
                (ordinal >= functionDefinitions_.size() || !functionDefinitions_[ordinal]))
            {
                continue;
            }
            const frontend::ProjectOopMirFunction& function = functions[ordinal];
            visit(function.body.get(), visit);
            for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
                visit(cleanup.action, visit);
        }
        if (!partitioned_ || ownsModuleGlobals_)
        {
            for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
                visit(initializer.value.get(), visit);
        }
        for (const auto& [target, vectorCall] : targets)
        {
            if (callbackGuardThunk(target, error, vectorCall, defineBodies) == nullptr) return false;
        }
        return true;
    }

bool OopLlvmImplementation::emitCallbackBoundaryHandler(std::string& error)
{
        if (ehFilterFunction_ == nullptr || panicEhFilterFunction_ == nullptr ||
            trapFunction_ == nullptr)
        {
            return fail(error, "C1.3 callback boundary needs the SEH filter and trap");
        }
        LLVMValueRef catchSwitch = LLVMBuildCatchSwitch(
            builder_, nullptr, nullptr, 2, "boundary.catchswitch");
        if (catchSwitch == nullptr)
        {
            return fail(error, "C1.3 failed to create the callback boundary catchswitch");
        }
        const auto emitHandler = [&](LLVMValueRef filter, std::string_view stem) -> bool
        {
            LLVMBasicBlockRef padBlock = appendBlock(std::string(stem) + ".catchpad");
            LLVMAddHandler(catchSwitch, padBlock);
            LLVMPositionBuilderAtEnd(builder_, padBlock);
            LLVMValueRef catchArguments[] = {filter};
            LLVMValueRef catchPad = LLVMBuildCatchPad(
                builder_, catchSwitch, catchArguments, 1,
                (std::string(stem) + ".catch").c_str());
            if (catchPad == nullptr) return false;
            LLVMValueRef reason = LLVMConstInt(
                LLVMInt32TypeInContext(context_),
                static_cast<std::uint64_t>(LlvmTrapReason::CallbackExceptionEscape), 0);
            LLVMValueRef trapArguments[] = {reason};
            LLVMValueRef bundleArguments[] = {catchPad};
            LLVMOperandBundleRef bundle = LLVMCreateOperandBundle("funclet", 7, bundleArguments, 1);
            LLVMOperandBundleRef bundles[] = {bundle};
            LLVMBuildCallWithOperandBundles(
                builder_, trapFunctionType_, trapFunction_, trapArguments, 1, bundles, 1, "");
            LLVMDisposeOperandBundle(bundle);
            LLVMBuildUnreachable(builder_);
            return true;
        };
        if (!emitHandler(ehFilterFunction_, "boundary.exception") ||
            !emitHandler(panicEhFilterFunction_, "boundary.panic"))
        {
            return fail(error, "C1.3 failed to create the callback boundary catchpads");
        }
        return true;
    }

const frontend::ProjectOopMirFunction* OopLlvmImplementation::findCallbackMirFunction(
        std::string_view targetName) const noexcept
{
        return mirModel_.functionForMangledName(targetName);
    }

LLVMTypeRef OopLlvmImplementation::createCallbackGuardThunkType(
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool& internalSretReturn,
        bool& externalSretReturn,
        LLVMTypeRef& vectorReturnType,
        LLVMTypeRef& externalReturnType,
        std::string& error)
{
        internalSretReturn = function.abi.returnPassing ==
            frontend::ProjectOopMirAbiPassing::Indirect;
        vectorReturnType = vectorCall ? vectorCallAbiType(function.returnTypeKey) : nullptr;
        externalSretReturn = internalSretReturn && vectorReturnType == nullptr;
        externalReturnType = externalSretReturn
            ? LLVMVoidTypeInContext(context_)
            : (vectorReturnType != nullptr
                ? vectorReturnType : llvmType(function.returnTypeKey, error));
        if (externalReturnType == nullptr) return nullptr;
        std::vector<LLVMTypeRef> parameterTypes;
        parameterTypes.reserve(function.parameterTypeKeys.size() + 2U);
        if (externalSretReturn)
            parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        if (!function.isStatic)
            parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        for (const std::string& typeKey : function.parameterTypeKeys)
        {
            LLVMTypeRef type = vectorCall ? vectorCallAbiType(typeKey) : nullptr;
            if (type == nullptr) type = abiParameterType(typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_))
            {
                fail(error, "C1.3 callback boundary has invalid parameter type '" + typeKey + "'");
                return nullptr;
            }
            parameterTypes.push_back(type);
        }
        return LLVMFunctionType(
            externalReturnType, parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()), 0);
    }

void OopLlvmImplementation::applyCallbackGuardThunkAttributes(
        LLVMValueRef thunk,
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool externalSretReturn)
{
        if (externalSretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddAttributeAtIndex(thunk, index, attribute); },
                0U, function.returnTypeKey);
        }
        if (!vectorCall) return;
        unsigned externalPosition = externalSretReturn ? 1U : 0U;
        if (!function.isStatic) ++externalPosition;
        for (const std::string& typeKey : function.parameterTypeKeys)
        {
            if (vectorCallHvaArity(typeKey) != 0U)
            {
                applyInRegAttribute(
                    [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                    { LLVMAddAttributeAtIndex(thunk, index, attribute); },
                    externalPosition);
            }
            ++externalPosition;
        }
    }

bool OopLlvmImplementation::prepareCallbackGuardArguments(
        LLVMValueRef thunk,
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool internalSretReturn,
        bool externalSretReturn,
        std::vector<LLVMValueRef>& forwarded,
        LLVMValueRef& internalSretSlot,
        std::string& error)
{
        forwarded.reserve(function.parameterTypeKeys.size() + 2U);
        unsigned externalIndex = 0U;
        internalSretSlot = nullptr;
        if (internalSretReturn)
        {
            if (externalSretReturn) internalSretSlot = LLVMGetParam(thunk, externalIndex++);
            else
            {
                LLVMTypeRef carrier = llvmType(function.returnTypeKey, error);
                if (carrier == nullptr) return false;
                internalSretSlot = entryBlockAlloca(
                    carrier, structAlignmentFor(function.returnTypeKey), "vectorcall.sret.bridge");
            }
            forwarded.push_back(internalSretSlot);
        }
        if (!function.isStatic) forwarded.push_back(LLVMGetParam(thunk, externalIndex++));
        for (const std::string& typeKey : function.parameterTypeKeys)
        {
            LLVMValueRef value = LLVMGetParam(thunk, externalIndex++);
            if (vectorCall && vectorCallAbiType(typeKey) != nullptr)
            {
                value = vectorCallAbiToCarrier(value, typeKey, error);
                if (value == nullptr) return false;
                value = wrapIndirectArgument(value, typeKey, error);
                if (value == nullptr) return false;
            }
            forwarded.push_back(value);
        }
        return true;
    }

bool OopLlvmImplementation::defineCallbackGuardThunkBody(
        LLVMValueRef thunk,
        const FunctionInfo& target,
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool internalSretReturn,
        bool externalSretReturn,
        LLVMTypeRef vectorReturnType,
        LLVMMetadataRef subprogram,
        std::string& error)
{
        LLVMValueRef savedFunction = currentFunction_;
        const std::size_t savedBlockCounter = blockCounter_;
        LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(builder_);
        const auto restore = [&]()
        {
            currentFunction_ = savedFunction;
            blockCounter_ = savedBlockCounter;
            if (savedBlock != nullptr) LLVMPositionBuilderAtEnd(builder_, savedBlock);
        };
        currentFunction_ = thunk;
        blockCounter_ = 0;
        ScopedDebugLocation generatedDebugLocation(
            builder_, debugSubprogramLocation(function, subprogram));
        LLVMBasicBlockRef entryBlock = appendBlock("boundary.entry");
        LLVMBasicBlockRef returnBlock = appendBlock("boundary.return");
        LLVMBasicBlockRef unwindBlock = appendBlock("boundary.unwind");
        LLVMPositionBuilderAtEnd(builder_, entryBlock);
        std::vector<LLVMValueRef> forwarded;
        LLVMValueRef internalSretSlot = nullptr;
        if (!prepareCallbackGuardArguments(
                thunk, function, vectorCall, internalSretReturn, externalSretReturn,
                forwarded, internalSretSlot, error)) return false;
        const bool internalReturnsVoid = internalSretReturn || function.returnTypeKey == "builtin:void";
        LLVMValueRef result = LLVMBuildInvoke2(
            builder_, target.functionType, target.function,
            forwarded.empty() ? nullptr : forwarded.data(), static_cast<unsigned>(forwarded.size()),
            returnBlock, unwindBlock, internalReturnsVoid ? "" : "boundary.result");
        if (result == nullptr)
        {
            restore();
            return fail(error, "C1.3 failed to emit the guarded callback invoke");
        }
        if (internalSretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(result, index, attribute); },
                0U, function.returnTypeKey);
        }
        LLVMPositionBuilderAtEnd(builder_, returnBlock);
        if (vectorReturnType != nullptr)
        {
            LLVMValueRef externalResult = LLVMBuildLoad2(
                builder_, vectorReturnType, internalSretSlot, "vectorcall.return.bridge");
            LLVMBuildRet(builder_, externalResult);
        }
        else if (externalSretReturn || function.returnTypeKey == "builtin:void")
            LLVMBuildRetVoid(builder_);
        else LLVMBuildRet(builder_, result);
        LLVMPositionBuilderAtEnd(builder_, unwindBlock);
        if (!emitCallbackBoundaryHandler(error))
        {
            restore();
            return false;
        }
        restore();
        return true;
    }

LLVMValueRef OopLlvmImplementation::callbackGuardThunk(
        const std::string& targetName,
        std::string& error,
        bool vectorCall,
        bool defineBody )
{
        const std::string cacheKey = targetName + (vectorCall ? "|vectorcall" : "|c");
        const auto cached = callbackGuardThunks_.find(cacheKey);
        if (cached != callbackGuardThunks_.end()) return cached->second.function;
        const auto found = functions_.find(targetName);
        if (found == functions_.end() || found->second.function == nullptr)
        {
            return failValue(error, "C1.3 callback boundary target is unavailable: " + targetName);
        }
        if (sehPersonalityFunction_ == nullptr || ehFilterFunction_ == nullptr ||
            trapFunction_ == nullptr)
        {
            return failValue(error, "C1.3 callback boundary needs the SEH personality and filter");
        }
        const frontend::ProjectOopMirFunction* mirFunction = findCallbackMirFunction(targetName);
        if (mirFunction == nullptr)
        {
            return failValue(error, "C1.3 callback boundary lost the target's MIR record: " + targetName);
        }

        // Item 345: the NativeSharp body deliberately keeps the stable aggregate carrier used by
        // every ordinary call.  A vectorcall boundary is different: Float32x4 and an HVA of one
        // to four Float32x4 fields travel as LLVM vectors / homogeneous vector structs.  Build the
        // public thunk type from those rules instead of reusing the body's ordinary Win64 type.
        bool internalSretReturn = false;
        bool externalSretReturn = false;
        LLVMTypeRef vectorReturnType = nullptr;
        LLVMTypeRef externalReturnType = nullptr;
        LLVMTypeRef thunkType = createCallbackGuardThunkType(
            *mirFunction, vectorCall, internalSretReturn, externalSretReturn,
            vectorReturnType, externalReturnType, error);
        if (thunkType == nullptr) return nullptr;
        const std::string thunkName = targetName +
            (vectorCall ? "__vectorcall_boundary" : "__c_boundary");
        LLVMValueRef thunk = LLVMAddFunction(module_, thunkName.c_str(), thunkType);
        if (thunk == nullptr)
        {
            return failValue(error, "C1.3 failed to declare the callback boundary thunk for " + targetName);
        }
        if (vectorCall) LLVMSetFunctionCallConv(thunk, LLVMX86VectorCallCallConv);
        applyUwtableAttribute(thunk);
        // Whatever ABI attributes the external declaration carries (sret or HVA inreg) must be
        // reproduced on the thunk, because the thunk is what native code sees.
        LLVMMetadataRef subprogram = nullptr;
        if (defineBody)
        {
            subprogram = createGeneratedDebugSubprogram(
                *mirFunction, "C callback boundary", thunkName, true);
            if (debugBuilder_ != nullptr && subprogram == nullptr)
            {
                return failValue(error, "failed to create debug subprogram for callback boundary " +
                    thunkName);
            }
            if (subprogram != nullptr) LLVMSetSubprogram(thunk, subprogram);
        }
        applyCallbackGuardThunkAttributes(
            thunk, *mirFunction, vectorCall, externalSretReturn);
        if (!defineBody)
        {
            callbackGuardThunks_[cacheKey] = FunctionInfo{thunk, thunkType, nullptr};
            return thunk;
        }
        LLVMSetPersonalityFn(thunk, sehPersonalityFunction_);
        if (!defineCallbackGuardThunkBody(
                thunk, found->second, *mirFunction, vectorCall, internalSretReturn,
                externalSretReturn, vectorReturnType, subprogram, error)) return nullptr;
        callbackGuardThunks_[cacheKey] = FunctionInfo{thunk, thunkType, subprogram};
        return thunk;
    }

LLVMValueRef OopLlvmImplementation::cStringGlobal(std::string_view text, std::string_view stem)
{
        const std::string key(text);
        const auto found = cStringGlobals_.find(key);
        if (found != cStringGlobals_.end()) return found->second;
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef bytesType = LLVMArrayType2(i8, text.size() + 1U);
        LLVMValueRef initializer = LLVMConstStringInContext2(context_, text.data(), text.size(), 0);
        const std::string name =
            "__nsc_cstr_" + std::string(stem) + "_" + std::to_string(cStringSequence_++);
        LLVMValueRef global = LLVMAddGlobal(module_, bytesType, name.c_str());
        if (global == nullptr || initializer == nullptr) return nullptr;
        LLVMSetInitializer(global, initializer);
        LLVMSetGlobalConstant(global, 1);
        LLVMSetLinkage(global, LLVMPrivateLinkage);
        cStringGlobals_.emplace(key, global);
        return global;
    }

bool OopLlvmImplementation::defineRuntimeBoundImport(
        const frontend::ProjectOopMirFunction& function, std::string& error)
{
        const auto found = functions_.find(function.mangledName);
        if (found == functions_.end() || found->second.function == nullptr)
        {
            return fail(error, "C1.2 runtime-bound import lost its declaration: " + function.mangledName);
        }
        if (function.importDllName.empty() || function.cAbiName.empty())
        {
            return fail(error, "C1.2 runtime-bound import lost its (library, symbol) identity");
        }
        LLVMValueRef importFunction = found->second.function;
        LLVMTypeRef importType = found->second.functionType;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);

        if (!ensureRuntimeImportBindingFunctions(error)) return false;

        LLVMValueRef libraryName = cStringGlobal(function.importDllName, "dll");
        LLVMValueRef symbolName = cStringGlobal(function.cAbiName, "sym");
        if (libraryName == nullptr || symbolName == nullptr)
        {
            return fail(error, "C1.2 failed to materialize the import identity strings");
        }
        LLVMValueRef cache = LLVMAddGlobal(module_, ptr, (function.mangledName + "__address").c_str());
        if (cache == nullptr)
        {
            return fail(error, "C1.2 failed to declare the runtime import address cache");
        }
        LLVMSetInitializer(cache, LLVMConstNull(ptr));
        LLVMSetLinkage(cache, LLVMPrivateLinkage);

        currentFunction_ = importFunction;
        blockCounter_ = 0;
        LLVMBasicBlockRef entryBlock = appendBlock("import.entry");
        LLVMBasicBlockRef bindBlock = appendBlock("import.bind");
        LLVMBasicBlockRef callBlock = appendBlock("import.call");
        LLVMPositionBuilderAtEnd(builder_, entryBlock);
        LLVMValueRef cached = LLVMBuildLoad2(builder_, ptr, cache, "import.cached");
        LLVMValueRef needsBinding =
            LLVMBuildICmp(builder_, LLVMIntEQ, cached, LLVMConstNull(ptr), "import.unbound");
        LLVMBuildCondBr(builder_, needsBinding, bindBlock, callBlock);

        LLVMPositionBuilderAtEnd(builder_, bindBlock);
        LLVMValueRef loadArguments[] = {libraryName};
        LLVMValueRef moduleHandle = LLVMBuildCall2(
            builder_, loadLibraryFunctionType_, loadLibraryFunction_, loadArguments, 1,
            "import.module");
        LLVMValueRef procArguments[] = {moduleHandle, symbolName};
        LLVMValueRef address = LLVMBuildCall2(
            builder_, getProcAddressFunctionType_, getProcAddressFunction_, procArguments, 2,
            "import.address");
        LLVMBuildStore(builder_, address, cache);
        LLVMBuildBr(builder_, callBlock);

        LLVMPositionBuilderAtEnd(builder_, callBlock);
        LLVMValueRef target = LLVMBuildPhi(builder_, ptr, "import.target");
        LLVMValueRef incomingValues[] = {cached, address};
        LLVMBasicBlockRef incomingBlocks[] = {entryBlock, bindBlock};
        LLVMAddIncoming(target, incomingValues, incomingBlocks, 2);
        // A null here means the library or the symbol is genuinely absent at run time. Trapping is
        // the only honest answer: an indirect call through null would be an access violation with
        // no attribution, and there is no error channel in the imported function's own C
        // signature to report it through.
        if (!emitGuard(
                LLVMBuildICmp(builder_, LLVMIntNE, target, LLVMConstNull(ptr), "import.bound"),
                LlvmTrapReason::ImportBindingFailure, "import.binding", error))
        {
            return false;
        }

        return emitRuntimeBoundForwarding(function, importFunction, importType, target, error);
    }

bool OopLlvmImplementation::ensureRuntimeImportBindingFunctions(std::string& error)
{
        if (loadLibraryFunction_ != nullptr) return true;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef loadLibraryParameters[] = {ptr};
        loadLibraryFunctionType_ = LLVMFunctionType(ptr, loadLibraryParameters, 1, 0);
        loadLibraryFunction_ = LLVMAddFunction(module_, "LoadLibraryA", loadLibraryFunctionType_);
        LLVMTypeRef getProcAddressParameters[] = {ptr, ptr};
        getProcAddressFunctionType_ = LLVMFunctionType(ptr, getProcAddressParameters, 2, 0);
        getProcAddressFunction_ =
            LLVMAddFunction(module_, "GetProcAddress", getProcAddressFunctionType_);
        if (loadLibraryFunction_ == nullptr || getProcAddressFunction_ == nullptr)
            return fail(error, "C1.2 failed to declare the runtime import-binding entry points");
        applyFunctionAttribute(loadLibraryFunction_, "nounwind");
        applyFunctionAttribute(getProcAddressFunction_, "nounwind");
        return true;
    }

bool OopLlvmImplementation::emitRuntimeBoundForwarding(
        const frontend::ProjectOopMirFunction& function,
        LLVMValueRef importFunction,
        LLVMTypeRef importType,
        LLVMValueRef target,
        std::string& error)
{
        // The prototype declareFunctions built for this import, reconstructed from the same MIR
        // facts rather than read back off the LLVM function: an import is always static (no
        // `this`), a leading sret pointer when the return travels indirectly, then one parameter
        // per declared parameter. Forwarding every one of them unchanged forwards the ABI
        // unchanged.
        const bool sretReturn = function.abi.returnPassing ==
            frontend::ProjectOopMirAbiPassing::Indirect;
        const unsigned parameterCount = static_cast<unsigned>(
            function.parameterTypeKeys.size() + (sretReturn ? 1U : 0U));
        std::vector<LLVMValueRef> forwarded(parameterCount);
        for (unsigned index = 0; index < parameterCount; ++index)
        {
            forwarded[index] = LLVMGetParam(importFunction, index);
        }
        const bool returnsVoid = sretReturn || function.returnTypeKey == "builtin:void";
        LLVMValueRef result = LLVMBuildCall2(
            builder_, importType, target, forwarded.empty() ? nullptr : forwarded.data(),
            parameterCount, returnsVoid ? "" : "import.result");
        if (result == nullptr)
        {
            return fail(error, "C1.2 failed to emit the runtime-bound import forwarding call");
        }
        // Win64 aggregate ABI: an indirect struct return travels through a leading sret pointer.
        // The declaration carries the attribute; the forwarding call site must carry it too, so
        // the two agree about which pointer is the return slot.
        if (sretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(result, index, attribute); },
                0U, function.returnTypeKey);
        }
        if (returnsVoid)
        {
            LLVMBuildRetVoid(builder_);
        }
        else
        {
            LLVMBuildRet(builder_, result);
        }
        currentFunction_ = nullptr;
        return true;
    }

bool OopLlvmImplementation::isExternCDelegateType(std::string_view typeKey) const noexcept
{
        if (const std::optional<language::FunctionType> functionType =
                language::parseFunctionTypeIdentity(typeKey))
            return functionType->carrier == language::FunctionCarrier::Pointer;
        if (!typeKey.starts_with("type:")) return false;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() != key) continue;
            return type.kind == frontend::NamedTypeDeclarationKind::Delegate &&
                type.syntax != nullptr && type.syntax->hasModifier(lexer::TokenKind::KwExtern);
        }
        return false;
    }

bool OopLlvmImplementation::isVectorCallDelegateType(std::string_view typeKey) const noexcept
{
        if (const std::optional<language::FunctionType> functionType =
                language::parseFunctionTypeIdentity(typeKey))
            return functionType->callingConvention == language::CallingConvention::VectorCall;
        if (!typeKey.starts_with("type:")) return false;
        const std::string_view key = typeKey.substr(5);
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() != key) continue;
            return type.kind == frontend::NamedTypeDeclarationKind::Delegate &&
                type.syntax != nullptr &&
                type.syntax->hasModifier(lexer::TokenKind::KwVectorCall);
        }
        return false;
    }

bool OopLlvmImplementation::defineCAbiWrappers(std::string& error)
{
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            if (!function.isExportedC) continue;
            if (!function.isStatic || function.cAbiName.empty())
            {
                return fail(error, "Stage 47 C ABI wrapper lost static/export metadata");
            }
            const auto internal = functions_.find(function.mangledName);
            if (internal == functions_.end())
            {
                return fail(error, "Stage 47 C ABI wrapper lost internal function " + function.mangledName);
            }
            if (!defineCAbiWrapper(function, internal->second, error)) return false;
        }
        currentFunction_ = nullptr;
        currentMirFunction_ = nullptr;
        blockCounter_ = 0;
        return true;
    }

LLVMValueRef OopLlvmImplementation::createCAbiWrapper(
        const frontend::ProjectOopMirFunction& function,
        bool sretReturn,
        LLVMTypeRef& returnType,
        LLVMMetadataRef& subprogram,
        std::string& error)
{
        // Win64 aggregate ABI at the EXPORT boundary: the wrapper's C signature IS the
        // contract an MSVC-compiled host sees, so it takes the same shape clang gives a C
        // function -- indirect struct parameters as plain pointers, an indirect struct
        // return as a leading sret pointer. Verified against a real clang-compiled C host
        // in Stage48HostAbi tests.
        returnType = sretReturn ? LLVMVoidTypeInContext(context_) : llvmType(function.returnTypeKey, error);
        if (returnType == nullptr) return nullptr;
        // C1.1 "function-pointer round trip": an extern C delegate IS a raw function pointer
        // (llvmType maps it to `ptr`; lowerDelegateCreate yields the target's own address), so
        // returning one through the wrapper needs no special case -- the hard refusal that
        // used to sit here was the backend half of a frontend rule now narrowed to ordinary
        // (cell-shaped) delegates. See ProjectMemberModel's C ABI return-type check.
        std::vector<LLVMTypeRef> parameterTypes;
        parameterTypes.reserve(function.parameterTypeKeys.size() + 1);
        if (sretReturn) parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        for (const std::string& typeKey : function.parameterTypeKeys)
        {
            LLVMTypeRef type = abiParameterType(typeKey, error);
            if (type == nullptr || type == LLVMVoidTypeInContext(context_)) return nullptr;
            parameterTypes.push_back(type);
        }
        LLVMTypeRef wrapperType = LLVMFunctionType(
            returnType, parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()), 0);
        LLVMValueRef wrapper = LLVMAddFunction(module_, function.cAbiName.c_str(), wrapperType);
        if (wrapper == nullptr)
            return failValue(error, "Stage 47 failed to declare C ABI wrapper " + function.cAbiName);
        subprogram = createGeneratedDebugSubprogram(function, "C ABI wrapper", function.cAbiName, false);
        if (debugBuilder_ != nullptr && subprogram == nullptr)
        {
            return failValue(
                error, "failed to create debug subprogram for C ABI wrapper " + function.cAbiName);
        }
        if (subprogram != nullptr) LLVMSetSubprogram(wrapper, subprogram);
        if (sretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddAttributeAtIndex(wrapper, index, attribute); },
                0U, function.returnTypeKey);
        }
        return wrapper;
    }

bool OopLlvmImplementation::defineCAbiWrapper(
        const frontend::ProjectOopMirFunction& function,
        const FunctionInfo& internal,
        std::string& error)
{
        const bool sretReturn = function.abi.returnPassing ==
            frontend::ProjectOopMirAbiPassing::Indirect;
        LLVMTypeRef returnType = nullptr;
        LLVMMetadataRef subprogram = nullptr;
        LLVMValueRef wrapper = createCAbiWrapper(
            function, sretReturn, returnType, subprogram, error);
        if (wrapper == nullptr) return false;
        currentFunction_ = wrapper;
        currentMirFunction_ = nullptr;
        blockCounter_ = 0;
        ScopedDebugLocation generatedDebugLocation(
            builder_, debugSubprogramLocation(function, subprogram));
        LLVMBasicBlockRef entry = appendBlock("cabi.entry");
        LLVMPositionBuilderAtEnd(builder_, entry);
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(function.parameterTypeKeys.size() + 1);
        // The internal function shares this exact ABI (declareFunctions applies the same
        // mapping), so both the host's argument pointers and the host's sret slot are
        // forwarded as-is: the per-call copies the C caller made belong to this call chain,
        // and nothing here reads them after the internal call returns.
        if (sretReturn) arguments.push_back(LLVMGetParam(wrapper, 0));
        const unsigned wrapperArgumentBase = sretReturn ? 1U : 0U;
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
        {
            // extern C delegates are native C function pointers end-to-end.
            // NativeSharp's ordinary delegate cell representation is deliberately not used here.
            arguments.push_back(LLVMGetParam(
                wrapper, wrapperArgumentBase + static_cast<unsigned>(index)));
        }
        // C1.3 "callback exception boundary": an `export extern C` entry point is the OTHER
        // door foreign code enters through, and it carries exactly the same hazard as a
        // callback -- a NativeSharp exception escaping here would unwind the HOST's frames
        // with this compiler's SEH exception code. The wrapper already exists as its own
        // function, so the guard is the same catchpad the callback thunk uses, emitted right
        // here: invoke instead of call, one catchpad filtered by __nativesharp_eh_filter (so
        // a genuine foreign SEH exception still propagates untouched), trap on catch.
        LLVMBasicBlockRef returnBlock = appendBlock("cabi.return");
        LLVMBasicBlockRef unwindBlock = appendBlock("cabi.unwind");
        LLVMSetPersonalityFn(wrapper, sehPersonalityFunction_);
        LLVMValueRef result = LLVMBuildInvoke2(
            builder_, internal.functionType, internal.function,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()), returnBlock, unwindBlock,
            returnType == LLVMVoidTypeInContext(context_) ? "" : "cabi.result");
        if (result == nullptr)
            return fail(error, "Stage 47 failed to emit the guarded C ABI wrapper call");
        if (sretReturn)
        {
            applySretAttributes(
                [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
                { LLVMAddCallSiteAttribute(result, index, attribute); },
                0U, function.returnTypeKey);
        }
        LLVMPositionBuilderAtEnd(builder_, returnBlock);
        if (returnType == LLVMVoidTypeInContext(context_)) LLVMBuildRetVoid(builder_);
        else LLVMBuildRet(builder_, result);
        LLVMPositionBuilderAtEnd(builder_, unwindBlock);
        return emitCallbackBoundaryHandler(error);
    }

bool OopLlvmImplementation::defineValueDropEntries(std::string& error)
{
        for (const frontend::ProjectOopMirStructLayout& layout : mirModel_.structLayouts())
        {
            if (!layout.requiresDrop || layout.isImported || layout.dropGlueEntryName.empty())
                continue;
            const auto found = valueDropEntries_.find(layout.typeKey);
            if (found == valueDropEntries_.end())
                return fail(error, "NativeSharp 3.0 lost declared value drop glue for '" +
                    layout.typeKey + "'");

            currentFunction_ = found->second.function;
            currentMirFunction_ = nullptr;
            thisValue_ = nullptr;
            closureEnvironment_ = nullptr;
            constructionThisOverride_ = nullptr;
            parameterStorage_.clear();
            localStorage_.clear();
            parameterValueDropFlags_.clear();
            localValueDropFlags_.clear();
            activeCatchExceptions_.clear();
            loopStack_.clear();
            exceptionDispatchBlocks_.clear();
            cleanupRegions_.clear();
            cleanupChainCache_.clear();
            activeCleanupRegions_.clear();
            exceptionHandlerStack_.clear();
            funcletStack_.clear();
            returnStorage_ = nullptr;
            sretStorage_ = nullptr;
            blockCounter_ = 0;

            LLVMBasicBlockRef entry = appendBlock("value.drop.entry");
            LLVMPositionBuilderAtEnd(builder_, entry);
            LLVMValueRef storage = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(storage, "storage", 7);
            if (layout.typeKey.starts_with("iterator:") ||
                layout.typeKey.starts_with("async-iterator:"))
            {
                // NS304-T06-F07: the iterator's drop glue dispatches through its Drop function
                // pointer (field 4, offset 32) — the per-generator Gen$drop, not field drops.
                LLVMValueRef dropFn = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(storage, 32, "drop.fn.addr"), "drop.fn");
                LLVMTypeRef fnParams[] = { LLVMPointerTypeInContext(context_, 0) };
                LLVMTypeRef fnType = LLVMFunctionType(LLVMVoidTypeInContext(context_), fnParams, 1, 0);
                LLVMValueRef args[] = { storage };
                if (LLVMBuildCall2(builder_, fnType, dropFn, args, 1U, "") == nullptr)
                    return fail(error, "iterator drop glue call failed");
                LLVMBuildRetVoid(builder_);
            }
            else if (!emitValueDropAddress(
                    storage, layout.typeKey, "value.drop", error, false))
                return false;
            if (!terminated()) LLVMBuildRetVoid(builder_);
            applyOdrDefinition(currentFunction_);
        }
        currentFunction_ = nullptr;
        currentMirFunction_ = nullptr;
        blockCounter_ = 0;
        return true;
    }

LLVMValueRef OopLlvmImplementation::beginObjectDestructionEntry(
        const frontend::ProjectOopMirDispatchTable& table,
        const FunctionInfo& objectEntry,
        SourceRange& debugRange)
{
        // The object-only entry owns the complete destructor/owned-field/base traversal.
        // The traditional entry below is now a thin wrapper that invokes it and releases
        // the outermost storage exactly once.
        currentFunction_ = objectEntry.function;
        currentMirFunction_ = nullptr;
        thisValue_ = nullptr;
        closureEnvironment_ = nullptr;
        constructionThisOverride_ = nullptr;
        parameterStorage_.clear();
        localStorage_.clear();
        parameterValueDropFlags_.clear();
        localValueDropFlags_.clear();
        activeCatchExceptions_.clear();
        loopStack_.clear();
        exceptionDispatchBlocks_.clear();
        cleanupRegions_.clear();
        cleanupChainCache_.clear();
        activeCleanupRegions_.clear();
        exceptionHandlerStack_.clear();
        funcletStack_.clear();
        returnStorage_ = nullptr;
        sretStorage_ = nullptr;
        blockCounter_ = 0;
        currentDebugSubprogram_ = objectEntry.subprogram;
        currentDebugScope_ = currentDebugSubprogram_;
        debugDeclaredLocals_.clear();
        currentDebugFileKey_ = 0;
        if (table.typeOrdinal < nameLookup_.namedTypes().size())
        {
            debugRange = nameLookup_.namedTypes()[table.typeOrdinal].range;
            if (debugRange.isValid()) currentDebugFileKey_ = debugRange.begin().fileId().value();
        }
        LLVMBasicBlockRef entry = appendBlock("destroy.entry");
        LLVMPositionBuilderAtEnd(builder_, entry);
        LLVMValueRef object = LLVMGetParam(currentFunction_, 0);
        LLVMSetValueName2(object, "object", 6);
        return object;
    }

bool OopLlvmImplementation::emitObjectDestructionSteps(
        const frontend::ProjectOopMirDispatchTable& table,
        LLVMValueRef object,
        std::string& error)
{
        for (const frontend::ProjectOopMirDispatchTable::DestructionStep& step : table.destructionSteps)
        {
            if (!step.isFieldRelease)
            {
                const auto bodyFound = functions_.find(step.destructorCalleeName);
                if (bodyFound == functions_.end())
                    return fail(error, "Stage 35 destructor body was not declared: " + step.destructorCalleeName);
                LLVMValueRef arguments[] = {object};
                buildNoUnwindCall(
                    bodyFound->second.functionType, bodyFound->second.function, arguments, 1, "");
                continue;
            }
            // Release one owned field: null-guarded, cleared before its destructor runs --
            // the same protocol scoped owners use (see emitCleanupGuardBranch). The null
            // guard is what makes this correct when a destructor body already released the
            // field by hand (it stored null through `field = null` or move-out), and it is
            // idempotent under exceptional re-entry for the same reason.
            const frontend::ProjectOopMirDispatchTable::OwnedField& ownedField = step.ownedField;
            LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
            LLVMValueRef offset = LLVMConstInt(
                LLVMInt64TypeInContext(context_), ownedField.offset, 0);
            LLVMValueRef indices[] = {offset};
            LLVMValueRef fieldAddress = LLVMBuildGEP2(
                builder_, LLVMInt8TypeInContext(context_), object, indices, 1, "owned.field.addr");
            LLVMValueRef fieldValue = LLVMBuildLoad2(builder_, ptr, fieldAddress, "owned.field.value");
            LLVMValueRef fieldIsNull = LLVMBuildICmp(
                builder_, LLVMIntEQ, fieldValue, LLVMConstNull(ptr), "owned.field.isnull");
            LLVMBasicBlockRef destroyBlock = appendBlock("owned.field.destroy");
            LLVMBasicBlockRef doneBlock = appendBlock("owned.field.done");
            LLVMBuildCondBr(builder_, fieldIsNull, doneBlock, destroyBlock);
            LLVMPositionBuilderAtEnd(builder_, destroyBlock);
            LLVMBuildStore(builder_, LLVMConstNull(ptr), fieldAddress);
            if (ownedField.isNativeString)
            {
                LLVMValueRef arguments[] = {fieldValue};
                buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            }
            else
            {
                if (descriptorType_ == nullptr || destructionEntryFunctionType_ == nullptr)
                    return fail(error, "Bloque 1 owned field has no class-descriptor destruction ABI");
                // Dispatched through the field's RUNTIME descriptor, not its declared type, so a
                // field declared as a base type destroys the derived object it actually holds.
                LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, fieldValue, "owned.field.descriptor");
                LLVMValueRef entryAddress = LLVMBuildStructGEP2(
                    builder_, descriptorType_, descriptor, 5, "owned.field.entry.addr");
                LLVMValueRef destructionEntry = LLVMBuildLoad2(
                    builder_, ptr, entryAddress, "owned.field.entry");
                LLVMValueRef arguments[] = {fieldValue};
                buildNoUnwindCall(
                    destructionEntryFunctionType_, destructionEntry, arguments, 1, "");
            }
            LLVMBuildBr(builder_, doneBlock);
            LLVMPositionBuilderAtEnd(builder_, doneBlock);
        }
        return true;
    }

bool OopLlvmImplementation::emitDelegatedBaseDestruction(
        const frontend::ProjectOopMirDispatchTable& table,
        LLVMValueRef object,
        std::string& error)
{
        if (!table.delegatedBaseDestructionTypeOrdinal.has_value()) return true;
        const auto baseAccessor = descriptorAccessorFunctions_.find(
            *table.delegatedBaseDestructionTypeOrdinal);
        if (baseAccessor == descriptorAccessorFunctions_.end())
        {
            return fail(error,
                "Stage 35 local destruction cannot resolve its precompiled base descriptor accessor");
        }
        LLVMValueRef baseDescriptor = LLVMBuildCall2(
            builder_, baseAccessor->second.functionType, baseAccessor->second.function,
            nullptr, 0, "destruction.base.descriptor");
        LLVMValueRef entryAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, baseDescriptor, 7, "destruction.base.object.entry.addr");
        LLVMValueRef baseDestructionEntry = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), entryAddress,
            "destruction.base.object.entry");
        LLVMValueRef arguments[] = {object};
        buildNoUnwindCall(
            destructionEntryFunctionType_, baseDestructionEntry, arguments, 1, "");
        return true;
    }

void OopLlvmImplementation::emitReleasingDestructionEntry(
        const FunctionInfo& entry,
        const FunctionInfo& objectEntry,
        const SourceRange& debugRange)
{
        currentFunction_ = entry.function;
        currentDebugSubprogram_ = entry.subprogram;
        currentDebugScope_ = currentDebugSubprogram_;
        blockCounter_ = 0;
        LLVMBasicBlockRef releasingEntry = appendBlock("destroy-and-free.entry");
        LLVMPositionBuilderAtEnd(builder_, releasingEntry);
        ScopedDebugLocation releasingLocation(
            builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
        LLVMValueRef object = LLVMGetParam(currentFunction_, 0);
        LLVMSetValueName2(object, "object", 6);
        LLVMValueRef arguments[] = {object};
        buildNoUnwindCall(objectEntry.functionType, objectEntry.function, arguments, 1, "");
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
        LLVMBuildRetVoid(builder_);
    }

bool OopLlvmImplementation::defineDestructionEntries(std::string& error)
{
        for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
        {
            // The imported descriptor already points at the producer's destruction entry. This
            // module neither declares nor defines that private implementation directly.
            if (table.isImported) continue;
            const auto entryFound = destructionFunctions_.find(table.typeOrdinal);
            const auto objectEntryFound = objectDestructionFunctions_.find(table.typeOrdinal);
            if (entryFound == destructionFunctions_.end() ||
                objectEntryFound == objectDestructionFunctions_.end())
            {
                return fail(error, "item 319 lost a declared destruction entry");
            }

            SourceRange debugRange;
            LLVMValueRef object = beginObjectDestructionEntry(
                table, objectEntryFound->second, debugRange);
            ScopedDebugLocation generatedLocation(
                builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
            // The destruction-order CONTRACT executes here, step by step, exactly as the
            // frontend froze it: most-derived destructor body, that class's owned fields in
            // reverse declaration order, then the base, up the chain -- destruction is the
            // reverse of construction. The full contract statement and its rationale live on
            // ProjectOopMirDispatchTable::destructionSteps; this loop deliberately contains no
            // ordering decisions of its own, so the order can never quietly diverge between the
            // model and the emitted code. Verified by execution
            // (Stage50_11OwnershipExecutionTests: DestructionOrderContract*).
            if (!emitObjectDestructionSteps(table, object, error)) return false;
            if (!emitDelegatedBaseDestruction(table, object, error)) return false;
            LLVMBuildRetVoid(builder_);
            emitReleasingDestructionEntry(
                entryFound->second, objectEntryFound->second, debugRange);
            // The ordinary descriptor preserves the historical heap contract. Custom allocator
            // objects publish a per-object descriptor sidecar whose field 5 points at the custom
            // release wrapper, so no provenance bytes are read from the public object layout.
        }
        currentDebugSubprogram_ = nullptr;
        currentDebugScope_ = nullptr;
        currentDebugFileKey_ = 0;
        return true;
    }

}
