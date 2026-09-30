#pragma once

#include "nativesharp/Language/LanguageEdition.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// The MIR schema is an internal producer/consumer boundary. It is versioned
// independently from the language and runtime ABI so tools can reject a dump
// whose node/ownership semantics they do not understand.
inline constexpr std::uint16_t kProjectOopMirSchemaMajor = 1;
// MINOR 14: namespace state has explicit load/address nodes and stable storage identity.
// MINOR 15: typed params and generalized aggregate spreads have explicit lowering nodes.
// MINOR 16: null-coalescing value/store control flow is explicit and single-evaluation.
// MINOR 17: tuple layouts plus alias-safe deconstruction stores are explicit.
// MINOR 18: abort-profile panic is an explicit divergent node with builtin:never type.
// MINOR 19: closure capture provenance and shared-cell environment layout are inspectable.
inline constexpr std::uint16_t kProjectOopMirSchemaMinor = 19;

enum class ProjectOopMirAbiPassing
{
    Direct,
    Indirect
};

const char* projectOopMirAbiPassingName(ProjectOopMirAbiPassing passing) noexcept;

enum class ProjectOopMirNodeKind
{
    Block,
    LocalDeclaration,
    ExpressionStatement,
    // NativeSharp 2.0 item 298. Mirrors the structured HIR assembly contract without reparsing
    // source text. See ProjectOopHirNodeKind::InlineAssembly and its four child kinds.
    InlineAssembly,
    InlineAssemblyLine,
    InlineAssemblyInput,
    InlineAssemblyOutput,
    InlineAssemblyClobber,
    If,
    While,
    DoWhile,
    For,
    // Mirrors the HIR shape exactly: children[0] governing expression, children[1..] sections;
    // a section carries its folded label values and its default flag. See ProjectOopHirModel.h.
    Switch,
    SwitchSection,
    Break,
    Continue,
    Return,
    TryRegion,
    CatchRegion,
    FinallyRegion,
    UsingRegion,
    Throw,
    Rethrow,
    CleanupContinuation,
    OwnedParameterCleanup,
    MoveValue,
    MoveArrayElement,
    MoveOwnedField,
    OwnedTemporaryBorrow,
    FullExpressionCleanup,
    PointerLoad,
    // C2.1 "fixed arrays" -- see ProjectOopHirNodeKind::RawElementAddress. `immediate` carries the
    // element size in bytes, `alignment` its alignment.
    RawElementAddress,
    // CC1 fase 4B. `immediate` = tamano del elemento, `alignment` = su alineacion. Un hijo: el
    // contador.
    StackAllocation,
    // CC1 fase 4C. `immediate` = tamano del elemento. Sin hijos.
    RawAllocation,
    // CC1 fase 4D. `immediate` = tamano del elemento (el paso). `operation` lleva `+` o `-`. Dos
    // hijos. El tipo del nodo distingue desplazamiento (`T*`) de diferencia (`builtin:nint`), que
    // es la misma distincion que ya hizo el frontend y no una decidida otra vez aqui.
    PointerArithmetic,
    // C6 -- see ProjectOopHirNodeKind::ValueConstruction. `immediate` is the struct's size,
    // `alignment` its alignment, `calleeName` the constructor to run (empty for a default).
    ValueConstruct,
    // Item 318. First child is exact T* destination storage; the remaining prefix of
    // argumentCount children are constructor arguments. Uses no allocator.
    PlacementConstruct,
    // NativeSharp 3.0 T01-F02. `immediate`/`alignment` are the element layout;
    // `secondaryImmediate` is the span carrier size; integerValue/bitStorageSize are the
    // pointer/count byte offsets. Children are converted element values in source order.
    ParameterPack,
    // C7 -- see ProjectOopHirNodeKind::AggregateInitializer. `immediate` is the aggregate's size,
    // `alignment` its alignment; children are AggregateElementStore nodes.
    AggregateInitialize,
    // NativeSharp 3.0 T01-F05. Mirrors AggregateSpread; descriptor children carry exact byte
    // offsets and sizes so the backend copies only fields not explicitly initialized.
    AggregateSpreadStore,
    // C7 -- see ProjectOopHirNodeKind::AggregateElement. `immediate` is the byte offset.
    AggregateElementStore,
    TupleDeconstruct,
    TupleDeconstructElementStore,
    // C6 -- see ProjectOopHirNodeKind::TemporaryAddress.
    TemporaryAddress,
    AddressValue,
    BorrowValue,
    ThisValue,
    ParameterValue,
    LocalValue,
    FieldLoad,
    NullConditionalFieldLoad,
    // Item 329. A bit field has no standalone address. BitFieldAddress is a logical lvalue
    // descriptor consumed only by Store/CompoundStore; it identifies the containing allocation
    // unit so the backend can perform one masked read-modify-write.
    BitFieldLoad,
    StaticFieldLoad,
    NamespaceStateLoad,
    ParameterAddress,
    LocalAddress,
    FieldAddress,
    BitFieldAddress,
    StaticFieldAddress,
    NamespaceStateAddress,
    IntegerConstant,
    FloatingConstant,
    BooleanConstant,
    NullConstant,
    DefaultValue,
    StringLiteral,
    ArrayAllocate,
    ArrayLength,
    ArrayElementLoad,
    ArrayElementAddress,
    StringLength,
    StringElementLoad,
    // ID 129. Ver ProjectOopHirNodeKind::EnumName para la forma y el porque.
    EnumName,
    StringConcat,
    StringEquals,
    StringSubstring,
    StringCharacterPointer,
    StringFromCharArray,
    CoreConsoleWriteLine,
    // C8 -- see ProjectOopHirNodeKind::CoreObjectTypeName / CoreObjectReferenceHash.
    CoreObjectTypeName,
    CoreObjectReferenceHash,
    // C8 struct half -- see ProjectOopHirNodeKind::AggregateEquals / AggregateHash.
    AggregateEquals,
    AggregateHash,
    // Stage 53.1 "Time" -- see ProjectOopHirNodeKind::CoreClockQuery* above.
    CoreClockQueryCounter,
    CoreClockQueryFrequency,
    CoreClockQuerySystemFileTime,
    // Stage 53.2 "Environment" -- see ProjectOopHirNodeKind::CoreEnvironment* above.
    CoreEnvironmentTickCount64,
    CoreEnvironmentProcessorCount,
    CoreEnvironmentExit,
    // Stage 53.2b "Environment (variables)" -- see ProjectOopHirNodeKind::CoreEnvironmentGet/
    // SetVariable above.
    CoreEnvironmentGetVariable,
    CoreEnvironmentSetVariable,
    // Stage 53.3 "Filesystem/Paths" -- see ProjectOopHirNodeKind::CoreFileExists/CoreFileDelete/
    // CoreDirectoryExists/CoreDirectoryCreate above.
    CoreFileExists,
    CoreFileDelete,
    CoreDirectoryExists,
    CoreDirectoryCreate,
    // Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- see
    // ProjectOopHirNodeKind::CoreFileReadAllText/CoreFileWriteAllText/CoreFileAppendAllText
    // above.
    CoreFileReadAllText,
    CoreFileWriteAllText,
    CoreFileAppendAllText,
    // Bloque 4, fase A -- ver ProjectOopHirNodeKind::CoreFileGetSize.
    CoreFileGetSize,
    // Bloque 4, fase A -- ver ProjectOopHirNodeKind::CoreFileReadAllBytes y siguientes.
    CoreFileReadAllBytes,
    CoreFileReadBytes,
    CoreFileWriteAllBytes,
    CoreEnvironmentCurrentDirectory,
    // Bloque 4, fase C -- ver MethodCoreOperationKind::ProcessRun.
    CoreProcessRun,
    // Stage 53.4 "Filesystem/Paths (cierre)" -- see ProjectOopHirNodeKind::CoreDirectoryDelete/
    // CorePathGetFullPath above.
    CoreDirectoryDelete,
    CorePathGetFullPath,
    // Stage 53.5 "Command-line arguments" -- see ProjectOopHirNodeKind::
    // CoreEnvironmentGetCommandLineArgCount/CoreEnvironmentGetCommandLineArgs above.
    CoreEnvironmentGetCommandLineArgCount,
    CoreEnvironmentGetCommandLineArgs,
    // Stage 53.6 "Enumeracion de directorios" -- see ProjectOopHirNodeKind::
    // CoreDirectoryGetFileCount/CoreDirectoryGetFiles/CoreDirectoryGetDirectoryCount/
    // CoreDirectoryGetDirectories above.
    CoreDirectoryGetFileCount,
    CoreDirectoryGetFiles,
    CoreDirectoryGetDirectoryCount,
    CoreDirectoryGetDirectories,
    // Stage 54.1 "Threads" -- see ProjectOopHirNodeKind::CoreThreadCreate/CoreThreadJoin/
    // CoreThreadDetach above.
    CoreThreadCreate,
    CoreThreadJoin,
    CoreThreadDetach,
    // Stage 54.2 "Atomics" -- see ProjectOopHirNodeKind::CoreInterlockedIncrementInt32/.../
    // CoreInterlockedCompareExchangeInt64 above.
    CoreAtomicLoadInt32,
    CoreAtomicStoreInt32,
    CoreInterlockedIncrementInt32,
    CoreInterlockedDecrementInt32,
    CoreInterlockedAddInt32,
    CoreInterlockedExchangeInt32,
    CoreInterlockedCompareExchangeInt32,
    CoreAtomicLoadInt64,
    CoreAtomicStoreInt64,
    CoreInterlockedIncrementInt64,
    CoreInterlockedDecrementInt64,
    CoreInterlockedAddInt64,
    CoreInterlockedExchangeInt64,
    CoreInterlockedCompareExchangeInt64,
    CoreAtomicLoadPointer,
    CoreAtomicStorePointer,
    CoreInterlockedExchangePointer,
    CoreInterlockedCompareExchangePointer,
    CoreAtomicThreadFence,
    CoreAtomicCompilerFence,
    CoreSimdFloat32x4Add,
    CoreSimdFloat32x4Subtract,
    CoreSimdFloat32x4Multiply,
    CoreSimdInt128Add,
    CoreSimdInt128Subtract,
    CoreSimdInt128Multiply,
    CoreSimdFloat256Add,
    CoreSimdFloat256Subtract,
    CoreSimdFloat256Multiply,
    CoreSimdInt256Add,
    CoreSimdInt256Subtract,
    CoreSimdInt256Multiply,
    CoreSimdLoadAligned128,
    CoreSimdStoreAligned128,
    CoreSimdLoadUnaligned128,
    CoreSimdStoreUnaligned128,
    CoreHalfFromFloat,
    CoreHalfToFloat,
    CoreSimdSwizzleWzyx,
    CoreSimdShuffleLowHigh,
    CoreSimdSqrt,
    CoreSimdReciprocalSqrt,
    CoreSimdFusedMultiplyAdd,
    CoreBitPopulationCount,
    CoreBitLeadingZeroCount,
    CoreBitTrailingZeroCount,
    CoreBitRotateLeft,
    CoreBitRotateRight,
    CoreBitByteSwap,
    CoreCpuPrefetchRead,
    CoreCpuPause,
    CoreCpuReadTimestamp,
    // Stage 54.3 "Mutex" -- see ProjectOopHirNodeKind::CoreMutexCreate/CoreMutexLock/
    // CoreMutexUnlock/CoreMutexClose above.
    CoreMutexCreate,
    CoreMutexCreateNamed,
    CoreMutexLock,
    CoreMutexUnlock,
    CoreMutexClose,
    // Stage 54.4 "Condition Variables" -- see ProjectOopHirNodeKind::CoreMonitor* above.
    CoreMonitorEnter,
    CoreMonitorExit,
    CoreMonitorWait,
    CoreMonitorPulse,
    CoreMonitorPulseAll,
    // Stage 54.5 "Otras primitivas de sincronizacion" -- see ProjectOopHirNodeKind::CoreSemaphore*/
    // CoreEvent* above.
    CoreSemaphoreCreate,
    CoreSemaphoreCreateNamed,
    CoreSemaphoreWait,
    CoreSemaphoreRelease,
    CoreSemaphoreClose,
    CoreEventCreate,
    CoreEventCreateNamed,
    CoreEventSet,
    CoreEventReset,
    CoreEventWait,
    CoreEventClose,
    // Stage 55.1/55.2/55.3 -- see ProjectOopHirNodeKind::CoreProcess*/CoreSystemInfo*/CoreDebug*
    // above. Every one of these is a trivial passthrough in ProjectOopMirModel.cpp (no
    // memberOrdinal/immediate resolution needed, unlike CoreMonitorWait) -- children (if any) are
    // copied generically by the enclosing loop.
    CoreProcessGetCurrentProcessId,
    CoreProcessGetWorkingSetSize,
    CoreProcessGetKernelTime,
    CoreProcessGetUserTime,
    CoreProcessGetPriorityClass,
    CoreProcessSetPriorityClass,
    CoreSystemInfoGetPageSize,
    CoreSystemInfoGetProcessorArchitecture,
    CoreSystemInfoGetAllocationGranularity,
    CoreSystemInfoGetTotalPhysicalMemoryBytes,
    CoreSystemInfoGetAvailablePhysicalMemoryBytes,
    CoreSystemInfoGetMemoryLoadPercent,
    CoreDebugIsDebuggerPresent,
    CoreDebugCaptureStackFrameCount,
    CoreDebugOutputString,
    // Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29).
    CoreThreadSleep,
    CoreFileCopy,
    CoreFileMove,
    CoreDirectoryMove,
    DelegateCreate,
    LambdaCreate,
    ClosureCaptureLoad,
    ClosureCaptureAddress,
    CaptureCellReference,
    ClosureCaptureCellReference,
    DelegateInvoke,
    EventAdd,
    EventRemove,
    EventInvoke,
    Unary,
    Binary,
    Conditional,
    NullCoalescing,
    NullCoalescingStore,
    Store,
    // IDs 79 y 81 -- ver ProjectOopHirNodeKind::CompoundStore, misma forma. `immediate` lleva el
    // paso en bytes SOLO cuando el destino es un puntero (`elementTypeKey` no vacio en el HIR);
    // vale 0 para un destino numerico, que no lo necesita.
    CompoundStore,
    Convert,
    RuntimeTypeTest,
    SafeReferenceCast,
    CheckedReferenceCast,
    DirectInstanceCall,
    DirectStaticCall,
    VirtualInstanceCall,
    InterfaceInstanceCall,
    AllocateAndConstruct,
    Deallocate,
    ObjectDestroy,
    // T07 abort-profile panic: __nativesharp_trap(Panic) followed by unreachable.
    Panic
};

