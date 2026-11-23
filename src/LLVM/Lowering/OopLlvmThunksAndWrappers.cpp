#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/CppBoolAbi.h"
#include "../Internal/CppSysVRecordAbi.h"
#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppVariadicAbiValidation.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "../Internal/CppDllExportHelper.h"
#include "../Internal/DropCase.h"
#include "../Internal/OopLlvmDirectCFunction.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"
#include "../../Frontend/Internal/TypeIndexShadowCheck.h"

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
                    !(function.isVariadic && function.mangledName.starts_with("__nsc_cpp_tls_import_")) &&
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
        std::vector<std::tuple<std::string, bool, bool, std::string>> targets;
        const auto visit = [&](const frontend::ProjectOopMirNode* node, auto&& self) -> void
        {
            if (node == nullptr) return;
            if ((node->kind == frontend::ProjectOopMirNodeKind::DelegateCreate ||
                 node->kind == frontend::ProjectOopMirNodeKind::LambdaCreate) &&
                isExternCDelegateType(node->typeKey) && !node->calleeName.empty())
            {
                // EN: T08-D18: a C function converted to a C pointer is its own address; it gets no thunk.
                // ES: T08-D18: una funcion C convertida a puntero C es su propia direccion; no recibe thunk.
                const auto* imported = findCallbackMirFunction(node->calleeName);
                if (node->kind == frontend::ProjectOopMirNodeKind::DelegateCreate &&
                    directCFunctionAddress(imported)) return;
                const std::tuple<std::string, bool, bool, std::string> target = {
                    node->calleeName, isVectorCallDelegateType(node->typeKey), isSysVDelegateType(node->typeKey),
                    node->typeKey};
                if (std::find(targets.begin(), targets.end(), target) == targets.end())
                    targets.push_back(target);
            }
            // C5-07: the Clang plan of a sysv_abi callback type. / El plan de Clang de un tipo de callback sysv_abi.
            if (node->cppFunctionPointerPlan && isSysVDelegateType(node->typeKey))
                sysvCallbackPlans_.emplace(node->typeKey, node->cppFunctionPointerPlan);
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
            // EN: X3-04. A method that implements an abstract C++ interface is called by the C++ adapter through its
            //     C boundary (`<method>__c_boundary`), which takes the receiver first and guards NativeSharp errors.
            // ES: X3-04. Un metodo que implementa una interfaz C++ abstracta lo llama el adaptador C++ por su limite
            //     C, que toma primero el receptor y protege los errores de NativeSharp.
            if (!function.namespaceFunctionOrdinal && !function.isStatic && memberModel_ != nullptr &&
                function.memberOrdinal < memberModel_->members().size())
            {
                const auto& method = memberModel_->members()[function.memberOrdinal];
                for (const std::size_t member : memberModel_->membersOfType(method.declaringTypeOrdinal))
                {
                    if (member >= memberModel_->members().size()) continue;
                    const auto& field = memberModel_->members()[member];
                    if (!field.cppInterfaceTypeOrdinal || field.name != "__nsc_cpp_interface_vptr") continue;
                    const auto* interface = nameLookup_.cppImportedType(*field.cppInterfaceTypeOrdinal);
                    if (interface != nullptr &&
                        std::find(interface->cppInterfaceMethods.begin(), interface->cppInterfaceMethods.end(),
                                  method.name) != interface->cppInterfaceMethods.end())
                    {
                        const std::tuple<std::string, bool, bool, std::string> target = {
                            function.mangledName, false, false, std::string()};
                        if (std::find(targets.begin(), targets.end(), target) == targets.end())
                            targets.push_back(target);
                    }
                }
            }
            visit(function.body.get(), visit);
            for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
                visit(cleanup.action, visit);
        }
        if (!partitioned_ || ownsModuleGlobals_)
        {
            for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
                visit(initializer.value.get(), visit);
        }
        for (const auto& [target, vectorCall, sysvCall, typeKey] : targets)
        {
            if (callbackGuardThunk(target, error, vectorCall, defineBodies, sysvCall, typeKey) == nullptr) return false;
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
            enterBlock(padBlock);
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
        // EN: T19-036. A C++ record that MSVC does not return in registers (cppIndirectReturn) leaves a callback
        //     through a hidden pointer, with vectorcall or the Win64 C convention of the C boundary, as from a direct
        //     declaration, even when NativeSharp's own body returns it directly. / ES: T19-036. Un record C++ que
        //     MSVC no devuelve en registros sale de un callback por un puntero oculto, con vectorcall o con la
        //     convencion C de Win64 del limite C, como de una declaracion directa, aunque el cuerpo NativeSharp lo
        //     devuelva directo.
        const bool cppMemoryReturn = cppIndirectReturnTypeKey(function.returnTypeKey);
        vectorReturnType = vectorCall && !cppMemoryReturn ? vectorCallAbiType(function.returnTypeKey) : nullptr;
        externalSretReturn = (internalSretReturn && vectorReturnType == nullptr) || cppMemoryReturn;
        externalReturnType = externalSretReturn
            ? LLVMVoidTypeInContext(context_)
            : (vectorReturnType != nullptr
                ? vectorReturnType : llvmType(function.returnTypeKey, error));
        if (externalReturnType == nullptr) return nullptr;
        // EN: T19-044. Outside vectorcall, a 128-bit integer leaves in XMM0 as `<2 x i64>` and arrives by the address
        //     of an aligned copy, as Clang calls the pointer (emitExternCDelegateCall).
        // ES: T19-044. Fuera de vectorcall, un entero de 128 bits sale en XMM0 como `<2 x i64>` y llega por la
        //     direccion de una copia alineada, como Clang llama al puntero (emitExternCDelegateCall).
        const auto wideInteger = [](std::string_view key) { return key == "builtin:i128" || key == "builtin:u128"; };
        if (!vectorCall && !callbackGuardSysV_ && !externalSretReturn && !function.referenceReturn &&
            wideInteger(function.returnTypeKey))
            externalReturnType = LLVMVectorType(LLVMInt64TypeInContext(context_), 2);
        std::vector<LLVMTypeRef> parameterTypes;
        parameterTypes.reserve(function.parameterTypeKeys.size() + 2U);
        if (externalSretReturn)
            parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        if (!function.isStatic)
            parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
        // EN: T08-D22: the same classification as a call through the pointer (vectorCallPassing).
        // ES: T08-D22: la misma clasificacion que una llamada por el puntero.
        const std::vector<VectorCallPassing> passing = vectorCall
            ? vectorCallPassing(function.parameterTypeKeys, function.parameterTypeKeys.size())
            : std::vector<VectorCallPassing>{};
        const std::size_t firstParameter = parameterTypes.size();
        if (!appendExternCDelegateParameterTypes(
                function.parameterTypeKeys, function.parameterTypeKeys.size(), false, passing, parameterTypes,
                error)) return nullptr;
        if (!vectorCall)
            for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
            {
                if (wideInteger(function.parameterTypeKeys[index]) && !callbackGuardSysV_)
                    parameterTypes[firstParameter + index] = LLVMPointerTypeInContext(context_, 0);
                // C3-01: C passes `_Float16` in XMM. / C pasa `_Float16` en XMM.
                if (function.parameterTypeKeys[index] == interop::kCppHalfTypeKey)
                    parameterTypes[firstParameter + index] = LLVMHalfTypeInContext(context_);
            }
        if (!vectorCall && !externalSretReturn && !function.referenceReturn &&
            function.returnTypeKey == interop::kCppHalfTypeKey)
            externalReturnType = LLVMHalfTypeInContext(context_);
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
        auto addBoolAttribute = [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddAttributeAtIndex(thunk, index, attribute); };
        if (!function.referenceReturn && function.returnTypeKey == "builtin:bool")
            applyBoolZeroExtension(context_, 0, addBoolAttribute);
        std::size_t boolPosition = (externalSretReturn ? 1U : 0U) + (function.isStatic ? 0U : 1U);
        for (const std::string& typeKey : function.parameterTypeKeys)
        {
            if (typeKey == "builtin:bool" &&
                LLVMTypeOf(LLVMGetParam(thunk, static_cast<unsigned>(boolPosition))) == LLVMInt1TypeInContext(context_))
                applyBoolZeroExtension(context_, static_cast<unsigned>(boolPosition + 1), addBoolAttribute);
            ++boolPosition;
        }
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
        applyVectorCallInRegAttributes(
            [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddAttributeAtIndex(thunk, index, attribute); },
            externalPosition, vectorCallPassing(function.parameterTypeKeys, function.parameterTypeKeys.size()));
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
        // T19-036: the external result pointer of a direct internal return is written after the call.
        else if (externalSretReturn) ++externalIndex;
        if (!function.isStatic) forwarded.push_back(LLVMGetParam(thunk, externalIndex++));
        const std::vector<VectorCallPassing> passing = vectorCall
            ? vectorCallPassing(function.parameterTypeKeys, function.parameterTypeKeys.size())
            : std::vector<VectorCallPassing>{};
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
        {
            const std::string& typeKey = function.parameterTypeKeys[index];
            LLVMValueRef value = LLVMGetParam(thunk, externalIndex++);
            const VectorCallPassing kind = index < passing.size() ? passing[index] : VectorCallPassing::Ordinary;
            // C3-01: a `_Float16` arrives in XMM. / Un `_Float16` llega en XMM.
            if (!vectorCall && typeKey == interop::kCppHalfTypeKey)
                value = carrierFromHalf(value, error);
            // T19-044: the caller's aligned copy of a 128-bit integer. / La copia alineada del llamador.
            else if (!vectorCall && !callbackGuardSysV_ && (typeKey == "builtin:i128" || typeKey == "builtin:u128"))
            {
                LLVMValueRef load = LLVMBuildLoad2(builder_, LLVMIntTypeInContext(context_, 128), value, "c.i128.arg");
                LLVMSetAlignment(load, 16U);
                value = load;
            }
            else if (kind == VectorCallPassing::Vector || kind == VectorCallPassing::HvaInReg)
                value = vectorCallAbiToCarrier(value, typeKey, error);
            else if (kind == VectorCallPassing::HvaIndirect)
            {
                // EN: T08-D22: the caller's copy is read and rewrapped with NativeSharp's own convention.
                // ES: T08-D22: se lee la copia del llamador y se vuelve a envolver con la convencion propia.
                LLVMTypeRef carrier = llvmType(typeKey, error);
                if (carrier == nullptr) return false;
                LLVMValueRef load = LLVMBuildLoad2(builder_, carrier, value, "vectorcall.hva.load");
                LLVMSetAlignment(load, structAlignmentFor(typeKey));
                value = load;
            }
            if (value == nullptr) return false;
            if (kind != VectorCallPassing::Ordinary)
            {
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
            if (savedBlock != nullptr) enterBlock(savedBlock);
        };
        currentFunction_ = thunk;
        blockCounter_ = 0;
        ScopedDebugLocation generatedDebugLocation(
            builder_, debugSubprogramLocation(function, subprogram));
        LLVMBasicBlockRef entryBlock = appendBlock("boundary.entry");
        LLVMBasicBlockRef returnBlock = appendBlock("boundary.return");
        LLVMBasicBlockRef unwindBlock = appendBlock("boundary.unwind");
        enterBlock(entryBlock);
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
        enterBlock(returnBlock);
        if (vectorReturnType != nullptr)
        {
            // EN: T08-D22: an 8-byte HFA returns directly inside NativeSharp; it is reshaped, not reloaded.
            // ES: T08-D22: una HFA de 8 bytes vuelve directa dentro de NativeSharp; se transforma.
            LLVMValueRef externalResult = internalSretSlot != nullptr
                ? LLVMBuildLoad2(builder_, vectorReturnType, internalSretSlot, "vectorcall.return.bridge")
                : carrierToVectorCallAbi(result, function.returnTypeKey, error);
            if (externalResult == nullptr)
            {
                restore();
                return false;
            }
            LLVMBuildRet(builder_, externalResult);
        }
        else if (externalSretReturn && !internalSretReturn && function.returnTypeKey != "builtin:void")
        {
            // EN: T19-036. NativeSharp returned the record directly; it is stored through the caller's pointer.
            // ES: T19-036. NativeSharp devolvio el record directo; se guarda por el puntero del llamador.
            LLVMValueRef store = LLVMBuildStore(builder_, result, LLVMGetParam(thunk, 0));
            LLVMSetAlignment(store, structAlignmentFor(function.returnTypeKey));
            LLVMBuildRetVoid(builder_);
        }
        else if (externalSretReturn || function.returnTypeKey == "builtin:void")
            LLVMBuildRetVoid(builder_);
        // T19-044: a 128-bit integer leaves in XMM0. / Un entero de 128 bits sale en XMM0.
        else if (!vectorCall && !callbackGuardSysV_ && !function.referenceReturn &&
                 (function.returnTypeKey == "builtin:i128" || function.returnTypeKey == "builtin:u128"))
            LLVMBuildRet(builder_, LLVMBuildBitCast(builder_, result,
                LLVMVectorType(LLVMInt64TypeInContext(context_), 2), "c.i128.return"));
        // C3-01: a `_Float16` leaves in XMM. / Un `_Float16` sale en XMM.
        else if (!vectorCall && !function.referenceReturn && function.returnTypeKey == interop::kCppHalfTypeKey)
            LLVMBuildRet(builder_, halfFromCarrier(result));
        else LLVMBuildRet(builder_, result);
        enterBlock(unwindBlock);
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
        bool defineBody,
        bool sysvCall,
        std::string_view sysvTypeKey)
{
        if (sysvCall && sysvCallbackPlans_.contains(std::string(sysvTypeKey)))
            return sysvPlannedCallbackThunk(targetName, sysvTypeKey, defineBody, error);
        const std::string cacheKey = targetName + (vectorCall ? "|vectorcall" : sysvCall ? "|sysv" : "|c");
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
        struct SysVBoundary
        {
            bool& flag;
            bool saved;
            ~SysVBoundary() { flag = saved; }
        } sysvBoundary{callbackGuardSysV_, callbackGuardSysV_};
        callbackGuardSysV_ = sysvCall;
        LLVMTypeRef thunkType = createCallbackGuardThunkType(
            *mirFunction, vectorCall, internalSretReturn, externalSretReturn,
            vectorReturnType, externalReturnType, error);
        if (thunkType == nullptr) return nullptr;
        const std::string thunkName = targetName +
            (vectorCall ? "__vectorcall_boundary" : sysvCall ? "__sysv_boundary" : "__c_boundary");
        LLVMValueRef thunk = LLVMAddFunction(module_, thunkName.c_str(), thunkType);
        if (thunk == nullptr)
        {
            return failValue(error, "C1.3 failed to declare the callback boundary thunk for " + targetName);
        }
        if (vectorCall) LLVMSetFunctionCallConv(thunk, LLVMX86VectorCallCallConv);
        // C5-07: a scalar sysv_abi callback; LLVM classifies its values as Clang does. / Un callback sysv_abi
        //     escalar; LLVM clasifica sus valores como Clang.
        if (sysvCall) LLVMSetFunctionCallConv(thunk, LLVMX8664SysVCallConv);
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
        // EN: T08-D02: the thunk name depends only on the target, so a precompiled library and its consumer
        //     may both define it (a published method converted to a C callback on each side). weak_odr plus
        //     a symbol COMDAT keeps one copy at final link, which also keeps a single callback address.
        // ES: T08-D02: el nombre del thunk depende solo del destino, así que una biblioteca precompilada y su
        //     consumidor pueden definirlo ambos (un método publicado convertido a callback C en cada lado).
        //     weak_odr con un COMDAT por símbolo conserva una copia al enlazar y una sola dirección de callback.
        applyOdrDefinition(thunk);
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

        LLVMValueRef libraryName = cStringGlobal(function.importDllName, "dll");
        LLVMValueRef symbolName = cStringGlobal(function.cAbiName, "sym");
        if (libraryName == nullptr || symbolName == nullptr)
        {
            return fail(error, "C1.2 failed to materialize the import identity strings");
        }
        if (function.mangledName.starts_with("__nsc_cpp_tls_import_"))
        {
            currentFunction_ = importFunction;
            blockCounter_ = 0;
            enterBlock(appendBlock("import.entry"));
            LLVMTypeRef arguments[] = {ptr, ptr};
            LLVMTypeRef helperType = LLVMFunctionType(ptr, arguments, 2, 0);
            LLVMValueRef values[] = {libraryName, symbolName};
            LLVMValueRef target = LLVMBuildCall2(builder_, helperType,
                cppDllTlsExportHelper(context_, module_), values, 2, "import.address");
            if (!emitGuard(LLVMBuildICmp(builder_, LLVMIntNE, target, LLVMConstNull(ptr), "import.bound"),
                           LlvmTrapReason::ImportBindingFailure, "import.binding", error)) return false;
            return emitRuntimeBoundForwarding(function, importFunction, importType, target, error);
        }
        if (!ensureRuntimeImportBindingFunctions(error)) return false;
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
        enterBlock(entryBlock);
        LLVMValueRef cached = LLVMBuildLoad2(builder_, ptr, cache, "import.cached");
        LLVMValueRef needsBinding =
            LLVMBuildICmp(builder_, LLVMIntEQ, cached, LLVMConstNull(ptr), "import.unbound");
        LLVMBuildCondBr(builder_, needsBinding, bindBlock, callBlock);

        enterBlock(bindBlock);
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

        enterBlock(callBlock);
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
        applyExternalBoolAbi(context_, &function,
            [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddCallSiteAttribute(result, index, attribute); });
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
        if constexpr (frontend::internal::kIndexShadowCheckEnabled)
        {
            frontend::internal::shadowCheckTypeIdentityIndex(
                nameLookup_, key, "OopLlvmImplementation::isExternCDelegateType");
        }
        // The shared type index keeps the first type of an identity, as the scan below; the
        // scan only runs for hand-assembled models that have no type index.
        if (const std::optional<std::size_t> ordinal = nameLookup_.typeOrdinalForIdentity(key))
        {
            const frontend::NamedTypeDeclaration* type = nameLookup_.typeForOrdinal(*ordinal);
            return type != nullptr && type->kind == frontend::NamedTypeDeclarationKind::Delegate &&
                type->syntax != nullptr && type->syntax->hasModifier(lexer::TokenKind::KwExtern);
        }
        if (nameLookup_.hasTypeIndex()) return false;
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.key() != key) continue;
            return type.kind == frontend::NamedTypeDeclarationKind::Delegate &&
                type.syntax != nullptr && type.syntax->hasModifier(lexer::TokenKind::KwExtern);
        }
        return false;
    }

