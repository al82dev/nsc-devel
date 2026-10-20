#include "HirLoweringSupport.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using StringCallLowering = HirCoreOperandCallPlan;

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup02(
    MethodCoreOperationKind kind);
std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup03(
    MethodCoreOperationKind kind);
std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup04(
    MethodCoreOperationKind kind);

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup01(
    MethodCoreOperationKind kind)
{
    switch (kind)
    {
    // ---------------------------------------------------------------------------------------
    // DOS OPERANDOS. Era el valor por omision de la cadena vieja, y por eso era la trampa: una
    // operacion nueva que se olvidara caia aqui en silencio. Ahora esta escrito.
    // ---------------------------------------------------------------------------------------
    case MethodCoreOperationKind::StringConcat:
        return HirCoreOperandCallPlan{2U, ProjectOopHirNodeKind::StringConcat};
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
    default:
        return hirCoreOperandCallPlanGroup02(kind);
    }

    // Inalcanzable: el switch de arriba cubre todos los enumeradores. Existe solo para callar el
    // aviso de "control reaches end of non-void function", que no entiende esa garantia.
    return std::nullopt;
}

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup02(
    MethodCoreOperationKind kind)
{
    switch (kind)
    {
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
    default:
        return hirCoreOperandCallPlanGroup03(kind);
    }

    // Inalcanzable: el switch de arriba cubre todos los enumeradores. Existe solo para callar el
    // aviso de "control reaches end of non-void function", que no entiende esa garantia.
    return std::nullopt;
}

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup03(
    MethodCoreOperationKind kind)
{
    switch (kind)
    {
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
    default:
        return hirCoreOperandCallPlanGroup04(kind);
    }

    // Inalcanzable: el switch de arriba cubre todos los enumeradores. Existe solo para callar el
    // aviso de "control reaches end of non-void function", que no entiende esa garantia.
    return std::nullopt;
}

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanGroup04(
    MethodCoreOperationKind kind)
{
    switch (kind)
    {
        // The earlier groups own these plans. Re-listing their labels here keeps this final
        // switch exhaustive without a default, so a new MethodCoreOperationKind still fails
        // compilation until its lowering route is selected.
        case MethodCoreOperationKind::BitByteSwap:
        case MethodCoreOperationKind::BitLeadingZeroCount:
        case MethodCoreOperationKind::BitPopulationCount:
        case MethodCoreOperationKind::BitRotateLeft:
        case MethodCoreOperationKind::BitRotateRight:
        case MethodCoreOperationKind::BitTrailingZeroCount:
        case MethodCoreOperationKind::CpuPause:
        case MethodCoreOperationKind::CpuPrefetchRead:
        case MethodCoreOperationKind::CpuReadTimestamp:
        case MethodCoreOperationKind::DebugOutputStringRaw:
        case MethodCoreOperationKind::DirectoryCreate:
        case MethodCoreOperationKind::DirectoryDelete:
        case MethodCoreOperationKind::DirectoryExists:
        case MethodCoreOperationKind::DirectoryGetDirectories:
        case MethodCoreOperationKind::DirectoryGetDirectoryCount:
        case MethodCoreOperationKind::DirectoryGetFileCount:
        case MethodCoreOperationKind::DirectoryGetFiles:
        case MethodCoreOperationKind::DirectoryMoveRaw:
        case MethodCoreOperationKind::EnvironmentGetCommandLineArgs:
        case MethodCoreOperationKind::EnvironmentGetVariable:
        case MethodCoreOperationKind::EnvironmentSetVariable:
        case MethodCoreOperationKind::EventClose:
        case MethodCoreOperationKind::EventCreate:
        case MethodCoreOperationKind::EventCreateNamed:
        case MethodCoreOperationKind::EventReset:
        case MethodCoreOperationKind::EventSet:
        case MethodCoreOperationKind::EventWait:
        case MethodCoreOperationKind::FileAppendAllText:
        case MethodCoreOperationKind::FileCopyRaw:
        case MethodCoreOperationKind::FileDelete:
        case MethodCoreOperationKind::FileExists:
        case MethodCoreOperationKind::FileGetSize:
        case MethodCoreOperationKind::FileMoveRaw:
        case MethodCoreOperationKind::FileReadAllBytes:
        case MethodCoreOperationKind::FileReadAllText:
        case MethodCoreOperationKind::FileReadBytes:
        case MethodCoreOperationKind::FileWriteAllBytes:
        case MethodCoreOperationKind::FileWriteAllText:
        case MethodCoreOperationKind::HalfFromFloat:
        case MethodCoreOperationKind::HalfToFloat:
        case MethodCoreOperationKind::MutexClose:
        case MethodCoreOperationKind::MutexCreate:
        case MethodCoreOperationKind::MutexCreateNamed:
        case MethodCoreOperationKind::MutexLock:
        case MethodCoreOperationKind::MutexUnlock:
        case MethodCoreOperationKind::ObjectReferenceHash:
        case MethodCoreOperationKind::ObjectTypeName:
        case MethodCoreOperationKind::PathGetFullPath:
        case MethodCoreOperationKind::ProcessRun:
        case MethodCoreOperationKind::ProcessSetPriorityClassRaw:
        case MethodCoreOperationKind::SemaphoreClose:
        case MethodCoreOperationKind::SemaphoreCreate:
        case MethodCoreOperationKind::SemaphoreCreateNamed:
        case MethodCoreOperationKind::SemaphoreRelease:
        case MethodCoreOperationKind::SemaphoreWait:
        case MethodCoreOperationKind::SimdFloat256Add:
        case MethodCoreOperationKind::SimdFloat256Multiply:
        case MethodCoreOperationKind::SimdFloat256Subtract:
        case MethodCoreOperationKind::SimdFloat32x4Add:
        case MethodCoreOperationKind::SimdFloat32x4Multiply:
        case MethodCoreOperationKind::SimdFloat32x4Subtract:
        case MethodCoreOperationKind::SimdFusedMultiplyAdd:
        case MethodCoreOperationKind::SimdInt128Add:
        case MethodCoreOperationKind::SimdInt128Multiply:
        case MethodCoreOperationKind::SimdInt128Subtract:
        case MethodCoreOperationKind::SimdInt256Add:
        case MethodCoreOperationKind::SimdInt256Multiply:
        case MethodCoreOperationKind::SimdInt256Subtract:
        case MethodCoreOperationKind::SimdLoadAligned128:
        case MethodCoreOperationKind::SimdLoadUnaligned128:
        case MethodCoreOperationKind::SimdReciprocalSqrt:
        case MethodCoreOperationKind::SimdShuffleLowHigh:
        case MethodCoreOperationKind::SimdSqrt:
        case MethodCoreOperationKind::SimdStoreAligned128:
        case MethodCoreOperationKind::SimdStoreUnaligned128:
        case MethodCoreOperationKind::SimdSwizzleWzyx:
        case MethodCoreOperationKind::StringCharacterPointer:
        case MethodCoreOperationKind::StringConcat:
        case MethodCoreOperationKind::StringEquals:
        case MethodCoreOperationKind::StringFromCharArray:
        case MethodCoreOperationKind::StringSubstring:
        case MethodCoreOperationKind::ThreadCreate:
        case MethodCoreOperationKind::ThreadDetach:
        case MethodCoreOperationKind::ThreadJoin:
        case MethodCoreOperationKind::ThreadSleepRaw:
            return hirCoreOperandCallPlanGroup01(kind);
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

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlanImpl(
    MethodCoreOperationKind kind)
{
    return hirCoreOperandCallPlanGroup01(kind);
}

}

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlan(
    MethodCoreOperationKind kind)
{
    return hirCoreOperandCallPlanImpl(kind);
}

std::optional<HirAtomicCallPlan> hirAtomicCallPlan(MethodCoreOperationKind kind)
{
    if (kind == MethodCoreOperationKind::AtomicLoadInt32)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreAtomicLoadInt32};
    if (kind == MethodCoreOperationKind::AtomicStoreInt32)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreAtomicStoreInt32};
    if (kind == MethodCoreOperationKind::InterlockedIncrementInt32)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreInterlockedIncrementInt32};
    if (kind == MethodCoreOperationKind::InterlockedDecrementInt32)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreInterlockedDecrementInt32};
    if (kind == MethodCoreOperationKind::InterlockedAddInt32)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreInterlockedAddInt32};
    if (kind == MethodCoreOperationKind::InterlockedExchangeInt32)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreInterlockedExchangeInt32};
    if (kind == MethodCoreOperationKind::InterlockedCompareExchangeInt32)
        return HirAtomicCallPlan{2U, ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt32};
    if (kind == MethodCoreOperationKind::AtomicLoadInt64)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreAtomicLoadInt64};
    if (kind == MethodCoreOperationKind::AtomicStoreInt64)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreAtomicStoreInt64};
    if (kind == MethodCoreOperationKind::InterlockedIncrementInt64)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreInterlockedIncrementInt64};
    if (kind == MethodCoreOperationKind::InterlockedDecrementInt64)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreInterlockedDecrementInt64};
    if (kind == MethodCoreOperationKind::InterlockedAddInt64)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreInterlockedAddInt64};
    if (kind == MethodCoreOperationKind::InterlockedExchangeInt64)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreInterlockedExchangeInt64};
    if (kind == MethodCoreOperationKind::InterlockedCompareExchangeInt64)
        return HirAtomicCallPlan{2U, ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt64};
    if (kind == MethodCoreOperationKind::AtomicLoadPointer)
        return HirAtomicCallPlan{0U, ProjectOopHirNodeKind::CoreAtomicLoadPointer};
    if (kind == MethodCoreOperationKind::AtomicStorePointer)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreAtomicStorePointer};
    if (kind == MethodCoreOperationKind::InterlockedExchangePointer)
        return HirAtomicCallPlan{1U, ProjectOopHirNodeKind::CoreInterlockedExchangePointer};
    if (kind == MethodCoreOperationKind::InterlockedCompareExchangePointer)
        return HirAtomicCallPlan{2U, ProjectOopHirNodeKind::CoreInterlockedCompareExchangePointer};
    return std::nullopt;
}