struct ProjectOopMirNode final
{
    ProjectOopMirNodeKind kind = ProjectOopMirNodeKind::Block;
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::string elementTypeKey;
    std::string stringValue;
    // Item 381. Source-facing name/shape retained for CodeView after HIR lowering.
    std::string debugDisplayName;
    lexer::TokenKind operation = lexer::TokenKind::Invalid;
    std::optional<std::size_t> memberOrdinal;
    std::optional<std::size_t> namespaceFunctionOrdinal;
    std::optional<std::size_t> namespaceStateOrdinal;
    // Stage 54.4 "Condition Variables" -- mirrors ProjectOopHirNode::secondaryMemberOrdinal (see
    // its comment there). Set only on a CoreMonitorWait node.
    std::optional<std::size_t> secondaryMemberOrdinal;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    std::optional<std::size_t> constructionOrdinal;
    std::optional<std::size_t> targetTypeOrdinal;
    std::optional<std::size_t> exceptionTypeOrdinal;
    std::optional<std::size_t> cleanupRegionOrdinal;
    std::optional<std::size_t> mirRegionOrdinal;
    std::optional<std::size_t> exceptionTargetRegionOrdinal;
    std::optional<std::size_t> targetScopeOrdinal;
    std::optional<std::size_t> disposeContractMemberOrdinal;
    std::optional<std::size_t> disposeImplementationMemberOrdinal;
    std::vector<std::size_t> cleanupRegionOrdinals;
    std::optional<MethodCleanupContinuationKind> cleanupContinuationKind;
    std::optional<MethodBorrowKind> borrowKind;
    std::optional<MethodLocalBorrowSourceKind> localBorrowSourceKind;
    std::optional<std::size_t> virtualSlot;
    std::optional<std::size_t> interfaceSlot;
    std::optional<ProjectConversionKind> conversionKind;
    // SwitchSection only -- carried through from the HIR unchanged. See ProjectOopHirModel.h.
    std::vector<std::int64_t> switchLabels;
    bool switchHasDefault = false;
    std::uint64_t immediate = 0;
    // Stage 54.4 "Condition Variables" -- set only on a CoreMonitorWait node, resolved during
    // HIR->MIR conversion exactly like `immediate` above (which, for THIS node kind only, holds
    // the `cv_` field's byte offset rather than an ordinary FieldAddress's own offset): the
    // `lock_` field's byte offset. See ProjectOopLlvmLowerer.cpp's CoreMonitorWait case for where
    // both offsets get consumed (two GEPs off one already-lowered receiver pointer).
    std::uint64_t secondaryImmediate = 0;
    std::uint64_t alignment = 1;
    // Item 329. Present together on BitFieldLoad/BitFieldAddress. `immediate` remains the byte
    // offset of the allocation unit; these values describe that unit and the logical slice in it.
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
    std::uint64_t bitStorageSize = 0;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
    // Item 312. Mirrors ProjectOopHirNode::referenceLocal for backend slot materialization.
    bool referenceLocal = false;
    // Item 313. Mirrors ProjectOopHirNode::referenceResult for physical call ABI selection.
    bool referenceResult = false;
    // Item 377. Retained for CodeView/PDB ownership and borrow visualization.
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    // Item 378. Scope-bound destruction remains visible to late debug-info lowering.
    bool automaticDestruction = false;
    bool variableLengthArray = false;
    std::size_t variableLengthArrayRank = 0;
    bool placementConstructed = false;
    // Item 323. Mirrors the HIR proof; AllocateAndConstruct consumes a frame-owned object buffer.
    bool stackPromotedObject = false;
    // Item 308. Mirrors ProjectOopHirNode::volatileAccess without re-inferring declarations.
    bool volatileAccess = false;
    bool allocationFailureReturnsNull = false;
    bool zeroInitialize = false;
    bool baseReceiver = false;
    bool targetIsInterface = false;
    bool dynamicDestruction = false;
    bool valueTypeNoOpDeletion = false;
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
    bool catchAll = false;
    bool interfaceCleanupDispatch = false;
    // Item 300: explicit optimization contract for InlineAssembly. These are derived once from
    // the validated MIR children; late consumers do not guess barrier semantics from raw text.
    bool inlineAssemblyHasSideEffects = false;
    bool inlineAssemblyClobbersMemory = false;
    bool inlineAssemblyClobbersFlags = false;
    std::size_t argumentCount = 0;
    std::vector<std::size_t> argumentParameterOrdinals;
    std::string calleeName;
    std::vector<std::string> argumentTypeKeys;
    std::vector<std::unique_ptr<ProjectOopMirNode>> children;
};