LLVMValueRef OopLlvmImplementation::sysvPlannedCallbackThunk(
        const std::string& targetName, std::string_view typeKey, bool defineBody, std::string& error)
{
        using namespace interop::abi_plan;
        const std::string cacheKey = targetName + "|sysv-plan|" + std::string(typeKey);
        if (const auto cached = callbackGuardThunks_.find(cacheKey); cached != callbackGuardThunks_.end())
            return cached->second.function;
        const auto signature = language::parseFunctionTypeIdentity(typeKey);
        const auto planned = sysvCallbackPlans_.find(std::string(typeKey));
        if (!signature || planned == sysvCallbackPlans_.end() || !planned->second)
            return failValue(error, "sysv_abi callback lost the Clang plan of its pointer type");
        const Plan& plan = *planned->second;
        if (!interop::validateCppSysVFunctionPointerPlan(plan) || plan.variadic || plan.callingConvention != 78 ||
            plan.logicalParameters.size() != signature->parameters.size())
            return failValue(error, "sysv_abi callback plan is not supported");
        // The C boundary of the target guards NativeSharp exceptions. / El limite C del destino guarda excepciones.
        LLVMValueRef boundary = callbackGuardThunk(targetName, error, false, defineBody, false);
        if (!boundary) return nullptr;
        const auto pieceType = [&](const Type& type) -> LLVMTypeRef {
            if (type.packed) return nullptr;
            if (type.kind == TypeKind::Vector)
                return type.widthOrCount == 2 && type.elements.size() == 1 && type.elements[0].kind == TypeKind::Float &&
                    !type.elements[0].widthOrCount && type.elements[0].elements.empty() ?
                    LLVMVectorType(LLVMFloatTypeInContext(context_), 2) : nullptr;
            if (!type.elements.empty()) return nullptr;
            if (type.kind == TypeKind::Integer)
                return type.widthOrCount && type.widthOrCount <= 64 ?
                    LLVMIntTypeInContext(context_, static_cast<unsigned>(type.widthOrCount)) : nullptr;
            if (type.widthOrCount) return nullptr;
            if (type.kind == TypeKind::Float) return LLVMFloatTypeInContext(context_);
            if (type.kind == TypeKind::Double) return LLVMDoubleTypeInContext(context_);
            if (type.kind == TypeKind::Pointer) return LLVMPointerTypeInContext(context_, 0);
            return nullptr;
        };
        const auto pieceLayout = [](const Type& type) -> std::pair<std::uint64_t, std::uint64_t> {
            if (type.kind == TypeKind::Integer)
            {
                std::uint64_t alignment = 1;
                while (alignment * 8 < type.widthOrCount && alignment < 8) alignment *= 2;
                return {(type.widthOrCount + 7) / 8, alignment};
            }
            if (type.kind == TypeKind::Float) return {4, 4};
            return {8, 8};
        };
        // The pieces of a direct record at their byte offsets, as the call through the pointer reads them.
        // Las piezas de un record directo en sus desplazamientos, como las lee la llamada por el puntero.
        const auto recordPieces = [&](const Type& physical, const std::optional<Type>& coercion, std::uint64_t offset,
                                      std::uint64_t size, std::vector<std::pair<Type, std::uint64_t>>& pieces) {
            pieces.clear();
            const Type& shape = coercion ? *coercion : physical;
            if (shape.kind != TypeKind::Structure)
            {
                if (!pieceType(shape)) return false;
                pieces.push_back({shape, offset});
            }
            else
            {
                if (shape.packed || shape.widthOrCount || shape.elements.empty() || shape.elements.size() > 2) return false;
                std::uint64_t at = offset;
                for (const auto& element : shape.elements)
                {
                    if (!pieceType(element)) return false;
                    const auto [bytes, alignment] = pieceLayout(element);
                    at = (at + alignment - 1) / alignment * alignment;
                    pieces.push_back({element, at});
                    at += bytes;
                }
            }
            for (const auto& [type, at] : pieces)
                if (at > size || pieceLayout(type).first > size - at) return false;
            return true;
        };
        const auto recordKey = [&](const std::string& key) {
            return key.starts_with("type:") && !key.ends_with('*') && findStructLayout(mirModel_, key) != nullptr;
        };
        // Physical signature. / Firma fisica.
        struct Logical { enum class Kind { Scalar, Pieces, ByValue } kind; std::vector<std::pair<Type, std::uint64_t>> pieces; };
        std::vector<Logical> logicals;
        std::vector<LLVMTypeRef> physicalTypes;
        const std::string& resultKey = signature->result;
        const bool recordResult = recordKey(resultKey);
        const auto& result = plan.logicalReturn;
        bool sretResult = false;
        LLVMTypeRef returnType = nullptr;
        std::vector<std::pair<Type, std::uint64_t>> returnPieces;
        if (recordResult)
        {
            const auto* layout = findStructLayout(mirModel_, resultKey);
            if (result.passing == Passing::Indirect)
            {
                if (plan.physicalReturn.kind != TypeKind::Void || plan.physicalParameters.empty() ||
                    !plan.physicalParameters[0].structureReturn || plan.physicalParameters[0].type.kind != TypeKind::Pointer)
                    return failValue(error, "sysv_abi callback record return plan is not supported");
                sretResult = true;
                returnType = LLVMVoidTypeInContext(context_);
                physicalTypes.push_back(LLVMPointerTypeInContext(context_, 0));
            }
            else
            {
                if (result.passing != Passing::Direct ||
                    !recordPieces(plan.physicalReturn, std::nullopt, result.offset, layout->size, returnPieces))
                    return failValue(error, "sysv_abi callback record return plan is not supported");
                std::vector<LLVMTypeRef> parts;
                for (const auto& [type, at] : returnPieces) parts.push_back(pieceType(type));
                returnType = plan.physicalReturn.kind == TypeKind::Structure ?
                    LLVMStructTypeInContext(context_, parts.data(), static_cast<unsigned>(parts.size()), 0) : parts.front();
            }
        }
        else if (resultKey == "builtin:void")
        {
            if (result.passing != Passing::Ignore || plan.physicalReturn.kind != TypeKind::Void)
                return failValue(error, "sysv_abi callback return plan is not supported");
            returnType = LLVMVoidTypeInContext(context_);
        }
        else
        {
            returnType = pieceType(plan.physicalReturn);
            LLVMTypeRef expected = llvmType(resultKey, error);
            if (!expected) return nullptr;
            if ((result.passing != Passing::Direct && result.passing != Passing::Extend) || returnType != expected ||
                result.offset)
                return failValue(error, "sysv_abi callback return plan is not supported");
        }
        for (std::size_t i = 0; i < signature->parameters.size(); ++i)
        {
            const auto& key = signature->parameters[i];
            const auto& logical = plan.logicalParameters[i];
            if (logical.firstPhysical != physicalTypes.size() || logical.physicalCount < 1 ||
                logical.firstPhysical + logical.physicalCount > plan.physicalParameters.size())
                return failValue(error, "sysv_abi callback argument plan is not supported");
            const auto& first = plan.physicalParameters[logical.firstPhysical];
            if (!recordKey(key))
            {
                LLVMTypeRef type = pieceType(first.type);
                LLVMTypeRef expected = llvmType(key, error);
                if (!expected) return nullptr;
                if ((logical.passing != Passing::Direct && logical.passing != Passing::Extend) ||
                    logical.physicalCount != 1 || logical.offset || type != expected || first.byValue || first.structureReturn)
                    return failValue(error, "sysv_abi callback scalar plan is not supported");
                logicals.push_back({Logical::Kind::Scalar, {}});
                physicalTypes.push_back(type);
                continue;
            }
            const auto* layout = findStructLayout(mirModel_, key);
            if (logical.passing == Passing::Indirect)
            {
                if (logical.physicalCount != 1 || !logical.byValue || !first.byValue || first.structureReturn ||
                    first.type.kind != TypeKind::Pointer || !first.alignment || first.alignment > 4096 ||
                    (first.alignment & (first.alignment - 1)) != 0)
                    return failValue(error, "sysv_abi callback byval plan is not supported");
                logicals.push_back({Logical::Kind::ByValue, {}});
                physicalTypes.push_back(LLVMPointerTypeInContext(context_, 0));
                continue;
            }
            std::vector<std::pair<Type, std::uint64_t>> pieces;
            if (logical.passing != Passing::Direct ||
                !recordPieces(first.type, logical.physicalCount == 1 ? std::nullopt : logical.coercion, logical.offset,
                    layout->size, pieces) || pieces.size() != logical.physicalCount)
                return failValue(error, "sysv_abi callback record plan is not supported");
            for (std::size_t p = 0; p < pieces.size(); ++p)
            {
                const auto& declared = plan.physicalParameters[logical.firstPhysical + p];
                if (declared.byValue || declared.structureReturn || declared.signExtend || declared.zeroExtend ||
                    pieceType(declared.type) != pieceType(pieces[p].first))
                    return failValue(error, "sysv_abi callback record plan is not supported");
                physicalTypes.push_back(pieceType(pieces[p].first));
            }
            logicals.push_back({Logical::Kind::Pieces, std::move(pieces)});
        }
        if (physicalTypes.size() != plan.physicalParameters.size())
            return failValue(error, "sysv_abi callback physical parameter count is invalid");
        LLVMTypeRef thunkType = LLVMFunctionType(returnType, physicalTypes.empty() ? nullptr : physicalTypes.data(),
            static_cast<unsigned>(physicalTypes.size()), 0);
        const std::string thunkName = targetName + "__sysv_boundary";
        LLVMValueRef thunk = LLVMAddFunction(module_, thunkName.c_str(), thunkType);
        if (!thunk) return failValue(error, "failed to declare the sysv_abi callback boundary for " + targetName);
        LLVMSetFunctionCallConv(thunk, LLVMX8664SysVCallConv);
        applyUwtableAttribute(thunk);
        const auto addAttribute = [&](LLVMAttributeIndex index, LLVMAttributeRef attribute)
            { LLVMAddAttributeAtIndex(thunk, index, attribute); };
        const auto extension = [&](LLVMAttributeIndex index, bool signExtend, bool zeroExtend) -> bool {
            if (!signExtend && !zeroExtend) return true;
            const std::string_view name = signExtend ? "signext" : "zeroext";
            const auto kind = llvm_api::LLVMGetEnumAttributeKindForName(name.data(), name.size());
            const auto attribute = kind ? llvm_api::LLVMCreateEnumAttribute(context_, kind, 0) : nullptr;
            if (!attribute) return false;
            addAttribute(index, attribute);
            return true;
        };
        if (!extension(0, plan.returnSignExtend, plan.returnZeroExtend))
            return failValue(error, "sysv_abi callback return extension is unavailable");
        for (std::size_t i = 0; i < plan.physicalParameters.size(); ++i)
            if (!extension(static_cast<LLVMAttributeIndex>(i + 1), plan.physicalParameters[i].signExtend,
                           plan.physicalParameters[i].zeroExtend))
                return failValue(error, "sysv_abi callback argument extension is unavailable");
        if (sretResult)
            applySretAttributes(addAttribute, 0U, resultKey);
        for (std::size_t i = 0; i < logicals.size(); ++i)
            if (logicals[i].kind == Logical::Kind::ByValue)
            {
                const auto& logical = plan.logicalParameters[i];
                LLVMTypeRef carrier = llvmType(signature->parameters[i], error);
                if (!carrier) return nullptr;
                applySysVRecordByValue(context_, carrier,
                    static_cast<unsigned>(plan.physicalParameters[logical.firstPhysical].alignment),
                    static_cast<unsigned>(logical.firstPhysical), addAttribute);
            }
        if (!defineBody)
        {
            callbackGuardThunks_[cacheKey] = FunctionInfo{thunk, thunkType, nullptr};
            return thunk;
        }
        // Body: the logical values, the C boundary, the physical result. / Cuerpo: valores logicos, limite C, resultado.
        LLVMValueRef savedFunction = currentFunction_;
        const std::size_t savedBlockCounter = blockCounter_;
        LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(builder_);
        LLVMMetadataRef savedLocation = LLVMGetCurrentDebugLocation2(builder_);
        const auto restore = [&]() {
            currentFunction_ = savedFunction;
            blockCounter_ = savedBlockCounter;
            if (savedBlock) enterBlock(savedBlock);
            LLVMSetCurrentDebugLocation2(builder_, savedLocation);
        };
        currentFunction_ = thunk;
        blockCounter_ = 0;
        LLVMSetCurrentDebugLocation2(builder_, nullptr);
        enterBlock(appendBlock("sysv.callback.entry"));
        std::vector<LLVMValueRef> values;
        for (std::size_t i = 0; i < logicals.size(); ++i)
        {
            const auto& key = signature->parameters[i];
            const auto& logical = plan.logicalParameters[i];
            LLVMValueRef value = nullptr;
            if (logicals[i].kind == Logical::Kind::Scalar)
                value = LLVMGetParam(thunk, static_cast<unsigned>(logical.firstPhysical));
            else
            {
                const auto* layout = findStructLayout(mirModel_, key);
                LLVMTypeRef carrier = llvmType(key, error);
                if (!carrier) { restore(); return nullptr; }
                LLVMValueRef source = nullptr;
                unsigned alignment = static_cast<unsigned>(layout->alignment);
                if (logicals[i].kind == Logical::Kind::ByValue)
                    source = LLVMGetParam(thunk, static_cast<unsigned>(logical.firstPhysical));
                else
                {
                    alignment = static_cast<unsigned>(std::max<std::uint64_t>(layout->alignment, 8));
                    source = entryBlockAlloca(carrier, alignment, "sysv.callback.record");
                    for (std::size_t p = 0; p < logicals[i].pieces.size(); ++p)
                    {
                        const auto& [type, at] = logicals[i].pieces[p];
                        LLVMValueRef address = at ? byteOffset(source, at, "sysv.callback.piece.addr") : source;
                        LLVMValueRef stored = LLVMBuildStore(builder_,
                            LLVMGetParam(thunk, static_cast<unsigned>(logical.firstPhysical + p)), address);
                        LLVMSetAlignment(stored, static_cast<unsigned>(pieceLayout(type).second));
                    }
                }
                LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, source, "sysv.callback.value");
                LLVMSetAlignment(loaded, alignment);
                value = wrapIndirectArgument(loaded, key, error);
            }
            if (!value) { restore(); return nullptr; }
            values.push_back(value);
        }
        language::FunctionType cSignature = *signature;
        cSignature.callingConvention = language::CallingConvention::C;
        LLVMValueRef logicalResult = emitExternCDelegateCall(boundary, values, signature->parameters, resultKey,
            language::functionTypeIdentity(cSignature), false, false, error);
        if (!logicalResult) { restore(); return nullptr; }
        if (sretResult)
        {
            LLVMValueRef stored = LLVMBuildStore(builder_, logicalResult, LLVMGetParam(thunk, 0));
            LLVMSetAlignment(stored, structAlignmentFor(resultKey));
            LLVMBuildRetVoid(builder_);
        }
        else if (recordResult)
        {
            const auto* layout = findStructLayout(mirModel_, resultKey);
            LLVMTypeRef carrier = llvmType(resultKey, error);
            if (!carrier) { restore(); return nullptr; }
            const auto alignment = static_cast<unsigned>(std::max<std::uint64_t>(layout->alignment, 8));
            LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "sysv.callback.result");
            LLVMValueRef stored = LLVMBuildStore(builder_, logicalResult, slot);
            LLVMSetAlignment(stored, alignment);
            LLVMValueRef physical = plan.physicalReturn.kind == TypeKind::Structure ? LLVMGetUndef(returnType) : nullptr;
            for (std::size_t p = 0; p < returnPieces.size(); ++p)
            {
                const auto& [type, at] = returnPieces[p];
                LLVMValueRef address = at ? byteOffset(slot, at, "sysv.callback.result.addr") : slot;
                LLVMValueRef piece = LLVMBuildLoad2(builder_, pieceType(type), address, "sysv.callback.result.piece");
                LLVMSetAlignment(piece, static_cast<unsigned>(pieceLayout(type).second));
                physical = physical ? LLVMBuildInsertValue(builder_, physical, piece, static_cast<unsigned>(p),
                    "sysv.callback.result.pack") : piece;
            }
            LLVMBuildRet(builder_, physical);
        }
        else if (resultKey == "builtin:void") LLVMBuildRetVoid(builder_);
        else LLVMBuildRet(builder_, logicalResult);
        restore();
        applyOdrDefinition(thunk);
        callbackGuardThunks_[cacheKey] = FunctionInfo{thunk, thunkType, nullptr};
        return thunk;
}

