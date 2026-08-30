#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <optional>
#include <set>
#include <utility>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
enum class MethodScopeKind
{
    Method,
    Constructor,
    Destructor,
    FieldInitializer,
    Block,
    For,
    Foreach,
    Catch,
    Using
};

struct MethodScope final
{
    MethodScopeKind kind = MethodScopeKind::Method;
    std::size_t methodMemberOrdinal = 0;
    std::optional<std::size_t> parentScopeOrdinal;
    std::size_t depth = 0;
    std::vector<std::size_t> localOrdinals;
};

struct MethodLocalSymbol final
{
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::size_t declarationOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string name;
    std::string typeKey;
    bool isConst = false;
    bool isRaii = false;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    std::optional<std::size_t> localBorrowBindingOrdinal;
    std::optional<std::size_t> lambdaOrdinal;
    std::optional<std::size_t> lambdaParameterOrdinal;
};

enum class MethodNameBindingKind
{
    Parameter,
    Local,
    ImplicitField,
    ImplicitMethodGroup,
    ThisReference,
    BaseReference,
    ExplicitThisField,
    ExplicitThisMethodGroup,
    ExplicitObjectField,
    ExplicitObjectMethodGroup,
    ExplicitBaseField,
    ExplicitBaseMethodGroup,
    StaticField,
    StaticMethodGroup,
    ImplicitEvent,
    ExplicitThisEvent,
    ExplicitObjectEvent,
    StaticEvent,
    TypeReference
};

struct MethodNameBinding final
{
    MethodNameBindingKind kind = MethodNameBindingKind::Local;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string name;
    std::string typeKey;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    std::vector<std::size_t> candidateMemberOrdinals;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    bool baseReceiver = false;
};

enum class MethodShadowingKind
{
    ParameterShadowsMember,
    LocalShadowsMember,
    LocalShadowsParameter,
    LocalShadowsLocal
};

struct MethodShadowingEvent final
{
    MethodShadowingKind kind = MethodShadowingKind::LocalShadowsLocal;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string name;
    SourceRange range;
};

struct MethodExpressionTypeBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string typeKey;
    std::optional<ProjectIntegerConstant> integerConstant;
};

enum class MethodRuntimeTypeOperationKind
{
    TypeTest,
    SafeCast,
    CheckedCast
};

struct MethodRuntimeTypeBinding final
{
    MethodRuntimeTypeOperationKind kind = MethodRuntimeTypeOperationKind::TypeTest;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string sourceTypeKey;
    std::string targetTypeKey;
    std::size_t targetTypeOrdinal = 0;
    bool targetIsInterface = false;
};


enum class MethodExceptionOperationKind
{
    Catch,
    Throw,
    Rethrow
};

struct MethodExceptionBinding final
{
    MethodExceptionOperationKind kind = MethodExceptionOperationKind::Throw;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string exceptionTypeKey;
    std::optional<std::size_t> exceptionTypeOrdinal;
    bool catchAll = false;
};

enum class MethodCleanupRegionKind
{
    Finally,
    UsingDispose,
    RaiiDestroy,
    OwnedParameterDestroy
};

struct MethodCleanupRegion final
{
    MethodCleanupRegionKind kind = MethodCleanupRegionKind::Finally;
    const ast::AstNode* syntax = nullptr;
    const ast::AstNode* cleanupSyntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t ownerScopeOrdinal = 0;
    std::optional<std::size_t> parentCleanupRegionOrdinal;
    std::optional<std::size_t> resourceLocalOrdinal;
    std::optional<std::size_t> resourceParameterOrdinal;
    std::optional<std::size_t> disposeContractMemberOrdinal;
    std::optional<std::size_t> disposeImplementationMemberOrdinal;
    std::size_t depth = 0;
};



struct MethodMoveBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
    std::string typeKey;
    // Stage 52: move(...) also accepts a native-array element (e.g. "move(items[i])"),
    // needed by generic collections (Queue<T>.Dequeue(), Stack<T>.Pop(), ...) to extract an
    // owned element out of backing storage. When true, sourceLocalOrdinal/sourceParameterOrdinal
    // are both unset -- HIR lowering re-derives the array/index sub-expressions directly from
    // `syntax->first()` (the IndexExpression being moved).
    //
    // `elementIsReferenceLike` records whether the SOURCE actually needs the load+null-store
    // treatment (a class/interface reference or native string) or is a plain value type, where
    // the move degrades to a harmless plain read -- for EVERY move() source shape, not just an
    // array element: a named RAII local or owned parameter closed over a generic T can equally
    // monomorphize to a value type (int, bool, a struct, ...), and MoveValue's LLVM lowering
    // (ProjectOopLlvmLowerer) needs this to know the storage slot's real width/type instead of
    // always treating it as a pointer -- see that case's comment for the bug this fixed
    // (LinkedList<int>.AddFirst's "move(value)" on an owned int parameter loading it as a
    // mismatched `ptr` and failing LLVM verification).
    bool sourceIsArrayElement = false;
    // Bloque 1: move(...) also accepts a direct `owned` field access -- `move(holder.slot)`, or
    // `move(slot)` through the implicit this. Same contract as the array-element case: both
    // source ordinals stay unset, HIR lowers the field access as an address and pairs the load
    // with a null-store into the vacated slot, so the container's own discharge (its destruction
    // entry, or a later overwrite's release) sees an empty slot instead of double-freeing the
    // value that was just moved out.
    bool sourceIsOwnedField = false;
    bool elementIsReferenceLike = false;
};

enum class MethodOwnershipTransferKind
{
    MoveLocalToOwnedParameter,
    MoveParameterToOwnedParameter,
    OwnedReturnToOwnedParameter,
    MoveLocalToOwnedReturn,
    MoveParameterToOwnedReturn,
    OwnedReturnForward
};

struct MethodOwnershipTransferBinding final
{
    MethodOwnershipTransferKind kind = MethodOwnershipTransferKind::MoveLocalToOwnedParameter;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
    std::optional<std::size_t> targetMemberOrdinal;
    std::optional<std::size_t> targetParameterOrdinal;
    std::string typeKey;
};

enum class MethodBorrowKind
{
    OwnerLocalToBorrowedParameter,
    OwnedParameterToBorrowedParameter,
    BorrowedParameterToBorrowedParameter,
    BorrowedLocalToBorrowedParameter,
    BorrowedReturnToBorrowedParameter,
    BorrowedParameterToBorrowedReturn,
    BorrowedLocalToBorrowedReturn,
    BorrowedReturnForward
};

struct MethodBorrowBinding final
{
    MethodBorrowKind kind = MethodBorrowKind::OwnerLocalToBorrowedParameter;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
    std::optional<std::size_t> targetMemberOrdinal;
    std::optional<std::size_t> targetParameterOrdinal;
    std::string typeKey;
};

