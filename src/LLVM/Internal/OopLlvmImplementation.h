#pragma once

#include "DropCase.h"
#include "OopLlvmActiveCatchBody.h"
#include "OopLlvmCachedCleanup.h"
#include "OopLlvmExceptionHandlerContext.h"
#include "OopLlvmFuncletContext.h"
#include "OopLlvmFuncletKind.h"
#include "OopLlvmFunctionInfo.h"
#include "OopLlvmLoopTargets.h"
#include "ScopedDebugLocation.h"
#include "ScopedDebugScope.h"
#include "StaticLocalGuard.h"
#include "StaticLocalStorage.h"
#include "../LlvmCApi.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/FloatingPointRoundingMode.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/Language/PortableSimdType.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;
class OopLlvmImplementation final
{
public:
    OopLlvmImplementation(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel* memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager* sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const DebugPrefixMap> debugPrefixMaps = {},
        bool partitioned = false,
        bool ownsModuleGlobals = true,
        std::span<const std::size_t> functionOrdinals = {},
        std::span<const std::size_t> declarationOrdinals = {},
        std::span<const std::uint8_t> reachableDefinitions = {},
        std::span<const std::uint8_t> staticFieldDeclarations = {},
        std::string_view moduleName = "NativeSharp.Stage34.OOP");

    bool lower(std::string& error);

    LLVMContextRef releaseContext() noexcept;

    LLVMModuleRef releaseModule() noexcept;

private:
    template <typename Callback>
    bool tracePhase(std::string_view phase, Callback&& callback);

    void dispose() noexcept;

    bool initializeDebugInformation(std::string& error);

    LLVMMetadataRef debugFile(SourceLocation location);

    unsigned debugCodeViewFileNumber(SourceLocation location);

    LLVMMetadataRef createDebugSubprogram(const frontend::ProjectOopMirFunction& function);

    LLVMMetadataRef createMemberDebugSubprogram(
        const frontend::ProjectOopMirFunction& function);

    LLVMMetadataRef createSynthesizedDebugSubprogram(
        SourceRange range,
        std::string_view displayName,
        std::string_view linkageName,
        std::string_view returnTypeKey,
        const std::vector<std::string>& parameterTypeKeys,
        bool localToUnit,
        bool thunk,
        const std::vector<frontend::ParameterPassKind>* parameterPassKinds = nullptr);

    LLVMMetadataRef debugParameterType(
        std::string_view typeKey,
        frontend::ParameterPassKind passKind,
        std::string_view parameterName);

    LLVMMetadataRef createGeneratedDebugSubprogram(
        const frontend::ProjectOopMirFunction& origin,
        std::string_view role,
        std::string_view linkageName,
        bool localToUnit);

    LLVMMetadataRef debugSubprogramLocation(SourceRange range, LLVMMetadataRef subprogram);

    LLVMMetadataRef debugSubprogramLocation(
        const frontend::ProjectOopMirFunction& origin,
        LLVMMetadataRef subprogram);

    LLVMMetadataRef debugLocation(const frontend::ProjectOopMirNode* node);

    LLVMMetadataRef createDebugLexicalScope(
        const frontend::ProjectOopMirNode* node,
        LLVMMetadataRef parent);

    const frontend::NamedTypeDeclaration* debugNamedType(std::string_view typeKey) const noexcept;

    std::string debugNamedDisplayName(const frontend::NamedTypeDeclaration& named) const;

    static LLVMDIFlags debugMemberFlags(frontend::ClassMemberAccessibility accessibility) noexcept;

    LLVMMetadataRef debugClassPointee(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::ProjectOopMirClassLayout& layout);

    void appendDebugClassMembers(
        const frontend::ProjectOopMirClassLayout& layout,
        LLVMMetadataRef file,
        unsigned line,
        LLVMMetadataRef temporary,
        std::vector<LLVMMetadataRef>& members);

    LLVMMetadataRef debugHeaderReference(
        std::string_view cacheKeyView,
        std::string_view displayName,
        std::string_view elementTypeKey);

    bool debugNameLooksLikeNativeHandle(std::string_view name) const;

    LLVMMetadataRef debugNativeHandleType();

    LLVMMetadataRef debugNamedValueType(std::string_view typeKey, std::string_view name);

    LLVMMetadataRef debugVariableLengthArrayType(
        std::string_view elementTypeKey,
        std::size_t rank);

    LLVMMetadataRef debugPlacementType(std::string_view typeKey);

    LLVMMetadataRef debugPlacementBase(
        std::string_view baseKey,
        const frontend::NamedTypeDeclaration& named,
        std::uint64_t& size,
        std::uint32_t& alignment);

    LLVMMetadataRef debugDelegateFunctionPointer(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named);

    LLVMMetadataRef debugDelegateReference(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named);

    std::optional<std::pair<std::string_view, std::uint64_t>> debugSimdShape(
        std::string_view typeKey) const noexcept;

    LLVMMetadataRef debugType(std::string_view typeKey);

    LLVMMetadataRef debugBasicType(std::string_view typeKey);

    LLVMMetadataRef debugPointerType(std::string_view typeKey);

    LLVMMetadataRef debugEnumType(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::builtins::IntegerTypeInfo& enumInfo);

    std::vector<LLVMMetadataRef> debugStructMembers(
        std::string_view typeKey,
        const frontend::ProjectOopMirStructLayout& layout,
        LLVMMetadataRef file,
        unsigned line,
        LLVMMetadataRef temporary);

    LLVMMetadataRef debugStructType(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::ProjectOopMirStructLayout& layout,
        std::string_view cacheKey);

    LLVMMetadataRef debugOwnershipType(
        std::string_view typeKey,
        sema::OwnershipKind ownership,
        bool automaticDestruction,
        SourceRange range);

    bool emitDebugParameter(
        const frontend::ProjectOopMirFunction& function,
        std::size_t index,
        LLVMValueRef storage,
        std::string& error);

    bool emitDebugCatchParameter(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::size_t handlerIndex,
        std::string& error);

    bool emitDebugThis(
        const frontend::ProjectOopMirFunction& function,
        std::string& error);

    bool emitDebugLocal(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error);

    bool emitDebugGlobal(
        const frontend::ProjectOopMirStaticField& field,
        LLVMValueRef global,
        std::string& error);

    // INSTRUMENTATION CODE: generated-program runtime trace.
    //
    // This is deliberately gated by Full debug information rather than -O0. Optimization and
    // diagnostics are independent contracts: a Release -O0 binary must not acquire tracing code,
    // while a Debug build remains traceable at any optimization level. The sink uses only static
    // strings, stack storage and direct Win32 calls; it must not allocate from the heap whose
    // corruption it is intended to diagnose.
    bool ensureRuntimeDebugTrace(std::string& error);

    bool buildRuntimeDebugTraceBody(
        LLVMTypeRef i32,
        LLVMValueRef path,
        std::string& error);

    bool emitRuntimeDebugTrace(
        std::uint32_t category,
        std::string_view text,
        std::string& error);

    std::string runtimeDebugCallableRecord(
        const frontend::ProjectOopMirFunction& function,
        std::string_view event) const;

    std::string runtimeDebugFieldStoreRecord(
        const frontend::ProjectOopMirNode& store,
        std::string_view event) const;

    std::string runtimeDebugFieldLoadRecord(
        const frontend::ProjectOopMirNode& field,
        std::string_view event) const;

    std::string runtimeDebugArrayRecord(
        const frontend::ProjectOopMirNode& node,
        std::string_view event) const;

    bool declareRuntime(std::string& error);
    bool declareRuntimeMemory(std::string& error);
    bool declareRuntimeAllocatorDebug(
        LLVMValueRef runtimeAllocationFunction,
        LLVMValueRef runtimeDeallocationFunction,
        std::string& error);
    void declareRuntimeConsoleTimeEnvironment();
    void declareRuntimeFilesystem();
    void declareRuntimeFilesystemCompletion();
    void declareRuntimeDirectoryAndThreads();
    void declareRuntimeAtomics();
    void declareRuntimeMutexAndMonitor();
    void declareRuntimeSynchronization();
    void declareRuntimeProcessSystemDiagnostics();
    void declareRuntimeSystemDiagnostics();
    void declareRuntimeExceptionsAndIntrinsics();
    bool runtimeDeclarationsValid(std::string& error) const;

    void applyOdrDefinition(LLVMValueRef value);

    void applyGenericOdrDefinition(LLVMValueRef value, const std::string& specializationKey);

    bool declareStaticFields(std::string& error, bool defineStorage);

    LLVMValueRef staticFieldInitializer(
        const frontend::ProjectOopMirStaticField& field,
        LLVMTypeRef type,
        std::string& error);

    bool defineStaticFieldAccessors(std::string& error);

    bool declareTypeInitializers(std::string& error, bool defineStorage);

    bool collectTypeInitializerRequirements(
        std::unordered_map<std::size_t, bool>& needsInitializer,
        bool defineStorage,
        std::string& error);

    bool defineTypeInitializers(std::string& error);

    void prepareTypeInitializerFunction(
        std::size_t typeOrdinal,
        const FunctionInfo& info,
        SourceRange& debugRange);

    LLVMValueRef emitTypeInitializerGuard(
        LLVMValueRef stateGlobal,
        LLVMValueRef ownerGlobal,
        LLVMValueRef zero,
        LLVMValueRef one,
        LLVMValueRef two,
        LLVMValueRef three,
        LLVMBasicBlockRef fastFailureCheck,
        LLVMBasicBlockRef casAttempt,
        LLVMBasicBlockRef casInitializedCheck,
        LLVMBasicBlockRef recursionCheck,
        LLVMBasicBlockRef spinWait,
        LLVMBasicBlockRef spinTerminal,
        LLVMBasicBlockRef initialize,
        LLVMBasicBlockRef done,
        LLVMBasicBlockRef failed);

    bool emitTypeInitializerBody(
        std::size_t typeOrdinal,
        LLVMValueRef stateGlobal,
        LLVMValueRef ownerGlobal,
        LLVMValueRef currentTid,
        LLVMValueRef initializedState,
        LLVMBasicBlockRef initialize,
        LLVMBasicBlockRef done,
        LLVMBasicBlockRef failureUnwind,
        std::string& error);

    bool emitTypeInitializerFailure(
        LLVMValueRef stateGlobal,
        LLVMValueRef failedState,
        LLVMBasicBlockRef failed,
        LLVMBasicBlockRef failureUnwind,
        std::string& error);

    bool defineTypeInitializer(
        std::size_t typeOrdinal,
        const FunctionInfo& info,
        std::string& error);

    bool declareFunctions(std::string& error);

    bool declareFunction(
        const frontend::ProjectOopMirFunction& function,
        std::size_t functionOrdinal,
        std::string& error);

    LLVMTypeRef createDeclaredFunctionType(
        const frontend::ProjectOopMirFunction& function,
        bool sretReturn,
        std::string& error);

    bool declareValueDropEntries(std::string& error);

    bool declareDestructionEntries(std::string& error);

    bool emitDispatchMetadata(std::string& error, bool defineStorage);

    bool declarePartitionedDispatchMetadata(std::string& error);

    bool declareDispatchMetadataStorage(
        LLVMTypeRef pointerType,
        LLVMTypeRef integerType,
        std::string& error);

    bool defineDispatchMetadataAccessors(
        LLVMTypeRef pointerType,
        LLVMTypeRef integerType,
        std::string& error);

    bool defineRttiHelpers(std::string& error);

    bool declareRttiHelpers(std::string& error);

    void defineClassRttiHelper();

    void defineInterfaceRttiHelper();

    void beginInterfaceRttiSearch(
        LLVMValueRef object,
        LLVMBasicBlockRef startBlock,
        LLVMBasicBlockRef mapBlock,
        LLVMBasicBlockRef failBlock,
        LLVMValueRef& count,
        LLVMValueRef& entries);

    void collectTypedCatchTypeOrdinals(
        const frontend::ProjectOopMirNode* node,
        std::vector<std::size_t>& ordinals) const;

    LLVMMetadataRef createTypedCatchFilterDebugSubprogram(
        std::size_t typeOrdinal,
        std::string_view name,
        SourceRange& debugRange);

    bool defineTypedCatchFilter(
        std::size_t typeOrdinal,
        const frontend::ProjectOopMirDispatchTable& table,
        std::string& error);

    bool defineTypedCatchFilters(std::string& error);

    bool defineFunctions(std::string& error);

    // C1.3 "callback exception boundary".
    //
    // THE CONTRACT, stated once: a NativeSharp exception cannot leave NativeSharp code through a
    // C boundary. It is not a style rule -- a NativeSharp `throw` is a Win64 SEH RaiseException,
    // and letting it continue past the boundary unwinds FOREIGN frames (a Win32 message pump, an
    // OpenGL driver, an SDL event loop) whose cleanup contract this compiler knows nothing about.
    // Whatever the foreign code would then do with a strange exception code, none of it is a
    // behaviour NativeSharp can define, so the boundary refuses to produce it.
    //
    // WHERE THE BOUNDARY IS. Exactly the two places foreign code can enter: a function whose
    // address was handed out as an `extern C delegate` (a callback), and an `export extern C`
    // entry point. Both get the SAME guard, because both are "C called us".
    //
    // WHAT THE GUARD IS. A thunk with the target's own signature -- which since Stage 40.2 IS the
    // Win64 C ABI for every C-expressible type -- that invokes the target inside one catchpad
    // filtered by `__nativesharp_eh_filter` (the filter that accepts this compiler's own
    // exception code and nothing else) and traps there. Two consequences worth being explicit
    // about:
    //
    //   * A FOREIGN SEH exception is NOT caught. The filter declines it, the catchswitch unwinds
    //     to caller, and an access violation raised inside a callback keeps propagating to
    //     whatever the host installed -- the boundary contains this compiler's exceptions, it
    //     does not swallow the operating system's.
    //   * The guard costs nothing on the success path: an invoke with no cleanup between it and
    //     the return, plus the unwind edge in the function's own exception table.
    //
    // The thunk is cached per target so that converting the same method twice yields the SAME
    // function pointer -- a host that compares two callbacks it was handed must still see them as
    // equal.
    bool defineCallbackGuardThunks(std::string& error, bool defineBodies);

    // C1.3 "callback exception boundary": the handler half, shared by both doors foreign code
    // enters through (an `extern C delegate` target's guard thunk and an `export extern C`
    // wrapper). The builder must already sit in the invoke's unwind block. One catchpad, filtered
    // by __nativesharp_eh_filter -- which accepts this compiler's own exception code and NOTHING
    // else, so a foreign SEH exception (an access violation raised inside the callback, say) is
    // declined here and keeps propagating to whatever the host installed. The catchswitch unwinds
    // to caller for the same reason. Reaching the trap means a NativeSharp exception was about to
    // unwind foreign frames; the process stops here instead, with a defined exit code.
    bool emitCallbackBoundaryHandler(std::string& error);

    LLVMValueRef callbackGuardThunk(
        const std::string& targetName,
        std::string& error,
        bool vectorCall,
        bool defineBody = true);

    const frontend::ProjectOopMirFunction* findCallbackMirFunction(
        std::string_view targetName) const noexcept;

    LLVMTypeRef createCallbackGuardThunkType(
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool& internalSretReturn,
        bool& externalSretReturn,
        LLVMTypeRef& vectorReturnType,
        LLVMTypeRef& externalReturnType,
        std::string& error);

    void applyCallbackGuardThunkAttributes(
        LLVMValueRef thunk,
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool externalSretReturn);

