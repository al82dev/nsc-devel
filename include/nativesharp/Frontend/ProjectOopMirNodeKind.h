#pragma once

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
enum class ProjectOopMirNodeKind
{
    Block,
    // NS304-T09-F05: a sequenced expression (Block that yields a value). Same shape as Block, but
    // the LAST child is the value expression.
    BlockExpression,
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
    MoveIteratorCurrent,
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
    // NS304-T10-F05: store condicional `receiver?.field = value` (receiver + value).
    NullConditionalFieldStore,
    // NS304-T10-F05: load condicional `receiver?[index]` (receiver + index).
    NullConditionalArrayLoad,
    // NS304-T10-F05: store condicional `receiver?[index] = value` (receiver + index + value).
    NullConditionalArrayStore,
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
    // NS304-T05-F03: address of one synthetic field of an opaque future. `immediate` holds the
    // resolved byte offset; the single child is the future pointer.
    FutureFieldAddress,
    // NS304-T05-F03: `await operand` over a synchronously-completing future. The single child is
    // the future value; `stringValue` is the poll mangled name; `elementTypeKey` the future key.
    AwaitFuture,
    // NS304-T05-F09: a bare `yield;` suspension point. `integerValue` is the resume state index.
    SuspendPoint,
    // NS304-T05-F09: address of the async poll function (F$poll). `stringValue` is the poll name.
    PollAddress,
    // NS304-T06-F03: `yield return v` of a synchronous generator. `integerValue` is the yield state.
    YieldValue,
    // NS304-T06-F03: `yield break` of a synchronous generator. No children.
    YieldBreak,
    // NS304-T06-F04: the generator constructor must allocate the frame and store the captured
    // parameters into the iterator's `self` local (lowered by the LLVM using asyncFrameSize +
    // asyncFrameParameters). `localOrdinal` is the iterator `self` local.
    GeneratorFrameInit,
    // NS304-T06-F08: iterator protocol nodes (Current/MoveNext/GetEnumerator). One child each.
    IteratorCurrent,
    IteratorMoveNext,
    IteratorGetEnumerator,
    // NS304-T06-F07: `it.Drop()` — calls the iterator's dropFn (field 4). One child: the address.
    IteratorDrop,
    // NS304-T07-F02: `it.MoveNextAsync()` — returns the future of the next demand. One child: address.
    IteratorMoveNextAsync,
    // NS304-T07-F05: `it.Cancel()` — sets the async-iterator's cancellation flag. One child: address.
    IteratorCancel,
    // NS304-T07-F02: the async-generator poll's `yield return v`. One child: the item value.
    AsyncYieldValue,
    // NS304-T06-F07: `Gen$drop` destroys the frame's live locals. One child: the iterator address.
    DropFrame,
    StaticFieldAddress,
    NamespaceStateAddress,
    IntegerConstant,
    FloatingConstant,
    BooleanConstant,
    NullConstant,
    DefaultValue,
    StringLiteral,
    // NS304-T10-F01: `u8"..."` — ReadOnlySpan<byte> over the literal's UTF-8 bytes (static bytes +
    // count, no heap). stringValue = bytes, integerValue = byte count.
    Utf8StringLiteral,
    ArrayAllocate,
    RangeArrayAllocate,
    ArrayLength,
    ArrayElementLoad,
    ArrayElementAddress,
    RangeArrayElementLoad,
    RangeArrayElementAddress,
    // T01-F04 structural Length/Slice protocol; see the HIR node for child/flag layout.
    RangeSlice,
    StringLength,
    StringElementLoad,
    // ID 129. Ver ProjectOopHirNodeKind::EnumName para la forma y el porque.
    EnumName,
    StringConcat,
    StringEquals,
    StringSubstring,
    StringCharacterPointer,
    StringFromCharArray,
    StringViewFromString,
    SpanFromArray,
    ReadOnlySpanFromArray,
    SpanFromFixedArray,
    ReadOnlySpanFromFixedArray,
    CoreConsoleWriteLine,
    CoreConsoleReadLine,
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
    OptionalCoalescing,
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
    // T07-F06 assertion family. Assert/Assume carry one bool child; Unreachable/Panic diverge.
    Assert,
    Unreachable,
    Assume,
    Panic,
    // T07-F01. One evaluated carrier, an explicit tag branch, and a structured Error return.
    ResultPropagation,
    // T07-F05. Value-producing infinite loop; see the corresponding HIR node contract.
    LoopExpression,
    // T03-F03. Explicit expression match; see the corresponding HIR nodes for child shape.
    MatchExpression,
    MatchArm,
    MatchValue,
    // NS304-T15-F05. `checked(expr)`/`saturating(expr)`; one operand and the policy string.
    OverflowPolicy
};
}