enum class MethodLocalBorrowSourceKind
{
    OwnerLocal,
    OwnedParameter,
    BorrowedParameter,
    BorrowedLocal
};

enum class MethodBorrowState
{
    Valid,
    Invalidated,
    MaybeInvalid
};

struct MethodLocalBorrowBinding final
{
    MethodLocalBorrowSourceKind sourceKind = MethodLocalBorrowSourceKind::OwnerLocal;
    MethodBorrowState state = MethodBorrowState::Valid;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t localOrdinal = 0;
    std::size_t creationScopeOrdinal = 0;
    std::size_t maximumValidScopeOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
    std::optional<std::size_t> sourceOwnerLocalOrdinal;
    std::optional<std::size_t> sourceOwnerParameterOrdinal;
    std::string typeKey;
};

struct MethodUsingBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::size_t resourceLocalOrdinal = 0;
    std::string resourceTypeKey;
    std::size_t resourceTypeOrdinal = 0;
    std::size_t disposableTypeOrdinal = 0;
    std::size_t disposeContractMemberOrdinal = 0;
    std::optional<std::size_t> disposeImplementationMemberOrdinal;
    bool interfaceDispatch = false;
};

enum class MethodStructuredExitKind
{
    Return,
    Break,
    Continue
};

struct MethodStructuredExitBinding final
{
    MethodStructuredExitKind kind = MethodStructuredExitKind::Return;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t sourceScopeOrdinal = 0;
    std::optional<std::size_t> targetScopeOrdinal;
    std::vector<std::size_t> cleanupRegionOrdinals;
};

enum class MethodCleanupContinuationKind
{
    Normal,
    Return,
    Break,
    Continue,
    Exception
};

struct MethodCleanupContinuation final
{
    MethodCleanupContinuationKind kind = MethodCleanupContinuationKind::Normal;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t sourceScopeOrdinal = 0;
    std::size_t cleanupRegionOrdinal = 0;
    std::optional<std::size_t> nextCleanupRegionOrdinal;
    std::optional<std::size_t> targetScopeOrdinal;
};

