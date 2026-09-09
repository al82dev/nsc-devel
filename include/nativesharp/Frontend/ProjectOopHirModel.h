#pragma once

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
class ProjectGenericConstraintModel;

enum class ProjectOopHirNodeKind
{
    Block,
    LocalDeclaration,
    ExpressionStatement,
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
    // C7 "aggregate initializer": `V v = {1, 2};` and `int[4] cells = {1};`. typeKey is the
    // aggregate; children are AggregateElement nodes, ALREADY FLATTENED to absolute byte offsets,
    // so nesting (`Matrix m = { {1, 0}, {0, 1} };`) is resolved here rather than in the backend.
    // Elements the source did not mention are simply absent: the storage is zeroed first, which
    // is exactly the rule C gives a partial initializer.
    AggregateInitializer,
    // C7: one element of the above. integerValue is its byte offset from the start of the
    // aggregate, typeKey its type, and its single child the value to store there.
    AggregateElement,
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
    StaticFieldValue,
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
    ArrayAllocation,
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
    CoreConsoleWriteLine,
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
    // Stage 54.3 "Mutex" -- see MethodCoreOperationKind::MutexCreate/MutexLock/MutexUnlock/
    // MutexClose in ProjectMethodBodyModel.h. Exact same shape as CoreThreadCreate/Join/Detach
    // above (a builtin:nint HANDLE round-tripped between separate NativeSharp calls), so these
    // four rejoin the shared operand-loop bucket directly -- MutexCreate is zero-operand (the
    // first core operation in the project with none at all), MutexLock/Unlock/Close are
    // one-operand (the saved HANDLE), same as ThreadJoin/ThreadDetach.
    CoreMutexCreate,
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
    CoreSemaphoreWait,
    CoreSemaphoreRelease,
    CoreSemaphoreClose,
    CoreEventCreate,
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
    Deallocation
};

struct ProjectOopHirNode final
{
    ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::Block;
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::string elementTypeKey;
    std::string stringValue;
    lexer::TokenKind operation = lexer::TokenKind::Invalid;
    std::optional<std::size_t> memberOrdinal;
    // Stage 54.4 "Condition Variables" -- set ONLY on a CoreMonitorWait node, alongside
    // memberOrdinal above (which, for THIS node kind only, holds the `cv_` field's ordinal rather
    // than an ordinary field access's own ordinal). Holds the `lock_` field's ordinal. Default-
    // constructed to nullopt for every other node kind -- adding this field required no update to
    // any existing construction site in this codebase, since ProjectOopHirNode is always built via
    // makeNode()+field assignment, never positional aggregate initialization (see the fresh
    // Stage 54.3 lesson about MethodCoreBinding, which WAS positionally aggregate-initialized and
    // needed all 16 call sites patched for exactly this reason -- this struct has no such risk).
    std::optional<std::size_t> secondaryMemberOrdinal;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    std::optional<std::size_t> constructionOrdinal;
    std::optional<std::size_t> targetTypeOrdinal;
    std::optional<std::size_t> exceptionTypeOrdinal;
    std::optional<std::size_t> cleanupRegionOrdinal;
    std::optional<std::size_t> hirRegionOrdinal;
    std::optional<std::size_t> exceptionTargetRegionOrdinal;
    std::optional<std::size_t> targetScopeOrdinal;
    std::optional<std::size_t> disposeContractMemberOrdinal;
    std::optional<std::size_t> disposeImplementationMemberOrdinal;
    std::vector<std::size_t> cleanupRegionOrdinals;
    std::optional<MethodCleanupContinuationKind> cleanupContinuationKind;
    std::optional<MethodBorrowKind> borrowKind;
    std::optional<MethodLocalBorrowSourceKind> localBorrowSourceKind;
    std::optional<ProjectImplicitConversion> conversion;
    // Set only on a SwitchSection node: the folded constant value of every label on that section,
    // in source order. Empty on a section whose only label is `default`. Signed even when the
    // governing type is unsigned -- the value is a bit pattern here, and the backend re-widens it
    // to the governing type when it builds the LLVM case constant.
    std::vector<std::int64_t> switchLabels;
    // Set only on a SwitchSection node: this section carries the `default` label.
    bool switchHasDefault = false;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
    bool baseReceiver = false;
    bool interfaceDispatch = false;
    bool targetIsInterface = false;
    bool dynamicDestruction = false;
    bool valueTypeNoOpDeletion = false;
    bool catchAll = false;
    bool interfaceCleanupDispatch = false;
    std::size_t argumentCount = 0;
    std::vector<std::string> argumentTypeKeys;
    // Children remain in source evaluation order.  Calls use this map to place the resulting
    // values in ABI parameter order only after every source expression has been evaluated.
    std::vector<std::size_t> argumentParameterOrdinals;
    std::vector<std::unique_ptr<ProjectOopHirNode>> children;
};

