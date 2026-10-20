#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisContext.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisStatistics.h"
#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectConstructionSite.h"
#include "nativesharp/Frontend/ProjectOwnershipFlowStatistics.h"
#include "nativesharp/Frontend/ProjectEscapePropagationStatistics.h"
#include "nativesharp/Frontend/ProjectMethodBodyBuildOptions.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Sema/ExpressionInfo.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
enum class MethodCoreOperationKind
{
    ArrayAllocation,
    RangeArrayAllocation,
    ArrayLength,
    // NativeSharp 3.0 T31. Inline fixed arrays do not carry a runtime header. Their Length is
    // part of the canonical type key and therefore folds to a compile-time integer constant.
    FixedArrayLength,
    ArrayElement,
    RangeArrayElement,
    // NS304-T10-F05: `receiver?[index]` sobre array nativo — mismo element-load que ArrayElement
    // pero con null-check del receptor (resultado nullable).
    NullConditionalArrayElement,
    // C2.1 "fixed arrays": indexing something whose storage is INLINE and unmanaged -- a fixed
    // array field/local (`float[4]`) or a raw pointer (`int*`). One operation kind covers both
    // because the address arithmetic is identical (base + index * elementSize) and neither has a
    // length header to bounds-check against. This is the raw tier, exactly like `*p` and `&x`: no
    // bounds check, no null check, C semantics, by design -- it is the shape C structs and C APIs
    // are written in. `ArrayElement` above stays what it always was: the MANAGED, heap-allocated,
    // length-prefixed, bounds-checked native array.
    RawElement,
    StringLiteral,
    // NS304-T10-F01: `u8"..."` — a typed UTF-8 literal that lowers to ReadOnlySpan<byte> (static
    // bytes + count), allocation-free.
    Utf8StringLiteral,
    // NS304-T10-F03: an interpolated string targeting a handler type lowers to
    // AppendLiteral(parts) + AppendFormatted(holes) + Finish(), with no temporary concatenation.
    InterpolationHandler,
    StringLength,
    StringElement,
    StringConcat,
    StringEquals,
    StringSubstring,
    // SHR-010 "StringView". La DIRECCION del caracter i-esimo de una cadena, no su valor.
    //
    // Por que hace falta una operacion nucleo y no basta `&texto[i]`: el operador `&` de este
    // lenguaje exige un local, un parametro o un campo direccionable, y el elemento de una
    // cadena no es ninguna de las tres -- una cadena nativa es longitud + bytes detras de una
    // referencia, y sus elementos son de solo lectura a proposito (el HIR rechaza
    // `texto[i] = c` con "Stage 41 strings are immutable"). Abrir `&` para cadenas habria dado
    // un puntero ESCRIBIBLE a algo declarado inmutable; una operacion con nombre no.
    //
    // Y es lo que convierte "sin copiar" en una MEDICION en vez de una afirmacion: con esto una
    // fixture puede comprobar que el puntero de la vista cae DENTRO del buffer de la cadena
    // original, que es la unica prueba de que no hubo copia.
    StringCharacterPointer,
    StringFromCharArray,
    // NS304-T03-F03 "list patterns sobre colecciones". Rest-capture `.. var x` sobre un
    // receptor SIN `Slice(int,int)` publico: el compilador sintetiza una vista prestada sin
    // copia ni heap. `StringViewFromString` produce `StringView` (no generico); los demas
    // producen `Span<T>`/`ReadOnlySpan<T>` (cerrado). El receptor + start + length viajan como
    // operandos del nodo; el LLVM emite `{ data, count }` reutilizando la GEP de
    // StringCharacterPointer / RawElement y la longitud ya disponible.
    StringViewFromString,
    SpanFromArray,
    ReadOnlySpanFromArray,
    SpanFromFixedArray,
    ReadOnlySpanFromFixedArray,
    // SHR-022. Las cuatro son LA MISMA operacion con dos bits distintos: a que descriptor se
    // escribe (salida estandar o error estandar) y si se anade el salto de linea. Se enumeran por
    // separado porque el reconocimiento es por NOMBRE DE METODO, y una sola entrada obligaria a
    // llevar el nombre hasta el HIR para volver a decidir alli.
    //
    // Aguas abajo vuelven a ser una: un unico nodo HIR/MIR y una unica funcion de runtime, con la
    // variante en una mascara de dos bits. Cuatro caminos completos habrian sido cuatro copias de
    // la misma llamada a WriteFile.
    ConsoleWriteLine,
    ConsoleWrite,
    ConsoleErrorWriteLine,
    ConsoleErrorWrite,
    ConsoleReadLine,
    // C8 "type root": the two primitives NativeSharp.Core.Object's virtual methods need and that
    // cannot be written in NativeSharp -- read a type's name out of its runtime descriptor, and
    // turn a reference into an integer. See lib/NativeSharp.Core/Object.ns.
    ObjectTypeName,
    ObjectReferenceHash,
    // CC1 fase 4A -- `sizeof(T)` y `sizeof(expression)`. La operacion NO lleva el tamano: lleva en
    // `elementTypeKey` el TIPO QUE SE ESTA MIDIENDO, y `resultTypeKey` es siempre "builtin:nuint".
    //
    // El tamano se resuelve en ProjectOopHirModel, no aqui, y no por gusto: el orden de
    // construccion del pipeline es modelo de cuerpos de metodo PRIMERO y ProjectObjectLayoutModel
    // DESPUES -- ProjectMethodBodyModel::build ni siquiera recibe el modelo de layout. Calcular el
    // tamano aqui exigiria reordenar el pipeline o duplicar las reglas de tamano en el modelo
    // semantico, que es exactamente la duplicacion que el milestone CC1 existe para quitar. El HIR
    // si tiene el modelo de layout y le pregunta a valueLayoutForTypeKey, el mismo servicio
    // canonico que ya comparten el stride de un arreglo y el offset de un campo.
    SizeofConstant,
    // `alignof(T)` and `alignof(expression)` use the same late layout query as sizeof, but select
    // the canonical layout's alignment. `elementTypeKey` is the measured closed type.
    AlignofConstant,
    // `offsetof(T, field)` stores T in elementTypeKey and the selected instance field in
    // implicitFieldMemberOrdinal. The byte offset is resolved later by ProjectObjectLayoutModel.
    OffsetofConstant,
    // CC1 fase 4B -- `stackalloc T[n]`. `elementTypeKey` es T; `resultTypeKey` es "T*". Igual que
    // SizeofConstant, el binding NO lleva el tamano: el modelo semantico corre antes que
    // ProjectObjectLayoutModel. El tamano y la alineacion los resuelve el MIR con elementLayout.
    StackAllocation,
    // CC1 fase 4C -- `new T` escalar (no clase, no arreglo). Misma forma: `elementTypeKey` es T,
    // `resultTypeKey` es "T*", el tamano lo resuelve el MIR.
    RawAllocation,
    // CC1 fase 4D -- `p + i`, `i + p`, `p - i` y `p - q`. `elementTypeKey` es el tipo APUNTADO, que
    // es lo que da el paso; `resultTypeKey` es "T*" para las tres primeras y "builtin:nint" para la
    // diferencia. La operacion concreta se deduce en el HIR del token del nodo de sintaxis, que ya
    // esta ahi -- no hace falta un discriminante propio en el binding.
    PointerArithmetic,
    // C8 "type root", struct half: the three root members a STRUCT gets, synthesized by the
    // compiler and resolved statically -- no vtable, no boxing, not one byte of layout change.
    // A struct that declares its own member of the same name wins; these only fill the gap.
    StructToString,
    StructEquals,
    StructHashCode,
    // NativeSharp 3.0 T02-F04. A record Clone is a plain value copy: no allocation, runtime
    // identity, or hidden layout state.
    StructClone,
    // ID 129. `ToString()` sobre un ENUM devuelve el NOMBRE del enumerador, no su numero.
    //
    // POR QUE ES DEL COMPILADOR Y NO DE String.ns, que es lo que este ID lleva anotado desde el
    // doc 92 §7: los nombres de los enumeradores NO SOBREVIVEN A LA COMPILACION. Un enum es un
    // entero con nombres en el codigo fuente y nada mas en el binario, asi que ninguna cantidad
    // de codigo en la biblioteca puede recuperarlos. Los datos existen aqui --`isEnumConstant` y
    // `enumConstantValue` en ProjectMemberModel.h-- y en ningun otro sitio.
    //
    // MISMO SITIO Y MISMA FORMA QUE StructToString, que es su precedente exacto: se sintetiza
    // cuando la busqueda ordinaria de miembros ya ha fallado, asi que un enum no puede tener un
    // ToString propio que esto pisara.
    //
    // LA DIFERENCIA CON StructToString, y es la que decide la implementacion: la respuesta del
    // struct es una CONSTANTE DE COMPILACION --el nombre de su tipo estatico-- y la del enum
    // depende del VALOR, que en general solo se conoce ejecutando. Por eso no baja a un literal
    // sino a una cadena de condicionales sobre los enumeradores. El detalle, en el sitio donde se
    // baja (ProjectOopHirModel.cpp).
    EnumToString,
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
    AtomicLoadInt32,
    AtomicStoreInt32,
    InterlockedIncrementInt32,
    InterlockedDecrementInt32,
    InterlockedAddInt32,
    InterlockedExchangeInt32,
    InterlockedCompareExchangeInt32,
    AtomicLoadInt64,
    AtomicStoreInt64,
    InterlockedIncrementInt64,
    InterlockedDecrementInt64,
    InterlockedAddInt64,
    InterlockedExchangeInt64,
    InterlockedCompareExchangeInt64,
    AtomicLoadPointer,
    AtomicStorePointer,
    InterlockedExchangePointer,
    InterlockedCompareExchangePointer,
    // Item 307: explicit process-wide and compiler-only memory fences. Their MemoryOrder is
    // consumed at compile time and carried as core-operation metadata.
    AtomicThreadFence,
    AtomicCompilerFence,
    // Entrega 228: primera familia SIMD deliberadamente pequena. Estas operaciones reconocen
    // exclusivamente NativeSharp.Core.Numerics.Float32x4 y conservan el struct publico como
    // contrato ABI; solamente la operacion aritmetica se convierte en un vector LLVM de 128 bits.
    SimdFloat32x4Add,
    SimdFloat32x4Subtract,
    SimdFloat32x4Multiply,
    // Item 347: signed integer vectors keep ordinary struct carriers at public boundaries and
    // use native 128-bit LLVM integer vectors for arithmetic inside the compiler.
    SimdInt128Add,
    SimdInt128Subtract,
    SimdInt128Multiply,
    // Item 348: AVX-width floating vectors, represented by stable 32-byte carriers.
    SimdFloat256Add,
    SimdFloat256Subtract,
    SimdFloat256Multiply,
    // Item 349: signed 256-bit integer vectors.
    SimdInt256Add,
    SimdInt256Subtract,
    SimdInt256Multiply,
    // Item 350: alignment is part of these memory operations' source contract.
    SimdLoadAligned128,
    SimdStoreAligned128,
    // Item 351: explicit unaligned memory operations never overstate pointer alignment.
    SimdLoadUnaligned128,
    SimdStoreUnaligned128,
    // Item 354: public 16-bit carrier converted through LLVM's native half type.
    HalfFromFloat,
    HalfToFloat,
    SimdSwizzleWzyx,
    SimdShuffleLowHigh,
    SimdSqrt,
    SimdReciprocalSqrt,
    SimdFusedMultiplyAdd,
    BitPopulationCount,
    BitLeadingZeroCount,
    BitTrailingZeroCount,
    BitRotateLeft,
    BitRotateRight,
    BitByteSwap,
    CpuPrefetchRead,
    CpuPause,
    CpuReadTimestamp,
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
    // Item 370: the NativeSharp string is copied to an ANSI, null-terminated Win32 object name.
    MutexCreateNamed,
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
    SemaphoreCreateNamed,
    SemaphoreWait,
    SemaphoreRelease,
    SemaphoreClose,
    EventCreate,
    EventCreateNamed,
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
    DirectoryMoveRaw,
    // Bloque 4, fase A -- SHR-030. `File.GetSize(string path)` devuelve `builtin:long`: un
    // builtin:string entra, un entero de 64 bits sale.
    //
    // POR QUE 64 BITS Y NO int. Un `.obj` de LLVM pasa de los 2 GiB con facilidad en un proyecto
    // grande, y `GetFileSizeEx` --que ya esta importado, lo usa FileReadAllText-- devuelve
    // precisamente un LARGE_INTEGER de 64 bits. Truncar aqui a i32 seria inventar un limite que
    // ni Windows ni el formato tienen. FileReadAllText SI trunca, y su propio comentario lo dice:
    // ahi es aceptable porque el resultado es una cadena que se va a manipular en memoria; aqui
    // el resultado ES el tamano y devolverlo mal no tiene arreglo aguas arriba.
    //
    // Devuelve -1 cuando el archivo no existe o no se puede abrir. No es un codigo de error
    // inventado: es el mismo convenio que el resto de esta familia --File.Exists devuelve false,
    // ReadAllText devuelve la cadena vacia-- y ademas -1 no colisiona con ningun tamano posible,
    // que es lo que un 0 si haria con un archivo vacio.
    FileGetSize,
    // Bloque 4, fase A -- SHR-028/029 y el directorio de trabajo.
    //
    // FileReadAllBytes: un builtin:string entra, un builtin:byte[] sale. La forma es IDENTICA a
    // FileReadAllText --un puntero entra, un puntero recien asignado sale-- y no por casualidad:
    // un `byte[]` y un `string` tienen el MISMO layout en memoria, { i64 longitud; datos }. El
    // hook del runtime es literalmente el mismo codigo con otro nombre.
    //
    // FileReadBytes: dos operandos --la ruta y el arreglo del llamador-- y un builtin:int fuera
    // (cuantos bytes se leyeron de verdad). Es el patron "buffer provisto por el llamador" que
    // Directory.GetFiles ya establecio, y es el que usara el driver: leer un .obj de 100 KB para
    // luego copiarlo a otro buffer es justo lo que el Bloque 3 existe para evitar.
    //
    // FileWriteAllBytes: tres operandos --ruta, arreglo, cuantos bytes escribir-- y builtin:bool
    // fuera. El contador es explicito y no la longitud del arreglo a proposito: quien construye
    // un buffer casi siempre lo tiene mas grande que lo que ha llenado, y obligarle a recortarlo
    // seria una copia entera por escritura.
    //
    // EnvironmentCurrentDirectory: cero operandos, builtin:string fuera. Cierra la ultima de las
    // siete operaciones de filesystem que el doc 81 midio (`current_path`).
    FileReadAllBytes,
    FileReadBytes,
    FileWriteAllBytes,
    EnvironmentCurrentDirectory,
    // Bloque 4, fase C -- SHR-032. `Process.Run(exe, linea, salida)` devuelve builtin:int.
    //
    // ES EL BLOQUEO DURO DEL AUTO-HOSPEDADO: sin crear procesos hijos, un `nsc` escrito en
    // NativeSharp no puede lanzar el enlazador. `Process.ns` existia desde Stage 55.1 pero su
    // cabecera decia que crear procesos estaba "deliberadamente fuera de alcance".
    //
    // Tres builtin:string entran, un builtin:int sale -- el codigo de salida del hijo, o -1 si no
    // se pudo lanzar. Es la primera operacion nucleo de tres cadenas.
    ProcessRun,
    // NativeSharp 3.0 T07-F06. Compiler-owned assertion intrinsics are contextual unqualified
    // calls. Assert and Assume consume one bool; Unreachable and Panic are divergent and consume
    // no values. Assume is accepted only inside an explicit unsafe block.
    Assert,
    Unreachable,
    Assume,
    Panic,
    // NS304-T06-F08: the opaque generator iterator's structural protocol. MoveNext takes the
    // iterator and returns builtin:bool (calls moveNextFn); Current is a property reading the
    // item (field 2); GetEnumerator returns the iterator itself (for foreach).
    IteratorMoveNext,
    IteratorCurrent,
    IteratorGetEnumerator,
    IteratorDrop,
    IteratorMoveNextAsync,
    IteratorCancel,
    // NS304-T09-F05: a collection expression targeting a builder (CreateBuilder/Add/ToCollection).
    CollectionBuilder,
    // NS304-T10-F05: a null-conditional index over a USER indexer (`obj?[i]`). The receiver is
    // null-checked and get_Item runs only on the non-null path (lowered via lowerResolvedCall with
    // nullConditional=true).
    NullConditionalUserIndex
};
}