enum class MethodCoreOperationKind
{
    ArrayAllocation,
    ArrayLength,
    ArrayElement,
    // C2.1 "fixed arrays": indexing something whose storage is INLINE and unmanaged -- a fixed
    // array field/local (`float[4]`) or a raw pointer (`int*`). One operation kind covers both
    // because the address arithmetic is identical (base + index * elementSize) and neither has a
    // length header to bounds-check against. This is the raw tier, exactly like `*p` and `&x`: no
    // bounds check, no null check, C semantics, by design -- it is the shape C structs and C APIs
    // are written in. `ArrayElement` above stays what it always was: the MANAGED, heap-allocated,
    // length-prefixed, bounds-checked native array.
    RawElement,
    StringLiteral,
    StringLength,
    StringElement,
    StringConcat,
    StringEquals,
    StringSubstring,
    StringFromCharArray,
    ConsoleWriteLine,
    // Stage 53.1 "Time" -- NativeSharp.Core.Diagnostics.NativeClock's three zero-argument raw
    // Win32 wrappers. See NativeClock.ns for why these three and not a higher-level API
    // directly: everything else in Stage 53.1 (Stopwatch/TimeSpan/DateTime) is ordinary
    // NativeSharp built on top of just these three core operations, matching the same
    // discipline Stage 51 established for String (a small core, a large surface built on it).
    ClockQueryCounter,
    ClockQueryFrequency,
    ClockQuerySystemFileTime,
    // Stage 53.2 "Environment" -- NativeSharp.Core.Environment's three raw Win32 wrappers.
    // TickCount64/ProcessorCount are zero-argument, exactly the same shape as the NativeClock
    // trio above (only the result type differs: long for TickCount64, int for ProcessorCount).
    // Exit takes one builtin:int argument and returns void, the same one-argument/void shape as
    // ConsoleWriteLine. GetEnvironmentVariable/SetEnvironmentVariable (dynamic string return)
    // are deliberately NOT part of this round -- see Environment.ns for why.
    EnvironmentTickCount64,
    EnvironmentProcessorCount,
    EnvironmentExit,
    // Stage 53.2b "Environment (variables)" -- the piece deliberately deferred out of Stage
    // 53.2's first round (see Environment.ns): GetEnvironmentVariable takes one builtin:string
    // and returns builtin:string, SetEnvironmentVariable takes two builtin:string and returns
    // void. Both reuse the exact same operand-lowering machinery as StringConcat/StringEquals
    // (see ProjectOopHirModel.cpp) -- from the frontend's point of view these are just string
    // intrinsics with a different runtime hook, not a new kind of call shape.
    EnvironmentGetVariable,
    EnvironmentSetVariable,
    // Stage 53.3 "Filesystem/Paths" -- NativeSharp.Core.IO.File.Exists/Delete and
    // NativeSharp.Core.IO.Directory.Exists/CreateDirectory. All four take exactly one
    // builtin:string argument (a path); Exists returns builtin:bool, Delete/CreateDirectory
    // return builtin:void. Like GetEnvironmentVariable/SetEnvironmentVariable above, these are
    // string intrinsics as far as the HIR shared block is concerned -- one string operand in,
    // a runtime hook call out -- so they reuse that exact mechanism (see ProjectOopHirModel.cpp)
    // rather than introducing a new call shape. NativeSharp.Core.IO.Path is deliberately NOT
    // part of this list: it is pure NativeSharp built on already-existing String methods
    // (Substring/Concat/EndsWith/LastIndexOf), zero new core operations needed for it.
    FileExists,
    FileDelete,
    DirectoryExists,
    DirectoryCreate,
    // Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- the piece 53.3's first round
    // deliberately deferred (see File.ns): reading/writing a file's actual byte content, not
    // just asking whether it exists. ReadAllText takes one builtin:string (a path) and returns
    // builtin:string -- same one-operand shape as FileExists/GetEnvironmentVariable above.
    // WriteAllText/AppendAllText take two builtin:string (path, content) and return
    // builtin:void -- same two-operand shape as EnvironmentSetVariable. All three still slot
    // into the shared string-intrinsic HIR block; see ProjectOopHirModel.cpp.
    FileReadAllText,
    FileWriteAllText,
    FileAppendAllText,
    // Stage 53.4 "Filesystem/Paths (cierre)" -- las dos piezas "casi gratis" identificadas al
    // cerrar 53.3b. DirectoryDelete: un builtin:string (path), builtin:void -- calco exacto de
    // FileDelete de arriba, mismo one-operand/void shape, solo cambia el hook de runtime
    // (RemoveDirectoryA en vez de DeleteFileA). PathGetFullPath: un builtin:string (path),
    // builtin:string -- mismo one-operand/string-out shape que EnvironmentGetVariable/
    // FileReadAllText (dos llamadas Win32: pedir tamaño, despues llenar el buffer). A diferencia
    // de FileExists/FileDelete/DirectoryExists/DirectoryCreate/FileReadAllText/WriteAllText/
    // AppendAllText (todos NativeSharp.Core.IO.File o NativeSharp.Core.IO.Directory),
    // PathGetFullPath es la PRIMERA operacion nucleo de NativeSharp.Core.IO.Path -- ver Path.ns
    // para por que el resto de esa clase se mantiene NativeSharp puro.
    DirectoryDelete,
    PathGetFullPath,
    // Stage 53.5 "Command-line arguments". GetCommandLineArgCount: zero arguments,
    // builtin:int -- same zero-argument shape as EnvironmentTickCount64/ProcessorCount above.
    // GetCommandLineArgs: one builtin:string[] argument (a caller-provided buffer -- the SAME
    // "hueco de owned string[]" workaround NativeSharp.Core.String.Split already established,
    // see String.ns), builtin:int (how many elements were actually written) -- a genuinely new
    // operand SHAPE for a core operation (an array, not a string), but it slots into the exact
    // same shared operand-lowering block as every other core operation since that block never
    // actually required its operand to be builtin:string, only lowered whatever expression was
    // there (see ProjectOopHirModel.cpp).
    EnvironmentGetCommandLineArgCount,
    EnvironmentGetCommandLineArgs,
    // Stage 53.6 "Enumeracion de directorios" -- el ultimo item pendiente de Stage 53. Ver el
    // comentario de cabecera de Directory.ns para el diseno completo (arreglo de dos pasadas,
    // NO un enumerador via foreach). GetFileCount/GetDirectoryCount: un builtin:string (path),
    // builtin:int -- una combinacion de forma nueva (un operando string, resultado int en vez de
    // string/void como PathGetFullPath/DirectoryDelete), pero el operando-count sigue siendo 1,
    // asi que reutiliza el mismo bucket "expected=1U" compartido, solo con su propio
    // resultTypeKey explicito. GetFiles/GetDirectories: dos operandos (builtin:string el path,
    // builtin:string[] el buffer -- la MISMA "buffer provisto por el caller" que
    // EnvironmentGetCommandLineArgs establecio), builtin:int -- el bucket "expected=2U" ya
    // existe (EnvironmentSetVariable/FileWriteAllText/FileAppendAllText), y al igual que con
    // EnvironmentGetCommandLineArgs, ese bloque nunca exigio que TODOS los operandos fueran
    // builtin:string, asi que un segundo operando builtin:string[] entra sin cambiar el
    // mecanismo compartido.
    DirectoryGetFileCount,
    DirectoryGetFiles,
    DirectoryGetDirectoryCount,
    DirectoryGetDirectories,
    // Stage 54.1 "Threads" -- NativeSharp.Core.Threading.Thread's tres helpers PRIVADOS Y
    // ESTATICOS (no publicos: el codigo NativeSharp normal llama al constructor/Join()/Detach()
    // de Thread, que a su vez llaman a estos tres). Ver Thread.ns para el diseno completo (clase
    // RAII, campo nint handle_, destructor que hace auto-Join()). ThreadCreate: un operando de
    // tipo delegate (ThreadStart) entra, builtin:nint sale -- una forma de operando GENUINAMENTE
    // NUEVA (ningun core operation anterior tomo un delegate como argumento), pero el bloque HIR
    // compartido nunca exigio un tipo de operando particular, asi que entra en el bucket
    // "expected=1U" existente sin cambios. ThreadJoin/ThreadDetach: un builtin:nint (el HANDLE
    // guardado por el llamador) entra, builtin:void sale -- mismo one-operand/void shape que
    // DirectoryDelete, solo que el operando es nint en vez de string.
    ThreadCreate,
    ThreadJoin,
    ThreadDetach,
    // Stage 54.2 "Atomics" -- NativeSharp.Core.Threading.AtomicInt32/AtomicInt64, cinco
    // operaciones (Increment/Decrement/Add/Exchange/CompareExchange) cada una en dos anchos
    // (Int32/Int64) -- diez core operations en total. NOTA: el diseno ORIGINAL de esta ronda era
    // una clase estatica "Interlocked" con un parametro `int*`/`long*` calcado de
    // System.Threading.Interlocked de C# (`Interlocked.Increment(&contador)`) -- se descarto
    // DURANTE la implementacion al confirmar que la tuberia OOP moderna de este compilador
    // (todas las stages desde 41) no baja los operadores `&`/`*` en absoluto (ver
    // ProjectOopHirModel.cpp's UnaryExpression case). El diseno FINAL, el que estos diez valores
    // de enum representan, son metodos de INSTANCIA (`contador.Increment()`) sobre
    // AtomicInt32/AtomicInt64, operando implicitamente sobre el campo privado `value_` del
    // receptor -- ver el comentario de MethodCoreBinding::implicitFieldMemberOrdinal mas abajo
    // para el mecanismo completo (un nodo ProjectOopHirNodeKind::FieldAddress sintetico), y
    // 58_stage54_2_atomics.md (raiz del repo) para la discusion de diseno completa de ambos
    // pivots de esta ronda. Formas de operando EXPLICITO (sin contar la ubicacion implicita, que
    // nunca es un operando sintactico de la llamada): Increment/Decrement toman CERO operandos
    // explicitos; Add/Exchange toman UNO (el valor); CompareExchange toma DOS (valor +
    // comparand) -- ver el bloque HIR dedicado en ProjectOopHirModel.cpp (deliberadamente
    // SEPARADO del bucket compartido "String core operation" que Thread/File/Directory/etc.
    // usan, porque estas diez necesitan armar el nodo FieldAddress a partir del receptor, algo
    // que ese bucket compartido no hace). Semantica de retorno calcada de
    // System.Threading.Interlocked real: Increment/Decrement/Add devuelven el valor NUEVO
    // (post-operacion); Exchange/CompareExchange devuelven el valor ORIGINAL (pre-operacion) --
    // ver Atomics.ns y WindowsRuntimeBuilder.cpp para el detalle completo (instrucciones
    // atomicas reales de LLVM, no llamadas a kernel32.dll -- el SEGUNDO pivot de esta ronda).
    InterlockedIncrementInt32,
    InterlockedDecrementInt32,
    InterlockedAddInt32,
    InterlockedExchangeInt32,
    InterlockedCompareExchangeInt32,
    InterlockedIncrementInt64,
    InterlockedDecrementInt64,
    InterlockedAddInt64,
    InterlockedExchangeInt64,
    InterlockedCompareExchangeInt64,
    // Stage 54.3 "Mutex" -- NativeSharp.Core.Threading.Mutex's cuatro helpers PRIVADOS Y
    // ESTATICOS, EXACTO mismo patron estructural que los tres helpers de Thread arriba (Stage
    // 54.1): un HANDLE de Win32 guardado como builtin:nint, creado por MutexCreate y consumido
    // por Lock/Unlock/Close via llamadas POSTERIORES y separadas. A diferencia de las diez
    // operaciones de AtomicInt32/AtomicInt64 (Stage 54.2, arriba), Mutex NO necesito ningun
    // mecanismo de FieldAddress sintetico -- reusa el mismo patron "handle nint de ida y vuelta"
    // que Thread ya establecio, cero riesgo arquitectonico nuevo. MutexCreate: CERO operandos
    // explicitos entra, builtin:nint sale -- la PRIMERA core operation de todo el proyecto con
    // cero operandos y sin receptor (ver el bucket "expected=0U" nuevo en
    // ProjectOopHirModel.cpp). MutexLock/MutexUnlock/MutexClose: un builtin:nint (el HANDLE que
    // el llamador ya tiene guardado) entra, builtin:void sale -- mismo shape que
    // ThreadJoin/ThreadDetach, bucket "expected=1U" compartido.
    MutexCreate,
    MutexLock,
    MutexUnlock,
    MutexClose,
    // Stage 54.4 "Condition Variables" -- NativeSharp.Core.Threading.Monitor. Five INSTANCE
    // methods (same "implicit receiver field" spirit as AtomicInt32/AtomicInt64 above), wrapping
    // Win32's lightweight SRWLOCK + CONDITION_VARIABLE pair (NOT a kernel object -- both are
    // plain 8-byte-on-x64 opaque values living INSIDE the object, never a HANDLE) instead of a
    // second kernel mutex: Monitor needed a lock to pair with a wait/pulse condition variable,
    // and Win32's condition variable API only sleeps against either a CRITICAL_SECTION or an
    // SRWLOCK -- never a kernel mutex HANDLE -- so Stage 54.3's Mutex could not be reused here.
    // SRWLOCK/CONDITION_VARIABLE are both officially DOCUMENTED to be safely initialized by their
    // all-zero bit pattern (SRWLOCK_INIT/CONDITION_VARIABLE_INIT are literally `{0}` -- the same
    // convention MSVC's own std::mutex/std::condition_variable rely on), and Stage 34's object
    // allocator already zero-fills every new object's storage before its constructor runs (see
    // ProjectObjectLayoutModel::zeroInitializesStorage) -- so Monitor's two backing fields need NO
    // explicit "Init" core operation at all, unlike CreateMutexA/CreateThread/CreateSemaphoreA/
    // CreateEventA, which all need a real syscall. Enter/Exit: one implicit field (the private
    // `lock_` field) -- AcquireSRWLockExclusive/ReleaseSRWLockExclusive, EXACT same one-
    // FieldAddress-child shape as InterlockedIncrementInt32 above, only the runtime hook differs.
    // Pulse/PulseAll: one implicit field (the private `cv_` field) -- WakeConditionVariable/
    // WakeAllConditionVariable, same shape again. Wait is the ONLY genuinely new shape in this
    // round: it needs the addresses of BOTH backing fields on the SAME receiver in a single call
    // (SleepConditionVariableSRW(PCONDITION_VARIABLE, PSRWLOCK, DWORD, ULONG)) plus one EXPLICIT
    // builtin:int argument (a real millisecond timeout -- the first Win32 wait this compiler
    // exposes with a caller-chosen timeout at all; every earlier wait, Thread.Join/Mutex.Lock, is
    // hardcoded INFINITE) -- see MethodCoreBinding::secondaryImplicitFieldMemberOrdinal below for
    // why a second ordinal was added instead of re-lowering the receiver expression a second time
    // (which would evaluate a non-trivial receiver expression TWICE, a real correctness bug this
    // round deliberately avoids). Returns builtin:bool: Win32's BOOL result (TRUE if woken,
    // FALSE only on failure -- timeout expiring is the practical failure case here) forwarded
    // directly, no extra logic needed. Per Win32's own contract (and POSIX/C++'s
    // std::condition_variable, identical here), Wait() MUST be called only while the calling
    // thread already holds the lock via Enter() -- calling it unlocked is undefined behavior at
    // the Win32 level, documented as a usage contract in Monitor.ns, not something this compiler
    // can check. A `true` return does not by itself mean the awaited condition became true (Win32
    // condition variables can wake SPURIOUSLY, exactly like POSIX/C++'s) -- see Monitor.ns and
    // 60_stage54_4_monitor.md for the mandatory "recheck the condition in a loop" usage pattern.
    MonitorEnter,
    MonitorExit,
    MonitorWait,
    MonitorPulse,
    MonitorPulseAll,
    // Stage 54.5 "Otras primitivas de sincronizacion" -- NativeSharp.Core.Threading.Semaphore y
    // NativeSharp.Core.Threading.Event. Ambas EXACTO mismo patron estructural que Mutex (Stage
    // 54.3): helpers PRIVADOS Y ESTATICOS, un HANDLE de KERNEL de Win32 guardado como builtin:nint
    // de ida y vuelta entre llamadas separadas -- cero mecanismo de FieldAddress nuevo hizo falta
    // para ninguna de las dos (a diferencia de Monitor arriba). SemaphoreCreate: DOS operandos
    // explicitos (initialCount, maximumCount, ambos builtin:int) sin receptor -- el bucket
    // compartido "expected=2U" que ya existe para EnvironmentSetVariable/WriteAllText/etc. cubre
    // esta forma sin cambios (Semaphore es la PRIMERA operacion nucleo sin receptor que reusa ese
    // bucket para dos builtin:int en vez de builtin:string, pero el bucket nunca exigio un tipo de
    // operando particular). SemaphoreWait/EventWait: builtin:nint (el HANDLE) + builtin:int (el
    // timeout en milisegundos) entra, builtin:bool sale (true = adquirido, false = timeout
    // expirado de verdad -- la MISMA novedad de "timeout real" que Monitor.Wait arriba introduce a
    // este compilador, comparten el mismo runtime hook de bajo nivel
    // __nativesharp_wait_for_handle_timeout, ver WindowsRuntimeBuilder.cpp). SemaphoreRelease:
    // builtin:nint entra, builtin:void sale (libera SIEMPRE un unico nivel -- ver Semaphore.ns
    // para por que un Release(int count) con conteo variable queda fuera de esta ronda).
    // SemaphoreClose: builtin:nint entra, builtin:void sale. EventCreate: DOS operandos explicitos
    // (manualReset, initialState, ambos builtin:bool) sin receptor -- mismo bucket "expected=2U"
    // que SemaphoreCreate, solo cambia el tipo de operando (bool en vez de int, tampoco exigido
    // por el bucket). EventSet/EventReset: builtin:nint entra, builtin:void sale -- mismo shape de
    // un operando que MutexLock/MutexUnlock. EventClose: builtin:nint entra, builtin:void sale.
    SemaphoreCreate,
    SemaphoreWait,
    SemaphoreRelease,
    SemaphoreClose,
    EventCreate,
    EventSet,
    EventReset,
    EventWait,
    EventClose,
    // Stage 55.1 "Process" -- NativeSharp.Core.Diagnostics.Process's raw wrappers around the
    // current process only (no CreateProcess/child-process spawning -- see roadmap doc, out of
    // scope for this stage). Every one of the five reuses the exact zero-explicit-argument shape
    // NativeClock (53.1) established: static method, no parameters, builtin:long result --
    // the process HANDLE (GetCurrentProcess()) is obtained INSIDE the runtime wrapper, never
    // exposed to NativeSharp, so from the frontend's point of view these look identical to
    // QueryCounterRaw. SetPriorityClassRaw is the one exception: one builtin:int argument, no
    // result -- same one-argument/void shape Environment.Exit (53.2) established.
    ProcessGetCurrentProcessIdRaw,
    ProcessGetWorkingSetSizeRaw,
    ProcessGetKernelTimeRaw,
    ProcessGetUserTimeRaw,
    ProcessGetPriorityClassRaw,
    ProcessSetPriorityClassRaw,
    // Stage 55.2 "Hardware / System Info" -- NativeSharp.Core.Diagnostics.SystemInfo. All six are
    // the same zero-argument/builtin:long shape as Process above -- GetSystemInfo and
    // GlobalMemoryStatusEx both fill an in-memory Win32 struct inside the runtime wrapper and this
    // just surfaces one field of it each, no struct type is ever visible to NativeSharp itself.
    // Deliberately does NOT duplicate Environment.ProcessorCount (53.2, already wraps
    // GetActiveProcessorCount) -- this stage covers what that one does not: page size, processor
    // architecture, allocation granularity, and physical memory totals.
    SystemInfoGetPageSizeRaw,
    SystemInfoGetProcessorArchitectureRaw,
    SystemInfoGetAllocationGranularityRaw,
    SystemInfoGetTotalPhysicalMemoryBytesRaw,
    SystemInfoGetAvailablePhysicalMemoryBytesRaw,
    SystemInfoGetMemoryLoadPercentRaw,
    // Stage 55.3 "Diagnostics" -- NativeSharp.Core.Diagnostics.Debug. IsDebuggerPresentRaw and
    // CaptureStackFrameCountRaw share the same zero-argument/builtin:long shape as Process/
    // SystemInfo above (IsDebuggerPresent's BOOL is surfaced as 0/1, the .ns layer turns it into a
    // real bool -- see Debug.ns). OutputStringRaw is the one exception: one builtin:string
    // argument, no result -- the exact one-argument/void shape File.Delete (53.3) established,
    // reusing the same buildNullTerminatedCopy marshaling Environment.SetEnvironmentVariable
    // (53.2b) introduced.
    DebugIsDebuggerPresentRaw,
    DebugCaptureStackFrameCountRaw,
    DebugOutputStringRaw,
    // Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29) -- four small additions
    // requested before starting Stage 56 Win32 Windowing. All four reuse the existing generic
    // "no receiver, operands straight from the call's AST argument list" HIR bucket (see
    // ProjectOopHirModel.cpp's UnaryExpression/String-core-operation block) -- SleepRaw joins the
    // expected=1U list, CopyRaw joins the expected=3U list (alongside StringSubstring), MoveRaw/
    // DirectoryMoveRaw fall through to the default expected=2U bucket. Zero new HIR shape needed.
    ThreadSleepRaw,
    FileCopyRaw,
    FileMoveRaw,
    DirectoryMoveRaw
};