struct ProjectOopMirFieldInitializer final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    bool isStatic = false;
    bool isConst = false;
    std::unique_ptr<ProjectOopMirNode> value;
};


struct ProjectOopMirStructLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    bool isUnion = false;
    std::uint64_t pack = 0;
    std::uint64_t minimumAlignment = 0;
    bool hasExplicitFieldOffsets = false;
    ProjectOopMirAbiPassing abiPassing = ProjectOopMirAbiPassing::Direct;
    std::vector<ObjectFieldLayout> fields;
};

// Debug and other late consumers need the authoritative physical class layout without reaching
// back into the mutable frontend pipeline.  This is the reference-shaped counterpart of
// ProjectOopMirStructLayout: object size includes the descriptor header and inherited subobject;
// fields carry their final byte offsets.
struct ProjectOopMirClassLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint64_t headerSize = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::uint64_t baseSubobjectSize = 0;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    std::vector<ObjectFieldLayout> fields;
};

struct ProjectOopMirFunctionAbi final
{
    ProjectOopMirAbiPassing returnPassing = ProjectOopMirAbiPassing::Direct;
    std::optional<std::size_t> hiddenReturnParameterOrdinal;
    std::vector<ProjectOopMirAbiPassing> parameterPassing;
    std::vector<std::size_t> parameterPhysicalOrdinals;
    std::size_t physicalParameterCount = 0;
};

