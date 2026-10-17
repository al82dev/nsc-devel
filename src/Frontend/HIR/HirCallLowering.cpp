#include "HirLoweringSupport.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
struct StringCallLowering final
{
    std::size_t operandCount;
    ProjectOopHirNodeKind hirKind;
};

std::optional<StringCallLowering> stringCallLowering(MethodCoreOperationKind kind)
{
    switch (kind)
    {
    // ---------------------------------------------------------------------------------------
    // DOS OPERANDOS. Era el valor por omision de la cadena vieja, y por eso era la trampa: una
    // operacion nueva que se olvidara caia aqui en silencio. Ahora esta escrito.
    // ---------------------------------------------------------------------------------------
    case MethodCoreOperationKind::StringConcat:
        return StringCallLowering{2U, ProjectOopHirNodeKind::StringConcat};
    case MethodCoreOperationKind::StringEquals:
        return StringCallLowering{2U, ProjectOopHirNodeKind::StringEquals};
    case MethodCoreOperationKind::SimdFloat32x4Add:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdFloat32x4Add};
    case MethodCoreOperationKind::SimdFloat32x4Subtract:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdFloat32x4Subtract};
    case MethodCoreOperationKind::SimdFloat32x4Multiply:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdFloat32x4Multiply};
    case MethodCoreOperationKind::SimdInt128Add:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdInt128Add};
    case MethodCoreOperationKind::SimdInt128Subtract:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdInt128Subtract};
    case MethodCoreOperationKind::SimdInt128Multiply:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdInt128Multiply};
    case MethodCoreOperationKind::SimdFloat256Add:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdFloat256Add};
    case MethodCoreOperationKind::SimdFloat256Subtract:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdFloat256Subtract};
    case MethodCoreOperationKind::SimdFloat256Multiply:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdFloat256Multiply};
    case MethodCoreOperationKind::SimdInt256Add:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdInt256Add};
    case MethodCoreOperationKind::SimdInt256Subtract:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdInt256Subtract};
    case MethodCoreOperationKind::SimdInt256Multiply:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdInt256Multiply};
    case MethodCoreOperationKind::SimdLoadAligned128:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSimdLoadAligned128};
    case MethodCoreOperationKind::SimdStoreAligned128:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdStoreAligned128};
    case MethodCoreOperationKind::SimdLoadUnaligned128:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSimdLoadUnaligned128};
    case MethodCoreOperationKind::SimdStoreUnaligned128:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdStoreUnaligned128};
    case MethodCoreOperationKind::HalfFromFloat:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreHalfFromFloat};
    case MethodCoreOperationKind::HalfToFloat:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreHalfToFloat};
    case MethodCoreOperationKind::SimdSwizzleWzyx:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSimdSwizzleWzyx};
    case MethodCoreOperationKind::SimdShuffleLowHigh:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSimdShuffleLowHigh};
    case MethodCoreOperationKind::SimdSqrt:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSimdSqrt};
    case MethodCoreOperationKind::SimdReciprocalSqrt:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSimdReciprocalSqrt};
    case MethodCoreOperationKind::SimdFusedMultiplyAdd:
        return StringCallLowering{3U, ProjectOopHirNodeKind::CoreSimdFusedMultiplyAdd};
    case MethodCoreOperationKind::BitPopulationCount:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreBitPopulationCount};
    case MethodCoreOperationKind::BitLeadingZeroCount:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreBitLeadingZeroCount};
    case MethodCoreOperationKind::BitTrailingZeroCount:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreBitTrailingZeroCount};
    case MethodCoreOperationKind::BitRotateLeft:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreBitRotateLeft};
    case MethodCoreOperationKind::BitRotateRight:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreBitRotateRight};
    case MethodCoreOperationKind::BitByteSwap:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreBitByteSwap};
    case MethodCoreOperationKind::CpuPrefetchRead:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreCpuPrefetchRead};
    case MethodCoreOperationKind::CpuPause:
        return StringCallLowering{0U, ProjectOopHirNodeKind::CoreCpuPause};
    case MethodCoreOperationKind::CpuReadTimestamp:
        return StringCallLowering{0U, ProjectOopHirNodeKind::CoreCpuReadTimestamp};
    // ---------------------------------------------------------------------------------------
    // TRES OPERANDOS.
    // ---------------------------------------------------------------------------------------
    case MethodCoreOperationKind::StringSubstring:
        return StringCallLowering{3U, ProjectOopHirNodeKind::StringSubstring};
    // SHR-010: `String.CharacterPointer(texto, indice)` cae en este mismo bloque --
    // sin receptor, dos operandos, un gancho distinto en el backend. Es exactamente
    // la forma que el bloque ya cubre; anadir un camino propio habria sido copiar
    // la conversion implicita de operandos por tercera vez.
    case MethodCoreOperationKind::StringCharacterPointer:
        return StringCallLowering{2U, ProjectOopHirNodeKind::StringCharacterPointer};
    // ---------------------------------------------------------------------------------------
    // UN OPERANDO. El grupo grande: "una cosa entra, otra sale". Casi todas son una cadena (una
    // ruta, el nombre de una variable de entorno), pero el bloque nunca exigio que el operando
    // FUERA una cadena -- baja lo que haya con lowerExpression, asi que GetCommandLineArgs entra
    // aqui con un builtin:string[] sin cambiar el mecanismo.
    // ---------------------------------------------------------------------------------------
    case MethodCoreOperationKind::StringFromCharArray:
        return StringCallLowering{1U, ProjectOopHirNodeKind::StringFromCharArray};
    // C8 "type root": RuntimeHelpers.TypeName/ReferenceHash -- one operand each (an
    // Object reference), a string and an int out. Same shared shape as everything
    // else here; the operand type is new, the mechanism is not.
    // C8: TypeName/ReferenceHash each take exactly the object reference.
    case MethodCoreOperationKind::ObjectTypeName:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreObjectTypeName};
    case MethodCoreOperationKind::ObjectReferenceHash:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreObjectReferenceHash};
    // Stage 53.2b "Environment (variables)": GetEnvironmentVariable/
    // SetEnvironmentVariable are string intrinsics in every way this block cares
    // about (operand count, operand lowering with implicit conversions, no
    // receiver) -- they only differ in which runtime hook the LLVM lowerer calls,
    // so they slot into this exact shared block rather than duplicating it.
    case MethodCoreOperationKind::EnvironmentGetVariable:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreEnvironmentGetVariable};
    case MethodCoreOperationKind::EnvironmentSetVariable:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreEnvironmentSetVariable};
    // Stage 53.3 "Filesystem/Paths": File.Exists/File.Delete/Directory.Exists/
    // Directory.CreateDirectory -- same story, one string operand, a different
    // runtime hook.
    case MethodCoreOperationKind::FileExists:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreFileExists};
    case MethodCoreOperationKind::FileDelete:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreFileDelete};
    case MethodCoreOperationKind::DirectoryExists:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreDirectoryExists};
    case MethodCoreOperationKind::DirectoryCreate:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreDirectoryCreate};
    // Stage 53.3b "Filesystem/Paths (contenido de archivo)": ReadAllText/
    // WriteAllText/AppendAllText -- same story again, still no receiver, still just
    // string operands (1 for ReadAllText, 2 for WriteAllText/AppendAllText).
    // Stage 53.3b: ReadAllText is "one string in, a string out" -- same bucket.
    // WriteAllText/AppendAllText are NOT here on purpose: they take two string
    // operands (path, content), so they fall through to the default `expected =
    // 2U` above, the same bucket EnvironmentSetVariable already uses.
    case MethodCoreOperationKind::FileReadAllText:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreFileReadAllText};
    case MethodCoreOperationKind::FileWriteAllText:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreFileWriteAllText};
    case MethodCoreOperationKind::FileAppendAllText:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreFileAppendAllText};
    // Stage 53.4 "Filesystem/Paths (cierre)": Directory.Delete/Path.GetFullPath --
    // same story again, one string operand, a different runtime hook.
    // Stage 53.4: Directory.Delete (one string in, void out) and
    // Path.GetFullPath (one string in, a string out) are both one-operand too.
    case MethodCoreOperationKind::DirectoryDelete:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreDirectoryDelete};
    case MethodCoreOperationKind::PathGetFullPath:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CorePathGetFullPath};
    // Stage 53.5 "Command-line arguments": GetCommandLineArgs -- one operand again,
    // but this time a builtin:string[] instead of a builtin:string. This block
    // never actually required its operand to BE a string -- it just lowers whatever
    // expression is there via lowerExpression, which already handles array-typed
    // expressions elsewhere in the compiler -- so an array operand slots in without
    // any change to the mechanism itself.
    // Stage 53.5: GetCommandLineArgs is "one array in (the caller-provided
    // buffer), an int out" -- also one operand.
    case MethodCoreOperationKind::EnvironmentGetCommandLineArgs:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgs};
    // Stage 53.6 "Enumeracion de directorios": GetFileCount/GetDirectoryCount are
    // "one string in (the path), an int out" -- one operand, same bucket as
    // Directory.Delete/Path.GetFullPath. GetFiles/GetDirectories are "one string in
    // (path) + one array in (the caller-provided buffer), an int out" -- two
    // operands, the same default `expected = 2U` bucket EnvironmentSetVariable/
    // WriteAllText/AppendAllText already use, so no explicit listing is needed for
    // those two below.
    // Stage 53.6: GetFileCount/GetDirectoryCount are "one string in (path), an
    // int out" -- one operand too. GetFiles/GetDirectories are NOT here on
    // purpose: they take two operands (path, destination buffer), so they fall
    // through to the default `expected = 2U` above, the same bucket
    // EnvironmentSetVariable/WriteAllText/AppendAllText already use.
    case MethodCoreOperationKind::DirectoryGetFileCount:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreDirectoryGetFileCount};
    case MethodCoreOperationKind::DirectoryGetFiles:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreDirectoryGetFiles};
    case MethodCoreOperationKind::DirectoryGetDirectoryCount:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreDirectoryGetDirectoryCount};
    case MethodCoreOperationKind::DirectoryGetDirectories:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreDirectoryGetDirectories};
    // Stage 54.1 "Threads": ThreadCreate/ThreadJoin/ThreadDetach are all one operand
    // (ThreadCreate's is a ThreadStart delegate value; ThreadJoin/ThreadDetach's is a
    // builtin:nint HANDLE) -- same shared "no receiver, operands lowered via
    // lowerExpression" shape as every other core operation here.
    // Stage 54.1: ThreadCreate (one delegate operand), ThreadJoin/ThreadDetach
    // (one nint operand) -- all three are one-operand too.
    case MethodCoreOperationKind::ThreadCreate:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreThreadCreate};
    case MethodCoreOperationKind::ThreadJoin:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreThreadJoin};
    case MethodCoreOperationKind::ThreadDetach:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreThreadDetach};
    // ---------------------------------------------------------------------------------------
    // SIN OPERANDOS.
    // ---------------------------------------------------------------------------------------
    // Stage 54.3 "Mutex": MutexCreate (zero operands -- the first core operation in
    // the project with none at all), MutexLock/MutexUnlock/MutexClose (one
    // builtin:nint operand, the saved HANDLE) -- exact same shape as
    // ThreadJoin/ThreadDetach above, no receiver, same shared machinery.
    case MethodCoreOperationKind::MutexCreate:
        return StringCallLowering{0U, ProjectOopHirNodeKind::CoreMutexCreate};
    case MethodCoreOperationKind::MutexCreateNamed:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreMutexCreateNamed};
    // Stage 54.3: MutexLock/MutexUnlock/MutexClose (one builtin:nint HANDLE
    // operand each). MutexCreate is NOT here on purpose -- see the `expected = 0U`
    // branch above.
    case MethodCoreOperationKind::MutexLock:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreMutexLock};
    case MethodCoreOperationKind::MutexUnlock:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreMutexUnlock};
    case MethodCoreOperationKind::MutexClose:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreMutexClose};
    // Stage 54.5 "Otras primitivas de sincronizacion": Semaphore/Event, EXACTO mismo
    // shape que Mutex arriba (sin receptor, HANDLE builtin:nint de ida y vuelta) --
    // SemaphoreCreate/EventCreate son de DOS operandos (bucket "expected=2U" por
    // defecto, ver mas abajo), SemaphoreWait/EventWait tambien DOS (HANDLE + timeout),
    // SemaphoreRelease/SemaphoreClose/EventSet/EventReset/EventClose de UNO (el
    // HANDLE) -- mismo bucket "expected=1U" que MutexLock/MutexUnlock/MutexClose.
    case MethodCoreOperationKind::SemaphoreCreate:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSemaphoreCreate};
    case MethodCoreOperationKind::SemaphoreCreateNamed:
        return StringCallLowering{3U, ProjectOopHirNodeKind::CoreSemaphoreCreateNamed};
    case MethodCoreOperationKind::SemaphoreWait:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreSemaphoreWait};
    // Stage 54.5: SemaphoreRelease/SemaphoreClose/EventSet/EventReset/EventClose
    // (one builtin:nint HANDLE operand each). SemaphoreCreate/EventCreate (two
    // operands) and SemaphoreWait/EventWait (HANDLE + timeout, two operands) are
    // NOT here on purpose -- they fall through to the default `expected = 2U`
    // above, the same bucket EnvironmentSetVariable/WriteAllText/etc. already use.
    case MethodCoreOperationKind::SemaphoreRelease:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSemaphoreRelease};
    case MethodCoreOperationKind::SemaphoreClose:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreSemaphoreClose};
    case MethodCoreOperationKind::EventCreate:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreEventCreate};
    case MethodCoreOperationKind::EventCreateNamed:
        return StringCallLowering{3U, ProjectOopHirNodeKind::CoreEventCreateNamed};
    case MethodCoreOperationKind::EventSet:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreEventSet};
    case MethodCoreOperationKind::EventReset:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreEventReset};
    case MethodCoreOperationKind::EventWait:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreEventWait};
    case MethodCoreOperationKind::EventClose:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreEventClose};
    // Stage 55.1/55.3: Process.SetPriorityClassRaw (one builtin:int operand) and
    // Debug.OutputStringRaw (one builtin:string operand) -- exact same "no receiver,
    // one operand lowered via lowerExpression, void result" shape as
    // MutexLock/FileDelete above; this bucket never required a particular operand
    // type (ThreadJoin's builtin:nint operand already proved that).
    // Stage 55.1/55.3: Process.SetPriorityClassRaw/Debug.OutputStringRaw -- one
    // operand each, see the top-of-block comment above.
    case MethodCoreOperationKind::ProcessSetPriorityClassRaw:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreProcessSetPriorityClass};
    case MethodCoreOperationKind::DebugOutputStringRaw:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreDebugOutputString};
    // Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29): ThreadSleepRaw
    // (one builtin:int operand, void out -- same shape as ProcessSetPriorityClassRaw
    // above), FileCopyRaw (three operands -- joins StringSubstring in the
    // expected=3U bucket below), FileMoveRaw/DirectoryMoveRaw (two operands each,
    // fall through to the default expected=2U bucket). THIS gate is the actual
    // switch that routes a recognized core-operation call into the shared "no
    // receiver, operands straight from the AST" HIR block below -- missing an entry
    // here means the call site is left as an ordinary static-call, and the core
    // operation silently never lowers to its LLVM runtime hook at all.
    // Pre-Stage-56 "utilidades basicas" -- Thread.SleepRaw (one builtin:int
    // operand, void out). FileMoveRaw/DirectoryMoveRaw are NOT here on purpose:
    // they take two operands (source, dest), so they fall through to the default
    // expected=2U bucket above.
    case MethodCoreOperationKind::ThreadSleepRaw:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreThreadSleep};
    // Pre-Stage-56 "utilidades basicas" -- File.CopyRaw(source, dest, overwrite).
    case MethodCoreOperationKind::FileCopyRaw:
        return StringCallLowering{3U, ProjectOopHirNodeKind::CoreFileCopy};
    case MethodCoreOperationKind::FileMoveRaw:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreFileMove};
    case MethodCoreOperationKind::DirectoryMoveRaw:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreDirectoryMove};
    // Bloque 4, fase A: File.GetSize -- una cadena entra, un i64 sale. Va en esta
    // lista y no en el bucket por omision porque ese es de DOS operandos: sin esta
    // linea el enlace no falla ni se queja, simplemente la llamada se queda siendo
    // el metodo NativeSharp normal --que devuelve -1-- y la operacion nucleo no se
    // emite nunca. Medido al implementarla: el hook aparecia declarado en el IR y
    // sin una sola llamada.
    // Bloque 4, fase A: File.GetSize -- una cadena entra, un i64 sale.
    //
    // OJO: SON DOS LISTAS, y hay que estar en LAS DOS. La de arriba decide que
    // esto ES una operacion nucleo con operandos tomados del AST; esta decide
    // CUANTOS operandos se esperan, y su omision es 2. Estar solo en la primera
    // no da un error de compilacion ni un diagnostico util: da
    // "Stage 51 string intrinsic received an invalid operand count" en tiempo de
    // carga, que no menciona ni el nombre de la operacion ni cual de las dos
    // listas falta. Medido al implementar File.GetSize.
    case MethodCoreOperationKind::FileGetSize:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreFileGetSize};
    // Bloque 4, fase A: ReadAllBytes (1 operando), ReadBytes (2, y por tanto en el
    // bucket por omision) y WriteAllBytes (3, listado abajo con StringSubstring).
    // Bloque 4, fase A: ReadAllBytes es de un operando. ReadBytes es de dos y cae
    // en la omision; WriteAllBytes es de tres y va en la lista de arriba.
    case MethodCoreOperationKind::FileReadAllBytes:
        return StringCallLowering{1U, ProjectOopHirNodeKind::CoreFileReadAllBytes};
    case MethodCoreOperationKind::FileReadBytes:
        return StringCallLowering{2U, ProjectOopHirNodeKind::CoreFileReadBytes};
    case MethodCoreOperationKind::FileWriteAllBytes:
        return StringCallLowering{3U, ProjectOopHirNodeKind::CoreFileWriteAllBytes};
    // Bloque 4, fase C: Process.Run -- tres operandos, listado abajo con
    // StringSubstring y FileWriteAllBytes.
    case MethodCoreOperationKind::ProcessRun:
        return StringCallLowering{3U, ProjectOopHirNodeKind::CoreProcessRun};

    // ---------------------------------------------------------------------------------------
    // NO PASAN POR ESTE BLOQUE, y estan listadas una por una a proposito: sin ellas el switch
    // necesitaria un `default`, y con un `default` el compilador ya no obliga a nadie a decidir
    // donde va la operacion numero 105. Cada una de estas se baja por su propio camino en
    // lowerExpression (acceso a arreglo, consola, atomicos, sincronizacion, ...).
    // ---------------------------------------------------------------------------------------
    case MethodCoreOperationKind::ArrayAllocation:
    case MethodCoreOperationKind::RangeArrayAllocation:
    case MethodCoreOperationKind::ArrayLength:
    case MethodCoreOperationKind::FixedArrayLength:
    case MethodCoreOperationKind::ArrayElement:
    case MethodCoreOperationKind::RangeArrayElement:
    case MethodCoreOperationKind::NullConditionalArrayElement:
    case MethodCoreOperationKind::RawElement:
    case MethodCoreOperationKind::StringLiteral:
    case MethodCoreOperationKind::Utf8StringLiteral:
    case MethodCoreOperationKind::StringLength:
    case MethodCoreOperationKind::StringElement:
    case MethodCoreOperationKind::ConsoleWriteLine:
    case MethodCoreOperationKind::ConsoleWrite:
    case MethodCoreOperationKind::ConsoleErrorWriteLine:
    case MethodCoreOperationKind::ConsoleErrorWrite:
    case MethodCoreOperationKind::ConsoleReadLine:
    case MethodCoreOperationKind::SizeofConstant:
    case MethodCoreOperationKind::AlignofConstant:
    case MethodCoreOperationKind::OffsetofConstant:
    case MethodCoreOperationKind::StackAllocation:
    case MethodCoreOperationKind::RawAllocation:
    case MethodCoreOperationKind::PointerArithmetic:
    case MethodCoreOperationKind::StructToString:
    case MethodCoreOperationKind::StructEquals:
    case MethodCoreOperationKind::StructHashCode:
    case MethodCoreOperationKind::StructClone:
    // ID 129. No pasa por el bloque de "llamada de tipo string": no toma operandos de cadena y no
    // se convierte en un nodo HIR unico, sino en una cadena de condicionales. Se baja antes, en
    // el mismo sitio que StructToString.
    case MethodCoreOperationKind::EnumToString:
    case MethodCoreOperationKind::ClockQueryCounter:
    case MethodCoreOperationKind::ClockQueryFrequency:
    case MethodCoreOperationKind::ClockQuerySystemFileTime:
    case MethodCoreOperationKind::EnvironmentTickCount64:
    case MethodCoreOperationKind::EnvironmentProcessorCount:
    case MethodCoreOperationKind::EnvironmentExit:
    case MethodCoreOperationKind::EnvironmentGetCommandLineArgCount:
    case MethodCoreOperationKind::AtomicLoadInt32:
    case MethodCoreOperationKind::AtomicStoreInt32:
    case MethodCoreOperationKind::InterlockedIncrementInt32:
    case MethodCoreOperationKind::InterlockedDecrementInt32:
    case MethodCoreOperationKind::InterlockedAddInt32:
    case MethodCoreOperationKind::InterlockedExchangeInt32:
    case MethodCoreOperationKind::InterlockedCompareExchangeInt32:
    case MethodCoreOperationKind::AtomicLoadInt64:
    case MethodCoreOperationKind::AtomicStoreInt64:
    case MethodCoreOperationKind::InterlockedIncrementInt64:
    case MethodCoreOperationKind::InterlockedDecrementInt64:
    case MethodCoreOperationKind::InterlockedAddInt64:
    case MethodCoreOperationKind::InterlockedExchangeInt64:
    case MethodCoreOperationKind::InterlockedCompareExchangeInt64:
    case MethodCoreOperationKind::AtomicLoadPointer:
    case MethodCoreOperationKind::AtomicStorePointer:
    case MethodCoreOperationKind::InterlockedExchangePointer:
    case MethodCoreOperationKind::InterlockedCompareExchangePointer:
    case MethodCoreOperationKind::AtomicThreadFence:
    case MethodCoreOperationKind::AtomicCompilerFence:
    case MethodCoreOperationKind::MonitorEnter:
    case MethodCoreOperationKind::MonitorExit:
    case MethodCoreOperationKind::MonitorWait:
    case MethodCoreOperationKind::MonitorPulse:
    case MethodCoreOperationKind::MonitorPulseAll:
    case MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw:
    case MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw:
    case MethodCoreOperationKind::ProcessGetKernelTimeRaw:
    case MethodCoreOperationKind::ProcessGetUserTimeRaw:
    case MethodCoreOperationKind::ProcessGetPriorityClassRaw:
    case MethodCoreOperationKind::SystemInfoGetPageSizeRaw:
    case MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw:
    case MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw:
    case MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw:
    case MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw:
    case MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw:
    case MethodCoreOperationKind::DebugIsDebuggerPresentRaw:
    case MethodCoreOperationKind::DebugCaptureStackFrameCountRaw:
    case MethodCoreOperationKind::EnvironmentCurrentDirectory:
    case MethodCoreOperationKind::Assert:
    case MethodCoreOperationKind::Unreachable:
    case MethodCoreOperationKind::Assume:
    case MethodCoreOperationKind::Panic:
    case MethodCoreOperationKind::StringViewFromString:
    case MethodCoreOperationKind::SpanFromArray:
    case MethodCoreOperationKind::ReadOnlySpanFromArray:
    case MethodCoreOperationKind::SpanFromFixedArray:
    case MethodCoreOperationKind::ReadOnlySpanFromFixedArray:
        return std::nullopt;
    // NS304-T06-F08: the iterator protocol is lowered by its own path (see the IteratorCurrent/
    // IteratorMoveNext/IteratorGetEnumerator blocks in lowerExpression), not the shared string block.
    case MethodCoreOperationKind::IteratorMoveNext:
    case MethodCoreOperationKind::IteratorCurrent:
    case MethodCoreOperationKind::IteratorGetEnumerator:
    case MethodCoreOperationKind::IteratorDrop:
    case MethodCoreOperationKind::IteratorMoveNextAsync:
    case MethodCoreOperationKind::IteratorCancel:
        return std::nullopt;
    case MethodCoreOperationKind::CollectionBuilder:
        return std::nullopt;
    case MethodCoreOperationKind::InterpolationHandler:
        return std::nullopt;
    case MethodCoreOperationKind::NullConditionalUserIndex:
        return std::nullopt;
    }

    // Inalcanzable: el switch de arriba cubre todos los enumeradores. Existe solo para callar el
    // aviso de "control reaches end of non-void function", que no entiende esa garantia.
    return std::nullopt;
}


}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEventBackingField(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error,
    const MethodEventBinding* binding,
    bool address)
{
    if (binding == nullptr || memberModel_ == nullptr ||
        binding->backingMemberOrdinal >= memberModel_->members().size())
    {
        error = "Stage 42 event binding has invalid backing member";
        return nullptr;
    }
    const ClassMemberSymbol& backing = memberModel_->members()[binding->backingMemberOrdinal];
    auto node = makeNode(
        binding->staticEvent
            ? (address ? ProjectOopHirNodeKind::StaticFieldAddress : ProjectOopHirNodeKind::StaticFieldValue)
            : (address ? ProjectOopHirNodeKind::FieldAddress : ProjectOopHirNodeKind::FieldValue),
        expression, backing.declaredTypeKey);
    node->memberOrdinal = binding->backingMemberOrdinal;
    if (!binding->staticEvent)
    {
        auto receiver = lowerReceiver(
            binding->receiverSyntax, binding->receiverTypeKey, memberOrdinal, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
    }
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBoundCall(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding* binding,
    const std::vector<const ast::AstNode*>& argumentSyntaxes,
    std::unique_ptr<ProjectOopHirNode> preloweredLast,
    bool nullConditional)
{
    if (binding == nullptr || binding->resolution != MethodCallResolutionKind::Resolved ||
        !binding->selectedMemberOrdinal.has_value())
    {
        error = "Stage 38 requires a resolved synthetic call binding";
        return nullptr;
    }
    std::size_t selectedMemberOrdinal = *binding->selectedMemberOrdinal;
    const ast::AstNode* receiverSyntax = binding->receiverSyntax;
    std::string_view receiverTypeKey = binding->receiverTypeKey;
    bool interfaceDispatch = binding->interfaceCall;

    // An interface conversion whose operand is a freshly constructed concrete object has one
    // exact dynamic type. Resolve its interface slot now so the ordinary direct-call path can
    // honor the implementation's inlining policy. This is deliberately narrow: locals,
    // parameters, fields, and arbitrary expressions still use runtime interface dispatch.
    if (interfaceDispatch && interfaceModel_ != nullptr && memberModel_ != nullptr &&
        binding->receiverTypeOrdinal.has_value() && receiverSyntax != nullptr &&
        (receiverSyntax->kind() == ast::AstNodeKind::CastExpression ||
         receiverSyntax->kind() == ast::AstNodeKind::SafeCastExpression) &&
        receiverSyntax->first() != nullptr &&
        (receiverSyntax->first()->kind() == ast::AstNodeKind::NewExpression ||
         receiverSyntax->first()->kind() == ast::AstNodeKind::ObjectCreationExpression))
    {
        const MethodExpressionTypeBinding* concreteType = expressionType(receiverSyntax->first());
        const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
            ? interfaceModel_->typeOrdinalForTypeKey(concreteType->typeKey)
            : std::nullopt;
        const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
            ? interfaceModel_->tablePlan(*concreteTypeOrdinal, *binding->receiverTypeOrdinal)
            : nullptr;
        if (plan != nullptr)
        {
            const auto contract = std::find(
                plan->interfaceMethodMemberOrdinals.begin(),
                plan->interfaceMethodMemberOrdinals.end(),
                selectedMemberOrdinal);
            if (contract != plan->interfaceMethodMemberOrdinals.end())
            {
                const std::size_t slot = static_cast<std::size_t>(
                    contract - plan->interfaceMethodMemberOrdinals.begin());
                if (slot < plan->implementationMemberOrdinals.size() &&
                    plan->implementationMemberOrdinals[slot].has_value())
                {
                    selectedMemberOrdinal = *plan->implementationMemberOrdinals[slot];
                    receiverSyntax = receiverSyntax->first();
                    receiverTypeKey = concreteType->typeKey;
                    interfaceDispatch = false;
                }
            }
        }
    }
    auto node = makeNode(
        binding->staticCall ? ProjectOopHirNodeKind::StaticCall : ProjectOopHirNodeKind::InstanceCall,
        expression,
        binding->resultTypeKey);
    // NS304-T08-F04: a namespace-callable adapter (extension operator) targets the namespace
    // function, not a class member (memberOrdinal >= members().size()).
    if (binding->staticCall && memberModel_ != nullptr &&
        selectedMemberOrdinal >= memberModel_->members().size())
    {
        node->namespaceFunctionOrdinal =
            selectedMemberOrdinal - memberModel_->members().size();
        node->memberOrdinal.reset();
    }
    else
    {
        node->memberOrdinal = selectedMemberOrdinal;
    }
    node->baseReceiver = binding->baseReceiver;
    node->interfaceDispatch = interfaceDispatch;
    node->nullConditionalCall = nullConditional;
    node->booleanValue = typeBinding != nullptr &&
        typeBinding->flowActiveVariantCase.has_value();
    if (interfaceDispatch) node->targetTypeOrdinal = binding->receiverTypeOrdinal;
    if (!binding->staticCall)
    {
        auto receiver = lowerReceiver(
            receiverSyntax, receiverTypeKey, memberOrdinal, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
    }
    bool hasOwnedTemporary = false;
    for (std::size_t index = 0; index < argumentSyntaxes.size(); ++index)
    {
        const ast::AstNode* argumentSyntax = argumentSyntaxes[index];
        const ClassMemberSymbol* operatorMember = callableSymbol(selectedMemberOrdinal);
        const bool implicitOperatorReference = operatorMember != nullptr &&
            operatorMember->isUserOperator &&
            index < operatorMember->parameters.size() &&
            operatorMember->parameters[index].passKind != ParameterPassKind::Value;
        auto argument = preloweredLast != nullptr && index + 1 == argumentSyntaxes.size()
            ? std::move(preloweredLast)
            : lowerExpression(
                argumentSyntax, memberOrdinal, implicitOperatorReference, error);
        if (argument == nullptr) return nullptr;
        if (index < binding->selectedConversions.size() &&
            binding->selectedConversions[index].kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), binding->selectedConversions[index], argumentSyntax);
        }
        argument = wrapCallArgumentBorrow(
            std::move(argument), argumentSyntax, selectedMemberOrdinal, index,
            hasOwnedTemporary);
        node->children.push_back(std::move(argument));
    }
    const ClassMemberSymbol* selectedMemberPtr = callableSymbol(selectedMemberOrdinal);
    if (selectedMemberPtr == nullptr)
    {
        error = "NSC-LIMIT-020 selected call has no callable metadata";
        return nullptr;
    }
    const ClassMemberSymbol& selectedMember = *selectedMemberPtr;
    for (std::size_t index = 0; index < binding->selectedDefaultArguments.size(); ++index)
    {
        const std::size_t parameterIndex = argumentSyntaxes.size() + index;
        if (parameterIndex >= selectedMember.parameters.size())
        {
            error = "NSC-LIMIT-020 selected default argument has no matching parameter";
            return nullptr;
        }
        auto argument = lowerDefaultArgument(
            binding->selectedDefaultArguments[index],
            selectedMember.parameters[parameterIndex].typeKey,
            error);
        if (argument == nullptr) return nullptr;
        node->children.push_back(std::move(argument));
    }
    node->referenceResult = selectedMember.isReferenceReturn;
    auto loweredCall = wrapFullExpressionCleanup(std::move(node), expression, hasOwnedTemporary);
    if (!selectedMember.isReferenceReturn || addressContext) return loweredCall;
    auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, selectedMember.declaredTypeKey);
    load->children.push_back(std::move(loweredCall));
    return load;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCallExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    const auto lowerEventBacking = [&](
        const MethodEventBinding* binding,
        bool address) -> std::unique_ptr<ProjectOopHirNode>
    {
        return lowerEventBackingField(expression, memberOrdinal, error, binding, address);
    };

    if (const MethodCoreBinding* intrinsic = coreBinding(expression);
        intrinsic != nullptr &&
        (intrinsic->kind == MethodCoreOperationKind::Assert ||
         intrinsic->kind == MethodCoreOperationKind::Unreachable ||
         intrinsic->kind == MethodCoreOperationKind::Assume ||
         intrinsic->kind == MethodCoreOperationKind::Panic))
    {
        if (addressContext)
        {
            error = std::string(methodCoreOperationKindName(intrinsic->kind)) +
                "() has no addressable result";
            return nullptr;
        }

        const bool hasCondition = intrinsic->kind == MethodCoreOperationKind::Assert ||
            intrinsic->kind == MethodCoreOperationKind::Assume;
        const bool isPanic = intrinsic->kind == MethodCoreOperationKind::Panic;
        const std::size_t expectedArguments = hasCondition ? 1U : 0U;
        const bool invalidArity = isPanic
            ? expression->children().size() > 1U
            : expression->children().size() != expectedArguments;
        const std::string_view expectedType = hasCondition ? "builtin:void" : "builtin:never";
        if (invalidArity ||
            intrinsic->resultTypeKey != expectedType)
        {
            error = "T07-F06 assertion binding lost its arity/result contract";
            return nullptr;
        }

        const ProjectOopHirNodeKind kind = intrinsic->kind == MethodCoreOperationKind::Assert
            ? ProjectOopHirNodeKind::Assert
            : intrinsic->kind == MethodCoreOperationKind::Unreachable
                ? ProjectOopHirNodeKind::Unreachable
                : intrinsic->kind == MethodCoreOperationKind::Assume
                    ? ProjectOopHirNodeKind::Assume
                    : ProjectOopHirNodeKind::Panic;
        auto node = makeNode(kind, expression, std::string(expectedType));
        if (intrinsic->kind == MethodCoreOperationKind::Panic)
        {
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > 0; --index)
                node->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
        }
        if (hasCondition)
        {
            auto condition = lowerExpression(
                expression->children().front(), memberOrdinal, false, error);
            if (condition == nullptr) return nullptr;
            if (condition->typeKey != "builtin:bool")
            {
                error = "T07-F06 assertion condition lost its bool type";
                return nullptr;
            }
            node->children.push_back(std::move(condition));
        }
        else if (isPanic && !expression->children().empty())
        {
            auto payload = lowerExpression(
                expression->children().front(), memberOrdinal, false, error);
            if (payload == nullptr) return nullptr;
            if (payload->typeKey != "builtin:string" &&
                payload->typeKey != "string-literal")
            {
                error = "T07-F06 panic payload lost its string type";
                return nullptr;
            }
            node->children.push_back(std::move(payload));
        }
        return node;
    }
    if (const MethodCoreBinding* iteratorCore = coreBinding(expression);
        iteratorCore != nullptr &&
        (iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNext ||
         iteratorCore->kind == MethodCoreOperationKind::IteratorGetEnumerator ||
         iteratorCore->kind == MethodCoreOperationKind::IteratorDrop ||
         iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNextAsync ||
         iteratorCore->kind == MethodCoreOperationKind::IteratorCancel))
    {
        if (addressContext || !expression->children().empty() ||
            expression->first() == nullptr || expression->first()->first() == nullptr)
        {
            error = std::string(methodCoreOperationKindName(iteratorCore->kind)) +
                "() has an invalid call shape";
            return nullptr;
        }
        // MoveNext/Drop/MoveNextAsync mutate the iterator (state/frame), so the receiver is
        // the ADDRESS.
        auto receiver = lowerExpression(
            expression->first()->first(), memberOrdinal,
            iteratorCore->kind != MethodCoreOperationKind::IteratorGetEnumerator, error);
        if (receiver == nullptr) return nullptr;
        auto node = makeNode(
            iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNext
                ? ProjectOopHirNodeKind::IteratorMoveNext
                : iteratorCore->kind == MethodCoreOperationKind::IteratorDrop
                    ? ProjectOopHirNodeKind::IteratorDrop
                    : iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNextAsync
                        ? ProjectOopHirNodeKind::IteratorMoveNextAsync
                        : iteratorCore->kind == MethodCoreOperationKind::IteratorCancel
                            ? ProjectOopHirNodeKind::IteratorCancel
                            : ProjectOopHirNodeKind::IteratorGetEnumerator,
            expression,
            iteratorCore->resultTypeKey);
        node->elementTypeKey = iteratorCore->elementTypeKey;
        node->children.push_back(std::move(receiver));
        // NS304-T06-F07: a MoveNext inside a try attaches the enclosing catch so a typed
        // generator error (state == -2) rethrows into it instead of ending the loop.
        if (!activeHirExceptionHandlers_.empty())
            node->exceptionTargetRegionOrdinal = activeHirExceptionHandlers_.back().hirRegionOrdinal;
        return node;
    }
    const NamespaceFunctionCallBinding* namespaceCall =
        namespaceFunctionCallBinding(expression);
    if (namespaceCall != nullptr)
    {
        if (namespaceCall->resolution != MethodCallResolutionKind::Resolved ||
            !namespaceCall->selectedFunctionOrdinal.has_value() ||
            namespaceFunctionModel_ == nullptr ||
            *namespaceCall->selectedFunctionOrdinal >=
                namespaceFunctionModel_->functions().size())
        {
            error = "namespace function call reached HIR without a resolved target";
            return nullptr;
        }
        const std::size_t functionOrdinal =
            *namespaceCall->selectedFunctionOrdinal;
        const NamespaceFunctionSymbol& target =
            namespaceFunctionModel_->functions()[functionOrdinal];
        if (addressContext && !target.isReferenceReturn)
        {
            error = "namespace function call result is not addressable unless it returns ref";
            return nullptr;
        }

        auto node = makeNode(
            ProjectOopHirNodeKind::StaticCall,
            expression,
            namespaceCall->resultTypeKey);
        node->namespaceFunctionOrdinal = functionOrdinal;
        node->stringValue = target.mangledName;
        const std::size_t targetCallableOrdinal =
            (memberModel_ != nullptr ? memberModel_->members().size() : 0) +
            functionOrdinal;
        bool hasOwnedTemporary = false;
        ProjectOopHirNode* expandedPack = nullptr;
        std::size_t argumentOffset = 0;
        if (namespaceCall->forwardedReceiver)
        {
            if (namespaceCall->receiverSyntax == nullptr || target.parameters.empty() ||
                namespaceCall->selectedArgumentParameterOrdinals.empty())
            {
                error = "extension call lost its synthetic receiver contract";
                return nullptr;
            }
            const std::size_t parameterOrdinal =
                namespaceCall->selectedArgumentParameterOrdinals.front();
            if (parameterOrdinal >= target.parameters.size())
            {
                error = "extension receiver has no matching parameter";
                return nullptr;
            }
            const bool addressReceiver =
                target.parameters[parameterOrdinal].passKind != ParameterPassKind::Value;
            auto receiver = lowerExpression(
                namespaceCall->receiverSyntax, memberOrdinal, addressReceiver, error);
            if (receiver == nullptr) return nullptr;
            if (!addressReceiver && !namespaceCall->selectedConversions.empty() &&
                namespaceCall->selectedConversions.front().kind !=
                    ProjectConversionKind::Identity)
            {
                receiver = wrapImplicitConversion(
                    std::move(receiver), namespaceCall->selectedConversions.front(),
                    namespaceCall->receiverSyntax);
            }
            receiver = wrapCallArgumentBorrow(
                std::move(receiver), namespaceCall->receiverSyntax,
                targetCallableOrdinal, parameterOrdinal, hasOwnedTemporary);
            node->children.push_back(std::move(receiver));
            node->argumentParameterOrdinals.push_back(parameterOrdinal);
            node->argumentTypeKeys.push_back(target.parameters[parameterOrdinal].typeKey);
            argumentOffset = 1;
        }
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            const ast::AstNode* argumentSyntax = expression->children()[index];
            auto argument = lowerExpression(
                argumentSyntax, memberOrdinal, false, error);
            if (argument == nullptr) return nullptr;
            const std::size_t selectedIndex = index + argumentOffset;
            if (selectedIndex >= namespaceCall->selectedArgumentParameterOrdinals.size())
            {
                error = "namespace function argument has no selected parameter ordinal";
                return nullptr;
            }
            const std::size_t parameterOrdinal =
                namespaceCall->selectedArgumentParameterOrdinals[selectedIndex];
            if (parameterOrdinal >= target.parameters.size())
            {
                error = "namespace function argument has no matching parameter";
                return nullptr;
            }
            if (selectedIndex < namespaceCall->selectedConversions.size() &&
                namespaceCall->selectedConversions[selectedIndex].kind !=
                    ProjectConversionKind::Identity)
            {
                argument = wrapImplicitConversion(
                    std::move(argument),
                    namespaceCall->selectedConversions[selectedIndex],
                    argumentSyntax);
            }
            const bool packElement =
                selectedIndex < namespaceCall->selectedParameterPackElements.size() &&
                namespaceCall->selectedParameterPackElements[selectedIndex];
            if (packElement)
            {
                const std::string& elementType =
                    target.parameters[parameterOrdinal].parameterPackElementTypeKey;
                if (elementType.empty())
                {
                    error = "T01-F02 namespace parameter pack lost its element type";
                    return nullptr;
                }
                if (expandedPack == nullptr)
                {
                    auto pack = makeNode(
                        ProjectOopHirNodeKind::ParameterPack,
                        argumentSyntax,
                        target.parameters[parameterOrdinal].typeKey);
                    pack->elementTypeKey = elementType;
                    expandedPack = pack.get();
                    node->children.push_back(std::move(pack));
                    node->argumentParameterOrdinals.push_back(parameterOrdinal);
                    node->argumentTypeKeys.push_back(
                        target.parameters[parameterOrdinal].typeKey);
                }
                expandedPack->children.push_back(std::move(argument));
                continue;
            }
            argument = wrapCallArgumentBorrow(
                std::move(argument),
                argumentSyntax,
                targetCallableOrdinal,
                parameterOrdinal,
                hasOwnedTemporary);
            node->children.push_back(std::move(argument));
            node->argumentParameterOrdinals.push_back(parameterOrdinal);
            node->argumentTypeKeys.push_back(
                target.parameters[parameterOrdinal].typeKey);
        }
        for (std::size_t index = 0;
             index < namespaceCall->selectedDefaultArguments.size();
             ++index)
        {
            if (index >=
                namespaceCall->selectedDefaultArgumentParameterOrdinals.size())
            {
                error = "namespace function default argument has no parameter ordinal";
                return nullptr;
            }
            const std::size_t parameterOrdinal =
                namespaceCall->selectedDefaultArgumentParameterOrdinals[index];
            if (parameterOrdinal >= target.parameters.size())
            {
                error = "namespace function default argument has no matching parameter";
                return nullptr;
            }
            auto argument = lowerDefaultArgument(
                namespaceCall->selectedDefaultArguments[index],
                target.parameters[parameterOrdinal].typeKey,
                error);
            if (argument == nullptr) return nullptr;
            node->children.push_back(std::move(argument));
            node->argumentParameterOrdinals.push_back(parameterOrdinal);
            node->argumentTypeKeys.push_back(
                target.parameters[parameterOrdinal].typeKey);
        }
        if (namespaceCall->selectedSynthesizesEmptyParameterPack)
        {
            if (!namespaceCall->selectedParameterPackOrdinal.has_value() ||
                *namespaceCall->selectedParameterPackOrdinal >= target.parameters.size())
            {
                error = "T01-F02 empty namespace parameter pack has no parameter";
                return nullptr;
            }
            const std::size_t parameterOrdinal =
                *namespaceCall->selectedParameterPackOrdinal;
            const std::string& elementType =
                target.parameters[parameterOrdinal].parameterPackElementTypeKey;
            if (elementType.empty())
            {
                error = "T01-F02 empty namespace parameter pack lost its element type";
                return nullptr;
            }
            auto pack = makeNode(
                ProjectOopHirNodeKind::ParameterPack,
                expression,
                target.parameters[parameterOrdinal].typeKey);
            pack->elementTypeKey = elementType;
            node->children.push_back(std::move(pack));
            node->argumentParameterOrdinals.push_back(parameterOrdinal);
            node->argumentTypeKeys.push_back(target.parameters[parameterOrdinal].typeKey);
        }
        node->argumentCount = node->children.size();
        node->referenceResult = target.isReferenceReturn;
        auto loweredCall = wrapFullExpressionCleanup(
            std::move(node), expression, hasOwnedTemporary);
        if (!target.isReferenceReturn || addressContext) return loweredCall;
        auto load = makeNode(
            ProjectOopHirNodeKind::PointerLoad,
            expression,
            target.declaredTypeKey);
        load->children.push_back(std::move(loweredCall));
        return load;
    }
    const MethodEventBinding* stage42Event = eventBinding(expression);
    if (stage42Event != nullptr && stage42Event->kind == MethodEventOperationKind::Invoke)
    {
        if (addressContext)
        {
            error = "Stage 42 event invocation is not addressable";
            return nullptr;
        }
        auto head = lowerEventBacking(stage42Event, false);
        if (head == nullptr) return nullptr;
        auto node = makeNode(ProjectOopHirNodeKind::EventInvoke, expression, stage42Event->resultTypeKey);
        node->elementTypeKey = stage42Event->delegateTypeKey;
        node->memberOrdinal = stage42Event->backingMemberOrdinal;
        node->children.push_back(std::move(head));
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            const ast::AstNode* argumentSyntax = expression->children()[index];
            auto argument = lowerExpression(argumentSyntax, memberOrdinal, false, error);
            if (argument == nullptr) return nullptr;
            if (index < stage42Event->argumentConversions.size())
            {
                const ProjectImplicitConversion& conversion = stage42Event->argumentConversions[index];
                node->argumentTypeKeys.push_back(conversion.targetTypeKey);
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    argument = wrapImplicitConversion(std::move(argument), conversion, argumentSyntax);
                }
            }
            node->children.push_back(std::move(argument));
        }
        node->argumentCount = expression->children().size();
        return node;
    }

    const MethodDelegateBinding* invoke = delegateBinding(expression);
    if (invoke != nullptr && invoke->kind == MethodDelegateOperationKind::Invoke)
    {
        if (addressContext)
        {
            error = "Stage 42 delegate invocation is not addressable";
            return nullptr;
        }
        auto node = makeNode(ProjectOopHirNodeKind::DelegateInvoke, expression, invoke->resultTypeKey);
        node->elementTypeKey = invoke->delegateTypeKey;
        node->stringValue = invoke->directLambdaName;
        node->booleanValue = invoke->directLambdaUsesEnvironment;
        if (invoke->directLambdaName.empty() || invoke->directLambdaUsesEnvironment)
        {
            auto callee = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (callee == nullptr) return nullptr;
            node->children.push_back(std::move(callee));
        }
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            const ast::AstNode* argumentSyntax = expression->children()[index];
            auto argument = lowerExpression(argumentSyntax, memberOrdinal, false, error);
            if (argument == nullptr) return nullptr;
            if (index < invoke->argumentConversions.size())
            {
                const ProjectImplicitConversion& conversion = invoke->argumentConversions[index];
                node->argumentTypeKeys.push_back(conversion.targetTypeKey);
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    argument = wrapImplicitConversion(std::move(argument), conversion, argumentSyntax);
                }
            }
            node->children.push_back(std::move(argument));
        }
        for (std::size_t index = 0; index < invoke->selectedDefaultArguments.size(); ++index)
        {
            const std::string& targetTypeKey =
                index < invoke->selectedDefaultArgumentTypeKeys.size()
                ? invoke->selectedDefaultArgumentTypeKeys[index] : std::string{};
            auto argument = lowerDefaultArgument(
                invoke->selectedDefaultArguments[index], targetTypeKey, error);
            if (argument == nullptr) return nullptr;
            node->argumentTypeKeys.push_back(targetTypeKey);
            node->children.push_back(std::move(argument));
        }
        node->argumentCount =
            expression->children().size() + invoke->selectedDefaultArguments.size();
        return node;
    }

    const MethodCoreBinding* core = coreBinding(expression);
    // NS304-T03-F03: list-pattern rest capture over a native string synthesizes a
    // StringView view directly (no Slice method). Receiver + start + length.
    if (core != nullptr &&
        core->kind == MethodCoreOperationKind::StringViewFromString)
    {
        if (addressContext)
        {
            error = "a StringView rest capture is a value and is not addressable";
            return nullptr;
        }
        auto node = makeNode(
            ProjectOopHirNodeKind::StringViewFromString, expression, core->resultTypeKey);
        node->elementTypeKey = core->elementTypeKey;
        auto receiver = lowerExpression(
            expression->first()->first(), memberOrdinal, false, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
        for (const ast::AstNode* child : expression->children())
        {
            auto operand = lowerExpression(child, memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;
            node->children.push_back(std::move(operand));
        }
        return node;
    }
    // NS304-T03-F03: same view synthesis over a native/fixed array -> Span<T>/ReadOnlySpan<T>.
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::SpanFromArray ||
         core->kind == MethodCoreOperationKind::ReadOnlySpanFromArray ||
         core->kind == MethodCoreOperationKind::SpanFromFixedArray ||
         core->kind == MethodCoreOperationKind::ReadOnlySpanFromFixedArray))
    {
        if (addressContext)
        {
            error = "a Span/ReadOnlySpan rest capture is a value and is not addressable";
            return nullptr;
        }
        ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::SpanFromArray;
        switch (core->kind)
        {
            case MethodCoreOperationKind::SpanFromArray:
                nodeKind = ProjectOopHirNodeKind::SpanFromArray;
                break;
            case MethodCoreOperationKind::ReadOnlySpanFromArray:
                nodeKind = ProjectOopHirNodeKind::ReadOnlySpanFromArray;
                break;
            case MethodCoreOperationKind::SpanFromFixedArray:
                nodeKind = ProjectOopHirNodeKind::SpanFromFixedArray;
                break;
            default:
                nodeKind = ProjectOopHirNodeKind::ReadOnlySpanFromFixedArray;
                break;
        }
        auto node = makeNode(nodeKind, expression, core->resultTypeKey);
        node->elementTypeKey = core->elementTypeKey;
        const bool fixedArrayView =
            core->kind == MethodCoreOperationKind::SpanFromFixedArray ||
            core->kind == MethodCoreOperationKind::ReadOnlySpanFromFixedArray;
        auto receiver = lowerExpression(
            expression->first()->first(), memberOrdinal, fixedArrayView, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
        for (const ast::AstNode* child : expression->children())
        {
            auto operand = lowerExpression(child, memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;
            node->children.push_back(std::move(operand));
        }
        return node;
    }
    // Las TRES preguntas --entra en este bloque, con cuantos operandos, y en que nodo
    // HIR se convierte-- salen de la MISMA respuesta: ver stringCallLowering() al
    // principio de este archivo.
    const std::optional<StringCallLowering> coreLowering =
        core != nullptr ? stringCallLowering(core->kind) : std::nullopt;
    if (coreLowering.has_value())
    {
        if (addressContext)
        {
            error = "Stage 51 NativeSharp.Core.String result is not addressable";
            return nullptr;
        }
        const MethodCallBinding* stringCall = callBinding(expression);
        if (stringCall == nullptr || !stringCall->selectedMemberOrdinal.has_value())
        {
            error = "Stage 51 string intrinsic requires a resolved Core.String call";
            return nullptr;
        }
        std::vector<const ast::AstNode*> operands;
        if (stringCall->forwardedReceiver)
        {
            if (stringCall->receiverSyntax == nullptr)
            {
                error = "Stage 51 forwarded string call has no receiver";
                return nullptr;
            }
            operands.push_back(stringCall->receiverSyntax);
        }
        operands.insert(operands.end(), expression->children().begin(), expression->children().end());
        // Stage 53.3: this used to be a nested ternary (readable enough for 4 kinds, not
        // for 8) -- rewritten as a plain if-chain now that File/Directory's four
        // one-operand operations join StringFromCharArray/EnvironmentGetVariable in that
        // bucket. Same three buckets as before: 3 operands (Substring only), 1 operand
        // (every "one string in, one thing out" operation), 2 operands (everything else:
        // Concat/Equals/SetEnvironmentVariable).
        // La aridad viene de la misma tabla que decidio entrar aqui, asi que no puede
        // discrepar con ella. El `expected = 2U` por omision de la version anterior
        // --la trampa que se tragaba cualquier operacion nueva olvidada-- ya no existe.
        const std::size_t expected = coreLowering->operandCount;
        if (operands.size() != expected)
        {
            error = "Stage 51 string intrinsic received an invalid operand count";
            return nullptr;
        }
        // EL NODO HIR VIENE DE LA MISMA TABLA. Aqui habia una cadena de 49 `else if`
        // con `StringConcat` como valor inicial, y ese valor inicial era la trampa: una
        // operacion nueva sin su rama no se quedaba sin nodo, se convertia en una
        // CONCATENACION DE CADENAS. `File.GetSize(ruta)` se habria bajado como
        // `concat(ruta, ...)`, sin un solo mensaje. Ahora la tabla que autorizo la
        // entrada es la que dice el nodo, asi que no hay omision posible.
        const ProjectOopHirNodeKind kind = coreLowering->hirKind;
        auto node = makeNode(kind, expression, core->resultTypeKey);
        for (std::size_t index = 0; index < operands.size(); ++index)
        {
            const ast::AstNode* operandSyntax = operands[index];
            auto operand = lowerExpression(operandSyntax, memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;
            if (index < stringCall->selectedConversions.size() &&
                stringCall->selectedConversions[index].kind != ProjectConversionKind::Identity)
            {
                operand = wrapImplicitConversion(
                    std::move(operand), stringCall->selectedConversions[index], operandSyntax);
            }
            node->children.push_back(std::move(operand));
        }
        return node;
    }
    // Item 307: AtomicFence has one syntactic argument, but that MemoryOrder was folded
    // already and is metadata rather than an executable operand.
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::AtomicThreadFence ||
         core->kind == MethodCoreOperationKind::AtomicCompilerFence))
    {
        if (addressContext)
        {
            error = "AtomicFence result is not addressable";
            return nullptr;
        }
        if (!core->atomicMemoryOrder.has_value() || expression->children().size() != 1U)
        {
            error = "AtomicFence requires exactly one validated compile-time MemoryOrder";
            return nullptr;
        }
        const ProjectOopHirNodeKind kind =
            core->kind == MethodCoreOperationKind::AtomicThreadFence
                ? ProjectOopHirNodeKind::CoreAtomicThreadFence
                : ProjectOopHirNodeKind::CoreAtomicCompilerFence;
        auto node = makeNode(kind, expression, core->resultTypeKey);
        node->integerValue = static_cast<std::uint64_t>(*core->atomicMemoryOrder) + 1U;
        return node;
    }

    // Stage 54.2/Item 311 atomics have one of two source-level locations. The wrapper API
    // (AtomicInt32/AtomicInt64/AtomicPointer) supplies a receiver and HIR synthesizes the
    // address of its private `value_` field. The public Interlocked API supplies an
    // explicit first `ref T` argument, whose parsed address-of expression lowers directly
    // to the location pointer. Both forms deliberately converge here on the same core HIR
    // nodes, so MIR and LLVM have one ordering/ABI implementation.
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::AtomicLoadInt32 ||
         core->kind == MethodCoreOperationKind::AtomicStoreInt32 ||
         core->kind == MethodCoreOperationKind::InterlockedIncrementInt32 ||
         core->kind == MethodCoreOperationKind::InterlockedDecrementInt32 ||
         core->kind == MethodCoreOperationKind::InterlockedAddInt32 ||
         core->kind == MethodCoreOperationKind::InterlockedExchangeInt32 ||
         core->kind == MethodCoreOperationKind::InterlockedCompareExchangeInt32 ||
         core->kind == MethodCoreOperationKind::AtomicLoadInt64 ||
         core->kind == MethodCoreOperationKind::AtomicStoreInt64 ||
         core->kind == MethodCoreOperationKind::InterlockedIncrementInt64 ||
         core->kind == MethodCoreOperationKind::InterlockedDecrementInt64 ||
         core->kind == MethodCoreOperationKind::InterlockedAddInt64 ||
         core->kind == MethodCoreOperationKind::InterlockedExchangeInt64 ||
         core->kind == MethodCoreOperationKind::InterlockedCompareExchangeInt64 ||
         core->kind == MethodCoreOperationKind::AtomicLoadPointer ||
         core->kind == MethodCoreOperationKind::AtomicStorePointer ||
         core->kind == MethodCoreOperationKind::InterlockedExchangePointer ||
         core->kind == MethodCoreOperationKind::InterlockedCompareExchangePointer))
    {
        if (addressContext)
        {
            error = "Stage 54.2 Atomic operation result is not addressable";
            return nullptr;
        }
        const MethodCallBinding* call = callBinding(expression);
        if (call == nullptr)
        {
            error = "Stage 54.2 Atomic core operation requires a resolved call";
            return nullptr;
        }
        const bool explicitLocation = !core->implicitFieldMemberOrdinal.has_value();
        const std::size_t argumentOffset = explicitLocation ? 1U : 0U;
        std::unique_ptr<ProjectOopHirNode> location;
        if (explicitLocation)
        {
            if (!call->staticCall || expression->children().empty())
            {
                error = "Stage 54.2 Interlocked operation requires an explicit ref location";
                return nullptr;
            }
            location = lowerExpression(expression->children().front(), memberOrdinal, false, error);
            if (location == nullptr) return nullptr;
        }
        else
        {
            if (call->receiverSyntax == nullptr)
            {
                error = "Stage 54.2 Atomic wrapper operation requires a resolved receiver";
                return nullptr;
            }
            auto receiver = lowerExpression(call->receiverSyntax, memberOrdinal, false, error);
            if (receiver == nullptr) return nullptr;
            location = makeNode(
                ProjectOopHirNodeKind::FieldAddress, expression, core->elementTypeKey);
            location->memberOrdinal = *core->implicitFieldMemberOrdinal;
            location->children.push_back(std::move(receiver));
        }

        std::size_t expectedValueArguments = 0U;
        if (core->kind == MethodCoreOperationKind::AtomicStoreInt32 ||
            core->kind == MethodCoreOperationKind::AtomicStoreInt64 ||
            core->kind == MethodCoreOperationKind::AtomicStorePointer ||
            core->kind == MethodCoreOperationKind::InterlockedAddInt32 ||
            core->kind == MethodCoreOperationKind::InterlockedAddInt64 ||
            core->kind == MethodCoreOperationKind::InterlockedExchangeInt32 ||
            core->kind == MethodCoreOperationKind::InterlockedExchangeInt64 ||
            core->kind == MethodCoreOperationKind::InterlockedExchangePointer)
        {
            expectedValueArguments = 1U;
        }
        else if (core->kind == MethodCoreOperationKind::InterlockedCompareExchangeInt32 ||
            core->kind == MethodCoreOperationKind::InterlockedCompareExchangeInt64 ||
            core->kind == MethodCoreOperationKind::InterlockedCompareExchangePointer)
        {
            expectedValueArguments = 2U;
        }
        const std::size_t expectedExplicitArguments = argumentOffset + expectedValueArguments +
            (core->atomicMemoryOrder.has_value() ? 1U : 0U) +
            (core->atomicFailureMemoryOrder.has_value() ? 1U : 0U);
        if (expression->children().size() != expectedExplicitArguments)
        {
            error = "Stage 54.2 Atomic core operation received an invalid explicit argument count";
            return nullptr;
        }

        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreAtomicLoadInt32;
        if (core->kind == MethodCoreOperationKind::AtomicLoadInt32) kind = ProjectOopHirNodeKind::CoreAtomicLoadInt32;
        else if (core->kind == MethodCoreOperationKind::AtomicStoreInt32) kind = ProjectOopHirNodeKind::CoreAtomicStoreInt32;
        else if (core->kind == MethodCoreOperationKind::InterlockedIncrementInt32) kind = ProjectOopHirNodeKind::CoreInterlockedIncrementInt32;
        else if (core->kind == MethodCoreOperationKind::InterlockedDecrementInt32) kind = ProjectOopHirNodeKind::CoreInterlockedDecrementInt32;
        else if (core->kind == MethodCoreOperationKind::InterlockedAddInt32) kind = ProjectOopHirNodeKind::CoreInterlockedAddInt32;
        else if (core->kind == MethodCoreOperationKind::InterlockedExchangeInt32) kind = ProjectOopHirNodeKind::CoreInterlockedExchangeInt32;
        else if (core->kind == MethodCoreOperationKind::InterlockedCompareExchangeInt32) kind = ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt32;
        else if (core->kind == MethodCoreOperationKind::AtomicLoadInt64) kind = ProjectOopHirNodeKind::CoreAtomicLoadInt64;
        else if (core->kind == MethodCoreOperationKind::AtomicStoreInt64) kind = ProjectOopHirNodeKind::CoreAtomicStoreInt64;
        else if (core->kind == MethodCoreOperationKind::InterlockedIncrementInt64) kind = ProjectOopHirNodeKind::CoreInterlockedIncrementInt64;
        else if (core->kind == MethodCoreOperationKind::InterlockedDecrementInt64) kind = ProjectOopHirNodeKind::CoreInterlockedDecrementInt64;
        else if (core->kind == MethodCoreOperationKind::InterlockedAddInt64) kind = ProjectOopHirNodeKind::CoreInterlockedAddInt64;
        else if (core->kind == MethodCoreOperationKind::InterlockedExchangeInt64) kind = ProjectOopHirNodeKind::CoreInterlockedExchangeInt64;
        else if (core->kind == MethodCoreOperationKind::InterlockedCompareExchangeInt64) kind = ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt64;
        else if (core->kind == MethodCoreOperationKind::AtomicLoadPointer) kind = ProjectOopHirNodeKind::CoreAtomicLoadPointer;
        else if (core->kind == MethodCoreOperationKind::AtomicStorePointer) kind = ProjectOopHirNodeKind::CoreAtomicStorePointer;
        else if (core->kind == MethodCoreOperationKind::InterlockedExchangePointer) kind = ProjectOopHirNodeKind::CoreInterlockedExchangePointer;
        else if (core->kind == MethodCoreOperationKind::InterlockedCompareExchangePointer) kind = ProjectOopHirNodeKind::CoreInterlockedCompareExchangePointer;

        auto node = makeNode(kind, expression, core->resultTypeKey);
        if (core->atomicMemoryOrder.has_value())
        {
            // Zero means "legacy overload" on the MIR node, so ordered values are carried
            // as ABI value + 1. LLVM lowering performs the explicit NativeSharp->LLVM map.
            node->integerValue =
                static_cast<std::uint64_t>(*core->atomicMemoryOrder) + 1U;
            if (core->atomicFailureMemoryOrder.has_value())
            {
                // CompareExchange packs its independent failure order in the next byte.
                // This is internal HIR/MIR metadata, not part of the public enum ABI.
                node->integerValue |=
                    (static_cast<std::uint64_t>(*core->atomicFailureMemoryOrder) + 1U) << 8U;
            }
        }
        node->children.push_back(std::move(location));
        for (std::size_t index = 0; index < expectedValueArguments; ++index)
        {
            const std::size_t sourceIndex = argumentOffset + index;
            const ast::AstNode* argumentSyntax = expression->children()[sourceIndex];
            auto argument = lowerExpression(argumentSyntax, memberOrdinal, false, error);
            if (argument == nullptr) return nullptr;
            if (sourceIndex < call->selectedConversions.size() &&
                call->selectedConversions[sourceIndex].kind != ProjectConversionKind::Identity)
            {
                argument = wrapImplicitConversion(
                    std::move(argument), call->selectedConversions[sourceIndex], argumentSyntax);
            }
            node->children.push_back(std::move(argument));
        }
        return node;
    }
    // Stage 54.4 "Condition Variables" -- NativeSharp.Core.Threading.Monitor's five
    // instance methods. Enter/Exit/Pulse/PulseAll rejoin the EXACT same "one implicit
    // FieldAddress child, built by hand from the receiver + a single resolved ordinal"
    // shape the Atomics block right above already established -- see that block's own
    // header comment for the full rationale (no `&`/`*` operator support in this
    // pipeline). Wait is handled separately below (it is NOT part of this if): it needs
    // TWO field addresses on the SAME receiver rather than one, which this shared shape
    // cannot express without lowering the receiver expression twice (see
    // MethodCoreOperationKind::MonitorWait's comment in ProjectMethodBodyModel.h).
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::MonitorEnter ||
         core->kind == MethodCoreOperationKind::MonitorExit ||
         core->kind == MethodCoreOperationKind::MonitorPulse ||
         core->kind == MethodCoreOperationKind::MonitorPulseAll))
    {
        if (addressContext)
        {
            error = "Stage 54.4 Monitor operation result is not addressable";
            return nullptr;
        }
        if (!core->implicitFieldMemberOrdinal.has_value())
        {
            error = "Stage 54.4 Monitor core operation is missing its implicit field member ordinal";
            return nullptr;
        }
        const MethodCallBinding* call = callBinding(expression);
        if (call == nullptr || call->receiverSyntax == nullptr)
        {
            error = "Stage 54.4 Monitor core operation requires a resolved receiver";
            return nullptr;
        }
        if (!expression->children().empty())
        {
            error = "Stage 54.4 Monitor.Enter/Exit/Pulse/PulseAll take no explicit arguments";
            return nullptr;
        }
        auto receiver = lowerExpression(call->receiverSyntax, memberOrdinal, false, error);
        if (receiver == nullptr) return nullptr;

        auto location = makeNode(ProjectOopHirNodeKind::FieldAddress, expression, core->elementTypeKey);
        location->memberOrdinal = *core->implicitFieldMemberOrdinal;
        location->children.push_back(std::move(receiver));

        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreMonitorEnter;
        if (core->kind == MethodCoreOperationKind::MonitorEnter) kind = ProjectOopHirNodeKind::CoreMonitorEnter;
        else if (core->kind == MethodCoreOperationKind::MonitorExit) kind = ProjectOopHirNodeKind::CoreMonitorExit;
        else if (core->kind == MethodCoreOperationKind::MonitorPulse) kind = ProjectOopHirNodeKind::CoreMonitorPulse;
        else if (core->kind == MethodCoreOperationKind::MonitorPulseAll) kind = ProjectOopHirNodeKind::CoreMonitorPulseAll;

        auto node = makeNode(kind, expression, core->resultTypeKey);
        node->children.push_back(std::move(location));
        return node;
    }
    // Stage 54.4: Monitor.Wait(int millisecondsTimeout) -- the one core operation in this
    // round that needs the addresses of TWO different fields (`cv_`, `lock_`) on the SAME
    // receiver for a single runtime call (SleepConditionVariableSRW). Deliberately built as
    // a single custom node whose ONLY child is the receiver expression lowered EXACTLY
    // ONCE (never wrapped in a FieldAddress node here) plus the explicit timeout argument,
    // with BOTH field ordinals carried as node metadata (memberOrdinal = cv_,
    // secondaryMemberOrdinal = lock_) for ProjectOopMirModel.cpp to resolve into byte
    // offsets and ProjectOopLlvmLowerer.cpp to turn into two GEPs off that single lowered
    // receiver value -- see MethodCoreOperationKind::MonitorWait's comment in
    // ProjectMethodBodyModel.h for why re-lowering the receiver a second time (the way
    // Enter/Exit/Pulse/PulseAll each lower their own single implicit field, above) would be
    // a real correctness bug for a non-trivial receiver expression.
    if (core != nullptr && core->kind == MethodCoreOperationKind::MonitorWait)
    {
        if (addressContext)
        {
            error = "Stage 54.4 Monitor.Wait result is not addressable";
            return nullptr;
        }
        if (!core->implicitFieldMemberOrdinal.has_value() ||
            !core->secondaryImplicitFieldMemberOrdinal.has_value())
        {
            error = "Stage 54.4 Monitor.Wait is missing one of its two implicit field member ordinals";
            return nullptr;
        }
        const MethodCallBinding* call = callBinding(expression);
        if (call == nullptr || call->receiverSyntax == nullptr)
        {
            error = "Stage 54.4 Monitor.Wait requires a resolved receiver";
            return nullptr;
        }
        if (expression->children().size() != 1)
        {
            error = "Stage 54.4 Monitor.Wait takes exactly one explicit millisecondsTimeout argument";
            return nullptr;
        }
        auto receiver = lowerExpression(call->receiverSyntax, memberOrdinal, false, error);
        if (receiver == nullptr) return nullptr;

        auto timeout = lowerExpression(expression->children().front(), memberOrdinal, false, error);
        if (timeout == nullptr) return nullptr;
        if (!call->selectedConversions.empty() &&
            call->selectedConversions.front().kind != ProjectConversionKind::Identity)
        {
            timeout = wrapImplicitConversion(
                std::move(timeout), call->selectedConversions.front(), expression->children().front());
        }

        auto node = makeNode(ProjectOopHirNodeKind::CoreMonitorWait, expression, core->resultTypeKey);
        node->memberOrdinal = *core->implicitFieldMemberOrdinal;
        node->secondaryMemberOrdinal = *core->secondaryImplicitFieldMemberOrdinal;
        node->children.push_back(std::move(receiver));
        node->children.push_back(std::move(timeout));
        return node;
    }
    // SHR-022. AQUI ES DONDE LAS CUATRO SALIDAS DE CONSOLA VUELVEN A SER UNA.
    //
    // El modelo semantico las distingue por nombre y les da cuatro `MethodCoreOperationKind`
    // distintos, porque el reconocimiento es por nombre de metodo. De aqui hacia abajo esa
    // distincion se convierte en DOS BITS y no en cuatro caminos: bit 0 = anade CRLF,
    // bit 1 = escribe por error estandar. Un unico nodo, un unico nodo MIR, una unica
    // funcion de runtime.
    //
    // Por que no cuatro nodos: lo unico que cambia entre los cuatro es que descriptor se
    // pide y si hay un segundo WriteFile para el salto de linea. Todo lo demas --leer la
    // longitud del string, saltar al offset 8, llamar a WriteFile-- es identico. Cuatro
    // copias de esa logica es exactamente como se separan: se arregla una y las otras tres
    // se quedan atras.
    //
    // La mascara se fija AQUI, en compilacion, no en ejecucion: el llamador escribio
    // `Console.ErrorWrite(...)`, asi que el valor es constante y llega al runtime como un
    // `i32` literal.
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::ConsoleWriteLine ||
         core->kind == MethodCoreOperationKind::ConsoleWrite ||
         core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine ||
         core->kind == MethodCoreOperationKind::ConsoleErrorWrite))
    {
        if (addressContext || expression->children().size() != 1)
        {
            error = "SHR-022 NativeSharp.Core.Console output requires one value argument";
            return nullptr;
        }
        const MethodCallBinding* call = callBinding(expression);
        auto argument = lowerExpression(expression->children().front(), memberOrdinal, false, error);
        if (argument == nullptr) return nullptr;
        if (call != nullptr && !call->selectedConversions.empty() &&
            call->selectedConversions.front().kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), call->selectedConversions.front(), expression->children().front());
        }
        bool hasOwnedTemporary = false;
        if (call != nullptr && call->selectedMemberOrdinal.has_value())
        {
            argument = wrapCallArgumentBorrow(
                std::move(argument), expression->children().front(),
                *call->selectedMemberOrdinal, 0, hasOwnedTemporary);
        }
        const bool appendNewline =
            core->kind == MethodCoreOperationKind::ConsoleWriteLine ||
            core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine;
        const bool useStandardError =
            core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine ||
            core->kind == MethodCoreOperationKind::ConsoleErrorWrite;
        auto node = makeNode(ProjectOopHirNodeKind::CoreConsoleWriteLine, expression, "builtin:void");
        node->integerValue = (appendNewline ? 1u : 0u) | (useStandardError ? 2u : 0u);
        node->children.push_back(std::move(argument));
        return wrapFullExpressionCleanup(std::move(node), expression, hasOwnedTemporary);
    }
    if (core != nullptr && core->kind == MethodCoreOperationKind::ConsoleReadLine)
    {
        if (addressContext || expression->children().size() != 1)
        {
            error = "NativeSharp.Core.Console.ReadLine requires one out status argument";
            return nullptr;
        }
        auto status = lowerExpression(
            expression->children().front(), memberOrdinal, false, error);
        if (status == nullptr) return nullptr;
        auto node = makeNode(
            ProjectOopHirNodeKind::CoreConsoleReadLine, expression, "builtin:string");
        node->children.push_back(std::move(status));
        return node;
    }
    // ID 129. `ToString()` sobre un ENUM devuelve el NOMBRE del enumerador.
    //
    // POR QUE UNA CADENA DE CONDICIONALES Y NO UN NODO NUEVO. La respuesta depende del
    // VALOR, asi que no es una constante como la del struct de abajo; pero tampoco hace
    // falta inventar un nodo, ni una tabla en el binario, ni una operacion del runtime.
    // `v == A ? "A" : (v == B ? "B" : ...)` se construye con nodos que YA existen, baja a
    // control de flujo ordinario, y el optimizador la pliega entera cuando el valor es
    // constante -- que es el caso comun. Cero sitios nuevos en la cadena del doc 100:
    // ni MIR, ni LLVM, ni runtime, ni imports.
    //
    // EL FONDO DE LA CADENA es el nombre del TIPO, no una cadena vacia ni el numero. Un
    // valor que no es ninguno de los enumeradores existe --nada impide `(Color)99`-- y
    // devolver "" haria que un diagnostico que lo imprima no dijera nada. C# devuelve el
    // numero; hacerlo aqui exigiria convertir un entero a cadena EN EJECUCION, que es una
    // operacion nucleo mas y una llamada mas por cada ToString. Queda anotado con su
    // condicion de reapertura: cuando alguien necesite ver el numero de un valor fuera de
    // rango, y no le baste con saber de que enum era.
    //
    // EL RECEPTOR SE BAJA UNA VEZ POR COMPARACION, que es la unica limitacion real de
    // esta forma. Por eso se exige que sea RE-EVALUABLE con la misma regla que ya usa la
    // asignacion compuesta sobre propiedades (ID 82): un local, un parametro, `this`, una
    // constante. `Dame().ToString()` se rechaza con un mensaje que dice que hay que
    // ligarlo a un local -- en vez de llamar a `Dame()` una vez por enumerador, que seria
    // un fallo silencioso y caro.
    if (core != nullptr && core->kind == MethodCoreOperationKind::EnumToString)
    {
        if (addressContext)
        {
            error = "ID 129 enum ToString() has no address";
            return nullptr;
        }
        const ast::AstNode* receiverSyntax = expression->first() != nullptr
            ? expression->first()->first() : nullptr;
        if (receiverSyntax == nullptr)
        {
            error = "ID 129 enum ToString() has no receiver";
            return nullptr;
        }
        if (memberModel_ == nullptr || nameLookup_ == nullptr)
        {
            error = "ID 129 enum ToString() needs the member model";
            return nullptr;
        }

        // Los enumeradores del tipo, en el orden en que se declararon.
        std::optional<std::size_t> enumOrdinal;
        if (core->elementTypeKey.starts_with("type:"))
        {
            const std::string_view identity =
                std::string_view(core->elementTypeKey).substr(5);
            for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size();
                 ++ordinal)
            {
                if (nameLookup_->namedTypes()[ordinal].identity.key() == identity)
                {
                    enumOrdinal = ordinal;
                    break;
                }
            }
        }
        if (!enumOrdinal.has_value())
        {
            error = "ID 129 enum ToString() cannot find the enum type";
            return nullptr;
        }

        std::vector<std::pair<std::int64_t, std::string>> enumerators;
        for (const ClassMemberSymbol& member : memberModel_->members())
        {
            if (member.isEnumConstant && member.declaringTypeOrdinal == *enumOrdinal)
            {
                enumerators.emplace_back(member.enumConstantValue, member.name);
            }
        }

        // EL NODO. children[0] es el valor, children[1] la cadena de reserva, y de ahi
        // en adelante PARES (constante, nombre) en orden de declaracion -- que es el
        // orden en que se comparan, y por tanto el que decide quien gana cuando dos
        // enumeradores comparten valor (un alias).
        //
        // EL RECEPTOR SE BAJA UNA SOLA VEZ. La primera version de esto era una cadena de
        // condicionales que lo bajaba una vez POR ENUMERADOR, y para no repetir efectos
        // exigia un receptor re-evaluable: `Dame().ToString()` se rechazaba. Eso era una
        // limitacion del compilador escrita como si fuera una regla del lenguaje.
        auto node = makeNode(ProjectOopHirNodeKind::EnumName, expression, "builtin:string");
        auto receiverNode = lowerExpression(receiverSyntax, memberOrdinal, false, error);
        if (receiverNode == nullptr) return nullptr;
        node->children.push_back(std::move(receiverNode));

        auto fallback =
            makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "builtin:string");
        fallback->stringValue = shortTypeName(core->elementTypeKey);
        node->children.push_back(std::move(fallback));

        for (const auto& enumerator : enumerators)
        {
            auto constant = makeNode(
                ProjectOopHirNodeKind::IntegerConstant, expression, core->elementTypeKey);
            constant->integerValue = static_cast<std::uint64_t>(enumerator.first);
            auto name = makeNode(
                ProjectOopHirNodeKind::StringLiteral, expression, "builtin:string");
            name->stringValue = enumerator.second;
            node->children.push_back(std::move(constant));
            node->children.push_back(std::move(name));
        }
        return node;
    }

    // C8 "type root", struct half. ToString on a struct is a COMPILE-TIME constant -- a
    // struct is resolved on its static type, so the answer is known here -- and therefore
    // lowers to an ordinary string literal rather than to a node of its own.
    if (core != nullptr && core->kind == MethodCoreOperationKind::StructToString)
    {
        if (addressContext)
        {
            error = "C8 struct ToString() has no address";
            return nullptr;
        }
        auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "builtin:string");
        node->stringValue = shortTypeName(core->elementTypeKey);
        return node;
    }
    if (core != nullptr && core->kind == MethodCoreOperationKind::StructClone)
    {
        if (addressContext || !expression->children().empty())
        {
            error = "NativeSharp 3.0 record Clone() received an invalid operand count";
            return nullptr;
        }
        const ast::AstNode* receiverSyntax = expression->first() != nullptr
            ? expression->first()->first() : nullptr;
        if (receiverSyntax == nullptr)
        {
            error = "NativeSharp 3.0 record Clone() has no receiver";
            return nullptr;
        }
        return lowerExpression(receiverSyntax, memberOrdinal, false, error);
    }
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::StructEquals ||
         core->kind == MethodCoreOperationKind::StructHashCode))
    {
        const bool isEquals = core->kind == MethodCoreOperationKind::StructEquals;
        if (addressContext || expression->children().size() != (isEquals ? 1U : 0U))
        {
            error = "C8 struct root member received an invalid operand count";
            return nullptr;
        }
        auto node = makeNode(
            isEquals ? ProjectOopHirNodeKind::AggregateEquals
                     : ProjectOopHirNodeKind::AggregateHash,
            expression,
            core->resultTypeKey);
        node->elementTypeKey = core->elementTypeKey;
        // Operand order: the receiver first, then Equals' argument. argumentCount is what
        // separates them from the leaf descriptors that follow.
        const ast::AstNode* receiverSyntax = expression->first() != nullptr
            ? expression->first()->first() : nullptr;
        if (receiverSyntax == nullptr)
        {
            error = "C8 struct root member has no receiver";
            return nullptr;
        }
        auto receiverNode = lowerExpression(receiverSyntax, memberOrdinal, false, error);
        if (receiverNode == nullptr) return nullptr;
        node->children.push_back(std::move(receiverNode));
        if (isEquals)
        {
            auto argument = lowerExpression(
                expression->children().front(), memberOrdinal, false, error);
            if (argument == nullptr) return nullptr;
            node->children.push_back(std::move(argument));
        }
        node->argumentCount = node->children.size();
        if (!appendAggregateLeaves(core->elementTypeKey, 0, expression, node->children, error))
        {
            return nullptr;
        }
        return node;
    }
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::ClockQueryCounter ||
         core->kind == MethodCoreOperationKind::ClockQueryFrequency ||
         core->kind == MethodCoreOperationKind::ClockQuerySystemFileTime))
    {
        // Stage 53.1 "Time": all three NativeClock raw wrappers take zero arguments and
        // return a plain builtin:long -- no operand lowering, no implicit conversion to
        // wrap, unlike every String core operation above. This is the simplest possible
        // core-operation shape in the whole compiler.
        if (addressContext || expression->children().size() != 0)
        {
            error = "Stage 53.1 NativeSharp.Core.Diagnostics.NativeClock raw operation takes no arguments";
            return nullptr;
        }
        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreClockQueryCounter;
        if (core->kind == MethodCoreOperationKind::ClockQueryFrequency)
        {
            kind = ProjectOopHirNodeKind::CoreClockQueryFrequency;
        }
        else if (core->kind == MethodCoreOperationKind::ClockQuerySystemFileTime)
        {
            kind = ProjectOopHirNodeKind::CoreClockQuerySystemFileTime;
        }
        return makeNode(kind, expression, "builtin:long");
    }
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::EnvironmentTickCount64 ||
         core->kind == MethodCoreOperationKind::EnvironmentProcessorCount ||
         // Stage 53.5 "Command-line arguments": GetCommandLineArgCount joins this same
         // zero-argument bucket -- it takes no operands either, just like TickCount64/
         // ProcessorCount, and returns builtin:int like ProcessorCount does.
         core->kind == MethodCoreOperationKind::EnvironmentGetCommandLineArgCount ||
         // Bloque 4, fase A: CurrentDirectory tambien es de cero argumentos, pero su
         // resultado es builtin:string -- el primer tipo de salida no numerico de este
         // bucket, de ahi el tercer caso en la cadena de abajo.
         core->kind == MethodCoreOperationKind::EnvironmentCurrentDirectory))
    {
        // Stage 53.2 "Environment": same zero-argument shape as the NativeClock trio
        // just above -- the only difference is the result type, "builtin:long" for
        // TickCount64 and "builtin:int" for ProcessorCount (GetActiveProcessorCount
        // returns a 32-bit DWORD in real Win32, so int is the honest NativeSharp type
        // here, not long). Stage 53.5 added a third result type: still builtin:int, so
        // this is a plain if-chain now rather than the original two-way ternary.
        if (addressContext || expression->children().size() != 0)
        {
            error = "Stage 53.2 NativeSharp.Core.Environment raw operation takes no arguments";
            return nullptr;
        }
        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreEnvironmentProcessorCount;
        std::string resultTypeKey = "builtin:int";
        if (core->kind == MethodCoreOperationKind::EnvironmentTickCount64)
        {
            kind = ProjectOopHirNodeKind::CoreEnvironmentTickCount64;
            resultTypeKey = "builtin:long";
        }
        else if (core->kind == MethodCoreOperationKind::EnvironmentGetCommandLineArgCount)
        {
            kind = ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgCount;
        }
        else if (core->kind == MethodCoreOperationKind::EnvironmentCurrentDirectory)
        {
            kind = ProjectOopHirNodeKind::CoreEnvironmentCurrentDirectory;
            resultTypeKey = "builtin:string";
        }
        return makeNode(kind, expression, resultTypeKey);
    }
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw ||
         core->kind == MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw ||
         core->kind == MethodCoreOperationKind::ProcessGetKernelTimeRaw ||
         core->kind == MethodCoreOperationKind::ProcessGetUserTimeRaw ||
         core->kind == MethodCoreOperationKind::ProcessGetPriorityClassRaw))
    {
        // Stage 55.1 "Process": same zero-argument shape as the NativeClock trio above --
        // all five results are builtin:long. The Win32 process HANDLE is obtained inside
        // the runtime wrapper (GetCurrentProcess()), never passed from here.
        if (addressContext || expression->children().size() != 0)
        {
            error = "Stage 55.1 NativeSharp.Core.Diagnostics.Process raw operation takes no arguments";
            return nullptr;
        }
        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreProcessGetCurrentProcessId;
        if (core->kind == MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw)
        {
            kind = ProjectOopHirNodeKind::CoreProcessGetWorkingSetSize;
        }
        else if (core->kind == MethodCoreOperationKind::ProcessGetKernelTimeRaw)
        {
            kind = ProjectOopHirNodeKind::CoreProcessGetKernelTime;
        }
        else if (core->kind == MethodCoreOperationKind::ProcessGetUserTimeRaw)
        {
            kind = ProjectOopHirNodeKind::CoreProcessGetUserTime;
        }
        else if (core->kind == MethodCoreOperationKind::ProcessGetPriorityClassRaw)
        {
            kind = ProjectOopHirNodeKind::CoreProcessGetPriorityClass;
        }
        return makeNode(kind, expression, "builtin:long");
    }
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::SystemInfoGetPageSizeRaw ||
         core->kind == MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw ||
         core->kind == MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw ||
         core->kind == MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw ||
         core->kind == MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw ||
         core->kind == MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw))
    {
        // Stage 55.2 "Hardware / System Info": same zero-argument shape, all six results
        // are builtin:long. Both GetSystemInfo and GlobalMemoryStatusEx fill an in-memory
        // Win32 struct entirely inside the runtime wrapper -- no struct type is ever
        // visible up here.
        if (addressContext || expression->children().size() != 0)
        {
            error = "Stage 55.2 NativeSharp.Core.Diagnostics.SystemInfo raw operation takes no arguments";
            return nullptr;
        }
        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreSystemInfoGetPageSize;
        if (core->kind == MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw)
        {
            kind = ProjectOopHirNodeKind::CoreSystemInfoGetProcessorArchitecture;
        }
        else if (core->kind == MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw)
        {
            kind = ProjectOopHirNodeKind::CoreSystemInfoGetAllocationGranularity;
        }
        else if (core->kind == MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw)
        {
            kind = ProjectOopHirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes;
        }
        else if (core->kind == MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw)
        {
            kind = ProjectOopHirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes;
        }
        else if (core->kind == MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw)
        {
            kind = ProjectOopHirNodeKind::CoreSystemInfoGetMemoryLoadPercent;
        }
        return makeNode(kind, expression, "builtin:long");
    }
    if (core != nullptr &&
        (core->kind == MethodCoreOperationKind::DebugIsDebuggerPresentRaw ||
         core->kind == MethodCoreOperationKind::DebugCaptureStackFrameCountRaw))
    {
        // Stage 55.3 "Diagnostics": same zero-argument shape, both results are
        // builtin:long (IsDebuggerPresentRaw's Win32 0/1 becomes a real bool in Debug.ns).
        if (addressContext || expression->children().size() != 0)
        {
            error = "Stage 55.3 NativeSharp.Core.Diagnostics.Debug raw operation takes no arguments";
            return nullptr;
        }
        ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreDebugIsDebuggerPresent;
        if (core->kind == MethodCoreOperationKind::DebugCaptureStackFrameCountRaw)
        {
            kind = ProjectOopHirNodeKind::CoreDebugCaptureStackFrameCount;
        }
        return makeNode(kind, expression, "builtin:long");
    }
    if (core != nullptr && core->kind == MethodCoreOperationKind::EnvironmentExit)
    {
        // Stage 53.2 "Environment": same one-argument/void shape as
        // Console.WriteLine above, just with a builtin:int argument instead of a
        // builtin:string one -- no string layout involved, the argument travels as a
        // plain i32 value all the way down to ExitProcess's own exit-code parameter.
        if (addressContext || expression->children().size() != 1)
        {
            error = "Stage 53.2 NativeSharp.Core.Environment.Exit requires one int argument";
            return nullptr;
        }
        const MethodCallBinding* call = callBinding(expression);
        auto argument = lowerExpression(expression->children().front(), memberOrdinal, false, error);
        if (argument == nullptr) return nullptr;
        if (call != nullptr && !call->selectedConversions.empty() &&
            call->selectedConversions.front().kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), call->selectedConversions.front(), expression->children().front());
        }
        auto node = makeNode(ProjectOopHirNodeKind::CoreEnvironmentExit, expression, "builtin:void");
        node->children.push_back(std::move(argument));
        return node;
    }
    const MethodCallBinding* binding = callBinding(expression);
    if (binding == nullptr || !binding->selectedMemberOrdinal.has_value())
    {
        error = "Stage 34 requires a resolved Stage 27 method call";
        return nullptr;
    }
    std::size_t selectedMemberOrdinal = *binding->selectedMemberOrdinal;
    const ast::AstNode* receiverSyntax = binding->receiverSyntax;
    std::string_view receiverTypeKey = binding->receiverTypeKey;
    bool interfaceDispatch = binding->interfaceCall;
    if (interfaceDispatch && !binding->forwardedReceiver && interfaceModel_ != nullptr &&
        memberModel_ != nullptr && binding->receiverTypeOrdinal.has_value() &&
        receiverSyntax != nullptr &&
        (receiverSyntax->kind() == ast::AstNodeKind::CastExpression ||
         receiverSyntax->kind() == ast::AstNodeKind::SafeCastExpression) &&
        receiverSyntax->first() != nullptr &&
        (receiverSyntax->first()->kind() == ast::AstNodeKind::NewExpression ||
         receiverSyntax->first()->kind() == ast::AstNodeKind::ObjectCreationExpression))
    {
        const MethodExpressionTypeBinding* concreteType = expressionType(receiverSyntax->first());
        const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
            ? interfaceModel_->typeOrdinalForTypeKey(concreteType->typeKey)
            : std::nullopt;
        const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
            ? interfaceModel_->tablePlan(*concreteTypeOrdinal, *binding->receiverTypeOrdinal)
            : nullptr;
        if (plan != nullptr)
        {
            const auto contract = std::find(
                plan->interfaceMethodMemberOrdinals.begin(),
                plan->interfaceMethodMemberOrdinals.end(),
                selectedMemberOrdinal);
            if (contract != plan->interfaceMethodMemberOrdinals.end())
            {
                const std::size_t slot = static_cast<std::size_t>(
                    contract - plan->interfaceMethodMemberOrdinals.begin());
                if (slot < plan->implementationMemberOrdinals.size() &&
                    plan->implementationMemberOrdinals[slot].has_value())
                {
                    selectedMemberOrdinal = *plan->implementationMemberOrdinals[slot];
                    receiverSyntax = receiverSyntax->first();
                    receiverTypeKey = concreteType->typeKey;
                    interfaceDispatch = false;
                }
            }
        }
    }
    auto node = makeNode(
        binding->staticCall ? ProjectOopHirNodeKind::StaticCall : ProjectOopHirNodeKind::InstanceCall,
        expression,
        binding->resultTypeKey);
    const bool writtenNullConditional = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression;
    const MethodExpressionTypeBinding* receiverFlow = expressionType(receiverSyntax);
    const bool provenNonNull = receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
    node->nullConditionalCall = writtenNullConditional && !provenNonNull;
    if (writtenNullConditional && provenNonNull) node->stringValue = "flow-nonnull";
    const MethodExpressionTypeBinding* callType = expressionType(expression);
    node->booleanValue = callType != nullptr && callType->flowActiveVariantCase.has_value();
    node->memberOrdinal = selectedMemberOrdinal;
    node->baseReceiver = binding->baseReceiver;
    node->interfaceDispatch = interfaceDispatch;
    if (interfaceDispatch)
    {
        node->targetTypeOrdinal = binding->receiverTypeOrdinal;
    }
    std::size_t conversionOffset = 0;
    if (binding->forwardedReceiver)
    {
        auto receiver = lowerExpression(binding->receiverSyntax, memberOrdinal, false, error);
        if (receiver == nullptr) return nullptr;
        if (!binding->selectedConversions.empty() &&
            binding->selectedConversions.front().kind != ProjectConversionKind::Identity)
        {
            receiver = wrapImplicitConversion(
                std::move(receiver), binding->selectedConversions.front(), binding->receiverSyntax);
        }
        node->children.push_back(std::move(receiver));
        node->argumentParameterOrdinals.push_back(0U);
        conversionOffset = 1;
    }
    else if (!binding->staticCall)
    {
        auto receiver = lowerReceiver(
            receiverSyntax, receiverTypeKey, memberOrdinal, error);
        if (receiver == nullptr)
        {
            return nullptr;
        }
        node->children.push_back(std::move(receiver));
    }
    if (memberModel_ == nullptr ||
        selectedMemberOrdinal >= memberModel_->members().size())
    {
        error = "NSC-LIMIT-020 selected call has no callable metadata";
        return nullptr;
    }
    const ClassMemberSymbol& selectedMember =
        memberModel_->members()[selectedMemberOrdinal];
    bool hasOwnedTemporary = false;
    ProjectOopHirNode* expandedPack = nullptr;
    for (std::size_t index = 0; index < expression->children().size(); ++index)
    {
        auto argument = lowerExpression(expression->children()[index], memberOrdinal, false, error);
        if (argument == nullptr)
        {
            return nullptr;
        }
        const std::size_t selectedIndex = index + conversionOffset;
        if (selectedIndex >= binding->selectedArgumentParameterOrdinals.size())
        {
            error = "named call argument has no selected parameter ordinal";
            return nullptr;
        }
        const std::size_t parameterIndex =
            binding->selectedArgumentParameterOrdinals[selectedIndex];
        if (selectedIndex < binding->selectedConversions.size() &&
            binding->selectedConversions[selectedIndex].kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), binding->selectedConversions[selectedIndex], expression->children()[index]);
        }
        const ast::AstNode* argumentSyntax = expression->children()[index];
        const bool packElement =
            selectedIndex < binding->selectedParameterPackElements.size() &&
            binding->selectedParameterPackElements[selectedIndex];
        if (packElement)
        {
            if (parameterIndex >= selectedMember.parameters.size())
            {
                error = "T01-F02 method parameter pack has no selected parameter";
                return nullptr;
            }
            const std::string& elementType =
                selectedMember.parameters[parameterIndex].parameterPackElementTypeKey;
            if (elementType.empty())
            {
                error = "T01-F02 method parameter pack lost its element type";
                return nullptr;
            }
            if (expandedPack == nullptr)
            {
                auto pack = makeNode(
                    ProjectOopHirNodeKind::ParameterPack,
                    argumentSyntax,
                    selectedMember.parameters[parameterIndex].typeKey);
                pack->elementTypeKey = elementType;
                expandedPack = pack.get();
                node->children.push_back(std::move(pack));
                node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            expandedPack->children.push_back(std::move(argument));
            continue;
        }
        if (memberModel_ != nullptr && selectedMemberOrdinal < memberModel_->members().size() &&
            parameterIndex < memberModel_->members()[selectedMemberOrdinal].parameters.size())
        {
            argument = wrapCallArgumentBorrow(
                std::move(argument), argumentSyntax, selectedMemberOrdinal,
                parameterIndex, hasOwnedTemporary);
        }
        node->children.push_back(std::move(argument));
        node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    for (std::size_t index = 0; index < binding->selectedDefaultArguments.size(); ++index)
    {
        if (index >= binding->selectedDefaultArgumentParameterOrdinals.size())
        {
            error = "selected default argument has no parameter ordinal";
            return nullptr;
        }
        const std::size_t parameterIndex =
            binding->selectedDefaultArgumentParameterOrdinals[index];
        if (parameterIndex >= selectedMember.parameters.size())
        {
            error = "NSC-LIMIT-020 selected default argument has no matching parameter";
            return nullptr;
        }
        auto argument = lowerDefaultArgument(
            binding->selectedDefaultArguments[index],
            selectedMember.parameters[parameterIndex].typeKey,
            error);
        if (argument == nullptr) return nullptr;
        node->children.push_back(std::move(argument));
        node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    if (binding->selectedSynthesizesEmptyParameterPack)
    {
        if (!binding->selectedParameterPackOrdinal.has_value() ||
            *binding->selectedParameterPackOrdinal >= selectedMember.parameters.size())
        {
            error = "T01-F02 empty method parameter pack has no parameter";
            return nullptr;
        }
        const std::size_t parameterIndex = *binding->selectedParameterPackOrdinal;
        const std::string& elementType =
            selectedMember.parameters[parameterIndex].parameterPackElementTypeKey;
        if (elementType.empty())
        {
            error = "T01-F02 empty method parameter pack lost its element type";
            return nullptr;
        }
        auto pack = makeNode(
            ProjectOopHirNodeKind::ParameterPack,
            expression,
            selectedMember.parameters[parameterIndex].typeKey);
        pack->elementTypeKey = elementType;
        node->children.push_back(std::move(pack));
        node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    if (addressContext && !selectedMember.isReferenceReturn)
    {
        error = "call result is not addressable unless the selected method returns ref";
        return nullptr;
    }
    node->referenceResult = selectedMember.isReferenceReturn;
    auto loweredCall = wrapFullExpressionCleanup(std::move(node), expression, hasOwnedTemporary);
    if (!selectedMember.isReferenceReturn || addressContext) return loweredCall;
    auto load = makeNode(
        ProjectOopHirNodeKind::PointerLoad, expression, selectedMember.declaredTypeKey);
    load->children.push_back(std::move(loweredCall));
    return load;
}
}