struct MethodCoreBinding final
{
    MethodCoreOperationKind kind = MethodCoreOperationKind::ArrayAllocation;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string resultTypeKey;
    std::string elementTypeKey;
    bool assignable = false;
    // Stage 54.2 "Atomics" -- set only for AtomicInt32/AtomicInt64's Increment/Decrement/Add/
    // Exchange/CompareExchange core operations. These are INSTANCE methods with an IMPLICIT
    // "location" operand -- the address of the receiver's own private `value_` field -- rather
    // than an explicit argument the caller writes out. NativeSharp's OOP LLVM lowering pipeline
    // has no support for the `&`/`*` raw-pointer operators (see ProjectOopHirModel.cpp's
    // UnaryExpression case: "Stage 34 OOP lowering keeps raw pointer address/dereference in the
    // procedural backend boundary" -- a deliberate, hard boundary, discovered during this
    // stage's implementation, that rules out a direct C#-Interlocked-style `int*` parameter
    // approach entirely for any code going through the modern OOP pipeline every stage since 41
    // uses). Resolved once, during method-body recognition (ProjectMethodBodyModel.cpp), by
    // searching the declaring type's own members for its Field named "value_" -- HIR lowering
    // (ProjectOopHirModel.cpp) then builds a synthetic ProjectOopHirNodeKind::FieldAddress node
    // using this ordinal, with the call's receiver as its one child, reusing the EXACT SAME
    // field-address machinery (memberOrdinal -> ProjectObjectLayoutModel byte offset -> GEP)
    // every ordinary NativeSharp field access already relies on -- no new LLVM-lowering
    // mechanism was needed once this was understood.
    std::optional<std::size_t> implicitFieldMemberOrdinal;
    // Stage 54.4 "Condition Variables" -- set ONLY for Monitor::Wait. Every other core operation
    // that needs an implicit field address (AtomicInt32/64 above, and Monitor's own Enter/Exit/
    // Pulse/PulseAll) needs exactly ONE -- implicitFieldMemberOrdinal above is enough. Wait is the
    // sole exception: SleepConditionVariableSRW needs the addresses of TWO different fields on the
    // SAME receiver in a single call (the private `cv_` field, in implicitFieldMemberOrdinal
    // above, AND the private `lock_` field, here) -- resolved once here, during method-body
    // recognition, by the exact same by-name field search Stage 54.2 already established.
    // Deliberately NOT modeled as two separate FieldAddress HIR nodes each re-lowering the
    // receiver expression from its own AST syntax (the way Enter/Exit/Pulse/PulseAll each lower
    // their own single implicit field): a receiver expression is not guaranteed to be free of
    // side effects or even deterministic (nothing stops `GetMonitor().Wait(...)` from being valid
    // NativeSharp), so lowering it twice could evaluate two DIFFERENT objects and take the cv_
    // address of one and the lock_ address of the other -- a real, silent correctness bug this
    // round deliberately designs around. See ProjectOopHirModel.cpp's CoreMonitorWait case (the
    // receiver is lowered exactly ONCE, into a plain object-reference HIR node, with both field
    // ordinals carried as node metadata instead of pre-synthesized FieldAddress children) and
    // ProjectOopMirModel.h's ProjectOopMirNode::secondaryImmediate (the resolved BYTE OFFSET for
    // this ordinal, computed once during HIR->MIR conversion, mirroring how `immediate` already
    // holds the primary field's byte offset for an ordinary FieldAddress node).
    std::optional<std::size_t> secondaryImplicitFieldMemberOrdinal;
};