std::optional<HirMonitorCallPlan> hirMonitorCallPlan(MethodCoreOperationKind kind)
{
    if (kind == MethodCoreOperationKind::MonitorEnter)
        return HirMonitorCallPlan{ProjectOopHirNodeKind::CoreMonitorEnter, false};
    if (kind == MethodCoreOperationKind::MonitorExit)
        return HirMonitorCallPlan{ProjectOopHirNodeKind::CoreMonitorExit, false};
    if (kind == MethodCoreOperationKind::MonitorWait)
        return HirMonitorCallPlan{ProjectOopHirNodeKind::CoreMonitorWait, true};
    if (kind == MethodCoreOperationKind::MonitorPulse)
        return HirMonitorCallPlan{ProjectOopHirNodeKind::CoreMonitorPulse, false};
    if (kind == MethodCoreOperationKind::MonitorPulseAll)
        return HirMonitorCallPlan{ProjectOopHirNodeKind::CoreMonitorPulseAll, false};
    return std::nullopt;
}

namespace
{
bool isAssertionCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::Assert ||
        kind == MethodCoreOperationKind::Unreachable ||
        kind == MethodCoreOperationKind::Assume ||
        kind == MethodCoreOperationKind::Panic;
}

bool isIteratorCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::IteratorMoveNext ||
        kind == MethodCoreOperationKind::IteratorGetEnumerator ||
        kind == MethodCoreOperationKind::IteratorDrop ||
        kind == MethodCoreOperationKind::IteratorMoveNextAsync ||
        kind == MethodCoreOperationKind::IteratorCancel;
}

bool isViewCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::StringViewFromString ||
        kind == MethodCoreOperationKind::SpanFromArray ||
        kind == MethodCoreOperationKind::ReadOnlySpanFromArray ||
        kind == MethodCoreOperationKind::SpanFromFixedArray ||
        kind == MethodCoreOperationKind::ReadOnlySpanFromFixedArray;
}

bool isAtomicFenceCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::AtomicThreadFence ||
        kind == MethodCoreOperationKind::AtomicCompilerFence;
}

bool isConsoleCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::ConsoleWriteLine ||
        kind == MethodCoreOperationKind::ConsoleWrite ||
        kind == MethodCoreOperationKind::ConsoleErrorWriteLine ||
        kind == MethodCoreOperationKind::ConsoleErrorWrite ||
        kind == MethodCoreOperationKind::ConsoleReadLine;
}

bool isStructCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::StructToString ||
        kind == MethodCoreOperationKind::StructClone ||
        kind == MethodCoreOperationKind::StructEquals ||
        kind == MethodCoreOperationKind::StructHashCode;
}

bool isClockCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::ClockQueryCounter ||
        kind == MethodCoreOperationKind::ClockQueryFrequency ||
        kind == MethodCoreOperationKind::ClockQuerySystemFileTime;
}

bool isEnvironmentQueryCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::EnvironmentTickCount64 ||
        kind == MethodCoreOperationKind::EnvironmentProcessorCount ||
        // Stage 53.5 "Command-line arguments": GetCommandLineArgCount joins this same
        // zero-argument bucket -- it takes no operands either, just like TickCount64/
        // ProcessorCount, and returns builtin:int like ProcessorCount does.
        kind == MethodCoreOperationKind::EnvironmentGetCommandLineArgCount ||
        // Bloque 4, fase A: CurrentDirectory tambien es de cero argumentos, pero su
        // resultado es builtin:string -- el primer tipo de salida no numerico de este
        // bucket, de ahi el tercer caso en la cadena de abajo.
        kind == MethodCoreOperationKind::EnvironmentCurrentDirectory;
}

bool isProcessQueryCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw ||
        kind == MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw ||
        kind == MethodCoreOperationKind::ProcessGetKernelTimeRaw ||
        kind == MethodCoreOperationKind::ProcessGetUserTimeRaw ||
        kind == MethodCoreOperationKind::ProcessGetPriorityClassRaw;
}

bool isSystemInfoCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::SystemInfoGetPageSizeRaw ||
        kind == MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw ||
        kind == MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw ||
        kind == MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw ||
        kind == MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw ||
        kind == MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw;
}

bool isDebugCoreCall(MethodCoreOperationKind kind) noexcept
{
    return kind == MethodCoreOperationKind::DebugIsDebuggerPresentRaw ||
        kind == MethodCoreOperationKind::DebugCaptureStackFrameCountRaw;
}
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEventBackingField(
    [[maybe_unused]] const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error,
    const MethodEventBinding* binding,
    bool address,
    std::unique_ptr<ProjectOopHirNode> preloweredReceiver)
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
        auto receiver = std::move(preloweredReceiver);
        if (receiver == nullptr)
            receiver = lowerReceiver(
                binding->receiverSyntax, binding->receiverTypeKey, memberOrdinal, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
    }
    return node;
}