enum class ProjectOopMirCleanupKind
{
    LocalOwner,
    TemporaryOwner,
    OwnedParameter,
    Using,
    Finally
};

const char* projectOopMirCleanupKindName(ProjectOopMirCleanupKind kind) noexcept;

struct ProjectOopMirCleanupRegion final
{
    std::size_t ordinal = 0;
    ProjectOopMirCleanupKind kind = ProjectOopMirCleanupKind::LocalOwner;
    const ProjectOopMirNode* action = nullptr;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    std::string typeKey;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    bool automaticDestruction = false;
};

enum class ProjectOopMirNonlocalExitKind
{
    Return,
    Break,
    Continue,
    Throw,
    Rethrow,
    CleanupContinuation
};

const char* projectOopMirNonlocalExitKindName(ProjectOopMirNonlocalExitKind kind) noexcept;

struct ProjectOopMirNonlocalExit final
{
    ProjectOopMirNonlocalExitKind kind = ProjectOopMirNonlocalExitKind::Return;
    const ProjectOopMirNode* node = nullptr;
    std::vector<std::size_t> cleanupRegionOrdinals;
    std::optional<std::size_t> targetScopeOrdinal;
    std::optional<std::size_t> exceptionTargetRegionOrdinal;
    std::optional<MethodCleanupContinuationKind> cleanupContinuationKind;
};