enum class MethodLambdaCaptureKind
{
    Local,
    Parameter,
    ThisReference
};

struct MethodLambdaCapture final
{
    MethodLambdaCaptureKind kind = MethodLambdaCaptureKind::Local;
    std::string name;
    std::string typeKey;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
};

struct MethodLambdaValueBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t lambdaOrdinal = 0;
    bool lambdaParameter = false;
    std::size_t valueOrdinal = 0;
    std::string typeKey;
};

struct MethodLambdaBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t lambdaOrdinal = 0;
    std::size_t ownerMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string delegateTypeKey;
    std::size_t delegateTypeOrdinal = 0;
    std::string returnTypeKey;
    std::vector<std::string> parameterNames;
    std::vector<std::string> parameterTypeKeys;
    std::vector<MethodLambdaCapture> captures;
    bool blockBody = false;
    std::string generatedName;
};

enum class MethodDelegateOperationKind
{
    Create,
    Invoke
};

struct MethodDelegateBinding final
{
    MethodDelegateOperationKind kind = MethodDelegateOperationKind::Create;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string delegateTypeKey;
    std::size_t delegateTypeOrdinal = 0;
    std::optional<std::size_t> selectedMemberOrdinal;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    bool staticTarget = false;
    std::string resultTypeKey;
    std::vector<ProjectImplicitConversion> argumentConversions;
};