ProjectOopHirModel::BoundCallTarget ProjectOopHirModel::resolveBoundCallTarget(
    const MethodCallBinding& binding) const noexcept
{
    BoundCallTarget target{
        *binding.selectedMemberOrdinal,
        binding.receiverSyntax,
        binding.receiverTypeKey,
        binding.interfaceCall};
    // A cast of a fresh concrete object has one exact dynamic type. Preserve the historical
    // interface-slot devirtualization before an iterative frame chooses which receiver to visit.
    if (!target.interfaceDispatch || interfaceModel_ == nullptr || memberModel_ == nullptr ||
        !binding.receiverTypeOrdinal.has_value() || target.receiverSyntax == nullptr ||
        (target.receiverSyntax->kind() != ast::AstNodeKind::CastExpression &&
         target.receiverSyntax->kind() != ast::AstNodeKind::SafeCastExpression) ||
        target.receiverSyntax->first() == nullptr ||
        (target.receiverSyntax->first()->kind() != ast::AstNodeKind::NewExpression &&
         target.receiverSyntax->first()->kind() != ast::AstNodeKind::ObjectCreationExpression))
    {
        return target;
    }

    const ast::AstNode* concreteReceiver = target.receiverSyntax->first();
    const MethodExpressionTypeBinding* concreteType = expressionType(concreteReceiver);
    const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
        ? interfaceModel_->typeOrdinalForTypeKey(concreteType->typeKey)
        : std::nullopt;
    const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
        ? interfaceModel_->tablePlan(*concreteTypeOrdinal, *binding.receiverTypeOrdinal)
        : nullptr;
    if (plan == nullptr) return target;

    const auto contract = std::find(
        plan->interfaceMethodMemberOrdinals.begin(),
        plan->interfaceMethodMemberOrdinals.end(),
        target.memberOrdinal);
    if (contract == plan->interfaceMethodMemberOrdinals.end()) return target;
    const std::size_t slot = static_cast<std::size_t>(
        contract - plan->interfaceMethodMemberOrdinals.begin());
    if (slot >= plan->implementationMemberOrdinals.size() ||
        !plan->implementationMemberOrdinals[slot].has_value())
    {
        return target;
    }
    target.memberOrdinal = *plan->implementationMemberOrdinals[slot];
    target.receiverSyntax = concreteReceiver;
    target.receiverTypeKey = concreteType->typeKey;
    target.interfaceDispatch = false;
    return target;
}