struct ProjectOopMirExceptionFlow final
{
    bool requiresPersonality = false;
    std::vector<std::size_t> protectedRegionOrdinals;
    std::size_t catchRegionCount = 0;
    std::size_t finallyRegionCount = 0;
    std::vector<ProjectOopMirNonlocalExit> nonlocalExits;
};

struct ProjectOopMirStaticField final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    std::string name;
    std::string typeKey;
    bool isReadonly = false;
    bool isVolatile = false;
    bool isConst = false;
    bool isImported = false;
    std::string genericSpecializationKey;
    // Mutable static storage remains owned by the producer. Consumers call the stable accessor
    // carried in NSMETA instead of defining a second global with a compilation-local ordinal.
    std::string storageSymbol;
    std::string accessorName;
    bool hasConstantInitializer = false;
    std::uint64_t constantIntegerValue = 0;
    double constantFloatingValue = 0.0;
    bool constantBooleanValue = false;
    std::string constantStringValue = {};
    // Stage 51.x: an enum member's declared type is its own enum type (so it type-checks and
    // compares correctly against other enum-typed expressions), not its int-sized underlying
    // representation -- so `ProjectTypeConversion::isIntegral(typeKey)` returns false for it.
    // This flag lets the LLVM lowerer take the integral constant-materialization path anyway.
    bool isEnumConstant = false;
    std::optional<std::size_t> namespaceStateOrdinal = std::nullopt;
};