enum class MethodEventOperationKind
{
    Add,
    Remove,
    Invoke
};

struct MethodEventBinding final
{
    MethodEventOperationKind kind = MethodEventOperationKind::Add;
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::size_t eventMemberOrdinal = 0;
    std::size_t backingMemberOrdinal = 0;
    std::string delegateTypeKey;
    std::size_t delegateTypeOrdinal = 0;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    bool staticEvent = false;
    std::string resultTypeKey;
    std::vector<ProjectImplicitConversion> argumentConversions;
};

enum class MethodCallResolutionKind
{
    Resolved,
    NoViableCandidate,
    Ambiguous
};

struct MethodCallCandidate final
{
    std::size_t memberOrdinal = 0;
    bool viable = false;
    std::vector<ProjectImplicitConversion> conversions;
};

struct MethodCallBinding final
{
    MethodCallResolutionKind resolution = MethodCallResolutionKind::NoViableCandidate;
    const ast::AstNode* syntax = nullptr;
    std::size_t callerMethodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string methodName;
    bool explicitThis = false;
    bool implicitThis = false;
    bool baseReceiver = false;
    bool staticCall = false;
    bool interfaceCall = false;
    bool forwardedReceiver = false;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    std::vector<std::size_t> candidateMemberOrdinals;
    std::vector<MethodCallCandidate> evaluatedCandidates;
    std::vector<std::size_t> viableCandidateMemberOrdinals;
    std::optional<std::size_t> selectedMemberOrdinal;
    std::vector<ProjectImplicitConversion> selectedConversions;
    std::string resultTypeKey;
};

class ProjectMethodBodyModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const MethodScope> scopes() const noexcept;
    std::span<const MethodLocalSymbol> locals() const noexcept;
    std::span<const MethodNameBinding> nameBindings() const noexcept;
    std::span<const MethodShadowingEvent> shadowingEvents() const noexcept;
    std::span<const MethodExpressionTypeBinding> expressionTypes() const noexcept;
    std::span<const MethodRuntimeTypeBinding> runtimeTypeBindings() const noexcept;
    std::span<const MethodCoreBinding> coreBindings() const noexcept;
    std::span<const MethodExceptionBinding> exceptionBindings() const noexcept;
    std::span<const MethodCleanupRegion> cleanupRegions() const noexcept;
    std::span<const MethodUsingBinding> usingBindings() const noexcept;
    std::span<const MethodMoveBinding> moveBindings() const noexcept;
    std::span<const MethodOwnershipTransferBinding> ownershipTransferBindings() const noexcept;
    std::span<const MethodBorrowBinding> borrowBindings() const noexcept;
    std::span<const MethodLocalBorrowBinding> localBorrowBindings() const noexcept;
    std::span<const MethodStructuredExitBinding> structuredExits() const noexcept;
    std::span<const MethodCleanupContinuation> cleanupContinuations() const noexcept;
    std::span<const MethodLambdaBinding> lambdaBindings() const noexcept;
    std::span<const MethodLambdaValueBinding> lambdaValueBindings() const noexcept;
    std::span<const MethodDelegateBinding> delegateBindings() const noexcept;
    std::span<const MethodEventBinding> eventBindings() const noexcept;
    std::span<const MethodCallBinding> calls() const noexcept;
    bool isBuilt() const noexcept;