struct ProjectOopHirFieldInitializer final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    bool isStatic = false;
    bool isConst = false;
    std::unique_ptr<ProjectOopHirNode> value;
};

struct ProjectOopHirFunction final
{
    std::size_t memberOrdinal = 0;
    std::size_t declaringTypeOrdinal = 0;
    bool constructor = false;
    bool destructor = false;
    bool isStatic = false;
    bool isExportedC = false;
    // Interop Foundation: a function declared in `extern C from "<dll>" { ... }`. It has NO body
    // anywhere in the project -- mangledName is the C symbol verbatim, the backend emits only a
    // declaration (with the same Win64 ABI mapping every definition gets), and (importDllName,
    // mangledName) flows to the linker so the import library materializes.
    bool isImportedC = false;
    // A public NativeSharp method materialized from .nsmeta. Its source declaration participates
    // in ordinary overload resolution, but its body and symbol live in the referenced library.
    bool isImportedNative = false;
    std::string importDllName;
    // C1.2 "import identity": this import's C symbol name is claimed by more than one library, so
    // it cannot be resolved statically by name -- mangledName is a unique internal name and the
    // backend emits a body that binds through LoadLibrary/GetProcAddress at first call.
    bool importRequiresRuntimeBinding = false;
    std::string cAbiName;
    bool isLambda = false;
    std::optional<std::size_t> lambdaOrdinal;
    bool hasClosureEnvironment = false;
    std::vector<std::string> captureTypeKeys;
    std::vector<std::size_t> capturedLocalOrdinals;
    std::vector<std::size_t> capturedParameterOrdinals;
    bool capturesThisCell = false;
    std::string genericSpecializationKey;
    std::string mangledName;
    std::string returnTypeKey;
    sema::OwnershipKind returnOwnership = sema::OwnershipKind::None;
    std::vector<std::string> parameterNames;
    std::vector<std::string> parameterTypeKeys;
    std::vector<sema::OwnershipKind> parameterOwnerships;
    std::vector<std::size_t> localOrdinals;
    std::unique_ptr<ProjectOopHirNode> body;
};

class ProjectOopHirModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectConstructionModel& constructionModel,
        const ProjectObjectLayoutModel& objectLayoutModel,
        std::string& error,
        const ProjectGenericConstraintModel* genericConstraintModel = nullptr);

    std::span<const ProjectOopHirFunction> functions() const noexcept;
    std::span<const ProjectOopHirFieldInitializer> fieldInitializers() const noexcept;
    const ProjectOopHirFunction* functionForMember(std::size_t memberOrdinal) const noexcept;
    bool isBuilt() const noexcept;

    static std::string mangleCallable(
        const NamedTypeDeclaration& type,
        const ClassMemberSymbol& member);

