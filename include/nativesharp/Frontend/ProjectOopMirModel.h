#pragma once

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
enum class ProjectOopMirNodeKind
{
    Block,
    LocalDeclaration,
    ExpressionStatement,
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
    // C7 -- see ProjectOopHirNodeKind::AggregateInitializer. `immediate` is the aggregate's size,
    // `alignment` its alignment; children are AggregateElementStore nodes.
    AggregateInitialize,
    // C7 -- see ProjectOopHirNodeKind::AggregateElement. `immediate` is the byte offset.
    AggregateElementStore,
    // C6 -- see ProjectOopHirNodeKind::TemporaryAddress.
    TemporaryAddress,
    AddressValue,
    BorrowValue,
    ThisValue,
    ParameterValue,
    LocalValue,
    FieldLoad,
    StaticFieldLoad,
    ParameterAddress,
    LocalAddress,
    FieldAddress,
    StaticFieldAddress,
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
    CoreInterlockedIncrementInt32,
    CoreInterlockedDecrementInt32,
    CoreInterlockedAddInt32,
    CoreInterlockedExchangeInt32,
    CoreInterlockedCompareExchangeInt32,
    CoreInterlockedIncrementInt64,
    CoreInterlockedDecrementInt64,
    CoreInterlockedAddInt64,
    CoreInterlockedExchangeInt64,
    CoreInterlockedCompareExchangeInt64,
    // Stage 54.3 "Mutex" -- see ProjectOopHirNodeKind::CoreMutexCreate/CoreMutexLock/
    // CoreMutexUnlock/CoreMutexClose above.
    CoreMutexCreate,
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
    CoreSemaphoreWait,
    CoreSemaphoreRelease,
    CoreSemaphoreClose,
    CoreEventCreate,
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
    DelegateInvoke,
    EventAdd,
    EventRemove,
    EventInvoke,
    Unary,
    Binary,
    Conditional,
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
    Deallocate
};

struct ProjectOopMirNode final
{
    ProjectOopMirNodeKind kind = ProjectOopMirNodeKind::Block;
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::string elementTypeKey;
    std::string stringValue;
    lexer::TokenKind operation = lexer::TokenKind::Invalid;
    std::optional<std::size_t> memberOrdinal;
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
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
    bool allocationFailureReturnsNull = false;
    bool zeroInitialize = false;
    bool baseReceiver = false;
    bool targetIsInterface = false;
    bool dynamicDestruction = false;
    bool valueTypeNoOpDeletion = false;
    bool catchAll = false;
    bool interfaceCleanupDispatch = false;
    std::size_t argumentCount = 0;
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
};

struct ProjectOopMirStaticField final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    std::string name;
    std::string typeKey;
    bool isReadonly = false;
    bool isConst = false;
    bool hasConstantInitializer = false;
    std::uint64_t constantIntegerValue = 0;
    double constantFloatingValue = 0.0;
    bool constantBooleanValue = false;
    // Stage 51.x: an enum member's declared type is its own enum type (so it type-checks and
    // compares correctly against other enum-typed expressions), not its int-sized underlying
    // representation -- so `ProjectTypeConversion::isIntegral(typeKey)` returns false for it.
    // This flag lets the LLVM lowerer take the integral constant-materialization path anyway.
    bool isEnumConstant = false;
};


struct ProjectOopMirDispatchTable final
{
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::string typeKey;
    std::string descriptorName;
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
    std::vector<std::size_t> destructorMemberOrdinals;
    std::vector<std::string> destructorCalleeNames;
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
    std::size_t declaringTypeOrdinal = 0;
    bool constructor = false;
    bool destructor = false;
    bool isStatic = false;
    bool isExportedC = false;
    bool isImportedC = false;
    std::string importDllName;
    // C1.2 "import identity" -- see ProjectOopHirFunction for the full note.
    bool importRequiresRuntimeBinding = false;
    std::string cAbiName;
    bool isLambda = false;
    std::optional<std::size_t> lambdaOrdinal;
    bool hasClosureEnvironment = false;
    std::vector<std::string> captureTypeKeys;
    std::string genericSpecializationKey;
    std::string mangledName;
    std::string returnTypeKey;
    sema::OwnershipKind returnOwnership = sema::OwnershipKind::None;
    std::vector<std::string> parameterTypeKeys;
    std::vector<sema::OwnershipKind> parameterOwnerships;
    std::vector<std::size_t> localOrdinals;
    std::unique_ptr<ProjectOopMirNode> body;
};

class ProjectOopMirModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        const ProjectVirtualDispatchModel& virtualDispatchModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectDestructionModel& destructionModel,
        const ProjectOopHirModel& hirModel,
        std::string& error);

    std::span<const ProjectOopMirFunction> functions() const noexcept;
    std::span<const ProjectOopMirFieldInitializer> fieldInitializers() const noexcept;
    std::span<const ProjectOopMirStaticField> staticFields() const noexcept;
    std::span<const ProjectOopMirStructLayout> structLayouts() const noexcept;
    std::span<const ProjectOopMirDispatchTable> dispatchTables() const noexcept;
    const ProjectOopMirDispatchTable* dispatchTableForType(std::size_t typeOrdinal) const noexcept;
    const ProjectOopMirFunction* functionForMember(std::size_t memberOrdinal) const noexcept;
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
    std::vector<std::optional<std::size_t>> functionByMember_;
    std::vector<ProjectOopMirDispatchTable> dispatchTables_;
    std::vector<std::optional<std::size_t>> dispatchTableByType_;
    bool built_ = false;
};

const char* projectOopMirNodeKindName(ProjectOopMirNodeKind kind) noexcept;
}