bool OopLlvmImplementation::isSysVDelegateType(std::string_view typeKey) const noexcept
{
        const auto functionType = language::parseFunctionTypeIdentity(typeKey);
        return functionType && functionType->callingConvention == language::CallingConvention::SysV;
}

bool OopLlvmImplementation::isVectorCallDelegateType(std::string_view typeKey) const noexcept
{
        if (const std::optional<language::FunctionType> functionType =
                language::parseFunctionTypeIdentity(typeKey))
            return functionType->callingConvention == language::CallingConvention::VectorCall;
        if (!typeKey.starts_with("type:")) return false;
        const std::string_view key = typeKey.substr(5);
        if constexpr (frontend::internal::kIndexShadowCheckEnabled)
        {
            frontend::internal::shadowCheckTypeIdentityIndex(
                nameLookup_, key, "OopLlvmImplementation::isVectorCallDelegateType");
        }
        // The shared type index keeps the first type of an identity, as the scan below; the
        // scan only runs for hand-assembled models that have no type index.
        if (const std::optional<std::size_t> ordinal = nameLookup_.typeOrdinalForIdentity(key))
        {
            const frontend::NamedTypeDeclaration* type = nameLookup_.typeForOrdinal(*ordinal);
            return type != nullptr && type->kind == frontend::NamedTypeDeclarationKind::Delegate &&
                type->syntax != nullptr &&
                type->syntax->hasModifier(lexer::TokenKind::KwVectorCall);
        }
        if (nameLookup_.hasTypeIndex()) return false;
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
        enterBlock(entry);
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
        enterBlock(returnBlock);
        if (returnType == LLVMVoidTypeInContext(context_)) LLVMBuildRetVoid(builder_);
        else LLVMBuildRet(builder_, result);
        enterBlock(unwindBlock);
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
                return fail(error, "NativeSharp lost declared value drop glue for '" +
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
            enterBlock(entry);
            LLVMValueRef storage = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(storage, "storage", 7);
            if (layout.typeKey.starts_with("iterator:") ||
                layout.typeKey.starts_with("async-iterator:"))
            {
                // The iterator's Drop function follows the dynamically sized item field.
                const std::uint64_t dropOffset = futureFieldOffset(layout.typeKey, 4);
                if (dropOffset == 0) return fail(error, "iterator drop glue has no Drop layout");
                LLVMValueRef dropFn = LLVMBuildLoad2(
                    builder_, LLVMPointerTypeInContext(context_, 0),
                    byteOffset(storage, dropOffset, "drop.fn.addr"), "drop.fn");
                // EN: LKF-473: a declared but never assigned iterator local is zero and has no Drop
                //     function; its scope exit called address zero (0xC0000005). Like a future, a
                //     zero iterator drops nothing.
                // ES: LKF-473: un local iterador declarado y nunca asignado vale cero y no tiene función
                //     Drop; su salida de scope llamaba a la dirección cero (0xC0000005). Como un future,
                //     un iterador en cero no destruye nada.
                LLVMBasicBlockRef call = appendBlock("iterator.drop.call");
                LLVMBasicBlockRef done = appendBlock("iterator.drop.done");
                LLVMBuildCondBr(
                    builder_,
                    LLVMBuildICmp(builder_, LLVMIntEQ, dropFn,
                        LLVMConstNull(LLVMPointerTypeInContext(context_, 0)), "drop.fn.none"),
                    done, call);
                enterBlock(call);
                LLVMTypeRef fnParams[] = { LLVMPointerTypeInContext(context_, 0) };
                LLVMTypeRef fnType = LLVMFunctionType(LLVMVoidTypeInContext(context_), fnParams, 1, 0);
                LLVMValueRef args[] = { storage };
                if (LLVMBuildCall2(builder_, fnType, dropFn, args, 1U, "") == nullptr)
                    return fail(error, "iterator drop glue call failed");
                LLVMBuildBr(builder_, done);
                enterBlock(done);
                LLVMBuildRetVoid(builder_);
            }
            else if (layout.typeKey.starts_with("future:"))
            {
                // EN: Freeze C-14: a future dispatches through its Drop pointer (member 5). A
                //     default or consumed (zeroed) future has none and drops nothing.
                // ES: Congelamiento C-14: un future despacha por su puntero Drop (miembro 5).
                //     Uno por defecto o consumido (en cero) no tiene y no destruye nada.
                LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
                LLVMValueRef dropFn = LLVMBuildLoad2(
                    builder_, ptr,
                    byteOffset(storage, futureFieldOffset(layout.typeKey, 5), "drop.fn.addr"),
                    "drop.fn");
                LLVMBasicBlockRef call = appendBlock("future.drop.call");
                LLVMBasicBlockRef done = appendBlock("future.drop.done");
                LLVMBuildCondBr(
                    builder_, LLVMBuildICmp(builder_, LLVMIntEQ, dropFn, LLVMConstNull(ptr), "drop.fn.none"),
                    done, call);
                enterBlock(call);
                LLVMTypeRef fnParams[] = { ptr };
                LLVMTypeRef fnType = LLVMFunctionType(LLVMVoidTypeInContext(context_), fnParams, 1, 0);
                LLVMValueRef args[] = { storage };
                if (LLVMBuildCall2(builder_, fnType, dropFn, args, 1U, "") == nullptr)
                    return fail(error, "future drop glue call failed");
                LLVMBuildBr(builder_, done);
                enterBlock(done);
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
        enterBlock(entry);
        LLVMValueRef object = LLVMGetParam(currentFunction_, 0);
        LLVMSetValueName2(object, "object", 6);
        return object;
    }

bool OopLlvmImplementation::emitObjectDestructionSteps(
        const frontend::ProjectOopMirDispatchTable& table,
        LLVMValueRef object,
        std::string& error)
{
        const auto emitStep = [&](const frontend::ProjectOopMirDispatchTable::DestructionStep& step) -> bool
        {
            // EN: Freeze C-11: a value field with a non-trivial lifetime is dropped in place.
            // ES: Congelamiento C-11: un campo de valor con lifetime no trivial se destruye en
            //     su lugar.
            if (step.isValueFieldDrop)
            {
                LLVMValueRef fieldAddress = byteOffset(object, step.ownedField.offset, "value.field.addr");
                if (!emitValueDropAddress(fieldAddress, step.valueFieldTypeKey, "value.field", error))
                    return false;
                return true;
            }
            if (!step.isFieldRelease)
            {
                const auto bodyFound = functions_.find(step.destructorCalleeName);
                if (bodyFound == functions_.end())
                    return fail(error, "Stage 35 destructor body was not declared: " + step.destructorCalleeName);
                LLVMValueRef arguments[] = {object};
                if (buildPotentiallyThrowingCall(
                        bodyFound->second.functionType, bodyFound->second.function, arguments, 1, "", error) == nullptr &&
                    !error.empty()) return false;
                return true;
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
            enterBlock(destroyBlock);
            LLVMBuildStore(builder_, LLVMConstNull(ptr), fieldAddress);
            if (ownedField.isNativeArray)
            {
                if (!emitNativeArrayDestroy(
                        fieldValue, ownedField.arrayTypeKey, "owned.field.array", true, error))
                    return false;
            }
            else if (ownedField.isNativeString)
            {
                LLVMValueRef arguments[] = {fieldValue};
                buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            }
            else if (!ownedField.arrayTypeKey.empty())
            {
                // Struct drop plans retain the concrete owned type, including delegates.
                if (!emitOwnedPointerDestroy(fieldValue, ownedField.arrayTypeKey,
                                             "owned.field.destroy", error)) return false;
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
                if (buildPotentiallyThrowingCall(
                        destructionEntryFunctionType_, destructionEntry, arguments, 1, "", error) == nullptr &&
                    !error.empty()) return false;
            }
            LLVMBuildBr(builder_, doneBlock);
            enterBlock(doneBlock);
            return true;
        };

        // Each exceptional pad runs one remaining step and unwinds to the next.
        // The normal path invokes a step with the pad for its suffix as destination.
        const std::size_t count = table.destructionSteps.size();
        const bool hasBase = table.delegatedBaseDestructionTypeOrdinal.has_value();
        std::vector<LLVMBasicBlockRef> unwind(count + 1U, nullptr);
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef guardParameters[] = {pointerType};
        LLVMTypeRef guardType = LLVMFunctionType(LLVMVoidTypeInContext(context_), guardParameters, 1, 0);
        LLVMValueRef filter = LLVMGetNamedFunction(module_, "__nsc_cleanup_accept_any");
        if (filter == nullptr)
        {
            LLVMTypeRef filterParameters[] = {pointerType, pointerType};
            LLVMTypeRef filterType = LLVMFunctionType(LLVMInt32TypeInContext(context_), filterParameters, 2, 0);
            filter = LLVMAddFunction(module_, "__nsc_cleanup_accept_any", filterType);
            LLVMSetLinkage(filter, LLVMInternalLinkage);
            LLVMBuilderRef filterBuilder = LLVMCreateBuilderInContext(context_);
            LLVMPositionBuilderAtEnd(filterBuilder, LLVMAppendBasicBlockInContext(context_, filter, "entry"));
            LLVMBuildRet(filterBuilder, LLVMConstInt(LLVMInt32TypeInContext(context_), 1, 0));
            LLVMDisposeBuilder(filterBuilder);
        }
        const auto buildGuard = [&](std::size_t index, bool base) -> LLVMValueRef
        {
            LLVMValueRef outerFunction = currentFunction_;
            LLVMBasicBlockRef outerBlock = LLVMGetInsertBlock(builder_);
            LLVMValueRef outerObject = object;
            const std::size_t outerBlockCounter = blockCounter_;
            const auto outerFunclets = funcletStack_;
            const auto outerHandlers = exceptionHandlerStack_;
            std::size_t nameLength = 0;
            const char* name = LLVMGetValueName2(outerFunction, &nameLength);
            const std::string guardName = std::string(name, nameLength) + ".cleanup.guard." +
                (base ? std::string("base") : std::to_string(index));
            LLVMValueRef guard = LLVMAddFunction(module_, guardName.c_str(), guardType);
            LLVMSetLinkage(guard, LLVMInternalLinkage);
            LLVMSetPersonalityFn(guard, sehPersonalityFunction_);
            currentFunction_ = guard;
            object = LLVMGetParam(guard, 0);
            blockCounter_ = 0;
            funcletStack_.clear();
            exceptionHandlerStack_.clear();
            LLVMBasicBlockRef entry = appendBlock("cleanup.guard.entry");
            LLVMBasicBlockRef dispatch = appendBlock("cleanup.guard.dispatch");
            enterBlock(entry);
            exceptionHandlerStack_.push_back(ExceptionHandlerContext{0, dispatch, 0, 0});
            const bool ok = base ? emitDelegatedBaseDestruction(table, object, error)
                                 : emitStep(table.destructionSteps[index]);
            exceptionHandlerStack_.pop_back();
            if (ok && !terminated()) LLVMBuildRetVoid(builder_);
            enterBlock(dispatch);
            LLVMValueRef catches = LLVMBuildCatchSwitch(builder_, nullptr, nullptr, 1, "cleanup.guard.switch");
            LLVMBasicBlockRef handler = appendBlock("cleanup.guard.handler");
            LLVMAddHandler(catches, handler);
            enterBlock(handler);
            LLVMValueRef catchArguments[] = {filter};
            LLVMValueRef catchPad = LLVMBuildCatchPad(builder_, catches, catchArguments, 1, "cleanup.guard.pad");
            funcletStack_.push_back(FuncletContext{catchPad, FuncletKind::Catch, 0, nullptr, nullptr});
            // Runtime trap codes are 128 plus this dedicated double-exception reason.
            constexpr std::uint64_t doubleExceptionDuringCleanup = 20;
            LLVMValueRef reason = LLVMConstInt(LLVMInt32TypeInContext(context_), doubleExceptionDuringCleanup, 0);
            LLVMValueRef arguments[] = {reason};
            buildNoUnwindCall(trapFunctionType_, trapFunction_, arguments, 1, "");
            funcletStack_.pop_back();
            LLVMBuildUnreachable(builder_);
            currentFunction_ = outerFunction;
            object = outerObject;
            blockCounter_ = outerBlockCounter;
            funcletStack_ = outerFunclets;
            exceptionHandlerStack_ = outerHandlers;
            enterBlock(outerBlock);
            return ok ? guard : nullptr;
        };
        if (count != 0U || hasBase)
        {
            LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
            LLVMBasicBlockRef normal = LLVMGetInsertBlock(builder_);
            LLVMBasicBlockRef next = buildCurrentUnwindDestination(error);
            if (!error.empty()) return false;
            if (hasBase)
            {
                LLVMBasicBlockRef block = appendBlock("destroy.cleanup.base");
                enterBlock(block);
                LLVMValueRef pad = LLVMBuildCleanupPad(builder_, nullptr, nullptr, 0, "destroy.cleanup.pad");
                funcletStack_.push_back(FuncletContext{pad, FuncletKind::Cleanup, 0, nullptr, nullptr});
                LLVMValueRef guard = buildGuard(0, true);
                if (guard == nullptr) return false;
                LLVMValueRef arguments[] = {object};
                buildNoUnwindCall(guardType, guard, arguments, 1, "");
                funcletStack_.pop_back();
                if (!terminated()) LLVMBuildCleanupRet(builder_, pad, next);
                next = block;
            }
            unwind[count] = next;
            for (std::size_t index = count; index > 1U; --index)
            {
                LLVMBasicBlockRef block = appendBlock("destroy.cleanup.step");
                enterBlock(block);
                LLVMValueRef pad = LLVMBuildCleanupPad(builder_, nullptr, nullptr, 0, "destroy.cleanup.pad");
                funcletStack_.push_back(FuncletContext{pad, FuncletKind::Cleanup, 0, nullptr, next});
                LLVMValueRef guard = buildGuard(index - 1U, false);
                if (guard == nullptr) return false;
                LLVMValueRef arguments[] = {object};
                buildNoUnwindCall(guardType, guard, arguments, 1, "");
                funcletStack_.pop_back();
                if (!terminated()) LLVMBuildCleanupRet(builder_, pad, next);
                next = block;
                unwind[index - 1U] = block;
            }
            enterBlock(normal);
        }
        for (std::size_t index = 0; index < count; ++index)
        {
            if (unwind[index + 1U] != nullptr)
                exceptionHandlerStack_.push_back(ExceptionHandlerContext{0, unwind[index + 1U], 0, 0});
            const bool ok = emitStep(table.destructionSteps[index]);
            if (unwind[index + 1U] != nullptr) exceptionHandlerStack_.pop_back();
            if (!ok) return false;
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
        if (buildPotentiallyThrowingCall(
                destructionEntryFunctionType_, baseDestructionEntry, arguments, 1, "", error) == nullptr &&
            !error.empty()) return false;
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
        enterBlock(releasingEntry);
        ScopedDebugLocation releasingLocation(
            builder_, debugSubprogramLocation(debugRange, currentDebugSubprogram_));
        LLVMValueRef object = LLVMGetParam(currentFunction_, 0);
        LLVMSetValueName2(object, "object", 6);
        LLVMValueRef arguments[] = {object};
        LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
        LLVMBasicBlockRef normal = appendBlock("destroy-and-free.normal");
        LLVMBasicBlockRef cleanup = appendBlock("destroy-and-free.cleanup");
        LLVMBuildInvoke2(builder_, objectEntry.functionType, objectEntry.function, arguments, 1,
                         normal, cleanup, "");
        enterBlock(cleanup);
        LLVMValueRef pad = LLVMBuildCleanupPad(builder_, nullptr, nullptr, 0, "destroy-and-free.pad");
        funcletStack_.push_back(FuncletContext{pad, FuncletKind::Cleanup, 0, nullptr, nullptr});
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
        funcletStack_.pop_back();
        LLVMBuildCleanupRet(builder_, pad, nullptr);
        enterBlock(normal);
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