struct ProjectOopMirDispatchTable final
{
    // Imported tables describe producer-owned runtime data. They participate in semantic slot
    // selection, but the consumer must neither define nor initialize their globals locally.
    bool isImported = false;
    std::string genericSpecializationKey;
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::string typeKey;
    std::string descriptorName;
    std::string descriptorAccessorName;
    std::string vtableName;
    std::uint64_t stableTypeId = 0;
    // C8 "type root": the type's own name, as source wrote it -- the string
    // NativeSharp.Core.Object.ToString() hands back. It goes into the class descriptor as a
    // pointer to a length-prefixed constant with exactly the layout of a string literal, so
    // returning it costs a load and allocates nothing.
    std::string typeDisplayName;
    std::string typeNameGlobalName;
    std::vector<std::size_t> slotMemberOrdinals;
    std::vector<std::string> slotCalleeNames;
    std::string destructionEntryName;
    std::string objectDestructionEntryName;
    std::vector<std::size_t> destructorMemberOrdinals;
    std::vector<std::string> destructorCalleeNames;
    // A local class may derive from a class whose destructor bodies and owned-field layout live
    // exclusively in a precompiled producer. After completing every local destruction step, the
    // local entry delegates the remaining base chain through that type's runtime descriptor.
    // The delegated entry also releases the allocation, so the local entry must not free it again.
    std::optional<std::size_t> delegatedBaseDestructionTypeOrdinal;
    // Bloque 1 item 1 (owning field storage) -- instance fields declared `owned` whose type really
    // owns a separate allocation, INCLUDING the ones inherited from base types, in the order they
    // must be destroyed. The destruction entry walks this list after running the declared
    // destructor bodies, so a class that owns something releases it without the author writing a
    // `delete` by hand. Empty for every type that declares no owned field, which is why nothing
    // changes for code that does not use the feature.
    //
    // The byte offset is resolved HERE rather than in the LLVM layer, matching how every other
    // field access in this pipeline works (the backend consumes offsets the frontend computed, and
    // never reaches into the object layout model itself).
    struct OwnedField final
    {
        std::size_t memberOrdinal = 0;
        std::uint64_t offset = 0;
        // A native string owns a flat allocation released directly; a class instance is released
        // through its runtime class descriptor's destruction entry.
        bool isNativeString = false;
    };
    std::vector<OwnedField> ownedFields;