    bool prepareCallbackGuardArguments(
        LLVMValueRef thunk,
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool internalSretReturn,
        bool externalSretReturn,
        std::vector<LLVMValueRef>& forwarded,
        LLVMValueRef& internalSretSlot,
        std::string& error);

    bool defineCallbackGuardThunkBody(
        LLVMValueRef thunk,
        const FunctionInfo& target,
        const frontend::ProjectOopMirFunction& function,
        bool vectorCall,
        bool internalSretReturn,
        bool externalSretReturn,
        LLVMTypeRef vectorReturnType,
        LLVMMetadataRef subprogram,
        std::string& error);

    // C1.2 "import identity". A NUL-terminated byte array in the module, for the two strings a
    // runtime-bound import needs: its library name and its external symbol name. Deliberately not
    // the NativeSharp string-literal shape (length + bytes) -- LoadLibraryA/GetProcAddress want a
    // plain C string and nothing else.
    LLVMValueRef cStringGlobal(std::string_view text, std::string_view stem);

    // C1.2 "import identity": the body of an import whose C symbol name is claimed by more than
    // one library.
    //
    // WHY A BODY AT ALL. A PE import table has exactly one slot per (import library, symbol
    // NAME), and an import library names its symbol after the C symbol. So `a.dll!Init` and
    // `b.dll!Init` cannot both be resolved statically: the two would be the same linker symbol.
    // MEASURED before this existed -- declaring both produced two LLVM functions asking to be
    // called `Init`, LLVM renamed the second to `Init.1`, and lld ended the build with
    // "undefined symbol: Init.1". The general fix is to stop asking the STATIC import machinery
    // to distinguish two identities it cannot express, and bind that import the way Windows
    // itself binds an ambiguous name: by library handle at run time.
    //
    // WHAT IT COSTS, AND WHO PAYS IT. Only the colliding imports get this. Every import whose
    // name a single library claims -- which is every import in a normal project -- still resolves
    // statically by name through the import library, exactly as the certified user32/kernel32
    // scenarios do, with the same PE import directory. The cache global means the LoadLibrary/
    // GetProcAddress pair runs at most once per import per process.
    //
    // The forwarding call reuses the function's OWN prototype verbatim, which is what keeps this
    // ABI-correct for free: since Stage 40.2 that prototype already IS the Win64 C ABI for every
    // C-expressible type (sret returns, indirect aggregate parameters and all), so forwarding
    // every parameter unchanged forwards the ABI unchanged.
    bool defineRuntimeBoundImport(
        const frontend::ProjectOopMirFunction& function, std::string& error);

    bool ensureRuntimeImportBindingFunctions(std::string& error);

    bool emitRuntimeBoundForwarding(
        const frontend::ProjectOopMirFunction& function,
        LLVMValueRef importFunction,
        LLVMTypeRef importType,
        LLVMValueRef target,
        std::string& error);

    bool isExternCDelegateType(std::string_view typeKey) const noexcept;

    bool isVectorCallDelegateType(std::string_view typeKey) const noexcept;


    bool defineCAbiWrappers(std::string& error);

    LLVMValueRef createCAbiWrapper(
        const frontend::ProjectOopMirFunction& function,
        bool sretReturn,
        LLVMTypeRef& returnType,
        LLVMMetadataRef& subprogram,
        std::string& error);

    bool defineCAbiWrapper(
        const frontend::ProjectOopMirFunction& function,
        const FunctionInfo& internal,
        std::string& error);

    bool defineValueDropEntries(std::string& error);

    bool defineDestructionEntries(std::string& error);

    LLVMValueRef beginObjectDestructionEntry(
        const frontend::ProjectOopMirDispatchTable& table,
        const FunctionInfo& objectEntry,
        SourceRange& debugRange);

    bool emitObjectDestructionSteps(
        const frontend::ProjectOopMirDispatchTable& table,
        LLVMValueRef object,
        std::string& error);

    bool emitDelegatedBaseDestruction(
        const frontend::ProjectOopMirDispatchTable& table,
        LLVMValueRef object,
        std::string& error);

    void emitReleasingDestructionEntry(
        const FunctionInfo& entry,
        const FunctionInfo& objectEntry,
        const SourceRange& debugRange);

    bool defineFunction(const frontend::ProjectOopMirFunction& function, std::string& error);

    void prepareFunctionDefinitionState(
        const frontend::ProjectOopMirFunction& function,
        const FunctionInfo& info);

    bool initializeFunctionReceiver(
        const frontend::ProjectOopMirFunction& function,
        std::string& error);

    bool initializeCapturedThis(
        const frontend::ProjectOopMirFunction& function,
        std::string& error);

    bool initializeFunctionParameters(
        const frontend::ProjectOopMirFunction& function,
        bool sretReturn,
        std::string& error);

    bool initializeFunctionParameter(
        const frontend::ProjectOopMirFunction& function,
        std::size_t index,
        std::string& error);

    void emitPollStateDebugVariable(const frontend::ProjectOopMirFunction& function);

    bool prepareFunctionCleanupState(
        const frontend::ProjectOopMirFunction& function,
        std::string& error);

    bool finishFunctionDefinition(
        const frontend::ProjectOopMirFunction& function,
        std::string& error);

    LLVMValueRef collectStaticLocalStorage(
        const frontend::ProjectOopMirNode* node,
        std::string_view storageTypeKey,
        std::string& error);

    void collectLocalAllocas(const frontend::ProjectOopMirNode* node, std::string& error);

    bool collectLocalStorage(
        const frontend::ProjectOopMirNode* node,
        bool& stopAfterChildren,
        std::string& error);