private:
    enum class ScopeNameKind
    {
        Parameter,
        Local
    };

    struct ScopeName final
    {
        ScopeNameKind kind = ScopeNameKind::Local;
        std::string name;
        std::size_t semanticOrdinal = 0;
        SourceRange range;
    };

    struct OwnershipFlowSnapshot final
    {
        bool reachable = true;
        std::unordered_set<std::size_t> movedRaiiLocals;
        std::unordered_set<std::size_t> maybeMovedRaiiLocals;
        std::unordered_set<std::size_t> movedOwnedParameters;
        std::unordered_set<std::size_t> maybeMovedOwnedParameters;
        std::unordered_set<std::size_t> invalidLocalBorrows;
        std::unordered_set<std::size_t> maybeInvalidLocalBorrows;
    };

    struct ExpressionResult final
    {
        std::string typeKey = "<error>";
        bool valid = false;
        bool methodGroup = false;
        bool assignable = false;
        std::vector<std::size_t> methodCandidates;
        std::optional<ProjectIntegerConstant> integerConstant;
        sema::OwnershipKind ownership = sema::OwnershipKind::None;
        bool ownershipTransfer = false;
        std::optional<std::size_t> ownershipSourceLocalOrdinal;
        std::optional<std::size_t> ownershipSourceParameterOrdinal;
        std::optional<std::size_t> borrowedLocalOrdinal;
        const ast::AstNode* receiverSyntax = nullptr;
        std::string receiverTypeKey;
        std::optional<std::size_t> receiverTypeOrdinal;
        bool baseReceiver = false;
        std::optional<std::size_t> eventMemberOrdinal;
        std::optional<std::size_t> assignmentMemberOrdinal;
        // C4 "language references": the `ref`/`out`/`in` keyword written at THIS argument
        // position, if any. Value for every ordinary argument. It is checked against the
        // parameter's own pass kind after overload resolution, so the call site and the signature
        // must agree -- which is the point of requiring the keyword at all: a reader of the call
        // can see that the argument may change without going to read the signature.
        ParameterPassKind argumentPassKind = ParameterPassKind::Value;
    };

    struct LoopContext final
    {
        std::size_t targetScopeOrdinal = 0;
        std::size_t breakCleanupDepth = 0;
        std::size_t continueCleanupDepth = 0;
        std::vector<OwnershipFlowSnapshot> breakStates;
        std::vector<OwnershipFlowSnapshot> continueStates;
    };

    struct FinallyContext final
    {
        std::size_t cleanupRegionOrdinal = 0;
        std::size_t loopDepthAtEntry = 0;
    };

    std::size_t createScope(
        MethodScopeKind kind,
        std::size_t methodMemberOrdinal,
        std::optional<std::size_t> parentScopeOrdinal);
    bool scopeContains(std::size_t ancestorScopeOrdinal, std::size_t scopeOrdinal) const noexcept;
    OwnershipFlowSnapshot captureOwnershipFlow() const;
    void restoreOwnershipFlow(const OwnershipFlowSnapshot& state);
    OwnershipFlowSnapshot joinOwnershipFlow(
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right) const;
    OwnershipFlowSnapshot joinOwnershipFlows(
        const OwnershipFlowSnapshot& seed,
        const std::vector<OwnershipFlowSnapshot>& states) const;
    MethodBorrowState localBorrowFlowState(std::size_t borrowOrdinal) const noexcept;
    void invalidateHistoricalBorrowsOfOwnerLocal(std::size_t localOrdinal);
    void invalidateHistoricalBorrowsOfOwnedParameter(std::size_t parameterOrdinal);
    bool hasActiveBorrowOfOwnerLocal(std::size_t localOrdinal, std::size_t scopeOrdinal) const noexcept;
    bool hasActiveBorrowOfOwnedParameter(std::size_t parameterOrdinal, std::size_t scopeOrdinal) const noexcept;
    void seedParameters(
        std::size_t methodMemberOrdinal,
        std::size_t rootScopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeBlock(
        const ast::AstNode* block,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool createNestedScope,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeVariable(
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeFor(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeForeach(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeReturn(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeLoopExit(
        const ast::AstNode* statement,
        MethodStructuredExitKind kind,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeTry(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeUsing(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> disposableTypeOrdinal() const noexcept;
    std::optional<std::size_t> disposableContractMemberOrdinal(std::size_t disposableTypeOrdinal) const noexcept;
    std::size_t addUsingLocal(
        const ast::AstNode* statement,
        std::string typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeThrow(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> exceptionRootTypeOrdinal() const noexcept;
    bool isExceptionTypeOrdinal(std::size_t typeOrdinal) const noexcept;
    std::size_t addCatchLocal(
        const ast::AstNode* catchClause,
        std::string typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::size_t createCleanupRegion(
        MethodCleanupRegionKind kind,
        const ast::AstNode* syntax,
        const ast::AstNode* cleanupSyntax,
        std::size_t methodMemberOrdinal,
        std::size_t ownerScopeOrdinal);
    std::vector<std::size_t> cleanupPathFromDepth(std::size_t retainedDepth) const;
    void recordStructuredExit(
        MethodStructuredExitKind kind,
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal,
        std::size_t sourceScopeOrdinal,
        std::optional<std::size_t> targetScopeOrdinal,
        std::size_t retainedCleanupDepth);
    void recordCleanupContinuations(const MethodStructuredExitBinding& exit);
    void recordNormalCleanupContinuation(
        std::size_t cleanupRegionOrdinal,
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal,
        std::size_t sourceScopeOrdinal);

    ExpressionResult analyzeExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup = false,
        bool allowEvent = false,
        bool allowMove = false);
    ExpressionResult analyzeIdentifier(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent);
    ExpressionResult analyzeMember(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent);
    ExpressionResult analyzeCall(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeUnary(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeBinary(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeAssignment(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult resolveSyntheticCall(
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::string methodName,
        std::vector<std::size_t> candidates,
        const std::vector<ExpressionResult>& arguments,
        const ast::AstNode* receiverSyntax,
        std::string receiverTypeKey,
        std::optional<std::size_t> receiverTypeOrdinal,
        bool baseReceiver,
        bool interfaceCall,
        diagnostics::DiagnosticEngine& diagnostics,
        bool diagnoseFailure = true);
    std::vector<std::size_t> stage38AccessorCandidates(
        std::size_t receiverTypeOrdinal,
        std::string_view accessorName,
        bool propertyAccessor,
        bool getter,
        bool typeReceiver,
        std::size_t accessingTypeOrdinal,
        std::size_t protectedReceiverTypeOrdinal) const;
    std::vector<std::size_t> stage38OperatorCandidates(
        std::string_view methodName,
        std::string_view leftTypeKey,
        std::optional<std::string_view> rightTypeKey) const;

    std::optional<ScopeName> lookupScopeName(
        std::size_t scopeOrdinal,
        std::string_view name) const;
    std::optional<ScopeName> lookupScopeNameInScope(
        std::size_t scopeOrdinal,
        std::string_view name) const;
    std::vector<std::size_t> memberCandidates(
        std::size_t methodMemberOrdinal,
        std::string_view name) const;
    std::vector<std::size_t> memberCandidatesForType(
        std::size_t typeOrdinal,
        std::string_view name) const;
    std::optional<std::size_t> namedTypeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    std::optional<std::size_t> classTypeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    std::optional<std::size_t> referenceTypeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    std::optional<std::size_t> resolveTypeIdentifier(
        std::size_t contextMemberOrdinal,
        std::string_view name) const noexcept;
    bool contextHasImplicitThis(std::size_t memberOrdinal) const noexcept;
    bool readonlyFieldAssignable(
        const ClassMemberSymbol& field,
        std::size_t contextMemberOrdinal) const noexcept;
    bool isCompileTimeConstantExpression(
        const ast::AstNode* expression,
        std::size_t contextMemberOrdinal) const;
    std::optional<std::size_t> delegateTypeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    bool isDelegateTypeKey(std::string_view typeKey) const noexcept;
    bool delegateSignature(
        std::string_view delegateTypeKey,
        std::string& returnTypeKey,
        std::vector<std::string>& parameterTypeKeys,
        std::size_t& delegateTypeOrdinal) const;
    ExpressionResult bindLambdaToDelegate(
        const ast::AstNode* syntax,
        std::string_view delegateTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectLambdaCaptures(
        MethodLambdaBinding& binding,
        std::size_t nameBindingBegin,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> lambdaCaptureOrdinal(
        const MethodLambdaBinding& binding,
        MethodLambdaCaptureKind kind,
        std::optional<std::size_t> localOrdinal,
        std::optional<std::size_t> parameterOrdinal) const noexcept;
    ExpressionResult bindMethodGroupToDelegate(
        const ast::AstNode* syntax,
        const ExpressionResult& methodGroup,
        std::string_view delegateTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> eventBackingMember(std::size_t eventMemberOrdinal) const noexcept;

    ProjectImplicitConversion classifyImplicit(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        std::optional<ProjectIntegerConstant> integerConstant = std::nullopt) const;
    std::string implicitThisTypeKey(std::size_t methodMemberOrdinal) const;
    // C4 "language references": enforce that a call writes the same `ref`/`out`/`in` keyword the
    // selected member's parameters declare.
    void checkArgumentPassKinds(
        const ast::AstNode* syntax,
        std::string_view methodName,
        std::size_t memberOrdinal,
        const std::vector<ExpressionResult>& arguments,
        diagnostics::DiagnosticEngine& diagnostics) const;
    // C4 "language references": (method member ordinal, parameter ordinal) pairs an `out`
    // parameter was assigned through. See verifyOutParameterAssignments for what is checked with
    // it and for the precise limit of that check.
    std::set<std::pair<std::size_t, std::size_t>> assignedOutParameters_;
    void verifyOutParameterAssignments(diagnostics::DiagnosticEngine& diagnostics) const;
    // C6 "struct members": class, interface OR struct -- everything that can declare accessor
    // members. See the definition for why this is not referenceTypeOrdinalForTypeKey.
    std::optional<std::size_t> accessorReceiverTypeOrdinalForTypeKey(
        std::string_view typeKey) const noexcept;
    bool isClassReferenceTypeKey(std::string_view typeKey) const noexcept;
    bool isReferenceTypeKey(std::string_view typeKey) const noexcept;
    // C1.1 "function-pointer round trip". The three sides of the round trip, as type-key
    // predicates: an extern C delegate (a raw C function pointer), a raw data pointer (`T*`,
    // `void*`) and the pointer-sized integral spelling C APIs hand out (`nint`/`nuint`).
    // Explicit casts among these three ARE the feature; every other cast touching a delegate
    // type is rejected here with a diagnostic instead of reaching the backend.
    bool isExternCDelegateTypeKey(std::string_view typeKey) const noexcept;
    bool isAnyDelegateTypeKey(std::string_view typeKey) const noexcept;
    static bool isRawPointerTypeKey(std::string_view typeKey) noexcept;
    static bool isPointerSizedIntegralTypeKey(std::string_view typeKey) noexcept;
    bool canRuntimeReferenceConvert(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey) const noexcept;
    const ClassMemberSymbol* methodSymbol(std::size_t methodMemberOrdinal) const noexcept;
    bool betterCandidate(
        const MethodCallCandidate& left,
        const MethodCallCandidate& right) const noexcept;
    void recordExpressionType(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        const ExpressionResult& result);
    void reportPrevious(
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string message) const;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    std::vector<MethodScope> scopes_;
    std::vector<std::vector<ScopeName>> scopeNames_;
    std::vector<MethodLocalSymbol> locals_;
    std::vector<MethodNameBinding> nameBindings_;
    std::vector<MethodShadowingEvent> shadowingEvents_;
    std::vector<MethodExpressionTypeBinding> expressionTypes_;
    std::vector<MethodRuntimeTypeBinding> runtimeTypeBindings_;
    std::vector<MethodCoreBinding> coreBindings_;
    std::vector<MethodExceptionBinding> exceptionBindings_;
    std::vector<MethodCleanupRegion> cleanupRegions_;
    std::vector<MethodUsingBinding> usingBindings_;
    std::vector<MethodMoveBinding> moveBindings_;
    std::vector<MethodOwnershipTransferBinding> ownershipTransferBindings_;
    std::vector<MethodBorrowBinding> borrowBindings_;
    std::vector<MethodLocalBorrowBinding> localBorrowBindings_;
    std::unordered_set<std::size_t> movedRaiiLocals_;
    std::unordered_set<std::size_t> maybeMovedRaiiLocals_;
    std::unordered_set<std::size_t> movedOwnedParameters_;
    std::unordered_set<std::size_t> maybeMovedOwnedParameters_;
    std::unordered_set<std::size_t> invalidLocalBorrows_;
    std::unordered_set<std::size_t> maybeInvalidLocalBorrows_;
    bool ownershipFlowReachable_ = true;
    std::vector<OwnershipFlowSnapshot> throwFlowStates_;
    std::vector<MethodStructuredExitBinding> structuredExits_;
    std::vector<MethodCleanupContinuation> cleanupContinuations_;
    std::vector<std::size_t> activeCleanupRegions_;
    std::vector<LoopContext> loopStack_;
    std::vector<FinallyContext> finallyStack_;
    std::vector<MethodLambdaBinding> lambdaBindings_;
    std::vector<MethodLambdaValueBinding> lambdaValueBindings_;
    std::vector<MethodDelegateBinding> delegateBindings_;
    std::optional<std::size_t> currentLambdaOrdinal_;
    std::string currentLambdaReturnTypeKey_;
    std::size_t currentCatchDepth_ = 0;
    std::vector<MethodEventBinding> eventBindings_;
    std::vector<MethodCallBinding> calls_;
    bool built_ = false;
};

const char* methodScopeKindName(MethodScopeKind kind) noexcept;
const char* methodNameBindingKindName(MethodNameBindingKind kind) noexcept;
const char* methodShadowingKindName(MethodShadowingKind kind) noexcept;
const char* methodCallResolutionKindName(MethodCallResolutionKind kind) noexcept;
const char* methodRuntimeTypeOperationKindName(MethodRuntimeTypeOperationKind kind) noexcept;
const char* methodCoreOperationKindName(MethodCoreOperationKind kind) noexcept;
const char* methodExceptionOperationKindName(MethodExceptionOperationKind kind) noexcept;
const char* methodCleanupRegionKindName(MethodCleanupRegionKind kind) noexcept;
const char* methodStructuredExitKindName(MethodStructuredExitKind kind) noexcept;
const char* methodCleanupContinuationKindName(MethodCleanupContinuationKind kind) noexcept;
const char* methodOwnershipTransferKindName(MethodOwnershipTransferKind kind) noexcept;
const char* methodBorrowKindName(MethodBorrowKind kind) noexcept;
const char* methodLocalBorrowSourceKindName(MethodLocalBorrowSourceKind kind) noexcept;
const char* methodBorrowStateName(MethodBorrowState state) noexcept;
const char* methodLambdaCaptureKindName(MethodLambdaCaptureKind kind) noexcept;
const char* methodDelegateOperationKindName(MethodDelegateOperationKind kind) noexcept;
const char* methodEventOperationKindName(MethodEventOperationKind kind) noexcept;
}
