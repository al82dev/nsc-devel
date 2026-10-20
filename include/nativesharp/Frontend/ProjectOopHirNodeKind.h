#pragma once

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace nativesharp::frontend
{
enum class ProjectOopHirNodeKind
{
    Block,
    // NS304-T09-F05: a sequenced expression — a Block that yields a value. All but the last child
    // are statements (LocalDeclaration/ExpressionStatement); the LAST child is the result
    // expression, lowered by the expression path. Used by the collection builder.
    BlockExpression,
    LocalDeclaration,
    ExpressionStatement,
    // NativeSharp 2.0 item 298. The complete, already-validated integrated-assembly contract.
    // stringValue preserves the exact bytes between the braces; children remain in source order
    // and are InlineAssemblyLine/Input/Output/Clobber nodes. Operand children contain the lowered
    // NativeSharp value (inputs) or storage address (outputs and memory inputs).
    InlineAssembly,
    InlineAssemblyLine,
    InlineAssemblyInput,
    InlineAssemblyOutput,
    InlineAssemblyClobber,
    If,
    While,
    // Same operand order as While -- children[0] is the condition, children[1] the body --
    // and a distinct kind only because the backend enters the body before the first test.
    DoWhile,
    For,
    // `switch` / `case`. children[0] is the governing expression; children[1..] are the sections,
    // in source order. Each section is a SwitchSection node whose `switchLabels` holds the ALREADY
    // FOLDED constant values of its labels and whose `switchHasDefault` marks the default section.
    // The label expressions themselves are never lowered: the body model resolved them to integers
    // and checked them for duplicates, so by this point a label is a number, not an expression.
    Switch,
    // One section. children[0] is a Block with the section's statements. Reaching the end of that
    // block leaves the switch -- there is no fall-through to the next section.
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
    // Bloque 1: move(holder.slot) / move(slot) -- extract the owner out of an `owned` field,
    // nulling the vacated slot. Address-shaped exactly like MoveArrayElement; kept as its own
    // kind so IR dumps say what the source actually was.
    MoveOwnedField,
    // NS304-T07-F04: move(it.Current) transfers a move-only item out of the iterator's item slot,
    // clearing the vacated slot. Address-shaped: the child is the iterator's address (not the item).
    MoveIteratorCurrent,
    // NSC-LIMIT-021. The inner value is a fresh owner lent as a call argument. The node gives it
    // a hidden slot/cleanup identity; FullExpressionCleanup below closes that lifetime.
    OwnedTemporaryBorrow,
    FullExpressionCleanup,
    // Interop Foundation: `*pointer` as a VALUE -- load of the pointee type through a raw
    // pointer. (`&lvalue` and `*p` as an assignment target need no node of their own: they are
    // the operand's address / the pointer's value, respectively.)
    PointerLoad,
    // C2.1 "fixed arrays": the ADDRESS of one element of inline, unmanaged storage --
    // `fixedArrayField[i]` or `rawPointer[i]`. children = [base address, index]; elementTypeKey
    // is the element type, and the MIR pass attaches its size. A read wraps this in PointerLoad;
    // a write stores straight through it.
    RawElementAddress,
    // C6 "struct members": `new Vec2(1, 2)` on a struct. Distinct from ObjectConstruction, which
    // allocates: this one initializes storage IN PLACE and yields the value. No heap, no class
    // descriptor, nothing to free -- which is exactly what makes a struct a struct.
    ValueConstruction,
    // Item 318. children[0] is caller-supplied T* storage, followed by argumentCount constructor
    // arguments and then synthesized field-initializer work. No allocation is performed.
    PlacementConstruction,
    // NativeSharp 3.0 T01-F02. Materializes the expanded elements of a final
    // params ReadOnlySpan<T> into readonly stack storage and yields the ordinary span value.
    // children are converted T values in source order; an empty list yields {null, 0}.
    ParameterPack,
    // C7 "aggregate initializer": `V v = {1, 2};` and `int[4] cells = {1};`. typeKey is the
    // aggregate; children are AggregateElement nodes, ALREADY FLATTENED to absolute byte offsets,
    // so nesting (`Matrix m = { {1, 0}, {0, 1} };`) is resolved here rather than in the backend.
    // Elements the source did not mention are simply absent: the storage is zeroed first, which
    // is exactly the rule C gives a partial initializer.
    AggregateInitializer,
    // NativeSharp 3.0 T01-F05. children[0] is one same-type spread source evaluated once;
    // children[1..] are AggregateElement descriptors for the still-unwritten target fields.
    AggregateSpread,
    // C7: one element of the above. integerValue is its byte offset from the start of the
    // aggregate, typeKey its type, and its single child the value to store there.
    AggregateElement,
    // T02-F03. children[0] is the tuple source; remaining children are element descriptors.
    // The source is materialized before any destination address is evaluated, making swaps and
    // aliasing deterministic. A descriptor carries source offset/type and one target address.
    TupleDeconstruct,
    TupleDeconstructElement,
    // C6 "struct members": the ADDRESS of a struct value that has no home -- the result of an
    // operator, a call, a conversion or a `new`. Reaching a member of one (`(a + b).x`,
    // `Cross(u, v).z`) needs an address, and a temporary has none until one is made for it.
    TemporaryAddress,
    // `&lvalue` as a value: wraps the operand's ADDRESS node, carrying the pointer type
    // (T*) as this expression's type without mutating the operand.
    AddressValue,
    BorrowValue,
    ThisValue,
    ParameterValue,
    LocalValue,
    FieldValue,
    // T01-F03 `receiver?.field` for a nullable field carrier. The receiver child is lowered once;
    // memberOrdinal identifies the ordinary instance field selected by semantic analysis.
    NullConditionalFieldValue,
    // NS304-T10-F05: `receiver?.field = value` — store condicional al null-check del receptor.
    NullConditionalFieldStore,
    // NS304-T10-F05: `receiver?[index]` — load condicional del elemento de array nativo.
    NullConditionalArrayLoad,
    // NS304-T10-F05: `receiver?[index] = value` — store condicional del elemento de array nativo.
    NullConditionalArrayStore,
    StaticFieldValue,
    NamespaceStateValue,
    ParameterAddress,
    LocalAddress,
    FieldAddress,
    // NS304-T05-F03: address of one synthetic field of an opaque future. `memberOrdinal` is the
    // field index (0 = state, 1 = frame, 2 = result) and `elementTypeKey` the future type key;
    // the MIR resolves the byte offset against the future's ProjectOopMirStructLayout. The single
    // child is the future pointer.
    FutureFieldAddress,
    // NS304-T05-F03: `await operand` over a synchronously-completing future. The single child is
    // the future value; stringValue is the poll function's mangled name; elementTypeKey is the
    // future type key; typeKey is the awaited inner type. MIR stores the future, calls the poll,
    // and loads the result field.
    AwaitFuture,
    // NS304-T05-F09: a bare `yield;` suspension point. integerValue is the resume state index N
    // (suspend writes self->state = N and returns; the next poll resumes here). No children.
    SuspendPoint,
    // NS304-T05-F09: the address of the async poll function (F$poll) stored into the future's
    // pollFn field. stringValue is the poll's mangled name; typeKey is builtin:void*.
    PollAddress,
    // NS304-T06-F03: `yield return v` of a synchronous generator. integerValue is the yield state
    // index N; the single child is the item value. Lowered to { self->item = v; self->state = N;
    // return true; } by the generator MoveNext split.
    YieldValue,
    // NS304-T06-F03: `yield break` of a synchronous generator. Lowered to { self->state =
    // completed; return false; }. No children.
    YieldBreak,
    // NS304-T06-F08: `it.Current` — reads the iterator's item (field 2). One child: the iterator
    // value. `it.MoveNext()` — calls the iterator's moveNextFn (field 3) and returns its bool.
    // `it.GetEnumerator()` — returns the iterator value unchanged.
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
    // NS304-T10-F01: `u8"..."` — a typed UTF-8 literal (ReadOnlySpan<byte>); stringValue holds the
    // UTF-8 bytes, elementTypeKey the span type, integerValue the byte count.
    Utf8StringLiteral,
    ArrayAllocation,
    RangeArrayAllocation,
    // CC1 fase 4B -- `stackalloc T[n]`. Un hijo: el contador. `elementTypeKey` es T. El MIR le
    // pone el tamano y la alineacion; el backend emite un alloca de arreglo.
    StackAllocation,
    // CC1 fase 4C -- `new T` escalar. Un hijo NO tiene: es una reserva de un elemento. El MIR le
    // pone el tamano; el backend llama al allocador.
    RawAllocation,
    // CC1 fase 4D -- `p + i`, `i + p`, `p - i`, `p - q`. Dos hijos, en el orden en que se
    // escribieron. `operation` lleva el token (`+` o `-`), `elementTypeKey` el tipo apuntado.
    // Cuando los dos hijos son punteros es una DIFERENCIA y el tipo del nodo es `builtin:nint`;
    // en cualquier otro caso es un desplazamiento y el tipo es `T*`.
    PointerArithmetic,
    ArrayLength,
    ArrayElementValue,
    ArrayElementAddress,
    RangeArrayElementValue,
    RangeArrayElementAddress,
    // NativeSharp 3.0 T01-F04. children = [receiver, written-start-or-zero,
    // written-end-or-zero]. memberOrdinal is Slice(int,int), secondaryMemberOrdinal is
    // Length(). integerValue is a four-bit endpoint mask: has-start, start-from-end, has-end,
    // end-from-end; bit 4 records a compile-time bounds proof. The backend reuses one evaluated
    // receiver for both protocol calls.
    RangeSlice,
    StringLength,
    StringElementValue,
    // ID 129. `ToString()` de un enum: el NOMBRE del enumerador.
    //
    // FORMA: children[0] es el VALOR del enum, children[1] la cadena de reserva (el nombre del
    // tipo), y a partir de children[2] van PARES (constante entera, literal de cadena), uno por
    // enumerador, en orden de declaracion.
    //
    // POR QUE UN NODO Y NO UNA CADENA DE CONDICIONALES, que es como se hizo primero: aquella
    // bajaba el RECEPTOR UNA VEZ POR ENUMERADOR, asi que un receptor con efectos --`Dame()`--
    // los repetia. Se rodeaba exigiendo un receptor re-evaluable y rechazando el resto, que es
    // una limitacion del compilador disfrazada de regla del lenguaje. Con un nodo, el valor se
    // baja UNA VEZ y la eleccion es una cadena de `select` sin saltos.
    EnumName,
    StringConcat,
    StringEquals,
    StringSubstring,
    StringCharacterPointer,
    StringFromCharArray,
    // NS304-T03-F03: list-pattern rest capture over a native string synthesizes a
    // StringView (receiver + start + length children); LLVM emits { data, count }.
    StringViewFromString,
    // NS304-T03-F03: same view synthesis over a native/fixed array -> Span<T>/ReadOnlySpan<T>.
    // elementTypeKey carries T; LLVM emits { &receiver[start], count }.
    SpanFromArray,
    ReadOnlySpanFromArray,
    SpanFromFixedArray,
    ReadOnlySpanFromFixedArray,
    CoreConsoleWriteLine,
    CoreConsoleReadLine,
    // C8 -- see MethodCoreOperationKind::ObjectTypeName / ObjectReferenceHash. One operand each
    // (the object reference); result is builtin:string and builtin:int respectively.
    CoreObjectTypeName,
    CoreObjectReferenceHash,
    // C8 struct half -- see MethodCoreOperationKind::StructEquals / StructHashCode. Children are
    // the operands (two for Equals, one for GetHashCode, counted by argumentCount) followed by
    // AggregateElement nodes describing the struct's SCALAR LEAVES: one per primitive field,
    // nesting and fixed arrays already flattened, padding deliberately absent. Each leaf carries
    // its byte offset in integerValue and its type in typeKey, and has no child of its own.
    // (StructToString needs no node of its own: the answer is a compile-time constant, so it
    // lowers to an ordinary StringLiteral.)
    AggregateEquals,
    AggregateHash,
    // Stage 53.1 "Time" -- see MethodCoreOperationKind::ClockQuery* in ProjectMethodBodyModel.h.
    CoreClockQueryCounter,
    CoreClockQueryFrequency,
    CoreClockQuerySystemFileTime,
    // Stage 53.2 "Environment" -- see MethodCoreOperationKind::Environment* in
    // ProjectMethodBodyModel.h.
    CoreEnvironmentTickCount64,
    CoreEnvironmentProcessorCount,
    CoreEnvironmentExit,
    // Stage 53.2b "Environment (variables)" -- see MethodCoreOperationKind::EnvironmentGet/
    // SetVariable in ProjectMethodBodyModel.h. Lowered through the same operand-loop machinery
    // as StringConcat/StringEquals, not a bespoke shape.
    CoreEnvironmentGetVariable,
    CoreEnvironmentSetVariable,
    // Stage 53.3 "Filesystem/Paths" -- see MethodCoreOperationKind::FileExists/FileDelete/
    // DirectoryExists/DirectoryCreate in ProjectMethodBodyModel.h. Same shared operand-loop
    // machinery as the Environment (variables) pair above: one string operand in, a runtime
    // hook call out.
    CoreFileExists,
    CoreFileDelete,
    CoreDirectoryExists,
    CoreDirectoryCreate,
    // Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- see
    // MethodCoreOperationKind::FileReadAllText/FileWriteAllText/FileAppendAllText above.
    CoreFileReadAllText,
    CoreFileWriteAllText,
    CoreFileAppendAllText,
    // Bloque 4, fase A -- ver MethodCoreOperationKind::FileGetSize. Un operando (la ruta), un
    // entero de 64 bits fuera: la misma forma de un operando que CoreFileExists, con otro tipo
    // de salida.
    CoreFileGetSize,
    // Bloque 4, fase A -- ver MethodCoreOperationKind::FileReadAllBytes y siguientes.
    CoreFileReadAllBytes,
    CoreFileReadBytes,
    CoreFileWriteAllBytes,
    CoreEnvironmentCurrentDirectory,
    // Bloque 4, fase C -- ver MethodCoreOperationKind::ProcessRun.
    CoreProcessRun,
    // Stage 53.4 "Filesystem/Paths (cierre)" -- see MethodCoreOperationKind::DirectoryDelete/
    // PathGetFullPath in ProjectMethodBodyModel.h. Same shared operand-loop machinery again.
    CoreDirectoryDelete,
    CorePathGetFullPath,
    // Stage 53.5 "Command-line arguments" -- see
    // MethodCoreOperationKind::EnvironmentGetCommandLineArgCount/EnvironmentGetCommandLineArgs
    // in ProjectMethodBodyModel.h. GetCommandLineArgCount joins the zero-argument bucket
    // (CoreEnvironmentTickCount64/ProcessorCount); GetCommandLineArgs joins the shared
    // operand-loop machinery, its one operand being a builtin:string[] instead of a string.
    CoreEnvironmentGetCommandLineArgCount,
    CoreEnvironmentGetCommandLineArgs,
    // Stage 53.6 "Enumeracion de directorios" -- see MethodCoreOperationKind::
    // DirectoryGetFileCount/GetFiles/GetDirectoryCount/GetDirectories in
    // ProjectMethodBodyModel.h. GetFileCount/GetDirectoryCount are one-operand (a
    // builtin:string path, builtin:int result); GetFiles/GetDirectories are two-operand (path +
    // a builtin:string[] buffer -- the same caller-provided-buffer pattern
    // CoreEnvironmentGetCommandLineArgs already established). All four join the exact same
    // shared operand-loop machinery as every other core operation.
    CoreDirectoryGetFileCount,
    CoreDirectoryGetFiles,
    CoreDirectoryGetDirectoryCount,
    CoreDirectoryGetDirectories,
    // Stage 54.1 "Threads" -- see MethodCoreOperationKind::ThreadCreate/ThreadJoin/ThreadDetach
    // in ProjectMethodBodyModel.h. All three are one-operand: ThreadCreate's single operand is a
    // ThreadStart delegate value (a genuinely new operand TYPE for this shared machinery, but not
    // a new SHAPE -- still exactly one operand, lowered the same way via lowerExpression);
    // ThreadJoin/ThreadDetach's single operand is a builtin:nint (the saved HANDLE). All three
    // join the exact same shared operand-loop machinery as every other core operation.
    CoreThreadCreate,
    CoreThreadJoin,
    CoreThreadDetach,
    // Stage 54.2 "Atomics" -- see MethodCoreOperationKind::InterlockedIncrementInt32/.../
    // InterlockedCompareExchangeInt64 in ProjectMethodBodyModel.h. NOT part of the shared
    // operand-loop bucket above (unlike CoreThreadCreate/Join/Detach) -- these ten have a real
    // receiver (the AtomicInt32/AtomicInt64 instance) and their first logical operand (the
    // atomic location) is never written by the caller, only synthesized by the compiler as a
    // FieldAddress node over the receiver's own `value_` field. See the dedicated block in
    // ProjectOopHirModel.cpp (right before the ConsoleWriteLine block) for the full mechanism.
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
    // Stage 54.3 "Mutex" -- see MethodCoreOperationKind::MutexCreate/MutexLock/MutexUnlock/
    // MutexClose in ProjectMethodBodyModel.h. Exact same shape as CoreThreadCreate/Join/Detach
    // above (a builtin:nint HANDLE round-tripped between separate NativeSharp calls), so these
    // four rejoin the shared operand-loop bucket directly -- MutexCreate is zero-operand (the
    // first core operation in the project with none at all), MutexLock/Unlock/Close are
    // one-operand (the saved HANDLE), same as ThreadJoin/ThreadDetach.
    CoreMutexCreate,
    CoreMutexCreateNamed,
    CoreMutexLock,
    CoreMutexUnlock,
    CoreMutexClose,
    // Stage 54.4 "Condition Variables" -- NativeSharp.Core.Threading.Monitor. Enter/Exit/Pulse/
    // PulseAll rejoin the SAME "one FieldAddress child, one runtime hook" shape
    // CoreInterlockedIncrementInt32 already established (Stage 54.2) -- see the dedicated Monitor
    // HIR block in ProjectOopHirModel.cpp for how each resolves its own single implicit field.
    // CoreMonitorWait is the ONE genuinely new shape: its sole child is the receiver expression
    // lowered ONCE (never a FieldAddress node), with memberOrdinal/secondaryMemberOrdinal on the
    // node itself carrying the cv_/lock_ field ordinals for MIR/LLVM to resolve into two real
    // pointers from that single lowered receiver -- see ProjectOopHirNode::secondaryMemberOrdinal
    // below for the full rationale.
    CoreMonitorEnter,
    CoreMonitorExit,
    CoreMonitorWait,
    CoreMonitorPulse,
    CoreMonitorPulseAll,
    // Stage 54.5 "Otras primitivas de sincronizacion" -- NativeSharp.Core.Threading.Semaphore y
    // NativeSharp.Core.Threading.Event. Mismo shape "sin receptor, operandos por la lista de
    // argumentos AST" que Mutex/Thread arriba -- cero mecanismo de FieldAddress.
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
    // Stage 55.1/55.2/55.3 -- Process/SystemInfo/Debug. All zero-argument (result carried purely
    // by `kind` + `typeKey`, no children) except CoreProcessSetPriorityClass and
    // CoreDebugOutputString, which each take exactly one child (the argument expression) --
    // same shape CoreEnvironmentExit/CoreFileDelete already established.
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
    ClosureCaptureValue,
    ClosureCaptureAddress,
    CaptureCellReference,
    ClosureCaptureCellReference,
    DelegateInvoke,
    EventAdd,
    EventRemove,
    EventInvoke,
    Unary,
    Binary,
    // `c ? a : b`. children[0] condition, children[1] true branch, children[2] false branch.
    // Both branches are lazily evaluated, so this lowers to control flow, not a select.
    Conditional,
    // NativeSharp 3.0 T01-F03. children[0] is evaluated once. children[1] is evaluated only
    // when that pointer/reference value is null; the result is the non-null left value or RHS.
    NullCoalescing,
    // E017 `Optional<T> ?? fallback`. The carrier is evaluated once; memberOrdinal names
    // HasValue(), secondaryMemberOrdinal names UncheckedValue(), and the fallback stays lazy.
    OptionalCoalescing,
    // T01-F03 `place ??= value`. children[0] is the destination ADDRESS and children[1] the
    // converted RHS. The backend loads/tests the address once and stores only on the null edge.
    NullCoalescingAssignment,
    Assignment,
    // IDs 79 y 81 -- `x op= e`, `++x`, `x++`, `--x`, `x--`.
    //
    // POR QUE UN NODO PROPIO Y NO UN DESAZUCARADO. La forma obvia seria reescribir `x += e` como
    // `x = x + e` aqui mismo. Es incorrecta: el destino se evaluaria DOS VECES, asi que
    // `a[f()] += 1` llamaria a `f` dos veces y `*p++ += 1` avanzaria dos veces. C# garantiza que
    // el destino se evalua una sola vez, y ese es justo el motivo por el que la tarea 62 decia
    // que esto "requiere semantica de actualizacion de lvalue".
    //
    // FORMA. children[0] es la DIRECCION del destino, bajada una unica vez en contexto de
    // direccion -- exactamente lo mismo que Assignment ya calculaba para su propio destino.
    // children[1] es el operando derecho, ya convertido al tipo del destino (o un entero de paso
    // cuando el destino es un puntero). `operation` lleva el operador BASE (`+` para `+=` y para
    // `++`, `<<` para `<<=`, etc.), nunca el token compuesto: quien emite la aritmetica es el
    // mismo codigo que emite un `Binary` corriente, y por eso no hay una segunda tabla de reglas
    // de division con signo, de desplazamiento o de coma flotante.
    //
    // `booleanValue` = true significa QUE EL NODO VALE EL VALOR ANTIGUO. Es la unica diferencia
    // entre `x++` y `++x`, y es una diferencia de un solo bit precisamente porque todo lo demas
    // -- la direccion, la carga, la operacion, el almacenamiento -- es identico.
    //
    // `elementTypeKey`, cuando no esta vacio, dice que el destino es un puntero y cual es el tipo
    // apuntado. El paso lo resuelve el MIR con el mismo `elementLayout` que ya usan
    // PointerArithmetic, el stride de un arreglo y `sizeof` -- una sola regla de tamano.
    CompoundStore,
    Conversion,
    RuntimeTypeTest,
    SafeReferenceCast,
    CheckedReferenceCast,
    InstanceCall,
    StaticCall,
    ConstructorInitializerCall,
    ObjectConstruction,
    Deallocation,
    // Item 319. One operand; dynamicDestruction selects the class descriptor's object-only
    // entry. A non-dynamic node is a typed no-op for a destructible-free struct lifetime.
    ObjectDestruction,
    // T07-F06 assertion family. Assert/Assume have one bool child and builtin:void type.
    // Unreachable/Panic have no children, builtin:never, and no normal successor.
    Assert,
    Unreachable,
    Assume,
    Panic,
    // T07-F01. children[0] is the Result carrier evaluated exactly once. The node freezes both
    // variant protocols and the cleanup path for its conditional Error return.
    ResultPropagation,
    // T07-F05. children[0] is the loop body. typeKey is the unified break-value type (or never),
    // stringValue is the optional label, and breaks carry their converted value as child[0].
    LoopExpression,
    // T03-F03. children[0] is the governing value evaluated exactly once; remaining children are
    // ordered MatchArm nodes. A MatchValue inside an arm reads that frozen value. MatchArm keeps
    // [condition, guard, bindings..., result] and owns cleanupRegionOrdinals for its local scope.
    MatchExpression,
    MatchArm,
    MatchValue,
    // NS304-T15-F05. `checked(expr)`/`saturating(expr)`. One operand; stringValue is the policy
    // ("checked"/"saturating"). The backend applies the policy to the decimal operations inside.
    OverflowPolicy
};
}