    void collectStackPromotedObjectStorage(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    // Emits the guarded entry to a cleanup: branches to `doneBlock` when this owner slot is already
    // empty and to `destroyBlock` otherwise, clearing the slot on the way in. Shared by
    // emitRaiiDestroy and emitOwnedParameterDestroy so the two can never drift apart on the
    // exactly-once guarantee.
    //
    // HISTORY -- do NOT re-add a separate "already destroyed" flag here. One was added for bug
    // B-006 (an enclosing cleanup running during the unwind to a same-frame catch, destroying an
    // object still in scope), back when the compiler still depended on that unwind edge and had to
    // survive the cleanup being entered twice. B-006 was then fixed STRUCTURALLY instead -- the
    // catchswitch unwinds to caller and the enclosing cleanups run explicitly on the
    // no-clause-matched path, see lowerTryRegion -- which removed the double entry at its source
    // and left the flag with nothing to protect against.
    //
    // Worse, the flag was actively wrong. It lived per STORAGE SLOT, but a slot legitimately holds
    // a SEQUENCE of owners over its lifetime through move-assignment (`a = move(b)`): destroying
    // the first value set the flag permanently, so the value moved in afterwards was never
    // destroyed at scope exit. That is a silent leak, and it was caught by real execution in
    // tests/unit/Stage50_11OwnershipExecutionTests.cpp, not by any IR-shape check. The owner
    // slot's own null state is the correct guard precisely because storing a new owner into the
    // slot re-arms it automatically.
    void emitCleanupGuardBranch(
        LLVMValueRef ownerStorage,
        LLVMValueRef ownerValue,
        LLVMBasicBlockRef destroyBlock,
        LLVMBasicBlockRef doneBlock,
        const char* namePrefix);

    std::optional<std::size_t> activeCleanupIndex(
        std::size_t ordinal,
        const std::vector<std::size_t>& active) const noexcept;

    LLVMValueRef activeFuncletPad() const noexcept;

    LLVMValueRef buildNoUnwindCall(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        const char* name);

    LLVMValueRef buildInvokeInCurrentFunclet(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        LLVMBasicBlockRef normalBlock,
        LLVMBasicBlockRef unwindBlock,
        const char* name);

    LLVMBasicBlockRef emitExceptionalCleanupChain(
        const std::vector<std::size_t>& cleanupPath,
        LLVMBasicBlockRef finalUnwindTarget,
        std::string& error);

    bool resolveExceptionalCleanupPath(
        const std::vector<std::size_t>& cleanupPath,
        const std::vector<std::size_t>& savedActive,
        const std::vector<FuncletContext>& savedFunclets,
        LLVMBasicBlockRef sourceBlock,
        std::vector<std::size_t>& regionIndices,
        std::vector<LLVMValueRef>& parentPads,
        std::vector<std::size_t>& parentPrefixes,
        std::string& error);

    LLVMBasicBlockRef buildCurrentUnwindDestination(std::string& error);

    LLVMValueRef buildPotentiallyThrowingCall(
        LLVMTypeRef functionType,
        LLVMValueRef function,
        LLVMValueRef* arguments,
        unsigned argumentCount,
        const char* name,
        std::string& error);

    bool findInterfaceCleanupTarget(
        std::size_t contractMemberOrdinal,
        std::uint64_t& stableInterfaceId,
        std::size_t& slot) const noexcept;

    bool emitInterfaceDispose(
        LLVMValueRef receiver,
        std::size_t contractMemberOrdinal,
        std::string& error);

    bool emitInterfaceDisposeSearch(
        LLVMValueRef receiver,
        LLVMValueRef map,
        std::uint64_t interfaceId,
        std::size_t slot,
        std::string& error);

    bool emitInterfaceDisposeTarget(
        LLVMValueRef receiver,
        LLVMValueRef entry,
        std::size_t slot,
        LLVMBasicBlockRef doneBlock,
        std::string& error);

    bool emitUsingDispose(const frontend::ProjectOopMirNode* node, std::string& error);

    // NativeSharp 3.0 T31. A native array owns every ownable reference stored in its element
    // slots. Destruction clears each slot before descending, destroys nested arrays recursively,
    // and releases the outer allocation last. All owning element shapes are pointer-sized, so
    // their canonical array data offset and stride are both eight on the Windows-x64-only ABI.
    bool emitNativeArrayDestroy(
        LLVMValueRef array,
        std::string_view arrayTypeKey,
        std::string_view stem,
        bool destroyElements,
        std::string& error);

    bool emitNativeArrayElementDestruction(
        LLVMValueRef array,
        std::string_view arrayTypeKey,
        std::string_view elementType,
        const std::string& prefix,
        LLVMBasicBlockRef destroyBlock,
        std::string& error);

    bool emitOwnedPointerDestroy(
        LLVMValueRef value,
        std::string_view typeKey,
        std::string_view stem,
        std::string& error);

    bool emitVariantDropAddress(
        LLVMValueRef address,
        const frontend::ProjectOopMirStructLayout& layout,
        std::string_view stem,
        std::string& error);

    void collectVariantCaseIndices(
        const frontend::ProjectOopMirStructLayout& layout,
        std::unordered_map<std::string, std::uint64_t>& caseIndices) const;

    bool collectVariantDropCases(
        const frontend::ProjectOopMirStructLayout& payloadLayout,
        const std::unordered_map<std::string, std::uint64_t>& caseIndices,
        std::vector<DropCase>& dropCases,
        std::string& error) const;

    bool emitVariantDropCases(
        LLVMValueRef address,
        const frontend::ObjectFieldLayout& payloadField,
        LLVMTypeRef tagType,
        LLVMValueRef tag,
        const std::vector<DropCase>& dropCases,
        std::string_view stem,
        std::string& error);

    bool emitValueDropAddress(
        LLVMValueRef address,
        std::string_view typeKey,
        std::string_view stem,
        std::string& error,
        bool allowNamedThunk = true);

    bool emitFixedArrayDropAddress(
        LLVMValueRef address,
        std::string_view typeKey,
        std::string_view stem,
        bool& handled,
        std::string& error);

    bool emitStructValueDropMembers(
        LLVMValueRef address,
        const frontend::ProjectOopMirStructLayout& layout,
        std::string_view stem,
        std::string& error);

    bool emitValueDrop(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        LLVMValueRef flag,
        std::string_view stem,
        std::string& error);

    bool emitRaiiDestroy(const frontend::ProjectOopMirNode* node, std::string& error);

    bool emitRaiiSpecialDestroy(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef value,
        LLVMBasicBlockRef doneBlock,
        bool& handled,
        std::string& error);

    bool emitOwnedParameterDestroy(const frontend::ProjectOopMirNode* node, std::string& error);

    bool emitCleanupAction(std::size_t ordinal, std::string& error);

    bool emitOneStructuredCleanup(std::size_t ordinal, std::string& error);

    // CC1 EH-001 -- "does this structured exit leave the body of an enclosing `catch`?"
    //
    // The invariant being repaired: the NORMAL end of a catch clause falls through
    // `eh.catch.handled.N`, which calls __nativesharp_exception_clear and drops the thread-local
    // current exception. A `break`, `continue` or `return` out of that same clause branches
    // straight to its structured target and never passes through that block, so the same clause
    // could end with the TLS slot cleared or not cleared depending purely on WHICH legal exit the
    // program took. Clearing the current exception is a semantic obligation of leaving a catch,
    // not a property of falling into one particular LLVM block, so it is decided here.
    //
    // `return` leaves every clause it sits inside. `break`/`continue` target loopStack_.back();
    // that frame leaves the innermost clause exactly when it already existed when the clause body
    // began. Testing the innermost clause is enough: an exit that does not leave the innermost one
    // leaves none, and one call clears the single TLS slot however many it leaves.
    bool structuredExitLeavesCatchBody(
        bool isReturn, std::size_t targetLoopIndex) const noexcept;

    // The clear is emitted into a block of its own rather than inline, so that what the obligation
    // produced is NAMEABLE in the emitted IR and can be asserted by a test instead of reasoned
    // about. -O2 folds the block away; the call survives.
    void emitCatchBodyExceptionClear();

    bool lowerStructuredExit(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef returnValueOverride = nullptr);

    bool resolveStructuredExitTarget(
        const frontend::ProjectOopMirNode* node,
        bool isBreak,
        bool isContinue,
        std::size_t& targetLoopIndex,
        std::size_t& targetFuncletDepth,
        std::string& error);

    bool emitSimpleStructuredExit(
        bool isReturn,
        bool isBreak,
        std::size_t targetLoopIndex,
        LLVMValueRef returnValue,
        std::string& error);

    bool emitStructuredExitFunclets(
        const frontend::ProjectOopMirNode* node,
        std::size_t targetFuncletDepth,
        std::size_t& pathIndex,
        std::string& error);

    bool emitRemainingStructuredCleanups(
        const frontend::ProjectOopMirNode* node,
        std::size_t& pathIndex,
        std::string& error);

    bool finishComplexStructuredExit(
        bool isReturn,
        bool isBreak,
        std::size_t targetLoopIndex,
        LLVMValueRef returnValue,
        std::string& error);

    bool lowerUsingRegion(const frontend::ProjectOopMirNode* node, std::string& error);

    bool lowerExceptionalTransfer(
        LLVMValueRef exception,
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    void emitThrowReturnedTrap();

    bool lowerNativePanicTransfer(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef* arguments,
        std::string& error);

    bool lowerRethrow(const frontend::ProjectOopMirNode* node, std::string& error);

    bool lowerInlineAssembly(const frontend::ProjectOopMirNode* node, std::string& error);

    bool collectInlineAssemblyRecords(
        const frontend::ProjectOopMirNode* node,
        std::vector<const frontend::ProjectOopMirNode*>& lines,
        std::vector<const frontend::ProjectOopMirNode*>& inputs,
        std::vector<const frontend::ProjectOopMirNode*>& outputs,
        std::vector<const frontend::ProjectOopMirNode*>& clobbers,
        std::string& error);

    bool collectInlineAssemblyOutputs(
        const std::vector<const frontend::ProjectOopMirNode*>& outputs,
        std::vector<std::string>& constraintParts,
        std::vector<LLVMTypeRef>& resultTypes,
        std::vector<LLVMValueRef>& resultAddresses,
        std::vector<LLVMTypeRef>& parameterTypes,
        std::vector<LLVMValueRef>& arguments,
        std::vector<std::pair<std::size_t, LLVMTypeRef>>& indirectElementTypes,
        std::vector<std::pair<LLVMValueRef, LLVMTypeRef>>& readWriteMemoryInputs,
        std::string& error);

    bool collectInlineAssemblyInputs(
        const std::vector<const frontend::ProjectOopMirNode*>& inputs,
        std::vector<std::string>& constraintParts,
        std::vector<LLVMTypeRef>& parameterTypes,
        std::vector<LLVMValueRef>& arguments,
        std::vector<std::pair<std::size_t, LLVMTypeRef>>& indirectElementTypes,
        std::string& error);

    bool collectInlineAssemblyClobbers(
        const frontend::ProjectOopMirNode* node,
        const std::vector<const frontend::ProjectOopMirNode*>& clobbers,
        std::vector<std::string>& constraintParts,
        std::string& error);

    bool rewriteInlineAssemblyPlaceholders(
        std::string_view source,
        std::size_t outputCount,
        const std::vector<std::size_t>& inputConstraintOrdinals,
        std::string& rewritten,
        std::string& error);

    bool buildInlineAssemblyText(
        const std::vector<const frontend::ProjectOopMirNode*>& lines,
        std::size_t outputCount,
        const std::vector<std::size_t>& inputConstraintOrdinals,
        std::string& assembly,
        std::string& error);

    bool emitInlineAssemblyCall(
        const frontend::ProjectOopMirNode* node,
        const std::vector<std::string>& constraintParts,
        const std::vector<LLVMTypeRef>& resultTypes,
        const std::vector<LLVMValueRef>& resultAddresses,
        const std::vector<LLVMTypeRef>& parameterTypes,
        const std::vector<LLVMValueRef>& arguments,
        const std::vector<std::pair<std::size_t, LLVMTypeRef>>& indirectElementTypes,
        std::string_view assembly,
        std::string& error);

    std::optional<StaticLocalGuard> beginStaticLocalInitialization(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    StaticLocalStorage* findStaticLocalStorage(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    void emitStaticLocalFastState(
        const StaticLocalStorage& storage,
        LLVMTypeRef i8,
        LLVMValueRef two,
        LLVMValueRef three,
        LLVMBasicBlockRef done,
        LLVMBasicBlockRef fastFailure,
        LLVMBasicBlockRef failed,
        LLVMBasicBlockRef casAttempt);

    bool finishStaticLocalInitialization(const StaticLocalGuard& guard, std::string& error);

    bool emitStaticLocalDestruction(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error);

    bool defineStaticLocalDestructors(std::string& error);

    bool lowerStatement(const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerBlockStatement(const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerExpressionStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerGeneratorFrameInitStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerLocalDeclarationStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    bool initializeAggregateLocalDeclaration(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        bool& initialized,
        std::string& error);
    bool lowerSuspendPointStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerYieldValueStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef prepareYieldValueFrame(
        LLVMValueRef self,
        LLVMBasicBlockRef entryBlock,
        std::string& error);
    bool restoreYieldValueFrame(LLVMValueRef self, std::string& error);
    bool lowerYieldBreakStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerAsyncYieldValueStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);
    bool prepareAsyncYieldRun(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef future,
        LLVMValueRef asyncIterator,
        LLVMTypeRef intType,
        LLVMBasicBlockRef& runBlock,
        std::string& error);
    LLVMValueRef prepareAsyncYieldFrame(
        LLVMValueRef asyncIterator,
        LLVMBasicBlockRef entryBlock,
        std::string& error);
    LLVMValueRef lowerAsyncYieldItem(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef frame,
        std::string& error);
    bool restoreAsyncYieldFrame(
        LLVMValueRef asyncIterator,
        std::string& error);
    bool lowerDropFrameStatement(
        const frontend::ProjectOopMirNode* node, std::string& error);

    // C7 "definite assignment", backend half. A local declared without an initializer used to be
    // an `alloca` and nothing else, so reading it returned whatever the stack happened to hold --
    // silently, at every optimization level, for every type. This gives that declaration a
    // meaning: the slot is zeroed at the point the declaration executes.
    //
    // Why at the declaration and not next to the entry-block alloca: a local declared inside a
    // loop body is a fresh variable on each iteration, and its default has to be re-established on
    // each one. The allocas are hoisted to the entry block (see collectLocalAllocas) purely so
    // mem2reg can promote them; the semantics live here.
    //
    // What this costs: nothing in a program the front-end analysis accepts. There, every read is
    // dominated by a real store, so the zeroing store is dead and SROA/mem2reg delete it. It is
    // paid for only where the program relies on the default -- an aggregate filled field by field,
    // which is exactly the C-style struct idiom the analysis deliberately permits.
    bool emitDefaultLocalStorage(const frontend::ProjectOopMirNode* node, std::string& error);

    bool lowerTryRegion(const frontend::ProjectOopMirNode* node, std::string& error);

    bool collectTryHandlers(
        const frontend::ProjectOopMirNode* node,
        std::vector<const frontend::ProjectOopMirNode*>& catchNodes,
        const frontend::ProjectOopMirNode*& finallyNode,
        std::string& error);

    void createTryHandlerBlocks(
        std::size_t catchCount,
        bool hasFinally,
        LLVMBasicBlockRef& continueBlock,
        LLVMBasicBlockRef& normalFinallyBlock,
        LLVMBasicBlockRef& dispatchBlock,
        std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        std::vector<LLVMBasicBlockRef>& catchBlocks,
        std::vector<LLVMBasicBlockRef>& handlerBlocks,
        std::vector<LLVMBasicBlockRef>& handledBlocks);

    bool lowerTryProtectedBody(
        const frontend::ProjectOopMirNode* node,
        bool hasFinally,
        LLVMBasicBlockRef normalFinallyBlock,
        LLVMBasicBlockRef continueBlock,
        LLVMBasicBlockRef dispatchBlock,
        bool hasCatches,
        std::string& error);

    bool beginTryCatchDispatch(
        LLVMBasicBlockRef dispatchBlock,
        bool hasFinally,
        LLVMValueRef& caughtException,
        LLVMBasicBlockRef& unmatchedBlock,
        std::string& error);

    bool emitTryCatchClauseTests(
        const std::vector<const frontend::ProjectOopMirNode*>& catchNodes,
        const std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        const std::vector<LLVMBasicBlockRef>& catchBlocks,
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        bool hasFinally,
        std::string& error);

    bool emitUnmatchedTryCatch(
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        bool hasFinally,
        std::string& error);

    bool emitTryCatchBodies(
        const std::vector<const frontend::ProjectOopMirNode*>& catchNodes,
        const std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        const std::vector<LLVMBasicBlockRef>& catchBlocks,
        const std::vector<LLVMBasicBlockRef>& handlerBlocks,
        const std::vector<LLVMBasicBlockRef>& handledBlocks,
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        LLVMBasicBlockRef normalFinallyBlock,
        LLVMBasicBlockRef continueBlock,
        bool hasFinally,
        std::string& error);

    bool emitOneTryCatchBody(
        const frontend::ProjectOopMirNode* catchNode,
        std::size_t index,
        const std::vector<LLVMBasicBlockRef>& clauseTestBlocks,
        LLVMBasicBlockRef catchBlock,
        LLVMBasicBlockRef handlerBlock,
        LLVMBasicBlockRef handledBlock,
        LLVMValueRef caughtException,
        LLVMBasicBlockRef unmatchedBlock,
        LLVMBasicBlockRef normalFinallyBlock,
        LLVMBasicBlockRef continueBlock,
        bool hasFinally,
        std::string& error);

    bool lowerThrow(const frontend::ProjectOopMirNode* node, std::string& error);

    bool lowerIf(const frontend::ProjectOopMirNode* node, std::string& error);

    bool lowerWhile(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerLoopExpression(
        const frontend::ProjectOopMirNode* node, std::string& error);

    // `do { body } while (cond);` -- the same loop as lowerWhile with one difference: the entry
    // branch goes to the BODY instead of to the condition, so the body always runs once.
    //
    // `continue` still lands on the condition block, which is what both C and C# specify: it skips
    // the rest of the body and goes to the test, not back to the top of the body. That falls out
    // of pushing the same {end, cond} pair onto loopStack_ as a while does.
    bool lowerDoWhile(const frontend::ProjectOopMirNode* node, std::string& error);

    // `switch (governing) { case a: ... }` lowers to ONE `LLVMBuildSwitch`, which is what the
    // whole construct is for: LLVM turns that into a jump table when the label values are dense
    // enough and into a balanced decision tree when they are not. The chain of `else if` it
    // replaces is n comparisons in the worst case no matter what the values look like, and a
    // WndProc is forty of them.
    //
    // NO FALL-THROUGH, so the block layout is flat: each case block ends by branching to
    // switch.end -- exactly as if the section had written `break` -- rather than falling into the
    // next section's block. `break` inside a section produces the same branch through the ordinary
    // structured-exit path, which is why `break` stays legal and means what it always meant.
    //
    // WHEN THERE IS NO `default`, the switch's default target is switch.end: an unmatched value
    // skips the whole statement.
    bool lowerSwitch(const frontend::ProjectOopMirNode* node, std::string& error);

    bool lowerFor(const frontend::ProjectOopMirNode* node, std::string& error);

    // NSC305-L04. The leaf expression family (literals/names) — the expression machine's first
    // family. These cases have no sub-expression recursion; the machine dispatches them directly.
    LLVMValueRef lowerLeafExpression(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerParameterValue(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    // NSC305-L04. The address sub-family of the member/index family. These cases compute an address
    // (not a value); the machine dispatches them directly.
    LLVMValueRef lowerAddressExpression(const frontend::ProjectOopMirNode* node, std::string& error);

    // NSC305-L04. The load sub-family of the member/index family. These cases load a value through
    // a member/index address; the machine dispatches them directly.
    LLVMValueRef lowerLoadExpression(const frontend::ProjectOopMirNode* node, std::string& error);

    // NSC305-L04. The exception sub-family of the cleanup/exceptions/async family (family 6/6).
    LLVMValueRef lowerExceptionExpression(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerPanicExpression(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    // NSC305-L04. The cleanup sub-family of the cleanup/exceptions/async family (family 6/6).
    LLVMValueRef lowerCleanupExpression(const frontend::ProjectOopMirNode* node, std::string& error);

    // NSC305-L04. The iterator sub-family of the cleanup/exceptions/async family (family 6/6).
    LLVMValueRef lowerIteratorExpression(const frontend::ProjectOopMirNode* node, std::string& error);

    // NSC305-L04. The await sub-family of the cleanup/exceptions/async family (family 6/6).
    LLVMValueRef lowerAwaitExpression(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef childOverride = nullptr);
    std::uint64_t futureFieldOffset(
        std::string_view futureTypeKey,
        std::size_t memberOrdinal) const;
    bool resolveAwaitPollTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        std::string_view addressName,
        std::string_view loadName,
        LLVMValueRef& pollFunction,
        LLVMTypeRef& pollFunctionType,
        std::string& error);
    LLVMValueRef allocateAwaitFrame(
        std::uint64_t frameSize,
        bool frameOnHeap,
        std::string_view stackName,
        std::string_view heapName,
        std::string_view heapFailure,
        std::string_view frameFailure,
        std::string& error);
    bool attachBlockingAwaitFrame(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        LLVMValueRef& heapFrame,
        std::string& error);
    bool emitBlockingAwaitPollLoop(
        LLVMValueRef futureSlot,
        LLVMValueRef pollFunction,
        LLVMTypeRef pollFunctionType,
        LLVMTypeRef intType,
        LLVMBasicBlockRef& loopDone,
        LLVMBasicBlockRef& loopError,
        std::string& error);
    void releaseAwaitHeapFrame(LLVMValueRef heapFrame);
    bool emitBlockingAwaitErrorPath(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        LLVMValueRef heapFrame,
        LLVMBasicBlockRef loopMerge,
        bool& transfersException,
        std::string& error);
    LLVMValueRef finishBlockingAwait(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef futureSlot,
        LLVMValueRef heapFrame,
        LLVMBasicBlockRef loopDone,
        LLVMBasicBlockRef loopError,
        std::string& error);
    LLVMValueRef lowerBlockingAwait(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef childOverride);
    void awaitChildFrameInfo(
        const frontend::ProjectOopMirNode* node,
        bool& frameOnHeap,
        std::uint64_t& frameSize) const;
    bool emitAwaitChildSelection(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef childOverride,
        LLVMValueRef self,
        LLVMTypeRef intType,
        LLVMTypeRef carrier,
        LLVMValueRef childSlot,
        bool childFrameOnHeap,
        std::uint64_t childFrameSize,
        LLVMBasicBlockRef& pollBlock,
        std::string& error);
    void emitAwaitSuspendPath(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef self,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        LLVMTypeRef carrier,
        LLVMBasicBlockRef suspendBlock);
    void releaseAwaitChildFrame(LLVMValueRef childSlot);
    void emitSuspendingAwaitErrorPath(
        LLVMValueRef self,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        LLVMTypeRef errorType,
        std::uint64_t errorOffset,
        bool childFrameOnHeap,
        LLVMBasicBlockRef errorBlock);
    LLVMValueRef finishSuspendingAwait(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef self,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        LLVMTypeRef carrier,
        bool childFrameOnHeap,
        std::string& error);
    LLVMValueRef lowerSuspendingAwait(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef childOverride);
    // NSC305-L04. The iterative expression machine's trampoline (first family: unary/binary). The
    // unary/binary self-calls are replaced by frame pushes; the sub-expressions receive their
    // values from a ValueStack. The remaining families still use the recursive lowerExpression and
    // are migrated family by family in later tandas.
    LLVMValueRef lowerExpressionIterative(const frontend::ProjectOopMirNode* node, std::string& error);
    bool lowerExpressionFrameGroup01(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup02(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup03(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup04(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup05(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup06(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup07(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup08(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup09(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrameGroup10(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame001Unary(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame002Binary(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerShortCircuitBinaryFrame(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values);
    void lowerOrdinaryBinaryFrame(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame003FieldAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame004FieldLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame005BitFieldLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame006ArrayElementAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    void finishArrayElementFrame(
        const frontend::ProjectOopMirNode* current, LLVMValueRef object,
        LLVMValueRef index, bool stringValue,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame007FutureFieldAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame008OwnedTemporaryBorrow(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame009FullExpressionCleanup(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame010OverflowPolicy(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame011IteratorGetEnumerator(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame012IteratorCurrent(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame013IteratorDrop(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame014IteratorCancel(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame015IteratorMoveNextAsync(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame016IteratorMoveNext(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame017BorrowValue(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame018DirectInstanceCall(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame019DelegateInvoke(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame020DirectStaticCall(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame021ObjectDestroy(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame022Deallocate(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame023ValueConstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame024AllocateAndConstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame025PlacementConstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame026AwaitFuture(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame027AggregateInitialize(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame028AddressValue(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame029MoveArrayElement(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame030PointerLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame031MoveOwnedField(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame032MoveIteratorCurrent(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame033RawElementAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame034PointerArithmetic(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame035CompoundStore(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame036StackAllocation(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame037NullConditionalFieldLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame038NullConditionalArrayLoad(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame039NullConditionalFieldStore(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame040NullConditionalArrayStore(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame041Convert(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame042Assert(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame043ResultPropagation(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame044Store(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame045TupleDeconstruct(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame046ParameterPack(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame047CoreConsoleWriteLine(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame048CoreConsoleReadLine(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame049CoreEnvironmentExit(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame050CoreEnvironmentGetVariable(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame051CoreEnvironmentSetVariable(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame052CoreFileExists(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame053CoreFileReadAllText(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame054CoreFileReadBytes(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame055CoreFileWriteAllBytes(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame056CoreFileWriteAllText(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame057CoreDirectoryDelete(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame058CoreEnvironmentGetCommandLineArgs(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame059CoreDirectoryGetFiles(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame060CoreThreadCreate(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame061CoreThreadJoin(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame062CoreMutexCreateNamed(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame063CoreMonitorEnter(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame064CoreSemaphoreRelease(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame065CoreSemaphoreWait(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame066CoreSemaphoreCreate(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame067CoreSemaphoreCreateNamed(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame068CoreProcessRun(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame069CoreProcessSetPriorityClass(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame070CoreFileCopy(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame071CoreFileMove(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame072CoreInterlockedIncrementInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame073CoreInterlockedIncrementInt64(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame074CoreInterlockedAddInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame075CoreInterlockedAddInt64(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame076CoreAtomicLoadInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame077CoreAtomicStoreInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame078CoreAtomicLoadPointer(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame079CoreAtomicStorePointer(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame080CoreInterlockedCompareExchangeInt32(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame081CoreInterlockedCompareExchangePointer(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame082CoreSimdLoadAligned128(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame083CoreSimdStoreAligned128(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame084CoreCpuPrefetchRead(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame085CoreHalfFromFloat(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame086CoreHalfToFloat(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame087CoreMonitorWait(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame088CoreSimdFloat32x4Multiply(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame089CoreSimdInt128Multiply(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame090TemporaryAddress(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame091CoreBitPopulationCount(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame092CoreBitRotateLeft(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame093CoreSimdFloat256Add(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame094CoreSimdInt256Add(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame095CoreSimdSqrt(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame096CoreSimdSwizzleWzyx(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    LLVMValueRef loadShuffleVector(
        LLVMTypeRef carrier, LLVMTypeRef vectorType,
        LLVMValueRef value, const char* name);
    bool lowerExpressionFrame097StringViewFromString(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    void finishStringViewFrame(
        const frontend::ProjectOopMirNode* current, LLVMValueRef object,
        LLVMValueRef index, LLVMValueRef count, LLVMValueRef countIsZero,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame098SpanFromArray(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    void finishArraySpanFrame(
        const frontend::ProjectOopMirNode* current, LLVMValueRef object,
        LLVMValueRef index, LLVMValueRef count, LLVMValueRef countIsZero,
        std::vector<LLVMValueRef>& values, std::string& error);
    bool lowerExpressionFrame099SpanFromFixedArray(
        const LlvmExpressionFrame& frame, std::vector<LlvmExpressionFrame>& frames,
        std::vector<LLVMValueRef>& values, std::string& error);
    void finishFixedArraySpanFrame(
        const frontend::ProjectOopMirNode* current, std::uint64_t fixedCount,
        LLVMValueRef base, LLVMValueRef start, LLVMValueRef countNative,
        LLVMValueRef zero64, std::vector<LLVMValueRef>& values, std::string& error);

    LLVMValueRef lowerExpression(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup01(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup02(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup03(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup04(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup05(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup06(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup07(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup08(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup09(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup10(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup11(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup12(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup12DiagnosticsAndUtilities(
        const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup13(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup14(const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerExpressionGroup15(const frontend::ProjectOopMirNode* node, std::string& error);

    bool emitGuard(LLVMValueRef valid, LlvmTrapReason reason, std::string_view stem, std::string& error);

    LLVMValueRef byteOffset(LLVMValueRef base, std::uint64_t offset, const char* name);

    std::pair<std::uint64_t, std::uint64_t> captureValueLayout(
        std::string_view typeKey, std::string& error) const;

    bool ensureCaptureCellRuntime(std::string& error);

    LLVMValueRef allocateCaptureCell(
        std::string_view typeKey,
        LLVMValueRef& valueAddress,
        std::string_view stem,
        std::string& error);

    LLVMValueRef captureCellValueAddress(
        LLVMValueRef cell,
        std::string_view typeKey,
        std::string_view stem,
        std::string& error);

    void retainCaptureCell(LLVMValueRef cell);

    void releaseCaptureCell(LLVMValueRef cell);

    void releaseOwnedCaptureCells();

    LLVMBasicBlockRef captureCellExceptionalCleanup(std::string& error);

    LLVMValueRef closureContextDestroy(
        std::string_view calleeName,
        std::size_t captureCount,
        std::string& error);

    LLVMValueRef createClosureContextDestroyFunction(
        const frontend::ProjectOopMirFunction& origin,
        std::string_view name,
        SourceRange& debugRange,
        LLVMMetadataRef& subprogram,
        std::string& error);

    bool dropClosureContextCaptures(
        LLVMValueRef environment,
        const frontend::ProjectOopMirFunction& origin,
        std::size_t captureCount,
        std::string& error);

    bool emitClosureContextDestroyFunctionBody(
        LLVMValueRef function,
        const frontend::ProjectOopMirFunction& origin,
        std::size_t captureCount,
        const SourceRange& debugRange,
        LLVMMetadataRef subprogram,
        std::string& error);

    LLVMValueRef checkedStage42Allocation(LLVMValueRef memory, std::string_view stem, std::string& error);

    LLVMValueRef allocateDelegateCell(
        LLVMValueRef code,
        LLVMValueRef receiver,
        LLVMValueRef contextDestroy,
        std::string_view stem,
        std::string& error);

    bool emitDelegateDestroy(LLVMValueRef cell, std::string_view stem, std::string& error);

    LLVMValueRef closureCaptureCell(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef closureCaptureAddress(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerClosureCaptureLoad(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerCaptureCellReference(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerLambdaCreate(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef createLambdaCaptureEnvironment(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerDelegateCreate(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef emitDelegateCall(
        LLVMValueRef delegateCell,
        const std::vector<LLVMValueRef>& argumentValues,
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        std::string& error);

    bool prepareDelegateCallTypes(
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        LLVMValueRef& sretSlot,
        LLVMTypeRef& returnType,
        LLVMTypeRef& staticFunctionType,
        LLVMTypeRef& instanceFunctionType,
        std::string& error);

    LLVMValueRef emitDelegateBranchCall(
        LLVMBasicBlockRef branchBlock,
        LLVMBasicBlockRef mergeBlock,
        LLVMTypeRef functionType,
        LLVMValueRef code,
        LLVMValueRef receiver,
        LLVMValueRef sretSlot,
        const std::vector<LLVMValueRef>& argumentValues,
        LLVMTypeRef returnType,
        std::string_view returnTypeKey,
        const char* resultName,
        unsigned sretPosition,
        LLVMBasicBlockRef& incomingBlock,
        std::string& error);

    LLVMValueRef emitExternCDelegateCall(
        LLVMValueRef functionPointer,
        const std::vector<LLVMValueRef>& argumentValues,
        const std::vector<std::string>& argumentTypeKeys,
        std::string_view returnTypeKey,
        std::string_view delegateTypeKey,
        bool vectorCall,
        std::string& error);

    bool appendExternCDelegateParameterTypes(
        const std::vector<std::string>& argumentTypeKeys,
        std::size_t fixedParameterCount,
        bool sretReturn,
        bool vectorCall,
        std::vector<LLVMTypeRef>& parameterTypes,
        std::string& error);

    void applyVectorCallArgumentAttributes(
        LLVMValueRef call,
        bool sretReturn,
        const std::vector<std::string>& argumentTypeKeys);

    LLVMValueRef lowerDelegateInvoke(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef delegateOverride = nullptr,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    LLVMValueRef delegateCellsEqual(LLVMValueRef left, LLVMValueRef right);

    LLVMValueRef lowerEventAdd(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerEventRemove(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerEventInvoke(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef headOverride = nullptr,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    // C8 "type root", struct half. Both members below read the struct's SCALAR LEAVES -- the flat
    // list of primitive fields with absolute byte offsets that the HIR pass computed (see
    // ProjectOopHirModel::appendAggregateLeaves). Padding is not in that list, deliberately.
    //
    // The one shared step: an operand of struct type arrives as a VALUE here (a carrier integer or
    // a loaded [N x i8]), and leaves are reached by address, so the value gets a home first.
    LLVMValueRef spillAggregate(const frontend::ProjectOopMirNode* node, std::size_t index, std::string& error);

    // Loads one leaf as raw BITS, widened to i64. Nothing here asks what TYPE the leaf is, only
    // how many bytes it occupies (the MIR pass resolved that from the same value layout everything
    // else in this compiler reads): memory is untyped, so a float leaf is loaded as an i32 and a
    // reference leaf as an i64 without a single cast instruction.
    //
    // Reading bits rather than values is the whole contract. It is what makes the synthesized
    // Equals and GetHashCode agree by construction -- a "compare floats with fcmp, hash their
    // bits" pairing would not, since +0.0 and -0.0 compare equal but hash apart, and a NaN would
    // never equal itself while hashing identically.
    LLVMValueRef loadLeafBits(
        LLVMValueRef base,
        const frontend::ProjectOopMirNode* leaf,
        std::string_view stem,
        std::string& error);

    LLVMValueRef lowerAggregateEquals(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerAggregateHash(const frontend::ProjectOopMirNode* node, std::string& error);

    // C8 "type root": materializes one type-name constant with the exact layout a NativeSharp
    // string has -- {i64 length, [N+1 x i8] bytes}. Deliberately the same shape lowerStringLiteral
    // builds, because the value ends up flowing through the language as an ordinary `string`.
    LLVMValueRef internTypeNameConstant(const std::string& globalName, const std::string& text);

    // C8 "type root": `RuntimeHelpers.TypeName(value)` -- the name stored in the object's class
    // descriptor, handed back as an ordinary `string`. The descriptor sits at offset 0 of every
    // object and its field 6 already points at a constant with the exact layout a NativeSharp
    // string has, so this is two loads and no allocation. A null reference answers a null string
    // rather than faulting: every string operation in this compiler is already null-tolerant (see
    // nullableStringLength), and a helper that traps would be worse than one that tells the truth.
    LLVMValueRef lowerObjectTypeName(const frontend::ProjectOopMirNode* node, std::string& error);

    // C8 "type root": `RuntimeHelpers.ReferenceHash(value)` -- the reference's own address, mixed
    // down to an int. The mix is the final avalanche of SplitMix64, chosen because it maps zero to
    // zero (so a null reference hashes to 0 without a branch) and because a raw address makes a
    // poor hash on its own: allocations are aligned, so the low bits a bucket index reads are
    // always the same. Not stable across runs -- ASLR sees to that -- exactly like real C#'s
    // RuntimeHelpers.GetHashCode.
    LLVMValueRef lowerObjectReferenceHash(const frontend::ProjectOopMirNode* node, std::string& error);

    // ID 129. `ToString()` de un enum -- el NOMBRE del enumerador.
    //
    // FORMA DEL NODO: children[0] el valor, children[1] la cadena de reserva (el nombre del tipo),
    // y de ahi en adelante PARES (constante, nombre) en orden de declaracion.
    //
    // SE BAJA A UN `switch` CON UN `phi`. La clave del switch es el PATRON DE BITS promovido sin
    // signo a i64, no el entero estrecho original:
    //
    //     %bits = zext i8 <valor> to i64
    //     switch i64 %bits, label %reserva [ i64 K1, label %n1  i64 K2, label %n2 ... ]
    //     %resultado = phi ptr [ S1, %n1 ], [ S2, %n2 ], ..., [ <reserva>, %reserva ]
    //
    // POR QUE NO ES UNA CADENA DE `select`, QUE ES LO QUE ERA HASTA EL 2026-09-04. Lo era, y la
    // cadena que emitia es CORRECTA: `icmp eq i8 %v, -3` seguido de `select` sobre punteros a
    // globales constantes. LLVM 18 la compila bien a -O2 --comprobado pasandole el IR de c38 por
    // `opt -O2`, que pliega la funcion entera hasta `ret i32 42`--. Con LLVM 23, que es contra el
    // que enlaza el compilador que se entrega, a -O2 devuelve la RESERVA.
    //
    // LO MEDIDO, y es lo unico sobre lo que se decidio (corrida del 2026-09-04):
    //
    //   * `c38` codigo 39 y `b9` codigo 61: `ConSigno.MenosTres.ToString()` --`enum : sbyte`,
    //     valor -3-- devuelve "ConSigno", el nombre del tipo. Con receptor CONSTANTE y con
    //     receptor opaco, o sea que no depende de que el valor cruce una llamada.
    //   * la capa 1 de `b9` pasa: el valor de 8 bits SIGUE siendo -3 al llegar a ToString().
    //   * `c38` codigo 26 PASA: `ClasificaConSigno`, que es un `switch` del lenguaje sobre el
    //     MISMO enum con el MISMO -3, acierta a -O2. En el IR eso es
    //     `switch i8 %v, [ i8 -3, ... ]`.
    //   * `c38` codigos 37, 38 y 50-53 pasan: un `enum : byte` con 200 y 128 --que en i8 son -56
    //     y -128-- acierta con la cadena de `select`, y los alias tambien.
    //
    // La primera correccion intento conservar `i8 -3` y cambiar solamente `select` por `switch`.
    // b9 demostro que no bastaba: LLVMRunPasses de LLVM 23 plegaba TAMBIEN ese switch al bloque de
    // reserva, aunque el IR impreso era correcto; el mismo IR serializado y pasado a opt.exe se
    // plegaba bien. Normalizar ambos lados al patron i64 elimina de la decision del optimizador la
    // unica forma conflictiva (una constante negativa estrecha) sin cambiar el valor observable
    // ni el ABI del enum. La igualdad de enums depende de sus bits, y esos bits son precisamente
    // los que ya se usan abajo para deduplicar aliases.
    //
    // LO QUE SE GANA DE PASO, y esto si es una mejora por si misma: EL ALIAS DEJA DE DEPENDER DEL
    // ORDEN DEL BUCLE. Un `switch` de LLVM no admite dos casos con el mismo valor --el
    // verificador lo rechaza-- asi que los pares se deduplican aqui y gana EL PRIMERO DECLARADO,
    // que es la regla que documenta el frontend y que fija la fixture c34 (`Solo = 1` declarado
    // despues de `Lectura = 1` sale "Lectura"). Antes esa regla se conseguia recorriendo los
    // pares al reves para que el primero quedara en el `select` de mas afuera: correcto, pero
    // invertible por accidente con un cambio de bucle -- y ya se invirtio una vez. Ahora esta
    // escrita como lo que es, una eleccion entre iguales, y no como un efecto del anidamiento.
    //
    // EL RECEPTOR SE SIGUE BAJANDO UNA SOLA VEZ, que es la propiedad por la que este nodo existe:
    // `Dame().ToString()` no puede llamar a `Dame()` una vez por enumerador.
    LLVMValueRef lowerEnumName(const frontend::ProjectOopMirNode* node, std::string& error);

    std::vector<std::pair<std::uint64_t, std::size_t>> collectEnumNameCases(
        const frontend::ProjectOopMirNode* node,
        std::uint64_t mask) const;

    static std::uint64_t enumNameMask(unsigned governingWidth) noexcept;

    LLVMValueRef internStringLiteral(std::string_view value, std::string& error);

    LLVMValueRef lowerStringLiteral(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerUtf8StringLiteral(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef nullableStringLength(LLVMValueRef value, std::string_view stem);

    LLVMValueRef dynamicByteOffset(
        LLVMValueRef base,
        LLVMValueRef offset,
        const char* name);

    bool emitMemcpyBytes(
        LLVMValueRef destination,
        LLVMValueRef source,
        LLVMValueRef length,
        std::string_view stem,
        std::string& error);

    LLVMValueRef allocateNativeString(
        LLVMValueRef length,
        std::string_view stem,
        std::string& error);

    LLVMValueRef lowerStringConcat(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerStringEquals(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    void emitEqualLengthStringComparison(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef leftLength,
        LLVMBasicBlockRef doneBlock,
        LLVMBasicBlockRef& byteMismatchIncoming,
        LLVMBasicBlockRef& equalIncoming);

    LLVMValueRef lowerStringSubstring(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerStringFromCharArray(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef normalizeNativeIndex(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    // NSC305-L04. The conversion half of normalizeNativeIndex, separated so the iterative machine
    // can evaluate the index operand as a frame and convert the resulting value here.
    LLVMValueRef normalizeNativeIndexFromValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef value,
        std::string& error);

    LLVMValueRef lowerNativeLength(
        const frontend::ProjectOopMirNode* node,
        bool stringValue,
        std::string& error);

    // NSC305-L04. The array/string element-address compute (the bounds check + the GEP), separated
    // from the receiver/index evaluation so the iterative machine can push them as frames.
    LLVMValueRef arrayElementAddressFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        LLVMValueRef index,
        bool stringValue,
        std::string& error);

    // NSC305-L04. The C2.1 raw element-address compute (flexible-array bounds + byte-wise GEP),
    // separated from the base/index evaluation so the iterative machine can push them as frames.
    LLVMValueRef rawElementAddressFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef base,
        LLVMValueRef index,
        std::string& error);

    // NSC305-L04. The CC1 pointer-arithmetic compute (ptrdiff or byte-wise GEP), separated from the
    // operand evaluation so the iterative machine can push both operands as frames.
    LLVMValueRef pointerArithmeticFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef left,
        LLVMValueRef right,
        std::string& error);

    // NSC305-L04. The compound-store compute (load, operate, store), separated from the
    // address/operand evaluation so the iterative machine can push both as frames.
    LLVMValueRef compoundStoreFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        LLVMValueRef value,
        std::string& error);

    LLVMValueRef compoundUpdatedValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef previous,
        LLVMValueRef value,
        std::string& error);

    // NSC305-L04. The CC1 stackalloc compute (extent guards + byte alloca), separated from the
    // extent evaluation so the iterative machine can push each extent as a frame.
    LLVMValueRef stackAllocationFromParts(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>& rawExtents,
        std::string& error);

    // NSC305-L04. The store compute (bitfield/RAII/owned release + final store), separated from the
    // address/value evaluation so the iterative machine can push both as frames.
    LLVMValueRef storeFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        LLVMValueRef value,
        std::string& error);

    LLVMValueRef storeBitFieldFromParts(
        const frontend::ProjectOopMirNode* field,
        LLVMValueRef address,
        LLVMValueRef value,
        std::string& error);

    // NSC305-L04. The tuple-deconstruction compute (element loads + stores), separated from the
    // source/target evaluation so the iterative machine can push them as frames.
    LLVMValueRef tupleDeconstructFromParts(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef sourceValue,
        const std::vector<LLVMValueRef>& targetAddresses,
        std::string& error);

    LLVMValueRef lowerArrayAllocate(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef collectArrayAllocationCount(
        const frontend::ProjectOopMirNode* node,
        std::vector<LLVMValueRef>& segmentValues,
        std::vector<LLVMValueRef>& segmentLengths,
        std::string& error);

    bool initializeArrayElements(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef memory,
        std::uint64_t dataOffset,
        const std::vector<LLVMValueRef>& segmentValues,
        const std::vector<LLVMValueRef>& segmentLengths,
        std::string& error);

    // NativeSharp 3.0 T32 Windows-x64 ABI:
    // [total:i64][dimension0..R-1:i64][stride0..R-1:i64][aligned row-major data].
    // Rank is part of the canonical type/MIR, so it is intentionally not repeated at runtime.
    LLVMValueRef lowerRangeArrayAllocate(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    bool collectRangeArrayDimensions(
        const frontend::ProjectOopMirNode* node,
        std::vector<LLVMValueRef>& dimensions,
        std::string& error);

    LLVMValueRef computeRangeArrayElementCount(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>& dimensions,
        std::uint64_t dataOffset,
        std::string& error);

    void initializeRangeArrayHeader(
        LLVMValueRef memory,
        LLVMValueRef totalBytes,
        LLVMValueRef totalCount,
        const std::vector<LLVMValueRef>& dimensions);

    LLVMValueRef lowerRangeArrayElementAddress(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef emitRuntimeTypeTest(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        std::string& error);

    LLVMValueRef lowerRuntimeTypeTest(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerSafeReferenceCast(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerCheckedReferenceCast(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef normalizeShiftCount(
        LLVMValueRef count,
        const std::string& countTypeKey,
        const std::string& valueTypeKey,
        std::string& error);

    // IDs 79/81. La aritmetica de DOS VALORES YA BAJADOS, sacada de lowerBinary para que la
    // asignacion compuesta y `++`/`--` la reusen en vez de traer una segunda copia de las mismas
    // reglas. Las reglas que estan aqui y en ningun otro sitio: coma flotante contra entero,
    // division y resto con signo contra sin signo, y desplazamiento a la derecha aritmetico
    // contra logico. Duplicarlas es exactamente el defecto que el milestone CC1 existe para
    // quitar -- una regla del lenguaje definida por dos capas que pueden discrepar.
    LLVMValueRef applyBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error,
        bool divisorProvenNonZero);
    LLVMValueRef applyComparisonOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        const std::optional<language::PortableSimdType>& portableSimd,
        bool floating);
    LLVMValueRef tryApplyConstrainedFloatingOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        FloatingPointRoundingMode rounding);
    LLVMValueRef applyFloatingBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        std::string& error);
    LLVMValueRef applyDecimalAddOrSubtract(
        lexer::TokenKind operation,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef right,
        LLVMValueRef scaleShift,
        LLVMValueRef scaleMask,
        LLVMValueRef& effectiveScale);
    LLVMValueRef buildSaturatedDecimalMultiply(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef mask96,
        LLVMTypeRef wide);
    void mergeSaturatedDecimalMultiply(
        LLVMValueRef saturated,
        LLVMBasicBlockRef preheader,
        LLVMBasicBlockRef trapBlock,
        LLVMValueRef& combined,
        LLVMValueRef& effectiveScale,
        LLVMTypeRef wide);
    bool resolveDecimalMultiplyOverflow(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef& combined,
        LLVMValueRef& effectiveScale,
        std::string& error);
    LLVMValueRef applyDecimalMultiply(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef scaleShift,
        LLVMValueRef scaleMask,
        LLVMValueRef& effectiveScale,
        std::string& error);
    LLVMValueRef applyDecimalDivide(
        LLVMValueRef right,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef scaleShift,
        LLVMValueRef& effectiveScale,
        std::string& error);
    LLVMValueRef applyDecimalRemainder(
        LLVMValueRef right,
        LLVMValueRef leftCoefficient,
        LLVMValueRef rightCoefficient,
        LLVMValueRef leftScale,
        LLVMValueRef scaleShift,
        LLVMValueRef& effectiveScale);
    LLVMValueRef applyDecimalBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        std::string& error);
    LLVMValueRef applyIntegerBinaryOperation(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error,
        bool divisorProvenNonZero);

    // LA DIVISION ENTERA, CON SUS DOS CASOS DEGENERADOS. Sacada de applyBinaryOperation porque
    // ya no cabe en una linea, y porque los dos casos son decisiones del contrato (doc 110 §2.5)
    // y no detalles de emision:
    //
    //   * DIVISION POR CERO -> trampa explicita. Antes se dejaba al hardware, y eso NO cumple el
    //     contrato: en -O0 el `idiv` falla con 0xC0000094, pero en -O2 `sdiv x, 0` es POISON en
    //     el IR y el optimizador pliega la funcion entera a lo que le convenga. El contrato dice
    //     "trampa en -O0 y en -O2", asi que la trampa tiene que estar EN EL IR. Ademas ahora
    //     llega con su razon (LlvmTrapReason::DivisionByZero) en vez de como fallo de hardware.
    //
    //   * MinValue / -1 -> ENVUELVE, dividiendo por 1. La regla vive en
    //     src/LLVM/SignedDivisionCorrection.h, compartida con el bajador de fichero unico, y
    //     alli esta escrito el por que con la medicion de la fixture c30.
    //
    // Lo sin signo no necesita nada mas que la guarda del cero: `udiv`/`urem` no tienen caso
    // degenerado de desbordamiento.
    LLVMValueRef applyIntegerDivision(
        lexer::TokenKind operation,
        LLVMValueRef left,
        LLVMValueRef right,
        const std::string& operandType,
        std::string& error,
        bool divisorProvenNonZero);

    // T03-F03 value-producing match. The governing expression is evaluated once and spilled once;
    // MatchValue nodes use that frozen pair as a value or address. Arms remain lazy and ordered.
    // Bindings exist only after the structural predicate succeeds, and their cleanup executes on
    // both a failed guard and the selected-result edge before the phi join.
    LLVMValueRef lowerMatchExpression(
        const frontend::ProjectOopMirNode* node, std::string& error);
    LLVMValueRef lowerMatchGoverningValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef& governingAddress,
        std::string& error);
    bool lowerMatchArmDeclarations(
        const frontend::ProjectOopMirNode* arm,
        std::string& error);
    bool emitMatchArmCleanups(std::size_t cleanupDepth, std::string& error);
    bool lowerSelectedMatchArm(
        const frontend::ProjectOopMirNode* arm,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error);
    LLVMValueRef finishMatchResult(
        const frontend::ProjectOopMirNode* node,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error);
    LLVMValueRef lowerSwitchMatchExpression(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef governingAddress,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error);
    bool lowerGuardedMatchArm(
        const frontend::ProjectOopMirNode* arm,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef nextBlock,
        std::size_t& cleanupDepth,
        std::string& error);
    bool lowerPatternMatchArm(
        const frontend::ProjectOopMirNode* arm,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error);
    LLVMValueRef lowerPatternMatchExpression(
        const frontend::ProjectOopMirNode* node,
        const std::vector<std::size_t>& outerCleanups,
        LLVMBasicBlockRef endBlock,
        std::vector<LLVMValueRef>& incomingValues,
        std::vector<LLVMBasicBlockRef>& incomingBlocks,
        std::string& error);

    // `c ? a : b`. Both branches are LAZILY evaluated -- only the taken one runs -- so this is
    // control flow plus a phi, not an LLVM `select`, which would evaluate both. That matters for
    // correctness and not just for speed: `p != null ? p.Value : 0` must not read through a null
    // pointer to build the value it then discards. Same shape as the `&&`/`||` lowering below.
    //
    // The two branches were already given a single result type by the front end, so the incoming
    // values agree and the phi needs no conversion of its own.
    LLVMValueRef lowerConditional(const frontend::ProjectOopMirNode* node, std::string& error);

    // T01-F03 `left ?? right`. The left value is both the condition input and one incoming phi
    // value, so it is physically lowered once. The fallback has its own block and therefore keeps
    // calls, moves and traps lazy even at O0.
    LLVMValueRef lowerNullCoalescing(
        const frontend::ProjectOopMirNode* node, std::string& error);

    // E017 `Optional<T> ?? fallback`. The compiler consumes the carrier exclusively through its
    // public HasValue()/UncheckedValue() protocol. The carrier is spilled once so both instance
    // calls observe the same value; no private field offset or layout convention is baked here.
    LLVMValueRef lowerOptionalCoalescing(
        const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerOptionalPresentValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef carrierSlot,
        const FunctionInfo& uncheckedValueTarget,
        std::string& error);

    // NS304-T09-F05: a sequenced expression (Block that yields a value). Every child except the
    // last is a statement (LocalDeclaration/ExpressionStatement); the last child is the result
    // expression lowered by the expression path.
    LLVMValueRef lowerBlockExpression(
        const frontend::ProjectOopMirNode* node, std::string& error);

    // T07-F01 `carrier?`. The carrier is evaluated and spilled exactly once. The public variant
    // protocol performs the tag test and payload extraction, so this lowering never reaches into
    // private variant storage. The Error edge builds the callable's return carrier and then uses
    // the ordinary structured-return helper, including lexical cleanup and catch-state handling.
    LLVMValueRef lowerResultPropagation(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef carrierOverride = nullptr);

    LLVMValueRef lowerResultPropagationInstanceCall(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef carrierSlot,
        std::string_view callee,
        std::string_view resultType,
        std::string_view stem,
        std::string& error);

    LLVMValueRef lowerResultPropagationStaticCall(
        std::string_view callee,
        std::string_view argumentType,
        LLVMValueRef argument,
        std::string_view resultType,
        std::string_view stem,
        std::string& error);

    // T01-F03 `place ??= fallback`. Address, current value and fallback are each evaluated once;
    // the store exists only on the null edge. A null slot owns nothing, so no replacement cleanup
    // is necessary here (unlike an ordinary owning store of a non-null displaced value).
    LLVMValueRef lowerNullCoalescingStore(
        const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerNullConditionalField(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride = nullptr);

    bool prepareNullConditionalFieldStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error,
        LLVMValueRef& address,
        LLVMValueRef& current,
        LLVMTypeRef& resultType,
        LLVMBasicBlockRef& nullBlock,
        LLVMBasicBlockRef& endBlock);

    LLVMValueRef concatenateNullConditionalFieldValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef current,
        LLVMValueRef value,
        std::string& error);

    LLVMValueRef computeNullConditionalFieldValue(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef current,
        LLVMValueRef value,
        std::string& error);

    LLVMValueRef finishNullConditionalFieldStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef address,
        LLVMValueRef current,
        LLVMValueRef value,
        LLVMTypeRef resultType,
        LLVMBasicBlockRef nullBlock,
        LLVMBasicBlockRef endBlock,
        std::string& error);

    bool prepareNullConditionalArrayLoad(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error,
        LLVMTypeRef& resultType,
        LLVMBasicBlockRef& nullBlock,
        LLVMBasicBlockRef& endBlock);

    LLVMValueRef nullConditionalArrayLoadAddress(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        std::string& error);

    LLVMValueRef finishNullConditionalArrayLoad(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        LLVMTypeRef resultType,
        LLVMBasicBlockRef nullBlock,
        LLVMBasicBlockRef endBlock,
        std::string& error);

    bool prepareNullConditionalArrayStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error,
        LLVMTypeRef& resultType,
        LLVMBasicBlockRef& nullBlock,
        LLVMBasicBlockRef& endBlock);

    LLVMValueRef nullConditionalArrayStoreAddress(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        std::string& error);

    LLVMValueRef finishNullConditionalArrayStore(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        LLVMValueRef index,
        LLVMValueRef value,
        LLVMTypeRef resultType,
        LLVMBasicBlockRef nullBlock,
        LLVMBasicBlockRef endBlock,
        std::string& error);

    bool callArgumentOrder(
        const frontend::ProjectOopMirNode* node,
        std::size_t sourceCount,
        std::vector<std::size_t>& parameterToSource,
        std::string& error);

    bool applyMustTail(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef call,
        std::string& error);

    // T01-F03. `?.` shares the ordinary direct/virtual/interface call lowerers.  Only the
    // scheduling differs: evaluate the receiver, branch on null, and lower every argument plus
    // the actual dispatch inside the non-null block.  This makes effectful arguments lazy and
    // keeps the receiver single-evaluation without a second call ABI implementation.
    LLVMValueRef lowerNullConditionalCall(
        const frontend::ProjectOopMirNode* node, std::string& error);

    // NativeSharp 3.0 T01-F04. The receiver is lowered once, then reused for both structural
    // protocol calls. Endpoints are half-open: omitted start/end mean 0/Length, and `^n` means
    // Length-n. The compiler validates 0 <= start <= end <= Length before invoking Slice, so a
    // user type cannot silently reinterpret malformed range syntax.
    LLVMValueRef lowerRangeSlice(
        const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride = nullptr,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    bool collectCallArguments(
        const frontend::ProjectOopMirNode* node,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& sourceArguments,
        LLVMValueRef& assignedValue,
        LLVMValueRef& terminatedValue,
        std::string& error);

    bool ensureTypeInitialized(std::size_t typeOrdinal, std::string& error);

    LLVMValueRef staticFieldAddress(const frontend::ProjectOopMirNode* node, std::string& error);

    LLVMValueRef lowerDirectClosureCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerStaticCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    LLVMValueRef loadVirtualTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error);

    LLVMValueRef lowerVirtualCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride = nullptr,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    bool prepareDynamicCallArguments(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& arguments,
        std::vector<LLVMTypeRef>& parameterTypes,
        LLVMValueRef& sretSlot,
        LLVMValueRef& assignedValue,
        std::string& error);

    LLVMValueRef loadInterfaceTarget(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef receiver,
        std::string& error);

    void emitInterfaceDispatchTrap();

    LLVMValueRef loadInterfaceMethodSlot(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef entry,
        LLVMTypeRef ptr,
        LLVMTypeRef i64);

    LLVMValueRef lowerInterfaceCall(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef receiverOverride = nullptr,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    // C6 "struct members": `new Vec2(1, 2)` on a struct.
    //
    // Everything lowerConstruction below does that is about being on the HEAP is absent here, and
    // that absence is the point: no allocator call, no null-check branch, no class descriptor
    // stored at offset 0, nothing to free. What remains is what construction actually means --
    // storage, zeroed, with the constructor run on it:
    //
    //     %slot = alloca <carrier>            (hoisted to the entry block, like every other
    //                                          temporary, so a loop does not grow the stack)
    //     memset(%slot, 0, size)              every field starts at its zero value
    //     call void @ctor(ptr %slot, args)    `this` is the slot
    //
    // The VALUE of the expression then follows the same rule every other struct-typed expression
    // in this backend follows, which is what lets `new Vec2(1,2)` appear anywhere a Vec2 may:
    // an aggregate of 1/2/4/8 bytes travels directly, so the value is a load of that width; any
    // other size is indirect, and the value IS the address of the storage.
    LLVMValueRef lowerValueConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    LLVMValueRef lowerParameterPack(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    bool initializeAggregateIntoSlot(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef slot,
        std::string& error,
        const std::vector<LLVMValueRef>* fieldValues = nullptr);

    bool initializeAggregateSpread(
        const frontend::ProjectOopMirNode* child,
        LLVMValueRef slot,
        const std::vector<LLVMValueRef>* fieldValues,
        std::size_t& fieldIndex,
        std::string& error);

    bool initializeAggregateField(
        const frontend::ProjectOopMirNode* child,
        LLVMValueRef slot,
        const std::vector<LLVMValueRef>* fieldValues,
        std::size_t& fieldIndex,
        std::string& error);

    // C7/T01-F05 aggregate expression. Locals bypass this temporary and call
    // initializeAggregateIntoSlot with their final storage from lowerStatement. A temporary is
    // still required when the aggregate is itself an argument, return value, or subexpression.
    LLVMValueRef lowerAggregateInitializer(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* fieldValues = nullptr);

    LLVMValueRef lowerConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    bool collectConstructionArguments(
        const frontend::ProjectOopMirNode* node,
        std::size_t childOffset,
        const std::vector<LLVMValueRef>* argumentOverrides,
        std::vector<LLVMValueRef>& orderedArguments,
        std::string& error);

    LLVMValueRef allocateConstructionStorage(
        const frontend::ProjectOopMirNode* node,
        LLVMBasicBlockRef& endBlock,
        std::string& error);

    bool invokeConstructionInitializer(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef object,
        const std::vector<LLVMValueRef>& orderedArguments,
        std::string& error);

    LLVMValueRef lowerPlacementConstruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef storageOverride = nullptr,
        const std::vector<LLVMValueRef>* argumentOverrides = nullptr);

    bool initializePlacementStorage(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        const frontend::NamedTypeDeclaration& targetType,
        std::string& error);

    bool invokePlacementConstructor(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        const frontend::NamedTypeDeclaration& targetType,
        const std::vector<LLVMValueRef>& orderedArguments,
        std::string& error);

    bool runPlacementInitializers(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error);

    LLVMValueRef lowerObjectDestruction(
        const frontend::ProjectOopMirNode* node,
        std::string& error);

    LLVMValueRef lowerDeallocation(
        const frontend::ProjectOopMirNode* node,
        std::string& error,
        LLVMValueRef loweredOperandOverride = nullptr);

    LLVMTypeRef bitFieldStorageType(
        const frontend::ProjectOopMirNode* node,
        std::string& error) const;

    LLVMValueRef extractBitField(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error);

    LLVMValueRef insertBitField(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef previousStorage,
        LLVMValueRef value,
        std::string& error);

    // NSC305-L04. The field-address compute (the null check + the GEP), separated from the receiver
    // evaluation so the iterative machine can push the receiver as a frame.
    LLVMValueRef fieldAddressFromReceiver(
        const frontend::ProjectOopMirNode* node, LLVMValueRef receiver, std::string& error);

    LLVMValueRef checkedReceiver(LLVMValueRef receiver, std::string& error);

    LLVMValueRef convert(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        std::string& error);
    LLVMValueRef convertStructuralTuple(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        LLVMTypeRef destination,
        std::string& error);
    std::string_view terminalTypeKey(std::string_view key) const;
    LLVMValueRef tryConvertSpanView(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        LLVMTypeRef destination,
        bool& handled,
        std::string& error);
    LLVMValueRef convertDecimalToInteger(
        LLVMValueRef value,
        LLVMTypeRef destination);
    LLVMValueRef convertIntegerToDecimal(
        LLVMValueRef value,
        bool sourceSigned);
    LLVMValueRef convertDecimalToFloating(
        LLVMValueRef value,
        LLVMTypeRef destination);
    LLVMValueRef convertFloatingToDecimal(
        LLVMValueRef value,
        std::string_view source,
        std::string& error);
    LLVMValueRef convertScalar(
        LLVMValueRef value,
        std::string_view source,
        std::string_view target,
        LLVMTypeRef destination,
        std::string& error);

    // EL TIPO ENTERO SUBYACENTE DE UN ENUM, buscado por su clave de tipo. Nulo si la clave no
    // nombra un enum -- ni una struct ni una referencia de clase, que comparten el prefijo
    // "type:".
    //
    // ES EL UNICO SITIO DE ESTE BACKEND que traduce una clave de tipo a la regla de
    // include/nativesharp/Frontend/EnumUnderlyingType.h. El tipo LLVM, el ancho de las
    // conversiones, el signo de las extensiones y el signo de las constantes de `case` salen los
    // cuatro de aqui, y por eso no pueden discrepar: hasta el 2026-09-03 eran cuatro copias de
    // "un enum mide 32 bits y tiene signo" escritas por separado, y un `enum : byte` las
    // contradecia a las cuatro a la vez.
    // (NSC305-L03: los lookups viven en LlvmTypeLayout.h; `findEnumUnderlyingInfo`/`isEnumTypeKey`.)

    LLVMTypeRef llvmType(std::string_view typeKey, std::string& error) const;

    // Creates the private descriptor shape used only by custom-allocator objects. Its first field
    // is a byte-for-byte public class descriptor, so every existing consumer can dispatch through
    // object[0] without knowing that context and thunk live immediately after that descriptor.
    bool ensureAllocatorDescriptorSidecar(std::string& error);

    // Installs custom allocator provenance without changing the object's physical layout. The
    // sidecar allocation is compiler-owned; if it fails after the user's Allocate succeeded, the
    // original block is returned through the user's thunk before reporting AllocationFailure.
    bool installAllocatorDescriptorSidecar(
        LLVMValueRef object,
        LLVMValueRef descriptor,
        LLVMValueRef context,
        LLVMValueRef thunk,
        std::string& error);

    // NS304-T13-F07: a custom allocator owns the raw block as soon as Allocate succeeds. If the
    // constructor unwinds, its own cleanup has already destroyed every initialized subobject; this
    // caller-side cleanuppad must therefore release only the block and its descriptor sidecar,
    // then continue the original unwind without running the object destructor.
    LLVMBasicBlockRef emitAllocatorConstructionFailureCleanup(
        LLVMValueRef object,
        LLVMBasicBlockRef outerUnwind,
        std::string& error);

    // NS304-T13-F07: generate (and cache) the per-allocator-type deallocate thunk
    // void(void* context, void* ptr) { <T>::Deallocate(context, ptr); }. Internal linkage, so each
    // module carries its own copy; the function pointer stored in the descriptor sidecar stays
    // valid in the final binary.
    LLVMValueRef allocatorDeallocateThunk(
        std::size_t allocatorTypeOrdinal,
        std::string_view deallocateCalleeName,
        std::string& error);

    bool isStructValue(std::string_view typeKey) const noexcept;

    // Stage 52 closure hardening (closure checklist P1 item 6: "requiresOwnedDestruction(typeKey)
    // ... reduce la posibilidad de que futuros stages repitan decisiones divergentes"). This is
    // the single, centralized answer to "does a closed NativeSharp type actually own separate
    // heap-allocated storage that a cleanup step must release" -- the exact classification that
    // emitOwnedParameterDestroy, emitRaiiDestroy, and lowerDeallocation each need at their own
    // "am I looking at a real owning pointer, or just a scalar bit pattern that happens to sit in
    // an `owned`-qualified slot" decision point.
    //
    // Before this helper existed, each of those three call sites spelled out its own copy of
    // this same reference-like/string/value-type split independently -- three chances to
    // diverge, which is exactly how "Bug A" (the Stage 50.8 finding: emitOwnedParameterDestroy
    // treated an `owned int` parameter's 4-byte alloca as an 8-byte pointer, corrupting adjacent
    // stack memory) went unnoticed for a full closure cycle: emitRaiiDestroy and
    // lowerDeallocation had each independently grown their own value-type handling by the time
    // emitOwnedParameterDestroy needed one too, and nobody had a single place to add it.
    //
    // A closed type requires real destruction when it is:
    //   - a class or interface reference (isClassReference) -- released through the class
    //     descriptor's virtual destruction entry (descriptorType_/destructionEntryFunctionType_);
    //   - `builtin:string` -- released through the native string deallocation path
    //     (deallocationFunction_/kDeallocationFunctionName);
    // and requires NO destruction (a harmless no-op cleanup) for every other closed type -- a
    // numeric/bool scalar, an enum (represented as its underlying scalar), or a struct value --
    // none of which separately own heap memory of their own, regardless of which generic T they
    // came from monomorphizing. This mirrors (without yet sharing translation-unit-level code
    // with) the equivalent HIR-layer classification `isReferenceType`/`nativeString` in
    // ProjectOopHirModel.cpp computes for `delete` (see its `node->valueTypeNoOpDeletion` flag)
    // -- unifying the HIR and LLVM layers into one shared classification function is a further,
    // still-open deduplication opportunity, out of scope for this pass, which only centralizes
    // the three call sites that already live in this file.
    bool requiresOwnedDestruction(std::string_view typeKey) const noexcept;

    bool isFloat32x4TypeKey(std::string_view typeKey) const noexcept;

    unsigned compactFloatVectorLaneCount(std::string_view typeKey) const noexcept;

    unsigned integerSimd128LaneBits(std::string_view typeKey) const noexcept;

    unsigned floatingSimd256LaneBits(std::string_view typeKey) const noexcept;

    unsigned integerSimd256LaneBits(std::string_view typeKey) const noexcept;

    // Microsoft x64 vectorcall recognizes an HVA only when the complete value is one to four
    // consecutive, identical vector members.  NativeSharp's first calculation vector is
    // Float32x4, so this deliberately does not promote an arbitrary 16-byte storage struct merely
    // because it happens to contain four floats.  Union/explicit/packed layouts are also excluded:
    // their byte compatibility does not make them homogeneous vector aggregates.
    unsigned vectorCallHvaArity(std::string_view typeKey) const noexcept;

    LLVMTypeRef vectorCallAbiType(std::string_view typeKey) const noexcept;

    LLVMValueRef carrierToVectorCallAbi(
        LLVMValueRef value, std::string_view typeKey, std::string& error);

    LLVMValueRef vectorCallAbiToCarrier(
        LLVMValueRef value, std::string_view typeKey, std::string& error);

    template <typename AddAttribute>
    void applyInRegAttribute(AddAttribute&& addAttribute, unsigned parameterPosition);

    // Win64 aggregate ABI (ground truth: clang for x86_64-pc-windows-msvc). True for a struct
    // whose size is not 1/2/4/8 -- the sizes that travel directly as integers. Everything else
    // (3 bytes included) is passed as a pointer to a caller-owned per-call copy and returned
    // through a leading sret pointer.
    bool isIndirectStructTypeKey(std::string_view typeKey) const noexcept;

    unsigned structAlignmentFor(std::string_view typeKey) const noexcept;

    // Allocates a stack slot in the current function's ENTRY block regardless of where the
    // builder currently sits, and stamps the struct layout's real alignment on it. Used for the
    // caller-side per-call argument copies and sret result slots the indirect ABI needs: emitted
    // at the call site itself an alloca would re-allocate on every loop iteration and grow the
    // stack without bound, which is why clang hoists the same temporaries into the entry block.
    LLVMValueRef entryBlockAlloca(LLVMTypeRef type, unsigned alignment, const char* name);

    // The parameter type a signature actually carries for `typeKey`: indirect aggregates travel
    // as an untyped pointer (clang emits a PLAIN `ptr` here -- no byval on Win64), everything
    // else as its direct carrier.
    LLVMTypeRef abiParameterType(std::string_view typeKey, std::string& error) const;

    // Wraps one already-lowered argument VALUE for the call boundary: an indirect aggregate is
    // spilled into a fresh entry-block copy owned by THIS call (Win64 contract: the callee may
    // scribble on it; re-passing the same struct farther down makes another copy -- measured on
    // clang's own output), and the pointer is what travels. Direct values pass through untouched.
    LLVMValueRef wrapIndirectArgument(LLVMValueRef value, std::string_view typeKey, std::string& error);

    // Attaches `sret(<carrier>) noalias align <n>` to parameter `parameterPosition` (0-based) of
    // a function or call -- the exact attribute set clang stamps on the hidden return slot.
    template <typename AddAttribute>
    void applySretAttributes(
        AddAttribute&& addAttribute, unsigned parameterPosition, std::string_view typeKey);

    // The one way a value leaves a function: through RAX for direct types, through the sret
    // slot for indirect aggregates. Both return sites (the plain exit and the EH spill-slot
    // path) go through here so the two can never disagree about the ABI.
    void emitAbiReturn(LLVMValueRef value);

    LLVMBasicBlockRef appendBlock(const std::string& stem);

    // NS304-T15-F04: 10^exponent as a runtime i64 loop (exponent 0..28).
    // NS304-T15-F04: the decimal scale must stay 0..28. `mul` adds scales and `div` can bump the
    // scale by one guard, so either can exceed 28; that is a decimal overflow, not silent wrap.
    bool emitDecimalScaleOverflow(LLVMValueRef effectiveScale, const char* stem, std::string& error);

    LLVMValueRef buildPow10(LLVMValueRef exponent, const char* stem);

    // Every real toolchain (clang included) marks EVERY defined function `uwtable` on a Windows
    // x64 target, unconditionally -- not just the ones that use exceptions. Before this call
    // existed, nothing in this file ever set a single LLVM function attribute, so none of our
    // emitted functions carried it. Setting it is correct, matches what every other Windows x64
    // toolchain does, and is worth keeping on those grounds alone.
    //
    // ACCURACY NOTE (external audit item "comentarios contradictorios sobre la causa raíz",
    // 2026-08-29): this comment used to assert that a missing `uwtable` was what corrupted
    // BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md's binary at runtime -- i.e. that
    // this call was the fix for bug B-001. That claim was never checked by running anything, and it
    // is FALSE: a controlled Wine re-run proved the attribute was already present in the built
    // binary (`attributes #N = { uwtable(sync) }` on the very function that crashed) while the
    // access violation still reproduced identically. B-001's actual fix is the per-function
    // mem2reg legalization in src/LLVM/LlvmOptimizer.cpp. See
    // claude/30_bloque0_correctness_gate_b001_b003.md for the full bisection. Treat this call as
    // correct Windows x64 hygiene, NOT as the fix for any known bug.
    // Adds a simple enum function attribute by name. Null-tolerant; a name LLVM does not know is
    // ignored rather than fatal, so a future LLVM renaming degrades to "attribute not set".
    void applyFunctionAttribute(LLVMValueRef function, const char* attributeName);

    // ID 130, ARREGLADO EL 2026-09-03. `uwtable` NO es un atributo booleano: su valor es un
    // UWTableKind, y el 0 que habia aqui significa None -- es decir, "esta funcion NO tiene tabla
    // de desenrollado", que es exactamente lo contrario de lo que este metodo dice hacer.
    //
    // MEDIDO contra libLLVM-18 con la propia src/LLVM/LlvmCApi.h de este proyecto y SIN el shim
    // del contenedor (que reescribia 0 -> 2 y por tanto tapaba el sintoma). kindId de `uwtable`
    // es 87, y lo que imprime cada valor:
    //
    //     valor 0  ->  NADA. Ni linea `attributes #0`, ni `#0` en el `define`. El modulo impreso
    //                  sale degradado: LLVMPrintModuleToString devuelve un texto en el que no
    //                  aparece ni la definicion de la funcion.
    //     valor 1  ->  attributes #0 = { uwtable(sync) }
    //     valor 2  ->  attributes #0 = { uwtable }          <- Async, la forma por omision
    //     valor 3  ->  attributes #0 = { uwtable(async) }
    //
    // SE ELIGE 2, que es Async y se imprime como `uwtable` a secas. Es lo que clang-cl emite en
    // Windows x64, donde las tablas de desenrollado asincronas son el comportamiento por omision.
    //
    // UNA ADVERTENCIA SOBRE LA MEDICION, porque importa: el contenedor usa LLVM 18 y el `nsc` de
    // Windows enlaza LLVM-C.lib de C:\clang-llvm, que es LLVM 23. La correspondencia
    // valor->UWTableKind es del formato del IR y no cambia entre versiones, pero como cada una
    // trata el 0 a su manera, el efecto OBSERVABLE puede diferir -- y eso explica que en el
    // contenedor el modulo saliera roto y en Windows el binario construido pareciera correcto.
    // Por eso el arreglo se hace igual y por eso la prueba tiene que mirar el IR EN WINDOWS
    // ademas de ejecutar EH.
    //
    // Y NO se toca applyFunctionAttribute: ese solo se usa para `nounwind`, `noreturn` y
    // `nounwind` de los thunks de carga, que son atributos SIN valor, donde el 0 es lo correcto.
    void applyUwtableAttribute(LLVMValueRef function);

    void applyDenormalFloatingPointAttribute(LLVMValueRef function);

    bool terminated() const noexcept;

    static bool fail(std::string& error, std::string message);

    static LLVMValueRef failValue(std::string& error, std::string message);

    const frontend::ProjectNameLookup& nameLookup_;
    const frontend::ProjectMemberModel* memberModel_ = nullptr;
    const frontend::ProjectOopMirModel& mirModel_;
    const source::SourceManager* sourceManager_ = nullptr;
    DebugInformationKind debugInformation_ = DebugInformationKind::None;
    LlvmOptimizationLevel optimizationLevel_ = LlvmOptimizationLevel::O0;
    std::vector<DebugPrefixMap> debugPrefixMaps_;
    bool partitioned_ = false;
    bool ownsModuleGlobals_ = true;
    std::vector<std::uint8_t> ownedReachableDefinitions_;
    std::span<const std::uint8_t> reachableDefinitions_;
    std::vector<bool> functionDefinitions_;
    std::vector<bool> functionDeclarations_;
    bool restrictFunctionDeclarations_ = false;
    std::vector<std::uint8_t> ownedStaticFieldDeclarations_;
    std::span<const std::uint8_t> staticFieldDeclarations_;
    inline static std::once_flag loweringTraceReset_;
    inline static std::mutex loweringTraceMutex_;
    std::string moduleName_;
    std::optional<std::size_t> invalidFunctionOrdinal_;
    LLVMContextRef context_ = nullptr;
    LLVMModuleRef module_ = nullptr;
    LLVMBuilderRef builder_ = nullptr;
    LLVMDIBuilderRef debugBuilder_ = nullptr;
    LLVMMetadataRef debugCompileUnit_ = nullptr;
    std::unordered_map<std::uint32_t, LLVMMetadataRef> debugFiles_;
    std::unordered_map<std::uint32_t, unsigned> debugCodeViewFileNumbers_;
    std::unordered_map<std::string, LLVMMetadataRef> debugTypes_;
    std::unordered_map<std::string, LLVMMetadataRef> debugPointeeTypes_;
    std::unordered_set<std::size_t> debugDeclaredLocals_;
    std::unordered_map<std::size_t, LLVMMetadataRef> debugCaptureVariables_;
    LLVMValueRef trapFunction_ = nullptr;
    LLVMTypeRef trapFunctionType_ = nullptr;
    LLVMValueRef rttiClassTestFunction_ = nullptr;
    LLVMValueRef rttiInterfaceTestFunction_ = nullptr;
    LLVMTypeRef rttiTestFunctionType_ = nullptr;
    LLVMValueRef allocationFunction_ = nullptr;
    LLVMTypeRef allocationFunctionType_ = nullptr;
    LLVMValueRef deallocationFunction_ = nullptr;
    LLVMTypeRef deallocationFunctionType_ = nullptr;
    // INSTRUMENTATION CODE: generated only for DebugInformationKind::Full modules.
    LLVMValueRef runtimeDebugTraceFunction_ = nullptr;
    LLVMTypeRef runtimeDebugTraceFunctionType_ = nullptr;
    LLVMValueRef runtimeDebugTraceFlags_ = nullptr;
    // NS304-T13-F07: allocator provenance stays outside the public object layout. The sidecar
    // begins with a complete public descriptor and appends context + deallocate thunk.
    LLVMTypeRef allocatorFreeThunkType_ = nullptr;
    LLVMTypeRef allocatorDescriptorSidecarType_ = nullptr;
    LLVMValueRef allocatorDescriptorSidecarReleaseFunction_ = nullptr;
    LLVMValueRef consoleWriteLineFunction_ = nullptr;
    LLVMTypeRef consoleWriteLineFunctionType_ = nullptr;
    LLVMValueRef consoleReadLineFunction_ = nullptr;
    LLVMTypeRef consoleReadLineFunctionType_ = nullptr;
    LLVMValueRef clockQueryCounterFunction_ = nullptr;
    LLVMTypeRef clockQueryCounterFunctionType_ = nullptr;
    LLVMValueRef clockQueryFrequencyFunction_ = nullptr;
    LLVMTypeRef clockQueryFrequencyFunctionType_ = nullptr;
    LLVMValueRef clockQuerySystemFileTimeFunction_ = nullptr;
    LLVMTypeRef clockQuerySystemFileTimeFunctionType_ = nullptr;
    LLVMValueRef environmentTickCount64Function_ = nullptr;
    LLVMTypeRef environmentTickCount64FunctionType_ = nullptr;
    LLVMValueRef environmentProcessorCountFunction_ = nullptr;
    LLVMTypeRef environmentProcessorCountFunctionType_ = nullptr;
    LLVMValueRef environmentExitFunction_ = nullptr;
    LLVMTypeRef environmentExitFunctionType_ = nullptr;
    LLVMValueRef environmentGetVariableFunction_ = nullptr;
    LLVMTypeRef environmentGetVariableFunctionType_ = nullptr;
    LLVMValueRef environmentSetVariableFunction_ = nullptr;
    LLVMTypeRef environmentSetVariableFunctionType_ = nullptr;
    LLVMValueRef fileExistsFunction_ = nullptr;
    LLVMTypeRef fileExistsFunctionType_ = nullptr;
    LLVMValueRef fileDeleteFunction_ = nullptr;
    LLVMTypeRef fileDeleteFunctionType_ = nullptr;
    LLVMValueRef directoryExistsFunction_ = nullptr;
    LLVMTypeRef directoryExistsFunctionType_ = nullptr;
    LLVMValueRef directoryCreateFunction_ = nullptr;
    LLVMTypeRef directoryCreateFunctionType_ = nullptr;
    LLVMValueRef fileReadAllTextFunction_ = nullptr;
    LLVMTypeRef fileGetSizeFunctionType_ = nullptr;
    LLVMValueRef fileGetSizeFunction_ = nullptr;
    LLVMTypeRef fileReadAllBytesFunctionType_ = nullptr;
    LLVMValueRef fileReadAllBytesFunction_ = nullptr;
    LLVMTypeRef fileReadBytesFunctionType_ = nullptr;
    LLVMValueRef fileReadBytesFunction_ = nullptr;
    LLVMTypeRef fileWriteAllBytesFunctionType_ = nullptr;
    LLVMValueRef fileWriteAllBytesFunction_ = nullptr;
    LLVMTypeRef environmentCurrentDirectoryFunctionType_ = nullptr;
    LLVMValueRef environmentCurrentDirectoryFunction_ = nullptr;
    LLVMTypeRef processRunFunctionType_ = nullptr;
    LLVMValueRef processRunFunction_ = nullptr;
    LLVMTypeRef fileReadAllTextFunctionType_ = nullptr;
    LLVMValueRef fileWriteAllTextFunction_ = nullptr;
    LLVMTypeRef fileWriteAllTextFunctionType_ = nullptr;
    LLVMValueRef fileAppendAllTextFunction_ = nullptr;
    LLVMTypeRef fileAppendAllTextFunctionType_ = nullptr;
    LLVMValueRef directoryDeleteFunction_ = nullptr;
    LLVMTypeRef directoryDeleteFunctionType_ = nullptr;
    LLVMValueRef pathGetFullPathFunction_ = nullptr;
    LLVMTypeRef pathGetFullPathFunctionType_ = nullptr;
    LLVMValueRef environmentGetCommandLineArgCountFunction_ = nullptr;
    LLVMTypeRef environmentGetCommandLineArgCountFunctionType_ = nullptr;
    LLVMValueRef environmentGetCommandLineArgsFunction_ = nullptr;
    LLVMTypeRef environmentGetCommandLineArgsFunctionType_ = nullptr;
    LLVMValueRef directoryGetFileCountFunction_ = nullptr;
    LLVMTypeRef directoryGetFileCountFunctionType_ = nullptr;
    LLVMValueRef directoryGetFilesFunction_ = nullptr;
    LLVMTypeRef directoryGetFilesFunctionType_ = nullptr;
    LLVMValueRef directoryGetDirectoryCountFunction_ = nullptr;
    LLVMTypeRef directoryGetDirectoryCountFunctionType_ = nullptr;
    LLVMValueRef directoryGetDirectoriesFunction_ = nullptr;
    LLVMTypeRef directoryGetDirectoriesFunctionType_ = nullptr;
    LLVMValueRef threadCreateFunction_ = nullptr;
    LLVMTypeRef threadCreateFunctionType_ = nullptr;
    LLVMValueRef threadJoinFunction_ = nullptr;
    LLVMTypeRef threadJoinFunctionType_ = nullptr;
    LLVMValueRef threadDetachFunction_ = nullptr;
    LLVMTypeRef threadDetachFunctionType_ = nullptr;
    LLVMValueRef atomicLoadInt32Function_ = nullptr;
    LLVMTypeRef atomicLoadInt32FunctionType_ = nullptr;
    LLVMValueRef atomicStoreInt32Function_ = nullptr;
    LLVMTypeRef atomicStoreInt32FunctionType_ = nullptr;
    LLVMValueRef atomicLoadInt64Function_ = nullptr;
    LLVMTypeRef atomicLoadInt64FunctionType_ = nullptr;
    LLVMValueRef atomicStoreInt64Function_ = nullptr;
    LLVMTypeRef atomicStoreInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedIncrementInt32Function_ = nullptr;
    LLVMTypeRef interlockedIncrementInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedDecrementInt32Function_ = nullptr;
    LLVMTypeRef interlockedDecrementInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedAddInt32Function_ = nullptr;
    LLVMTypeRef interlockedAddInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedExchangeInt32Function_ = nullptr;
    LLVMTypeRef interlockedExchangeInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedCompareExchangeInt32Function_ = nullptr;
    LLVMTypeRef interlockedCompareExchangeInt32FunctionType_ = nullptr;
    LLVMValueRef interlockedIncrementInt64Function_ = nullptr;
    LLVMTypeRef interlockedIncrementInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedDecrementInt64Function_ = nullptr;
    LLVMTypeRef interlockedDecrementInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedAddInt64Function_ = nullptr;
    LLVMTypeRef interlockedAddInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedExchangeInt64Function_ = nullptr;
    LLVMTypeRef interlockedExchangeInt64FunctionType_ = nullptr;
    LLVMValueRef interlockedCompareExchangeInt64Function_ = nullptr;
    LLVMTypeRef interlockedCompareExchangeInt64FunctionType_ = nullptr;
    LLVMValueRef mutexCreateFunction_ = nullptr;
    LLVMTypeRef mutexCreateFunctionType_ = nullptr;
    LLVMValueRef mutexCreateNamedFunction_ = nullptr;
    LLVMTypeRef mutexCreateNamedFunctionType_ = nullptr;
    LLVMValueRef mutexLockFunction_ = nullptr;
    LLVMTypeRef mutexLockFunctionType_ = nullptr;
    LLVMValueRef mutexUnlockFunction_ = nullptr;
    LLVMTypeRef mutexUnlockFunctionType_ = nullptr;
    LLVMValueRef mutexCloseFunction_ = nullptr;
    LLVMTypeRef mutexCloseFunctionType_ = nullptr;
    LLVMValueRef getCurrentThreadIdFunction_ = nullptr;
    LLVMTypeRef getCurrentThreadIdFunctionType_ = nullptr;
    LLVMValueRef monitorEnterFunction_ = nullptr;
    LLVMTypeRef monitorEnterFunctionType_ = nullptr;
    LLVMValueRef monitorExitFunction_ = nullptr;
    LLVMTypeRef monitorExitFunctionType_ = nullptr;
    LLVMValueRef monitorPulseFunction_ = nullptr;
    LLVMTypeRef monitorPulseFunctionType_ = nullptr;
    LLVMValueRef monitorPulseAllFunction_ = nullptr;
    LLVMTypeRef monitorPulseAllFunctionType_ = nullptr;
    LLVMValueRef monitorWaitFunction_ = nullptr;
    LLVMTypeRef monitorWaitFunctionType_ = nullptr;
    LLVMValueRef semaphoreCreateFunction_ = nullptr;
    LLVMTypeRef semaphoreCreateFunctionType_ = nullptr;
    LLVMValueRef semaphoreCreateNamedFunction_ = nullptr;
    LLVMTypeRef semaphoreCreateNamedFunctionType_ = nullptr;
    LLVMValueRef waitForHandleTimeoutFunction_ = nullptr;
    LLVMTypeRef waitForHandleTimeoutFunctionType_ = nullptr;
    LLVMValueRef semaphoreReleaseFunction_ = nullptr;
    LLVMTypeRef semaphoreReleaseFunctionType_ = nullptr;
    LLVMValueRef semaphoreCloseFunction_ = nullptr;
    LLVMTypeRef semaphoreCloseFunctionType_ = nullptr;
    LLVMValueRef eventCreateFunction_ = nullptr;
    LLVMTypeRef eventCreateFunctionType_ = nullptr;
    LLVMValueRef eventCreateNamedFunction_ = nullptr;
    LLVMTypeRef eventCreateNamedFunctionType_ = nullptr;
    LLVMValueRef eventSetFunction_ = nullptr;
    LLVMTypeRef eventSetFunctionType_ = nullptr;
    LLVMValueRef eventResetFunction_ = nullptr;
    LLVMTypeRef eventResetFunctionType_ = nullptr;
    LLVMValueRef eventCloseFunction_ = nullptr;
    LLVMTypeRef eventCloseFunctionType_ = nullptr;
    // Stage 55.1 "Process".
    LLVMValueRef processGetCurrentProcessIdFunction_ = nullptr;
    LLVMTypeRef processGetCurrentProcessIdFunctionType_ = nullptr;
    LLVMValueRef processGetWorkingSetSizeFunction_ = nullptr;
    LLVMTypeRef processGetWorkingSetSizeFunctionType_ = nullptr;
    LLVMValueRef processGetKernelTimeFunction_ = nullptr;
    LLVMTypeRef processGetKernelTimeFunctionType_ = nullptr;
    LLVMValueRef processGetUserTimeFunction_ = nullptr;
    LLVMTypeRef processGetUserTimeFunctionType_ = nullptr;
    LLVMValueRef processGetPriorityClassFunction_ = nullptr;
    LLVMTypeRef processGetPriorityClassFunctionType_ = nullptr;
    LLVMValueRef processSetPriorityClassFunction_ = nullptr;
    LLVMTypeRef processSetPriorityClassFunctionType_ = nullptr;
    // Stage 55.2 "Hardware / System Info".
    LLVMValueRef systemInfoGetPageSizeFunction_ = nullptr;
    LLVMTypeRef systemInfoGetPageSizeFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetProcessorArchitectureFunction_ = nullptr;
    LLVMTypeRef systemInfoGetProcessorArchitectureFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetAllocationGranularityFunction_ = nullptr;
    LLVMTypeRef systemInfoGetAllocationGranularityFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetTotalPhysicalMemoryBytesFunction_ = nullptr;
    LLVMTypeRef systemInfoGetTotalPhysicalMemoryBytesFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetAvailablePhysicalMemoryBytesFunction_ = nullptr;
    LLVMTypeRef systemInfoGetAvailablePhysicalMemoryBytesFunctionType_ = nullptr;
    LLVMValueRef systemInfoGetMemoryLoadPercentFunction_ = nullptr;
    LLVMTypeRef systemInfoGetMemoryLoadPercentFunctionType_ = nullptr;
    // Stage 55.3 "Diagnostics".
    LLVMValueRef debugIsDebuggerPresentFunction_ = nullptr;
    LLVMTypeRef debugIsDebuggerPresentFunctionType_ = nullptr;
    LLVMValueRef debugCaptureStackFrameCountFunction_ = nullptr;
    LLVMTypeRef debugCaptureStackFrameCountFunctionType_ = nullptr;
    LLVMValueRef debugOutputStringFunction_ = nullptr;
    LLVMTypeRef debugOutputStringFunctionType_ = nullptr;
    // Pre-Stage-56 "utilidades basicas" round.
    LLVMValueRef threadSleepFunction_ = nullptr;
    LLVMTypeRef threadSleepFunctionType_ = nullptr;
    LLVMValueRef fileCopyFunction_ = nullptr;
    LLVMTypeRef fileCopyFunctionType_ = nullptr;
    LLVMValueRef moveFileFunction_ = nullptr;
    LLVMTypeRef moveFileFunctionType_ = nullptr;
    LLVMValueRef nativeThrowFunction_ = nullptr;
    LLVMTypeRef nativeThrowFunctionType_ = nullptr;
    LLVMValueRef raiseExceptionFunction_ = nullptr;
    LLVMTypeRef raiseExceptionFunctionType_ = nullptr;
    LLVMValueRef exceptionCurrentFunction_ = nullptr;
    LLVMTypeRef exceptionCurrentFunctionType_ = nullptr;
    LLVMValueRef exceptionClearFunction_ = nullptr;
    LLVMTypeRef exceptionClearFunctionType_ = nullptr;
    LLVMValueRef ehFilterFunction_ = nullptr;
    LLVMTypeRef ehFilterFunctionType_ = nullptr;
    LLVMValueRef panicEhFilterFunction_ = nullptr;
    LLVMTypeRef panicEhFilterFunctionType_ = nullptr;
    std::unordered_map<std::size_t, LLVMValueRef> typedCatchFilters_;
    // Multi-catch fix -- exception type ordinal -> the stable RTTI type id its catch clause tests
    // against on the parent-side dispatch path. See lowerTryRegion's catchswitch comment.
    std::unordered_map<std::size_t, std::uint64_t> typedCatchTypeIds_;
    // Multi-catch fix -- innermost-last stack of the ENCLOSING catch clause bodies currently being
    // lowered. Catch bodies are no longer lowered inside a funclet, so `rethrow` can no longer find
    // its exception by walking funcletStack_; this is where it looks instead.
    //
    // CC1 EH-001 added the second field. `loopDepthAtEntry` is loopStack_.size() at the moment the
    // clause body started lowering, and it is what tells a `break`/`continue` whether it LEAVES this
    // catch body or merely leaves a loop/switch that was opened INSIDE it: a structured exit whose
    // target loop frame already existed when the body began lands outside the body; one whose frame
    // was pushed inside it does not. See structuredExitLeavesCatchBody().
    std::vector<ActiveCatchBody> activeCatchExceptions_;
    LLVMValueRef sehPersonalityFunction_ = nullptr;
    LLVMTypeRef sehPersonalityFunctionType_ = nullptr;
    LLVMValueRef memsetFunction_ = nullptr;
    LLVMTypeRef memsetFunctionType_ = nullptr;
    LLVMValueRef memcpyFunction_ = nullptr;
    LLVMTypeRef memcpyFunctionType_ = nullptr;
    LLVMValueRef assumeFunction_ = nullptr;
    LLVMTypeRef assumeFunctionType_ = nullptr;
    LLVMTypeRef descriptorType_ = nullptr;
    LLVMTypeRef descriptorAccessorFunctionType_ = nullptr;
    LLVMTypeRef interfaceMapEntryType_ = nullptr;
    LLVMTypeRef interfaceMapType_ = nullptr;
    LLVMTypeRef destructionEntryFunctionType_ = nullptr;
    std::unordered_map<std::size_t, LLVMValueRef> descriptorGlobals_;
    // NS304-T13-F07: per-allocator-type deallocate thunks (void(void* context, void* ptr)).
    std::unordered_map<std::size_t, LLVMValueRef> allocatorThunks_;
    std::unordered_map<std::size_t, FunctionInfo> descriptorAccessorFunctions_;
    // Imported descriptors are only promised through their PE-safe accessor. Local descendants
    // of those descriptors also use their accessors so the complete base chain is initialized
    // before an object publishes its most-derived descriptor pointer.
    std::unordered_set<std::size_t> runtimeDescriptorAccessorTypes_;
    std::unordered_map<std::size_t, LLVMValueRef> interfaceMapGlobals_;
    std::unordered_map<std::size_t, FunctionInfo> destructionFunctions_;
    std::unordered_map<std::size_t, FunctionInfo> objectDestructionFunctions_;
    LLVMTypeRef valueDropEntryFunctionType_ = nullptr;
    std::unordered_map<std::string, FunctionInfo> valueDropEntries_;
    std::unordered_map<std::size_t, LLVMValueRef> vtableGlobals_;
    std::unordered_map<std::size_t, LLVMValueRef> staticFieldGlobals_;
    std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*> staticFieldInfo_;
    std::unordered_map<std::size_t, FunctionInfo> staticFieldAccessorFunctions_;
    std::unordered_map<std::size_t, LLVMValueRef> staticInitStateGlobals_;
    // Stage 54.3 "Mutex" (fix) -- see defineTypeInitializers's own comment for the full guard
    // design this owner-thread-id global enables (distinguishing a genuine cross-thread race from
    // a same-thread self-referencing static field initializer recursing into its own cctor).
    std::unordered_map<std::size_t, LLVMValueRef> staticInitOwnerGlobals_;
    std::unordered_map<std::size_t, FunctionInfo> staticInitFunctions_;
    std::unordered_map<std::size_t, FunctionInfo> staticConstructorBodies_;
    // T01-F06. Each declaration owns private module storage even in partitioned builds; only the
    // code unit that defines the containing function emits the storage and its unload thunk.
    std::unordered_map<std::size_t, StaticLocalStorage> staticLocalStorage_;
    // Non-null only while the generated body of one type-initialization guard is being lowered.
    // Potentially throwing calls and native-unwind panic use this cleanuppad to publish the
    // terminal Failed state before preserving the original unwind to the caller.
    LLVMBasicBlockRef staticInitializationFailureUnwindBlock_ = nullptr;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::unordered_map<std::string, LLVMValueRef> stringGlobals_;
    // C8 "type root": one name constant per class, keyed by its global's name.
    std::unordered_map<std::string, LLVMValueRef> typeNameGlobals_;
    std::size_t stringSequence_ = 0;
    // C1.2 "import identity": plain NUL-terminated C strings (library names, external symbol
    // names) and the two Win32 entry points a runtime-bound import needs. All three stay null in
    // a project with no ambiguous import name, which is the normal case.
    std::unordered_set<std::string> definedRuntimeBoundImports_;
    std::unordered_map<std::string, LLVMValueRef> cStringGlobals_;
    std::size_t cStringSequence_ = 0;
    LLVMValueRef loadLibraryFunction_ = nullptr;
    LLVMTypeRef loadLibraryFunctionType_ = nullptr;
    LLVMValueRef getProcAddressFunction_ = nullptr;
    LLVMTypeRef getProcAddressFunctionType_ = nullptr;
    // C1.3 "callback exception boundary": one guard thunk per target function, cached by the
    // target's mangled name -- two `extern C delegate` conversions of the same method must yield
    // the SAME address, or delegate equality and host-side identity checks would break.
    std::unordered_map<std::string, FunctionInfo> callbackGuardThunks_;
    LLVMValueRef currentFunction_ = nullptr;
    const frontend::ProjectOopMirFunction* currentMirFunction_ = nullptr;
    LLVMMetadataRef currentDebugSubprogram_ = nullptr;
    LLVMMetadataRef currentDebugScope_ = nullptr;
    std::uint32_t currentDebugFileKey_ = 0;
    LLVMValueRef thisValue_ = nullptr;
    LLVMValueRef closureEnvironment_ = nullptr;
    LLVMValueRef constructionThisOverride_ = nullptr;
    std::unordered_map<std::size_t, LLVMValueRef> parameterStorage_;
    std::unordered_map<std::size_t, LLVMValueRef> localStorage_;
    // Only custom-destructor aggregates need an explicit guard. Aggregates whose drop consists
    // solely of owned fields use the cleared field slots themselves as optimized drop flags.
    std::unordered_map<std::size_t, LLVMValueRef> parameterValueDropFlags_;
    std::unordered_map<std::size_t, LLVMValueRef> localValueDropFlags_;
    // Item 323. Backing bytes for promoted class objects, separate from each local's pointer slot.
    std::unordered_map<std::size_t, LLVMValueRef> stackObjectStorage_;
    std::unordered_map<std::size_t, LLVMValueRef> parameterCaptureCells_;
    std::unordered_map<std::size_t, LLVMValueRef> localCaptureCells_;
    LLVMValueRef thisCaptureCell_ = nullptr;
    std::vector<LLVMValueRef> ownedCaptureCells_;
    LLVMBasicBlockRef captureCellExceptionalCleanupBlock_ = nullptr;
    LLVMTypeRef captureCellFunctionType_ = nullptr;
    LLVMValueRef captureCellRetainFunction_ = nullptr;
    LLVMValueRef captureCellReleaseFunction_ = nullptr;
    std::unordered_map<std::string, LLVMValueRef> closureContextDestroyFunctions_;
    std::unordered_map<std::size_t, LLVMBasicBlockRef> exceptionDispatchBlocks_;
    std::unordered_map<std::size_t, const frontend::ProjectOopMirNode*> cleanupRegions_;
    std::unordered_map<std::size_t, CachedCleanup> cleanupChainCache_;
    std::vector<std::size_t> activeCleanupRegions_;
    // T03-F03. The value/address pair for each nested match governing expression. The value is
    // computed once; generated MatchValue nodes select the representation their HIR context needs.
    std::vector<std::pair<LLVMValueRef, LLVMValueRef>> matchValueStack_;
    // NS304-T15-F05. Active overflow policy for the wrapped expression ("checked"/"saturating",
    // empty outside). The decimal arithmetic ops consult this single-threaded flag.
    std::string currentOverflowPolicy_;
    std::vector<ExceptionHandlerContext> exceptionHandlerStack_;
    std::vector<FuncletContext> funcletStack_;
    std::vector<LoopTargets> loopStack_;
    LLVMValueRef returnStorage_ = nullptr;
    // Win64 aggregate ABI: the incoming sret pointer when the CURRENT function returns an
    // indirect struct -- returns store through it and `ret void`. Null otherwise.
    LLVMValueRef sretStorage_ = nullptr;
    std::size_t blockCounter_ = 0;
};
}