    // Bloque 1 (audit finding "orden de destruccion debe fijarse por contrato") -- the SINGLE,
    // ordered sequence the destruction entry executes. This is the language's destruction-order
    // contract, frozen here and verified by execution
    // (Stage50_11OwnershipExecutionTests: DestructionOrderContract*):
    //
    //     most-derived destructor body
    //     most-derived class's owned fields, in REVERSE declaration order
    //     base destructor body
    //     base class's owned fields, in REVERSE declaration order
    //     ... up the chain
    //
    // -- i.e. destruction is the exact reverse of construction, the same rule scoped owners
    // already follow (NestedScopesDestroyInReverseOrder) and the rule C++ fixes for member
    // subobjects. Two properties fall out of it and are load-bearing:
    //
    //   * a destructor body always runs while ITS OWN class's owned fields are still alive (it
    //     may read them; the null-guarded release right after it skips anything it discharged by
    //     hand), and
    //   * a derived class's fields are released before ANY base state goes away, so a derived
    //     field's destructor may still rely on base fields -- never the other way around.
    //
    // HISTORY: before this contract existed, the entry ran ALL destructor bodies (derived->base)
    // and THEN released ALL owned fields base-first in declaration order -- measured, not read:
    // ~Derived, ~Base, base fields, derived fields. That inverted both properties above (base
    // fields died before derived fields that might reference them, and every base body ran while
    // no field had been released yet). Nothing could have depended on the old order -- it was
    // undocumented and untested -- which is exactly why the audit required freezing one now.
    //
    // Exactly one of the two payloads is meaningful per step: isFieldRelease selects between a
    // body call (destructorCalleeName) and a null-guarded slot release (ownedField).
    struct DestructionStep final
    {
        std::string destructorCalleeName;
        OwnedField ownedField;
        bool isFieldRelease = false;
    };
    std::vector<DestructionStep> destructionSteps;
    struct InterfaceTable final
    {
        std::size_t interfaceTypeOrdinal = 0;
        std::string interfaceTypeKey;
        std::uint64_t stableInterfaceTypeId = 0;
        std::string methodTableName;
        std::vector<std::size_t> interfaceMemberOrdinals;
        std::vector<std::size_t> implementationMemberOrdinals;
        std::vector<std::string> calleeNames;
    };
    std::string interfaceMapName;
    std::vector<InterfaceTable> interfaceTables;
};

struct ProjectOopMirFunction final
{
    std::size_t memberOrdinal = 0;
    std::optional<std::size_t> namespaceFunctionOrdinal;
    std::size_t declaringTypeOrdinal = 0;
    bool constructor = false;
    bool destructor = false;
    bool isStatic = false;
    bool referenceReturn = false;
    bool isNoHeap = false;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    bool isExportedC = false;
    bool isImportedC = false;
    bool isVariadic = false;
    bool isImportedNative = false;
    bool isSyntheticImplicitConstructor = false;
    std::string importDllName;
    // C1.2 "import identity" -- see ProjectOopHirFunction for the full note.
    bool importRequiresRuntimeBinding = false;
    std::string cAbiName;
    bool isLambda = false;
    std::optional<std::size_t> lambdaOrdinal;
    bool hasClosureEnvironment = false;
    std::vector<std::string> captureTypeKeys;
    std::vector<std::string> captureNames;
    std::vector<MethodLambdaCaptureSourceKind> captureSourceKinds;
    std::vector<std::size_t> capturedLocalOrdinals;
    std::vector<std::size_t> capturedParameterOrdinals;
    bool capturesThisCell = false;
    std::string genericSpecializationKey;
    std::string mangledName;
    std::string debugDisplayName;
    std::string returnTypeKey;
    sema::OwnershipKind returnOwnership = sema::OwnershipKind::None;
    std::vector<std::string> parameterNames;
    std::vector<std::string> parameterTypeKeys;
    std::vector<sema::OwnershipKind> parameterOwnerships;
    ProjectOopMirFunctionAbi abi;
    std::vector<std::size_t> localOrdinals;
    std::vector<ProjectOopMirCleanupRegion> cleanupRegions;
    ProjectOopMirExceptionFlow exceptionFlow;
    std::unique_ptr<ProjectOopMirNode> body;
};

// Item 398: a single MIR-owned catalogue of every closed generic identity.  Consumers no longer
// need to rediscover which functions, static fields, and runtime tables belong to one
// specialization by scanning three unrelated collections.
struct ProjectOopMirGenericSpecialization final
{
    std::string key;
    std::vector<std::size_t> functionIndices;
    std::vector<std::size_t> staticFieldIndices;
    std::vector<std::size_t> dispatchTableIndices;
};