bool ProjectOopHirModel::boundCallArgumentNeedsAddress(
    std::size_t selectedMemberOrdinal,
    std::size_t argumentIndex) const noexcept
{
    const ClassMemberSymbol* target = callableSymbol(selectedMemberOrdinal);
    return target != nullptr && argumentIndex < target->parameters.size() &&
        target->parameters[argumentIndex].passKind != ParameterPassKind::Value;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::prepareBoundCallArgument(
    const MethodCallBinding& binding,
    std::size_t selectedMemberOrdinal,
    std::size_t argumentIndex,
    const ast::AstNode* argumentSyntax,
    std::unique_ptr<ProjectOopHirNode> argument,
    bool& hasOwnedTemporary)
{
    if (argumentIndex < binding.selectedConversions.size() &&
        binding.selectedConversions[argumentIndex].kind != ProjectConversionKind::Identity)
    {
        argument = wrapImplicitConversion(
            std::move(argument), binding.selectedConversions[argumentIndex], argumentSyntax);
    }
    return wrapCallArgumentBorrow(
        std::move(argument), argumentSyntax, selectedMemberOrdinal,
        argumentIndex, hasOwnedTemporary);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishBoundCall(
    const ast::AstNode* expression,
    bool addressContext,
    std::string& error,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding& binding,
    const BoundCallTarget& target,
    std::size_t explicitArgumentCount,
    bool nullConditional,
    std::unique_ptr<ProjectOopHirNode> receiver,
    std::vector<std::unique_ptr<ProjectOopHirNode>> arguments,
    bool hasOwnedTemporary)
{
    const std::size_t selectedMemberOrdinal = target.memberOrdinal;
    auto node = makeNode(
        binding.staticCall ? ProjectOopHirNodeKind::StaticCall :
            ProjectOopHirNodeKind::InstanceCall,
        expression,
        binding.resultTypeKey);
    if (binding.staticCall && memberModel_ != nullptr &&
        selectedMemberOrdinal >= memberModel_->members().size())
    {
        node->namespaceFunctionOrdinal = selectedMemberOrdinal - memberModel_->members().size();
        node->memberOrdinal.reset();
    }
    else
    {
        node->memberOrdinal = selectedMemberOrdinal;
    }
    node->baseReceiver = binding.baseReceiver;
    node->interfaceDispatch = target.interfaceDispatch;
    node->nullConditionalCall = nullConditional;
    node->booleanValue = typeBinding != nullptr && typeBinding->flowActiveVariantCase.has_value();
    if (target.interfaceDispatch) node->targetTypeOrdinal = binding.receiverTypeOrdinal;
    if (!binding.staticCall) node->children.push_back(std::move(receiver));
    for (auto& argument : arguments) node->children.push_back(std::move(argument));

    const ClassMemberSymbol* selectedMemberPtr = callableSymbol(selectedMemberOrdinal);
    if (selectedMemberPtr == nullptr)
    {
        error = "NSC-LIMIT-020 selected call has no callable metadata";
        return nullptr;
    }
    const ClassMemberSymbol& selectedMember = *selectedMemberPtr;
    for (std::size_t index = 0; index < binding.selectedDefaultArguments.size(); ++index)
    {
        const std::size_t parameterIndex = explicitArgumentCount + index;
        if (parameterIndex >= selectedMember.parameters.size())
        {
            error = "NSC-LIMIT-020 selected default argument has no matching parameter";
            return nullptr;
        }
        auto argument = lowerDefaultArgument(
            binding.selectedDefaultArguments[index],
            selectedMember.parameters[parameterIndex].typeKey,
            error);
        if (argument == nullptr) return nullptr;
        node->children.push_back(std::move(argument));
    }
    node->referenceResult = selectedMember.isReferenceReturn;
    auto loweredCall = wrapFullExpressionCleanup(
        std::move(node), expression, hasOwnedTemporary);
    if (!selectedMember.isReferenceReturn || addressContext) return loweredCall;
    auto load = makeNode(
        ProjectOopHirNodeKind::PointerLoad, expression, selectedMember.declaredTypeKey);
    load->children.push_back(std::move(loweredCall));
    return load;
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
    bool nullConditional,
    std::unique_ptr<ProjectOopHirNode> preloweredReceiver,
    std::vector<std::unique_ptr<ProjectOopHirNode>>* preloweredArguments)
{
    if (binding == nullptr || binding->resolution != MethodCallResolutionKind::Resolved ||
        !binding->selectedMemberOrdinal.has_value())
    {
        error = "Stage 38 requires a resolved synthetic call binding";
        return nullptr;
    }
    const BoundCallTarget target = resolveBoundCallTarget(*binding);
    const std::size_t selectedMemberOrdinal = target.memberOrdinal;
    std::unique_ptr<ProjectOopHirNode> receiver;
    if (!binding->staticCall)
    {
        receiver = preloweredReceiver != nullptr
            ? std::move(preloweredReceiver)
            : lowerReceiver(
                target.receiverSyntax, target.receiverTypeKey, memberOrdinal, error);
        if (receiver == nullptr) return nullptr;
    }
    bool hasOwnedTemporary = false;
    std::vector<std::unique_ptr<ProjectOopHirNode>> arguments;
    arguments.reserve(argumentSyntaxes.size());
    for (std::size_t index = 0; index < argumentSyntaxes.size(); ++index)
    {
        const ast::AstNode* argumentSyntax = argumentSyntaxes[index];
        auto argument = preloweredArguments != nullptr
            ? std::move((*preloweredArguments)[index])
            : preloweredLast != nullptr && index + 1 == argumentSyntaxes.size()
                ? std::move(preloweredLast)
                : lowerExpression(
                    argumentSyntax,
                    memberOrdinal,
                    boundCallArgumentNeedsAddress(selectedMemberOrdinal, index),
                    error);
        if (argument == nullptr) return nullptr;
        arguments.push_back(prepareBoundCallArgument(
            *binding,
            selectedMemberOrdinal,
            index,
            argumentSyntax,
            std::move(argument),
            hasOwnedTemporary));
    }
    return finishBoundCall(
        expression,
        addressContext,
        error,
        typeBinding,
        *binding,
        target,
        argumentSyntaxes.size(),
        nullConditional,
        std::move(receiver),
        std::move(arguments),
        hasOwnedTemporary);
}

bool ProjectOopHirModel::validateNoHeapDirectCall(
    std::size_t callerMemberOrdinal,
    const MethodCallBinding& binding,
    std::string& error) const
{
    // Recursive lowering used to run this check once for every nested getter. An explicit frame
    // must do the same instead of validating only the outermost expression after unwinding.
    if (memberModel_ == nullptr || callerMemberOrdinal >= memberModel_->members().size())
        return true;
    const ClassMemberSymbol& caller = memberModel_->members()[callerMemberOrdinal];
    if (!sema::EffectContext(caller.effects).allows(sema::Effect::NoHeap) ||
        binding.resolution != MethodCallResolutionKind::Resolved ||
        !binding.selectedMemberOrdinal.has_value() ||
        *binding.selectedMemberOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& callee =
        memberModel_->members()[*binding.selectedMemberOrdinal];
    if (callee.isNoHeap) return true;
    error = "NS3030: noheap callable '" + caller.signatureKey +
        "' cannot call non-noheap callable '" + callee.signatureKey + "'";
    return false;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerPropertyGetterWithReceiver(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error,
    const MethodCallBinding& binding,
    bool nullConditional,
    bool flowProvenNonNull,
    std::unique_ptr<ProjectOopHirNode> receiver)
{
    auto call = lowerBoundCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        expressionType(expression),
        &binding,
        {},
        nullptr,
        nullConditional,
        std::move(receiver));
    if (call == nullptr || !validateNoHeapDirectCall(memberOrdinal, binding, error))
        return nullptr;
    if (flowProvenNonNull) call->stringValue = "flow-nonnull";
    return call;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerIndexerGetterWithOperands(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error,
    const MethodCallBinding& binding,
    const std::vector<const ast::AstNode*>& argumentSyntaxes,
    bool nullConditional,
    std::unique_ptr<ProjectOopHirNode> receiver,
    std::vector<std::unique_ptr<ProjectOopHirNode>> arguments)
{
    auto call = lowerBoundCall(
        expression,
        memberOrdinal,
        addressContext,
        error,
        expressionType(expression),
        &binding,
        argumentSyntaxes,
        nullptr,
        nullConditional,
        std::move(receiver),
        &arguments);
    if (call == nullptr || !validateNoHeapDirectCall(memberOrdinal, binding, error))
        return nullptr;
    return call;
}

std::optional<bool> ProjectOopHirModel::extensionPropertyReceiverAddress(
    const NamespaceFunctionCallBinding& binding) const noexcept
{
    if (binding.resolution != MethodCallResolutionKind::Resolved ||
        !binding.selectedFunctionOrdinal.has_value() || namespaceFunctionModel_ == nullptr ||
        *binding.selectedFunctionOrdinal >= namespaceFunctionModel_->functions().size() ||
        !binding.forwardedReceiver || binding.receiverSyntax == nullptr ||
        binding.selectedArgumentParameterOrdinals.empty())
    {
        return std::nullopt;
    }
    const NamespaceFunctionSymbol& target =
        namespaceFunctionModel_->functions()[*binding.selectedFunctionOrdinal];
    const std::size_t parameterOrdinal = binding.selectedArgumentParameterOrdinals.front();
    if (target.parameters.empty() || parameterOrdinal >= target.parameters.size())
        return std::nullopt;
    return target.parameters[parameterOrdinal].passKind != ParameterPassKind::Value;
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::lowerExtensionPropertyGetterWithReceiver(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    std::string& error,
    const NamespaceFunctionCallBinding& binding,
    std::unique_ptr<ProjectOopHirNode> receiver)
{
    if (binding.resolution != MethodCallResolutionKind::Resolved ||
        !binding.selectedFunctionOrdinal.has_value() || namespaceFunctionModel_ == nullptr ||
        *binding.selectedFunctionOrdinal >= namespaceFunctionModel_->functions().size())
    {
        error = "extension property getter reached HIR without a resolved target";
        return nullptr;
    }
    const std::size_t functionOrdinal = *binding.selectedFunctionOrdinal;
    const NamespaceFunctionSymbol& target =
        namespaceFunctionModel_->functions()[functionOrdinal];
    auto node = makeNode(ProjectOopHirNodeKind::StaticCall, expression, binding.resultTypeKey);
    node->namespaceFunctionOrdinal = functionOrdinal;
    node->stringValue = target.mangledName;
    if (!binding.forwardedReceiver) return node;

    if (binding.receiverSyntax == nullptr || target.parameters.empty() ||
        binding.selectedArgumentParameterOrdinals.empty())
    {
        error = "extension property getter lost its receiver contract";
        return nullptr;
    }
    const std::size_t parameterOrdinal = binding.selectedArgumentParameterOrdinals.front();
    if (parameterOrdinal >= target.parameters.size())
    {
        error = "extension receiver has no matching parameter";
        return nullptr;
    }
    const bool addressReceiver =
        target.parameters[parameterOrdinal].passKind != ParameterPassKind::Value;
    if (receiver == nullptr)
        receiver = lowerExpression(binding.receiverSyntax, memberOrdinal, addressReceiver, error);
    if (receiver == nullptr) return nullptr;
    if (!addressReceiver && !binding.selectedConversions.empty() &&
        binding.selectedConversions.front().kind != ProjectConversionKind::Identity)
    {
        receiver = wrapImplicitConversion(
            std::move(receiver), binding.selectedConversions.front(), binding.receiverSyntax);
    }
    const std::size_t targetCallableOrdinal =
        (memberModel_ != nullptr ? memberModel_->members().size() : 0) + functionOrdinal;
    bool hasOwnedTemporary = false;
    receiver = wrapCallArgumentBorrow(
        std::move(receiver), binding.receiverSyntax,
        targetCallableOrdinal, parameterOrdinal, hasOwnedTemporary);
    node->children.push_back(std::move(receiver));
    node->argumentParameterOrdinals.push_back(parameterOrdinal);
    node->argumentTypeKeys.push_back(target.parameters[parameterOrdinal].typeKey);
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishNamespaceCall(
    const ast::AstNode* expression,
    bool addressContext,
    std::string& error,
    const NamespaceFunctionCallBinding& binding,
    const NamespaceFunctionSymbol& target,
    std::unique_ptr<ProjectOopHirNode> node,
    bool hasOwnedTemporary)
{
    for (std::size_t index = 0; index < binding.selectedDefaultArguments.size(); ++index)
    {
        if (index >= binding.selectedDefaultArgumentParameterOrdinals.size())
        {
            error = "namespace function default argument has no parameter ordinal";
            return nullptr;
        }
        const std::size_t parameterOrdinal =
            binding.selectedDefaultArgumentParameterOrdinals[index];
        if (parameterOrdinal >= target.parameters.size())
        {
            error = "namespace function default argument has no matching parameter";
            return nullptr;
        }
        auto argument = lowerDefaultArgument(
            binding.selectedDefaultArguments[index],
            target.parameters[parameterOrdinal].typeKey,
            error);
        if (argument == nullptr) return nullptr;
        node->children.push_back(std::move(argument));
        node->argumentParameterOrdinals.push_back(parameterOrdinal);
        node->argumentTypeKeys.push_back(target.parameters[parameterOrdinal].typeKey);
    }
    if (binding.selectedSynthesizesEmptyParameterPack)
    {
        if (!binding.selectedParameterPackOrdinal.has_value() ||
            *binding.selectedParameterPackOrdinal >= target.parameters.size())
        {
            error = "T01-F02 empty namespace parameter pack has no parameter";
            return nullptr;
        }
        const std::size_t parameterOrdinal = *binding.selectedParameterPackOrdinal;
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

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishDelegateInvoke(
    const ast::AstNode* expression,
    std::string& error,
    const MethodDelegateBinding& binding,
    std::unique_ptr<ProjectOopHirNode> node)
{
    for (std::size_t index = 0; index < binding.selectedDefaultArguments.size(); ++index)
    {
        const std::string& targetTypeKey =
            index < binding.selectedDefaultArgumentTypeKeys.size()
            ? binding.selectedDefaultArgumentTypeKeys[index] : std::string{};
        auto argument = lowerDefaultArgument(
            binding.selectedDefaultArguments[index], targetTypeKey, error);
        if (argument == nullptr) return nullptr;
        node->argumentTypeKeys.push_back(targetTypeKey);
        node->children.push_back(std::move(argument));
    }
    node->argumentCount =
        expression->children().size() + binding.selectedDefaultArguments.size();
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerAssertionCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    if (addressContext)
    {
        error = std::string(methodCoreOperationKindName(binding.kind)) +
            "() has no addressable result";
        return nullptr;
    }

    const bool hasCondition = binding.kind == MethodCoreOperationKind::Assert ||
        binding.kind == MethodCoreOperationKind::Assume;
    const bool isPanic = binding.kind == MethodCoreOperationKind::Panic;
    const std::size_t expectedArguments = hasCondition ? 1U : 0U;
    const bool invalidArity = isPanic
        ? expression->children().size() > 1U
        : expression->children().size() != expectedArguments;
    const std::string_view expectedType = hasCondition ? "builtin:void" : "builtin:never";
    if (invalidArity || binding.resultTypeKey != expectedType)
    {
        error = "T07-F06 assertion binding lost its arity/result contract";
        return nullptr;
    }

    const ProjectOopHirNodeKind kind = binding.kind == MethodCoreOperationKind::Assert
        ? ProjectOopHirNodeKind::Assert
        : binding.kind == MethodCoreOperationKind::Unreachable
            ? ProjectOopHirNodeKind::Unreachable
            : binding.kind == MethodCoreOperationKind::Assume
                ? ProjectOopHirNodeKind::Assume
                : ProjectOopHirNodeKind::Panic;
    auto node = makeNode(kind, expression, std::string(expectedType));
    if (binding.kind == MethodCoreOperationKind::Panic)
    {
        node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;
        for (std::size_t index = activeHirCleanupRegions_.size(); index > 0; --index)
            node->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
    }
    if (hasCondition || (isPanic && !expression->children().empty()))
    {
        error = "Stage 34 assertion core escaped iterative lowering";
        return nullptr;
    }
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerIteratorCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    if (addressContext || !expression->children().empty() ||
        expression->first() == nullptr || expression->first()->first() == nullptr)
    {
        error = std::string(methodCoreOperationKindName(binding.kind)) +
            "() has an invalid call shape";
        return nullptr;
    }
    error = "Stage 34 iterator core call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerNamespaceFunctionCall(
    const ast::AstNode* expression,
    bool addressContext,
    const NamespaceFunctionCallBinding& binding,
    std::string& error)
{
    if (binding.resolution != MethodCallResolutionKind::Resolved ||
        !binding.selectedFunctionOrdinal.has_value() ||
        namespaceFunctionModel_ == nullptr ||
        *binding.selectedFunctionOrdinal >= namespaceFunctionModel_->functions().size())
    {
        error = "namespace function call reached HIR without a resolved target";
        return nullptr;
    }
    const std::size_t functionOrdinal = *binding.selectedFunctionOrdinal;
    const NamespaceFunctionSymbol& target = namespaceFunctionModel_->functions()[functionOrdinal];
    if (addressContext && !target.isReferenceReturn)
    {
        error = "namespace function call result is not addressable unless it returns ref";
        return nullptr;
    }
    if (binding.forwardedReceiver || !expression->children().empty())
    {
        error = "Stage 34 namespace-call expression escaped iterative lowering";
        return nullptr;
    }
    auto node = makeNode(ProjectOopHirNodeKind::StaticCall, expression, binding.resultTypeKey);
    node->namespaceFunctionOrdinal = functionOrdinal;
    node->stringValue = target.mangledName;
    return finishNamespaceCall(
        expression, addressContext, error, binding, target, std::move(node), false);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEventInvocation(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const MethodEventBinding& binding,
    std::string& error)
{
    if (addressContext)
    {
        error = "Stage 42 event invocation is not addressable";
        return nullptr;
    }
    const bool iterativeReceiver = !binding.staticEvent &&
        binding.receiverSyntax != nullptr &&
        binding.receiverSyntax->kind() != ast::AstNodeKind::BaseExpression;
    if (iterativeReceiver || !expression->children().empty())
    {
        error = "Stage 34 event invocation escaped iterative lowering";
        return nullptr;
    }
    auto head = lowerEventBackingField(expression, memberOrdinal, error, &binding, false);
    if (head == nullptr) return nullptr;
    auto node = makeNode(ProjectOopHirNodeKind::EventInvoke, expression, binding.resultTypeKey);
    node->elementTypeKey = binding.delegateTypeKey;
    node->memberOrdinal = binding.backingMemberOrdinal;
    node->children.push_back(std::move(head));
    node->argumentCount = 0U;
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDelegateInvocation(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodDelegateBinding& binding,
    std::string& error)
{
    if (addressContext)
    {
        error = "Stage 42 delegate invocation is not addressable";
        return nullptr;
    }
    const bool lowersCallee = binding.directLambdaName.empty() ||
        binding.directLambdaUsesEnvironment;
    if (lowersCallee || !expression->children().empty())
    {
        error = "Stage 34 delegate invocation escaped iterative lowering";
        return nullptr;
    }
    auto node = makeNode(ProjectOopHirNodeKind::DelegateInvoke, expression, binding.resultTypeKey);
    node->elementTypeKey = binding.delegateTypeKey;
    node->stringValue = binding.directLambdaName;
    node->booleanValue = binding.directLambdaUsesEnvironment;
    return finishDelegateInvoke(expression, error, binding, std::move(node));
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerViewCoreCall(
    [[maybe_unused]] const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    // NS304-T03-F03: list-pattern rest capture over a native string synthesizes a
    // StringView view directly (no Slice method). Receiver + start + length.
    if (binding.kind == MethodCoreOperationKind::StringViewFromString)
    {
        if (addressContext)
        {
            error = "a StringView rest capture is a value and is not addressable";
            return nullptr;
        }
        error = "Stage 34 StringView core call escaped iterative lowering";
        return nullptr;
    }
    // NS304-T03-F03: same view synthesis over a native/fixed array -> Span<T>/ReadOnlySpan<T>.
    if (addressContext)
    {
        error = "a Span/ReadOnlySpan rest capture is a value and is not addressable";
        return nullptr;
    }
    error = "Stage 34 Span core call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerOperandCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::size_t operandCount,
    ProjectOopHirNodeKind hirKind,
    std::string& error)
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
    const std::size_t expected = operandCount;
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
    if (operandCount != 0U)
    {
        error = "Stage 34 core operand call escaped iterative lowering";
        return nullptr;
    }
    return makeNode(hirKind, expression, binding.resultTypeKey);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerAtomicFenceCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    // Item 307: AtomicFence has one syntactic argument, but that MemoryOrder was folded
    // already and is metadata rather than an executable operand.
    if (addressContext)
    {
        error = "AtomicFence result is not addressable";
        return nullptr;
    }
    if (!binding.atomicMemoryOrder.has_value() || expression->children().size() != 1U)
    {
        error = "AtomicFence requires exactly one validated compile-time MemoryOrder";
        return nullptr;
    }
    const ProjectOopHirNodeKind kind =
        binding.kind == MethodCoreOperationKind::AtomicThreadFence
            ? ProjectOopHirNodeKind::CoreAtomicThreadFence
            : ProjectOopHirNodeKind::CoreAtomicCompilerFence;
    auto node = makeNode(kind, expression, binding.resultTypeKey);
    node->integerValue = static_cast<std::uint64_t>(*binding.atomicMemoryOrder) + 1U;
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::rejectEscapedAtomicCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding&,
    std::string& error)
{
    // Stage 54.2/Item 311 atomics have one of two source-level locations. The wrapper API
    // (AtomicInt32/AtomicInt64/AtomicPointer) supplies a receiver and HIR synthesizes the
    // address of its private `value_` field. The public Interlocked API supplies an
    // explicit first `ref T` argument, whose parsed address-of expression lowers directly
    // to the location pointer. Both forms deliberately converge here on the same core HIR
    // nodes, so MIR and LLVM have one ordering/ABI implementation.
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
    error = "Stage 34 atomic core call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::rejectEscapedMonitorCoreCall(
    const ast::AstNode*,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    // H02S14J owns all five Monitor operations in the iterative expression machine. This
    // residual guard proves that no recursive receiver/timeout lowering can silently return.
    if (addressContext)
    {
        error = binding.kind == MethodCoreOperationKind::MonitorWait
            ? "Stage 54.4 Monitor.Wait result is not addressable"
            : "Stage 54.4 Monitor operation result is not addressable";
        return nullptr;
    }
    error = "Stage 34 monitor core call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerConsoleCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    if (binding.kind == MethodCoreOperationKind::ConsoleReadLine)
    {
        if (addressContext || expression->children().size() != 1)
        {
            error = "NativeSharp.Core.Console.ReadLine requires one out status argument";
            return nullptr;
        }
        error = "Stage 34 console ReadLine call escaped iterative lowering";
        return nullptr;
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
    if (addressContext || expression->children().size() != 1)
    {
        error = "SHR-022 NativeSharp.Core.Console output requires one value argument";
        return nullptr;
    }
    error = "Stage 34 console output call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEnumToStringCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
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
    if (binding.elementTypeKey.starts_with("type:"))
    {
        const std::string_view identity =
            std::string_view(binding.elementTypeKey).substr(5);
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

    error = "Stage 34 enum ToString call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerStructCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    // C8 "type root", struct half. ToString on a struct is a COMPILE-TIME constant -- a
    // struct is resolved on its static type, so the answer is known here -- and therefore
    // lowers to an ordinary string literal rather than to a node of its own.
    if (binding.kind == MethodCoreOperationKind::StructToString)
    {
        if (addressContext)
        {
            error = "C8 struct ToString() has no address";
            return nullptr;
        }
        auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "builtin:string");
        node->stringValue = shortTypeName(binding.elementTypeKey);
        return node;
    }
    if (binding.kind == MethodCoreOperationKind::StructClone)
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
        error = "Stage 34 struct Clone call escaped iterative lowering";
        return nullptr;
    }

    const bool isEquals = binding.kind == MethodCoreOperationKind::StructEquals;
    if (addressContext || expression->children().size() != (isEquals ? 1U : 0U))
    {
        error = "C8 struct root member received an invalid operand count";
        return nullptr;
    }
    // Operand order: the receiver first, then Equals' argument. argumentCount is what
    // separates them from the leaf descriptors that follow.
    const ast::AstNode* receiverSyntax = expression->first() != nullptr
        ? expression->first()->first() : nullptr;
    if (receiverSyntax == nullptr)
    {
        error = "C8 struct root member has no receiver";
        return nullptr;
    }
    error = "Stage 34 struct aggregate call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerClockCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
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
    if (binding.kind == MethodCoreOperationKind::ClockQueryFrequency)
    {
        kind = ProjectOopHirNodeKind::CoreClockQueryFrequency;
    }
    else if (binding.kind == MethodCoreOperationKind::ClockQuerySystemFileTime)
    {
        kind = ProjectOopHirNodeKind::CoreClockQuerySystemFileTime;
    }
    return makeNode(kind, expression, "builtin:long");
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEnvironmentQueryCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
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
    if (binding.kind == MethodCoreOperationKind::EnvironmentTickCount64)
    {
        kind = ProjectOopHirNodeKind::CoreEnvironmentTickCount64;
        resultTypeKey = "builtin:long";
    }
    else if (binding.kind == MethodCoreOperationKind::EnvironmentGetCommandLineArgCount)
    {
        kind = ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgCount;
    }
    else if (binding.kind == MethodCoreOperationKind::EnvironmentCurrentDirectory)
    {
        kind = ProjectOopHirNodeKind::CoreEnvironmentCurrentDirectory;
        resultTypeKey = "builtin:string";
    }
    return makeNode(kind, expression, resultTypeKey);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerProcessQueryCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
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
    if (binding.kind == MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw)
    {
        kind = ProjectOopHirNodeKind::CoreProcessGetWorkingSetSize;
    }
    else if (binding.kind == MethodCoreOperationKind::ProcessGetKernelTimeRaw)
    {
        kind = ProjectOopHirNodeKind::CoreProcessGetKernelTime;
    }
    else if (binding.kind == MethodCoreOperationKind::ProcessGetUserTimeRaw)
    {
        kind = ProjectOopHirNodeKind::CoreProcessGetUserTime;
    }
    else if (binding.kind == MethodCoreOperationKind::ProcessGetPriorityClassRaw)
    {
        kind = ProjectOopHirNodeKind::CoreProcessGetPriorityClass;
    }
    return makeNode(kind, expression, "builtin:long");
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerSystemInfoCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
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
    if (binding.kind == MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw)
    {
        kind = ProjectOopHirNodeKind::CoreSystemInfoGetProcessorArchitecture;
    }
    else if (binding.kind == MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw)
    {
        kind = ProjectOopHirNodeKind::CoreSystemInfoGetAllocationGranularity;
    }
    else if (binding.kind == MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw)
    {
        kind = ProjectOopHirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes;
    }
    else if (binding.kind == MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw)
    {
        kind = ProjectOopHirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes;
    }
    else if (binding.kind == MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw)
    {
        kind = ProjectOopHirNodeKind::CoreSystemInfoGetMemoryLoadPercent;
    }
    return makeNode(kind, expression, "builtin:long");
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDebugCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding& binding,
    std::string& error)
{
    // Stage 55.3 "Diagnostics": same zero-argument shape, both results are
    // builtin:long (IsDebuggerPresentRaw's Win32 0/1 becomes a real bool in Debug.ns).
    if (addressContext || expression->children().size() != 0)
    {
        error = "Stage 55.3 NativeSharp.Core.Diagnostics.Debug raw operation takes no arguments";
        return nullptr;
    }
    ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::CoreDebugIsDebuggerPresent;
    if (binding.kind == MethodCoreOperationKind::DebugCaptureStackFrameCountRaw)
    {
        kind = ProjectOopHirNodeKind::CoreDebugCaptureStackFrameCount;
    }
    return makeNode(kind, expression, "builtin:long");
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerEnvironmentExitCoreCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCoreBinding&,
    std::string& error)
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
    error = "Stage 34 Environment.Exit call escaped iterative lowering";
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerCallExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (const MethodCoreBinding* intrinsic = coreBinding(expression);
        intrinsic != nullptr && isAssertionCoreCall(intrinsic->kind))
    {
        return lowerAssertionCoreCall(expression, addressContext, *intrinsic, error);
    }
    if (const MethodCoreBinding* iteratorCore = coreBinding(expression);
        iteratorCore != nullptr && isIteratorCoreCall(iteratorCore->kind))
    {
        return lowerIteratorCoreCall(expression, addressContext, *iteratorCore, error);
    }
    const NamespaceFunctionCallBinding* namespaceCall =
        namespaceFunctionCallBinding(expression);
    if (namespaceCall != nullptr)
    {
        return lowerNamespaceFunctionCall(expression, addressContext, *namespaceCall, error);
    }
    const MethodEventBinding* stage42Event = eventBinding(expression);
    if (stage42Event != nullptr && stage42Event->kind == MethodEventOperationKind::Invoke)
    {
        return lowerEventInvocation(
            expression, memberOrdinal, addressContext, *stage42Event, error);
    }

    const MethodDelegateBinding* invoke = delegateBinding(expression);
    if (invoke != nullptr && invoke->kind == MethodDelegateOperationKind::Invoke)
    {
        return lowerDelegateInvocation(expression, addressContext, *invoke, error);
    }

    const MethodCoreBinding* core = coreBinding(expression);
    if (core != nullptr && isViewCoreCall(core->kind))
    {
        return lowerViewCoreCall(expression, addressContext, *core, error);
    }
    // Las TRES preguntas --entra en este bloque, con cuantos operandos, y en que nodo
    // HIR se convierte-- salen de la MISMA respuesta: ver hirCoreOperandCallPlan() al
    // principio de este archivo.
    const std::optional<HirCoreOperandCallPlan> coreLowering =
        core != nullptr ? hirCoreOperandCallPlan(core->kind) : std::nullopt;
    if (coreLowering.has_value())
    {
        return lowerOperandCoreCall(
            expression,
            addressContext,
            *core,
            coreLowering->operandCount,
            coreLowering->hirKind,
            error);
    }
    if (core != nullptr && isAtomicFenceCoreCall(core->kind))
    {
        return lowerAtomicFenceCoreCall(expression, addressContext, *core, error);
    }

    if (core != nullptr && hirAtomicCallPlan(core->kind).has_value())
    {
        return rejectEscapedAtomicCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && hirMonitorCallPlan(core->kind).has_value())
    {
        return rejectEscapedMonitorCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && isConsoleCoreCall(core->kind))
    {
        return lowerConsoleCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && core->kind == MethodCoreOperationKind::EnumToString)
    {
        return lowerEnumToStringCoreCall(expression, addressContext, *core, error);
    }

    if (core != nullptr && isStructCoreCall(core->kind))
    {
        return lowerStructCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && isClockCoreCall(core->kind))
    {
        return lowerClockCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && isEnvironmentQueryCoreCall(core->kind))
    {
        return lowerEnvironmentQueryCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && isProcessQueryCoreCall(core->kind))
    {
        return lowerProcessQueryCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && isSystemInfoCoreCall(core->kind))
    {
        return lowerSystemInfoCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && isDebugCoreCall(core->kind))
    {
        return lowerDebugCoreCall(expression, addressContext, *core, error);
    }
    if (core != nullptr && core->kind == MethodCoreOperationKind::EnvironmentExit)
    {
        return lowerEnvironmentExitCoreCall(expression, addressContext, *core, error);
    }
    const MethodCallBinding* binding = callBinding(expression);
    if (binding == nullptr || !binding->selectedMemberOrdinal.has_value())
    {
        error = "Stage 34 requires a resolved Stage 27 method call";
        return nullptr;
    }
    return lowerOrdinaryCall(expression, addressContext, *binding, error);
}

namespace
{
bool ordinaryInterfaceDispatchCandidate(
    const MethodCallBinding& binding,
    const ProjectInterfaceModel* interfaceModel,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* receiverSyntax,
    bool interfaceDispatch)
{
    return interfaceDispatch && !binding.forwardedReceiver && interfaceModel != nullptr &&
        memberModel != nullptr && binding.receiverTypeOrdinal.has_value() &&
        receiverSyntax != nullptr &&
        (receiverSyntax->kind() == ast::AstNodeKind::CastExpression ||
         receiverSyntax->kind() == ast::AstNodeKind::SafeCastExpression) &&
        receiverSyntax->first() != nullptr &&
        (receiverSyntax->first()->kind() == ast::AstNodeKind::NewExpression ||
         receiverSyntax->first()->kind() == ast::AstNodeKind::ObjectCreationExpression);
}

void resolveOrdinaryInterfaceDispatch(
    const MethodCallBinding& binding,
    const ProjectInterfaceModel* interfaceModel,
    const MethodExpressionTypeBinding* concreteType,
    std::size_t& selectedMemberOrdinal,
    const ast::AstNode*& receiverSyntax,
    std::string_view& receiverTypeKey,
    bool& interfaceDispatch)
{
    const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
        ? interfaceModel->typeOrdinalForTypeKey(concreteType->typeKey)
        : std::nullopt;
    const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
        ? interfaceModel->tablePlan(*concreteTypeOrdinal, *binding.receiverTypeOrdinal)
        : nullptr;
    if (plan == nullptr) return;
    const auto contract = std::find(
        plan->interfaceMethodMemberOrdinals.begin(),
        plan->interfaceMethodMemberOrdinals.end(),
        selectedMemberOrdinal);
    if (contract == plan->interfaceMethodMemberOrdinals.end()) return;
    const std::size_t slot = static_cast<std::size_t>(
        contract - plan->interfaceMethodMemberOrdinals.begin());
    if (slot >= plan->implementationMemberOrdinals.size() ||
        !plan->implementationMemberOrdinals[slot].has_value())
        return;
    selectedMemberOrdinal = *plan->implementationMemberOrdinals[slot];
    receiverSyntax = receiverSyntax->first();
    receiverTypeKey = concreteType->typeKey;
    interfaceDispatch = false;
}

void configureOrdinaryNullFlow(
    ProjectOopHirNode& node,
    bool writtenNullConditional,
    bool provenNonNull)
{
    node.nullConditionalCall = writtenNullConditional && !provenNonNull;
    if (writtenNullConditional && provenNonNull) node.stringValue = "flow-nonnull";
}

void configureOrdinaryCallNode(
    ProjectOopHirNode& node,
    const MethodCallBinding& binding,
    std::size_t selectedMemberOrdinal,
    bool interfaceDispatch,
    const MethodExpressionTypeBinding* callType)
{
    node.booleanValue = callType != nullptr && callType->flowActiveVariantCase.has_value();
    node.memberOrdinal = selectedMemberOrdinal;
    node.baseReceiver = binding.baseReceiver;
    node.interfaceDispatch = interfaceDispatch;
    if (interfaceDispatch) node.targetTypeOrdinal = binding.receiverTypeOrdinal;
}

const ClassMemberSymbol* ordinarySelectedMember(
    const ProjectMemberModel* memberModel,
    std::size_t selectedMemberOrdinal,
    std::string& error)
{
    if (memberModel != nullptr && selectedMemberOrdinal < memberModel->members().size())
        return &memberModel->members()[selectedMemberOrdinal];
    error = "NSC-LIMIT-020 selected call has no callable metadata";
    return nullptr;
}

bool resolveEmptyOrdinaryParameterPack(
    const MethodCallBinding& binding,
    const ClassMemberSymbol& selectedMember,
    std::size_t& parameterIndex,
    const std::string*& elementType,
    std::string& error)
{
    if (!binding.selectedParameterPackOrdinal.has_value() ||
        *binding.selectedParameterPackOrdinal >= selectedMember.parameters.size())
    {
        error = "T01-F02 empty method parameter pack has no parameter";
        return false;
    }
    parameterIndex = *binding.selectedParameterPackOrdinal;
    elementType = &selectedMember.parameters[parameterIndex].parameterPackElementTypeKey;
    if (!elementType->empty()) return true;
    error = "T01-F02 empty method parameter pack lost its element type";
    return false;
}

bool resolveOrdinaryDefaultArgument(
    const MethodCallBinding& binding,
    const ClassMemberSymbol& selectedMember,
    std::size_t index,
    std::size_t& parameterIndex,
    std::string_view& parameterTypeKey,
    std::string& error)
{
    if (index >= binding.selectedDefaultArgumentParameterOrdinals.size())
    {
        error = "selected default argument has no parameter ordinal";
        return false;
    }
    parameterIndex = binding.selectedDefaultArgumentParameterOrdinals[index];
    if (parameterIndex >= selectedMember.parameters.size())
    {
        error = "NSC-LIMIT-020 selected default argument has no matching parameter";
        return false;
    }
    parameterTypeKey = selectedMember.parameters[parameterIndex].typeKey;
    return true;
}
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerOrdinaryCall(
    const ast::AstNode* expression,
    bool addressContext,
    const MethodCallBinding& binding,
    std::string& error)
{
    std::size_t selectedMemberOrdinal = *binding.selectedMemberOrdinal;
    const ast::AstNode* receiverSyntax = binding.receiverSyntax;
    std::string_view receiverTypeKey = binding.receiverTypeKey;
    bool interfaceDispatch = binding.interfaceCall;
    if (ordinaryInterfaceDispatchCandidate(
            binding, interfaceModel_, memberModel_, receiverSyntax, interfaceDispatch))
    {
        const MethodExpressionTypeBinding* concreteType = expressionType(receiverSyntax->first());
        resolveOrdinaryInterfaceDispatch(
            binding,
            interfaceModel_,
            concreteType,
            selectedMemberOrdinal,
            receiverSyntax,
            receiverTypeKey,
            interfaceDispatch);
    }
    auto node = makeNode(
        binding.staticCall ? ProjectOopHirNodeKind::StaticCall : ProjectOopHirNodeKind::InstanceCall,
        expression,
        binding.resultTypeKey);
    const bool writtenNullConditional = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression;
    const MethodExpressionTypeBinding* receiverFlow = expressionType(receiverSyntax);
    const bool provenNonNull = receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
    configureOrdinaryNullFlow(*node, writtenNullConditional, provenNonNull);
    const MethodExpressionTypeBinding* callType = expressionType(expression);
    configureOrdinaryCallNode(
        *node,
        binding,
        selectedMemberOrdinal,
        interfaceDispatch,
        callType);
    const bool explicitReceiver = !binding.staticCall && receiverSyntax != nullptr &&
        receiverSyntax->kind() != ast::AstNodeKind::BaseExpression;
    if (binding.forwardedReceiver || explicitReceiver || !expression->children().empty())
    {
        error = "Stage 34 ordinary call escaped iterative lowering";
        return nullptr;
    }
    if (!binding.staticCall)
    {
        auto receiver = lowerImplicitReceiver(receiverSyntax, receiverTypeKey, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
    }
    const ClassMemberSymbol* selectedMember = ordinarySelectedMember(
        memberModel_, selectedMemberOrdinal, error);
    if (selectedMember == nullptr) return nullptr;
    bool hasOwnedTemporary = false;
    if (!appendOrdinaryDefaultArguments(
            *node, expression, binding, *selectedMember, error))
        return nullptr;
    return finishOrdinaryCall(
        std::move(node),
        expression,
        addressContext,
        *selectedMember,
        hasOwnedTemporary,
        error);
}

bool ProjectOopHirModel::appendOrdinaryDefaultArguments(
    ProjectOopHirNode& node,
    const ast::AstNode* expression,
    const MethodCallBinding& binding,
    const ClassMemberSymbol& selectedMember,
    std::string& error)
{
    for (std::size_t index = 0; index < binding.selectedDefaultArguments.size(); ++index)
    {
        std::size_t parameterIndex = 0;
        std::string_view parameterTypeKey;
        if (!resolveOrdinaryDefaultArgument(
                binding, selectedMember, index, parameterIndex, parameterTypeKey, error))
            return false;
        auto argument = lowerDefaultArgument(
            binding.selectedDefaultArguments[index],
            parameterTypeKey,
            error);
        if (argument == nullptr) return false;
        node.children.push_back(std::move(argument));
        node.argumentParameterOrdinals.push_back(parameterIndex);
    }
    if (binding.selectedSynthesizesEmptyParameterPack)
    {
        std::size_t parameterIndex = 0;
        const std::string* elementType = nullptr;
        if (!resolveEmptyOrdinaryParameterPack(
                binding, selectedMember, parameterIndex, elementType, error))
            return false;
        auto pack = makeNode(
            ProjectOopHirNodeKind::ParameterPack,
            expression,
            selectedMember.parameters[parameterIndex].typeKey);
        pack->elementTypeKey = *elementType;
        node.children.push_back(std::move(pack));
        node.argumentParameterOrdinals.push_back(parameterIndex);
    }
    return true;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishOrdinaryCall(
    std::unique_ptr<ProjectOopHirNode> node,
    const ast::AstNode* expression,
    bool addressContext,
    const ClassMemberSymbol& selectedMember,
    bool hasOwnedTemporary,
    std::string& error)
{
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
