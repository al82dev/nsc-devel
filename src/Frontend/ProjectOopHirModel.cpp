#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include "nativesharp/Sema/EffectContext.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"

#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
// LA UNICA TABLA de las operaciones nucleo que se bajan por el bloque compartido de "llamada
// tipo string" de lowerExpression. Contesta LAS TRES preguntas que hay que hacer sobre una de
// ellas: si entra en el bloque, cuantos operandos toma, y en que nodo HIR se convierte.
//
// POR QUE EXISTE, y es la correccion del 2026-09-02 (arity) ampliada el 2026-09-03 (nodo HIR).
// Antes esto eran TRES cosas separadas dentro de la misma funcion, a cientos de lineas unas de
// otras:
//
//   1) una cadena de `core->kind == ... ||` que decidia si la operacion entraba en el bloque;
//   2) una cadena de if/else-if que le asignaba la aridad, con un `expected = 2U` por omision;
//   3) una cadena de if/else-if que le asignaba el nodo HIR, con `StringConcat` por omision.
//
// Anadir una operacion y olvidar cualquiera de las tres NO daba ningun error, y cada olvido
// fallaba de una forma distinta -- ninguna decia el nombre de la operacion:
//
//   * solo en (1): la aridad caia al 2 por omision. Si la operacion no tomaba dos operandos, el
//     resultado era "Stage 51 string intrinsic received an invalid operand count" -- un mensaje
//     que no nombra ni la operacion ni la lista.
//   * solo en (2): la operacion nunca entraba al bloque, asi que la llamada se quedaba siendo el
//     metodo NativeSharp normal -- el que devuelve -1 o "" como marcador de posicion. Compila,
//     enlaza, ejecuta, y da una respuesta plausible y FALSA. Medido al implementar File.GetSize:
//     el gancho aparecia declarado en el IR y sin una sola llamada.
//   * olvidar (3) era el peor de los tres: la operacion no se quedaba sin nodo, se convertia en
//     `StringConcat`. Es decir, `File.GetSize(ruta)` se habria bajado como una CONCATENACION DE
//     CADENAS. Ningun mensaje, ningun aviso; codigo generado que hace otra cosa.
//
// Con una sola funcion las tres respuestas salen del mismo `case`, asi que no pueden discrepar.
//
// Y EL SWITCH NO TIENE `default` A PROPOSITO. Con -Wswitch (y /WX, que este proyecto usa),
// anadir un enumerador a MethodCoreOperationKind y no tocar esta funcion es un ERROR DE
// COMPILACION que nombra el enumerador que falta. Ese es todo el punto: la comprobacion la hace
// el compilador y no la memoria de quien anade la operacion numero 105. Si la operacion nueva no
// va por este bloque, se pone en el grupo de `std::nullopt` y ya esta -- pero hay que decirlo.
//
// COMPROBADO ANTES DE FUSIONAR, para no fusionar dos cosas que ya discreparan: las 50 operaciones
// autorizadas por (1) y las 49 mapeadas explicitamente en (3) --mas `StringConcat`, que era el
// valor por omision-- eran exactamente el mismo conjunto. Cero codigo muerto, cero operacion sin
// mapeo. La tabla de abajo se derivo de las dos cadenas con un script, no a mano.
//
// LOS COMENTARIOS POR OPERACION son los de las cadenas viejas, movidos aqui tal cual: son la
// historia de por que cada operacion cae donde cae, y se pierden si se quedan en un fichero que ya
// no existe.
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
        return std::nullopt;
    case MethodCoreOperationKind::CollectionBuilder:
        return std::nullopt;
    case MethodCoreOperationKind::NullConditionalUserIndex:
        return std::nullopt;
    }

    // Inalcanzable: el switch de arriba cubre todos los enumeradores. Existe solo para callar el
    // aviso de "control reaches end of non-void function", que no entiende esa garantia.
    return std::nullopt;
}

std::string sanitize(std::string value)
{
    for (char& ch : value)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) == 0 && ch != '_')
        {
            ch = '_';
        }
    }
    return value;
}

std::string stableSymbolSuffix(std::string_view value)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char raw : value)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(raw));
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << hash;
    return stream.str();
}

// C1.2 "import identity": the INTERNAL name of an import whose C symbol name is shared by two
// libraries. It encodes the identity's library and symbol so the two never collide again, and it
// is never seen by the linker as an external symbol -- the backend gives this name a real body
// that binds the external symbol at first call. The `__nsc_` prefix is already reserved (see
// isReservedStage47CAbiExportName), so no user declaration can land on it.
std::string importIdentityInternalName(std::string_view dllName, std::string_view symbolName)
{
    return "__nsc_import_" + sanitize(std::string(dllName)) + "__" + sanitize(std::string(symbolName));
}

bool isComparison(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::EqualEqual || kind == lexer::TokenKind::BangEqual ||
        kind == lexer::TokenKind::Less || kind == lexer::TokenKind::LessEqual ||
        kind == lexer::TokenKind::Greater || kind == lexer::TokenKind::GreaterEqual;
}

std::string typeSuffix(const ClassMemberSymbol& member)
{
    std::string result;
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        // C4 "language references": `F(int* p)` and `F(ref int p)` are different members of the
        // language that lower to the same machine signature, so the mangled name has to keep them
        // apart or the two would become one LLVM function.
        if (parameter.passKind != ParameterPassKind::Value)
        {
            result += "__";
            result += parameterPassKindKeyword(parameter.passKind);
        }
        // NSC-LIMIT-009: mismo motivo que en mangleCallable. Un parametro de tipo anidado
        // colapsaba con uno del tipo homonimo de namespace, y dos sobrecargas distintas se
        // convertian en una sola funcion LLVM.
        result += "__" + compilation::symbolFragmentForIdentityKey(parameter.typeKey);
    }
    // Conversion overload identity includes the target type. Without this suffix,
    // `implicit operator A(Source)` and `implicit operator B(Source)` become the same
    // LLVM/COFF symbol even though the language resolves them by the requested target.
    if (member.isUserConversion)
    {
        result += "__to__" + compilation::symbolFragmentForIdentityKey(member.declaredTypeKey);
    }
    return result;
}

std::unique_ptr<ProjectOopHirNode> makeNode(
    ProjectOopHirNodeKind kind,
    const ast::AstNode* syntax,
    std::string typeKey = {})
{
    auto node = std::make_unique<ProjectOopHirNode>();
    node->kind = kind;
    node->syntax = syntax;
    node->typeKey = std::move(typeKey);
    return node;
}

// Item 323 deliberately proves only uses that cannot expose the address of the containing class.
// Reading a field copies its value. A FieldAddress is equally safe only while it is the direct
// destination of a store; every other appearance of the local is conservatively an escape.
bool stackClassLocalUsesAreSafe(
    const ProjectOopHirNode* node,
    const ProjectOopHirNode* declaration,
    std::size_t localOrdinal,
    const ProjectOopHirNode* parent = nullptr,
    const ProjectOopHirNode* grandparent = nullptr,
    std::size_t childIndex = 0,
    std::size_t parentChildIndex = 0)
{
    if (node == nullptr) return true;
    if (node != declaration && node->localOrdinal == localOrdinal)
    {
        const bool copiedField = node->kind == ProjectOopHirNodeKind::LocalValue &&
            parent != nullptr && parent->kind == ProjectOopHirNodeKind::FieldValue &&
            childIndex == 0;
        const bool directFieldStore = node->kind == ProjectOopHirNodeKind::LocalValue &&
            parent != nullptr && parent->kind == ProjectOopHirNodeKind::FieldAddress &&
            childIndex == 0 && grandparent != nullptr && parentChildIndex == 0 &&
            (grandparent->kind == ProjectOopHirNodeKind::Assignment ||
             grandparent->kind == ProjectOopHirNodeKind::CompoundStore);
        if (!copiedField && !directFieldStore) return false;
    }
    for (std::size_t index = 0; index < node->children.size(); ++index)
    {
        if (!stackClassLocalUsesAreSafe(
                node->children[index].get(), declaration, localOrdinal,
                node, parent, index, childIndex))
        {
            return false;
        }
    }
    return true;
}

void collectLocalDeclarations(
    ProjectOopHirNode* node,
    std::vector<ProjectOopHirNode*>& declarations)
{
    if (node == nullptr) return;
    if (node->kind == ProjectOopHirNodeKind::LocalDeclaration)
        declarations.push_back(node);
    for (auto& child : node->children)
        collectLocalDeclarations(child.get(), declarations);
}

const ProjectOopHirNode* firstUnpromotedObjectConstruction(
    const ProjectOopHirNode* node) noexcept
{
    if (node == nullptr) return nullptr;
    if (node->kind == ProjectOopHirNodeKind::ObjectConstruction &&
        !node->stackPromotedObject)
    {
        return node;
    }
    for (const auto& child : node->children)
    {
        if (const ProjectOopHirNode* found =
                firstUnpromotedObjectConstruction(child.get()))
        {
            return found;
        }
    }
    return nullptr;
}

enum class StaticAssertConstantKind
{
    Integer,
    Floating,
    Boolean
};

struct StaticAssertConstant final
{
    StaticAssertConstantKind kind = StaticAssertConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
};

std::optional<unsigned> staticAssertIntegerWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte") return 8U;
    if (typeKey == "builtin:char" || typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16U;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32U;
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint") return 64U;
    if (typeKey == "builtin:i128" || typeKey == "builtin:u128") return 128U;
    return std::nullopt;
}

bool staticAssertSignedInteger(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
        typeKey == "builtin:int" || typeKey == "builtin:long" || typeKey == "builtin:nint" ||
        typeKey == "builtin:i128";
}

std::uint64_t staticAssertMask(unsigned width) noexcept
{
    return width == 64U ? ~std::uint64_t{0} : ((std::uint64_t{1} << width) - 1U);
}

std::uint64_t staticAssertNormalize(std::uint64_t value, std::string_view typeKey) noexcept
{
    const std::optional<unsigned> width = staticAssertIntegerWidth(typeKey);
    return width.has_value() ? value & staticAssertMask(*width) : value;
}

std::int64_t staticAssertSignedValue(std::uint64_t value, std::string_view typeKey) noexcept
{
    const unsigned width = staticAssertIntegerWidth(typeKey).value_or(64U);
    value &= staticAssertMask(width);
    if (width == 64U) return std::bit_cast<std::int64_t>(value);
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((value & sign) != 0U) value |= ~staticAssertMask(width);
    return std::bit_cast<std::int64_t>(value);
}

std::optional<StaticAssertConstant> evaluateStaticAssertConstant(const ProjectOopHirNode& node)
{
    const auto integer = [&](std::uint64_t value, std::string typeKey)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Integer;
        result.integerValue = staticAssertNormalize(value, typeKey);
        result.typeKey = std::move(typeKey);
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto floating = [&](double value, std::string typeKey)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Floating;
        result.floatingValue = value;
        result.typeKey = std::move(typeKey);
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto boolean = [&](bool value)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Boolean;
        result.booleanValue = value;
        result.typeKey = "builtin:bool";
        return std::optional<StaticAssertConstant>(std::move(result));
    };

    switch (node.kind)
    {
        case ProjectOopHirNodeKind::IntegerConstant:
            return integer(node.integerValue, node.typeKey);
        case ProjectOopHirNodeKind::FloatingConstant:
            return floating(node.floatingValue, node.typeKey);
        case ProjectOopHirNodeKind::BooleanConstant:
            return boolean(node.booleanValue);
        case ProjectOopHirNodeKind::Conversion:
        {
            if (node.children.size() != 1U) return std::nullopt;
            const std::optional<StaticAssertConstant> value =
                evaluateStaticAssertConstant(*node.children.front());
            if (!value.has_value()) return std::nullopt;
            if (node.typeKey == "builtin:bool")
                return value->kind == StaticAssertConstantKind::Boolean
                    ? boolean(value->booleanValue) : std::nullopt;
            if (node.typeKey == "builtin:float" || node.typeKey == "builtin:double")
            {
                if (value->kind == StaticAssertConstantKind::Floating)
                    return floating(
                        node.typeKey == "builtin:float"
                            ? static_cast<double>(static_cast<float>(value->floatingValue))
                            : value->floatingValue,
                        node.typeKey);
                if (value->kind != StaticAssertConstantKind::Integer) return std::nullopt;
                const double converted = staticAssertSignedInteger(value->typeKey)
                    ? static_cast<double>(staticAssertSignedValue(value->integerValue, value->typeKey))
                    : static_cast<double>(value->integerValue);
                return floating(
                    node.typeKey == "builtin:float"
                        ? static_cast<double>(static_cast<float>(converted)) : converted,
                    node.typeKey);
            }
            if (!staticAssertIntegerWidth(node.typeKey).has_value()) return std::nullopt;
            if (value->kind == StaticAssertConstantKind::Integer)
                return integer(value->integerValue, node.typeKey);
            if (value->kind != StaticAssertConstantKind::Floating ||
                !std::isfinite(value->floatingValue)) return std::nullopt;
            const long double converted = std::trunc(static_cast<long double>(value->floatingValue));
            if (staticAssertSignedInteger(node.typeKey))
            {
                const unsigned width = *staticAssertIntegerWidth(node.typeKey);
                const long double minimum = width == 64U
                    ? static_cast<long double>(std::numeric_limits<std::int64_t>::min())
                    : -static_cast<long double>(std::uint64_t{1} << (width - 1U));
                const long double maximum = width == 64U
                    ? static_cast<long double>(std::numeric_limits<std::int64_t>::max())
                    : static_cast<long double>((std::uint64_t{1} << (width - 1U)) - 1U);
                if (converted < minimum || converted > maximum) return std::nullopt;
                return integer(
                    std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(converted)), node.typeKey);
            }
            const unsigned width = *staticAssertIntegerWidth(node.typeKey);
            const long double maximum = width == 64U
                ? static_cast<long double>(std::numeric_limits<std::uint64_t>::max())
                : static_cast<long double>(staticAssertMask(width));
            if (converted < 0.0L || converted > maximum) return std::nullopt;
            return integer(static_cast<std::uint64_t>(converted), node.typeKey);
        }
        case ProjectOopHirNodeKind::Unary:
        {
            if (node.children.size() != 1U) return std::nullopt;
            const std::optional<StaticAssertConstant> value =
                evaluateStaticAssertConstant(*node.children.front());
            if (!value.has_value()) return std::nullopt;
            switch (node.operation)
            {
                case lexer::TokenKind::Plus:
                    return value;
                case lexer::TokenKind::Minus:
                    if (value->kind == StaticAssertConstantKind::Floating)
                        return floating(-value->floatingValue, node.typeKey);
                    if (value->kind == StaticAssertConstantKind::Integer)
                        return integer(std::uint64_t{0} - value->integerValue, node.typeKey);
                    return std::nullopt;
                case lexer::TokenKind::Tilde:
                    return value->kind == StaticAssertConstantKind::Integer
                        ? integer(~value->integerValue, node.typeKey) : std::nullopt;
                case lexer::TokenKind::Bang:
                    return value->kind == StaticAssertConstantKind::Boolean
                        ? boolean(!value->booleanValue) : std::nullopt;
                default:
                    return std::nullopt;
            }
        }
        case ProjectOopHirNodeKind::Binary:
        {
            if (node.children.size() != 2U) return std::nullopt;
            const std::optional<StaticAssertConstant> left =
                evaluateStaticAssertConstant(*node.children[0]);
            if (!left.has_value()) return std::nullopt;
            if (node.operation == lexer::TokenKind::AmpAmp)
            {
                if (left->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                if (!left->booleanValue) return boolean(false);
            }
            if (node.operation == lexer::TokenKind::PipePipe)
            {
                if (left->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                if (left->booleanValue) return boolean(true);
            }
            const std::optional<StaticAssertConstant> right =
                evaluateStaticAssertConstant(*node.children[1]);
            if (!right.has_value()) return std::nullopt;
            if (node.operation == lexer::TokenKind::AmpAmp || node.operation == lexer::TokenKind::PipePipe)
            {
                if (right->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                return boolean(right->booleanValue);
            }

            const bool floatingOperands = left->kind == StaticAssertConstantKind::Floating &&
                right->kind == StaticAssertConstantKind::Floating;
            const bool integerOperands = left->kind == StaticAssertConstantKind::Integer &&
                right->kind == StaticAssertConstantKind::Integer;
            const bool booleanOperands = left->kind == StaticAssertConstantKind::Boolean &&
                right->kind == StaticAssertConstantKind::Boolean;
            if (node.operation == lexer::TokenKind::EqualEqual ||
                node.operation == lexer::TokenKind::BangEqual)
            {
                bool equal = false;
                if (floatingOperands) equal = left->floatingValue == right->floatingValue;
                else if (integerOperands) equal = left->integerValue == right->integerValue;
                else if (booleanOperands) equal = left->booleanValue == right->booleanValue;
                else return std::nullopt;
                return boolean(node.operation == lexer::TokenKind::EqualEqual ? equal : !equal);
            }
            if (floatingOperands)
            {
                switch (node.operation)
                {
                    case lexer::TokenKind::Less: return boolean(left->floatingValue < right->floatingValue);
                    case lexer::TokenKind::LessEqual: return boolean(left->floatingValue <= right->floatingValue);
                    case lexer::TokenKind::Greater: return boolean(left->floatingValue > right->floatingValue);
                    case lexer::TokenKind::GreaterEqual: return boolean(left->floatingValue >= right->floatingValue);
                    case lexer::TokenKind::Plus: return floating(left->floatingValue + right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Minus: return floating(left->floatingValue - right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Star: return floating(left->floatingValue * right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Slash: return floating(left->floatingValue / right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Percent: return floating(std::fmod(left->floatingValue, right->floatingValue), node.typeKey);
                    default: return std::nullopt;
                }
            }
            if (!integerOperands) return std::nullopt;
            const unsigned width = staticAssertIntegerWidth(left->typeKey).value_or(64U);
            const bool signedOperands = staticAssertSignedInteger(left->typeKey);
            const std::int64_t signedLeft = staticAssertSignedValue(left->integerValue, left->typeKey);
            const std::int64_t signedRight = staticAssertSignedValue(right->integerValue, right->typeKey);
            switch (node.operation)
            {
                case lexer::TokenKind::Less:
                    return boolean(signedOperands ? signedLeft < signedRight
                                                  : left->integerValue < right->integerValue);
                case lexer::TokenKind::LessEqual:
                    return boolean(signedOperands ? signedLeft <= signedRight
                                                  : left->integerValue <= right->integerValue);
                case lexer::TokenKind::Greater:
                    return boolean(signedOperands ? signedLeft > signedRight
                                                  : left->integerValue > right->integerValue);
                case lexer::TokenKind::GreaterEqual:
                    return boolean(signedOperands ? signedLeft >= signedRight
                                                  : left->integerValue >= right->integerValue);
                case lexer::TokenKind::Plus:
                    return integer(left->integerValue + right->integerValue, node.typeKey);
                case lexer::TokenKind::Minus:
                    return integer(left->integerValue - right->integerValue, node.typeKey);
                case lexer::TokenKind::Star:
                    return integer(left->integerValue * right->integerValue, node.typeKey);
                case lexer::TokenKind::Amp:
                    return integer(left->integerValue & right->integerValue, node.typeKey);
                case lexer::TokenKind::Pipe:
                    return integer(left->integerValue | right->integerValue, node.typeKey);
                case lexer::TokenKind::Caret:
                    return integer(left->integerValue ^ right->integerValue, node.typeKey);
                case lexer::TokenKind::LessLess:
                    if (right->integerValue >= width) return std::nullopt;
                    return integer(left->integerValue << right->integerValue, node.typeKey);
                case lexer::TokenKind::GreaterGreater:
                    if (right->integerValue >= width) return std::nullopt;
                    return signedOperands
                        ? integer(
                            std::bit_cast<std::uint64_t>(signedLeft >> right->integerValue), node.typeKey)
                        : integer(left->integerValue >> right->integerValue, node.typeKey);
                case lexer::TokenKind::Slash:
                case lexer::TokenKind::Percent:
                    if (right->integerValue == 0U) return std::nullopt;
                    if (!signedOperands)
                        return integer(
                            node.operation == lexer::TokenKind::Slash
                                ? left->integerValue / right->integerValue
                                : left->integerValue % right->integerValue,
                            node.typeKey);
                    if (signedLeft == std::numeric_limits<std::int64_t>::min() && signedRight == -1)
                        return integer(
                            node.operation == lexer::TokenKind::Slash
                                ? left->integerValue : std::uint64_t{0},
                            node.typeKey);
                    return integer(
                        std::bit_cast<std::uint64_t>(
                            node.operation == lexer::TokenKind::Slash
                                ? signedLeft / signedRight : signedLeft % signedRight),
                        node.typeKey);
                default:
                    return std::nullopt;
            }
        }
        case ProjectOopHirNodeKind::Conditional:
        {
            if (node.children.size() != 3U) return std::nullopt;
            const std::optional<StaticAssertConstant> condition =
                evaluateStaticAssertConstant(*node.children[0]);
            if (!condition.has_value() || condition->kind != StaticAssertConstantKind::Boolean)
                return std::nullopt;
            return evaluateStaticAssertConstant(
                *node.children[condition->booleanValue ? 1U : 2U]);
        }
        default:
            return std::nullopt;
    }
}

void scanParallelOrdinals(
    const ProjectOopHirNode* node,
    std::size_t localFloor,
    std::size_t cleanupFloor,
    std::size_t& localCount,
    std::size_t& cleanupCount,
    std::size_t& regionCount)
{
    if (node == nullptr) return;
    if (node->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow)
    {
        if (node->localOrdinal.has_value() && *node->localOrdinal >= localFloor)
            localCount = std::max(localCount, *node->localOrdinal - localFloor + 1U);
        if (node->cleanupRegionOrdinal.has_value() && *node->cleanupRegionOrdinal >= cleanupFloor)
            cleanupCount = std::max(
                cleanupCount, *node->cleanupRegionOrdinal - cleanupFloor + 1U);
    }
    if (node->hirRegionOrdinal.has_value())
        regionCount = std::max(regionCount, *node->hirRegionOrdinal + 1U);
    for (const std::unique_ptr<ProjectOopHirNode>& child : node->children)
        scanParallelOrdinals(
            child.get(), localFloor, cleanupFloor, localCount, cleanupCount, regionCount);
}

void applyParallelOrdinalOffsets(
    ProjectOopHirNode* node,
    std::size_t localFloor,
    std::size_t cleanupFloor,
    std::size_t localOffset,
    std::size_t cleanupOffset,
    std::size_t regionOffset)
{
    if (node == nullptr) return;
    if (node->kind == ProjectOopHirNodeKind::OwnedTemporaryBorrow)
    {
        if (node->localOrdinal.has_value() && *node->localOrdinal >= localFloor)
            *node->localOrdinal += localOffset;
        if (node->cleanupRegionOrdinal.has_value() && *node->cleanupRegionOrdinal >= cleanupFloor)
            *node->cleanupRegionOrdinal += cleanupOffset;
    }
    if (node->hirRegionOrdinal.has_value()) *node->hirRegionOrdinal += regionOffset;
    if (node->exceptionTargetRegionOrdinal.has_value())
        *node->exceptionTargetRegionOrdinal += regionOffset;
    for (const std::unique_ptr<ProjectOopHirNode>& child : node->children)
        applyParallelOrdinalOffsets(
            child.get(), localFloor, cleanupFloor, localOffset, cleanupOffset, regionOffset);
}
}

const char* projectOopHirNodeKindName(ProjectOopHirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopHirNodeKind::Block: return "block";
        case ProjectOopHirNodeKind::BlockExpression: return "block-expression";
        case ProjectOopHirNodeKind::LocalDeclaration: return "local-declaration";
        case ProjectOopHirNodeKind::ExpressionStatement: return "expression-statement";
        case ProjectOopHirNodeKind::InlineAssembly: return "inline-assembly";
        case ProjectOopHirNodeKind::InlineAssemblyLine: return "inline-assembly-line";
        case ProjectOopHirNodeKind::InlineAssemblyInput: return "inline-assembly-input";
        case ProjectOopHirNodeKind::InlineAssemblyOutput: return "inline-assembly-output";
        case ProjectOopHirNodeKind::InlineAssemblyClobber: return "inline-assembly-clobber";
        case ProjectOopHirNodeKind::If: return "if";
        case ProjectOopHirNodeKind::While: return "while";
        case ProjectOopHirNodeKind::DoWhile: return "do-while";
        case ProjectOopHirNodeKind::For: return "for";
        case ProjectOopHirNodeKind::Switch: return "switch";
        case ProjectOopHirNodeKind::SwitchSection: return "switch-section";
        case ProjectOopHirNodeKind::Break: return "break";
        case ProjectOopHirNodeKind::Continue: return "continue";
        case ProjectOopHirNodeKind::Return: return "return";
        case ProjectOopHirNodeKind::TryRegion: return "try-region";
        case ProjectOopHirNodeKind::CatchRegion: return "catch-region";
        case ProjectOopHirNodeKind::FinallyRegion: return "finally-region";
        case ProjectOopHirNodeKind::UsingRegion: return "using-region";
        case ProjectOopHirNodeKind::Throw: return "throw";
        case ProjectOopHirNodeKind::Rethrow: return "rethrow";
        case ProjectOopHirNodeKind::CleanupContinuation: return "cleanup-continuation";
        case ProjectOopHirNodeKind::OwnedParameterCleanup: return "owned-parameter-cleanup";
        case ProjectOopHirNodeKind::MoveValue: return "move-value";
        case ProjectOopHirNodeKind::MoveArrayElement: return "move-array-element";
        case ProjectOopHirNodeKind::MoveOwnedField: return "move-owned-field";
        case ProjectOopHirNodeKind::OwnedTemporaryBorrow: return "owned-temporary-borrow";
        case ProjectOopHirNodeKind::FullExpressionCleanup: return "full-expression-cleanup";
        case ProjectOopHirNodeKind::PointerLoad: return "pointer-load";
        case ProjectOopHirNodeKind::RawElementAddress: return "raw-element-address";
        case ProjectOopHirNodeKind::StackAllocation: return "stack-allocation";
        case ProjectOopHirNodeKind::RawAllocation: return "raw-allocation";
        case ProjectOopHirNodeKind::PointerArithmetic: return "pointer-arithmetic";
        case ProjectOopHirNodeKind::ValueConstruction: return "value-construction";
        case ProjectOopHirNodeKind::PlacementConstruction: return "placement-construction";
        case ProjectOopHirNodeKind::AggregateInitializer: return "aggregate-initializer";
        case ProjectOopHirNodeKind::AggregateSpread: return "aggregate-spread";
        case ProjectOopHirNodeKind::AggregateElement: return "aggregate-element";
        case ProjectOopHirNodeKind::TupleDeconstruct: return "tuple-deconstruct";
        case ProjectOopHirNodeKind::TupleDeconstructElement: return "tuple-deconstruct-element";
        case ProjectOopHirNodeKind::ParameterPack: return "parameter-pack";
        case ProjectOopHirNodeKind::TemporaryAddress: return "temporary-address";
        case ProjectOopHirNodeKind::AddressValue: return "address-value";
        case ProjectOopHirNodeKind::BorrowValue: return "borrow-value";
        case ProjectOopHirNodeKind::ThisValue: return "this";
        case ProjectOopHirNodeKind::ParameterValue: return "parameter-value";
        case ProjectOopHirNodeKind::LocalValue: return "local-value";
        case ProjectOopHirNodeKind::FieldValue: return "field-value";
        case ProjectOopHirNodeKind::NullConditionalFieldValue: return "null-conditional-field-value";
        case ProjectOopHirNodeKind::NullConditionalFieldStore: return "null-conditional-field-store";
        case ProjectOopHirNodeKind::NullConditionalArrayLoad: return "null-conditional-array-load";
        case ProjectOopHirNodeKind::NullConditionalArrayStore: return "null-conditional-array-store";
        case ProjectOopHirNodeKind::StaticFieldValue: return "static-field-value";
        case ProjectOopHirNodeKind::NamespaceStateValue: return "namespace-state-value";
        case ProjectOopHirNodeKind::ParameterAddress: return "parameter-address";
        case ProjectOopHirNodeKind::LocalAddress: return "local-address";
        case ProjectOopHirNodeKind::FieldAddress: return "field-address";
        case ProjectOopHirNodeKind::FutureFieldAddress: return "future-field-address";
        case ProjectOopHirNodeKind::AwaitFuture: return "await-future";
        case ProjectOopHirNodeKind::SuspendPoint: return "suspend-point";
        case ProjectOopHirNodeKind::PollAddress: return "poll-address";
        case ProjectOopHirNodeKind::YieldValue: return "yield-value";
        case ProjectOopHirNodeKind::YieldBreak: return "yield-break";
        case ProjectOopHirNodeKind::IteratorCurrent: return "iterator-current";
        case ProjectOopHirNodeKind::IteratorMoveNext: return "iterator-move-next";
        case ProjectOopHirNodeKind::IteratorGetEnumerator: return "iterator-get-enumerator";
        case ProjectOopHirNodeKind::IteratorDrop: return "iterator-drop";
        case ProjectOopHirNodeKind::IteratorMoveNextAsync: return "iterator-move-next-async";
        case ProjectOopHirNodeKind::AsyncYieldValue: return "async-yield-value";
        case ProjectOopHirNodeKind::DropFrame: return "drop-frame";
        case ProjectOopHirNodeKind::StaticFieldAddress: return "static-field-address";
        case ProjectOopHirNodeKind::NamespaceStateAddress: return "namespace-state-address";
        case ProjectOopHirNodeKind::IntegerConstant: return "integer-constant";
        case ProjectOopHirNodeKind::FloatingConstant: return "floating-constant";
        case ProjectOopHirNodeKind::BooleanConstant: return "boolean-constant";
        case ProjectOopHirNodeKind::NullConstant: return "null-constant";
        case ProjectOopHirNodeKind::DefaultValue: return "default-value";
        case ProjectOopHirNodeKind::StringLiteral: return "string-literal";
        case ProjectOopHirNodeKind::ArrayAllocation: return "array-allocation";
        case ProjectOopHirNodeKind::RangeArrayAllocation: return "range-array-allocation";
        case ProjectOopHirNodeKind::ArrayLength: return "array-length";
        case ProjectOopHirNodeKind::ArrayElementValue: return "array-element-value";
        case ProjectOopHirNodeKind::ArrayElementAddress: return "array-element-address";
        case ProjectOopHirNodeKind::RangeArrayElementValue: return "range-array-element-value";
        case ProjectOopHirNodeKind::RangeArrayElementAddress: return "range-array-element-address";
        case ProjectOopHirNodeKind::RangeSlice: return "range-slice";
        case ProjectOopHirNodeKind::StringLength: return "string-length";
        case ProjectOopHirNodeKind::StringElementValue: return "string-element-value";
        case ProjectOopHirNodeKind::EnumName: return "enum-name";
        case ProjectOopHirNodeKind::StringConcat: return "string-concat";
        case ProjectOopHirNodeKind::StringEquals: return "string-equals";
        case ProjectOopHirNodeKind::StringSubstring: return "string-substring";
        case ProjectOopHirNodeKind::StringCharacterPointer: return "string-character-pointer";
        case ProjectOopHirNodeKind::StringFromCharArray: return "string-from-char-array";
        case ProjectOopHirNodeKind::StringViewFromString: return "string-view-from-string";
        case ProjectOopHirNodeKind::SpanFromArray: return "span-from-array";
        case ProjectOopHirNodeKind::ReadOnlySpanFromArray: return "readonly-span-from-array";
        case ProjectOopHirNodeKind::SpanFromFixedArray: return "span-from-fixed-array";
        case ProjectOopHirNodeKind::ReadOnlySpanFromFixedArray: return "readonly-span-from-fixed-array";
        case ProjectOopHirNodeKind::CoreConsoleWriteLine: return "core-console-writeline";
        case ProjectOopHirNodeKind::CoreConsoleReadLine: return "core-console-readline";
        case ProjectOopHirNodeKind::AggregateEquals: return "aggregate-equals";
        case ProjectOopHirNodeKind::AggregateHash: return "aggregate-hash";
        case ProjectOopHirNodeKind::CoreObjectTypeName: return "core-object-type-name";
        case ProjectOopHirNodeKind::CoreObjectReferenceHash: return "core-object-reference-hash";
        case ProjectOopHirNodeKind::CoreClockQueryCounter: return "core-clock-query-counter";
        case ProjectOopHirNodeKind::CoreClockQueryFrequency: return "core-clock-query-frequency";
        case ProjectOopHirNodeKind::CoreClockQuerySystemFileTime: return "core-clock-query-system-filetime";
        case ProjectOopHirNodeKind::CoreEnvironmentTickCount64: return "core-environment-tick-count64";
        case ProjectOopHirNodeKind::CoreEnvironmentProcessorCount: return "core-environment-processor-count";
        case ProjectOopHirNodeKind::CoreEnvironmentExit: return "core-environment-exit";
        case ProjectOopHirNodeKind::CoreEnvironmentGetVariable: return "core-environment-get-variable";
        case ProjectOopHirNodeKind::CoreEnvironmentSetVariable: return "core-environment-set-variable";
        case ProjectOopHirNodeKind::CoreFileExists: return "core-file-exists";
        case ProjectOopHirNodeKind::CoreFileDelete: return "core-file-delete";
        case ProjectOopHirNodeKind::CoreDirectoryExists: return "core-directory-exists";
        case ProjectOopHirNodeKind::CoreDirectoryCreate: return "core-directory-create";
        case ProjectOopHirNodeKind::CoreFileReadAllText: return "core-file-read-all-text";
        case ProjectOopHirNodeKind::CoreFileGetSize: return "core-file-get-size";
        case ProjectOopHirNodeKind::CoreFileReadAllBytes: return "core-file-read-all-bytes";
        case ProjectOopHirNodeKind::CoreFileReadBytes: return "core-file-read-bytes";
        case ProjectOopHirNodeKind::CoreFileWriteAllBytes: return "core-file-write-all-bytes";
        case ProjectOopHirNodeKind::CoreEnvironmentCurrentDirectory: return "core-environment-current-directory";
        case ProjectOopHirNodeKind::CoreProcessRun: return "core-process-run";
        case ProjectOopHirNodeKind::CoreFileWriteAllText: return "core-file-write-all-text";
        case ProjectOopHirNodeKind::CoreFileAppendAllText: return "core-file-append-all-text";
        case ProjectOopHirNodeKind::CoreDirectoryDelete: return "core-directory-delete";
        case ProjectOopHirNodeKind::CorePathGetFullPath: return "core-path-get-full-path";
        case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgCount: return "core-environment-get-command-line-arg-count";
        case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgs: return "core-environment-get-command-line-args";
        case ProjectOopHirNodeKind::CoreDirectoryGetFileCount: return "core-directory-get-file-count";
        case ProjectOopHirNodeKind::CoreDirectoryGetFiles: return "core-directory-get-files";
        case ProjectOopHirNodeKind::CoreDirectoryGetDirectoryCount: return "core-directory-get-directory-count";
        case ProjectOopHirNodeKind::CoreDirectoryGetDirectories: return "core-directory-get-directories";
        case ProjectOopHirNodeKind::CoreThreadCreate: return "core-thread-create";
        case ProjectOopHirNodeKind::CoreThreadJoin: return "core-thread-join";
        case ProjectOopHirNodeKind::CoreThreadDetach: return "core-thread-detach";
        case ProjectOopHirNodeKind::CoreAtomicLoadInt32: return "core-atomic-load-int32";
        case ProjectOopHirNodeKind::CoreAtomicStoreInt32: return "core-atomic-store-int32";
        case ProjectOopHirNodeKind::CoreInterlockedIncrementInt32: return "core-interlocked-increment-int32";
        case ProjectOopHirNodeKind::CoreInterlockedDecrementInt32: return "core-interlocked-decrement-int32";
        case ProjectOopHirNodeKind::CoreInterlockedAddInt32: return "core-interlocked-add-int32";
        case ProjectOopHirNodeKind::CoreInterlockedExchangeInt32: return "core-interlocked-exchange-int32";
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt32: return "core-interlocked-compare-exchange-int32";
        case ProjectOopHirNodeKind::CoreAtomicLoadInt64: return "core-atomic-load-int64";
        case ProjectOopHirNodeKind::CoreAtomicStoreInt64: return "core-atomic-store-int64";
        case ProjectOopHirNodeKind::CoreInterlockedIncrementInt64: return "core-interlocked-increment-int64";
        case ProjectOopHirNodeKind::CoreInterlockedDecrementInt64: return "core-interlocked-decrement-int64";
        case ProjectOopHirNodeKind::CoreInterlockedAddInt64: return "core-interlocked-add-int64";
        case ProjectOopHirNodeKind::CoreInterlockedExchangeInt64: return "core-interlocked-exchange-int64";
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt64: return "core-interlocked-compare-exchange-int64";
        case ProjectOopHirNodeKind::CoreAtomicLoadPointer: return "core-atomic-load-pointer";
        case ProjectOopHirNodeKind::CoreAtomicStorePointer: return "core-atomic-store-pointer";
        case ProjectOopHirNodeKind::CoreInterlockedExchangePointer: return "core-interlocked-exchange-pointer";
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangePointer: return "core-interlocked-compare-exchange-pointer";
        case ProjectOopHirNodeKind::CoreAtomicThreadFence: return "core-atomic-thread-fence";
        case ProjectOopHirNodeKind::CoreAtomicCompilerFence: return "core-atomic-compiler-fence";
        case ProjectOopHirNodeKind::CoreSimdFloat32x4Add: return "core-simd-float32x4-add";
        case ProjectOopHirNodeKind::CoreSimdFloat32x4Subtract: return "core-simd-float32x4-subtract";
        case ProjectOopHirNodeKind::CoreSimdFloat32x4Multiply: return "core-simd-float32x4-multiply";
        case ProjectOopHirNodeKind::CoreSimdInt128Add: return "core-simd-int128-add";
        case ProjectOopHirNodeKind::CoreSimdInt128Subtract: return "core-simd-int128-subtract";
        case ProjectOopHirNodeKind::CoreSimdInt128Multiply: return "core-simd-int128-multiply";
        case ProjectOopHirNodeKind::CoreSimdFloat256Add: return "core-simd-float256-add";
        case ProjectOopHirNodeKind::CoreSimdFloat256Subtract: return "core-simd-float256-subtract";
        case ProjectOopHirNodeKind::CoreSimdFloat256Multiply: return "core-simd-float256-multiply";
        case ProjectOopHirNodeKind::CoreSimdInt256Add: return "core-simd-int256-add";
        case ProjectOopHirNodeKind::CoreSimdInt256Subtract: return "core-simd-int256-subtract";
        case ProjectOopHirNodeKind::CoreSimdInt256Multiply: return "core-simd-int256-multiply";
        case ProjectOopHirNodeKind::CoreSimdLoadAligned128: return "core-simd-load-aligned-128";
        case ProjectOopHirNodeKind::CoreSimdStoreAligned128: return "core-simd-store-aligned-128";
        case ProjectOopHirNodeKind::CoreSimdLoadUnaligned128: return "core-simd-load-unaligned-128";
        case ProjectOopHirNodeKind::CoreSimdStoreUnaligned128: return "core-simd-store-unaligned-128";
        case ProjectOopHirNodeKind::CoreHalfFromFloat: return "core-half-from-float";
        case ProjectOopHirNodeKind::CoreHalfToFloat: return "core-half-to-float";
        case ProjectOopHirNodeKind::CoreSimdSwizzleWzyx: return "core-simd-swizzle-wzyx";
        case ProjectOopHirNodeKind::CoreSimdShuffleLowHigh: return "core-simd-shuffle-low-high";
        case ProjectOopHirNodeKind::CoreSimdSqrt: return "core-simd-sqrt";
        case ProjectOopHirNodeKind::CoreSimdReciprocalSqrt: return "core-simd-reciprocal-sqrt";
        case ProjectOopHirNodeKind::CoreSimdFusedMultiplyAdd: return "core-simd-fused-multiply-add";
        case ProjectOopHirNodeKind::CoreBitPopulationCount: return "core-bit-popcount";
        case ProjectOopHirNodeKind::CoreBitLeadingZeroCount: return "core-bit-clz";
        case ProjectOopHirNodeKind::CoreBitTrailingZeroCount: return "core-bit-ctz";
        case ProjectOopHirNodeKind::CoreBitRotateLeft: return "core-bit-rotate-left";
        case ProjectOopHirNodeKind::CoreBitRotateRight: return "core-bit-rotate-right";
        case ProjectOopHirNodeKind::CoreBitByteSwap: return "core-bit-byte-swap";
        case ProjectOopHirNodeKind::CoreCpuPrefetchRead: return "core-cpu-prefetch-read";
        case ProjectOopHirNodeKind::CoreCpuPause: return "core-cpu-pause";
        case ProjectOopHirNodeKind::CoreCpuReadTimestamp: return "core-cpu-read-timestamp";
        case ProjectOopHirNodeKind::CoreMutexCreate: return "core-mutex-create";
        case ProjectOopHirNodeKind::CoreMutexCreateNamed: return "core-mutex-create-named";
        case ProjectOopHirNodeKind::CoreMutexLock: return "core-mutex-lock";
        case ProjectOopHirNodeKind::CoreMutexUnlock: return "core-mutex-unlock";
        case ProjectOopHirNodeKind::CoreMutexClose: return "core-mutex-close";
        case ProjectOopHirNodeKind::CoreMonitorEnter: return "core-monitor-enter";
        case ProjectOopHirNodeKind::CoreMonitorExit: return "core-monitor-exit";
        case ProjectOopHirNodeKind::CoreMonitorWait: return "core-monitor-wait";
        case ProjectOopHirNodeKind::CoreMonitorPulse: return "core-monitor-pulse";
        case ProjectOopHirNodeKind::CoreMonitorPulseAll: return "core-monitor-pulse-all";
        case ProjectOopHirNodeKind::CoreSemaphoreCreate: return "core-semaphore-create";
        case ProjectOopHirNodeKind::CoreSemaphoreCreateNamed: return "core-semaphore-create-named";
        case ProjectOopHirNodeKind::CoreSemaphoreWait: return "core-semaphore-wait";
        case ProjectOopHirNodeKind::CoreSemaphoreRelease: return "core-semaphore-release";
        case ProjectOopHirNodeKind::CoreSemaphoreClose: return "core-semaphore-close";
        case ProjectOopHirNodeKind::CoreEventCreate: return "core-event-create";
        case ProjectOopHirNodeKind::CoreEventCreateNamed: return "core-event-create-named";
        case ProjectOopHirNodeKind::CoreEventSet: return "core-event-set";
        case ProjectOopHirNodeKind::CoreEventReset: return "core-event-reset";
        case ProjectOopHirNodeKind::CoreEventWait: return "core-event-wait";
        case ProjectOopHirNodeKind::CoreEventClose: return "core-event-close";
        case ProjectOopHirNodeKind::CoreProcessGetCurrentProcessId: return "core-process-get-current-process-id";
        case ProjectOopHirNodeKind::CoreProcessGetWorkingSetSize: return "core-process-get-working-set-size";
        case ProjectOopHirNodeKind::CoreProcessGetKernelTime: return "core-process-get-kernel-time";
        case ProjectOopHirNodeKind::CoreProcessGetUserTime: return "core-process-get-user-time";
        case ProjectOopHirNodeKind::CoreProcessGetPriorityClass: return "core-process-get-priority-class";
        case ProjectOopHirNodeKind::CoreProcessSetPriorityClass: return "core-process-set-priority-class";
        case ProjectOopHirNodeKind::CoreSystemInfoGetPageSize: return "core-system-info-get-page-size";
        case ProjectOopHirNodeKind::CoreSystemInfoGetProcessorArchitecture: return "core-system-info-get-processor-architecture";
        case ProjectOopHirNodeKind::CoreSystemInfoGetAllocationGranularity: return "core-system-info-get-allocation-granularity";
        case ProjectOopHirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes: return "core-system-info-get-total-physical-memory-bytes";
        case ProjectOopHirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes: return "core-system-info-get-available-physical-memory-bytes";
        case ProjectOopHirNodeKind::CoreSystemInfoGetMemoryLoadPercent: return "core-system-info-get-memory-load-percent";
        case ProjectOopHirNodeKind::CoreDebugIsDebuggerPresent: return "core-debug-is-debugger-present";
        case ProjectOopHirNodeKind::CoreDebugCaptureStackFrameCount: return "core-debug-capture-stack-frame-count";
        case ProjectOopHirNodeKind::CoreDebugOutputString: return "core-debug-output-string";
        case ProjectOopHirNodeKind::CoreThreadSleep: return "core-thread-sleep";
        case ProjectOopHirNodeKind::CoreFileCopy: return "core-file-copy";
        case ProjectOopHirNodeKind::CoreFileMove: return "core-file-move";
        case ProjectOopHirNodeKind::CoreDirectoryMove: return "core-directory-move";
        case ProjectOopHirNodeKind::DelegateCreate: return "delegate-create";
        case ProjectOopHirNodeKind::LambdaCreate: return "lambda-create";
        case ProjectOopHirNodeKind::ClosureCaptureValue: return "closure-capture-value";
        case ProjectOopHirNodeKind::ClosureCaptureAddress: return "closure-capture-address";
        case ProjectOopHirNodeKind::CaptureCellReference: return "capture-cell-reference";
        case ProjectOopHirNodeKind::ClosureCaptureCellReference: return "closure-capture-cell-reference";
        case ProjectOopHirNodeKind::DelegateInvoke: return "delegate-invoke";
        case ProjectOopHirNodeKind::EventAdd: return "event-add";
        case ProjectOopHirNodeKind::EventRemove: return "event-remove";
        case ProjectOopHirNodeKind::EventInvoke: return "event-invoke";
        case ProjectOopHirNodeKind::Unary: return "unary";
        case ProjectOopHirNodeKind::Binary: return "binary";
        case ProjectOopHirNodeKind::Conditional: return "conditional";
        case ProjectOopHirNodeKind::NullCoalescing: return "null-coalescing";
        case ProjectOopHirNodeKind::OptionalCoalescing: return "optional-coalescing";
        case ProjectOopHirNodeKind::NullCoalescingAssignment: return "null-coalescing-assignment";
        case ProjectOopHirNodeKind::Assignment: return "assignment";
        case ProjectOopHirNodeKind::CompoundStore: return "compound-store";
        case ProjectOopHirNodeKind::Conversion: return "conversion";
        case ProjectOopHirNodeKind::RuntimeTypeTest: return "runtime-type-test";
        case ProjectOopHirNodeKind::SafeReferenceCast: return "safe-reference-cast";
        case ProjectOopHirNodeKind::CheckedReferenceCast: return "checked-reference-cast";
        case ProjectOopHirNodeKind::InstanceCall: return "instance-call";
        case ProjectOopHirNodeKind::StaticCall: return "static-call";
        case ProjectOopHirNodeKind::ConstructorInitializerCall: return "constructor-initializer-call";
        case ProjectOopHirNodeKind::ObjectConstruction: return "object-construction";
        case ProjectOopHirNodeKind::Deallocation: return "deallocation";
        case ProjectOopHirNodeKind::ObjectDestruction: return "object-destruction";
        case ProjectOopHirNodeKind::Assert: return "assert";
        case ProjectOopHirNodeKind::Unreachable: return "unreachable";
        case ProjectOopHirNodeKind::Assume: return "assume";
        case ProjectOopHirNodeKind::Panic: return "panic";
        case ProjectOopHirNodeKind::ResultPropagation: return "result-propagation";
        case ProjectOopHirNodeKind::LoopExpression: return "loop-expression";
        case ProjectOopHirNodeKind::MatchExpression: return "match-expression";
        case ProjectOopHirNodeKind::MatchArm: return "match-arm";
        case ProjectOopHirNodeKind::MatchValue: return "match-value";
    }
    return "unknown";
}

std::string ProjectOopHirModel::mangleCallable(
    const NamedTypeDeclaration& type,
    const ClassMemberSymbol& member)
{
    const std::string prefix = member.kind == ClassMemberKind::Constructor
        ? (member.isStatic ? "__nsc_cctor_body_" : "__nsc_ctor_")
        : member.kind == ClassMemberKind::Destructor
            ? "__nsc_dtor_body_"
            : "__nsc_method_";
    // NSC-LIMIT-009: la identidad pasa por el codificador que conserva el `+`, no por `sanitize`.
    // El nombre del MIEMBRO si usa `sanitize`: no puede contener `+`, y cambiarlo moveria todos
    // los simbolos ya existentes.
    std::string result = prefix + compilation::symbolFragmentForIdentityKey(type.identity.key()) +
        "__" + sanitize(member.name) + typeSuffix(member);
    if (member.isControlledSpecialization)
        result += "__specialize_" + stableSymbolSuffix(member.genericTemplateKey);
    return result;
}

bool ProjectOopHirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::string& error,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::size_t workerCount,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel)
{
    return buildImpl(
        nameLookup,
        memberModel,
        inheritanceModel,
        interfaceModel,
        methodBodyModel,
        constructionModel,
        objectLayoutModel,
        error,
        genericConstraintModel,
        workerCount,
        namespaceFunctionModel,
        std::nullopt,
        nullptr);
}

bool ProjectOopHirModel::buildImpl(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::string& error,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::size_t workerCount,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel,
    std::optional<std::size_t> memberFilter,
    const ProjectOopHirModel* lookupSource)
{
    error.clear();
    genericConstraintModel_ = genericConstraintModel;
    lookupSource_ = lookupSource;
    if (built_)
    {
        error = "ProjectOopHirModel instances are single-use";
        return false;
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() || !interfaceModel.isBuilt() ||
        !methodBodyModel.isBuilt() ||
        !constructionModel.isBuilt() || !objectLayoutModel.isBuilt())
    {
        error = "Stage 34 OOP HIR requires completed Stage 24-29 project models";
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;
    methodBodyModel_ = &methodBodyModel;
    namespaceFunctionModel_ = namespaceFunctionModel;
    constructionModel_ = &constructionModel;
    objectLayoutModel_ = &objectLayoutModel;
    nextSyntheticLocalOrdinal_ = methodBodyModel.locals().size();
    nextSyntheticCleanupRegionOrdinal_ = methodBodyModel.cleanupRegions().size();

    if (lookupSource_ == nullptr)
    {
        const auto indexBySyntax = [](auto& index, const auto& values) {
            index.reserve(values.size());
            for (const auto& value : values) index[value.syntax] = &value;
        };
        indexBySyntax(nameBindingBySyntax_, methodBodyModel.nameBindings());
        indexBySyntax(expressionTypeBySyntax_, methodBodyModel.expressionTypes());
        indexBySyntax(callBySyntax_, methodBodyModel.calls());
        indexBySyntax(namespaceFunctionCallBySyntax_, methodBodyModel.namespaceFunctionCalls());
        indexBySyntax(switchBindingBySyntax_, methodBodyModel.switchBindings());
        indexBySyntax(runtimeTypeBindingBySyntax_, methodBodyModel.runtimeTypeBindings());
        indexBySyntax(delegateBindingBySyntax_, methodBodyModel.delegateBindings());
        indexBySyntax(lambdaBindingBySyntax_, methodBodyModel.lambdaBindings());
        indexBySyntax(lambdaValueBindingBySyntax_, methodBodyModel.lambdaValueBindings());
        indexBySyntax(eventBindingBySyntax_, methodBodyModel.eventBindings());
        indexBySyntax(coreBindingBySyntax_, methodBodyModel.coreBindings());
        indexBySyntax(optionalCarrierBindingBySyntax_, methodBodyModel.optionalCarrierBindings());
        indexBySyntax(
            resultPropagationBindingBySyntax_, methodBodyModel.resultPropagationBindings());
        indexBySyntax(rangeSliceBindingBySyntax_, methodBodyModel.rangeSliceBindings());
        indexBySyntax(exceptionBindingBySyntax_, methodBodyModel.exceptionBindings());
        indexBySyntax(cleanupRegionBySyntax_, methodBodyModel.cleanupRegions());
        indexBySyntax(usingBindingBySyntax_, methodBodyModel.usingBindings());
        indexBySyntax(moveBindingBySyntax_, methodBodyModel.moveBindings());
        indexBySyntax(borrowBindingBySyntax_, methodBodyModel.borrowBindings());
        indexBySyntax(structuredExitBySyntax_, methodBodyModel.structuredExits());
        indexBySyntax(constructionBySyntax_, constructionModel.constructions());
        indexBySyntax(localBySyntax_, methodBodyModel.locals());
    }
    functionByMember_.resize(memberModel.members().size());
    if (namespaceFunctionModel_ != nullptr)
        functionByNamespaceFunction_.resize(namespaceFunctionModel_->functions().size());
    const bool regularFunctionsBuiltInParallel = workerCount > 1 && !memberFilter.has_value();
    if (regularFunctionsBuiltInParallel)
    {
        struct RegularFunctionResult final
        {
            // A worker can produce more than one callable for a single member: an async method
            // yields the lazy constructor AND its `$poll` function.
            std::vector<ProjectOopHirFunction> functions;
            std::string error;
            SourceRange errorRange;
            bool success = false;
        };
        std::vector<RegularFunctionResult> results(memberModel.members().size());
        driver::CompilationWorkerPool workers(workerCount);
        for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
        {
            const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
            if ((member.kind != ClassMemberKind::Method &&
                 member.kind != ClassMemberKind::Constructor &&
                 member.kind != ClassMemberKind::Destructor) || member.isAbstract)
            {
                results[memberOrdinal].success = true;
                continue;
            }
            workers.submit([&, memberOrdinal](std::size_t) {
                ProjectOopHirModel worker;
                RegularFunctionResult& result = results[memberOrdinal];
                result.success = worker.buildImpl(
                    nameLookup,
                    memberModel,
                    inheritanceModel,
                    interfaceModel,
                    methodBodyModel,
                    constructionModel,
                    objectLayoutModel,
                    result.error,
                    genericConstraintModel,
                    1,
                    namespaceFunctionModel,
                    memberOrdinal,
                    this);
                result.errorRange = worker.lastErrorRange();
                if (result.success && !worker.functions_.empty())
                    result.functions = std::move(worker.functions_);
            });
        }
        workers.wait();
        const std::size_t localFloor = methodBodyModel.locals().size();
        const std::size_t cleanupFloor = methodBodyModel.cleanupRegions().size();
        for (std::size_t memberOrdinal = 0; memberOrdinal < results.size(); ++memberOrdinal)
        {
            RegularFunctionResult& result = results[memberOrdinal];
            if (!result.success)
            {
                error = std::move(result.error);
                lastErrorRange_ = result.errorRange;
                return false;
            }
            if (!result.functions.empty())
            {
                functionByMember_[memberOrdinal] = functions_.size();
                for (ProjectOopHirFunction& function : result.functions)
                {
                    std::size_t localCount = 0;
                    std::size_t cleanupCount = 0;
                    std::size_t regionCount = 0;
                    scanParallelOrdinals(
                        function.body.get(),
                        localFloor,
                        cleanupFloor,
                        localCount,
                        cleanupCount,
                        regionCount);
                    applyParallelOrdinalOffsets(
                        function.body.get(),
                        localFloor,
                        cleanupFloor,
                        nextSyntheticLocalOrdinal_ - localFloor,
                        nextSyntheticCleanupRegionOrdinal_ - cleanupFloor,
                        nextHirRegionOrdinal_);
                    nextSyntheticLocalOrdinal_ += localCount;
                    nextSyntheticCleanupRegionOrdinal_ += cleanupCount;
                    nextHirRegionOrdinal_ += regionCount;
                    functions_.push_back(std::move(function));
                }
            }
        }
    }

    if (!regularFunctionsBuiltInParallel)
    {
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        if (memberFilter.has_value() && *memberFilter != memberOrdinal) continue;
        const ClassMemberSymbol& member = memberModel.members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor &&
            member.kind != ClassMemberKind::Destructor)
        {
            continue;
        }
        if (member.isAbstract)
        {
            continue;
        }
        // NSC-LIMIT-018. Signatures of a closed generic type are eager. For executables, ordinary
        // template method bodies are analyzed only after a real call or delegate conversion
        // selects them. Library products additionally demand their complete public/protected
        // closed surface because an external consumer is itself the call site. In both cases this
        // exact query prevents HIR from lowering a body for which semantic bindings do not exist.
        constexpr std::string_view kDeferredSpecializationPrefix = "stage44-specialization:";
        const ast::AstNode* declaringSyntaxForDemand =
            member.declaringTypeOrdinal < nameLookup.namedTypes().size()
                ? nameLookup.namedTypes()[member.declaringTypeOrdinal].syntax
                : nullptr;
        const bool deferredGenericMethod = member.kind == ClassMemberKind::Method &&
            !member.isVirtual && !member.isOverride && !member.isAbstract &&
            ((member.syntax != nullptr &&
              member.syntax->auxiliaryText().starts_with(kDeferredSpecializationPrefix)) ||
             (declaringSyntaxForDemand != nullptr &&
              declaringSyntaxForDemand->auxiliaryText().starts_with(kDeferredSpecializationPrefix)));
        if (deferredGenericMethod && !methodBodyModel.callableBodyAnalyzed(memberOrdinal))
        {
            continue;
        }
        // NSC-LIMIT-007. La misma pregunta que hace el modelo de cuerpos, y hace falta aqui
        // TAMBIEN: este bucle vuelve a recorrer todos los miembros y baja sus cuerpos, asi que
        // saltarla solo alla dejaria un miembro con cuerpo sin analizar del que HIR pediria
        // bindings que nadie escribio.
        //
        // Se preguntan DOS nodos, y hacen falta los dos. Para un METODO generico la invalidez
        // esta en su propio nodo; para un TIPO generico --`Caja<int>` con `where T : class`--
        // esta en el nodo del tipo, y sus metodos son hijos suyos, asi que preguntar solo por
        // `member.syntax` no la veria nunca.
        if (genericConstraintModel_ != nullptr &&
            (!genericConstraintModel_->specializationSatisfiesConstraints(member.syntax) ||
             (member.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
              !genericConstraintModel_->specializationSatisfiesConstraints(
                  nameLookup.namedTypes()[member.declaringTypeOrdinal].syntax))))
        {
            continue;
        }
        if (member.kind == ClassMemberKind::Destructor && member.isPrecompiled)
        {
            // The consumer needs the declaration to build the destruction plan, but never imports
            // or regenerates the destructor body. Destruction crosses the producer-owned entry
            // stored in the runtime class descriptor.
            continue;
        }
        if ((member.kind == ClassMemberKind::Method ||
             member.kind == ClassMemberKind::Constructor) &&
            member.isPrecompiled && !member.isImportedC)
        {
            // The declaration came from .nsmeta and participates in the ordinary NativeSharp
            // overload set, but its implementation is already in the referenced .lib/DLL. The
            // The semantic signature came from synthetic source, but the physical identity was
            // produced once by the library producer and transported through NSMETA.  Recomputing
            // it here is incorrect when a closed generic argument acquired a qualified spelling
            // during source reconstruction.
            ProjectOopHirFunction imported;
            imported.memberOrdinal = memberOrdinal;
            imported.declaringTypeOrdinal = member.declaringTypeOrdinal;
            imported.constructor = member.kind == ClassMemberKind::Constructor;
            imported.destructor = false;
            imported.isStatic = member.isStatic;
            imported.referenceReturn = member.isReferenceReturn;
            imported.isNoHeap = member.isNoHeap;
            imported.effects = member.effects;
            // NSMETA declarations carry optimization policy as part of their callable contract.
            // In particular, `noinline` has no transported body but must still constrain calls
            // emitted by this consumer.
            imported.inliningPolicy = member.inliningPolicy;
            imported.isImportedNative = true;
            if (member.precompiledMangledName.empty())
            {
                error = "precompiled callable '" + member.signatureKey +
                    "' has no physical symbol bound from NSMETA";
                return false;
            }
            imported.mangledName = member.precompiledMangledName;
            imported.returnTypeKey = imported.constructor ? "builtin:void" : member.declaredTypeKey;
            imported.errorTypeKey = member.declaredErrorTypeKey;
            imported.returnOwnership = imported.constructor
                ? sema::OwnershipKind::None
                : member.declaredOwnership;
            for (const MethodParameterSymbol& parameter : member.parameters)
            {
                imported.parameterTypeKeys.push_back(parameter.typeKey);
                imported.parameterPassKinds.push_back(parameter.passKind);
                imported.parameterOwnerships.push_back(parameter.ownership);
            }
            functionByMember_[memberOrdinal] = functions_.size();
            functions_.push_back(std::move(imported));
            continue;
        }
        if (member.isImportedC)
        {
            // Interop Foundation: an imported C function is a DECLARATION by definition -- the
            // body lives in the named DLL. Record the function (C symbol as its mangled name, no
            // body) so ordinary call resolution and the backend's declaration pass see it, and
            // move on before the body requirement below.
            ProjectOopHirFunction imported;
            imported.memberOrdinal = memberOrdinal;
            imported.declaringTypeOrdinal = member.declaringTypeOrdinal;
            imported.isStatic = true;
            imported.isImportedC = true;
            imported.isVariadic = member.isVariadic;
            imported.importDllName = member.importDllName;
            imported.importRequiresRuntimeBinding = member.importRequiresRuntimeBinding;
            imported.cAbiName = member.name;
            // C1.2 "import identity": `cAbiName` is always the EXTERNAL symbol (what the DLL or
            // the archive exports); `mangledName` is the INTERNAL name the module and every call
            // site use. In the ordinary case the two coincide, which is exactly what makes the
            // link resolve the import statically by name -- unchanged, and what the certified
            // user32/kernel32 scenarios exercise. When one symbol name is claimed by two
            // libraries the two MUST diverge: a single LLVM module cannot hold two functions
            // called `Init`, and asking for it silently produced `Init.1`, which the linker then
            // could not resolve (measured). The internal name is derived from the whole identity,
            // so `a.dll!Init` and `b.dll!Init` are two distinct functions from here on.
            imported.mangledName = member.importRequiresRuntimeBinding
                ? importIdentityInternalName(member.importDllName, member.name)
                : member.name;
            imported.returnTypeKey = member.declaredTypeKey;
            imported.returnOwnership = sema::OwnershipKind::None;
            for (const MethodParameterSymbol& parameter : member.parameters)
            {
                imported.parameterTypeKeys.push_back(parameter.typeKey);
                imported.parameterPassKinds.push_back(parameter.passKind);
                imported.parameterOwnerships.push_back(parameter.ownership);
            }
            functionByMember_[memberOrdinal] = functions_.size();
            functions_.push_back(std::move(imported));
            continue;
        }
        if (member.syntax == nullptr || member.syntax->first() == nullptr)
        {
            error = "Stage 35 concrete OOP callable has no body: " + member.signatureKey;
            return false;
        }
        if (member.declaringTypeOrdinal >= nameLookup.namedTypes().size())
        {
            error = "Stage 34 OOP callable refers to invalid declaring type ordinal";
            return false;
        }

        ProjectOopHirFunction function;
        function.memberOrdinal = memberOrdinal;
        function.declaringTypeOrdinal = member.declaringTypeOrdinal;
        function.constructor = member.kind == ClassMemberKind::Constructor;
        function.destructor = member.kind == ClassMemberKind::Destructor;
        function.isStatic = member.isStatic;
        function.referenceReturn = member.isReferenceReturn;
        function.isNoHeap = member.isNoHeap;
        function.effects = member.effects;
        function.inliningPolicy = member.inliningPolicy;
        function.isExportedC = member.isExportedC;
        function.cAbiName = member.cAbiName;
        constexpr std::string_view kSpecializationPrefix = "stage44-specialization:";
        const auto specializationKey = [&](const ast::AstNode* syntax) -> std::string
        {
            if (syntax == nullptr || !syntax->auxiliaryText().starts_with(kSpecializationPrefix))
                return {};
            std::string key = syntax->auxiliaryText().substr(kSpecializationPrefix.size());
            if (const std::size_t metadata = key.find(";nsc-meta:"); metadata != std::string::npos)
                key.resize(metadata);
            return key;
        };
        if (member.syntax != nullptr && member.syntax->auxiliaryText().starts_with(kSpecializationPrefix))
        {
            function.genericSpecializationKey = specializationKey(member.syntax);
        }
        else
        {
            const ast::AstNode* declaringSyntax = nameLookup.namedTypes()[member.declaringTypeOrdinal].syntax;
            if (declaringSyntax != nullptr && declaringSyntax->auxiliaryText().starts_with(kSpecializationPrefix))
                function.genericSpecializationKey = specializationKey(declaringSyntax);
        }
        function.mangledName = mangleCallable(nameLookup.namedTypes()[member.declaringTypeOrdinal], member);
        function.returnTypeKey = (function.constructor || function.destructor)
            ? "builtin:void"
            : member.declaredTypeKey;
        function.errorTypeKey = member.declaredErrorTypeKey;
        function.frameStorage = member.frameStorage;
        function.returnOwnership = (function.constructor || function.destructor)
            ? sema::OwnershipKind::None
            : member.declaredOwnership;
        for (const MethodParameterSymbol& parameter : member.parameters)
        {
            function.parameterNames.push_back(parameter.name);
            function.parameterTypeKeys.push_back(parameter.typeKey);
            function.parameterPassKinds.push_back(parameter.passKind);
            function.parameterOwnerships.push_back(parameter.ownership);
        }
        for (std::size_t localOrdinal = 0; localOrdinal < methodBodyModel.locals().size(); ++localOrdinal)
        {
            const MethodLocalSymbol& local = methodBodyModel.locals()[localOrdinal];
            if (local.methodMemberOrdinal == memberOrdinal && !local.lambdaOrdinal.has_value())
            {
                function.localOrdinals.push_back(localOrdinal);
            }
        }
        activeHirCleanupRegions_.clear();
        std::vector<std::unique_ptr<ProjectOopHirNode>> ownedParameterCleanupNodes;
        for (std::size_t cleanupOrdinal = 0; cleanupOrdinal < methodBodyModel.cleanupRegions().size(); ++cleanupOrdinal)
        {
            const MethodCleanupRegion& region = methodBodyModel.cleanupRegions()[cleanupOrdinal];
            if (region.methodMemberOrdinal != memberOrdinal ||
                (region.kind != MethodCleanupRegionKind::OwnedParameterDestroy &&
                 region.kind != MethodCleanupRegionKind::ValueParameterDrop) ||
                !region.resourceParameterOrdinal.has_value())
            {
                continue;
            }
            const std::size_t parameterOrdinal = *region.resourceParameterOrdinal;
            if (parameterOrdinal >= member.parameters.size())
            {
                error = "Stage 50.8 owned-parameter cleanup has an invalid parameter ordinal";
                return false;
            }
            activeHirCleanupRegions_.push_back(cleanupOrdinal);
            auto cleanup = makeNode(
                ProjectOopHirNodeKind::OwnedParameterCleanup,
                member.syntax,
                member.parameters[parameterOrdinal].typeKey);
            cleanup->parameterOrdinal = parameterOrdinal;
            cleanup->cleanupRegionOrdinal = cleanupOrdinal;
            cleanup->automaticDestruction =
                region.kind == MethodCleanupRegionKind::ValueParameterDrop;
            ownedParameterCleanupNodes.push_back(std::move(cleanup));
        }

        loweringAsyncBody_ = function.effects.contains(sema::Effect::Async);
        function.body = lowerBlock(member.syntax->first(), memberOrdinal, error);
        activeHirCleanupRegions_.clear();
        if (function.body == nullptr)
        {
            return false;
        }
        if (!ownedParameterCleanupNodes.empty())
        {
            function.body->children.insert(
                function.body->children.begin(),
                std::make_move_iterator(ownedParameterCleanupNodes.begin()),
                std::make_move_iterator(ownedParameterCleanupNodes.end()));
        }
        if (function.constructor && !function.isStatic)
        {
            std::vector<std::unique_ptr<ProjectOopHirNode>> prefix;
            const ConstructorInitializerBinding* initializerBinding =
                constructionModel.constructorInitializerBindingForMember(memberOrdinal);
            if (member.isNoHeap && initializerBinding != nullptr &&
                (initializerBinding->resolution == ConstructorInitializerResolutionKind::ResolvedExplicit ||
                 initializerBinding->resolution == ConstructorInitializerResolutionKind::ResolvedImplicitDefault))
            {
                if (!initializerBinding->selectedEntryConstructorMemberOrdinal.has_value() ||
                    *initializerBinding->selectedEntryConstructorMemberOrdinal >= memberModel.members().size())
                {
                    error = "NS3030: noheap constructor '" + member.signatureKey +
                        "' cannot call an implicit constructor without a noheap contract";
                    return false;
                }
                const ClassMemberSymbol& selectedConstructor = memberModel.members()[
                    *initializerBinding->selectedEntryConstructorMemberOrdinal];
                if (!selectedConstructor.isNoHeap)
                {
                    error = "NS3030: noheap constructor '" + member.signatureKey +
                        "' cannot call non-noheap constructor '" +
                        selectedConstructor.signatureKey + "'";
                    return false;
                }
            }
            if (initializerBinding != nullptr &&
                (initializerBinding->resolution == ConstructorInitializerResolutionKind::ResolvedExplicit ||
                 initializerBinding->resolution == ConstructorInitializerResolutionKind::ResolvedImplicitDefault) &&
                initializerBinding->selectedEntryConstructorMemberOrdinal.has_value() &&
                initializerBinding->targetTypeOrdinal.has_value())
            {
                auto initializerCall = makeNode(
                    ProjectOopHirNodeKind::ConstructorInitializerCall,
                    initializerBinding->initializerSyntax,
                    "builtin:void");
                initializerCall->memberOrdinal = initializerBinding->selectedEntryConstructorMemberOrdinal;
                initializerCall->baseReceiver =
                    initializerBinding->kind != ConstructorInitializerKind::ExplicitThis;
                initializerCall->children.push_back(makeNode(
                    ProjectOopHirNodeKind::ThisValue,
                    initializerBinding->initializerSyntax,
                    inheritanceModel.typeKeyForOrdinal(*initializerBinding->targetTypeOrdinal)));
                bool initializerHasOwnedTemporary = false;
                if (initializerBinding->resolution == ConstructorInitializerResolutionKind::ResolvedExplicit &&
                    initializerBinding->initializerSyntax != nullptr)
                {
                    if (!initializerBinding->selectedEntryConstructorMemberOrdinal.has_value() ||
                        *initializerBinding->selectedEntryConstructorMemberOrdinal >= memberModel.members().size())
                    {
                        error = "NSC-LIMIT-020 selected constructor initializer has no callable metadata";
                        return false;
                    }
                    const ClassMemberSymbol& selectedConstructor = memberModel.members()[
                        *initializerBinding->selectedEntryConstructorMemberOrdinal];
                    ProjectOopHirNode* expandedPack = nullptr;
                    for (std::size_t index = 0; index < initializerBinding->initializerSyntax->children().size(); ++index)
                    {
                        const ast::AstNode* argumentSyntax = initializerBinding->initializerSyntax->children()[index];
                        auto argument = lowerExpression(argumentSyntax, memberOrdinal, false, error);
                        if (argument == nullptr)
                        {
                            return false;
                        }
                        if (index < initializerBinding->selectedConversions.size() &&
                            initializerBinding->selectedConversions[index].kind != ProjectConversionKind::Identity)
                        {
                            argument = wrapImplicitConversion(
                                std::move(argument), initializerBinding->selectedConversions[index], argumentSyntax);
                        }
                        if (index >= initializerBinding->selectedArgumentParameterOrdinals.size())
                        {
                            error = "named constructor initializer argument has no parameter ordinal";
                            return false;
                        }
                        const std::size_t parameterIndex =
                            initializerBinding->selectedArgumentParameterOrdinals[index];
                        const bool packElement =
                            index < initializerBinding->selectedParameterPackElements.size() &&
                            initializerBinding->selectedParameterPackElements[index];
                        if (packElement)
                        {
                            if (parameterIndex >= selectedConstructor.parameters.size())
                            {
                                error = "T01-F02 constructor initializer pack has no parameter";
                                return false;
                            }
                            const std::string& elementType =
                                selectedConstructor.parameters[parameterIndex].parameterPackElementTypeKey;
                            if (elementType.empty())
                            {
                                error = "T01-F02 constructor initializer pack lost its element type";
                                return false;
                            }
                            if (expandedPack == nullptr)
                            {
                                auto pack = makeNode(
                                    ProjectOopHirNodeKind::ParameterPack,
                                    argumentSyntax,
                                    selectedConstructor.parameters[parameterIndex].typeKey);
                                pack->elementTypeKey = elementType;
                                expandedPack = pack.get();
                                initializerCall->children.push_back(std::move(pack));
                                initializerCall->argumentParameterOrdinals.push_back(parameterIndex);
                            }
                            expandedPack->children.push_back(std::move(argument));
                            continue;
                        }
                        if (std::ranges::find(
                                initializerBinding->selectedFullExpressionBorrowArgumentOrdinals,
                                index) != initializerBinding->selectedFullExpressionBorrowArgumentOrdinals.end())
                        {
                            argument = wrapFreshOwnedTemporaryBorrow(
                                std::move(argument), argumentSyntax, initializerHasOwnedTemporary);
                        }
                        initializerCall->children.push_back(std::move(argument));
                        initializerCall->argumentParameterOrdinals.push_back(parameterIndex);
                    }
                    if (!initializerBinding->selectedDefaultArguments.empty())
                    {
                        for (std::size_t index = 0;
                             index < initializerBinding->selectedDefaultArguments.size(); ++index)
                        {
                            if (index >= initializerBinding->selectedDefaultArgumentParameterOrdinals.size())
                            {
                                error = "selected constructor initializer default has no parameter ordinal";
                                return false;
                            }
                            const std::size_t parameterIndex =
                                initializerBinding->selectedDefaultArgumentParameterOrdinals[index];
                            if (parameterIndex >= selectedConstructor.parameters.size())
                            {
                                error = "NSC-LIMIT-020 constructor initializer default has no parameter";
                                return false;
                            }
                            auto argument = lowerDefaultArgument(
                                initializerBinding->selectedDefaultArguments[index],
                                selectedConstructor.parameters[parameterIndex].typeKey,
                                error);
                            if (argument == nullptr) return false;
                            initializerCall->children.push_back(std::move(argument));
                            initializerCall->argumentParameterOrdinals.push_back(parameterIndex);
                        }
                    }
                    if (initializerBinding->selectedSynthesizesEmptyParameterPack)
                    {
                        if (!initializerBinding->selectedParameterPackOrdinal.has_value() ||
                            *initializerBinding->selectedParameterPackOrdinal >=
                                selectedConstructor.parameters.size())
                        {
                            error = "T01-F02 empty constructor initializer pack has no parameter";
                            return false;
                        }
                        const std::size_t parameterIndex =
                            *initializerBinding->selectedParameterPackOrdinal;
                        const std::string& elementType =
                            selectedConstructor.parameters[parameterIndex].parameterPackElementTypeKey;
                        if (elementType.empty())
                        {
                            error = "T01-F02 empty constructor initializer pack lost its element type";
                            return false;
                        }
                        auto pack = makeNode(
                            ProjectOopHirNodeKind::ParameterPack,
                            initializerBinding->initializerSyntax,
                            selectedConstructor.parameters[parameterIndex].typeKey);
                        pack->elementTypeKey = elementType;
                        initializerCall->children.push_back(std::move(pack));
                        initializerCall->argumentParameterOrdinals.push_back(parameterIndex);
                    }
                }
                prefix.push_back(wrapFullExpressionCleanup(
                    std::move(initializerCall), initializerBinding->initializerSyntax,
                    initializerHasOwnedTemporary));
            }

            // A this(...) delegating constructor must not execute this type's field initializers:
            // the terminal constructor in the delegation chain executes them exactly once.
            const bool delegatesToThis = initializerBinding != nullptr &&
                initializerBinding->kind == ConstructorInitializerKind::ExplicitThis;
            if (!delegatesToThis)
            {
                // When the immediate base has only an implicit constructor, Macro A can elide the
                // physical callable and jump to the nearest explicit ancestor.  Stage 37 must still
                // execute field initializers for every skipped implicit base, in base-to-derived order.
                if (initializerBinding != nullptr &&
                    initializerBinding->resolution == ConstructorInitializerResolutionKind::ResolvedImplicitDefault &&
                    initializerBinding->targetTypeOrdinal.has_value())
                {
                    for (const std::size_t typeOrdinal : implicitInitializerTypes(
                             *initializerBinding->targetTypeOrdinal,
                             initializerBinding->selectedEntryConstructorMemberOrdinal))
                    {
                        appendInstanceFieldInitializers(typeOrdinal, prefix, error, memberOrdinal);
                        if (!error.empty())
                        {
                            return false;
                        }
                    }
                }
                appendInstanceFieldInitializers(
                    member.declaringTypeOrdinal, prefix, error, memberOrdinal);
                if (!error.empty())
                {
                    return false;
                }
            }

            function.body->children.insert(
                function.body->children.begin(),
                std::make_move_iterator(prefix.begin()),
                std::make_move_iterator(prefix.end()));
        }

        // Item 323. Promotion is intentionally a post-HIR proof: every use of each local is now
        // visible, including nested control flow and synthesized work. Only a trivial, local class
        // construction whose identity never becomes observable may exchange owned heap storage
        // for frame storage. The one-mebibyte cap matches explicit stackalloc's fixed budget.
        std::vector<ProjectOopHirNode*> localDeclarations;
        collectLocalDeclarations(function.body.get(), localDeclarations);
        constexpr std::uint64_t kMaximumPromotedObjectBytes = std::uint64_t{1} << 20;
        for (ProjectOopHirNode* declaration : localDeclarations)
        {
            if (declaration == nullptr || !declaration->localOrdinal.has_value() ||
                declaration->children.size() != 1 || declaration->children.front() == nullptr)
            {
                continue;
            }
            ProjectOopHirNode* construction = declaration->children.front().get();
            if (construction->kind != ProjectOopHirNodeKind::ObjectConstruction ||
                construction->argumentCount != 0 || !construction->children.empty() ||
                !construction->constructionOrdinal.has_value() ||
                !construction->targetTypeOrdinal.has_value())
            {
                continue;
            }
            const MethodLocalSymbol* local = localSymbol(declaration->syntax);
            const ObjectConstructionBinding* binding = constructionBinding(construction->syntax);
            const std::size_t targetTypeOrdinal = *construction->targetTypeOrdinal;
            const ClassObjectLayout* layout = objectLayoutModel_ != nullptr
                ? objectLayoutModel_->layoutForType(targetTypeOrdinal) : nullptr;
            if (local == nullptr || local->isStatic || !local->isRaii || local->isReference ||
                local->objectLifetimeOwnership != sema::OwnershipKind::Owned ||
                local->storageOwnership != sema::OwnershipKind::Owned ||
                binding == nullptr || !binding->selectedImplicitDefault ||
                binding->selectedConstructorMemberOrdinal.has_value() ||
                binding->implicitDefaultBaseConstructorMemberOrdinal.has_value() ||
                layout == nullptr || layout->baseTypeOrdinal.has_value() || layout->size == 0 ||
                layout->size > kMaximumPromotedObjectBytes ||
                targetTypeOrdinal >= nameLookup.namedTypes().size() ||
                nameLookup.namedTypes()[targetTypeOrdinal].isPrecompiled)
            {
                continue;
            }
            bool hasNontrivialLifetimeMember = false;
            for (const ClassMemberSymbol& targetMember : memberModel.members())
            {
                if (targetMember.declaringTypeOrdinal != targetTypeOrdinal) continue;
                if (targetMember.kind == ClassMemberKind::Destructor ||
                    (targetMember.kind == ClassMemberKind::Field && !targetMember.isStatic &&
                     targetMember.declaredOwnership == sema::OwnershipKind::Owned))
                {
                    hasNontrivialLifetimeMember = true;
                    break;
                }
            }
            if (hasNontrivialLifetimeMember ||
                !stackClassLocalUsesAreSafe(
                    function.body.get(), declaration, *declaration->localOrdinal))
            {
                continue;
            }
            declaration->stackPromotedObject = true;
            declaration->storageOwnership = sema::OwnershipKind::None;
            construction->stackPromotedObject = true;
            construction->localOrdinal = declaration->localOrdinal;
            construction->storageOwnership = sema::OwnershipKind::None;
        }

        // Object construction is deferred until after the proof so a noheap callable can use a
        // proven frame object. Every construction that remains heap-backed keeps item 321's exact
        // diagnostic and contract.
        if (function.isNoHeap)
        {
            if (const ProjectOopHirNode* allocation =
                    firstUnpromotedObjectConstruction(function.body.get()))
            {
                error = "NS3030: noheap callable '" + member.name +
                    "' cannot contain direct heap allocation '" +
                    std::string(projectOopHirNodeKindName(allocation->kind)) + "'";
                return false;
            }
        }
        std::unique_ptr<ProjectOopHirFunction> poll;
        std::unique_ptr<ProjectOopHirFunction> moveNext;
        std::unique_ptr<ProjectOopHirFunction> drop;
        // NS304-T07-F02: an async generator is ONE machine (await + yield) — the async-generator
        // split produces MoveNext (future) + poll + drop.
        const bool asyncGenerator =
            !function.constructor && !function.destructor &&
            function.effects.contains(sema::Effect::Generator) &&
            function.effects.contains(sema::Effect::Async);
        if (asyncGenerator)
        {
            auto split = splitAsyncGeneratorFunction(function, error);
            if (split.size() != 3U) return false;
            moveNext = std::move(split[0]);
            poll = std::move(split[1]);
            drop = std::move(split[2]);
        }
        else
        {
            if (!function.constructor && !function.destructor &&
                function.effects.contains(sema::Effect::Async))
            {
                poll = splitAsyncFunction(function, error);
                if (poll == nullptr) return false;
            }
            if (!function.constructor && !function.destructor &&
                function.effects.contains(sema::Effect::Generator))
            {
                auto split = splitGeneratorFunction(function, error);
                if (split.first == nullptr) return false;
                moveNext = std::move(split.first);
                drop = std::move(split.second);
            }
        }
        functionByMember_[memberOrdinal] = functions_.size();
        functions_.push_back(std::move(function));
        if (poll) functions_.push_back(std::move(*poll));
        if (moveNext) functions_.push_back(std::move(*moveNext));
        if (drop) functions_.push_back(std::move(*drop));
    }
    }

    if (!memberFilter.has_value() && namespaceFunctionModel_ != nullptr)
    {
        const std::size_t callableBase = memberModel.members().size();
        for (std::size_t functionOrdinal = 0;
             functionOrdinal < namespaceFunctionModel_->functions().size();
             ++functionOrdinal)
        {
            const NamespaceFunctionSymbol& symbol =
                namespaceFunctionModel_->functions()[functionOrdinal];
            const std::size_t callableOrdinal = callableBase + functionOrdinal;

            ProjectOopHirFunction function;
            function.memberOrdinal = callableOrdinal;
            function.namespaceFunctionOrdinal = functionOrdinal;
            function.declaringTypeOrdinal = nameLookup.namedTypes().size();
            function.isStatic = true;
            function.inliningPolicy = symbol.inliningPolicy;
            function.referenceReturn = symbol.isReferenceReturn;
            function.isVariadic = symbol.isVariadic;
            function.isExportedC = symbol.isExportedC;
            if (symbol.isExportedC) function.cAbiName = symbol.name;
            function.mangledName = symbol.mangledName;
            function.debugDisplayName = symbol.namespaceName.empty()
                ? symbol.name
                : symbol.namespaceName + "." + symbol.name;
            function.returnTypeKey = symbol.declaredTypeKey;
            function.errorTypeKey = symbol.declaredErrorTypeKey;
            function.effects = symbol.effects;
            function.returnOwnership = symbol.declaredOwnership;
            for (const MethodParameterSymbol& parameter : symbol.parameters)
            {
                function.parameterNames.push_back(parameter.name);
                function.parameterTypeKeys.push_back(parameter.typeKey);
                function.parameterPassKinds.push_back(parameter.passKind);
                function.parameterOwnerships.push_back(parameter.ownership);
            }

            if (symbol.definitionSyntax == nullptr)
            {
                if (symbol.isPrecompiled && !symbol.isExternC)
                {
                    function.isImportedNative = true;
                    functionByNamespaceFunction_[functionOrdinal] = functions_.size();
                    functions_.push_back(std::move(function));
                    continue;
                }
                if (!symbol.isExternC)
                {
                    // A prototype that is never selected emits no code. A selected call or
                    // delegate conversion was already rejected by method-body binding.
                    continue;
                }
                function.isImportedC = true;
                function.cAbiName = symbol.name;
                function.mangledName = symbol.name;
                functionByNamespaceFunction_[functionOrdinal] = functions_.size();
                functions_.push_back(std::move(function));
                continue;
            }
            if (!methodBodyModel.namespaceFunctionBodyAnalyzed(functionOrdinal))
            {
                error = "namespace function '" + symbol.signatureKey +
                    "' reached HIR without analyzed body bindings";
                return false;
            }
            if (symbol.definitionSyntax->first() == nullptr)
            {
                error = "namespace function '" + symbol.signatureKey +
                    "' has no body syntax";
                return false;
            }

            for (std::size_t localOrdinal = 0;
                 localOrdinal < methodBodyModel.locals().size();
                 ++localOrdinal)
            {
                const MethodLocalSymbol& local = methodBodyModel.locals()[localOrdinal];
                if (local.methodMemberOrdinal == callableOrdinal && !local.lambdaOrdinal.has_value())
                    function.localOrdinals.push_back(localOrdinal);
            }

            activeHirCleanupRegions_.clear();
            std::vector<std::unique_ptr<ProjectOopHirNode>> ownedParameterCleanupNodes;
            for (std::size_t cleanupOrdinal = 0;
                 cleanupOrdinal < methodBodyModel.cleanupRegions().size();
                 ++cleanupOrdinal)
            {
                const MethodCleanupRegion& region = methodBodyModel.cleanupRegions()[cleanupOrdinal];
                if (region.methodMemberOrdinal != callableOrdinal ||
                    (region.kind != MethodCleanupRegionKind::OwnedParameterDestroy &&
                     region.kind != MethodCleanupRegionKind::ValueParameterDrop) ||
                    !region.resourceParameterOrdinal.has_value())
                {
                    continue;
                }
                const std::size_t parameterOrdinal = *region.resourceParameterOrdinal;
                if (parameterOrdinal >= symbol.parameters.size())
                {
                    error = "namespace function owned-parameter cleanup has an invalid parameter ordinal";
                    return false;
                }
                activeHirCleanupRegions_.push_back(cleanupOrdinal);
                auto cleanup = makeNode(
                    ProjectOopHirNodeKind::OwnedParameterCleanup,
                    symbol.definitionSyntax,
                    symbol.parameters[parameterOrdinal].typeKey);
                cleanup->parameterOrdinal = parameterOrdinal;
                cleanup->cleanupRegionOrdinal = cleanupOrdinal;
                cleanup->automaticDestruction =
                    region.kind == MethodCleanupRegionKind::ValueParameterDrop;
                ownedParameterCleanupNodes.push_back(std::move(cleanup));
            }

            loweringAsyncBody_ = function.effects.contains(sema::Effect::Async);
            function.body = lowerBlock(symbol.definitionSyntax->first(), callableOrdinal, error);
            activeHirCleanupRegions_.clear();
            if (function.body == nullptr) return false;
            if (!ownedParameterCleanupNodes.empty())
            {
                function.body->children.insert(
                    function.body->children.begin(),
                    std::make_move_iterator(ownedParameterCleanupNodes.begin()),
                    std::make_move_iterator(ownedParameterCleanupNodes.end()));
            }
            std::unique_ptr<ProjectOopHirFunction> poll;
            if (function.effects.contains(sema::Effect::Async))
            {
                poll = splitAsyncFunction(function, error);
                if (poll == nullptr) return false;
            }
            functionByNamespaceFunction_[functionOrdinal] = functions_.size();
            functions_.push_back(std::move(function));
            if (poll) functions_.push_back(std::move(*poll));
        }
    }
    if (memberFilter.has_value())
    {
        built_ = true;
        return true;
    }
    // Stage 43 lowers every target-typed lambda as a synthetic native callable.  Capturing
    // lambdas use the Stage 42 receiver slot as a closure-environment pointer; non-capturing
    // lambdas remain ordinary static callables with a null delegate receiver.
    for (const MethodLambdaBinding& lambda : methodBodyModel.lambdaBindings())
    {
        if (lambda.ownerMemberOrdinal >= memberModel.members().size())
        {
            error = "Stage 43 lambda owner member ordinal is invalid";
            return false;
        }
        const ClassMemberSymbol& owner = memberModel.members()[lambda.ownerMemberOrdinal];
        ProjectOopHirFunction function;
        function.memberOrdinal = lambda.ownerMemberOrdinal;
        function.declaringTypeOrdinal = owner.declaringTypeOrdinal;
        function.isLambda = true;
        function.lambdaOrdinal = lambda.lambdaOrdinal;
        function.hasClosureEnvironment = !lambda.captures.empty();
        function.callableTrait = lambda.callableTrait;
        function.closureLayoutSize = lambda.closureLayout.size;
        function.closureLayoutAlignment = lambda.closureLayout.alignment;
        function.isStatic = !function.hasClosureEnvironment;
        function.mangledName = lambda.generatedName;
        function.returnTypeKey = lambda.returnTypeKey;
        function.errorTypeKey = lambda.errorTypeKey;
        if (!lambda.errorTypeKey.empty())
            function.effects = function.effects.with(sema::Effect::Throws);
        function.parameterNames = lambda.parameterNames;
        function.parameterTypeKeys = lambda.parameterTypeKeys;
        function.parameterPassKinds.assign(
            lambda.parameterTypeKeys.size(), ParameterPassKind::Value);
        for (const MethodLambdaCapture& capture : lambda.captures)
        {
            function.captureTypeKeys.push_back(capture.typeKey);
            function.captureNames.push_back(capture.name);
            function.captureSourceKinds.push_back(capture.sourceKind);
            function.captureModes.push_back(capture.mode);
            function.captureOwnsValues.push_back(capture.ownsCapturedValue);
        }
        for (std::size_t localOrdinal = 0; localOrdinal < methodBodyModel.locals().size(); ++localOrdinal)
        {
            const MethodLocalSymbol& local = methodBodyModel.locals()[localOrdinal];
            if (local.lambdaOrdinal == lambda.lambdaOrdinal && !local.lambdaParameterOrdinal.has_value())
                function.localOrdinals.push_back(localOrdinal);
        }

        currentLambda_ = &lambda;
        if (lambda.blockBody)
        {
            function.body = lowerBlock(lambda.syntax->first(), lambda.ownerMemberOrdinal, error);
        }
        else
        {
            auto block = makeNode(ProjectOopHirNodeKind::Block, lambda.syntax, "builtin:void");
            auto value = lowerExpression(lambda.syntax->first(), lambda.ownerMemberOrdinal, false, error);
            if (value != nullptr)
            {
                const MethodExpressionTypeBinding* sourceType = expressionType(lambda.syntax->first());
                if (lambda.returnTypeKey == "builtin:void")
                {
                    auto statement = makeNode(ProjectOopHirNodeKind::ExpressionStatement, lambda.syntax->first(), "builtin:void");
                    statement->children.push_back(std::move(value));
                    block->children.push_back(std::move(statement));
                }
                else
                {
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey, lambda.returnTypeKey,
                        sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
                    if (!conversion.exists())
                    {
                        error = "Stage 43 lambda body lost its validated return conversion";
                    }
                    else
                    {
                        if (conversion.kind != ProjectConversionKind::Identity)
                            value = wrapImplicitConversion(std::move(value), conversion, lambda.syntax->first());
                        auto statement = makeNode(ProjectOopHirNodeKind::Return, lambda.syntax->first(), "builtin:void");
                        statement->children.push_back(std::move(value));
                        block->children.push_back(std::move(statement));
                    }
                }
            }
            function.body = std::move(block);
        }
        currentLambda_ = nullptr;
        if (function.body == nullptr || !error.empty()) return false;
        functions_.push_back(std::move(function));
    }

    // E11 closure cells belong to the callable that creates the lambda.  Mark the creator's
    // locals/parameters here, after regular and synthetic functions both exist, so LLVM can
    // heap-home those variables once and every delegate can retain the same cell.
    const auto appendUnique = [](std::vector<std::size_t>& values, std::size_t value)
    {
        if (std::ranges::find(values, value) == values.end()) values.push_back(value);
    };
    for (const MethodLambdaBinding& lambda : methodBodyModel.lambdaBindings())
    {
        if (lambda.captures.empty()) continue;
        ProjectOopHirFunction* creator = nullptr;
        if (lambda.parentLambdaOrdinal.has_value())
        {
            for (ProjectOopHirFunction& candidate : functions_)
            {
                if (candidate.isLambda && candidate.lambdaOrdinal == lambda.parentLambdaOrdinal)
                {
                    creator = &candidate;
                    break;
                }
            }
        }
        else if (lambda.ownerMemberOrdinal < functionByMember_.size() &&
                 functionByMember_[lambda.ownerMemberOrdinal].has_value())
        {
            creator = &functions_[*functionByMember_[lambda.ownerMemberOrdinal]];
        }
        if (creator == nullptr)
        {
            error = "E11 lambda creator function is unavailable";
            return false;
        }

        for (const MethodLambdaCapture& capture : lambda.captures)
        {
            switch (capture.sourceKind)
            {
                case MethodLambdaCaptureSourceKind::OriginalLocal:
                case MethodLambdaCaptureSourceKind::ParentLocal:
                    if (capture.sourceOrdinal.has_value())
                        appendUnique(creator->capturedLocalOrdinals, *capture.sourceOrdinal);
                    break;
                case MethodLambdaCaptureSourceKind::OriginalParameter:
                case MethodLambdaCaptureSourceKind::ParentParameter:
                    if (capture.sourceOrdinal.has_value())
                        appendUnique(creator->capturedParameterOrdinals, *capture.sourceOrdinal);
                    break;
                case MethodLambdaCaptureSourceKind::OriginalThis:
                    creator->capturesThisCell = true;
                    break;
                case MethodLambdaCaptureSourceKind::ParentCapture:
                    break;
            }
        }
    }

    // Keep a canonical lowered form for every explicit field initializer.  Static initializers are
    // consumed by the Stage 37 native type-initialization routine; instance initializers are also
    // useful to dumps/tests even though constructor lowering injects per-construction copies.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& field = memberModel.members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field)
        {
            continue;
        }
        // Stage 51.x: an enum constant has no real initializer expression to lower for the
        // (common) auto-increment case -- its value was already computed by
        // ProjectMemberModel::collectEnumMembers. Synthesize the same IntegerConstant HIR node
        // `lowerExpression` would have produced for a literal, so every downstream consumer
        // (Stage 37 const-field materialization in the MIR builder, then LLVM constant-global
        // lowering) sees an ordinary `static const int` field and needs no enum-specific code.
        if (field.isEnumConstant)
        {
            // EL LITERAL LLEVA EL TIPO SUBYACENTE DEL ENUM, no `builtin:int`.
            //
            // Aguas abajo, `normalizeInteger` trunca al ancho de esta clave y extiende el signo. Con
            // `builtin:int` fijo, TODO enumerador se truncaba a 32 bits antes de llegar al IR: en un
            // `enum : ulong`, el valor 4294967295 salia valiendo -1, y el modelo de miembros --que
            // ya lo habia aceptado como valido-- no tenia forma de enterarse. La regla esta en
            // EnumUnderlyingType.h.
            const frontend::NamedTypeDeclaration* enumType =
                field.declaringTypeOrdinal < nameLookup.namedTypes().size()
                    ? &nameLookup.namedTypes()[field.declaringTypeOrdinal]
                    : nullptr;
            const std::string_view underlyingKey = enumType != nullptr
                ? enumUnderlyingTypeKey(enumType->syntax)
                : std::string_view("builtin:int");
            auto literal = makeNode(
                ProjectOopHirNodeKind::IntegerConstant, field.syntax, std::string(underlyingKey));
            literal->integerValue = static_cast<std::uint64_t>(field.enumConstantValue);
            fieldInitializers_.push_back(ProjectOopHirFieldInitializer{
                memberOrdinal, field.declaringTypeOrdinal, field.isStatic, field.isConst, std::move(literal)});
            continue;
        }
        if (field.syntax == nullptr || field.syntax->first() == nullptr)
        {
            continue;
        }
        auto value = lowerExpression(field.syntax->first(), memberOrdinal, false, error);
        if (value == nullptr)
        {
            return false;
        }
        const MethodExpressionTypeBinding* sourceType = expressionType(field.syntax->first());
        const ProjectImplicitConversion conversion = classifyImplicit(
            value->typeKey, field.declaredTypeKey,
            sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
        if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
        {
            value = wrapImplicitConversion(std::move(value), conversion, field.syntax->first());
        }
        fieldInitializers_.push_back(ProjectOopHirFieldInitializer{
            memberOrdinal, field.declaringTypeOrdinal, field.isStatic, field.isConst, std::move(value)});
    }

    // A source class with no written constructor still owns executable construction semantics:
    // base chaining, zero initialization (performed by the allocator path), and its instance
    // field initializers. Publish those semantics through the same void(this) ABI as a written
    // constructor so a metadata consumer never has to recreate producer source expressions.
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        if (type.kind != NamedTypeDeclarationKind::Class || type.isPrecompiled || type.isStatic)
            continue;
        const bool hasExplicitConstructor = std::ranges::any_of(
            memberModel.membersOfType(typeOrdinal), [&](const std::size_t ordinal) {
                return ordinal < memberModel.members().size() &&
                    memberModel.members()[ordinal].kind == ClassMemberKind::Constructor;
            });
        if (hasExplicitConstructor) continue;

        const ImplicitDefaultConstructorBinding* binding =
            constructionModel.implicitDefaultConstructorBindingForType(typeOrdinal);
        if (binding == nullptr ||
            binding->resolution == ImplicitDefaultConstructorResolutionKind::Invalid ||
            binding->resolution == ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor ||
            binding->resolution == ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor)
        {
            continue;
        }

        ProjectOopHirFunction function;
        function.memberOrdinal = std::numeric_limits<std::size_t>::max();
        function.declaringTypeOrdinal = typeOrdinal;
        function.constructor = true;
        function.isSyntheticImplicitConstructor = true;
        const std::string fullyQualifiedName = type.identity.fullyQualifiedName();
        const std::size_t separator = fullyQualifiedName.find_last_of(".+");
        const std::string typeName = separator == std::string::npos
            ? fullyQualifiedName
            : fullyQualifiedName.substr(separator + 1U);
        function.mangledName = "__nsc_ctor_" +
            compilation::symbolFragmentForIdentityKey(type.identity.key()) + "__" + sanitize(typeName);
        function.returnTypeKey = "builtin:void";
        function.body = makeNode(ProjectOopHirNodeKind::Block, type.syntax, "builtin:void");

        if (binding->entryConstructorMemberOrdinal.has_value() && binding->baseTypeOrdinal.has_value())
        {
            auto baseCall = makeNode(ProjectOopHirNodeKind::ConstructorInitializerCall, type.syntax, "builtin:void");
            baseCall->memberOrdinal = binding->entryConstructorMemberOrdinal;
            baseCall->baseReceiver = true;
            baseCall->children.push_back(makeNode(
                ProjectOopHirNodeKind::ThisValue,
                type.syntax,
                inheritanceModel.typeKeyForOrdinal(*binding->baseTypeOrdinal)));
            function.body->children.push_back(std::move(baseCall));
        }
        if (binding->baseTypeOrdinal.has_value())
        {
            for (const std::size_t initializerType : implicitInitializerTypes(
                     *binding->baseTypeOrdinal, binding->entryConstructorMemberOrdinal))
            {
                appendInstanceFieldInitializers(initializerType, function.body->children, error);
                if (!error.empty()) return false;
            }
        }
        appendInstanceFieldInitializers(typeOrdinal, function.body->children, error);
        if (!error.empty()) return false;
        functions_.push_back(std::move(function));
    }

    built_ = true;
    return true;
}

std::span<const ProjectOopHirFunction> ProjectOopHirModel::functions() const noexcept
{
    return functions_;
}

std::span<const ProjectOopHirFieldInitializer> ProjectOopHirModel::fieldInitializers() const noexcept
{
    return fieldInitializers_;
}

const ProjectOopHirFunction* ProjectOopHirModel::functionForMember(std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= functionByMember_.size())
    {
        return functionForNamespaceFunction(memberOrdinal - functionByMember_.size());
    }
    if (!functionByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByMember_[memberOrdinal]];
}

const ProjectOopHirFunction* ProjectOopHirModel::functionForNamespaceFunction(
    std::size_t functionOrdinal) const noexcept
{
    if (functionOrdinal >= functionByNamespaceFunction_.size() ||
        !functionByNamespaceFunction_[functionOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByNamespaceFunction_[functionOrdinal]];
}
bool ProjectOopHirModel::isBuilt() const noexcept
{
    return built_;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBlock(
    const ast::AstNode* block,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (block == nullptr || block->kind() != ast::AstNodeKind::BlockStatement)
    {
        error = "Stage 34 OOP HIR requires block statement body";
        return nullptr;
    }
    auto result = makeNode(ProjectOopHirNodeKind::Block, block, "builtin:void");
    const std::size_t cleanupDepthAtEntry = activeHirCleanupRegions_.size();
    for (const ast::AstNode* statement : block->children())
    {
        std::unique_ptr<ProjectOopHirNode> lowered = lowerStatement(statement, memberOrdinal, error);
        if (lowered == nullptr)
        {
            activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
            return nullptr;
        }
        if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
            lowered->cleanupRegionOrdinal.has_value())
        {
            activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
        }
        result->children.push_back(std::move(lowered));
    }
    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
    return result;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (statement == nullptr)
    {
        error = "null statement reached Stage 34 OOP HIR";
        return nullptr;
    }

    const auto cleanupOrdinalFor = [&](const MethodCleanupRegion* region) -> std::optional<std::size_t>
    {
        if (region == nullptr || methodBodyModel_ == nullptr) return std::nullopt;
        const auto regions = methodBodyModel_->cleanupRegions();
        if (regions.empty() || region < regions.data() || region >= regions.data() + regions.size())
            return std::nullopt;
        return static_cast<std::size_t>(region - regions.data());
    };

    const auto attachStructuredExit = [&](ProjectOopHirNode& node)
    {
        const MethodStructuredExitBinding* binding = structuredExitBinding(statement);
        if (binding == nullptr) return;
        node.cleanupRegionOrdinals = binding->cleanupRegionOrdinals;
        node.targetScopeOrdinal = binding->targetScopeOrdinal;
        switch (binding->kind)
        {
            case MethodStructuredExitKind::Return:
                node.cleanupContinuationKind = MethodCleanupContinuationKind::Return;
                break;
            case MethodStructuredExitKind::Break:
                node.cleanupContinuationKind = MethodCleanupContinuationKind::Break;
                break;
            case MethodStructuredExitKind::Continue:
                node.cleanupContinuationKind = MethodCleanupContinuationKind::Continue;
                break;
        }
    };

    switch (statement->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            return lowerBlock(statement, memberOrdinal, error);

        case ast::AstNodeKind::VariableDeclarationStatement:
        {
            const MethodLocalSymbol* local = localSymbol(statement);
            if (local == nullptr)
            {
                error = "Stage 34 cannot map local declaration to Stage 27 local symbol";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::LocalDeclaration, statement, local->typeKey);
            node->stringValue = local->name;
            node->referenceLocal = local->isReference;
            node->referenceMutable = local->referenceMutable;
            node->staticLocal = local->isStatic;
            node->pinnedStorage = local->isPinned;
            node->ownership = local->ownership;
            node->automaticDestruction = local->isRaii || local->requiresValueDrop;
            node->objectLifetimeOwnership = local->objectLifetimeOwnership;
            node->storageOwnership = local->storageOwnership;
            const auto localIndex = static_cast<std::size_t>(local - methodBodyModel_->locals().data());
            node->localOrdinal = localIndex;
            if (!local->isStatic && (local->isRaii || local->requiresValueDrop))
            {
                const MethodCleanupRegion* region = cleanupRegion(statement);
                if (region == nullptr)
                {
                    error = "drop-bearing local is missing its lexical cleanup region";
                    return nullptr;
                }
                node->cleanupRegionOrdinal = cleanupOrdinalFor(region);
                if (!node->cleanupRegionOrdinal.has_value())
                {
                    error = "drop-bearing local cleanup region has no stable ordinal";
                    return nullptr;
                }
            }
            if (local->isVariableLengthArray)
            {
                if (local->variableLengthArrayCountSyntax == nullptr ||
                    local->variableLengthArrayElementTypeKey.empty())
                {
                    error = "NativeSharp 2.0 VLA local lost its element type or runtime extent";
                    return nullptr;
                }
                node->variableLengthArray = true;
                node->variableLengthArrayRank =
                    std::max<std::size_t>(1, local->variableLengthArrayExtentSyntaxes.size());
                node->elementTypeKey = local->variableLengthArrayElementTypeKey;
                std::unique_ptr<ProjectOopHirNode> count;
                std::vector<std::unique_ptr<ProjectOopHirNode>> allocationCounts;
                std::unique_ptr<ProjectOopHirNode> vlaBlock;
                if (local->variableLengthArrayExtentSyntaxes.size() > 1)
                {
                    if (local->variableLengthArrayExtentLocalOrdinals.size() !=
                        local->variableLengthArrayExtentSyntaxes.size())
                    {
                        error = "multidimensional VLA lost its captured extent locals";
                        return nullptr;
                    }
                    vlaBlock = makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
                    for (std::size_t dimension = 0;
                         dimension < local->variableLengthArrayExtentSyntaxes.size(); ++dimension)
                    {
                        auto extent = lowerExpression(
                            local->variableLengthArrayExtentSyntaxes[dimension],
                            memberOrdinal,
                            false,
                            error);
                        if (extent == nullptr) return nullptr;
                        const std::size_t extentOrdinal =
                            local->variableLengthArrayExtentLocalOrdinals[dimension];
                        const std::string& extentType =
                            methodBodyModel_->locals()[extentOrdinal].typeKey;
                        auto extentDeclaration = makeNode(
                            ProjectOopHirNodeKind::LocalDeclaration,
                            local->variableLengthArrayExtentSyntaxes[dimension],
                            extentType);
                        extentDeclaration->stringValue =
                            methodBodyModel_->locals()[extentOrdinal].name;
                        extentDeclaration->debugDisplayName = local->name + ".Length[" +
                            std::to_string(dimension) + "]";
                        extentDeclaration->localOrdinal = extentOrdinal;
                        extentDeclaration->children.push_back(std::move(extent));
                        vlaBlock->children.push_back(std::move(extentDeclaration));

                        auto extentValue = makeNode(
                            ProjectOopHirNodeKind::LocalValue,
                            local->variableLengthArrayExtentSyntaxes[dimension],
                            extentType);
                        extentValue->localOrdinal = extentOrdinal;
                        allocationCounts.push_back(std::move(extentValue));
                    }
                }
                else
                {
                    count = lowerExpression(
                        local->variableLengthArrayCountSyntax, memberOrdinal, false, error);
                    if (count == nullptr) return nullptr;
                }

                auto allocation = makeNode(
                    ProjectOopHirNodeKind::StackAllocation, statement, local->typeKey);
                allocation->elementTypeKey = local->variableLengthArrayElementTypeKey;
                if (!allocationCounts.empty())
                {
                    for (auto& extent : allocationCounts)
                        allocation->children.push_back(std::move(extent));
                }
                else
                {
                    allocation->children.push_back(std::move(count));
                }
                node->children.push_back(std::move(allocation));
                if (vlaBlock != nullptr)
                {
                    vlaBlock->children.push_back(std::move(node));
                    return vlaBlock;
                }
            }
            else if (statement->first() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> initializer = lowerExpression(
                    statement->first(), memberOrdinal, local->isReference, error);
                if (initializer == nullptr)
                {
                    return nullptr;
                }
                node->placementConstructed =
                    initializer->kind == ProjectOopHirNodeKind::PlacementConstruction;
                const ProjectImplicitConversion conversion = local->isReference
                    ? ProjectImplicitConversion{
                        ProjectConversionKind::Identity, 0, local->typeKey, local->typeKey,
                        std::nullopt}
                    : classifyImplicit(
                        initializer->typeKey,
                        local->typeKey,
                        expressionType(statement->first()) != nullptr
                            ? expressionType(statement->first())->integerConstant
                            : std::nullopt);
                if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                {
                    initializer = wrapImplicitConversion(std::move(initializer), conversion, statement->first());
                }
                if (local->ownership == sema::OwnershipKind::Borrowed &&
                    local->localBorrowBindingOrdinal.has_value() &&
                    *local->localBorrowBindingOrdinal < methodBodyModel_->localBorrowBindings().size())
                {
                    const MethodLocalBorrowBinding& borrow =
                        methodBodyModel_->localBorrowBindings()[*local->localBorrowBindingOrdinal];
                    auto borrowed = makeNode(ProjectOopHirNodeKind::BorrowValue, statement->first(), local->typeKey);
                    borrowed->localBorrowSourceKind = borrow.sourceKind;
                    borrowed->localOrdinal = borrow.sourceLocalOrdinal;
                    borrowed->parameterOrdinal = borrow.sourceParameterOrdinal;
                    borrowed->children.push_back(std::move(initializer));
                    initializer = std::move(borrowed);
                }
                node->children.push_back(std::move(initializer));
            }
            return node;
        }

        case ast::AstNodeKind::DeconstructionDeclarationStatement:
        {
            if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
            {
                error = "T02-F03 deconstruction declaration requires value layouts";
                return nullptr;
            }
            const MethodDeconstructionBinding* binding =
                methodBodyModel_->deconstructionBinding(statement);
            if (binding == nullptr)
            {
                error = "T02-F03 deconstruction declaration has no semantic binding";
                return nullptr;
            }
            auto source = lowerDeconstructionSource(
                statement, statement->first(), memberOrdinal, error);
            if (source == nullptr) return nullptr;
            const std::optional<std::vector<std::string>> elements =
                ProjectTypeConversion::tupleElementTypeKeys(binding->tupleTypeKey);
            const std::optional<language::AggregateLayout> layout =
                objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, binding->tupleTypeKey);
            if (!elements.has_value() || !layout.has_value() ||
                elements->size() != statement->children().size() ||
                layout->offsets.size() != elements->size())
            {
                error = "T02-F03 deconstruction declaration lost its tuple shape or layout";
                return nullptr;
            }
            auto block = makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
            auto deconstruct = makeNode(
                ProjectOopHirNodeKind::TupleDeconstruct, statement, binding->tupleTypeKey);
            deconstruct->children.push_back(std::move(source));
            std::function<bool(const ast::AstNode*, const std::string&, std::uint64_t)>
                appendBinding;
            appendBinding = [&](const ast::AstNode* bindingSyntax, const std::string& typeKey,
                                std::uint64_t byteOffset) -> bool
            {
                if (bindingSyntax == nullptr) return false;
                if (bindingSyntax->kind() == ast::AstNodeKind::TupleExpression)
                {
                    const std::optional<std::vector<std::string>> nestedElements =
                        ProjectTypeConversion::tupleElementTypeKeys(typeKey);
                    const std::optional<language::AggregateLayout> nestedLayout =
                        objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, typeKey);
                    if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                        nestedElements->size() != bindingSyntax->children().size() ||
                        nestedLayout->offsets.size() != nestedElements->size())
                    {
                        error = "T02-F03 nested declaration pattern lost its tuple layout";
                        return false;
                    }
                    for (std::size_t index = 0; index < nestedElements->size(); ++index)
                        if (!appendBinding(
                                bindingSyntax->children()[index], (*nestedElements)[index],
                                byteOffset + nestedLayout->offsets[index])) return false;
                    return true;
                }
                if (bindingSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                    bindingSyntax->text() == "_") return true;
                const MethodLocalSymbol* local = localSymbol(bindingSyntax);
                if (local == nullptr)
                {
                    error = "T02-F03 deconstruction binding has no local symbol";
                    return false;
                }
                const std::size_t localOrdinal = static_cast<std::size_t>(
                    local - methodBodyModel_->locals().data());
                auto declaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration, bindingSyntax, local->typeKey);
                declaration->stringValue = local->name;
                declaration->localOrdinal = localOrdinal;
                block->children.push_back(std::move(declaration));

                auto address = makeNode(
                    ProjectOopHirNodeKind::LocalAddress, bindingSyntax, local->typeKey);
                address->localOrdinal = localOrdinal;
                auto element = makeNode(
                    ProjectOopHirNodeKind::TupleDeconstructElement, bindingSyntax, local->typeKey);
                element->elementTypeKey = typeKey;
                element->integerValue = byteOffset;
                element->conversion = ProjectTypeConversion::classifyImplicit(
                    typeKey, local->typeKey);
                element->children.push_back(std::move(address));
                deconstruct->children.push_back(std::move(element));
                return true;
            };
            for (std::size_t index = 0; index < statement->children().size(); ++index)
            {
                if (!appendBinding(
                        statement->children()[index], (*elements)[index],
                        static_cast<std::uint64_t>(layout->offsets[index]))) return nullptr;
            }
            block->children.push_back(std::move(deconstruct));
            return block;
        }

        case ast::AstNodeKind::ExpressionStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::ExpressionStatement, statement, "builtin:void");
            std::unique_ptr<ProjectOopHirNode> expression = lowerExpression(
                statement->first(), memberOrdinal, false, error);
            if (expression == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(expression));
            return node;
        }

        case ast::AstNodeKind::InlineAssemblyStatement:
        {
            auto assembly = makeNode(ProjectOopHirNodeKind::InlineAssembly, statement, "builtin:void");
            assembly->stringValue = statement->text();
            for (const ast::AstNode* child : statement->children())
            {
                if (child == nullptr)
                {
                    error = "integrated assembly contains a null contract record";
                    return nullptr;
                }

                if (child->kind() == ast::AstNodeKind::InlineAssemblyLine)
                {
                    auto line = makeNode(
                        ProjectOopHirNodeKind::InlineAssemblyLine, child, "builtin:void");
                    line->stringValue = child->text();
                    line->elementTypeKey = child->auxiliaryText();
                    assembly->children.push_back(std::move(line));
                    continue;
                }

                if (child->kind() == ast::AstNodeKind::InlineAssemblyClobber)
                {
                    auto clobber = makeNode(
                        ProjectOopHirNodeKind::InlineAssemblyClobber, child, "builtin:void");
                    clobber->stringValue = child->text();
                    clobber->elementTypeKey = child->auxiliaryText();
                    clobber->integerValue = child->integerValue();
                    clobber->booleanValue = child->flag();
                    assembly->children.push_back(std::move(clobber));
                    continue;
                }

                const bool input = child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand;
                const bool output = child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand;
                if (!input && !output)
                {
                    error = "integrated assembly contains an unknown contract record";
                    return nullptr;
                }
                if (child->first() == nullptr)
                {
                    error = "integrated assembly operand lost its NativeSharp expression";
                    return nullptr;
                }

                const bool memoryInput = input && child->auxiliaryText() == "m";
                auto value = lowerExpression(
                    child->first(), memberOrdinal, output || memoryInput, error);
                if (value == nullptr) return nullptr;
                const MethodExpressionTypeBinding* operandType = expressionType(child->first());
                if (operandType == nullptr || operandType->typeKey.empty())
                {
                    error = "integrated assembly operand lost its resolved NativeSharp type";
                    return nullptr;
                }

                auto operand = makeNode(
                    input ? ProjectOopHirNodeKind::InlineAssemblyInput
                          : ProjectOopHirNodeKind::InlineAssemblyOutput,
                    child,
                    operandType->typeKey);
                operand->stringValue = child->auxiliaryText();
                operand->integerValue = child->integerValue();
                operand->booleanValue = child->flag();
                operand->children.push_back(std::move(value));
                assembly->children.push_back(std::move(operand));
            }
            return assembly;
        }

        case ast::AstNodeKind::StaticAssertStatement:
        {
            std::unique_ptr<ProjectOopHirNode> condition = lowerExpression(
                statement->first(), memberOrdinal, false, error);
            if (condition == nullptr) return nullptr;
            const std::optional<StaticAssertConstant> value =
                evaluateStaticAssertConstant(*condition);
            if (!value.has_value() || value->kind != StaticAssertConstantKind::Boolean)
            {
                error = "static_assert condition is not a compile-time bool constant";
                return nullptr;
            }
            if (!value->booleanValue)
            {
                error = "static assertion failed";
                if (!statement->text().empty()) error += ": " + statement->text();
                return nullptr;
            }
            return makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
        }

        case ast::AstNodeKind::IfStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::If, statement, "builtin:void");
            auto condition = lowerExpression(statement->first(), memberOrdinal, false, error);
            auto thenBlock = lowerBlock(statement->second(), memberOrdinal, error);
            if (condition == nullptr || thenBlock == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(condition));
            node->children.push_back(std::move(thenBlock));
            if (statement->third() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> elseNode = statement->third()->kind() == ast::AstNodeKind::BlockStatement
                    ? lowerBlock(statement->third(), memberOrdinal, error)
                    : lowerStatement(statement->third(), memberOrdinal, error);
                if (elseNode == nullptr)
                {
                    return nullptr;
                }
                node->children.push_back(std::move(elseNode));
            }
            return node;
        }

        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::WhileStatement:
        {
            auto node = makeNode(
                statement->kind() == ast::AstNodeKind::DoWhileStatement
                    ? ProjectOopHirNodeKind::DoWhile
                    : ProjectOopHirNodeKind::While,
                statement,
                "builtin:void");
            auto condition = lowerExpression(statement->first(), memberOrdinal, false, error);
            auto body = lowerBlock(statement->second(), memberOrdinal, error);
            if (condition == nullptr || body == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(condition));
            node->children.push_back(std::move(body));
            return node;
        }

        // `switch`. The label EXPRESSIONS are not lowered at all: the body model folded them to
        // integers, checked them against the governing type and rejected duplicates, and left the
        // answer in a MethodSwitchBinding. So this walk copies numbers and lowers statements --
        // there is exactly one place in the compiler that knows how to evaluate a case label, and
        // it is not here.
        case ast::AstNodeKind::SwitchStatement:
        {
            const MethodSwitchBinding* binding = switchBinding(statement);
            if (binding == nullptr)
            {
                error = "Stage 34 OOP HIR reached a switch statement with no semantic binding";
                return nullptr;
            }

            auto node = makeNode(ProjectOopHirNodeKind::Switch, statement, "builtin:void");
            auto governing = lowerExpression(statement->first(), memberOrdinal, false, error);
            if (governing == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(governing));

            std::size_t sectionIndex = 0;
            for (const ast::AstNode* section : statement->children())
            {
                if (section == nullptr || section->kind() != ast::AstNodeKind::SwitchSection)
                {
                    continue;
                }
                if (sectionIndex >= binding->sections.size())
                {
                    error = "Stage 34 OOP HIR found more switch sections than its semantic binding";
                    return nullptr;
                }
                const MethodSwitchSectionBinding& sectionBinding = binding->sections[sectionIndex];
                ++sectionIndex;

                auto sectionNode =
                    makeNode(ProjectOopHirNodeKind::SwitchSection, section, "builtin:void");
                sectionNode->switchLabels = sectionBinding.labelValues;
                sectionNode->switchHasDefault = sectionBinding.hasDefault;

                // The section's statements become an ordinary Block, so every consumer downstream
                // sees a shape it already handles. Built here rather than through lowerBlock()
                // because a section is not a BlockStatement -- its child list starts with labels.
                auto body = makeNode(ProjectOopHirNodeKind::Block, section, "builtin:void");
                const std::size_t sectionCleanupDepth = activeHirCleanupRegions_.size();
                for (const ast::AstNode* child : section->children())
                {
                    if (child == nullptr || child->kind() == ast::AstNodeKind::SwitchLabel)
                    {
                        continue;
                    }
                    std::unique_ptr<ProjectOopHirNode> lowered =
                        lowerStatement(child, memberOrdinal, error);
                    if (lowered == nullptr)
                    {
                        activeHirCleanupRegions_.resize(sectionCleanupDepth);
                        return nullptr;
                    }
                    if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                        lowered->cleanupRegionOrdinal.has_value())
                    {
                        activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
                    }
                    body->children.push_back(std::move(lowered));
                }
                activeHirCleanupRegions_.resize(sectionCleanupDepth);

                sectionNode->children.push_back(std::move(body));
                node->children.push_back(std::move(sectionNode));
            }
            return node;
        }

        case ast::AstNodeKind::ForStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::For, statement, "builtin:void");
            const std::size_t cleanupDepthAtEntry = activeHirCleanupRegions_.size();
            if (statement->first() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> init;
                if (statement->first()->kind() == ast::AstNodeKind::BlockStatement)
                    init = lowerBlock(statement->first(), memberOrdinal, error);
                else if (statement->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement)
                    init = lowerStatement(statement->first(), memberOrdinal, error);
                else
                    init = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (init == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                    return nullptr;
                }
                if (init->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                    init->cleanupRegionOrdinal.has_value())
                {
                    activeHirCleanupRegions_.push_back(*init->cleanupRegionOrdinal);
                }
                node->children.push_back(std::move(init));
            }
            else
            {
                node->children.push_back(nullptr);
            }
            if (statement->second() != nullptr)
            {
                auto condition = lowerExpression(statement->second(), memberOrdinal, false, error);
                if (condition == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                    return nullptr;
                }
                node->children.push_back(std::move(condition));
            }
            else
            {
                node->children.push_back(nullptr);
            }
            if (statement->third() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> increment =
                    statement->third()->kind() == ast::AstNodeKind::BlockStatement
                        ? lowerBlock(statement->third(), memberOrdinal, error)
                        : lowerExpression(statement->third(), memberOrdinal, false, error);
                if (increment == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                    return nullptr;
                }
                node->children.push_back(std::move(increment));
            }
            else
            {
                node->children.push_back(nullptr);
            }
            if (statement->children().empty())
            {
                error = "Stage 34 for statement is missing a body";
                activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                return nullptr;
            }
            auto body = lowerBlock(statement->children().front(), memberOrdinal, error);
            if (body == nullptr)
            {
                activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                return nullptr;
            }
            node->children.push_back(std::move(body));
            activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
            return node;
        }

        case ast::AstNodeKind::ForeachStatement:
        {
            // Foreach was desugared at parse time into two unconditionally-built candidate loops
            // (see the design comment atop Parser::parseForeachStatement and
            // ProjectMethodBodyModel::analyzeForeach). ProjectMethodBodyModel::analyzeForeach
            // already picked the "real" candidate and ran ordinary semantic analysis on ONLY it --
            // but the raw AST this function walks still carries the original ForeachStatement
            // node, with BOTH candidate subtrees still attached, as the statement in the enclosing
            // block. There is no side-channel recording which candidate Stage 27 chose, so HIR
            // lowering re-derives the SAME array-vs-enumerator decision independently, the same
            // way: resolve __foreach_source's already-analyzed typeKey via localSymbol() (populated
            // from MethodLocalSymbol::syntax, see localBySyntax_) and check for the "[]" array
            // suffix (see ProjectGenericModel::canonicalTypeKey for that representation). Both
            // passes read the identical resolved type, so they always agree.
            //
            // No new HIR node kind is introduced: __foreach_source's declaration lowers through
            // the ordinary VariableDeclarationStatement case above, and the chosen candidate --
            // itself just a BlockStatement holding an index/enumerator VariableDeclarationStatement
            // plus a WhileStatement -- lowers through the ordinary lowerBlock/WhileStatement paths.
            // The two are wrapped in one outer Block so the foreach statement lowers to exactly one
            // HIR node, mirroring the single Foreach semantic scope analyzeForeach created.
            const MethodLocalSymbol* sourceLocal = localSymbol(statement->first());
            if (sourceLocal == nullptr)
            {
                error = "Stage 34 foreach source local is missing its Stage 27 semantic binding";
                return nullptr;
            }
            const bool isArray = sourceLocal->typeKey.size() > 2 &&
                sourceLocal->typeKey.compare(sourceLocal->typeKey.size() - 2, 2, "[]") == 0;
            const bool isSpan =
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span<") ||
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span__g") ||
                sourceLocal->typeKey.find("::NativeSharp.Core.Span__g") != std::string::npos ||
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan<") ||
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan__g") ||
                sourceLocal->typeKey.find("::NativeSharp.Core.ReadOnlySpan__g") != std::string::npos;
            const ast::AstNode* chosenCandidate = isArray
                ? statement->second()
                : (isSpan && statement->children().size() > 1U
                    ? statement->children()[1]
                    : statement->third());

            auto node = makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
            std::unique_ptr<ProjectOopHirNode> sourceDeclHir =
                lowerStatement(statement->first(), memberOrdinal, error);
            if (sourceDeclHir == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(sourceDeclHir));

            std::unique_ptr<ProjectOopHirNode> candidateHir = lowerBlock(chosenCandidate, memberOrdinal, error);
            if (candidateHir == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(candidateHir));
            return node;
        }

        case ast::AstNodeKind::BreakStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Break, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            if (statement->first() != nullptr)
            {
                const MethodLoopBreakBinding* binding = methodBodyModel_ != nullptr
                    ? methodBodyModel_->loopBreakBinding(statement) : nullptr;
                if (binding == nullptr)
                {
                    error = "T07-F05 break value has no loop-expression semantic binding";
                    return nullptr;
                }
                auto value = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                if (binding->conversion.exists() &&
                    binding->conversion.kind != ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(
                        std::move(value), binding->conversion, statement->first());
                }
                node->children.push_back(std::move(value));
                node->elementTypeKey = binding->resultTypeKey;
            }
            return node;
        }
        case ast::AstNodeKind::ContinueStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Continue, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            return node;
        }

        case ast::AstNodeKind::SuspendStatement:
        {
            // NS304-T05-F09: a bare `yield;` suspends the poll once. integerValue is the resume
            // state index (1 for the single-suspension case; multi-suspension generalizes like
            // the await resume indices).
            auto node = makeNode(ProjectOopHirNodeKind::SuspendPoint, statement, "builtin:void");
            node->integerValue = 1;
            return node;
        }

        case ast::AstNodeKind::YieldReturnStatement:
        {
            // NS304-T06-F03: `yield return v` becomes a YieldValue node. integerValue is the yield
            // state index, assigned by splitGeneratorFunction (the MoveNext rewrite).
            auto node = makeNode(ProjectOopHirNodeKind::YieldValue, statement, "builtin:void");
            node->integerValue = 0;
            if (statement->first() != nullptr)
            {
                // NS304-T06-F05: a `ref`/`ref mut` item yields the ADDRESS of the local (the item is
                // a reference into the frame), not a copy of its value.
                bool itemByReference = false;
                if (memberModel_ != nullptr && memberOrdinal < memberModel_->members().size())
                {
                    const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                    if (const std::optional<std::string_view> inner =
                            language::iteratorInnerTypeKey(member.declaredTypeKey);
                        inner.has_value() &&
                        (inner->starts_with("ref:") || inner->starts_with("refmut:")))
                    {
                        itemByReference = true;
                    }
                }
                auto value = lowerExpression(
                    statement->first(), memberOrdinal, itemByReference, error);
                if (value == nullptr) return nullptr;
                node->children.push_back(std::move(value));
            }
            return node;
        }

        case ast::AstNodeKind::YieldBreakStatement:
        {
            // NS304-T06-F07: `yield break` terminates the generator and must run the active cleanup
            // regions (defer/drop) exactly once before returning false.
            auto node = makeNode(ProjectOopHirNodeKind::YieldBreak, statement, "builtin:void");
            attachStructuredExit(*node);
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            return node;
        }

        case ast::AstNodeKind::ReturnStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Return, statement, "builtin:void");
            attachStructuredExit(*node);
            if (statement->first() != nullptr)
            {
                bool referenceReturn = false;
                bool valueReturningCallable = false;
                std::string declaredTypeKey;
                sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
                if (memberModel_ != nullptr && memberOrdinal < memberModel_->members().size())
                {
                    const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                    referenceReturn = member.isReferenceReturn;
                    valueReturningCallable = member.kind == ClassMemberKind::Method;
                    declaredTypeKey = member.declaredTypeKey;
                    declaredOwnership = member.declaredOwnership;
                }
                else
                {
                    const std::size_t callableBase =
                        memberModel_ != nullptr ? memberModel_->members().size() : 0;
                    const std::size_t functionOrdinal =
                        memberOrdinal >= callableBase ? memberOrdinal - callableBase :
                        std::numeric_limits<std::size_t>::max();
                    if (namespaceFunctionModel_ == nullptr ||
                        functionOrdinal >= namespaceFunctionModel_->functions().size())
                    {
                        error = "Stage 34 return refers to invalid callable";
                        return nullptr;
                    }
                    const NamespaceFunctionSymbol& function =
                        namespaceFunctionModel_->functions()[functionOrdinal];
                    referenceReturn = function.isReferenceReturn;
                    valueReturningCallable = true;
                    declaredTypeKey = function.declaredTypeKey;
                    declaredOwnership = function.declaredOwnership;
                }

                auto value = lowerExpression(
                    statement->first(), memberOrdinal, referenceReturn, error);
                if (value == nullptr)
                {
                    return nullptr;
                }
                if (valueReturningCallable && declaredTypeKey != "builtin:void" && !referenceReturn)
                {
                    const MethodExpressionTypeBinding* type = expressionType(statement->first());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey,
                        declaredTypeKey,
                        type != nullptr ? type->integerConstant : std::nullopt);
                    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(std::move(value), conversion, statement->first());
                    }
                    if (declaredOwnership == sema::OwnershipKind::Borrowed)
                    {
                        const MethodBorrowBinding* borrow = borrowBinding(statement->first());
                        if (borrow == nullptr)
                        {
                            error = "Stage 50.8 borrowed return has no frozen borrow binding";
                            return nullptr;
                        }
                        auto borrowed = makeNode(
                            ProjectOopHirNodeKind::BorrowValue,
                            statement->first(),
                            declaredTypeKey);
                        borrowed->borrowKind = borrow->kind;
                        borrowed->localOrdinal = borrow->sourceLocalOrdinal;
                        borrowed->parameterOrdinal = borrow->sourceParameterOrdinal;
                        borrowed->children.push_back(std::move(value));
                        value = std::move(borrowed);
                    }
                }
                if (statement->flag())
                {
                    switch (value->kind)
                    {
                        case ProjectOopHirNodeKind::InstanceCall:
                        case ProjectOopHirNodeKind::StaticCall:
                            value->mustTailCall = true;
                            break;
                        default:
                            error = "T08-F06 validated must_tail return did not lower to a direct call";
                            return nullptr;
                    }
                }
                node->children.push_back(std::move(value));
            }
            return node;
        }
        case ast::AstNodeKind::ThrowStatement:
        {
            const MethodExceptionBinding* binding = exceptionBinding(statement);
            if (binding == nullptr)
            {
                error = "Stage 45.6 throw statement has no Stage 45.2 exception binding";
                return nullptr;
            }
            auto node = makeNode(
                binding->kind == MethodExceptionOperationKind::Rethrow
                    ? ProjectOopHirNodeKind::Rethrow
                    : ProjectOopHirNodeKind::Throw,
                statement, "builtin:void");
            node->typeKey = binding->exceptionTypeKey.empty() ? "builtin:void" : binding->exceptionTypeKey;
            node->exceptionTypeOrdinal = binding->exceptionTypeOrdinal;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;

            const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
                ? 0
                : activeHirExceptionHandlers_.back().retainedCleanupDepth;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > retainedDepth; --index)
                node->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
            if (!activeHirExceptionHandlers_.empty())
                node->exceptionTargetRegionOrdinal = activeHirExceptionHandlers_.back().hirRegionOrdinal;

            if (statement->first() != nullptr)
            {
                auto value = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                node->children.push_back(std::move(value));
            }
            return node;
        }

        case ast::AstNodeKind::UsingStatement:
        {
            const MethodUsingBinding* binding = usingBinding(statement);
            const MethodCleanupRegion* region = cleanupRegion(statement);
            const std::optional<std::size_t> cleanupOrdinal = cleanupOrdinalFor(region);
            if (binding == nullptr || !cleanupOrdinal.has_value())
            {
                error = "Stage 45.6 using statement has no frozen using/cleanup binding";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::UsingRegion, statement, "builtin:void");
            node->localOrdinal = binding->resourceLocalOrdinal;
            node->elementTypeKey = binding->resourceTypeKey;
            if (binding->resourceLocalOrdinal < methodBodyModel_->locals().size())
                node->stringValue = methodBodyModel_->locals()[binding->resourceLocalOrdinal].name;
            node->cleanupRegionOrdinal = cleanupOrdinal;
            node->disposeContractMemberOrdinal = binding->disposeContractMemberOrdinal;
            node->disposeImplementationMemberOrdinal = binding->disposeImplementationMemberOrdinal;
            node->interfaceCleanupDispatch = binding->interfaceDispatch;

            auto initializer = lowerExpression(statement->first(), memberOrdinal, false, error);
            if (initializer == nullptr) return nullptr;
            const MethodExpressionTypeBinding* sourceType = expressionType(statement->first());
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer->typeKey, binding->resourceTypeKey,
                sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
            if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                initializer = wrapImplicitConversion(std::move(initializer), conversion, statement->first());
            node->children.push_back(std::move(initializer));

            activeHirCleanupRegions_.push_back(*cleanupOrdinal);
            auto body = lowerBlock(statement->second(), memberOrdinal, error);
            activeHirCleanupRegions_.pop_back();
            if (body == nullptr) return nullptr;
            node->children.push_back(std::move(body));

            auto continuation = makeNode(ProjectOopHirNodeKind::CleanupContinuation, statement, "builtin:void");
            continuation->cleanupRegionOrdinal = cleanupOrdinal;
            continuation->cleanupContinuationKind = MethodCleanupContinuationKind::Normal;
            node->children.push_back(std::move(continuation));
            return node;
        }

        case ast::AstNodeKind::TryStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::TryRegion, statement, "builtin:void");
            node->hirRegionOrdinal = nextHirRegionOrdinal_++;
            const MethodCleanupRegion* region = cleanupRegion(statement);
            const std::optional<std::size_t> cleanupOrdinal = cleanupOrdinalFor(region);
            if (cleanupOrdinal.has_value())
            {
                node->cleanupRegionOrdinal = cleanupOrdinal;
                activeHirCleanupRegions_.push_back(*cleanupOrdinal);
            }

            const bool hasCatches = !statement->children().empty();
            if (hasCatches)
            {
                activeHirExceptionHandlers_.push_back(HirExceptionHandlerContext{
                    *node->hirRegionOrdinal, activeHirCleanupRegions_.size()});
            }
            auto protectedBlock = lowerBlock(statement->first(), memberOrdinal, error);
            if (hasCatches) activeHirExceptionHandlers_.pop_back();
            if (protectedBlock == nullptr)
            {
                if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                return nullptr;
            }
            node->children.push_back(std::move(protectedBlock));

            for (const ast::AstNode* catchClause : statement->children())
            {
                const MethodExceptionBinding* catchBinding = exceptionBinding(catchClause);
                if (catchBinding == nullptr || catchBinding->kind != MethodExceptionOperationKind::Catch)
                {
                    if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                    error = "Stage 45.6 catch clause has no Stage 45.2 catch binding";
                    return nullptr;
                }
                auto catchNode = makeNode(ProjectOopHirNodeKind::CatchRegion, catchClause, "builtin:void");
                catchNode->elementTypeKey = catchBinding->exceptionTypeKey;
                catchNode->exceptionTypeOrdinal = catchBinding->exceptionTypeOrdinal;
                catchNode->catchAll = catchBinding->catchAll;
                catchNode->catchHasFilter = catchBinding->hasFilter;
                catchNode->catchExhaustive = catchBinding->exhaustive;
                catchNode->catchMatchedErrorTypeKeys = catchBinding->matchedErrorTypeKeys;
                if (const MethodLocalSymbol* local = localSymbol(catchClause); local != nullptr)
                {
                    catchNode->localOrdinal = static_cast<std::size_t>(
                        local - methodBodyModel_->locals().data());
                    catchNode->stringValue = local->name;
                }
                if (catchBinding->hasFilter)
                {
                    auto catchFilter = lowerExpression(
                        catchClause->second(), memberOrdinal, false, error);
                    if (catchFilter == nullptr)
                    {
                        if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                        return nullptr;
                    }
                    catchNode->children.push_back(std::move(catchFilter));
                }
                auto catchBody = lowerBlock(catchClause->first(), memberOrdinal, error);
                if (catchBody == nullptr)
                {
                    if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                    return nullptr;
                }
                catchNode->children.push_back(std::move(catchBody));
                node->children.push_back(std::move(catchNode));
            }

            if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();

            if (statement->second() != nullptr)
            {
                if (!cleanupOrdinal.has_value())
                {
                    error = "Stage 45.6 finally block has no cleanup region";
                    return nullptr;
                }
                auto finallyNode = makeNode(ProjectOopHirNodeKind::FinallyRegion, statement->second(), "builtin:void");
                finallyNode->cleanupRegionOrdinal = cleanupOrdinal;
                auto finallyBody = lowerBlock(statement->second(), memberOrdinal, error);
                if (finallyBody == nullptr) return nullptr;
                finallyNode->children.push_back(std::move(finallyBody));
                auto continuation = makeNode(ProjectOopHirNodeKind::CleanupContinuation, statement, "builtin:void");
                continuation->cleanupRegionOrdinal = cleanupOrdinal;
                continuation->cleanupContinuationKind = MethodCleanupContinuationKind::Normal;
                finallyNode->children.push_back(std::move(continuation));
                node->children.push_back(std::move(finallyNode));
            }
            return node;
        }

        default:
            error = "unsupported Stage 34 OOP statement kind: " +
                std::string(ast::astNodeKindName(statement->kind()));
            return nullptr;
    }
}

// Ver el comentario de la declaracion en ProjectOopHirModel.h: este envoltorio existe para que
// los 125 sitios que ponen un error dentro de lowerExpressionImpl no tengan que adjuntar la
// ubicacion cada uno.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    std::unique_ptr<ProjectOopHirNode> lowered =
        lowerExpressionImpl(expression, memberOrdinal, addressContext, error);
    const ClassMemberSymbol* activeMember = memberModel_ != nullptr &&
            memberOrdinal < memberModel_->members().size()
        ? &memberModel_->members()[memberOrdinal]
        : nullptr;
    const sema::EffectContext effects(activeMember != nullptr
        ? activeMember->effects
        : sema::EffectSet{});
    const auto directlyAllocatesHeap = [](ProjectOopHirNodeKind kind) noexcept
    {
        switch (kind)
        {
            case ProjectOopHirNodeKind::ArrayAllocation:
            case ProjectOopHirNodeKind::RangeArrayAllocation:
            case ProjectOopHirNodeKind::RawAllocation:
            case ProjectOopHirNodeKind::StringConcat:
            case ProjectOopHirNodeKind::StringSubstring:
            case ProjectOopHirNodeKind::StringFromCharArray:
            case ProjectOopHirNodeKind::DelegateCreate:
            case ProjectOopHirNodeKind::LambdaCreate:
            case ProjectOopHirNodeKind::EventAdd:
                return true;
            default:
                return false;
        }
    };
    if (lowered != nullptr && effects.allows(sema::Effect::NoHeap) &&
        directlyAllocatesHeap(lowered->kind))
    {
        error = "NS3030: noheap callable '" + activeMember->name +
            "' cannot contain direct heap allocation '" +
            std::string(projectOopHirNodeKindName(lowered->kind)) + "'";
        lowered.reset();
    }
    if (lowered != nullptr && effects.allows(sema::Effect::NoHeap))
    {
        const ClassMemberSymbol& caller = *activeMember;
        const MethodCallBinding* call = callBinding(expression);
        if (call != nullptr && call->resolution == MethodCallResolutionKind::Resolved &&
            call->selectedMemberOrdinal.has_value() &&
            *call->selectedMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& callee = memberModel_->members()[*call->selectedMemberOrdinal];
            if (!callee.isNoHeap)
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot call non-noheap callable '" + callee.signatureKey + "'";
                lowered.reset();
            }
        }
        if (lowered != nullptr &&
            (lowered->kind == ProjectOopHirNodeKind::DelegateInvoke ||
             lowered->kind == ProjectOopHirNodeKind::EventInvoke))
        {
            error = "NS3030: noheap callable '" + caller.signatureKey +
                "' cannot invoke an indirect target without a noheap contract";
            lowered.reset();
        }
        if (lowered != nullptr &&
            (lowered->kind == ProjectOopHirNodeKind::PlacementConstruction ||
             lowered->kind == ProjectOopHirNodeKind::ValueConstruction))
        {
            const ObjectConstructionBinding* binding = constructionBinding(expression);
            if (binding == nullptr || !binding->selectedConstructorMemberOrdinal.has_value() ||
                *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot invoke an implicit constructor without a noheap contract";
                lowered.reset();
            }
            else
            {
                const ClassMemberSymbol& constructor = memberModel_->members()[
                    *binding->selectedConstructorMemberOrdinal];
                if (!constructor.isNoHeap)
                {
                    error = "NS3030: noheap callable '" + caller.signatureKey +
                        "' cannot call non-noheap constructor '" +
                        constructor.signatureKey + "'";
                    lowered.reset();
                }
            }
        }
    }
    // Solo se apunta si NO hay nada apuntado: asi gana el nodo mas interno, que es el que de
    // verdad fallo, y no la expresion que lo contiene.
    if (lowered == nullptr && !error.empty() && !lastErrorRange_.isValid() && expression != nullptr)
    {
        lastErrorRange_ = expression->range();
    }
    return lowered;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::rewriteAsyncPollReturns(
    std::unique_ptr<ProjectOopHirNode> body,
    std::size_t selfParameterOrdinal,
    std::string_view resultTypeKey,
    std::string_view futureKey,
    std::string& error)
{
    if (body == nullptr)
    {
        return nullptr;
    }
    if (body->kind == ProjectOopHirNodeKind::Throw)
    {
        // NS304-T05-F07: `throw E;` becomes a future error, not a native unwind:
        // { self->error = E; self->state = -2 (error); return; }
        auto block = makeNode(ProjectOopHirNodeKind::Block, body->syntax, "builtin:void");
        const auto selfParameter = [&]() -> std::unique_ptr<ProjectOopHirNode>
        {
            auto self = makeNode(
                ProjectOopHirNodeKind::ParameterValue, body->syntax, std::string(futureKey) + "*");
            self->parameterOrdinal = selfParameterOrdinal;
            return self;
        };

        auto errorField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, body->syntax,
            body->children.empty() ? std::string{} : body->children[0]->typeKey);
        errorField->memberOrdinal = 4; // error (pollFn is index 3)
        errorField->elementTypeKey = std::string(futureKey);
        errorField->children.push_back(selfParameter());

        std::unique_ptr<ProjectOopHirNode> errorValue = !body->children.empty()
            ? std::move(body->children[0])
            : makeNode(ProjectOopHirNodeKind::NullConstant, body->syntax, errorField->typeKey);
        auto errorStore = makeNode(
            ProjectOopHirNodeKind::Assignment, body->syntax, errorField->typeKey);
        errorStore->operation = lexer::TokenKind::Equal;
        errorStore->children.push_back(std::move(errorField));
        errorStore->children.push_back(std::move(errorValue));
        block->children.push_back(std::move(errorStore));

        auto stateField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, body->syntax, "builtin:int");
        stateField->memberOrdinal = 0; // state
        stateField->elementTypeKey = std::string(futureKey);
        stateField->children.push_back(selfParameter());
        auto errored = makeNode(ProjectOopHirNodeKind::IntegerConstant, body->syntax, "builtin:int");
        errored->integerValue = ~0ULL - 1ULL; // error = -2 (uniform; ready = -1, resume 1..N)
        auto stateStore = makeNode(
            ProjectOopHirNodeKind::Assignment, body->syntax, "builtin:int");
        stateStore->operation = lexer::TokenKind::Equal;
        stateStore->children.push_back(std::move(stateField));
        stateStore->children.push_back(std::move(errored));
        block->children.push_back(std::move(stateStore));

        auto synthesizedReturn = makeNode(
            ProjectOopHirNodeKind::Return, body->syntax, "builtin:void");
        block->children.push_back(std::move(synthesizedReturn));
        return block;
    }
    if (body->kind == ProjectOopHirNodeKind::Return)
    {
        // `return v;` -> { self->result = v; self->state = 1 (ready); return; }
        auto block = makeNode(ProjectOopHirNodeKind::Block, body->syntax, "builtin:void");

        const auto selfParameter = [&]() -> std::unique_ptr<ProjectOopHirNode>
        {
            auto self = makeNode(
                ProjectOopHirNodeKind::ParameterValue, body->syntax, std::string(futureKey) + "*");
            self->parameterOrdinal = selfParameterOrdinal;
            return self;
        };

        auto resultField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, body->syntax, std::string(resultTypeKey));
        resultField->memberOrdinal = 2; // result
        resultField->elementTypeKey = futureKey;
        resultField->children.push_back(selfParameter());

        std::unique_ptr<ProjectOopHirNode> value = !body->children.empty()
            ? std::move(body->children[0])
            : makeNode(ProjectOopHirNodeKind::DefaultValue, body->syntax, std::string(resultTypeKey));

        auto resultStore = makeNode(
            ProjectOopHirNodeKind::Assignment, body->syntax, std::string(resultTypeKey));
        resultStore->operation = lexer::TokenKind::Equal;
        resultStore->children.push_back(std::move(resultField));
        resultStore->children.push_back(std::move(value));
        block->children.push_back(std::move(resultStore));

        auto stateField = makeNode(
            ProjectOopHirNodeKind::FutureFieldAddress, body->syntax, "builtin:int");
        stateField->memberOrdinal = 0; // state
        stateField->elementTypeKey = futureKey;
        stateField->children.push_back(selfParameter());

        auto ready = makeNode(ProjectOopHirNodeKind::IntegerConstant, body->syntax, "builtin:int");
        ready->integerValue = ~0ULL; // ready = -1 (uniform); pending/resume states are 1..N

        auto stateStore = makeNode(
            ProjectOopHirNodeKind::Assignment, body->syntax, "builtin:int");
        stateStore->operation = lexer::TokenKind::Equal;
        stateStore->children.push_back(std::move(stateField));
        stateStore->children.push_back(std::move(ready));
        block->children.push_back(std::move(stateStore));

        // NS304-T05-F06: the synthesized `return;` must carry the original return's cleanup
        // regions, so owned locals drop exactly once on completion (the drop rides the non-local
        // exit's cleanupRegionOrdinals).
        auto synthesizedReturn = makeNode(
            ProjectOopHirNodeKind::Return, body->syntax, "builtin:void");
        synthesizedReturn->cleanupRegionOrdinals = body->cleanupRegionOrdinals;
        synthesizedReturn->cleanupContinuationKind = body->cleanupContinuationKind;
        block->children.push_back(std::move(synthesizedReturn));
        return block;
    }
    if (body->kind == ProjectOopHirNodeKind::YieldValue)
    {
        // NS304-T07-F02: `yield return v;` in the async-generator poll -> AsyncYieldValue(v). The
        // poll stores the item + the demand future's result/state; integerValue is the resume state.
        auto node = makeNode(ProjectOopHirNodeKind::AsyncYieldValue, body->syntax, "builtin:void");
        node->integerValue = body->integerValue;
        if (!body->children.empty() && body->children[0] != nullptr)
            node->children.push_back(std::move(body->children[0]));
        return node;
    }
    for (auto& child : body->children)
    {
        child = rewriteAsyncPollReturns(
            std::move(child), selfParameterOrdinal, resultTypeKey, futureKey, error);
        if (child == nullptr)
        {
            return nullptr;
        }
    }
    return body;
}

std::unique_ptr<ProjectOopHirFunction> ProjectOopHirModel::splitAsyncFunction(
    ProjectOopHirFunction& function,
    std::string& error)
{
    if (function.body == nullptr)
    {
        error = "async function has no lowered body to split";
        return nullptr;
    }
    const std::string futureKey = function.returnTypeKey;
    const auto inner = language::futureInnerTypeKey(futureKey);
    if (!inner.has_value())
    {
        error = "async function return type is not a future surface: " + futureKey;
        return nullptr;
    }
    const std::string resultTypeKey(*inner);
    const ast::AstNode* syntax = function.body->syntax;

    auto poll = std::make_unique<ProjectOopHirFunction>();
    poll->memberOrdinal = function.memberOrdinal;
    poll->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    poll->declaringTypeOrdinal = function.declaringTypeOrdinal;
    poll->isStatic = true;
    poll->isAsyncPoll = true;
    poll->inliningPolicy = function.inliningPolicy;
    poll->mangledName = function.mangledName + "$poll";
    poll->debugDisplayName = function.debugDisplayName + "$poll";
    poll->returnTypeKey = "builtin:void";
    poll->errorTypeKey = function.errorTypeKey;
    poll->effects = function.effects;
    poll->returnOwnership = sema::OwnershipKind::None;
    poll->parameterNames.push_back("self");
    poll->parameterTypeKeys.push_back(futureKey + "*");
    poll->parameterPassKinds.push_back(ParameterPassKind::Value);
    poll->parameterOwnerships.push_back(sema::OwnershipKind::None);
    poll->body = rewriteAsyncPollReturns(
        std::move(function.body), 0, resultTypeKey, futureKey, error);
    if (poll->body == nullptr)
    {
        return nullptr;
    }

    // Constructor: `future<T> self = default; self.pollFn = F$poll; return self;` -- lazy, never
    // runs the body, but carries the poll function pointer so `await` over a variable works.
    auto constructorBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t selfOrdinal = nextSyntheticLocalOrdinal_++;
    auto selfDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, futureKey);
    selfDecl->localOrdinal = selfOrdinal;
    selfDecl->stringValue = "self";
    selfDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, futureKey));
    constructorBody->children.push_back(std::move(selfDecl));

    auto pollField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    pollField->memberOrdinal = 3; // pollFn
    pollField->elementTypeKey = futureKey;
    auto selfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, futureKey + "*");
    selfAddress->localOrdinal = selfOrdinal;
    pollField->children.push_back(std::move(selfAddress));
    auto pollAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    pollAddress->stringValue = function.mangledName + "$poll";
    auto pollStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    pollStore->operation = lexer::TokenKind::Equal;
    pollStore->children.push_back(std::move(pollField));
    pollStore->children.push_back(std::move(pollAddress));
    constructorBody->children.push_back(std::move(pollStore));

    auto retSelf = makeNode(ProjectOopHirNodeKind::Return, syntax, futureKey);
    auto selfReturn = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, futureKey);
    selfReturn->localOrdinal = selfOrdinal;
    retSelf->children.push_back(std::move(selfReturn));
    constructorBody->children.push_back(std::move(retSelf));
    function.body = std::move(constructorBody);
    return poll;
}

std::pair<std::unique_ptr<ProjectOopHirFunction>, std::unique_ptr<ProjectOopHirFunction>>
ProjectOopHirModel::splitGeneratorFunction(
    ProjectOopHirFunction& function,
    std::string& error)
{
    if (function.body == nullptr)
    {
        error = "generator has no lowered body to split";
        return {nullptr, nullptr};
    }
    const std::string iteratorKey = function.returnTypeKey;
    auto item = language::iteratorInnerTypeKey(iteratorKey);
    if (!item.has_value()) item = language::asyncIteratorInnerTypeKey(iteratorKey);
    if (!item.has_value())
    {
        error = "generator return type is not an iterator surface: " + iteratorKey;
        return {nullptr, nullptr};
    }
    const std::string itemTypeKey(*item);
    const ast::AstNode* syntax = function.body->syntax;

    // MoveNext: `bool Gen$moveNext(iterator<T>* self)` runs the body to the next yield. The body's
    // YieldValue/YieldBreak nodes are lowered by the LLVM (item + state + return true/false).
    auto moveNext = std::make_unique<ProjectOopHirFunction>();
    moveNext->memberOrdinal = function.memberOrdinal;
    moveNext->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    moveNext->declaringTypeOrdinal = function.declaringTypeOrdinal;
    moveNext->isStatic = true;
    moveNext->inliningPolicy = function.inliningPolicy;
    moveNext->mangledName = function.mangledName + "$moveNext";
    moveNext->debugDisplayName = function.debugDisplayName + "$moveNext";
    moveNext->returnTypeKey = "builtin:bool";
    moveNext->errorTypeKey = function.errorTypeKey;
    moveNext->effects = function.effects;
    moveNext->returnOwnership = sema::OwnershipKind::None;
    moveNext->parameterNames.push_back("self");
    moveNext->parameterTypeKeys.push_back(iteratorKey + "*");
    moveNext->parameterPassKinds.push_back(ParameterPassKind::Value);
    moveNext->parameterOwnerships.push_back(sema::OwnershipKind::None);
    moveNext->body = std::move(function.body);
    // NS304-T06-F04: the MoveNext captures the original callable's parameters into its frame.
    moveNext->generatorParameterTypeKeys = function.parameterTypeKeys;

    // NS304-T06-F07: `throw E;` becomes a typed generator error, not a native unwind:
    // { self->error = E; return false; } (the consumer checks state/error and rethrows).
    const std::optional<std::string_view> errorType = language::iteratorErrorTypeKey(iteratorKey);
    if (errorType.has_value())
    {
        const auto rewriteThrows = [&](const auto& self, ProjectOopHirNode* node) -> void
        {
            if (node == nullptr) return;
            for (auto& child : node->children)
            {
                if (child != nullptr && child->kind == ProjectOopHirNodeKind::Throw)
                {
                    auto block = makeNode(ProjectOopHirNodeKind::Block, child->syntax, "builtin:void");
                    auto errorField = makeNode(
                        ProjectOopHirNodeKind::FutureFieldAddress, child->syntax, std::string(*errorType));
                    errorField->memberOrdinal = 5; // error (dropFn is index 4)
                    errorField->elementTypeKey = iteratorKey;
                    auto selfAddress = makeNode(
                        ProjectOopHirNodeKind::ParameterValue, child->syntax, iteratorKey + "*");
                    selfAddress->parameterOrdinal = 0;
                    errorField->children.push_back(std::move(selfAddress));
                    auto errorStore = makeNode(
                        ProjectOopHirNodeKind::Assignment, child->syntax, std::string(*errorType));
                    errorStore->operation = lexer::TokenKind::Equal;
                    errorStore->children.push_back(std::move(errorField));
                    errorStore->children.push_back(!child->children.empty()
                        ? std::move(child->children[0])
                        : makeNode(ProjectOopHirNodeKind::NullConstant, child->syntax, std::string(*errorType)));
                    block->children.push_back(std::move(errorStore));
                    auto stateField = makeNode(
                        ProjectOopHirNodeKind::FutureFieldAddress, child->syntax, "builtin:int");
                    stateField->memberOrdinal = 0; // state
                    stateField->elementTypeKey = iteratorKey;
                    auto stateSelf = makeNode(
                        ProjectOopHirNodeKind::ParameterValue, child->syntax, iteratorKey + "*");
                    stateSelf->parameterOrdinal = 0;
                    stateField->children.push_back(std::move(stateSelf));
                    auto errored = makeNode(ProjectOopHirNodeKind::IntegerConstant, child->syntax, "builtin:int");
                    errored->integerValue = ~0ULL - 1ULL; // -2 = error
                    auto stateStore = makeNode(
                        ProjectOopHirNodeKind::Assignment, child->syntax, "builtin:int");
                    stateStore->operation = lexer::TokenKind::Equal;
                    stateStore->children.push_back(std::move(stateField));
                    stateStore->children.push_back(std::move(errored));
                    block->children.push_back(std::move(stateStore));
                    auto retFalse = makeNode(ProjectOopHirNodeKind::Return, child->syntax, "builtin:bool");
                    auto falseValue = makeNode(
                        ProjectOopHirNodeKind::BooleanConstant, child->syntax, "builtin:bool");
                    falseValue->booleanValue = false;
                    retFalse->children.push_back(std::move(falseValue));
                    block->children.push_back(std::move(retFalse));
                    child = std::move(block);
                }
                else if (child != nullptr)
                {
                    self(self, child.get());
                }
            }
        };
        rewriteThrows(rewriteThrows, moveNext->body.get());
    }

    // NS304-T06-F03: number the yields 1..N (the state set after each yield; 0 is the entry state)
    // and append the terminal `return false` after the last yield.
    std::size_t nextYieldState = 0;
    const auto assignStates = [&](const auto& self, ProjectOopHirNode* node) -> void
    {
        if (node == nullptr) return;
        if (node->kind == ProjectOopHirNodeKind::YieldValue)
            node->integerValue = ++nextYieldState;
        for (auto& child : node->children) self(self, child.get());
    };
    assignStates(assignStates, moveNext->body.get());
    auto wrapped = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    wrapped->children.push_back(std::move(moveNext->body));
    auto retFalse = makeNode(ProjectOopHirNodeKind::Return, syntax, "builtin:bool");
    auto falseValue = makeNode(ProjectOopHirNodeKind::BooleanConstant, syntax, "builtin:bool");
    falseValue->booleanValue = false;
    retFalse->children.push_back(std::move(falseValue));
    wrapped->children.push_back(std::move(retFalse));
    moveNext->body = std::move(wrapped);

    // Constructor: `iterator<T> self = default; self.moveNextFn = Gen$moveNext; return self;` --
    // lazy, never runs the body, but carries the MoveNext pointer so `it.MoveNext()` works.
    auto constructorBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t selfOrdinal = nextSyntheticLocalOrdinal_++;
    auto selfDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, iteratorKey);
    selfDecl->localOrdinal = selfOrdinal;
    selfDecl->stringValue = "self";
    selfDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, iteratorKey));
    constructorBody->children.push_back(std::move(selfDecl));

    auto moveNextField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    moveNextField->memberOrdinal = 3; // moveNextFn
    moveNextField->elementTypeKey = iteratorKey;
    auto selfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    selfAddress->localOrdinal = selfOrdinal;
    moveNextField->children.push_back(std::move(selfAddress));
    auto moveNextAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    moveNextAddress->stringValue = function.mangledName + "$moveNext";
    auto moveNextStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    moveNextStore->operation = lexer::TokenKind::Equal;
    moveNextStore->children.push_back(std::move(moveNextField));
    moveNextStore->children.push_back(std::move(moveNextAddress));
    constructorBody->children.push_back(std::move(moveNextStore));

    // NS304-T06-F07: also carry the Drop function pointer (field 4).
    auto dropField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    dropField->memberOrdinal = 4; // dropFn
    dropField->elementTypeKey = iteratorKey;
    auto dropSelfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    dropSelfAddress->localOrdinal = selfOrdinal;
    dropField->children.push_back(std::move(dropSelfAddress));
    auto dropAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    dropAddress->stringValue = function.mangledName + "$drop";
    auto dropStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    dropStore->operation = lexer::TokenKind::Equal;
    dropStore->children.push_back(std::move(dropField));
    dropStore->children.push_back(std::move(dropAddress));
    constructorBody->children.push_back(std::move(dropStore));

    auto retSelf = makeNode(ProjectOopHirNodeKind::Return, syntax, iteratorKey);
    auto selfReturn = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, iteratorKey);
    selfReturn->localOrdinal = selfOrdinal;
    retSelf->children.push_back(std::move(selfReturn));
    constructorBody->children.push_back(std::move(retSelf));
    function.body = std::move(constructorBody);
    // NS304-T06-F04: the original callable is now the generator constructor — it must allocate the
    // frame and store the captured parameters.
    function.isGeneratorConstructor = true;

    // Drop: `void Gen$drop(iterator<T>* self)` destroys the frame's live locals on early stop.
    // The drop-glue body is emitted by the LLVM from the MoveNext's frame locals.
    auto drop = std::make_unique<ProjectOopHirFunction>();
    drop->memberOrdinal = function.memberOrdinal;
    drop->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    drop->declaringTypeOrdinal = function.declaringTypeOrdinal;
    drop->isStatic = true;
    drop->inliningPolicy = function.inliningPolicy;
    drop->mangledName = function.mangledName + "$drop";
    drop->debugDisplayName = function.debugDisplayName + "$drop";
    drop->returnTypeKey = "builtin:void";
    drop->errorTypeKey = function.errorTypeKey;
    drop->effects = function.effects;
    drop->returnOwnership = sema::OwnershipKind::None;
    drop->parameterNames.push_back("self");
    drop->parameterTypeKeys.push_back(iteratorKey + "*");
    drop->parameterPassKinds.push_back(ParameterPassKind::Value);
    drop->parameterOwnerships.push_back(sema::OwnershipKind::None);
    auto dropBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    auto dropFrame = makeNode(ProjectOopHirNodeKind::DropFrame, syntax, "builtin:void");
    auto dropSelf = makeNode(ProjectOopHirNodeKind::ParameterValue, syntax, iteratorKey + "*");
    dropSelf->parameterOrdinal = 0;
    dropFrame->children.push_back(std::move(dropSelf));
    dropBody->children.push_back(std::move(dropFrame));
    drop->body = std::move(dropBody);

    return {std::move(moveNext), std::move(drop)};
}

std::vector<std::unique_ptr<ProjectOopHirFunction>> ProjectOopHirModel::splitAsyncGeneratorFunction(
    ProjectOopHirFunction& function,
    std::string& error)
{
    if (function.body == nullptr)
    {
        error = "async generator has no lowered body to split";
        return {};
    }
    const std::string iteratorKey = function.returnTypeKey;
    const auto item = language::asyncIteratorInnerTypeKey(iteratorKey);
    if (!item.has_value())
    {
        error = "async generator return type is not an async-iterator surface: " + iteratorKey;
        return {};
    }
    const std::string futureKey = language::futureTypeKey(
        "builtin:bool",
        language::asyncIteratorErrorTypeKey(iteratorKey).has_value()
            ? std::string(*language::asyncIteratorErrorTypeKey(iteratorKey))
            : std::string{});
    const std::string errorTypeKey =
        language::asyncIteratorErrorTypeKey(iteratorKey).has_value()
            ? std::string(*language::asyncIteratorErrorTypeKey(iteratorKey))
            : std::string{};
    const ast::AstNode* syntax = function.body->syntax;

    // NS304-T07-F02: number the yields 1..N (state after each yield) and append the terminal
    // `return false` (no more items), exactly like splitGeneratorFunction.
    std::size_t nextYieldState = 0;
    const auto assignStates = [&](const auto& self, ProjectOopHirNode* node) -> void
    {
        if (node == nullptr) return;
        if (node->kind == ProjectOopHirNodeKind::YieldValue)
            node->integerValue = ++nextYieldState;
        for (auto& child : node->children) self(self, child.get());
    };
    assignStates(assignStates, function.body.get());
    auto wrapped = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    wrapped->children.push_back(std::move(function.body));
    auto retFalse = makeNode(ProjectOopHirNodeKind::Return, syntax, "builtin:bool");
    auto falseValue = makeNode(ProjectOopHirNodeKind::BooleanConstant, syntax, "builtin:bool");
    falseValue->booleanValue = false;
    retFalse->children.push_back(std::move(falseValue));
    wrapped->children.push_back(std::move(retFalse));
    function.body = std::move(wrapped);

    // 1. The poll: `void Gen$moveNext$poll(future<bool>* self)` — runs the body to the next yield
    //    or await. rewriteAsyncPollReturns maps `return` -> result/state and `yield` -> AsyncYieldValue.
    auto poll = std::make_unique<ProjectOopHirFunction>();
    poll->memberOrdinal = function.memberOrdinal;
    poll->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    poll->declaringTypeOrdinal = function.declaringTypeOrdinal;
    poll->isStatic = true;
    poll->isAsyncPoll = true;
    poll->inliningPolicy = function.inliningPolicy;
    poll->mangledName = function.mangledName + "$moveNext$poll";
    poll->debugDisplayName = function.debugDisplayName + "$moveNext$poll";
    poll->returnTypeKey = "builtin:void";
    poll->errorTypeKey = errorTypeKey;
    poll->effects = function.effects;
    poll->returnOwnership = sema::OwnershipKind::None;
    poll->parameterNames.push_back("self");
    poll->parameterTypeKeys.push_back(futureKey + "*");
    poll->parameterPassKinds.push_back(ParameterPassKind::Value);
    poll->parameterOwnerships.push_back(sema::OwnershipKind::None);
    poll->body = rewriteAsyncPollReturns(
        std::move(function.body), 0, "builtin:bool", futureKey, error);
    if (poll->body == nullptr) return {};

    // 2. The MoveNext: `future<bool> Gen$moveNext(async-iterator* self)` — creates the demand future,
    //    carries the async-iterator in future.frame, and points future.pollFn at the poll.
    auto moveNext = std::make_unique<ProjectOopHirFunction>();
    moveNext->memberOrdinal = function.memberOrdinal;
    moveNext->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    moveNext->declaringTypeOrdinal = function.declaringTypeOrdinal;
    moveNext->isStatic = true;
    moveNext->inliningPolicy = function.inliningPolicy;
    moveNext->mangledName = function.mangledName + "$moveNext";
    moveNext->debugDisplayName = function.debugDisplayName + "$moveNext";
    moveNext->returnTypeKey = futureKey;
    moveNext->errorTypeKey = errorTypeKey;
    moveNext->effects = function.effects;
    moveNext->returnOwnership = sema::OwnershipKind::None;
    moveNext->parameterNames.push_back("self");
    moveNext->parameterTypeKeys.push_back(iteratorKey + "*");
    moveNext->parameterPassKinds.push_back(ParameterPassKind::Value);
    moveNext->parameterOwnerships.push_back(sema::OwnershipKind::None);

    auto moveNextBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t futureOrdinal = nextSyntheticLocalOrdinal_++;
    auto futureDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, futureKey);
    futureDecl->localOrdinal = futureOrdinal;
    futureDecl->stringValue = "f";
    futureDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, futureKey));
    moveNextBody->children.push_back(std::move(futureDecl));
    // future.frame = self (the async-iterator).
    auto frameField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    frameField->memberOrdinal = 1;
    frameField->elementTypeKey = futureKey;
    auto futureAddr = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, futureKey + "*");
    futureAddr->localOrdinal = futureOrdinal;
    frameField->children.push_back(std::move(futureAddr));
    auto selfParam = makeNode(ProjectOopHirNodeKind::ParameterValue, syntax, iteratorKey + "*");
    selfParam->parameterOrdinal = 0;
    auto frameStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    frameStore->operation = lexer::TokenKind::Equal;
    frameStore->children.push_back(std::move(frameField));
    frameStore->children.push_back(std::move(selfParam));
    moveNextBody->children.push_back(std::move(frameStore));
    // future.pollFn = Gen$moveNext$poll.
    auto pollField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    pollField->memberOrdinal = 3;
    pollField->elementTypeKey = futureKey;
    auto futureAddr2 = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, futureKey + "*");
    futureAddr2->localOrdinal = futureOrdinal;
    pollField->children.push_back(std::move(futureAddr2));
    auto pollAddr = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    pollAddr->stringValue = function.mangledName + "$moveNext$poll";
    auto pollStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    pollStore->operation = lexer::TokenKind::Equal;
    pollStore->children.push_back(std::move(pollField));
    pollStore->children.push_back(std::move(pollAddr));
    moveNextBody->children.push_back(std::move(pollStore));
    // return future.
    auto retFuture = makeNode(ProjectOopHirNodeKind::Return, syntax, futureKey);
    auto futureValue = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, futureKey);
    futureValue->localOrdinal = futureOrdinal;
    retFuture->children.push_back(std::move(futureValue));
    moveNextBody->children.push_back(std::move(retFuture));
    moveNext->body = std::move(moveNextBody);

    // 3. The drop: `void Gen$drop(async-iterator* self)` — destroys the frame's live locals.
    auto drop = std::make_unique<ProjectOopHirFunction>();
    drop->memberOrdinal = function.memberOrdinal;
    drop->namespaceFunctionOrdinal = function.namespaceFunctionOrdinal;
    drop->declaringTypeOrdinal = function.declaringTypeOrdinal;
    drop->isStatic = true;
    drop->inliningPolicy = function.inliningPolicy;
    drop->mangledName = function.mangledName + "$drop";
    drop->debugDisplayName = function.debugDisplayName + "$drop";
    drop->returnTypeKey = "builtin:void";
    drop->errorTypeKey = function.errorTypeKey;
    drop->effects = function.effects;
    drop->returnOwnership = sema::OwnershipKind::None;
    drop->parameterNames.push_back("self");
    drop->parameterTypeKeys.push_back(iteratorKey + "*");
    drop->parameterPassKinds.push_back(ParameterPassKind::Value);
    drop->parameterOwnerships.push_back(sema::OwnershipKind::None);
    auto dropBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    auto dropFrame = makeNode(ProjectOopHirNodeKind::DropFrame, syntax, "builtin:void");
    auto dropSelf = makeNode(ProjectOopHirNodeKind::ParameterValue, syntax, iteratorKey + "*");
    dropSelf->parameterOrdinal = 0;
    dropFrame->children.push_back(std::move(dropSelf));
    dropBody->children.push_back(std::move(dropFrame));
    drop->body = std::move(dropBody);

    // 4. The constructor (the original function): async-iterator + moveNextFn + dropFn.
    auto constructorBody = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    const std::size_t selfOrdinal = nextSyntheticLocalOrdinal_++;
    auto selfDecl = makeNode(ProjectOopHirNodeKind::LocalDeclaration, syntax, iteratorKey);
    selfDecl->localOrdinal = selfOrdinal;
    selfDecl->stringValue = "self";
    selfDecl->children.push_back(makeNode(ProjectOopHirNodeKind::DefaultValue, syntax, iteratorKey));
    constructorBody->children.push_back(std::move(selfDecl));
    auto moveNextField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    moveNextField->memberOrdinal = 3; // moveNextFn
    moveNextField->elementTypeKey = iteratorKey;
    auto selfAddress = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    selfAddress->localOrdinal = selfOrdinal;
    moveNextField->children.push_back(std::move(selfAddress));
    auto moveNextAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    moveNextAddress->stringValue = function.mangledName + "$moveNext";
    auto moveNextStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    moveNextStore->operation = lexer::TokenKind::Equal;
    moveNextStore->children.push_back(std::move(moveNextField));
    moveNextStore->children.push_back(std::move(moveNextAddress));
    constructorBody->children.push_back(std::move(moveNextStore));
    auto dropField = makeNode(ProjectOopHirNodeKind::FutureFieldAddress, syntax, "builtin:void*");
    dropField->memberOrdinal = 4; // dropFn
    dropField->elementTypeKey = iteratorKey;
    auto dropSelfAddr = makeNode(ProjectOopHirNodeKind::LocalAddress, syntax, iteratorKey + "*");
    dropSelfAddr->localOrdinal = selfOrdinal;
    dropField->children.push_back(std::move(dropSelfAddr));
    auto dropAddress = makeNode(ProjectOopHirNodeKind::PollAddress, syntax, "builtin:void*");
    dropAddress->stringValue = function.mangledName + "$drop";
    auto dropStore = makeNode(ProjectOopHirNodeKind::Assignment, syntax, "builtin:void*");
    dropStore->operation = lexer::TokenKind::Equal;
    dropStore->children.push_back(std::move(dropField));
    dropStore->children.push_back(std::move(dropAddress));
    constructorBody->children.push_back(std::move(dropStore));
    auto retSelf = makeNode(ProjectOopHirNodeKind::Return, syntax, iteratorKey);
    auto selfReturn = makeNode(ProjectOopHirNodeKind::LocalValue, syntax, iteratorKey);
    selfReturn->localOrdinal = selfOrdinal;
    retSelf->children.push_back(std::move(selfReturn));
    constructorBody->children.push_back(std::move(retSelf));
    function.body = std::move(constructorBody);
    // NS304-T07-F04: the original callable is the async-generator constructor — it must allocate the
    // frame (heap) so `ref` items reference persistent slots, not the poll's stack.
    function.isGeneratorConstructor = true;

    std::vector<std::unique_ptr<ProjectOopHirFunction>> result;
    result.push_back(std::move(moveNext));
    result.push_back(std::move(poll));
    result.push_back(std::move(drop));
    return result;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionImpl(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (expression == nullptr)
    {
        error = "null expression reached Stage 34 OOP HIR";
        return nullptr;
    }
    const MethodLambdaBinding* stage43Lambda = lambdaBinding(expression);
    if (stage43Lambda != nullptr && expression->kind() == ast::AstNodeKind::LambdaExpression)
    {
        auto node = makeNode(ProjectOopHirNodeKind::LambdaCreate, expression, stage43Lambda->delegateTypeKey);
        node->elementTypeKey = stage43Lambda->delegateTypeKey;
        node->stringValue = stage43Lambda->generatedName;
        node->integerValue = stage43Lambda->lambdaOrdinal;
        for (const MethodLambdaCapture& capture : stage43Lambda->captures)
        {
            std::unique_ptr<ProjectOopHirNode> value;
            switch (capture.sourceKind)
            {
                case MethodLambdaCaptureSourceKind::OriginalLocal:
                case MethodLambdaCaptureSourceKind::ParentLocal:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                        value->localOrdinal = capture.sourceOrdinal;
                    }
                    break;
                case MethodLambdaCaptureSourceKind::OriginalParameter:
                case MethodLambdaCaptureSourceKind::ParentParameter:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                        value->parameterOrdinal = capture.sourceOrdinal;
                    }
                    break;
                case MethodLambdaCaptureSourceKind::OriginalThis:
                    value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                    break;
                case MethodLambdaCaptureSourceKind::ParentCapture:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(
                            ProjectOopHirNodeKind::ClosureCaptureCellReference,
                            expression,
                            capture.typeKey);
                        value->integerValue = *capture.sourceOrdinal;
                    }
                    break;
            }
            if (value == nullptr)
            {
                error = "Stage 43 lambda capture source is unavailable";
                return nullptr;
            }
            value->captureMode = capture.mode;
            value->booleanValue = capture.clearMovedSource;
            node->children.push_back(std::move(value));
        }
        return node;
    }

    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    if (stage42Delegate != nullptr && stage42Delegate->kind == MethodDelegateOperationKind::Create)
    {
        const bool memberTarget = stage42Delegate->selectedMemberOrdinal.has_value() &&
            memberModel_ != nullptr &&
            *stage42Delegate->selectedMemberOrdinal < memberModel_->members().size();
        const bool namespaceTarget = stage42Delegate->selectedMemberOrdinal.has_value() &&
            memberModel_ != nullptr && namespaceFunctionModel_ != nullptr &&
            *stage42Delegate->selectedMemberOrdinal >= memberModel_->members().size() &&
            *stage42Delegate->selectedMemberOrdinal - memberModel_->members().size() <
                namespaceFunctionModel_->functions().size();
        if (!memberTarget && !namespaceTarget)
        {
            error = "Stage 42 delegate creation has no selected callable";
            return nullptr;
        }
        auto node = makeNode(ProjectOopHirNodeKind::DelegateCreate, expression, stage42Delegate->delegateTypeKey);
        node->elementTypeKey = stage42Delegate->delegateTypeKey;
        node->memberOrdinal = stage42Delegate->selectedMemberOrdinal;
        node->booleanValue = stage42Delegate->virtualDispatch;
        node->interfaceDispatch = stage42Delegate->interfaceDispatch;
        node->ownership = stage42Delegate->ownsReceiver
            ? sema::OwnershipKind::Owned : sema::OwnershipKind::Borrowed;
        if (stage42Delegate->interfaceDispatch)
            node->targetTypeOrdinal = stage42Delegate->receiverTypeOrdinal;
        if (!stage42Delegate->staticTarget)
        {
            std::unique_ptr<ProjectOopHirNode> receiver;
            if (stage42Delegate->ownsReceiver &&
                (stage42Delegate->receiverSourceLocalOrdinal.has_value() ||
                 stage42Delegate->receiverSourceParameterOrdinal.has_value()))
            {
                receiver = makeNode(
                    ProjectOopHirNodeKind::MoveValue,
                    stage42Delegate->receiverSyntax,
                    stage42Delegate->receiverTypeKey);
                receiver->localOrdinal = stage42Delegate->receiverSourceLocalOrdinal;
                receiver->parameterOrdinal = stage42Delegate->receiverSourceParameterOrdinal;
                receiver->booleanValue = true;
            }
            else
            {
                receiver = lowerReceiver(
                    stage42Delegate->receiverSyntax, stage42Delegate->receiverTypeKey,
                    memberOrdinal, error);
            }
            if (receiver == nullptr) return nullptr;
            node->children.push_back(std::move(receiver));
        }
        return node;
    }

    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";

    // ID 82. `preloweredLast`, cuando no es nulo, SUSTITUYE la bajada del ultimo argumento. Tiene
    // un solo llamador: la asignacion compuesta sobre propiedad e indexador, donde el valor que
    // recibe el setter no es una expresion de la fuente sino `get_X(...) op derecho`, un arbol que
    // hay que construir en el sitio. Todo lo demas --receptor, conversiones seleccionadas,
    // borrows-- sigue siendo el mismo codigo para todas las llamadas resueltas, que es justamente
    // el motivo de entrar por aqui en vez de armar el nodo de llamada a mano.
    const auto lowerResolvedCall = [&](
        const MethodCallBinding* binding,
        const std::vector<const ast::AstNode*>& argumentSyntaxes,
        std::unique_ptr<ProjectOopHirNode> preloweredLast = nullptr,
        bool nullConditional = false)
        -> std::unique_ptr<ProjectOopHirNode>
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
    };

    const auto lowerEventBacking = [&](
        const MethodEventBinding* binding,
        bool address) -> std::unique_ptr<ProjectOopHirNode>
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
    };

    switch (expression->kind())
    {
        case ast::AstNodeKind::LoopExpression:
        {
            if (addressContext)
            {
                error = "a loop expression produces a value, not addressable storage";
                return nullptr;
            }
            const MethodLoopExpressionBinding* binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->loopExpressionBinding(expression) : nullptr;
            if (binding == nullptr)
            {
                error = "T07-F05 loop expression has no semantic binding";
                return nullptr;
            }
            auto node = makeNode(
                ProjectOopHirNodeKind::LoopExpression, expression, binding->resultTypeKey);
            node->stringValue = binding->label;
            auto body = lowerBlock(expression->first(), memberOrdinal, error);
            if (body == nullptr) return nullptr;
            node->children.push_back(std::move(body));
            return node;
        }
        case ast::AstNodeKind::ThisExpression:
        {
            if (currentLambda_ != nullptr)
            {
                const std::optional<std::size_t> capture = currentLambdaThisCapture();
                if (!capture.has_value())
                {
                    error = "Stage 43 lambda this-reference has no captured receiver";
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext && isStructValueType(resultType)
                        ? ProjectOopHirNodeKind::ClosureCaptureAddress
                        : ProjectOopHirNodeKind::ClosureCaptureValue,
                    expression,
                    resultType);
                node->integerValue = *capture;
                return node;
            }
            // `this` inside a STRUCT method is the receiver's ADDRESS -- struct receivers are
            // passed by address and memory-homed since Stage 40.2 -- so in a VALUE context the
            // expression `this` has to load through it, exactly the way a `ref` parameter does a
            // few cases below. Handing back the bare ThisValue there was two reported bugs at
            // once, and they are the same line:
            //
            //   * `Get(this)` passed the POINTER where the callee's signature wanted the
            //     aggregate; the LLVM verifier rejected the module. Loud.
            //   * `S8 self = this;` stored the pointer BITS into the local's own bytes -- for an
            //     8-byte struct the carrier is `i64`, so `store ptr` fit and nothing complained --
            //     and every later field read decoded those bits as the struct's fields. Silent,
            //     no diagnostic, passes the verifier, and it is the one that reaches production.
            //     It was also, bitterly, the workaround people wrote for the loud one.
            //
            // Nothing changes for a CLASS: `this` IS the reference, and its value in both
            // contexts is that pointer. Nothing changes in an ADDRESS context either -- and every
            // existing consumer of ThisValue is one: `lowerReceiver` lowers a struct receiver
            // with addressContext = true (so `this.field` and `this.Method()` keep taking the
            // address), and the constructor-initializer and instance-initializer helpers build
            // ThisValue directly as a receiver.
            auto self = makeNode(ProjectOopHirNodeKind::ThisValue, expression, resultType);
            if (addressContext || !isStructValueType(resultType))
            {
                return self;
            }
            auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
            load->children.push_back(std::move(self));
            return load;
        }

        case ast::AstNodeKind::NullConditionalMemberExpression:
        {
            if (addressContext)
            {
                error = "a null-conditional access produces a value, not addressable storage";
                return nullptr;
            }
            const MethodExpressionTypeBinding* receiverFlow = expressionType(expression->first());
            const bool provenNonNull = receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
            if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
                accessorCall != nullptr)
            {
                auto call = lowerResolvedCall(accessorCall, {}, nullptr, !provenNonNull);
                if (call == nullptr) return nullptr;
                if (provenNonNull) call->stringValue = "flow-nonnull";
                return call;
            }
            const MethodNameBinding* binding = nameBinding(expression);
            if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
                binding->kind != MethodNameBindingKind::ExplicitObjectField)
            {
                error = "T01-F03 null-conditional access has no unique instance-field binding";
                return nullptr;
            }
            auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (receiver == nullptr) return nullptr;
            auto node = makeNode(
                provenNonNull ? ProjectOopHirNodeKind::FieldValue
                              : ProjectOopHirNodeKind::NullConditionalFieldValue,
                expression, resultType);
            node->memberOrdinal = binding->candidateMemberOrdinals.front();
            node->booleanValue = provenNonNull;
            node->volatileAccess = memberModel_ != nullptr &&
                *node->memberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*node->memberOrdinal].isVolatile;
            node->children.push_back(std::move(receiver));
            return node;
        }

        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            if (core != nullptr && core->kind == MethodCoreOperationKind::FixedArrayLength)
            {
                if (addressContext)
                {
                    error = "NativeSharp 3.0 fixed-array Length is a constant value and is not addressable";
                    return nullptr;
                }
                const std::optional<std::uint64_t> count =
                    ProjectMemberModel::fixedArrayCount(core->elementTypeKey);
                if (!count.has_value())
                {
                    error = "NativeSharp 3.0 fixed-array Length lost its canonical extent";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::IntegerConstant, expression, core->resultTypeKey);
                node->integerValue = *count;
                return node;
            }
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayLength ||
                 core->kind == MethodCoreOperationKind::StringLength))
            {
                if (addressContext)
                {
                    error = "Stage 41 Length is an intrinsic value and is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto node = makeNode(
                    core->kind == MethodCoreOperationKind::ArrayLength
                        ? ProjectOopHirNodeKind::ArrayLength
                        : ProjectOopHirNodeKind::StringLength,
                    expression,
                    core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                return node;
            }
            if (core != nullptr && core->kind == MethodCoreOperationKind::IteratorCurrent)
            {
                // NS304-T06-F05: a `ref:`/`refmut:` item Current IS a reference into the frame, so
                // taking it in address context is allowed (it yields the reference, not a value).
                const bool itemByReference = core->resultTypeKey.starts_with("ref:") ||
                    core->resultTypeKey.starts_with("refmut:");
                if (addressContext && !itemByReference)
                {
                    error = "iterator Current is an intrinsic value and is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::IteratorCurrent, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                return node;
            }
            // NSC-LIMIT-008: la misma pregunta que hace isNaturallyAddressable, hecha una sola
            // vez. Antes estaba escrita aqui a mano y alli no se hacia, y esa discrepancia era el
            // defecto: un getter se clasificaba como direccionable y lowerReceiver no le hacia
            // temporal.
            if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
                accessorCall != nullptr)
            {
                const std::size_t accessorOrdinal = *accessorCall->selectedMemberOrdinal;
                if (addressContext && !memberModel_->members()[accessorOrdinal].isReferenceReturn)
                {
                    error = "Stage 38 properties are semantic accessor calls and are not addressable";
                    return nullptr;
                }
                return lowerResolvedCall(accessorCall, {});
            }
            // NS304-T03-F03: a marked list-pattern length member that resolved to a
            // zero-argument `Length()` method was auto-invoked in Sema; lower it as the
            // ordinary instance call it became.
            if (const MethodCallBinding* lengthCall = callBinding(expression);
                lengthCall != nullptr && expression->auxiliaryText() == "list-pattern-length")
            {
                return lowerResolvedCall(lengthCall, {});
            }
            // NS304-T08-F02: extension property getter auto-invocado en Sema (namespace call con
            // receptor reenviado). Se baja igual que la llamada de namespace del path de Call.
            if (const NamespaceFunctionCallBinding* namespaceCall =
                    namespaceFunctionCallBinding(expression);
                namespaceCall != nullptr)
            {
                if (namespaceCall->resolution != MethodCallResolutionKind::Resolved ||
                    !namespaceCall->selectedFunctionOrdinal.has_value() ||
                    namespaceFunctionModel_ == nullptr ||
                    *namespaceCall->selectedFunctionOrdinal >=
                        namespaceFunctionModel_->functions().size())
                {
                    error = "extension property getter reached HIR without a resolved target";
                    return nullptr;
                }
                const std::size_t functionOrdinal = *namespaceCall->selectedFunctionOrdinal;
                const NamespaceFunctionSymbol& target =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::StaticCall, expression, namespaceCall->resultTypeKey);
                node->namespaceFunctionOrdinal = functionOrdinal;
                node->stringValue = target.mangledName;
                const std::size_t targetCallableOrdinal =
                    (memberModel_ != nullptr ? memberModel_->members().size() : 0) +
                    functionOrdinal;
                bool hasOwnedTemporary = false;
                if (namespaceCall->forwardedReceiver)
                {
                    if (namespaceCall->receiverSyntax == nullptr || target.parameters.empty() ||
                        namespaceCall->selectedArgumentParameterOrdinals.empty())
                    {
                        error = "extension property getter lost its receiver contract";
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
                }
                return node;
            }
            if (currentLambda_ != nullptr)
            {
                const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(expression);
                if (lambdaValue != nullptr && lambdaValue->lambdaOrdinal == currentLambda_->lambdaOrdinal)
                {
                    if (lambdaValue->lambdaParameter)
                    {
                        if (lambdaValue->lambdaParameterByReference)
                        {
                            const std::string pointerType = lambdaValue->typeKey + "*";
                            auto pointer = makeNode(
                                ProjectOopHirNodeKind::ParameterValue, expression, pointerType);
                            pointer->parameterOrdinal = lambdaValue->valueOrdinal;
                            if (addressContext) return pointer;
                            auto value = makeNode(
                                ProjectOopHirNodeKind::PointerLoad, expression, lambdaValue->typeKey);
                            value->children.push_back(std::move(pointer));
                            return value;
                        }
                        auto node = makeNode(
                            addressContext ? ProjectOopHirNodeKind::ParameterAddress : ProjectOopHirNodeKind::ParameterValue,
                            expression, lambdaValue->typeKey);
                        node->parameterOrdinal = lambdaValue->valueOrdinal;
                        return node;
                    }
                    auto node = makeNode(
                        addressContext
                            ? ProjectOopHirNodeKind::ClosureCaptureAddress
                            : ProjectOopHirNodeKind::ClosureCaptureValue,
                        expression,
                        lambdaValue->typeKey);
                    node->integerValue = lambdaValue->valueOrdinal;
                    return node;
                }
            }
            const MethodNameBinding* binding = nameBinding(expression);
            if (binding == nullptr)
            {
                error = "Stage 34 cannot map identifier/member expression to Stage 27 binding";
                return nullptr;
            }
            if (binding->kind == MethodNameBindingKind::Parameter)
            {
                // C4 "language references": a by-reference parameter physically receives an
                // ADDRESS, and every use of it means the value on the other side. So the
                // parameter slot itself is the pointer:
                //
                //   value context   -> load the pointer, then load through it (PointerLoad)
                //   address context -> the pointer IS the address; hand it over untouched
                //
                // which is exactly how `*p` already behaves for an explicit pointer parameter.
                // Nothing new reaches MIR or the backend: `ref`/`out`/`in` are rules on top of
                // machinery C1 already certified, not a second calling convention.
                const MethodParameterSymbol* parameter = nullptr;
                if (binding->parameterOrdinal.has_value() && memberModel_ != nullptr)
                {
                    if (binding->methodMemberOrdinal < memberModel_->members().size())
                    {
                        const ClassMemberSymbol& owner =
                            memberModel_->members()[binding->methodMemberOrdinal];
                        if (*binding->parameterOrdinal < owner.parameters.size())
                            parameter = &owner.parameters[*binding->parameterOrdinal];
                    }
                    else if (namespaceFunctionModel_ != nullptr)
                    {
                        const std::size_t functionOrdinal =
                            binding->methodMemberOrdinal - memberModel_->members().size();
                        if (functionOrdinal < namespaceFunctionModel_->functions().size())
                        {
                            const NamespaceFunctionSymbol& owner =
                                namespaceFunctionModel_->functions()[functionOrdinal];
                            if (*binding->parameterOrdinal < owner.parameters.size())
                                parameter = &owner.parameters[*binding->parameterOrdinal];
                        }
                    }
                }
                if (parameter != nullptr && parameter->passKind != ParameterPassKind::Value)
                {
                    auto pointer = makeNode(
                        ProjectOopHirNodeKind::ParameterValue, expression, parameter->typeKey);
                    pointer->parameterOrdinal = binding->parameterOrdinal;
                    if (addressContext) return pointer;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad, expression, parameter->pointeeTypeKey);
                    value->children.push_back(std::move(pointer));
                    return value;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::ParameterAddress : ProjectOopHirNodeKind::ParameterValue,
                    expression,
                    binding->typeKey);
                node->parameterOrdinal = binding->parameterOrdinal;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::Local)
            {
                const MethodLocalSymbol* local = binding->localOrdinal.has_value() &&
                        *binding->localOrdinal < methodBodyModel_->locals().size()
                    ? &methodBodyModel_->locals()[*binding->localOrdinal]
                    : nullptr;
                if (local != nullptr && local->isReference)
                {
                    // The reference local's slot stores a pointer. Address context consumes that
                    // pointer directly; value context loads through it once more.
                    auto pointer = makeNode(
                        ProjectOopHirNodeKind::LocalValue,
                        expression,
                        binding->typeKey + "*");
                    pointer->localOrdinal = binding->localOrdinal;
                    if (addressContext) return pointer;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad,
                        expression,
                        binding->typeKey);
                    value->children.push_back(std::move(pointer));
                    return value;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::LocalAddress : ProjectOopHirNodeKind::LocalValue,
                    expression,
                    binding->typeKey);
                node->localOrdinal = binding->localOrdinal;
                node->volatileAccess = local != nullptr && local->isVolatile;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::ImplicitField ||
                binding->kind == MethodNameBindingKind::ExplicitThisField ||
                binding->kind == MethodNameBindingKind::ExplicitObjectField ||
                binding->kind == MethodNameBindingKind::ExplicitBaseField)
            {
                if (binding->candidateMemberOrdinals.empty())
                {
                    error = "Macro A field binding has no candidate member";
                    return nullptr;
                }
                const std::size_t fieldOrdinal = binding->candidateMemberOrdinals.front();
                auto receiver = lowerReceiver(
                    binding->receiverSyntax, binding->receiverTypeKey, memberOrdinal, error);
                if (receiver == nullptr)
                {
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::FieldAddress : ProjectOopHirNodeKind::FieldValue,
                    expression,
                    binding->typeKey);
                node->memberOrdinal = fieldOrdinal;
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                node->baseReceiver = binding->baseReceiver;
                node->children.push_back(std::move(receiver));
                return node;
            }
            if (binding->kind == MethodNameBindingKind::NamespaceState)
            {
                if (binding->candidateMemberOrdinals.size() != 1 ||
                    namespaceFunctionModel_ == nullptr ||
                    binding->candidateMemberOrdinals.front() >=
                        namespaceFunctionModel_->states().size())
                {
                    error = "namespace state binding has no unique state ordinal";
                    return nullptr;
                }
                const std::size_t stateOrdinal =
                    binding->candidateMemberOrdinals.front();
                const NamespaceStateSymbol& state =
                    namespaceFunctionModel_->states()[stateOrdinal];
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::NamespaceStateAddress
                                   : ProjectOopHirNodeKind::NamespaceStateValue,
                    expression,
                    binding->typeKey);
                node->namespaceStateOrdinal = stateOrdinal;
                node->memberOrdinal =
                    (memberModel_ != nullptr ? memberModel_->members().size() : 0) +
                    stateOrdinal;
                node->stringValue = state.storageName;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::StaticField)
            {
                if (binding->candidateMemberOrdinals.empty())
                {
                    error = "Stage 37 static-field binding has no candidate member";
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::StaticFieldAddress
                                   : ProjectOopHirNodeKind::StaticFieldValue,
                    expression,
                    binding->typeKey);
                node->memberOrdinal = binding->candidateMemberOrdinals.front();
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::TypeReference)
            {
                error = "Stage 37 type reference may only appear as the receiver of a static member access";
                return nullptr;
            }
            error = "Stage 34 method-group expression can only appear as call target";
            return nullptr;
        }

        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, resultType);
            node->integerValue = expression->integerValue();
            return node;
        }

        case ast::AstNodeKind::FloatingLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, resultType);
            node->floatingValue = expression->floatingValue();
            return node;
        }

        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::NameOfExpression:
        {
            if (addressContext)
            {
                error = "Stage 41 string literals are immutable references and are not addressable values";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = expression->text();
            return node;
        }

        case ast::AstNodeKind::InterpolatedStringExpression:
        {
            if (addressContext)
            {
                error = "a static interpolated string is immutable and not addressable";
                return nullptr;
            }
            const std::string* value = methodBodyModel_ != nullptr
                ? methodBodyModel_->staticInterpolationValue(expression) : nullptr;
            if (value == nullptr)
            {
                error = "T01-F06 static interpolation has no compile-time handler result";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = *value;
            return node;
        }

        case ast::AstNodeKind::BooleanLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::BooleanConstant, expression, "builtin:bool");
            node->booleanValue = expression->tokenKind() == lexer::TokenKind::KwTrue;
            return node;
        }

        case ast::AstNodeKind::NullLiteralExpression:
            return makeNode(ProjectOopHirNodeKind::NullConstant, expression, "builtin:null");

        case ast::AstNodeKind::DefaultValueExpression:
        {
            // Stage 52: default(T) -- the resolved concrete type (already monomorphized, if T
            // was a generic parameter -- see ProjectMethodBodyModel's DefaultValueExpression
            // case) was recorded during semantic analysis; LLVM lowering builds the all-zero-bits
            // value of that EXACT type (see the DefaultValue case in ProjectOopLlvmLowerer.cpp),
            // which is uniformly correct for every T: 0/false/'\0' for a numeric primitive, a
            // null pointer for a reference type or string, an all-zero-fields instance for a
            // value-typed struct.
            if (addressContext)
            {
                error = "Stage 52 default(T) value is not addressable";
                return nullptr;
            }
            const MethodExpressionTypeBinding* type = expressionType(expression);
            if (type == nullptr)
            {
                error = "Stage 52 default(T) expression has no resolved semantic type";
                return nullptr;
            }
            return makeNode(ProjectOopHirNodeKind::DefaultValue, expression, type->typeKey);
        }

        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
        {
            const MethodCallBinding* operatorCall = callBinding(expression);
            if (operatorCall != nullptr && operatorCall->resolution == MethodCallResolutionKind::Resolved &&
                operatorCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal) != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal)->isUserOperator)
            {
                return lowerResolvedCall(operatorCall, {expression->first()});
            }
            if (expression->tokenKind() == lexer::TokenKind::PlusPlus ||
                expression->tokenKind() == lexer::TokenKind::MinusMinus)
            {
                // ID 79. `++x` es `x += 1` y `x++` es lo mismo devolviendo el valor anterior, asi
                // que los dos salen por el MISMO nodo que la asignacion compuesta y no por una
                // ruta paralela. La tarea 62 pedia extender esto "para punteros" cuando no existia
                // para ningun tipo; aqui entra para los dos a la vez, que es la unica forma de que
                // `++p` y `++i` no se separen.
                if (addressContext)
                {
                    error = "the result of ++/-- is a value, not addressable storage";
                    return nullptr;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "Stage 34 ++/-- operand has no resolved semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;

                auto step = makeNode(ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
                step->operation = expression->tokenKind() == lexer::TokenKind::PlusPlus
                    ? lexer::TokenKind::Plus
                    : lexer::TokenKind::Minus;
                // La UNICA diferencia entre postfijo y prefijo.
                step->booleanValue = expression->kind() == ast::AstNodeKind::PostfixExpression;

                std::unique_ptr<ProjectOopHirNode> one;
                if (ProjectTypeConversion::isPointer(targetType->typeKey))
                {
                    // `++p` avanza UN ELEMENTO, no un byte. El tipo apuntado es lo que da el paso,
                    // y lo resuelve el MIR con el mismo servicio de tamano que todo lo demas.
                    step->elementTypeKey = targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
                    one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nint");
                    one->integerValue = 1;
                }
                else if (ProjectTypeConversion::isNumeric(targetType->typeKey) &&
                         !ProjectTypeConversion::isIntegral(targetType->typeKey))
                {
                    one = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, targetType->typeKey);
                    one->floatingValue = 1.0;
                }
                else
                {
                    // El uno se construye YA en el tipo del destino, no en `int` con una conversion
                    // detras: asi la operacion entera ocurre en un solo tipo y no hay una segunda
                    // regla de promocion que mantener de acuerdo con la de `Binary`.
                    one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, targetType->typeKey);
                    one->integerValue = 1;
                }
                step->children.push_back(std::move(address));
                step->children.push_back(std::move(one));
                return step;
            }
            // Interop Foundation: `&lvalue` and `*pointer` are now first-class in the OOP
            // pipeline. HISTORY: both used to be walled off ("keeps raw pointer
            // address/dereference in the procedural backend boundary"), which made every
            // out-parameter Win32 API unusable -- `GetMessage(&msg)` could not produce a MSG*
            // from a local at all. Since Stage 40.2 every local is memory-homed (an alloca,
            // with indirect structs carried as [N x i8]), so an address-of is LITERALLY the
            // operand lowered in address context -- no new machinery, just returning what the
            // assignment path always computed for its own targets.
            //
            // These are the language's RAW tier -- same third reference category the Bloque 1
            // docs pin for plain aliases: no lifetime tracking, dangles if it outlives its
            // target, by design. C APIs are expressed through exactly this shape.
            if (expression->tokenKind() == lexer::TokenKind::Amp)
            {
                if (addressContext)
                {
                    error = "Stage 34 address-of result is not itself addressable";
                    return nullptr;
                }
                // Bit fields are logical slices of an allocation unit, not independently
                // addressable objects.  Assignment lowering may request their allocation-unit
                // address internally, but source `&field` must never expose that address as a
                // pointer to the logical field type.
                if (const MethodNameBinding* target = nameBinding(expression->first());
                    target != nullptr && !target->candidateMemberOrdinals.empty() &&
                    memberModel_ != nullptr &&
                    target->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                    memberModel_->members()[target->candidateMemberOrdinals.front()].isBitField)
                {
                    error = "bit fields have no address";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                // WRAPPED, never mutated: overwriting the operand node's typeKey was measured
                // to miscompile -- a ParameterValue whose typeKey was rewritten to the pointee
                // struct loaded the [N x i8] aggregate where the pointer belonged ("GEP base
                // pointer is not a vector..."). The wrapper carries the expression's type (T*)
                // for consumers that derive ABI decisions from operand typeKeys (call-site
                // indirect-argument wrapping reads exactly that), while the operand keeps its
                // own meaning.
                auto node = makeNode(ProjectOopHirNodeKind::AddressValue, expression, resultType);
                node->children.push_back(std::move(address));
                return node;
            }
            if (expression->tokenKind() == lexer::TokenKind::Star)
            {
                if (addressContext)
                {
                    // The ADDRESS of `*p` is p's own value: `(*m).field = x` GEPs off it, and
                    // `*p = v` stores straight through it.
                    auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
                    if (pointer == nullptr) return nullptr;
                    // Returned AS-IS: the pointer's value IS the address, and its own typeKey
                    // (T*) is already the honest description of an address-of-T.
                    return pointer;
                }
                auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (pointer == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
                node->children.push_back(std::move(pointer));
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Unary, expression, resultType);
            node->operation = expression->tokenKind();
            auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::NullCoalescingExpression:
        {
            if (addressContext)
            {
                error = "a null-coalescing expression produces a value, not addressable storage";
                return nullptr;
            }
            auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
            auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (left == nullptr || right == nullptr) return nullptr;
            const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                right->typeKey,
                resultType,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            if (!conversion.exists())
            {
                error = "T01-F03 null-coalescing RHS cannot convert from '" + right->typeKey +
                    "' to '" + resultType + "'";
                return nullptr;
            }
            if (conversion.kind != ProjectConversionKind::Identity)
                right = wrapImplicitConversion(std::move(right), conversion, expression->second());
            const MethodOptionalCarrierBinding* optional = optionalCarrierBinding(expression);
            auto node = makeNode(
                optional != nullptr ? ProjectOopHirNodeKind::OptionalCoalescing
                                    : ProjectOopHirNodeKind::NullCoalescing,
                expression, resultType);
            if (optional != nullptr)
            {
                node->elementTypeKey = optional->carrierTypeKey;
                node->memberOrdinal = optional->hasValueMemberOrdinal;
                node->secondaryMemberOrdinal = optional->uncheckedValueMemberOrdinal;
            }
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            return node;
        }

        case ast::AstNodeKind::MatchValueExpression:
        {
            if (addressContext)
            {
                auto node = makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
                node->referenceResult = true;
                return node;
            }
            return makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
        }

        case ast::AstNodeKind::MatchExpression:
        {
            const MethodMatchExpressionBinding* binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->matchExpressionBinding(expression) : nullptr;
            if (binding == nullptr)
            {
                error = "T03-F03 match expression has no semantic binding";
                return nullptr;
            }
            if (addressContext && !binding->referenceResult)
            {
                error = "a value match expression does not produce addressable storage";
                return nullptr;
            }

            auto node = makeNode(ProjectOopHirNodeKind::MatchExpression, expression, resultType);
            node->referenceResult = binding->referenceResult;
            node->governingReference = binding->governingReference;
            node->secondaryMemberOrdinal = binding->switchTagMemberOrdinal;
            node->booleanValue = binding->switchTagMemberOrdinal.has_value();
            auto governing = lowerExpression(
                expression->first(), memberOrdinal, binding->governingReference, error);
            if (governing == nullptr) return nullptr;
            node->children.push_back(std::move(governing));

            for (std::size_t armIndex = 0; armIndex < expression->children().size(); ++armIndex)
            {
                const ast::AstNode* armSyntax = expression->children()[armIndex];
                if (armSyntax == nullptr || armSyntax->first() == nullptr ||
                    armSyntax->third() == nullptr)
                {
                    error = "T03-F03 malformed match expression arm";
                    return nullptr;
                }
                auto arm = makeNode(ProjectOopHirNodeKind::MatchArm, armSyntax, resultType);
                if (armIndex < binding->armTags.size())
                    arm->switchLabels = binding->armTags[armIndex];
                auto condition = lowerExpression(armSyntax->first(), memberOrdinal, false, error);
                if (condition == nullptr) return nullptr;
                arm->children.push_back(std::move(condition));

                if (armSyntax->second() != nullptr)
                {
                    auto guard = lowerExpression(armSyntax->second(), memberOrdinal, false, error);
                    if (guard == nullptr) return nullptr;
                    arm->children.push_back(std::move(guard));
                }
                else
                {
                    auto guard = makeNode(
                        ProjectOopHirNodeKind::BooleanConstant, armSyntax, "builtin:bool");
                    guard->booleanValue = true;
                    arm->children.push_back(std::move(guard));
                }

                const std::size_t cleanupDepth = activeHirCleanupRegions_.size();
                for (const ast::AstNode* declaration : armSyntax->children())
                {
                    auto lowered = lowerStatement(declaration, memberOrdinal, error);
                    if (lowered == nullptr)
                    {
                        activeHirCleanupRegions_.resize(cleanupDepth);
                        return nullptr;
                    }
                    if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                        lowered->cleanupRegionOrdinal.has_value())
                    {
                        activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
                    }
                    arm->children.push_back(std::move(lowered));
                }

                auto value = lowerExpression(
                    armSyntax->third(), memberOrdinal, binding->referenceResult, error);
                if (value == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepth);
                    return nullptr;
                }
                if (!binding->referenceResult && armIndex < binding->armConversions.size())
                {
                    const ProjectImplicitConversion& conversion = binding->armConversions[armIndex];
                    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                        value = wrapImplicitConversion(
                            std::move(value), conversion, armSyntax->third());
                }
                arm->children.push_back(std::move(value));
                arm->cleanupRegionOrdinals.assign(
                    activeHirCleanupRegions_.begin() + static_cast<std::ptrdiff_t>(cleanupDepth),
                    activeHirCleanupRegions_.end());
                activeHirCleanupRegions_.resize(cleanupDepth);
                node->children.push_back(std::move(arm));
            }

            if (!binding->referenceResult || addressContext) return node;
            auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
            load->children.push_back(std::move(node));
            return load;
        }

        case ast::AstNodeKind::MatchArmExpression:
            error = "a match arm reached HIR outside its match expression";
            return nullptr;

        case ast::AstNodeKind::ConditionalExpression:
        {
            if (addressContext)
            {
                error = "a conditional expression produces a value, not addressable storage";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Conditional, expression, resultType);
            auto condition = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (condition == nullptr) return nullptr;
            auto whenTrue = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (whenTrue == nullptr) return nullptr;
            auto whenFalse = lowerExpression(expression->third(), memberOrdinal, false, error);
            if (whenFalse == nullptr) return nullptr;

            // Both branches have to arrive at the phi AS THE RESULT TYPE. The front end already
            // decided that type -- the common numeric type when the branches differ -- but
            // deciding it is not the same as producing it: without these conversions, `n > 0 ? anInt
            // : aLong` lowers to a phi whose result is `i64` and whose first incoming value is
            // still `i32`, and the LLVM verifier rejects the module with "PHI node operands are not
            // the same type as the result". Same treatment the binary operators give their operands
            // once commonNumericType has spoken.
            const auto convertBranch = [&](std::unique_ptr<ProjectOopHirNode>& branch,
                                           const ast::AstNode* syntax)
            {
                if (branch->typeKey == resultType) return;
                const MethodExpressionTypeBinding* binding = expressionType(syntax);
                const ProjectImplicitConversion conversion = ProjectTypeConversion::classifyImplicit(
                    branch->typeKey,
                    resultType,
                    binding != nullptr ? binding->integerConstant : std::nullopt);
                if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                {
                    branch = wrapImplicitConversion(std::move(branch), conversion, syntax);
                }
            };
            convertBranch(whenTrue, expression->second());
            convertBranch(whenFalse, expression->third());

            node->children.push_back(std::move(condition));
            node->children.push_back(std::move(whenTrue));
            node->children.push_back(std::move(whenFalse));
            return node;
        }

        case ast::AstNodeKind::BinaryExpression:
        {
            const MethodCoreBinding* stringCore = coreBinding(expression);
            // NSC-LIMIT-004. `a == b` y `a != b` sobre dos structs. El modelo semantico registro
            // un `StructEquals` sobre el propio BinaryExpression, asi que aqui se construye el
            // MISMO nodo que produce `a.Equals(b)` -- no hay una segunda semantica de igualdad de
            // agregados, solo dos formas de escribirla. La clase del nodo de sintaxis es lo que
            // las distingue: alli es una CallExpression, aqui un BinaryExpression.
            //
            // Para `!=` se envuelve la igualdad en el `Unary`/`Bang` que ya existe, en vez de
            // dar al nodo de agregados un modo negado propio: negar un bool es una instruccion, y
            // un segundo modo seria un segundo sitio donde la regla puede discrepar.
            if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::StructEquals)
            {
                if (addressContext)
                {
                    error = "C8 struct comparison result is not addressable";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::AggregateEquals, expression, stringCore->resultTypeKey);
                node->elementTypeKey = stringCore->elementTypeKey;
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (left == nullptr) return nullptr;
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (right == nullptr) return nullptr;
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                node->argumentCount = node->children.size();
                if (!appendAggregateLeaves(
                        stringCore->elementTypeKey, 0, expression, node->children, error))
                {
                    return nullptr;
                }
                if (expression->tokenKind() == lexer::TokenKind::BangEqual)
                {
                    auto negated = makeNode(
                        ProjectOopHirNodeKind::Unary, expression, stringCore->resultTypeKey);
                    negated->operation = lexer::TokenKind::Bang;
                    negated->children.push_back(std::move(node));
                    return negated;
                }
                return node;
            }
            if (stringCore != nullptr &&
                (stringCore->kind == MethodCoreOperationKind::StringConcat ||
                 stringCore->kind == MethodCoreOperationKind::StringEquals))
            {
                if (addressContext)
                {
                    error = "Stage 51 string operation result is not addressable";
                    return nullptr;
                }
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (left == nullptr || right == nullptr) return nullptr;
                auto node = makeNode(
                    stringCore->kind == MethodCoreOperationKind::StringConcat
                        ? ProjectOopHirNodeKind::StringConcat
                        : ProjectOopHirNodeKind::StringEquals,
                    expression,
                    stringCore->resultTypeKey);
                node->operation = expression->tokenKind();
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                return node;
            }

            // CC1 fase 4D. El binding ya decidio que esto es aritmetica de punteros y con que
            // tipo de elemento; aqui solo se construye el nodo con los dos operandos EN EL ORDEN
            // EN QUE SE ESCRIBIERON. Ese orden importa: `i + p` y `p + i` dan lo mismo, pero
            // `p - i` y `i - p` no, y el backend necesita saber cual de los dos hijos es el
            // puntero. Lo deduce de los tipos de los hijos, no de una bandera aparte.
            if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::PointerArithmetic)
            {
                if (addressContext)
                {
                    error = "CC1 pointer arithmetic yields a value, not addressable storage";
                    return nullptr;
                }
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (left == nullptr) return nullptr;
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (right == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::PointerArithmetic, expression, stringCore->resultTypeKey);
                node->operation = expression->tokenKind();
                node->elementTypeKey = stringCore->elementTypeKey;
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                return node;
            }

            const MethodCallBinding* operatorCall = callBinding(expression);
            if (operatorCall != nullptr && operatorCall->resolution == MethodCallResolutionKind::Resolved &&
                operatorCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal) != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal)->isUserOperator)
            {
                return lowerResolvedCall(operatorCall, {expression->first(), expression->second()});
            }
            auto node = makeNode(ProjectOopHirNodeKind::Binary, expression, resultType);
            node->operation = expression->tokenKind();
            node->booleanValue = typeBinding != nullptr && typeBinding->flowRuntimeCheckElided;
            auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
            auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (left == nullptr || right == nullptr)
            {
                return nullptr;
            }

            // The semantic model admits exactly one implicit integer-to-enum form: literal zero
            // in ==/!=. Materialize that decision as the same Conversion node an explicit enum
            // cast uses. This matters for byte/short/long-backed enums: leaving the zero as i32
            // would make the LLVM comparison operands disagree even though the frontend accepted
            // them.
            const auto isEnumTypeKey = [&](std::string_view typeKey) noexcept
            {
                if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
                const std::string_view identity = typeKey.substr(5);
                return std::any_of(
                    nameLookup_->namedTypes().begin(),
                    nameLookup_->namedTypes().end(),
                    [&](const NamedTypeDeclaration& type)
                    {
                        return type.kind == NamedTypeDeclarationKind::Enum &&
                            type.identity.key() == identity;
                    });
            };
            const auto isZeroIntegerLiteral = [](const ast::AstNode* syntax) noexcept
            {
                return syntax != nullptr &&
                    syntax->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
                    syntax->integerValue() == 0;
            };
            if (expression->tokenKind() == lexer::TokenKind::EqualEqual ||
                expression->tokenKind() == lexer::TokenKind::BangEqual)
            {
                if (isEnumTypeKey(left->typeKey) && isZeroIntegerLiteral(expression->second()))
                {
                    auto converted = makeNode(
                        ProjectOopHirNodeKind::Conversion, expression->second(), left->typeKey);
                    converted->children.push_back(std::move(right));
                    right = std::move(converted);
                }
                else if (isEnumTypeKey(right->typeKey) && isZeroIntegerLiteral(expression->first()))
                {
                    auto converted = makeNode(
                        ProjectOopHirNodeKind::Conversion, expression->first(), right->typeKey);
                    converted->children.push_back(std::move(left));
                    left = std::move(converted);
                }
            }
            std::optional<std::string> numericOperandType;
            if (ProjectTypeConversion::isNumeric(resultType))
            {
                numericOperandType = resultType;
            }
            else if (isComparison(expression->tokenKind()) &&
                ProjectTypeConversion::isNumeric(left->typeKey) &&
                ProjectTypeConversion::isNumeric(right->typeKey))
            {
                const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
                const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
                numericOperandType = ProjectTypeConversion::commonNumericType(
                    left->typeKey,
                    right->typeKey,
                    leftType != nullptr ? leftType->integerConstant : std::nullopt,
                    rightType != nullptr ? rightType->integerConstant : std::nullopt);
            }
            if (numericOperandType.has_value())
            {
                const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
                const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
                const ProjectImplicitConversion leftConversion = ProjectTypeConversion::classifyImplicit(
                    left->typeKey,
                    *numericOperandType,
                    leftType != nullptr ? leftType->integerConstant : std::nullopt);
                const ProjectImplicitConversion rightConversion = ProjectTypeConversion::classifyImplicit(
                    right->typeKey,
                    *numericOperandType,
                    rightType != nullptr ? rightType->integerConstant : std::nullopt);
                if (leftConversion.exists() && leftConversion.kind != ProjectConversionKind::Identity)
                {
                    left = wrapImplicitConversion(std::move(left), leftConversion, expression->first());
                }
                if (rightConversion.exists() && rightConversion.kind != ProjectConversionKind::Identity)
                {
                    right = wrapImplicitConversion(std::move(right), rightConversion, expression->second());
                }
            }
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            return node;
        }

        case ast::AstNodeKind::IndexExpression:
        case ast::AstNodeKind::NullConditionalIndexExpression:
        {
            if (const MethodRangeSliceBinding* range = rangeSliceBinding(expression);
                range != nullptr)
            {
                if (addressContext)
                {
                    error = "T01-F04 a range slice is a value and cannot be used as assignable storage";
                    return nullptr;
                }
                if (memberModel_ == nullptr ||
                    range->lengthMemberOrdinal >= memberModel_->members().size() ||
                    range->sliceMemberOrdinal >= memberModel_->members().size())
                {
                    error = "T01-F04 range protocol lost its selected Length/Slice members";
                    return nullptr;
                }
                auto receiver = lowerReceiver(
                    range->receiverSyntax, range->receiverTypeKey, memberOrdinal, error);
                if (receiver == nullptr) return nullptr;

                const auto lowerEndpoint = [&](
                    const ast::AstNode* syntax,
                    const ProjectImplicitConversion& conversion)
                    -> std::unique_ptr<ProjectOopHirNode>
                {
                    if (syntax == nullptr)
                    {
                        auto zero = makeNode(
                            ProjectOopHirNodeKind::IntegerConstant,
                            expression,
                            "builtin:int");
                        zero->integerValue = 0;
                        return zero;
                    }
                    auto endpoint = lowerExpression(syntax, memberOrdinal, false, error);
                    if (endpoint == nullptr) return nullptr;
                    if (conversion.exists() &&
                        conversion.kind != ProjectConversionKind::Identity)
                    {
                        endpoint = wrapImplicitConversion(
                            std::move(endpoint), conversion, syntax);
                    }
                    return endpoint;
                };

                auto start = lowerEndpoint(range->startSyntax, range->startConversion);
                auto end = lowerEndpoint(range->endSyntax, range->endConversion);
                if (start == nullptr || end == nullptr) return nullptr;

                const ClassMemberSymbol& sliceMember =
                    memberModel_->members()[range->sliceMemberOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::RangeSlice,
                    expression,
                    range->resultTypeKey);
                node->elementTypeKey = range->receiverTypeKey;
                node->memberOrdinal = range->sliceMemberOrdinal;
                node->secondaryMemberOrdinal = range->lengthMemberOrdinal;
                node->referenceResult = sliceMember.isReferenceReturn;
                node->integerValue =
                    (range->hasStart ? 1U : 0U) |
                    (range->startFromEnd ? 2U : 0U) |
                    (range->hasEnd ? 4U : 0U) |
                    (range->endFromEnd ? 8U : 0U) |
                    (range->boundsStaticallyProven ? 16U : 0U);
                node->argumentCount = 2;
                node->argumentParameterOrdinals = {0U, 1U};
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(start));
                node->children.push_back(std::move(end));
                return node;
            }
            const MethodCoreBinding* core = coreBinding(expression);
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayElement ||
                 core->kind == MethodCoreOperationKind::RangeArrayElement ||
                 core->kind == MethodCoreOperationKind::StringElement))
            {
                if (core->kind == MethodCoreOperationKind::StringElement && addressContext)
                {
                    error = "Stage 41 strings are immutable; string elements are not assignable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                std::vector<const ast::AstNode*> indexSyntaxes;
                if (!expression->children().empty())
                    indexSyntaxes.assign(expression->children().begin(), expression->children().end());
                else indexSyntaxes.push_back(expression->second());
                ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::StringElementValue;
                if (core->kind == MethodCoreOperationKind::ArrayElement)
                {
                    kind = addressContext
                        ? ProjectOopHirNodeKind::ArrayElementAddress
                        : ProjectOopHirNodeKind::ArrayElementValue;
                }
                else if (core->kind == MethodCoreOperationKind::RangeArrayElement)
                {
                    kind = addressContext
                        ? ProjectOopHirNodeKind::RangeArrayElementAddress
                        : ProjectOopHirNodeKind::RangeArrayElementValue;
                }
                auto node = makeNode(kind, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->nativeArrayRank = core->kind == MethodCoreOperationKind::RangeArrayElement
                    ? indexSyntaxes.size() : 1U;
                node->children.push_back(std::move(receiver));
                for (std::size_t dimension = 0; dimension < indexSyntaxes.size(); ++dimension)
                {
                    const ast::AstNode* syntax = indexSyntaxes[dimension];
                    const bool fromEnd = syntax != nullptr &&
                        syntax->kind() == ast::AstNodeKind::FromEndExpression;
                    if (dimension == 0U) node->booleanValue = fromEnd;
                    if (fromEnd && dimension < 64U)
                        node->integerValue |= (std::uint64_t{1} << dimension);
                    auto index = lowerExpression(
                        fromEnd ? syntax->first() : syntax, memberOrdinal, false, error);
                    if (index == nullptr) return nullptr;
                    node->children.push_back(std::move(index));
                }
                return node;
            }
            // NS304-T10-F05: `receiver?[index]` — load condicional de elemento de array nativo.
            if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalArrayElement)
            {
                if (addressContext)
                {
                    error = "T10-F05 null-conditional array element is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                const ast::AstNode* indexSyntax = !expression->children().empty()
                    ? expression->children().front() : expression->second();
                auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                if (index == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalArrayLoad, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(index));
                return node;
            }
            // NS304-T10-F05: `receiver?[index]` sobre un indexador de usuario — null-check +
            // llamada al get_Item (receptor evaluado una vez por lowerResolvedCall).
            if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalUserIndex)
            {
                if (addressContext)
                {
                    error = "T10-F05 null-conditional user index is not addressable";
                    return nullptr;
                }
                const MethodCallBinding* indexerCall = callBinding(expression);
                if (indexerCall == nullptr ||
                    indexerCall->resolution != MethodCallResolutionKind::Resolved ||
                    !indexerCall->selectedMemberOrdinal.has_value())
                {
                    error = "T10-F05 null-conditional user index has no resolved get_Item accessor";
                    return nullptr;
                }
                std::vector<const ast::AstNode*> arguments;
                if (!expression->children().empty())
                    arguments.assign(expression->children().begin(), expression->children().end());
                else if (expression->second() != nullptr)
                    arguments.push_back(expression->second());
                return lowerResolvedCall(indexerCall, arguments, nullptr, true);
            }
            // C2.1 "fixed arrays": inline, unmanaged element access -- `fixedArrayField[i]` and
            // `rawPointer[i]`. The only difference between the two is how the BASE ADDRESS is
            // obtained, and it is a real difference: a fixed array IS its storage, so its address
            // comes from lowering the base in address context; a pointer HOLDS an address, so its
            // address comes from lowering the base as an ordinary value. Everything after that --
            // the scaled GEP, the load, the store -- is identical, which is why one node covers
            // both.
            if (core != nullptr && core->kind == MethodCoreOperationKind::RawElement)
            {
                const ast::AstNode* baseSyntax = expression->first();
                const MethodExpressionTypeBinding* baseType = expressionType(baseSyntax);
                const bool baseIsFixedArray = baseType != nullptr &&
                    ProjectMemberModel::fixedArrayElementTypeKey(baseType->typeKey).has_value();
                const bool baseIsFlexibleArray = baseType != nullptr &&
                    ProjectMemberModel::flexibleArrayElementTypeKey(baseType->typeKey).has_value();
                auto base = lowerExpression(
                    baseSyntax, memberOrdinal, baseIsFixedArray || baseIsFlexibleArray, error);
                if (base == nullptr) return nullptr;
                std::vector<const ast::AstNode*> indices;
                if (!expression->children().empty())
                    indices.assign(expression->children().begin(), expression->children().end());
                else if (expression->second() != nullptr)
                    indices.push_back(expression->second());

                const MethodVariableArrayIndexBinding* variableArrayIndex =
                    methodBodyModel_ != nullptr
                        ? methodBodyModel_->variableArrayIndexBinding(expression)
                        : nullptr;
                if (variableArrayIndex != nullptr)
                {
                    if (indices.size() != variableArrayIndex->extentLocalOrdinals.size())
                    {
                        error = "multidimensional VLA index lost its semantic rank";
                        return nullptr;
                    }
                    std::unique_ptr<ProjectOopHirNode> linearIndex;
                    for (std::size_t dimension = 0; dimension < indices.size(); ++dimension)
                    {
                        auto index = lowerExpression(indices[dimension], memberOrdinal, false, error);
                        if (index == nullptr) return nullptr;
                        if (index->typeKey != "builtin:nuint")
                        {
                            auto converted = makeNode(
                                ProjectOopHirNodeKind::Conversion, indices[dimension], "builtin:nuint");
                            converted->children.push_back(std::move(index));
                            index = std::move(converted);
                        }
                        if (linearIndex == nullptr)
                        {
                            linearIndex = std::move(index);
                            continue;
                        }

                        auto extent = makeNode(
                            ProjectOopHirNodeKind::LocalValue,
                            expression,
                            methodBodyModel_->locals()[
                                variableArrayIndex->extentLocalOrdinals[dimension]].typeKey);
                        extent->localOrdinal =
                            variableArrayIndex->extentLocalOrdinals[dimension];
                        if (extent->typeKey != "builtin:nuint")
                        {
                            auto convertedExtent = makeNode(
                                ProjectOopHirNodeKind::Conversion, expression, "builtin:nuint");
                            convertedExtent->children.push_back(std::move(extent));
                            extent = std::move(convertedExtent);
                        }
                        auto rowOffset = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
                        rowOffset->operation = lexer::TokenKind::Star;
                        rowOffset->children.push_back(std::move(linearIndex));
                        rowOffset->children.push_back(std::move(extent));
                        auto sum = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
                        sum->operation = lexer::TokenKind::Plus;
                        sum->children.push_back(std::move(rowOffset));
                        sum->children.push_back(std::move(index));
                        linearIndex = std::move(sum);
                    }

                    auto address = makeNode(
                        ProjectOopHirNodeKind::RawElementAddress,
                        expression,
                        core->elementTypeKey);
                    address->elementTypeKey = core->elementTypeKey;
                    address->children.push_back(std::move(base));
                    address->children.push_back(std::move(linearIndex));
                    if (addressContext) return address;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad,
                        expression,
                        core->resultTypeKey);
                    value->children.push_back(std::move(address));
                    return value;
                }

                std::string indexedType = baseType != nullptr ? baseType->typeKey : std::string();
                std::unique_ptr<ProjectOopHirNode> address;
                for (const ast::AstNode* indexSyntax : indices)
                {
                    const std::optional<std::string> fixedElement = baseIsFixedArray
                        ? ProjectMemberModel::fixedArrayElementTypeKey(indexedType)
                        : std::optional<std::string>();
                    const std::optional<std::string> flexibleElement = baseIsFlexibleArray
                        ? ProjectMemberModel::flexibleArrayElementTypeKey(indexedType)
                        : std::optional<std::string>();
                    const std::string elementType = fixedElement.has_value()
                        ? *fixedElement
                        : flexibleElement.has_value() ? *flexibleElement : core->elementTypeKey;
                    auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                    if (index == nullptr) return nullptr;
                    address = makeNode(
                        ProjectOopHirNodeKind::RawElementAddress, expression, elementType);
                    address->elementTypeKey = elementType;
                    address->children.push_back(std::move(base));
                    address->children.push_back(std::move(index));
                    base = std::move(address);
                    indexedType = elementType;
                }
                if (base == nullptr)
                {
                    error = "fixed-array/raw-pointer index expression has no index";
                    return nullptr;
                }
                if (addressContext) return base;
                auto value = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, core->resultTypeKey);
                value->children.push_back(std::move(base));
                return value;
            }

            const MethodCallBinding* indexerCall = callBinding(expression);
            if (indexerCall != nullptr && indexerCall->resolution == MethodCallResolutionKind::Resolved &&
                indexerCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *indexerCall->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*indexerCall->selectedMemberOrdinal].isIndexerAccessor &&
                memberModel_->members()[*indexerCall->selectedMemberOrdinal].isGetter)
            {
                const ClassMemberSymbol& getter =
                    memberModel_->members()[*indexerCall->selectedMemberOrdinal];
                if (addressContext && !getter.isReferenceReturn)
                {
                    error = "Stage 38 indexers are semantic accessor calls and are not directly addressable";
                    return nullptr;
                }
                std::vector<const ast::AstNode*> arguments;
                if (!expression->children().empty())
                {
                    arguments.insert(arguments.end(), expression->children().begin(), expression->children().end());
                }
                else if (expression->second() != nullptr)
                {
                    arguments.push_back(expression->second());
                }
                return lowerResolvedCall(indexerCall, arguments);
            }
            error = "Stage 38 OOP lowering requires class/interface index expressions to resolve to an indexer accessor";
            return nullptr;
        }

        case ast::AstNodeKind::AssignmentExpression:
        {
            if (expression->tokenKind() == lexer::TokenKind::QuestionQuestionEqual)
            {
                if (addressContext)
                {
                    error = "the result of null-coalescing assignment is a value, not addressable storage";
                    return nullptr;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "T01-F03 null-coalescing assignment target has no semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                auto fallback = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (address == nullptr || fallback == nullptr) return nullptr;
                const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                const ProjectImplicitConversion conversion = classifyImplicit(
                    fallback->typeKey,
                    targetType->typeKey,
                    rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                if (!conversion.exists())
                {
                    error = "T01-F03 null-coalescing assignment RHS cannot convert from '" +
                        fallback->typeKey + "' to '" + targetType->typeKey + "'";
                    return nullptr;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                    fallback = wrapImplicitConversion(
                        std::move(fallback), conversion, expression->second());
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullCoalescingAssignment,
                    expression,
                    targetType->typeKey);
                node->children.push_back(std::move(address));
                node->children.push_back(std::move(fallback));
                return node;
            }

            if (expression->tokenKind() == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::TupleExpression)
            {
                if (addressContext)
                {
                    error = "tuple deconstruction result is a value, not addressable storage";
                    return nullptr;
                }
                const MethodDeconstructionBinding* binding =
                    methodBodyModel_->deconstructionBinding(expression);
                if (binding == nullptr)
                {
                    error = "T02-F03 deconstruction assignment has no semantic binding";
                    return nullptr;
                }
                auto source = lowerDeconstructionSource(
                    expression, expression->second(), memberOrdinal, error);
                if (source == nullptr) return nullptr;
                const std::optional<std::vector<std::string>> sourceElements =
                    ProjectTypeConversion::tupleElementTypeKeys(binding->tupleTypeKey);
                if (!sourceElements.has_value() || objectLayoutModel_ == nullptr ||
                    nameLookup_ == nullptr)
                {
                    error = "T02-F03 deconstruction source has no tuple layout";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::TupleDeconstruct, expression, binding->tupleTypeKey);
                node->children.push_back(std::move(source));
                std::function<bool(const ast::AstNode*, const std::string&, std::uint64_t)>
                    appendTarget;
                appendTarget = [&](const ast::AstNode* targetSyntax,
                                   const std::string& sourceTypeKey,
                                   std::uint64_t byteOffset) -> bool
                {
                    if (targetSyntax == nullptr) return false;
                    if (targetSyntax->kind() == ast::AstNodeKind::TupleExpression)
                    {
                        const std::optional<std::vector<std::string>> nestedElements =
                            ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
                        const std::optional<language::AggregateLayout> nestedLayout =
                            objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, sourceTypeKey);
                        if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                            nestedElements->size() != targetSyntax->children().size() ||
                            nestedLayout->offsets.size() != nestedElements->size())
                        {
                            error = "T02-F03 nested assignment target lost its tuple layout";
                            return false;
                        }
                        for (std::size_t index = 0; index < nestedElements->size(); ++index)
                            if (!appendTarget(
                                    targetSyntax->children()[index], (*nestedElements)[index],
                                    byteOffset + nestedLayout->offsets[index])) return false;
                        return true;
                    }
                    if (targetSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                        targetSyntax->text() == "_") return true;
                    const MethodExpressionTypeBinding* targetType = expressionType(targetSyntax);
                    if (targetType == nullptr)
                    {
                        error = "T02-F03 deconstruction target has no semantic type";
                        return false;
                    }
                    auto address = lowerExpression(targetSyntax, memberOrdinal, true, error);
                    if (address == nullptr) return false;
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        sourceTypeKey, targetType->typeKey, std::nullopt);
                    if (!conversion.exists())
                    {
                        error = "T02-F03 deconstruction lost its validated element conversion";
                        return false;
                    }
                    auto element = makeNode(
                        ProjectOopHirNodeKind::TupleDeconstructElement,
                        targetSyntax,
                        targetType->typeKey);
                    element->elementTypeKey = sourceTypeKey;
                    element->integerValue = byteOffset;
                    element->conversion = conversion;
                    element->children.push_back(std::move(address));
                    node->children.push_back(std::move(element));
                    return true;
                };
                if (!appendTarget(expression->first(), binding->tupleTypeKey, 0)) return nullptr;
                return node;
            }

            const MethodEventBinding* stage42Event = eventBinding(expression);
            if (stage42Event != nullptr &&
                (stage42Event->kind == MethodEventOperationKind::Add ||
                 stage42Event->kind == MethodEventOperationKind::Remove))
            {
                if (addressContext)
                {
                    error = "Stage 42 event subscription expression is not addressable";
                    return nullptr;
                }
                if (stage42Event->customAccessorMemberOrdinal.has_value())
                {
                    // NS304-T09-F08: custom accessors -> instancia una llamada a add_/remove_.
                    auto node = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
                    node->memberOrdinal = *stage42Event->customAccessorMemberOrdinal;
                    if (!stage42Event->staticEvent)
                    {
                        auto receiver = lowerReceiver(
                            stage42Event->receiverSyntax, stage42Event->receiverTypeKey, memberOrdinal, error);
                        if (receiver == nullptr) return nullptr;
                        node->children.push_back(std::move(receiver));
                    }
                    auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
                    if (handler == nullptr) return nullptr;
                    if (!stage42Event->argumentConversions.empty() &&
                        stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
                    {
                        handler = wrapImplicitConversion(
                            std::move(handler), stage42Event->argumentConversions.front(), expression->second());
                    }
                    node->children.push_back(std::move(handler));
                    node->argumentParameterOrdinals.push_back(0);
                    node->argumentTypeKeys.push_back(stage42Event->delegateTypeKey);
                    return node;
                }
                auto backingAddress = lowerEventBacking(stage42Event, true);
                auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (backingAddress == nullptr || handler == nullptr) return nullptr;
                if (!stage42Event->argumentConversions.empty() &&
                    stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
                {
                    handler = wrapImplicitConversion(
                        std::move(handler), stage42Event->argumentConversions.front(), expression->second());
                }
                auto node = makeNode(
                    stage42Event->kind == MethodEventOperationKind::Add
                        ? ProjectOopHirNodeKind::EventAdd : ProjectOopHirNodeKind::EventRemove,
                    expression, "builtin:void");
                node->elementTypeKey = stage42Event->delegateTypeKey;
                node->memberOrdinal = stage42Event->backingMemberOrdinal;
                node->children.push_back(std::move(backingAddress));
                node->children.push_back(std::move(handler));
                return node;
            }

            const MethodCallBinding* setterCall = callBinding(expression);
            if (setterCall != nullptr && setterCall->resolution == MethodCallResolutionKind::Resolved &&
                setterCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *setterCall->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& setter = memberModel_->members()[*setterCall->selectedMemberOrdinal];
                if ((setter.isPropertyAccessor || setter.isIndexerAccessor) && setter.isSetter)
                {
                    std::vector<const ast::AstNode*> arguments;
                    if (setter.isIndexerAccessor && expression->first() != nullptr &&
                        expression->first()->kind() == ast::AstNodeKind::IndexExpression)
                    {
                        const ast::AstNode* indexExpression = expression->first();
                        if (!indexExpression->children().empty())
                        {
                            arguments.insert(
                                arguments.end(), indexExpression->children().begin(), indexExpression->children().end());
                        }
                        else if (indexExpression->second() != nullptr)
                        {
                            arguments.push_back(indexExpression->second());
                        }
                    }
                    arguments.push_back(expression->second());

                    // ID 82. `p.X op= e` es `set_X(p, get_X(p) op e)`. El valor que recibe el
                    // setter se arma AQUI, no se baja de la fuente.
                    //
                    // El operando izquierdo de esa operacion es la llamada al GETTER, y se obtiene
                    // bajando `expression->first()` en contexto de VALOR: los casos
                    // IdentifierExpression/MemberExpression e IndexExpression de esta misma
                    // funcion ya saben convertir ese nodo en la llamada al getter, con su receptor
                    // y sus argumentos de indice. No hay una segunda ruta de resolucion de
                    // accesores aqui; se reusa la unica que hay.
                    //
                    // EL RECEPTOR Y LOS INDICES SE BAJAN DOS VECES -- una dentro de la llamada al
                    // getter y otra para la del setter -- porque el arbol es de `unique_ptr` y un
                    // nodo no se comparte entre dos padres. Eso NO es un descuido: el modelo
                    // semantico ya rechazo toda forma en la que la segunda bajada pudiera dar algo
                    // distinto (isAccessorDestinationReevaluable), asi que lo que llega hasta aqui
                    // son locals, parametros, `this` y constantes. La diferencia con un destino
                    // CON direccion es que aquel si tiene una direccion que bajar una sola vez, y
                    // por eso ese caso lo resuelve `CompoundStore` unas lineas mas abajo.
                    //
                    // El operando derecho se baja UNA sola vez, dentro del `Binary`, DESPUES de la
                    // llamada al getter -- el mismo orden que C#: receptor, indices, getter, lado
                    // derecho, operacion, setter.
                    //
                    // La operacion ocurre entera en el tipo del destino, y la conversion del lado
                    // derecho se clasifica con la MISMA llamada que usa `CompoundStore`. El modelo
                    // semantico ya exigio que exista, asi que si no existe aqui es una discrepancia
                    // entre capas y se reporta como tal, no se ignora.
                    if (expression->tokenKind() != lexer::TokenKind::Equal)
                    {
                        if (addressContext)
                        {
                            error = "the result of a compound assignment is a value, not addressable storage";
                            return nullptr;
                        }
                        const lexer::TokenKind baseOperator =
                            lexer::compoundAssignmentBaseOperator(expression->tokenKind());
                        if (baseOperator == lexer::TokenKind::Invalid)
                        {
                            error = "unsupported compound assignment operator on a property/indexer";
                            return nullptr;
                        }
                        auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
                        if (current == nullptr) return nullptr;
                        auto operand = lowerExpression(expression->second(), memberOrdinal, false, error);
                        if (operand == nullptr) return nullptr;
                        const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                        const ProjectImplicitConversion conversion = classifyImplicit(
                            operand->typeKey,
                            current->typeKey,
                            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                        const bool shift = baseOperator == lexer::TokenKind::LessLess ||
                            baseOperator == lexer::TokenKind::GreaterGreater;
                        if (!shift && !conversion.exists())
                        {
                            error = "compound assignment to a property/indexer cannot convert its "
                                "right-hand side from '" + operand->typeKey + "' to '" +
                                current->typeKey + "'";
                            return nullptr;
                        }
                        if (!shift && conversion.kind != ProjectConversionKind::Identity)
                        {
                            operand = wrapImplicitConversion(
                                std::move(operand), conversion, expression->second());
                        }
                        auto combined = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, current->typeKey);
                        combined->operation = baseOperator;
                        combined->children.push_back(std::move(current));
                        combined->children.push_back(std::move(operand));
                        auto setterNode = lowerResolvedCall(
                            setterCall, arguments, std::move(combined));
                        if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
                        return setterNode;
                    }
                    auto setterNode = lowerResolvedCall(setterCall, arguments);
                    if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
                    return setterNode;
                }
            }

            // NS304-T08-F02: an extension-property assignment is a synthetic namespace setter call
            // recorded under the assignment node (`set_Name(receiver, value)`). Emit it as a
            // StaticCall whose first argument is the receiver and whose last argument is the RHS.
            const NamespaceFunctionCallBinding* namespaceSetter =
                namespaceFunctionCallBinding(expression);
            if (namespaceSetter != nullptr &&
                namespaceSetter->resolution == MethodCallResolutionKind::Resolved &&
                namespaceSetter->selectedFunctionOrdinal.has_value() &&
                namespaceFunctionModel_ != nullptr &&
                *namespaceSetter->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
            {
                if (addressContext)
                {
                    error = "the result of an extension-property assignment is a value, not addressable storage";
                    return nullptr;
                }
                const std::size_t functionOrdinal = *namespaceSetter->selectedFunctionOrdinal;
                const NamespaceFunctionSymbol& target =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::StaticCall, expression, namespaceSetter->resultTypeKey);
                node->namespaceFunctionOrdinal = functionOrdinal;
                node->stringValue = target.mangledName;
                if (namespaceSetter->forwardedReceiver)
                {
                    if (namespaceSetter->receiverSyntax == nullptr || target.parameters.empty())
                    {
                        error = "extension setter lost its synthetic receiver contract";
                        return nullptr;
                    }
                    auto receiver = lowerExpression(
                        namespaceSetter->receiverSyntax, memberOrdinal, false, error);
                    if (receiver == nullptr) return nullptr;
                    if (!namespaceSetter->selectedConversions.empty() &&
                        namespaceSetter->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        receiver = wrapImplicitConversion(
                            std::move(receiver), namespaceSetter->selectedConversions.front(),
                            namespaceSetter->receiverSyntax);
                    }
                    node->children.push_back(std::move(receiver));
                    node->argumentParameterOrdinals.push_back(0);
                    node->argumentTypeKeys.push_back(target.parameters[0].typeKey);
                }
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                const std::size_t valueParameterOrdinal = namespaceSetter->forwardedReceiver ? 1 : 0;
                if (valueParameterOrdinal >= target.parameters.size())
                {
                    error = "extension setter has no value parameter";
                    return nullptr;
                }
                if (valueParameterOrdinal < namespaceSetter->selectedConversions.size() &&
                    namespaceSetter->selectedConversions[valueParameterOrdinal].kind !=
                        ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(
                        std::move(value),
                        namespaceSetter->selectedConversions[valueParameterOrdinal],
                        expression->second());
                }
                node->children.push_back(std::move(value));
                node->argumentParameterOrdinals.push_back(valueParameterOrdinal);
                node->argumentTypeKeys.push_back(target.parameters[valueParameterOrdinal].typeKey);
                node->returnsAssignedValue = true;
                return node;
            }

            // NS304-T10-F05/F06: the receiver remains the first child so LLVM can branch before
            // lowering the RHS. booleanValue carries the semantic owning-destination contract.
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
            {
                const MethodNameBinding* binding = nameBinding(expression->first());
                if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
                    binding->kind != MethodNameBindingKind::ExplicitObjectField)
                {
                    error = "T10-F05 null-conditional assignment has no unique instance-field binding";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalFieldStore, expression, resultType);
                node->operation = expression->tokenKind();
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
                node->memberOrdinal = binding->candidateMemberOrdinals.front();
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(value));
                return node;
            }

            // NS304-T10-F05: receiver, index, and value remain separate children so only the
            // receiver is evaluated before the null branch.
            if (expression->tokenKind() == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
            {
                const MethodCoreBinding* core = coreBinding(expression->first());
                if (core == nullptr ||
                    core->kind != MethodCoreOperationKind::NullConditionalArrayElement)
                {
                    error = "T10-F05 null-conditional index store has no native-array element binding";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                const ast::AstNode* indexSyntax = !expression->first()->children().empty()
                    ? expression->first()->children().front() : expression->first()->second();
                auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                if (index == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalArrayStore, expression, resultType);
                node->elementTypeKey = core->elementTypeKey;
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(index));
                node->children.push_back(std::move(value));
                return node;
            }

            auto node = makeNode(ProjectOopHirNodeKind::Assignment, expression, resultType);
            node->operation = expression->tokenKind();
            if (node->operation == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const MethodNameBinding* targetBinding = nameBinding(expression->first());
                if (targetBinding != nullptr && targetBinding->localOrdinal.has_value() &&
                    *targetBinding->localOrdinal < methodBodyModel_->locals().size() &&
                    (methodBodyModel_->locals()[*targetBinding->localOrdinal].isRaii ||
                     methodBodyModel_->locals()[*targetBinding->localOrdinal].requiresValueDrop))
                {
                    node->booleanValue = true;
                    node->localOrdinal = *targetBinding->localOrdinal;
                }
            }
            // Owning replacement is a semantic property of the destination, not an AST pattern.
            // Fields and elements of owned arrays both lower to an address-based store; the
            // semantic binding records which stores own their current value so this layer can
            // request the existing release-before-store path without rediscovering declarations.
            if (!node->booleanValue)
            {
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
            }
            // T03-F01/T06-F08: a move-only aggregate owns its current active payload even when
            // the replacement is a freshly returned value rather than an explicit move(local).
            // Mark every ordinary store of a drop-bearing value so the backend destroys the
            // displaced value after evaluating the RHS and before writing the replacement.
            if (!node->booleanValue && node->operation == lexer::TokenKind::Equal &&
                memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(resultType))
            {
                node->booleanValue = true;
            }
            if (node->operation != lexer::TokenKind::Equal)
            {
                // ID 81. Los diez operadores compuestos. El destino se baja UNA VEZ en contexto de
                // direccion -- si se desazucarara a `x = x + e` se evaluaria dos veces y
                // `a[f()] += 1` llamaria a `f` dos veces.
                if (addressContext)
                {
                    error = "the result of a compound assignment is a value, not addressable storage";
                    return nullptr;
                }
                // T41: `raii string += value` is an ownership-aware replacement, not a numeric
                // CompoundStore. The RAII destination is bound specially by semantic analysis,
                // so its identifier does not need a separate expression-type record here; the
                // assignment result type is the authoritative resolved type.
                if (node->operation == lexer::TokenKind::PlusEqual &&
                    resultType == "builtin:string")
                {
                    const MethodNameBinding* targetBinding = nameBinding(expression->first());
                    if (targetBinding == nullptr || !targetBinding->localOrdinal.has_value())
                    {
                        error = "string += requires an identifier-backed RAII string destination";
                        return nullptr;
                    }
                    auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                    auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
                    auto suffix = lowerExpression(expression->second(), memberOrdinal, false, error);
                    if (address == nullptr || current == nullptr || suffix == nullptr) return nullptr;

                    auto concatenated = makeNode(
                        ProjectOopHirNodeKind::StringConcat, expression, "builtin:string");
                    concatenated->operation = lexer::TokenKind::Plus;
                    concatenated->children.push_back(std::move(current));
                    concatenated->children.push_back(std::move(suffix));

                    node->operation = lexer::TokenKind::Equal;
                    node->booleanValue = true;
                    node->localOrdinal = *targetBinding->localOrdinal;
                    node->children.push_back(std::move(address));
                    node->children.push_back(std::move(concatenated));
                    return node;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "Stage 34 compound assignment target has no resolved semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;

                auto compound = makeNode(
                    ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
                compound->operation = lexer::compoundAssignmentBaseOperator(node->operation);
                if (compound->operation == lexer::TokenKind::Invalid)
                {
                    error = "unsupported compound assignment operator";
                    return nullptr;
                }
                // El valor de `x += e` es el NUEVO, como en C#.
                compound->booleanValue = false;

                if (ProjectTypeConversion::isPointer(targetType->typeKey))
                {
                    compound->elementTypeKey =
                        targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
                }
                else
                {
                    // La operacion ocurre ENTERA en el tipo del destino. El modelo semantico ya
                    // exigio que el operando derecho convierta a ese tipo (ver analyzeAssignment),
                    // asi que aqui no hay ni un estrechamiento silencioso ni una segunda tabla de
                    // promociones distinta de la de `Binary`.
                    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey,
                        targetType->typeKey,
                        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                    const bool shift = compound->operation == lexer::TokenKind::LessLess ||
                        compound->operation == lexer::TokenKind::GreaterGreater;
                    if (!shift && !conversion.exists())
                    {
                        error = "compound assignment cannot convert its right-hand side from '" +
                            value->typeKey + "' to '" + targetType->typeKey + "'";
                        return nullptr;
                    }
                    if (!shift && conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(
                            std::move(value), conversion, expression->second());
                    }
                }
                compound->children.push_back(std::move(address));
                compound->children.push_back(std::move(value));
                return compound;
            }
            auto target = lowerExpression(expression->first(), memberOrdinal, true, error);
            auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (target == nullptr || value == nullptr)
            {
                return nullptr;
            }
            const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                value->typeKey,
                target->typeKey,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
            {
                value = wrapImplicitConversion(std::move(value), conversion, expression->second());
            }
            node->children.push_back(std::move(target));
            node->children.push_back(std::move(value));
            return node;
        }

        case ast::AstNodeKind::MoveExpression:
        {
            if (addressContext)
            {
                error = "Stage 49 move value is not addressable";
                return nullptr;
            }
            const MethodMoveBinding* move = moveBinding(expression);
            if (move == nullptr)
            {
                error = "Stage 49 move expression has no semantic move binding";
                return nullptr;
            }
            if (move->sourceIsArrayElement)
            {
                // Stage 52: move(items[i]) extracts an owned element out of native-array
                // backing storage (needed by Queue<T>.Dequeue()/Stack<T>.Pop()). Reuse the
                // existing IndexExpression address-lowering path (ArrayElementAddress) to
                // compute the slot's address, then wrap it in a dedicated node so LLVM
                // lowering can load the value and, only for reference-like elements,
                // null out the slot to prevent a later double-free/double-use. Value-typed
                // elements (int, bool, a plain struct, ...) have no ownership to transfer,
                // so the load degrades to a harmless plain read with no null-store.
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveArrayElement, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(address));
                return node;
            }
            if (move->sourceIsOwnedField)
            {
                // Bloque 1: move(holder.slot) / move(slot) -- take the owner OUT of an `owned`
                // field. Same address-shaped contract as the array-element case above: compute
                // the slot's address here, load-and-null in the backend. The null-store is what
                // makes the container's own discharge (destruction entry, overwrite release) see
                // an empty slot instead of double-freeing the value that just left.
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveOwnedField, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(address));
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::MoveValue, expression, move->typeKey);
            node->localOrdinal = move->sourceLocalOrdinal;
            node->parameterOrdinal = move->sourceParameterOrdinal;
            // Stage 52: whether the source is actually reference-like (needs the load+null-store
            // treatment) or a plain value type closed over a generic T -- see
            // MethodMoveBinding::clearSourceStorage's comment and the MoveValue LLVM
            // lowering case for the bug this fixes.
            node->booleanValue = move->clearSourceStorage;
            if (node->localOrdinal.has_value() == node->parameterOrdinal.has_value())
            {
                error = "Stage 50.8 move value must identify exactly one owner source";
                return nullptr;
            }
            return node;
        }

        case ast::AstNodeKind::ResultPropagationExpression:
        {
            if (addressContext)
            {
                error = "postfix '?' produces a value, not addressable storage";
                return nullptr;
            }
            const MethodResultPropagationBinding* binding =
                resultPropagationBinding(expression);
            const MethodStructuredExitBinding* exit = structuredExitBinding(expression);
            if (binding == nullptr || exit == nullptr ||
                exit->kind != MethodStructuredExitKind::Return)
            {
                error = "T07-F01 Result propagation has no complete semantic binding";
                return nullptr;
            }
            auto operand = lowerExpression(
                binding->operandSyntax, memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;

            auto node = makeNode(
                ProjectOopHirNodeKind::ResultPropagation, expression, resultType);
            node->resultSourceCarrierTypeKey = binding->sourceCarrierTypeKey;
            node->resultSourceErrorTypeKey = binding->sourceErrorTypeKey;
            node->resultReturnCarrierTypeKey = binding->returnCarrierTypeKey;
            node->resultReturnErrorTypeKey = binding->returnErrorTypeKey;
            node->resultSuccessTag = binding->sourceSuccessTag;
            node->resultErrorTag = binding->sourceErrorTag;
            node->resultReturnErrorTag = binding->returnErrorTag;
            node->resultSuccessTestMemberOrdinal =
                binding->sourceSuccessTestMemberOrdinal;
            node->resultSuccessExtractorMemberOrdinal =
                binding->sourceSuccessExtractorMemberOrdinal;
            node->resultErrorExtractorMemberOrdinal =
                binding->sourceErrorExtractorMemberOrdinal;
            node->resultErrorFactoryMemberOrdinal =
                binding->returnErrorFactoryMemberOrdinal;
            node->resultErrorConversionMemberOrdinal =
                binding->errorConversionMemberOrdinal;
            node->cleanupRegionOrdinals = exit->cleanupRegionOrdinals;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::CallExpression:
        {
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
                 iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNextAsync))
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

        // CC1 fase 4B -- `stackalloc T[n]`. Un alloca de arreglo y nada mas: sin cabecera de
        // longitud, sin comprobacion de rango, sin liberacion. Es el nivel crudo, igual que
        // `p[i]`, y la validacion de que el elemento puede vivir ahi ya la hizo el modelo
        // semantico (sin destructor y sin campos `owned`).
        case ast::AstNodeKind::StackallocExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            if (core == nullptr || core->kind != MethodCoreOperationKind::StackAllocation)
            {
                error = "CC1 stackalloc reached OOP lowering without its resolved element type";
                return nullptr;
            }
            if (addressContext)
            {
                error = "CC1 stackalloc yields a pointer value, not addressable storage";
                return nullptr;
            }
            auto count = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (count == nullptr) return nullptr;

            // =================================================================================
            // ID 123 (3/3). QUE LA RESERVA QUEPA.
            //
            // Esta comprobacion vive AQUI y no en el modelo semantico porque necesita el TAMANO
            // DEL ELEMENTO, y aquel corre antes que ProjectObjectLayoutModel -- lo dice el
            // comentario de SizeofConstant en ProjectMethodBodyModel.h. El tamano se lo pregunta
            // al MISMO servicio del que salen `sizeof`, el stride de un arreglo y el offset de un
            // campo: ni una tabla de tamanos nueva.
            //
            // SOLO CUANDO EL CONTADOR ES CONSTANTE. Con un contador en tiempo de ejecucion no hay
            // nada que decidir en compilacion, y este lenguaje no promete comprobar la pila.
            //
            // NO SE MULTIPLICA NUNCA, y esa es la parte que importa. La forma obvia
            //
            //     if (elements * elementSize > kLimit)
            //
            // desborda en 64 bits justo en los casos que quiere atrapar: `stackalloc int[1<<62]`
            // da un producto que envuelve a 0 y pasa la comprobacion. Dividiendo el limite entre
            // el tamano, la comparacion es exacta para todo `elements` de 64 bits y el
            // desbordamiento del producto deja de existir por construccion, en vez de vigilarse
            // aparte. Por eso las dos ultimas comprobaciones del ID 123 son UNA sola.
            //
            // EL LIMITE, y de donde sale. Nada en el arbol le pasa `/STACK` a `lld-link`
            // (comprobado: cero apariciones en src/Linker/), asi que el PE se queda con la
            // reserva por omision del enlazador, que en Windows x64 es 1 MiB. Una sola reserva de
            // 1 MiB no puede caber ahi -- el marco de la funcion tambien ocupa -- asi que el
            // limite rechaza SOLO lo que falla seguro y no opina sobre lo que quiza quepa. Si
            // algun dia el enlazador recibe un `/STACK` mayor, este numero se mueve con el.
            constexpr std::uint64_t kMaximumStackallocBytes = std::uint64_t{1} << 20;

            if (count->kind == ProjectOopHirNodeKind::IntegerConstant)
            {
                if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                {
                    error = "CC1 stackalloc requires the object layout model to size it";
                    return nullptr;
                }
                const std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout =
                    objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, core->elementTypeKey);
                if (!elementLayout.has_value())
                {
                    error = "stackalloc cannot size '" + core->elementTypeKey +
                        "': it has no value layout";
                    return nullptr;
                }
                const std::uint64_t elementSize = elementLayout->first;
                const std::uint64_t elements = count->integerValue;
                if (elementSize != 0 && elements > kMaximumStackallocBytes / elementSize)
                {
                    error = "stackalloc of " + std::to_string(elements) + " x " +
                        std::to_string(elementSize) + " bytes does not fit on the stack; the "
                        "default reserve is " + std::to_string(kMaximumStackallocBytes) +
                        " bytes -- allocate it on the heap instead";
                    return nullptr;
                }
            }

            auto node = makeNode(ProjectOopHirNodeKind::StackAllocation, expression, core->resultTypeKey);
            node->elementTypeKey = core->elementTypeKey;
            node->children.push_back(std::move(count));
            return node;
        }

        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::CollectionExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            // CC1 fase 4C -- `new T` escalar. Hasta esta ronda el mensaje de abajo decia que el
            // raw new "remains outside this pipeline", y era cierto: el modelo semantico lo
            // aceptaba y lo tipaba `T*`, y aqui moria. Ahora trae binding y se baja.
            if (core != nullptr && core->kind == MethodCoreOperationKind::RawAllocation)
            {
                if (addressContext)
                {
                    error = "CC1 raw allocation yields a pointer value, not addressable storage";
                    return nullptr;
                }
                auto node = makeNode(ProjectOopHirNodeKind::RawAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                return node;
            }
            if (core != nullptr && core->kind == MethodCoreOperationKind::CollectionBuilder)
            {
                if (addressContext)
                {
                    error = "collection builder result is a value, not addressable storage";
                    return nullptr;
                }
                const std::string tempTypeKey = core->resultTypeKey;
                const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;

                auto block = makeNode(
                    ProjectOopHirNodeKind::BlockExpression, expression, core->resultTypeKey);

                // CreateBuilder() static call initializes the compiler-generated temporary.
                auto create = makeNode(ProjectOopHirNodeKind::StaticCall, expression, tempTypeKey);
                create->memberOrdinal = core->builderMemberOrdinals[0];
                auto declaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration, expression, tempTypeKey);
                declaration->localOrdinal = tempOrdinal;
                declaration->stringValue = "builder";
                declaration->children.push_back(std::move(create));
                block->children.push_back(std::move(declaration));

                // Add(element) instance calls over the temporary, preserving source order.
                for (const ast::AstNode* child : expression->children())
                {
                    auto element = lowerExpression(child, memberOrdinal, false, error);
                    if (element == nullptr) return nullptr;
                    auto add = makeNode(ProjectOopHirNodeKind::InstanceCall, child, "builtin:void");
                    add->memberOrdinal = core->builderMemberOrdinals[1];
                    auto addReceiver = makeNode(
                        ProjectOopHirNodeKind::LocalAddress, expression, tempTypeKey + "*");
                    addReceiver->localOrdinal = tempOrdinal;
                    add->children.push_back(std::move(addReceiver));
                    add->children.push_back(std::move(element));
                    auto statement = makeNode(
                        ProjectOopHirNodeKind::ExpressionStatement, child, "builtin:void");
                    statement->children.push_back(std::move(add));
                    block->children.push_back(std::move(statement));
                }

                // ToCollection() instance call yields the result value.
                auto toCollection = makeNode(
                    ProjectOopHirNodeKind::InstanceCall, expression, core->resultTypeKey);
                toCollection->memberOrdinal = core->builderMemberOrdinals[2];
                auto toCollectionReceiver = makeNode(
                    ProjectOopHirNodeKind::LocalAddress, expression, tempTypeKey + "*");
                toCollectionReceiver->localOrdinal = tempOrdinal;
                toCollection->children.push_back(std::move(toCollectionReceiver));
                block->children.push_back(std::move(toCollection));

                return block;
            }
            if (core == nullptr ||
                (core->kind != MethodCoreOperationKind::ArrayAllocation &&
                 core->kind != MethodCoreOperationKind::RangeArrayAllocation))
            {
                error = "Stage 41 OOP HIR only owns dynamic new T[count]; raw new remains outside this pipeline";
                return nullptr;
            }
            if (addressContext)
            {
                error = "Stage 41 array allocation result is a reference value, not addressable storage";
                return nullptr;
            }
            if (core->kind == MethodCoreOperationKind::RangeArrayAllocation)
            {
                auto node = makeNode(
                    ProjectOopHirNodeKind::RangeArrayAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->nativeArrayRank = ProjectTypeConversion::nativeArrayRank(core->resultTypeKey);
                std::vector<const ast::AstNode*> dimensions;
                dimensions.push_back(expression->first());
                dimensions.insert(dimensions.end(), expression->children().begin(),
                    expression->children().end());
                for (const ast::AstNode* dimension : dimensions)
                {
                    auto extent = lowerExpression(dimension, memberOrdinal, false, error);
                    if (extent == nullptr) return nullptr;
                    node->children.push_back(std::move(extent));
                }
                return node;
            }
            // NS304-T09-F05: preserve every materialized segment in source order. A parallel
            // kind vector makes spread position explicit through HIR and MIR.
            bool spreadSeen = false;
            for (const ast::AstNode* child : expression->children())
            {
                if (child != nullptr && child->kind() == ast::AstNodeKind::SpreadElementExpression)
                {
                    spreadSeen = true;
                    break;
                }
            }
            if (spreadSeen)
            {
                auto node = makeNode(
                    ProjectOopHirNodeKind::ArrayAllocation, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                for (const ast::AstNode* initializer : expression->children())
                {
                    if (initializer != nullptr &&
                        initializer->kind() == ast::AstNodeKind::SpreadElementExpression)
                    {
                        auto source = lowerExpression(
                            initializer->first(), memberOrdinal, false, error);
                        if (source == nullptr) return nullptr;
                        node->children.push_back(std::move(source));
                        node->collectionSegments.push_back(ProjectCollectionSegmentKind::Spread);
                        continue;
                    }
                    auto value = lowerExpression(initializer, memberOrdinal, false, error);
                    if (value == nullptr) return nullptr;
                    const MethodExpressionTypeBinding* valueType = expressionType(initializer);
                    if (valueType == nullptr)
                    {
                        error = "NSC-LIMIT-017 array spread element has no semantic type";
                        return nullptr;
                    }
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        valueType->typeKey, core->elementTypeKey, valueType->integerConstant);
                    if (!conversion.exists())
                    {
                        error = "NSC-LIMIT-017 array spread element lost its validated conversion";
                        return nullptr;
                    }
                    if (conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(std::move(value), conversion, initializer);
                    }
                    node->children.push_back(std::move(value));
                    node->collectionSegments.push_back(ProjectCollectionSegmentKind::Element);
                }
                return node;
            }
            const bool inferredInitializer =
                expression->auxiliaryText() == "inferred-array-initializer";
            std::unique_ptr<ProjectOopHirNode> count;
            if (inferredInitializer)
            {
                count = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:int");
                count->integerValue = expression->children().size();
            }
            else
            {
                count = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (count == nullptr) return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::ArrayAllocation, expression, core->resultTypeKey);
            node->elementTypeKey = core->elementTypeKey;
            node->booleanValue = inferredInitializer;
            node->children.push_back(std::move(count));
            for (const ast::AstNode* initializer : expression->children())
            {
                auto value = lowerExpression(initializer, memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                const MethodExpressionTypeBinding* valueType = expressionType(initializer);
                if (valueType == nullptr)
                {
                    error = "NSC-LIMIT-017 array initializer element has no semantic type";
                    return nullptr;
                }
                const ProjectImplicitConversion conversion = classifyImplicit(
                    valueType->typeKey, core->elementTypeKey, valueType->integerConstant);
                if (!conversion.exists())
                {
                    error = "NSC-LIMIT-017 array initializer lost its validated conversion";
                    return nullptr;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(std::move(value), conversion, initializer);
                }
                node->children.push_back(std::move(value));
            }
            return node;
        }

        case ast::AstNodeKind::ObjectCreationExpression:
        case ast::AstNodeKind::PlacementConstructionExpression:
        {
            const bool placement =
                expression->kind() == ast::AstNodeKind::PlacementConstructionExpression;
            const ObjectConstructionBinding* binding = constructionBinding(expression);
            if (binding == nullptr ||
                (binding->resolution != ConstructionResolutionKind::ResolvedExplicit &&
                 binding->resolution != ConstructionResolutionKind::ResolvedImplicitDefault))
            {
                error = "Stage 34 requires a resolved Stage 28 object construction";
                return nullptr;
            }
            const auto ordinal = static_cast<std::size_t>(
                binding - constructionModel_->constructions().data());
            auto node = makeNode(
                placement
                    ? ProjectOopHirNodeKind::PlacementConstruction
                    : binding->targetIsStruct
                    ? ProjectOopHirNodeKind::ValueConstruction
                    : ProjectOopHirNodeKind::ObjectConstruction,
                expression,
                placement && binding->targetIsStruct
                    ? binding->targetTypeKey + "*"
                    : binding->targetTypeKey);
            node->constructionOrdinal = ordinal;
            if (placement)
            {
                node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
                node->storageOwnership = sema::OwnershipKind::Borrowed;
            }
            else if (!binding->targetIsStruct)
            {
                node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
                node->storageOwnership = sema::OwnershipKind::Owned;
            }
            node->targetTypeOrdinal = binding->targetTypeOrdinal;
            node->memberOrdinal = binding->selectedConstructorMemberOrdinal.has_value()
                ? binding->selectedConstructorMemberOrdinal
                : binding->implicitDefaultBaseConstructorMemberOrdinal;
            node->argumentCount = 0;
            bool hasOwnedTemporary = false;
            ProjectOopHirNode* expandedPack = nullptr;
            if (placement)
            {
                auto storage = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (storage == nullptr) return nullptr;
                node->children.push_back(std::move(storage));
            }
            for (std::size_t index = 0; index < expression->children().size(); ++index)
            {
                auto argument = lowerExpression(expression->children()[index], memberOrdinal, false, error);
                if (argument == nullptr)
                {
                    return nullptr;
                }
                if (index < binding->selectedConversions.size() &&
                    binding->selectedConversions[index].kind != ProjectConversionKind::Identity)
                {
                    argument = wrapImplicitConversion(
                        std::move(argument), binding->selectedConversions[index], expression->children()[index]);
                }
                if (index >= binding->selectedArgumentParameterOrdinals.size())
                {
                    error = "named construction argument has no selected parameter ordinal";
                    return nullptr;
                }
                const std::size_t parameterIndex =
                    binding->selectedArgumentParameterOrdinals[index];
                const bool packElement =
                    index < binding->selectedParameterPackElements.size() &&
                    binding->selectedParameterPackElements[index];
                if (packElement)
                {
                    if (!binding->selectedConstructorMemberOrdinal.has_value() ||
                        *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
                    {
                        error = "T01-F02 construction parameter pack has no constructor metadata";
                        return nullptr;
                    }
                    const ClassMemberSymbol& constructor =
                        memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
                    if (parameterIndex >= constructor.parameters.size())
                    {
                        error = "T01-F02 construction parameter pack has no parameter";
                        return nullptr;
                    }
                    const std::string& elementType =
                        constructor.parameters[parameterIndex].parameterPackElementTypeKey;
                    if (elementType.empty())
                    {
                        error = "T01-F02 construction parameter pack lost its element type";
                        return nullptr;
                    }
                    if (expandedPack == nullptr)
                    {
                        auto pack = makeNode(
                            ProjectOopHirNodeKind::ParameterPack,
                            expression->children()[index],
                            constructor.parameters[parameterIndex].typeKey);
                        pack->elementTypeKey = elementType;
                        expandedPack = pack.get();
                        node->children.push_back(std::move(pack));
                        node->argumentParameterOrdinals.push_back(parameterIndex);
                    }
                    expandedPack->children.push_back(std::move(argument));
                    continue;
                }
                if (std::ranges::find(
                        binding->selectedFullExpressionBorrowArgumentOrdinals,
                        index) != binding->selectedFullExpressionBorrowArgumentOrdinals.end())
                {
                    argument = wrapFreshOwnedTemporaryBorrow(
                        std::move(argument), expression->children()[index], hasOwnedTemporary);
                }
                node->children.push_back(std::move(argument));
                node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            if (!binding->selectedDefaultArguments.empty())
            {
                if (!binding->selectedConstructorMemberOrdinal.has_value() ||
                    *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
                {
                    error = "NSC-LIMIT-020 selected construction has no callable metadata";
                    return nullptr;
                }
                const ClassMemberSymbol& selectedConstructor =
                    memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
                for (std::size_t index = 0; index < binding->selectedDefaultArguments.size(); ++index)
                {
                    if (index >= binding->selectedDefaultArgumentParameterOrdinals.size())
                    {
                        error = "selected construction default has no parameter ordinal";
                        return nullptr;
                    }
                    const std::size_t parameterIndex =
                        binding->selectedDefaultArgumentParameterOrdinals[index];
                    if (parameterIndex >= selectedConstructor.parameters.size())
                    {
                        error = "NSC-LIMIT-020 construction default has no parameter";
                        return nullptr;
                    }
                    auto argument = lowerDefaultArgument(
                        binding->selectedDefaultArguments[index],
                        selectedConstructor.parameters[parameterIndex].typeKey,
                        error);
                    if (argument == nullptr) return nullptr;
                    node->children.push_back(std::move(argument));
                    node->argumentParameterOrdinals.push_back(parameterIndex);
                }
            }
            if (binding->selectedSynthesizesEmptyParameterPack)
            {
                if (!binding->selectedConstructorMemberOrdinal.has_value() ||
                    *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size() ||
                    !binding->selectedParameterPackOrdinal.has_value())
                {
                    error = "T01-F02 empty construction parameter pack has no constructor metadata";
                    return nullptr;
                }
                const ClassMemberSymbol& constructor =
                    memberModel_->members()[*binding->selectedConstructorMemberOrdinal];
                const std::size_t parameterIndex = *binding->selectedParameterPackOrdinal;
                if (parameterIndex >= constructor.parameters.size())
                {
                    error = "T01-F02 empty construction parameter pack has no parameter";
                    return nullptr;
                }
                const std::string& elementType =
                    constructor.parameters[parameterIndex].parameterPackElementTypeKey;
                if (elementType.empty())
                {
                    error = "T01-F02 empty construction parameter pack lost its element type";
                    return nullptr;
                }
                auto pack = makeNode(
                    ProjectOopHirNodeKind::ParameterPack,
                    expression,
                    constructor.parameters[parameterIndex].typeKey);
                pack->elementTypeKey = elementType;
                node->children.push_back(std::move(pack));
                node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            node->argumentCount = node->argumentParameterOrdinals.size();
            if (binding->selectedImplicitDefault)
            {
                for (const std::size_t typeOrdinal : implicitInitializerTypes(
                         binding->targetTypeOrdinal,
                         binding->implicitDefaultBaseConstructorMemberOrdinal))
                {
                    std::vector<std::unique_ptr<ProjectOopHirNode>> initializers;
                    appendInstanceFieldInitializers(
                        typeOrdinal, initializers, error, memberOrdinal);
                    if (!error.empty())
                    {
                        return nullptr;
                    }
                    for (auto& initializer : initializers)
                    {
                        node->children.push_back(std::move(initializer));
                    }
                }
            }
            return wrapFullExpressionCleanup(std::move(node), expression, hasOwnedTemporary);
        }

        case ast::AstNodeKind::TupleExpression:
        case ast::AstNodeKind::AggregateInitializerExpression:
        case ast::AstNodeKind::WithExpression:
        {
            const MethodExpressionTypeBinding* targetType = expressionType(expression);
            if (targetType == nullptr)
            {
                error = "aggregate/with initializer has no bound target type";
                return nullptr;
            }
            auto node = makeNode(
                ProjectOopHirNodeKind::AggregateInitializer, expression, targetType->typeKey);
            if (!appendAggregateElements(expression, targetType->typeKey, 0, memberOrdinal, node->children, error))
            {
                return nullptr;
            }
            return node;
        }

        case ast::AstNodeKind::DeleteExpression:
        {
            const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
            if (operandType == nullptr)
            {
                error = "Stage 41 cannot determine delete operand type";
                return nullptr;
            }
            if (expression->flag())
            {
                if (!ProjectTypeConversion::isNativeArray(operandType->typeKey))
                {
                    error = "Stage 41 delete[] requires a native dynamic array reference";
                    return nullptr;
                }
                const MethodCoreBinding* elementBinding = coreBinding(expression->first());
                const bool directElement = elementBinding != nullptr &&
                    (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
                     elementBinding->kind == MethodCoreOperationKind::RawElement);
                auto operand = lowerExpression(
                    expression->first(), memberOrdinal, directElement, error);
                if (operand == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
                node->dynamicDestruction = false;
                // The array allocation is always released by delete[]. Element ownership is a
                // separate axis carried by the operand binding. In particular, a plain T[] or
                // borrowed T[] owns no T elements even when its containing class manually
                // releases the backing buffer in its destructor.
                node->storageOwnership = operandType->storageOwnership;
                // A jagged row is itself an owning native-array reference. Carrying the slot
                // address lets LLVM null it before recursive destruction, so an explicit row
                // delete followed by deletion of the outer array can never double-drop it.
                node->booleanValue = directElement;
                node->elementTypeKey = operandType->typeKey;
                node->children.push_back(std::move(operand));
                return node;
            }
            const bool rawPointer = operandType->typeKey.ends_with("*");
            const bool nativeString = operandType->typeKey == "builtin:string";
            const bool deletable = operandType->typeKey == "builtin:null" ||
                isReferenceType(operandType->typeKey) || rawPointer || nativeString;
            // Stage 52 generic collections: a container's cleanup code (e.g. List<T>.Clear())
            // must delete each stored element uniformly, but T may monomorphize to a plain
            // value type (int, bool, a struct, ...) that never owned heap memory in the first
            // place. Rather than forbid `delete` on such values -- which would make it
            // impossible to write ONE generic Clear()/destructor body that works for every T --
            // treat `delete <value-typed expression>;` as a legal no-op: the operand is still
            // evaluated (so e.g. an array-index bounds check still fires), but no runtime
            // deallocation call is emitted. This mirrors how `delete[] arr;` already tolerates
            // arrays of any element type. `delete` on a genuinely non-deletable receiver
            // (unknown/void expression, etc.) is caught earlier by expressionType() returning a
            // real value type here -- there is no way to reach this branch with something that
            // isn't a fully-typed value expression.
            const MethodCoreBinding* elementBinding = coreBinding(expression->first());
            const bool directElement = elementBinding != nullptr &&
                (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
                 elementBinding->kind == MethodCoreOperationKind::RawElement);
            const bool clearsElementSlot = directElement &&
                (isReferenceType(operandType->typeKey) || nativeString);
            auto operand = lowerExpression(
                expression->first(), memberOrdinal, clearsElementSlot, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Deallocation, expression, "builtin:void");
            node->dynamicDestruction = isReferenceType(operandType->typeKey) && !nativeString;
            node->valueTypeNoOpDeletion = !deletable;
            if (!node->valueTypeNoOpDeletion)
            {
                node->objectLifetimeOwnership = node->dynamicDestruction
                    ? sema::OwnershipKind::Owned : sema::OwnershipKind::None;
                node->storageOwnership = sema::OwnershipKind::Owned;
            }
            // Deleting a directly addressable reference element is an ownership operation on the
            // slot, not merely on a copied pointer.  Carry its address and type so LLVM can load
            // and clear it before destruction.  This prevents a later release-before-store from
            // observing a dangling owner and destroying the same allocation twice.
            node->booleanValue = clearsElementSlot;
            node->elementTypeKey = operandType->typeKey;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::DestroyExpression:
        {
            const MethodExpressionTypeBinding* operandType = expressionType(expression->first());
            if (operandType == nullptr)
            {
                error = "item 319 cannot determine destroy operand type";
                return nullptr;
            }
            auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;

            bool dynamicDestruction = isReferenceType(operandType->typeKey);
            if (!dynamicDestruction && operandType->typeKey.ends_with('*'))
            {
                const std::string pointee = operandType->typeKey.substr(
                    0, operandType->typeKey.size() - 1);
                dynamicDestruction = isReferenceType(pointee);
            }

            auto node = makeNode(
                ProjectOopHirNodeKind::ObjectDestruction, expression, "builtin:void");
            node->dynamicDestruction = dynamicDestruction;
            node->elementTypeKey = operandType->typeKey;
            node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
            node->storageOwnership = sema::OwnershipKind::Borrowed;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
        {
            const MethodRuntimeTypeBinding* binding = runtimeTypeBinding(expression);
            if (binding == nullptr)
            {
                error = "Stage 39 runtime type operation lost its semantic binding";
                return nullptr;
            }
            auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            auto node = makeNode(
                binding->kind == MethodRuntimeTypeOperationKind::TypeTest
                    ? ProjectOopHirNodeKind::RuntimeTypeTest
                    : ProjectOopHirNodeKind::SafeReferenceCast,
                expression,
                resultType);
            node->targetTypeOrdinal = binding->targetTypeOrdinal;
            node->targetIsInterface = binding->targetIsInterface;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::CastExpression:
        {
            const MethodRuntimeTypeBinding* runtimeBinding = runtimeTypeBinding(expression);
            if (runtimeBinding != nullptr &&
                runtimeBinding->kind == MethodRuntimeTypeOperationKind::CheckedCast)
            {
                auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (operand == nullptr)
                {
                    return nullptr;
                }
                auto node = makeNode(ProjectOopHirNodeKind::CheckedReferenceCast, expression, resultType);
                node->targetTypeOrdinal = runtimeBinding->targetTypeOrdinal;
                node->targetIsInterface = runtimeBinding->targetIsInterface;
                node->children.push_back(std::move(operand));
                return node;
            }
            const MethodCallBinding* conversionCall = callBinding(expression);
            if (conversionCall != nullptr && conversionCall->resolution == MethodCallResolutionKind::Resolved &&
                conversionCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *conversionCall->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*conversionCall->selectedMemberOrdinal].isUserConversion)
            {
                return lowerResolvedCall(conversionCall, {expression->first()});
            }
            auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Conversion, expression, resultType);
            node->children.push_back(std::move(operand));
            return node;
        }

        // CC1 fase 4A -- `sizeof`. Las dos formas llegan aqui como una CONSTANTE y nada mas: MIR y
        // LLVM no necesitan enterarse de que en el fuente habia un `sizeof`.
        //
        // El operando de `sizeof(expression)` NO se baja. Es lo que hace que un efecto secundario
        // escrito dentro de el no llegue a ejecutarse: no hay nodo que lo represente. El modelo
        // semantico ya se encargo de que tampoco altere el estado de flujo.
        //
        // El tamano se lo pregunta a `valueLayoutForTypeKey`, que es el MISMO servicio del que
        // salen el stride de un arreglo y el offset de un campo. Ni una tabla de tamanos nueva:
        // esa es toda la diferencia entre cerrar el hueco y crear una segunda regla de tamano que
        // pueda divergir de la primera.
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        {
            const MethodCoreBinding* binding = coreBinding(expression);
            if (binding == nullptr ||
                (binding->kind != MethodCoreOperationKind::SizeofConstant &&
                 binding->kind != MethodCoreOperationKind::AlignofConstant))
            {
                error = "layout query reached OOP lowering without its resolved measured type";
                return nullptr;
            }
            if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
            {
                error = "layout query requires the object layout model";
                return nullptr;
            }
            const std::optional<std::pair<std::uint64_t, std::uint64_t>> layout =
                objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, binding->elementTypeKey);
            if (!layout.has_value())
            {
                error = (binding->kind == MethodCoreOperationKind::AlignofConstant ? "alignof" : "sizeof") +
                    std::string(" cannot measure '") + binding->elementTypeKey + "': it has no value layout";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nuint");
            node->integerValue = binding->kind == MethodCoreOperationKind::AlignofConstant
                ? layout->second
                : layout->first;
            return node;
        }

        case ast::AstNodeKind::OffsetofExpression:
        {
            const MethodCoreBinding* binding = coreBinding(expression);
            if (binding == nullptr || binding->kind != MethodCoreOperationKind::OffsetofConstant ||
                !binding->implicitFieldMemberOrdinal.has_value())
            {
                error = "offsetof reached OOP lowering without its resolved field";
                return nullptr;
            }
            if (objectLayoutModel_ == nullptr)
            {
                error = "offsetof requires the object layout model";
                return nullptr;
            }
            const ObjectFieldLayout* field = objectLayoutModel_->fieldLayoutForMember(
                *binding->implicitFieldMemberOrdinal);
            if (field == nullptr)
            {
                error = "offsetof cannot find the canonical layout for field '" +
                    expression->text() + "'";
                return nullptr;
            }
            if (field->bitWidth.has_value())
            {
                error = "offsetof cannot be applied to a bit-field";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nuint");
            node->integerValue = field->offset;
            return node;
        }

        // ID 80, CERRADA. Aqui habia un rechazo con mensaje propio para `p->campo`: era la ultima
        // forma de expresion con la mitad delantera del pipeline presente y la trasera ausente,
        // despues de sizeof, stackalloc, raw new y la aritmetica de punteros.
        //
        // Se cerro donde tocaba, que no era aqui: el parser convierte `p->x` en `(*p).x` (ver
        // Parser::parsePostfixExpression), asi que este nodo ya no se construye y este caso ya no
        // se alcanza. Anadir en su lugar un lowering propio habria significado una SEGUNDA ruta de
        // acceso a miembro que mantener de acuerdo con la de `.` -- que es como se abrio el hueco.
        //
        // NOTA sobre como se hizo el inventario que la encontro: un barrido de las 30 formas de
        // expresion del AST contra los `case` de esta funcion daba TAMBIEN `LambdaExpression`. Es
        // un falso positivo del barrido: las lambdas se bajan por binding ANTES del switch, no por
        // un `case`. Comprobado antes de escribirlo -- el barrido mide lo que mide, no lo que uno
        // quiere que mida.

        case ast::AstNodeKind::AwaitExpression:
        {
            // NS304-T05-F03: `await operand` over a synchronously-completing future. The operand
            // must be a direct call to an async function (returns future:<T>); the poll function
            // runs the body and stores the result, which this node reads back. Suspend/resume for
            // pending futures is the state-machine step.
            const MethodCallBinding* binding = callBinding(expression->first());
            if (binding == nullptr || !binding->selectedMemberOrdinal.has_value() ||
                *binding->selectedMemberOrdinal >= memberModel_->members().size())
            {
                // NS304-T05-F09: await over a future VARIABLE (not a direct call). The future
                // carries its poll (pollFn), so the await resolves it from the operand's value.
                auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (operand == nullptr) return nullptr;
                const auto variableInner = language::futureInnerTypeKey(operand->typeKey);
                if (!variableInner.has_value())
                {
                    error = "await operand is neither an async call nor a future value: " +
                        operand->typeKey;
                    return nullptr;
                }
                auto await = makeNode(
                    ProjectOopHirNodeKind::AwaitFuture, expression, std::string(*variableInner));
                await->elementTypeKey = operand->typeKey;
                // Empty calleeName marks the pollFn path: the MIR/LLVM loads the poll from the
                // future's pollFn field instead of a direct function reference.
                await->stringValue.clear();
                await->integerValue = loweringAsyncBody_ ? 1 : 0;
                await->children.push_back(std::move(operand));
                return await;
            }
            const ClassMemberSymbol& callee = memberModel_->members()[*binding->selectedMemberOrdinal];
            // NS304-T05-F03: reject an await over a producer that can actually suspend
            // (transitively awaits a suspend-capable future), which the caller cannot complete
            // synchronously. A producer that only awaits synchronously-completing futures never
            // suspends and is safe to await.
            std::unordered_set<std::size_t> visitedSuspendMembers;
            const auto canSuspend = [&](const auto& self, std::size_t memberIdx) -> bool
            {
                if (!visitedSuspendMembers.insert(memberIdx).second) return false;
                if (memberIdx >= memberModel_->members().size()) return false;
                const ClassMemberSymbol& awaitedMember = memberModel_->members()[memberIdx];
                if (awaitedMember.syntax == nullptr) return false;
                const auto scan = [&](const auto& scanSelf, const ast::AstNode* node) -> bool
                {
                    if (node == nullptr) return false;
                    if (node->kind() == ast::AstNodeKind::AwaitExpression)
                    {
                        const MethodCallBinding* awaitedBinding = callBinding(node->first());
                        if (awaitedBinding != nullptr &&
                            awaitedBinding->selectedMemberOrdinal.has_value() &&
                            *awaitedBinding->selectedMemberOrdinal < memberModel_->members().size())
                            return self(self, *awaitedBinding->selectedMemberOrdinal);
                        return false;
                    }
                    if (scanSelf(scanSelf, node->first())) return true;
                    if (scanSelf(scanSelf, node->second())) return true;
                    for (const ast::AstNode* child : node->children())
                        if (scanSelf(scanSelf, child)) return true;
                    return false;
                };
                return scan(scan, awaitedMember.syntax->first());
            };
            if (canSuspend(canSuspend, *binding->selectedMemberOrdinal))
            {
                error = "await on an async function that can suspend requires the state machine "
                        "lowering (NS304-T05-F03)";
                return nullptr;
            }
            auto call = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (call == nullptr) return nullptr;
            const auto inner = language::futureInnerTypeKey(call->typeKey);
            if (!inner.has_value())
            {
                error = "await operand does not produce a future surface: " + call->typeKey;
                return nullptr;
            }
            auto await = makeNode(ProjectOopHirNodeKind::AwaitFuture, expression, std::string(*inner));
            await->elementTypeKey = call->typeKey;
            await->stringValue = mangleCallable(
                nameLookup_->namedTypes()[callee.declaringTypeOrdinal], callee) + "$poll";
            // NS304-T05-F03: async context -> resume state index (single await = 1); sync context
            // -> blocking await (integerValue = 0, poll once and read the result).
            await->integerValue = loweringAsyncBody_ ? 1 : 0;
            await->children.push_back(std::move(call));
            // NS304-T05-F07 rethrow: a BLOCKING await inside a try rethrows the future's error slot
            // into the enclosing catch, exactly like a `throw`. The async suspend path is separate.
            if (!loweringAsyncBody_ && !activeHirExceptionHandlers_.empty())
            {
                const std::size_t retainedDepth = activeHirExceptionHandlers_.back().retainedCleanupDepth;
                for (std::size_t index = activeHirCleanupRegions_.size(); index > retainedDepth; --index)
                    await->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
                await->exceptionTargetRegionOrdinal =
                    activeHirExceptionHandlers_.back().hirRegionOrdinal;
            }
            return await;
        }

        default:
            error = "unsupported Stage 34 OOP expression kind: " +
                std::string(ast::astNodeKindName(expression->kind()));
            return nullptr;
    }
}

// NSC-LIMIT-008. Is this name actually a GETTER CALL?
//
// `source.Current` and `table[key]` are written like storage and parse like storage, but if the
// semantic model bound them to a property or indexer accessor they are calls: they RETURN a value,
// and that value has no home. Syntax cannot tell the two apart -- `point.X` is a field on one type
// and a property on another, and the difference is only known after member resolution.
//
// Two places need this answer and they used to disagree. lowerExpression asked it (the
// MemberExpression case, which rejects `addressContext` for a getter) while isNaturallyAddressable
// did not, and answered `true` for every MemberExpression. That disagreement is the whole defect:
// `source.Current.Read()` classified the getter as addressable, so lowerReceiver never entered its
// TemporaryAddress path and the struct method received the address of something that was never
// stored.
const MethodCallBinding* ProjectOopHirModel::accessorGetterBinding(
    const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr || memberModel_ == nullptr) return nullptr;
    switch (expression->kind())
    {
        // The three spellings a getter can wear. `Current` unqualified inside its own class is an
        // IdentifierExpression, `source.Current` a MemberExpression, `table[key]` an
        // IndexExpression -- and all three are calls once the model has resolved them.
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::NullConditionalMemberExpression:
        case ast::AstNodeKind::IndexExpression:
            break;
        default:
            return nullptr;
    }
    const MethodCallBinding* accessorCall = callBinding(expression);
    if (accessorCall == nullptr ||
        accessorCall->resolution != MethodCallResolutionKind::Resolved ||
        !accessorCall->selectedMemberOrdinal.has_value() ||
        *accessorCall->selectedMemberOrdinal >= memberModel_->members().size())
    {
        // No binding at all is the ORDINARY case, not an error: a real field, a real array element
        // and a real fixed-array element never produce one. They keep their own address.
        return nullptr;
    }
    const ClassMemberSymbol& selected = memberModel_->members()[*accessorCall->selectedMemberOrdinal];
    // PROPIEDAD *O* INDEXADOR. `isIndexerAccessor` es una bandera distinta de `isPropertyAccessor`,
    // no un caso suyo, y mirar solo la primera dejaba fuera `obj[i].MetodoDeStruct()`: el
    // IndexExpression entraba por el switch de arriba, resolvia a un getter de indexador, y este
    // filtro lo dejaba pasar como si fuera almacenamiento. Exactamente el mismo defecto que este
    // ID cierra, en la otra mitad de la sintaxis.
    if (!selected.isPropertyAccessor && !selected.isIndexerAccessor) return nullptr;
    if (!selected.isGetter) return nullptr;
    return accessorCall;
}

// C6 "struct members": does this expression denote storage that already exists? An identifier, a
// field of one, an element of one, and a dereference all do -- and a parenthesized form of any of
// them does too. A call, an operator, a conversion or a `new` does not: its result is a value with
// no home. Only the second group needs a temporary made for it.
//
// NSC-LIMIT-008 added the one case where syntax lies: a property or indexer getter is written like
// a field or an element and is a call. See accessorGetterBinding.
bool ProjectOopHirModel::isNaturallyAddressable(const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr) return false;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::IndexExpression:
        {
            // A real field or element is storage. An ordinary accessor returns a value with no
            // home, while a ref-returning accessor deliberately returns existing storage.
            const MethodCallBinding* accessor = accessorGetterBinding(expression);
            if (accessor == nullptr) return true;
            return accessor->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *accessor->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*accessor->selectedMemberOrdinal].isReferenceReturn;
        }
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::BaseExpression:
        case ast::AstNodeKind::MatchValueExpression:
            return true;
        case ast::AstNodeKind::UnaryExpression:
            // `*p` is storage; `-v` is a value.
            return expression->tokenKind() == lexer::TokenKind::Star;
        default:
            return false;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDeconstructionSource(
    const ast::AstNode* bindingSyntax,
    const ast::AstNode* sourceSyntax,
    std::size_t memberOrdinal,
    std::string& error)
{
    const MethodDeconstructionBinding* binding = methodBodyModel_ != nullptr
        ? methodBodyModel_->deconstructionBinding(bindingSyntax) : nullptr;
    if (binding == nullptr)
    {
        error = "T02-F03 deconstruction has no semantic source binding";
        return nullptr;
    }
    if (!binding->selectedMemberOrdinal.has_value())
        return lowerExpression(sourceSyntax, memberOrdinal, false, error);

    auto receiver = lowerReceiver(
        sourceSyntax, binding->sourceTypeKey, memberOrdinal, error);
    if (receiver == nullptr) return nullptr;
    auto call = makeNode(
        ProjectOopHirNodeKind::InstanceCall, sourceSyntax, binding->tupleTypeKey);
    call->memberOrdinal = *binding->selectedMemberOrdinal;
    call->children.push_back(std::move(receiver));
    return call;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerReceiver(
    const ast::AstNode* receiverSyntax,
    std::string_view receiverTypeKey,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (receiverTypeKey.empty())
    {
        error = "Stage 40 receiver has no semantic class/interface/struct type";
        return nullptr;
    }
    if (receiverSyntax == nullptr || receiverSyntax->kind() == ast::AstNodeKind::BaseExpression)
    {
        if (currentLambda_ != nullptr)
        {
            const std::optional<std::size_t> capture = currentLambdaThisCapture();
            if (!capture.has_value())
            {
                error = "Stage 43 implicit instance receiver has no captured this";
                return nullptr;
            }
            auto node = makeNode(
                isStructValueType(receiverTypeKey)
                    ? ProjectOopHirNodeKind::ClosureCaptureAddress
                    : ProjectOopHirNodeKind::ClosureCaptureValue,
                receiverSyntax,
                std::string(receiverTypeKey));
            node->integerValue = *capture;
            return node;
        }
        return makeNode(ProjectOopHirNodeKind::ThisValue, receiverSyntax, std::string(receiverTypeKey));
    }
    // Struct instance methods receive the address of existing value storage.  Unlike a class
    // receiver, lowering a struct receiver as a value here would copy it and mutations of `this`
    // would be lost.  Stage 40 therefore requires an addressable receiver and preserves that
    // address explicitly into MIR/LLVM.
    //
    // C6 "struct members": ... and a struct temporary has no address until one is made for it.
    // Before C6 that did not come up much, because a struct expression was almost always a local
    // or a field. Operators change that completely -- `(a + b).x`, `Cross(u, v).z`,
    // `(-v).LengthSquared`, `new Vector3(1,2,3)[0]` are ordinary math, and every one of them
    // reaches a member of a value that lives nowhere. Measured before this: the operator path had
    // no addressability check at all and produced IR the verifier rejected ("GEP base pointer is
    // not a vector", GEPing into a loaded `[12 x i8]`), while the call path reported "temporary
    // struct values are not addressable".
    //
    // So a temporary receiver is MATERIALIZED: given a home, and the member reached through it.
    // Anything that is naturally addressable keeps its own address, unchanged -- this adds a slot
    // only where there was none to begin with.
    if (isStructValueType(receiverTypeKey) && !isNaturallyAddressable(receiverSyntax))
    {
        auto value = lowerExpression(receiverSyntax, memberOrdinal, false, error);
        if (value == nullptr) return nullptr;
        auto node = makeNode(
            ProjectOopHirNodeKind::TemporaryAddress, receiverSyntax, std::string(receiverTypeKey));
        node->elementTypeKey = std::string(receiverTypeKey);
        node->children.push_back(std::move(value));
        return node;
    }
    return lowerExpression(receiverSyntax, memberOrdinal, isStructValueType(receiverTypeKey), error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapImplicitConversion(
    std::unique_ptr<ProjectOopHirNode> value,
    const ProjectImplicitConversion& conversion,
    const ast::AstNode* syntax)
{
    // `never -> T` is a semantic coercion only. There is no source value to convert and no
    // destination value to materialize, so HIR preserves the divergent node itself. Parents use
    // their declared result type for reachable paths and LLVM observes the terminating block.
    if (conversion.kind == ProjectConversionKind::Never)
    {
        return value;
    }
    if ((conversion.kind == ProjectConversionKind::UserDefinedImplicit ||
         conversion.kind == ProjectConversionKind::UserDefinedExplicit) &&
        conversion.userDefinedMemberOrdinal.has_value())
    {
        auto node = makeNode(ProjectOopHirNodeKind::StaticCall, syntax, conversion.targetTypeKey);
        node->memberOrdinal = conversion.userDefinedMemberOrdinal;
        node->children.push_back(std::move(value));
        return node;
    }
    // NS304-T11-F02: a native array view (`Span<T>`/`ReadOnlySpan<T>`) reuses the SpanFromArray
    // core lowering (base+length, no temporary array). Two children signal the WHOLE array: the
    // lowering derives count from the object's own length header.
    if (conversion.kind == ProjectConversionKind::View &&
        ProjectTypeConversion::isNativeArray(conversion.sourceTypeKey))
    {
        if (const std::optional<std::string> element =
                ProjectTypeConversion::nativeArrayElementType(conversion.sourceTypeKey))
        {
            const auto terminal = [](std::string_view key) -> std::string_view
            {
                const std::size_t separator = key.find_last_of(".:+");
                return separator == std::string_view::npos ? key : key.substr(separator + 1U);
            };
            const bool readOnly =
                terminal(conversion.targetTypeKey).starts_with("ReadOnlySpan__g1__");
            auto node = makeNode(
                readOnly ? ProjectOopHirNodeKind::ReadOnlySpanFromArray
                         : ProjectOopHirNodeKind::SpanFromArray,
                syntax, conversion.targetTypeKey);
            node->elementTypeKey = *element;
            node->children.push_back(std::move(value));
            auto zero = makeNode(ProjectOopHirNodeKind::IntegerConstant, syntax, "builtin:int");
            zero->integerValue = 0;
            node->children.push_back(std::move(zero));
            return node;
        }
    }
    // NS304-T11-F02: a native string view (`StringView`) reuses the StringViewFromString core
    // lowering (UTF-8 base+length). Two children signal the WHOLE string.
    if (conversion.kind == ProjectConversionKind::View &&
        conversion.sourceTypeKey == "builtin:string")
    {
        auto node = makeNode(
            ProjectOopHirNodeKind::StringViewFromString, syntax, conversion.targetTypeKey);
        node->elementTypeKey = "builtin:string";
        node->children.push_back(std::move(value));
        auto zero = makeNode(ProjectOopHirNodeKind::IntegerConstant, syntax, "builtin:int");
        zero->integerValue = 0;
        node->children.push_back(std::move(zero));
        return node;
    }
    auto node = makeNode(ProjectOopHirNodeKind::Conversion, syntax, conversion.targetTypeKey);
    node->conversion = conversion;
    node->children.push_back(std::move(value));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapCallArgumentBorrow(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* argumentSyntax,
    std::size_t targetMemberOrdinal,
    std::size_t targetParameterOrdinal,
    bool& hasOwnedTemporary)
{
    const MethodBorrowBinding* borrow = borrowBinding(argumentSyntax);
    if (borrow == nullptr || borrow->targetMemberOrdinal != targetMemberOrdinal ||
        borrow->targetParameterOrdinal != targetParameterOrdinal)
    {
        return value;
    }

    if (borrow->kind == MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter)
    {
        return wrapFreshOwnedTemporaryBorrow(
            std::move(value), argumentSyntax, hasOwnedTemporary);
    }

    auto borrowed = makeNode(ProjectOopHirNodeKind::BorrowValue, argumentSyntax, value->typeKey);
    borrowed->borrowKind = borrow->kind;
    borrowed->localOrdinal = borrow->sourceLocalOrdinal;
    borrowed->parameterOrdinal = borrow->sourceParameterOrdinal;
    borrowed->children.push_back(std::move(value));
    value = std::move(borrowed);

    return value;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapFreshOwnedTemporaryBorrow(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* argumentSyntax,
    bool& hasOwnedTemporary)
{
    auto borrowed = makeNode(ProjectOopHirNodeKind::BorrowValue, argumentSyntax, value->typeKey);
    borrowed->borrowKind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
    borrowed->children.push_back(std::move(value));
    value = std::move(borrowed);

    // The slot is both the owner and the exactly-once guard. It is initialized only after the
    // fresh expression succeeds; from that point until FullExpressionCleanup finishes, every
    // potentially-throwing nested call sees the synthetic cleanup as active.
    auto temporary = makeNode(ProjectOopHirNodeKind::OwnedTemporaryBorrow, argumentSyntax, value->typeKey);
    temporary->localOrdinal = nextSyntheticLocalOrdinal_++;
    temporary->cleanupRegionOrdinal = nextSyntheticCleanupRegionOrdinal_++;
    temporary->children.push_back(std::move(value));
    hasOwnedTemporary = true;
    return temporary;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapFullExpressionCleanup(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* syntax,
    bool hasOwnedTemporary)
{
    if (!hasOwnedTemporary) return value;
    auto fullExpression = makeNode(ProjectOopHirNodeKind::FullExpressionCleanup, syntax, value->typeKey);
    fullExpression->children.push_back(std::move(value));
    return fullExpression;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDefaultArgument(
    const ast::AstNode* syntax,
    std::string_view parameterTypeKey,
    std::string& error)
{
    if (syntax == nullptr)
    {
        error = "NSC-LIMIT-020 selected optional parameter has no default expression";
        return nullptr;
    }
    const std::string target(parameterTypeKey);
    switch (syntax->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
            if (target == "builtin:float" || target == "builtin:double")
            {
                auto value = makeNode(ProjectOopHirNodeKind::FloatingConstant, syntax, target);
                value->floatingValue = static_cast<double>(syntax->integerValue());
                return value;
            }
            else
            {
                auto value = makeNode(ProjectOopHirNodeKind::IntegerConstant, syntax, target);
                value->integerValue = syntax->integerValue();
                return value;
            }
        case ast::AstNodeKind::FloatingLiteralExpression:
        {
            auto value = makeNode(ProjectOopHirNodeKind::FloatingConstant, syntax, target);
            value->floatingValue = syntax->floatingValue();
            return value;
        }
        case ast::AstNodeKind::BooleanLiteralExpression:
        {
            auto value = makeNode(ProjectOopHirNodeKind::BooleanConstant, syntax, "builtin:bool");
            value->booleanValue = syntax->tokenKind() == lexer::TokenKind::KwTrue;
            return value;
        }
        case ast::AstNodeKind::StringLiteralExpression:
        {
            auto value = makeNode(ProjectOopHirNodeKind::StringLiteral, syntax, "string-literal");
            value->stringValue = syntax->text();
            const ProjectImplicitConversion conversion =
                classifyImplicit("string-literal", target, std::nullopt);
            if (!conversion.exists())
            {
                error = "NSC-LIMIT-020 string default cannot convert to parameter type '" +
                    target + "'";
                return nullptr;
            }
            if (conversion.kind != ProjectConversionKind::Identity)
            {
                value = wrapImplicitConversion(std::move(value), conversion, syntax);
            }
            return value;
        }
        case ast::AstNodeKind::NullLiteralExpression:
            return makeNode(ProjectOopHirNodeKind::NullConstant, syntax, target);
        case ast::AstNodeKind::UnaryExpression:
        {
            auto operand = lowerDefaultArgument(syntax->first(), parameterTypeKey, error);
            if (operand == nullptr) return nullptr;
            auto value = makeNode(ProjectOopHirNodeKind::Unary, syntax, target);
            value->operation = syntax->tokenKind();
            value->children.push_back(std::move(operand));
            return value;
        }
        default:
            error = "NSC-LIMIT-020 default argument is not a supported literal constant";
            return nullptr;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerInstanceFieldInitialization(
    std::size_t fieldMemberOrdinal,
    std::optional<std::size_t> callerMemberOrdinal,
    std::string& error)
{
    if (memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        fieldMemberOrdinal >= memberModel_->members().size())
    {
        error = "Stage 37 cannot lower an invalid instance field initializer";
        return nullptr;
    }
    const ClassMemberSymbol& field = memberModel_->members()[fieldMemberOrdinal];
    if (field.kind != ClassMemberKind::Field || field.isStatic || field.syntax == nullptr ||
        field.syntax->first() == nullptr)
    {
        error = "Stage 37 instance initializer helper received a non-instance field";
        return nullptr;
    }

    auto target = makeNode(ProjectOopHirNodeKind::FieldAddress, field.syntax, field.declaredTypeKey);
    target->memberOrdinal = fieldMemberOrdinal;
    target->volatileAccess = field.isVolatile;
    target->children.push_back(makeNode(
        ProjectOopHirNodeKind::ThisValue,
        field.syntax,
        inheritanceModel_->typeKeyForOrdinal(field.declaringTypeOrdinal)));

    auto value = lowerExpression(
        field.syntax->first(), callerMemberOrdinal.value_or(fieldMemberOrdinal), false, error);
    if (value == nullptr)
    {
        return nullptr;
    }
    const MethodExpressionTypeBinding* sourceType = expressionType(field.syntax->first());
    const ProjectImplicitConversion conversion = classifyImplicit(
        value->typeKey,
        field.declaredTypeKey,
        sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
    if (!conversion.exists())
    {
        error = "Stage 37 field initializer lost its validated implicit conversion";
        return nullptr;
    }
    if (conversion.kind != ProjectConversionKind::Identity)
    {
        value = wrapImplicitConversion(std::move(value), conversion, field.syntax->first());
    }

    auto assignment = makeNode(ProjectOopHirNodeKind::Assignment, field.syntax, field.declaredTypeKey);
    assignment->operation = lexer::TokenKind::Equal;
    assignment->children.push_back(std::move(target));
    assignment->children.push_back(std::move(value));
    return assignment;
}

void ProjectOopHirModel::appendInstanceFieldInitializers(
    std::size_t typeOrdinal,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& output,
    std::string& error,
    std::optional<std::size_t> callerMemberOrdinal)
{
    if (memberModel_ == nullptr)
    {
        error = "Stage 37 instance initializer lowering has no member model";
        return;
    }
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& field = memberModel_->members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field || field.isStatic || field.syntax == nullptr ||
            field.syntax->first() == nullptr)
        {
            continue;
        }
        auto initializer = lowerInstanceFieldInitialization(
            memberOrdinal, callerMemberOrdinal, error);
        if (initializer == nullptr)
        {
            return;
        }
        output.push_back(std::move(initializer));
    }
}

std::vector<std::size_t> ProjectOopHirModel::implicitInitializerTypes(
    std::size_t targetTypeOrdinal,
    std::optional<std::size_t> entryConstructorMemberOrdinal) const
{
    std::vector<std::size_t> reversePath;
    if (inheritanceModel_ == nullptr || memberModel_ == nullptr)
    {
        return reversePath;
    }

    std::optional<std::size_t> stopType;
    if (entryConstructorMemberOrdinal.has_value() &&
        *entryConstructorMemberOrdinal < memberModel_->members().size())
    {
        stopType = memberModel_->members()[*entryConstructorMemberOrdinal].declaringTypeOrdinal;
    }

    std::optional<std::size_t> current = targetTypeOrdinal;
    while (current.has_value() && (!stopType.has_value() || *current != *stopType))
    {
        reversePath.push_back(*current);
        current = inheritanceModel_->baseTypeOrdinal(*current);
    }
    std::reverse(reversePath.begin(), reversePath.end());
    return reversePath;
}

const MethodNameBinding* ProjectOopHirModel::nameBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? nameBindingBySyntax_ : lookupSource_->nameBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodExpressionTypeBinding* ProjectOopHirModel::expressionType(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? expressionTypeBySyntax_ : lookupSource_->expressionTypeBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodCallBinding* ProjectOopHirModel::callBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? callBySyntax_ : lookupSource_->callBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const NamespaceFunctionCallBinding* ProjectOopHirModel::namespaceFunctionCallBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? namespaceFunctionCallBySyntax_
        : lookupSource_->namespaceFunctionCallBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}
const MethodSwitchBinding* ProjectOopHirModel::switchBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? switchBindingBySyntax_ : lookupSource_->switchBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodRuntimeTypeBinding* ProjectOopHirModel::runtimeTypeBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? runtimeTypeBindingBySyntax_ : lookupSource_->runtimeTypeBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodLambdaBinding* ProjectOopHirModel::lambdaBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? lambdaBindingBySyntax_ : lookupSource_->lambdaBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodLambdaValueBinding* ProjectOopHirModel::lambdaValueBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? lambdaValueBindingBySyntax_ : lookupSource_->lambdaValueBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

std::optional<std::size_t> ProjectOopHirModel::currentLambdaThisCapture() const noexcept
{
    if (currentLambda_ == nullptr) return std::nullopt;
    for (std::size_t index = 0; index < currentLambda_->captures.size(); ++index)
    {
        if (currentLambda_->captures[index].kind == MethodLambdaCaptureKind::ThisReference)
            return index;
    }
    return std::nullopt;
}

const MethodDelegateBinding* ProjectOopHirModel::delegateBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? delegateBindingBySyntax_ : lookupSource_->delegateBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodEventBinding* ProjectOopHirModel::eventBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? eventBindingBySyntax_ : lookupSource_->eventBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodCoreBinding* ProjectOopHirModel::coreBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? coreBindingBySyntax_ : lookupSource_->coreBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodOptionalCarrierBinding* ProjectOopHirModel::optionalCarrierBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? optionalCarrierBindingBySyntax_ : lookupSource_->optionalCarrierBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodResultPropagationBinding* ProjectOopHirModel::resultPropagationBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? resultPropagationBindingBySyntax_ : lookupSource_->resultPropagationBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodRangeSliceBinding* ProjectOopHirModel::rangeSliceBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr
        ? rangeSliceBindingBySyntax_ : lookupSource_->rangeSliceBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodExceptionBinding* ProjectOopHirModel::exceptionBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? exceptionBindingBySyntax_ : lookupSource_->exceptionBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodCleanupRegion* ProjectOopHirModel::cleanupRegion(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? cleanupRegionBySyntax_ : lookupSource_->cleanupRegionBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodUsingBinding* ProjectOopHirModel::usingBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? usingBindingBySyntax_ : lookupSource_->usingBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodMoveBinding* ProjectOopHirModel::moveBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? moveBindingBySyntax_ : lookupSource_->moveBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodBorrowBinding* ProjectOopHirModel::borrowBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? borrowBindingBySyntax_ : lookupSource_->borrowBindingBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodStructuredExitBinding* ProjectOopHirModel::structuredExitBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? structuredExitBySyntax_ : lookupSource_->structuredExitBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const ObjectConstructionBinding* ProjectOopHirModel::constructionBinding(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? constructionBySyntax_ : lookupSource_->constructionBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

const MethodLocalSymbol* ProjectOopHirModel::localSymbol(const ast::AstNode* syntax) const noexcept
{
    const auto& index = lookupSource_ == nullptr ? localBySyntax_ : lookupSource_->localBySyntax_;
    const auto found = index.find(syntax);
    return found == index.end() ? nullptr : found->second;
}

ProjectImplicitConversion ProjectOopHirModel::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant) const
{
    if (inheritanceModel_ != nullptr)
    {
        return ProjectConversionClassifier(*inheritanceModel_, interfaceModel_, memberModel_).classifyImplicit(
            sourceTypeKey, targetTypeKey, integerConstant);
    }
    return ProjectTypeConversion::classifyImplicit(
        sourceTypeKey, targetTypeKey, integerConstant, isReferenceType(targetTypeKey));
}

// C7 "aggregate initializer". Semantic analysis has already established that the shape is legal
// (see ProjectMethodBodyModel::bindAggregateInitializer); this only has to say WHERE each value
// goes. Nesting is resolved here, against the same StructValueLayout the C header generator emits
// offsets from, so an initializer and a C host cannot disagree about which bytes a field occupies.
bool ProjectOopHirModel::appendAggregateElements(
    const ast::AstNode* syntax,
    const std::string& targetTypeKey,
    std::uint64_t baseOffset,
    std::size_t memberOrdinal,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& elements,
    std::string& error)
{
    // Per-element type and offset, for the two aggregate shapes the language has.
    struct AggregateSlot final
    {
        std::string typeKey;
        std::uint64_t offset = 0;
        std::optional<std::uint64_t> bitOffset;
        std::optional<std::uint64_t> bitWidth;
        std::uint64_t storageSize = 0;
        std::uint64_t storageAlignment = 1;
        bool volatileAccess = false;
    };
    std::vector<AggregateSlot> slots;
    if (const std::optional<std::vector<std::string>> tupleElements =
            ProjectTypeConversion::tupleElementTypeKeys(targetTypeKey);
        tupleElements.has_value())
    {
        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
        {
            error = "tuple initializer requires the object layout model";
            return false;
        }
        const std::optional<language::AggregateLayout> layout =
            objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, targetTypeKey);
        if (!layout.has_value() || layout->offsets.size() != tupleElements->size())
        {
            error = "tuple initializer has no structural layout for '" + targetTypeKey + "'";
            return false;
        }
        for (std::size_t index = 0; index < tupleElements->size(); ++index)
        {
            const std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout =
                objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, (*tupleElements)[index]);
            if (!elementLayout.has_value())
            {
                error = "tuple initializer cannot size element " + std::to_string(index);
                return false;
            }
            slots.push_back(AggregateSlot{
                (*tupleElements)[index],
                static_cast<std::uint64_t>(layout->offsets[index]),
                std::nullopt, std::nullopt, 0, elementLayout->second, false});
        }
    }
    else if (const std::optional<std::string> elementTypeKey =
            ProjectMemberModel::fixedArrayElementTypeKey(targetTypeKey);
        elementTypeKey.has_value())
    {
        const std::optional<std::uint64_t> count =
            ProjectMemberModel::fixedArrayCount(targetTypeKey);
        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
        {
            error = "C7 aggregate initializer requires the object layout model";
            return false;
        }
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> layout =
            objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, *elementTypeKey);
        if (!count.has_value() || !layout.has_value() || layout->first == 0)
        {
            error = "C7 aggregate initializer cannot size the element type of '" + targetTypeKey + "'";
            return false;
        }
        for (std::uint64_t index = 0; index < *count; ++index)
        {
            slots.push_back(AggregateSlot{
                *elementTypeKey, index * layout->first, std::nullopt, std::nullopt, 0,
                layout->second, false});
        }
    }
    else
    {
        if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
        {
            error = "C7 aggregate initializer requires the object layout model";
            return false;
        }
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(targetTypeKey);
        const StructValueLayout* layout = typeOrdinal.has_value()
            ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
        if (layout == nullptr)
        {
            error = "C7 aggregate initializer has no value layout for '" + targetTypeKey + "'";
            return false;
        }
        for (const ObjectFieldLayout& field : layout->fields)
        {
            const ClassMemberSymbol* member = memberModel_ != nullptr &&
                    field.memberOrdinal < memberModel_->members().size()
                ? &memberModel_->members()[field.memberOrdinal] : nullptr;
            if (member != nullptr && member->isUnnamedBitField) continue;
            slots.push_back(AggregateSlot{
                field.typeKey,
                field.offset,
                field.bitOffset,
                field.bitWidth,
                field.bitWidth.has_value() ? field.size : 0,
                field.alignment,
                member != nullptr && member->isVolatile});
            // A union takes exactly one initializer, for its first member -- the same rule C has,
            // and the same one bindAggregateInitializer enforced.
            if (layout->isUnion) break;
        }
    }

    const MethodAggregateInitializerBinding* binding = methodBodyModel_ != nullptr
        ? methodBodyModel_->aggregateInitializerBinding(syntax) : nullptr;
    if (binding == nullptr || binding->entries.size() != syntax->children().size())
    {
        error = "T01-F05 aggregate initializer has no semantic element mapping";
        return false;
    }

    if (syntax->kind() == ast::AstNodeKind::WithExpression)
    {
        if (syntax->first() == nullptr)
        {
            error = "T01-F07 with expression has no source value";
            return false;
        }
        auto source = lowerExpression(syntax->first(), memberOrdinal, false, error);
        if (source == nullptr) return false;
        auto spread = makeNode(ProjectOopHirNodeKind::AggregateSpread, syntax, targetTypeKey);
        spread->children.push_back(std::move(source));
        for (const AggregateSlot& target : slots)
        {
            if (target.bitWidth.has_value())
            {
                error = "T01-F07 with expressions over bit fields require explicit copy semantics";
                return false;
            }
            auto descriptor = makeNode(
                ProjectOopHirNodeKind::AggregateElement, syntax, target.typeKey);
            descriptor->integerValue = baseOffset + target.offset;
            descriptor->secondaryIntegerValue = target.offset;
            descriptor->bitStorageAlignment = target.storageAlignment;
            descriptor->volatileAccess = target.volatileAccess;
            spread->children.push_back(std::move(descriptor));
        }
        elements.push_back(std::move(spread));
    }

    for (std::size_t index = 0; index < binding->entries.size(); ++index)
    {
        const MethodAggregateInitializerEntryBinding& entry = binding->entries[index];
        const ast::AstNode* element = entry.syntax;
        if (entry.spread)
        {
            if (element == nullptr || element->kind() != ast::AstNodeKind::SpreadElementExpression ||
                element->first() == nullptr)
            {
                error = "T01-F05 aggregate spread has no source expression";
                return false;
            }
            const ast::AstNode* sourceSyntax = element->first();
            if (entry.moveSpread && sourceSyntax != nullptr &&
                sourceSyntax->kind() == ast::AstNodeKind::MoveExpression)
            {
                sourceSyntax = sourceSyntax->first();
            }
            auto source = lowerExpression(
                sourceSyntax, memberOrdinal, entry.moveSpread, error);
            if (source == nullptr) return false;
            auto spread = makeNode(ProjectOopHirNodeKind::AggregateSpread, element, targetTypeKey);
            spread->booleanValue = entry.moveSpread;
            spread->localOrdinal = entry.moveSourceLocalOrdinal;
            spread->parameterOrdinal = entry.moveSourceParameterOrdinal;
            spread->children.push_back(std::move(source));
            for (const std::size_t targetOrdinal : entry.targetElementOrdinals)
            {
                if (targetOrdinal >= slots.size())
                {
                    error = "T01-F05 aggregate spread target is out of range";
                    return false;
                }
                const AggregateSlot& target = slots[targetOrdinal];
                auto descriptor = makeNode(
                    ProjectOopHirNodeKind::AggregateElement,
                    element,
                    target.typeKey);
                descriptor->integerValue = baseOffset + target.offset;
                descriptor->secondaryIntegerValue = target.offset;
                descriptor->bitOffset = target.bitOffset;
                descriptor->bitWidth = target.bitWidth;
                descriptor->bitStorageSize = target.storageSize;
                descriptor->bitStorageAlignment = target.storageAlignment;
                descriptor->volatileAccess = target.volatileAccess;
                if (target.bitWidth.has_value())
                {
                    error = "T01-F05 spread initialization of bit fields requires explicit named values";
                    return false;
                }
                spread->children.push_back(std::move(descriptor));
            }
            elements.push_back(std::move(spread));
            continue;
        }
        if (entry.targetElementOrdinals.size() != 1U ||
            entry.targetElementOrdinals.front() >= slots.size())
        {
            error = "T01-F05 aggregate value has no unique target element";
            return false;
        }
        const AggregateSlot& target = slots[entry.targetElementOrdinals.front()];
        const std::string& slotTypeKey = target.typeKey;
        const std::uint64_t slotOffset = baseOffset + target.offset;
        if (element->kind() == ast::AstNodeKind::AggregateInitializerExpression)
        {
            if (!appendAggregateElements(
                    element, slotTypeKey, slotOffset, memberOrdinal, elements, error))
            {
                return false;
            }
            continue;
        }
        auto value = lowerExpression(element, memberOrdinal, false, error);
        if (value == nullptr)
        {
            return false;
        }
        const ProjectImplicitConversion conversion = classifyImplicit(
            value->typeKey,
            slotTypeKey,
            expressionType(element) != nullptr ? expressionType(element)->integerConstant
                                              : std::nullopt);
        if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
        {
            value = wrapImplicitConversion(std::move(value), conversion, element);
        }
        auto slot = makeNode(ProjectOopHirNodeKind::AggregateElement, element, slotTypeKey);
        slot->integerValue = slotOffset;
        slot->bitOffset = target.bitOffset;
        slot->bitWidth = target.bitWidth;
        slot->bitStorageSize = target.storageSize;
        slot->bitStorageAlignment = target.storageAlignment;
        slot->volatileAccess = target.volatileAccess;
        slot->children.push_back(std::move(value));
        elements.push_back(std::move(slot));
    }
    return true;
}

// C8 "type root", struct half: the SHORT name a struct's synthesized ToString() answers --
// `Vector3`, not `Proj::game.math.Vector3`. Same rule the class descriptor's runtime name uses,
// so the two spellings of "what is this type called" cannot drift apart.
std::string ProjectOopHirModel::shortTypeName(std::string_view typeKey) const
{
    std::string_view identity = typeKey;
    if (identity.starts_with("type:")) identity = identity.substr(5);
    const std::size_t lastSeparator = identity.find_last_of(".:");
    return std::string(
        lastSeparator == std::string_view::npos ? identity : identity.substr(lastSeparator + 1));
}

// C8 "type root", struct half: flattens a struct into its SCALAR LEAVES -- one AggregateElement
// per primitive field, with nested structs and fixed arrays expanded in place and their absolute
// byte offsets computed here. Padding bytes are deliberately not leaves: a struct that crossed a C
// boundary carries whatever the other side left in its padding, so comparing or hashing those
// bytes would make two equal values disagree.
//
// The contract these leaves define, and the reason it is stated in exactly one place: the
// synthesized Equals is BIT equality of the fields, and the synthesized GetHashCode mixes the
// same bits. That keeps the two in agreement by construction -- which a "compare floats with
// fcmp, hash their bits" pairing would not, since +0.0 and -0.0 compare equal but hash apart, and
// a NaN would never equal itself while hashing identically. Anything wanting other semantics --
// value comparison of a string field, deep comparison through a reference -- declares its own
// Equals and GetHashCode, and the compiler then steps aside entirely.
bool ProjectOopHirModel::appendAggregateLeaves(
    const std::string& typeKey,
    std::uint64_t baseOffset,
    const ast::AstNode* syntax,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& leaves,
    std::string& error)
{
    if (const std::optional<std::string> elementTypeKey =
            ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        elementTypeKey.has_value())
    {
        const std::optional<std::uint64_t> count = ProjectMemberModel::fixedArrayCount(typeKey);
        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr || !count.has_value())
        {
            error = "C8 cannot expand fixed-array field '" + typeKey + "'";
            return false;
        }
        const std::optional<std::pair<std::uint64_t, std::uint64_t>> stride =
            objectLayoutModel_->valueLayoutForTypeKey(*nameLookup_, *elementTypeKey);
        if (!stride.has_value() || stride->first == 0)
        {
            error = "C8 cannot size fixed-array element '" + *elementTypeKey + "'";
            return false;
        }
        for (std::uint64_t index = 0; index < *count; ++index)
        {
            if (!appendAggregateLeaves(
                    *elementTypeKey, baseOffset + index * stride->first, syntax, leaves, error))
            {
                return false;
            }
        }
        return true;
    }

    if (isStructValueType(typeKey))
    {
        if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
        {
            error = "C8 struct root member requires the object layout model";
            return false;
        }
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(typeKey);
        const StructValueLayout* layout = typeOrdinal.has_value()
            ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
        if (layout == nullptr)
        {
            error = "C8 struct root member has no value layout for '" + typeKey + "'";
            return false;
        }
        for (const ObjectFieldLayout& field : layout->fields)
        {
            if (!appendAggregateLeaves(
                    field.typeKey, baseOffset + field.offset, syntax, leaves, error))
            {
                return false;
            }
        }
        return true;
    }

    auto leaf = makeNode(ProjectOopHirNodeKind::AggregateElement, syntax, typeKey);
    leaf->integerValue = baseOffset;
    leaves.push_back(std::move(leaf));
    return true;
}

bool ProjectOopHirModel::isStructValueType(std::string_view typeKey) const noexcept
{
    if (ProjectTypeConversion::isTuple(typeKey)) return true;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() == key) return type.kind == NamedTypeDeclarationKind::Struct;
    }
    return false;
}

bool ProjectOopHirModel::isClassReferenceType(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return false;
    }
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() == identity)
        {
            return type.kind == NamedTypeDeclarationKind::Class;
        }
    }
    return false;
}

const ClassMemberSymbol* ProjectOopHirModel::callableSymbol(std::size_t ordinal) const noexcept
{
    if (methodBodyModel_ != nullptr) return methodBodyModel_->callableSymbol(ordinal);
    if (memberModel_ == nullptr) return nullptr;
    if (ordinal < memberModel_->members().size())
        return &memberModel_->members()[ordinal];
    return nullptr;
}

bool ProjectOopHirModel::isReferenceType(std::string_view typeKey) const noexcept
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Reference;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return false;
    }
    const std::string_view identity = typeKey.substr(5);
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if (type.identity.key() == identity)
        {
            return type.kind == NamedTypeDeclarationKind::Class ||
                type.kind == NamedTypeDeclarationKind::Interface ||
                type.kind == NamedTypeDeclarationKind::Delegate;
        }
    }
    return false;
}
}