// Item 398: the complete class/interface binding consumed by runtime metadata and interface
// cleanup dispatch.  This is deliberately independent of ProjectInterfaceModel so every backend
// decision is available at the MIR boundary.
struct ProjectOopMirInterfaceBinding final
{
    std::size_t implementingTypeOrdinal = 0;
    std::size_t interfaceTypeOrdinal = 0;
    std::string interfaceTypeKey;
    std::uint64_t stableInterfaceTypeId = 0;
    std::string methodTableName;
    std::vector<std::size_t> interfaceMemberOrdinals;
    std::vector<std::size_t> implementationMemberOrdinals;
    std::vector<std::string> calleeNames;
};

class ProjectOopMirModel final
{
public:
    void setLanguageEdition(language::LanguageEdition edition) noexcept;
    language::LanguageEdition languageEdition() const noexcept;
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        std::string& error,
        std::size_t workerCount = 1);

    std::span<const ProjectOopMirFunction> functions() const noexcept;
    std::span<const ProjectOopMirFieldInitializer> fieldInitializers() const noexcept;
    std::span<const ProjectOopMirStaticField> staticFields() const noexcept;
    std::span<const ProjectOopMirStructLayout> structLayouts() const noexcept;
    std::span<const ProjectOopMirClassLayout> classLayouts() const noexcept;
    std::span<const ProjectOopMirDispatchTable> dispatchTables() const noexcept;
    std::span<const ProjectOopMirGenericSpecialization> genericSpecializations() const noexcept;
    std::span<const ProjectOopMirInterfaceBinding> interfaceBindings() const noexcept;
    const ProjectOopMirDispatchTable* dispatchTableForType(std::size_t typeOrdinal) const noexcept;
    const ProjectOopMirInterfaceBinding* interfaceBindingForTypes(
        std::size_t implementingTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
    bool interfaceSlotForMember(
        std::size_t interfaceMemberOrdinal,
        std::uint64_t& stableInterfaceTypeId,
        std::size_t& slot) const noexcept;
    const ProjectOopMirFunction* functionForMember(std::size_t memberOrdinal) const noexcept;
    const ProjectOopMirFunction* functionForNamespaceFunction(
        std::size_t functionOrdinal) const noexcept;    const ProjectOopMirFunction* executableEntryPoint(
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        bool& ambiguous) const noexcept;
    bool isBuilt() const noexcept;

private:
    // C2.1 "fixed arrays": size/alignment of one array element, for both the managed
    // (length-prefixed, heap) and the raw (inline storage / raw pointer) element paths -- one
    // answer, so a GEP stride can never disagree with the layout a field was given.
    std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout(
        std::string_view typeKey) const;

    std::unique_ptr<ProjectOopMirNode> lowerNode(
        const ProjectOopHirNode* node,
        std::string& error);
    const ObjectFieldLayout* fieldLayout(std::size_t memberOrdinal) const noexcept;
    const ObjectAllocationPlan* allocationPlan(std::size_t constructionOrdinal) const noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectObjectLayoutModel* objectLayoutModel_ = nullptr;
    const ProjectVirtualDispatchModel* virtualDispatchModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectDestructionModel* destructionModel_ = nullptr;
    const ProjectOopHirModel* hirModel_ = nullptr;
    std::vector<ProjectOopMirFunction> functions_;
    std::vector<ProjectOopMirFieldInitializer> fieldInitializers_;
    std::vector<ProjectOopMirStaticField> staticFields_;
    std::vector<ProjectOopMirStructLayout> structLayouts_;
    std::vector<ProjectOopMirClassLayout> classLayouts_;
    std::vector<std::optional<std::size_t>> functionByMember_;
    std::vector<std::optional<std::size_t>> functionByNamespaceFunction_;
    std::vector<ProjectOopMirDispatchTable> dispatchTables_;
    std::vector<std::optional<std::size_t>> dispatchTableByType_;
    std::vector<ProjectOopMirGenericSpecialization> genericSpecializations_;
    std::vector<ProjectOopMirInterfaceBinding> interfaceBindings_;
    bool built_ = false;
    language::LanguageEdition languageEdition_ = language::LanguageEdition::V2;
};

const char* projectOopMirNodeKindName(ProjectOopMirNodeKind kind) noexcept;
}