private:
    std::unique_ptr<ProjectOopHirNode> lowerBlock(
        const ast::AstNode* block,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerStatement(
        const ast::AstNode* statement,
        std::size_t memberOrdinal,
        std::string& error);
    // EL ENVOLTORIO QUE RECUERDA DONDE FALLO, y por que es un envoltorio y no 125 ediciones.
    //
    // Este fichero tiene 125 sitios que hacen `error = "..."; return nullptr;`, y ninguno decia en
    // que parte del programa del usuario estaba el problema. Adjuntar la ubicacion en cada uno
    // seria 125 ediciones, cada una necesitando el nodo correcto a mano.
    //
    // En vez de eso: `lowerExpression` es el embudo por el que pasan casi todos. El envoltorio
    // llama al trabajo real y, si vuelve en fallo y todavia no hay rango apuntado, apunta el del
    // nodo QUE ESTABA BAJANDO. Como el fallo se propaga desde dentro hacia fuera y solo se apunta
    // si no hay nada apuntado, el que gana es el nodo MAS INTERNO -- el que de verdad no se pudo
    // bajar, no la expresion entera que lo contiene.
    //
    // Es aproximado a proposito: apunta a un nodo, no al subtermino exacto en los casos donde el
    // error se detecta un nivel mas arriba. Eso es infinitamente mas que nada, cuesta una edicion
    // y no toca el camino de exito.
    std::unique_ptr<ProjectOopHirNode> lowerExpression(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);

    std::unique_ptr<ProjectOopHirNode> lowerExpressionImpl(
        const ast::AstNode* expression,
        std::size_t memberOrdinal,
        bool addressContext,
        std::string& error);

public:
    // El rango del nodo mas interno que no se pudo bajar, o un rango invalido si el fallo no vino
    // de una expresion. Quien reporta el error lo traduce a fichero:linea:columna con el
    // SourceManager -- este modelo no lo tiene y no le hace falta para recordar el sitio.
    SourceRange lastErrorRange() const noexcept { return lastErrorRange_; }

private:
    SourceRange lastErrorRange_{};
    // C6 "struct members": true when the expression denotes storage that already exists, so a
    // struct receiver can take its address directly instead of needing a temporary.
    bool isNaturallyAddressable(const ast::AstNode* expression) const noexcept;
    // NSC-LIMIT-008. The call binding of a name that the semantic model resolved to a
    // PROPERTY/INDEXER GETTER -- a call wearing the clothes of a name -- or null when the name is
    // real storage (a field, an array element) or is not resolved at all.
    //
    // It returns the BINDING and not a bool because the two callers need different halves of the
    // same answer: lowerExpression needs the binding to lower the accessor call, and
    // isNaturallyAddressable only needs to know whether there was one. Returning a bool would leave
    // lowerExpression re-deriving the binding, which is how the two sites came to disagree in the
    // first place.
    const MethodCallBinding* accessorGetterBinding(const ast::AstNode* expression) const noexcept;
    std::unique_ptr<ProjectOopHirNode> lowerReceiver(
        const ast::AstNode* receiverSyntax,
        std::string_view receiverTypeKey,
        std::size_t memberOrdinal,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> wrapImplicitConversion(
        std::unique_ptr<ProjectOopHirNode> value,
        const ProjectImplicitConversion& conversion,
        const ast::AstNode* syntax);
    std::unique_ptr<ProjectOopHirNode> wrapCallArgumentBorrow(
        std::unique_ptr<ProjectOopHirNode> value,
        const ast::AstNode* argumentSyntax,
        std::size_t targetMemberOrdinal,
        std::size_t targetParameterOrdinal,
        bool& hasOwnedTemporary);
    std::unique_ptr<ProjectOopHirNode> wrapFreshOwnedTemporaryBorrow(
        std::unique_ptr<ProjectOopHirNode> value,
        const ast::AstNode* argumentSyntax,
        bool& hasOwnedTemporary);
    std::unique_ptr<ProjectOopHirNode> wrapFullExpressionCleanup(
        std::unique_ptr<ProjectOopHirNode> value,
        const ast::AstNode* syntax,
        bool hasOwnedTemporary);
    std::unique_ptr<ProjectOopHirNode> lowerDefaultArgument(
        const ast::AstNode* syntax,
        std::string_view parameterTypeKey,
        std::string& error);
    std::unique_ptr<ProjectOopHirNode> lowerInstanceFieldInitialization(
        std::size_t fieldMemberOrdinal,
        std::string& error);
    void appendInstanceFieldInitializers(
        std::size_t typeOrdinal,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& output,
        std::string& error);
    std::vector<std::size_t> implicitInitializerTypes(
        std::size_t targetTypeOrdinal,
        std::optional<std::size_t> entryConstructorMemberOrdinal) const;

    const MethodNameBinding* nameBinding(const ast::AstNode* syntax) const noexcept;
    const MethodExpressionTypeBinding* expressionType(const ast::AstNode* syntax) const noexcept;
    const MethodCallBinding* callBinding(const ast::AstNode* syntax) const noexcept;
    const MethodSwitchBinding* switchBinding(const ast::AstNode* syntax) const noexcept;
    const MethodRuntimeTypeBinding* runtimeTypeBinding(const ast::AstNode* syntax) const noexcept;
    const MethodDelegateBinding* delegateBinding(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaBinding* lambdaBinding(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaValueBinding* lambdaValueBinding(const ast::AstNode* syntax) const noexcept;
    std::optional<std::size_t> currentLambdaThisCapture() const noexcept;
    const MethodEventBinding* eventBinding(const ast::AstNode* syntax) const noexcept;
    const MethodCoreBinding* coreBinding(const ast::AstNode* syntax) const noexcept;
    const MethodExceptionBinding* exceptionBinding(const ast::AstNode* syntax) const noexcept;
    const MethodCleanupRegion* cleanupRegion(const ast::AstNode* syntax) const noexcept;
    const MethodUsingBinding* usingBinding(const ast::AstNode* syntax) const noexcept;
    const MethodMoveBinding* moveBinding(const ast::AstNode* syntax) const noexcept;
    const MethodBorrowBinding* borrowBinding(const ast::AstNode* syntax) const noexcept;
    const MethodStructuredExitBinding* structuredExitBinding(const ast::AstNode* syntax) const noexcept;
    const ObjectConstructionBinding* constructionBinding(const ast::AstNode* syntax) const noexcept;
    const MethodLocalSymbol* localSymbol(const ast::AstNode* syntax) const noexcept;
    ProjectImplicitConversion classifyImplicit(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        std::optional<ProjectIntegerConstant> integerConstant = std::nullopt) const;
    bool isStructValueType(std::string_view typeKey) const noexcept;
    // C8 "type root", struct half. See the definitions for the contract the leaves define.
    std::string shortTypeName(std::string_view typeKey) const;
    bool appendAggregateLeaves(
        const std::string& typeKey,
        std::uint64_t baseOffset,
        const ast::AstNode* syntax,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& leaves,
        std::string& error);
    // C7 "aggregate initializer": flattens one braced initializer -- and, recursively, any braced
    // initializer nested inside it -- into AggregateElement nodes carrying absolute byte offsets
    // measured from the start of the OUTERMOST aggregate. `baseOffset` is where the aggregate
    // being filled starts within that outermost one.
    bool appendAggregateElements(
        const ast::AstNode* syntax,
        const std::string& targetTypeKey,
        std::uint64_t baseOffset,
        std::size_t memberOrdinal,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& elements,
        std::string& error);
    bool isClassReferenceType(std::string_view typeKey) const noexcept;
    bool isReferenceType(std::string_view typeKey) const noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectMethodBodyModel* methodBodyModel_ = nullptr;
    const ProjectConstructionModel* constructionModel_ = nullptr;
    const ProjectObjectLayoutModel* objectLayoutModel_ = nullptr;
    const ProjectGenericConstraintModel* genericConstraintModel_ = nullptr;
    std::vector<ProjectOopHirFunction> functions_;
    std::vector<ProjectOopHirFieldInitializer> fieldInitializers_;
    std::vector<std::optional<std::size_t>> functionByMember_;
    std::unordered_map<const ast::AstNode*, const MethodNameBinding*> nameBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodExpressionTypeBinding*> expressionTypeBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCallBinding*> callBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodSwitchBinding*> switchBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodRuntimeTypeBinding*> runtimeTypeBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodDelegateBinding*> delegateBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaBinding*> lambdaBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaValueBinding*> lambdaValueBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodEventBinding*> eventBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCoreBinding*> coreBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodExceptionBinding*> exceptionBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCleanupRegion*> cleanupRegionBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodUsingBinding*> usingBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodMoveBinding*> moveBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodBorrowBinding*> borrowBindingBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodStructuredExitBinding*> structuredExitBySyntax_;
    std::unordered_map<const ast::AstNode*, const ObjectConstructionBinding*> constructionBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLocalSymbol*> localBySyntax_;
    struct HirExceptionHandlerContext final
    {
        std::size_t hirRegionOrdinal = 0;
        std::size_t retainedCleanupDepth = 0;
    };

    const MethodLambdaBinding* currentLambda_ = nullptr;
    std::vector<std::size_t> activeHirCleanupRegions_;
    std::vector<HirExceptionHandlerContext> activeHirExceptionHandlers_;
    std::size_t nextHirRegionOrdinal_ = 0;
    std::size_t nextSyntheticLocalOrdinal_ = 0;
    std::size_t nextSyntheticCleanupRegionOrdinal_ = 0;
    bool built_ = false;
};

const char* projectOopHirNodeKindName(ProjectOopHirNodeKind kind) noexcept;
}
