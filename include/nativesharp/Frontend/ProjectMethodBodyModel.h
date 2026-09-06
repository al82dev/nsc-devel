#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <cstdint>
#include <functional>
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
class ProjectGenericConstraintModel;

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
    // El VALOR de un local `const` de tipo entero, plegado al declararlo.
    //
    // POR QUE SE GUARDA AQUI y no se recalcula al usarlo: la expresion que lo inicializa esta en
    // el sitio de la declaracion y en ningun otro. Cuando alguien escribe `x / cero`, lo unico
    // que tiene a mano es el nombre.
    //
    // Y POR QUE NO VIAJA EN `ExpressionResult::integerConstant`, que seria lo obvio: ese campo lo
    // consume `commonNumericType` para decidir el tipo comun de una operacion, asi que darle un
    // valor a cada `const` local cambiaria la PROMOCION de expresiones que hoy compilan. Esto se
    // usa para una pregunta concreta --¿el divisor es cero?-- y no para tipar nada.
    std::optional<std::int64_t> constantIntegerValue;
    bool isRaii = false;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    std::optional<std::size_t> localBorrowBindingOrdinal;
    std::optional<std::size_t> lambdaOrdinal;
    std::optional<std::size_t> lambdaParameterOrdinal;
    // SHR-013 "vistas y tiempo de vida". Solo para un local de tipo vista: el NOMBRE del local
    // de este mismo metodo del que la vista toma prestado, si lo hay. Vacio quiere decir que la
    // vista se enraiza en algo que sobrevive a la llamada -- un parametro, un campo, un literal,
    // o el resultado de una llamada que no menciono ningun local.
    //
    // Se calcula al declarar y se vuelve a calcular en cada asignacion, porque `v = otra;`
    // cambia de que presta `v`. Es un ANALISIS SINTACTICO deliberadamente conservador: mira que
    // nombres aparecen en la expresion, no a donde apunta el puntero en ejecucion. Un analisis
    // exacto necesitaria parametros de tiempo de vida en las firmas, que es otro problema y no
    // el de este bloque; este contesta la pregunta que el bloque plantea -- devolver una vista
    // de un local -- y se equivoca siempre del lado de rechazar, nunca del de aceptar.
    std::string viewRootLocalName;
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

// NSC-LIMIT-010. Por que ESTE veredicto es un tipo y no un `bool`.
//
// La regla de compatibilidad entre un argumento y un parametro con calificador de propiedad la
// necesitan DOS resolutores: el de llamadas ordinarias (`analyzeCall`) y el de constructores
// (`ProjectConstructionModel::bindConstruction`). Hasta el 2026-09-04 solo existia en el primero,
// y el segundo elegia constructor mirando unicamente tipos y conversiones fisicas -- por eso
// `new Caja(move(duenio))` no encontraba el constructor `owned` aunque estuviera escrito.
//
// Un `bool` habria bastado para descartar candidatos, pero no para el diagnostico: el contrato
// pide distinguir «falta move», «un prestado no satisface owned» y «move a un parametro sin
// calificador», y con un booleano cada llamador tendria que volver a deducir cual de los tres fue
// -- o sea, una segunda copia de la misma regla, que es exactamente el defecto que se esta
// cerrando.
enum class OwnershipArgumentFit
{
    Ok,
    // El parametro es `owned` y el argumento no transfiere: falta `move(...)`, o el valor es
    // prestado y por tanto no es suyo para darlo.
    OwnedRequiresTransfer,
    // El parametro es `borrowed` y el argumento no es una fuente prestable comprobada.
    BorrowedRequiresCheckedSource,
    // El parametro no lleva calificador y el argumento trae un contrato: transferirlo lo
    // perderia en silencio.
    UnqualifiedRejectsOwnership
};

// La regla, en un solo sitio y sin dependencias del modelo.
//
// `parameterOwnable` entra COMO DATO y no se calcula aqui a proposito: depende del name lookup
// (`isReferenceTypeKey`), que esta funcion no tiene ni debe tener. Los dos llamadores ya saben
// responderla. Y es el primer filtro, no el ultimo: un `owned T`/`borrowed T` generico
// monomorfizado sobre un tipo valor lleva el calificador escrito pero no tiene nada que poseer,
// asi que ahi TODA forma de argumento vale. Ese orden costo una correccion en Stage 52 --
// comprobar primero el calificador y despues la ownabilidad rechazaba
// `LinkedList<int>.AddFirst`, que pasa un `move` de un `int` a un parametro `owned T`.
OwnershipArgumentFit classifyOwnershipArgument(
    bool parameterOwnable,
    sema::OwnershipKind parameterOwnership,
    sema::OwnershipKind argumentOwnership,
    bool argumentTransfer,
    bool argumentIsFreshOwner) noexcept;

// Texto del diagnostico para un veredicto que no es Ok. Vive junto a la regla para que los dos
// resolutores digan lo mismo ante la misma causa.
std::string ownershipArgumentFitMessage(
    OwnershipArgumentFit fit,
    std::string_view parameterName,
    std::string_view parameterTypeKey);

struct MethodExpressionTypeBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string typeKey;
    std::optional<ProjectIntegerConstant> integerConstant;
    // NSC-LIMIT-010. El resultado semantico de una expresion llevaba ownership, transferencia y
    // origen, y `recordExpressionType` los TIRABA al copiarlo aqui. ProjectConstructionModel solo
    // ve estos bindings, asi que sin estos cuatro campos no tiene con que decidir: para el, un
    // `move(duenio)` y un `duenio` a secas eran la misma expresion del mismo tipo.
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    bool ownershipTransfer = false;
    // NSC-LIMIT-010: ver ExpressionResult::ownershipFreshOwner.
    bool ownershipFreshOwner = false;
    std::optional<std::size_t> ownershipSourceLocalOrdinal;
    std::optional<std::size_t> ownershipSourceParameterOrdinal;
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
    // NSC-LIMIT-021. A fresh owned result may be lent for one full expression. Unlike a
    // BorrowedReturn, this value still needs an owner: HIR materializes a hidden RAII slot and
    // destroys it after the containing call, including the exceptional path.
    FreshOwnedTemporaryToBorrowedParameter,
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

// `switch` / `case`. The body model is where a case label stops being an expression: it is folded
// to an integer, checked against the governing type, and checked for duplicates. Everything
// downstream reads these numbers, so no later pass has to know how to evaluate a constant.
struct MethodSwitchSectionBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::vector<std::int64_t> labelValues;
    bool hasDefault = false;
};

struct MethodSwitchBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string governingTypeKey;
    std::vector<MethodSwitchSectionBinding> sections;
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
    ProcessRun
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
    std::size_t defaultedParameterCount = 0;
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
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::string resultTypeKey;
};

class ProjectMethodBodyModel final
{
public:
    // NSC-LIMIT-007, tercera vuelta. El modelo de constraints entra aqui porque la unica fase que
    // puede decidir si una violacion de constraint es un error del programa es esta: la que
    // resuelve sobrecargas conociendo los tipos de los argumentos. Es un puntero y admite null
    // --una compilacion sin genericos no necesita el modelo-- y su ausencia significa
    // exactamente «ninguna candidata tiene constraints que consultar».
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericConstraintModel* genericConstraintModel = nullptr);

    std::span<const MethodScope> scopes() const noexcept;
    std::span<const MethodLocalSymbol> locals() const noexcept;
    std::span<const MethodNameBinding> nameBindings() const noexcept;
    std::span<const MethodShadowingEvent> shadowingEvents() const noexcept;
    std::span<const MethodExpressionTypeBinding> expressionTypes() const noexcept;
    // NSC-LIMIT-010. "Este tipo tiene algo que poseer": una referencia de clase/interfaz o una
    // cadena nativa. Publico porque ProjectConstructionModel lo necesita para aplicar la MISMA
    // regla de propiedad que las llamadas ordinarias, y volver a escribirlo alli seria la segunda
    // definicion de ownable -- que es la forma exacta del defecto que este ID cierra.
    bool typeKeyIsOwnable(std::string_view typeKey) const noexcept;
    std::span<const MethodRuntimeTypeBinding> runtimeTypeBindings() const noexcept;
    std::span<const MethodCoreBinding> coreBindings() const noexcept;
    std::span<const MethodExceptionBinding> exceptionBindings() const noexcept;
    std::span<const MethodCleanupRegion> cleanupRegions() const noexcept;
    std::span<const MethodUsingBinding> usingBindings() const noexcept;
    std::span<const MethodMoveBinding> moveBindings() const noexcept;
    std::span<const MethodOwnershipTransferBinding> ownershipTransferBindings() const noexcept;
    std::span<const MethodBorrowBinding> borrowBindings() const noexcept;
    std::span<const MethodLocalBorrowBinding> localBorrowBindings() const noexcept;
    std::span<const MethodSwitchBinding> switchBindings() const noexcept;
    std::span<const MethodStructuredExitBinding> structuredExits() const noexcept;
    std::span<const MethodCleanupContinuation> cleanupContinuations() const noexcept;
    std::span<const MethodLambdaBinding> lambdaBindings() const noexcept;
    std::span<const MethodLambdaValueBinding> lambdaValueBindings() const noexcept;
    std::span<const MethodDelegateBinding> delegateBindings() const noexcept;
    std::span<const MethodEventBinding> eventBindings() const noexcept;
    std::span<const MethodCallBinding> calls() const noexcept;
    // NSC-LIMIT-018. A generic specialization exposes all member signatures, but only bodies
    // reached from real source code are instantiated. HIR uses this exact set so a deferred,
    // unbound body can never be lowered accidentally.
    bool callableBodyAnalyzed(std::size_t memberOrdinal) const noexcept;
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
        // C7 "definite assignment". Unlike every other set here it is a MUST set, not a MAY set:
        // joining two paths keeps only what both agree on. joinOwnershipFlow intersects it.
        std::unordered_set<std::size_t> assignedLocals;
        // How many locals existed when this snapshot was taken. Local ordinals only ever grow, so
        // this separates "not assigned" from "did not exist yet" -- see restoreOwnershipFlow.
        std::size_t localCount = 0;
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
        // NSC-LIMIT-010. ESTA EXPRESION ACABA DE CREAR UN OWNER que no pertenece a nadie todavia:
        // `new T(...)`. No es lo mismo que una transferencia -- no hay fuente con nombre de la que
        // salga, y por eso `move` no se puede ni escribir sobre ella.
        //
        // Es una tercera situacion y no un caso de las otras dos, y hace falta porque un parametro
        // `owned` exigia «ownership == Owned && ownershipTransfer», lo que dejaba fuera
        // `new Caja(new Carga(7))` -- no habia de donde mover.
        //
        // SOLO RELAJA, NUNCA ENDURECE. Este comentario llego a afirmar que un parametro SIN
        // calificador rechazaba un owner recien creado; era falso y ademas seria un cambio de
        // lenguaje, no una comprobacion: `Insert(new Student(...))` sobre un parametro sin
        // calificador es la forma mas ordinaria que hay --cinco ejemplos del libro la usan-- y
        // `Student s = new Student(...); Insert(s);` hace lo mismo con el mismo objeto. Un
        // parametro sin calificador no rastrea la vida del valor, y eso vale igual para uno
        // recien creado que para uno que ya existia. Ver classifyOwnershipArgument.
        bool ownershipFreshOwner = false;
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
        // A switch pushes a context too, because `break` inside a switch leaves the SWITCH. But it
        // is not a `continue` target: `continue` inside a switch inside a loop continues the loop,
        // so analyzeLoopExit walks past every frame with this flag when it looks for one. Without
        // the flag a `continue` in a `switch` in a `while` would either be rejected or, worse,
        // bound to a jump target that does not exist.
        bool isSwitch = false;
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

    // CC1-200 "operacion comun de branch snapshot/join".
    //
    // POR QUE EXISTE. El compilador ya tenia la infraestructura correcta -- captureOwnershipFlow,
    // restoreOwnershipFlow, joinOwnershipFlow -- y `case IfStatement` la usaba bien. Pero cada
    // construccion nueva con ramas tenia que acordarse de repetir el patron a mano, y ni `?:` ni
    // `&&` ni `||` lo hicieron: analizaban sus dos lados secuencialmente, como si los dos se
    // ejecutaran siempre.
    //
    // La consecuencia no es cosmetica. Definite assignment es un conjunto MUST y su join es una
    // INTERSECCION; analizar secuencialmente equivale a UNIR, que es la direccion INSEGURA -- el
    // compilador da por asignada una variable que solo se asigna en una rama. Y como C7 pone a cero
    // el almacenamiento de los locales, el programa no explota: devuelve 0. El backend tapa el bug
    // del analisis, que es lo que lo hacia dificil de ver.
    //
    // Estas dos funciones convierten ese patron en algo que se LLAMA en vez de algo que se
    // RECUERDA. analyzeBranchState corre `work` partiendo de `entry` y devuelve el estado en que
    // quedo, sin dejar rastro en el estado activo; commitBranchJoin instala la union de dos de esos
    // estados como estado activo.
    OwnershipFlowSnapshot analyzeBranchState(
        const OwnershipFlowSnapshot& entry, const std::function<void()>& work);
    void commitBranchJoin(const OwnershipFlowSnapshot& left, const OwnershipFlowSnapshot& right);
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
    void analyzeSwitch(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    // CC1-105. El plegado de una etiqueta de `case` YA NO vive aqui: lo hace
    // `ProjectIntegerEvaluator`, el unico evaluador entero tipado del compilador. Esto queda como
    // el punto de entrada del modelo semantico, y devuelve el par (bits, tipo) -- nunca un
    // `std::int64_t` suelto, porque comparar dos etiquetas sin conocer su tipo fue exactamente el
    // defecto C-001.
    //
    // `declaringTypeOrdinal` se pasa solo mientras se expande el inicializador de un campo `const`.
    std::optional<ProjectIntegerValue> foldIntegerConstant(
        const ast::AstNode* expression,
        std::optional<std::size_t> declaringTypeOrdinal) const;

    // Adaptador que le da al evaluador compartido la resolucion de miembros y de tipos de ESTE
    // modelo, sin que el evaluador tenga que conocer el modelo entero.
    class ConstantMemberResolver final : public ProjectConstantMemberResolver
    {
    public:
        explicit ConstantMemberResolver(const ProjectMethodBodyModel& model) noexcept
            : model_(model)
        {
        }

        std::optional<std::size_t> resolveConstantMember(
            const ast::AstNode* syntax,
            std::optional<std::size_t> declaringTypeOrdinal) const override;
        bool describeConstantMember(
            std::size_t memberOrdinal,
            bool& isEnumConstant,
            std::int64_t& enumConstantValue,
            std::string& enumUnderlyingTypeKey,
            const ast::AstNode*& initializer,
            std::string& declaredTypeKey,
            std::optional<std::size_t>& declaringTypeOrdinal) const override;
        std::string resolveTypeKey(const ast::TypeSyntax* typeSyntax) const override;

    private:
        const ProjectMethodBodyModel& model_;
    };

    // Reverse linear scan of nameBindings_. Only ever called while folding a case label, whose
    // bindings were recorded moments earlier, so the scan stops almost immediately.
    const MethodNameBinding* recordedNameBinding(const ast::AstNode* syntax) const noexcept;
    // The const or enum-constant field of `typeOrdinal` named `name`, if there is one.
    std::optional<std::size_t> constantMemberOfType(
        std::size_t typeOrdinal, std::string_view name) const noexcept;
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

    // ID 82. `p.X += 3` significa `set_X(p, get_X(p) + 3)`, y el receptor de esas DOS llamadas
    // --mas los argumentos del indexador-- se baja una vez por llamada. Un destino CON direccion
    // no tiene este problema: `CompoundStore` baja la direccion una sola vez, y por eso existe
    // (ver su comentario en ProjectOopHirModel.h). Una propiedad NO TIENE direccion, asi que no
    // hay nada que bajar una sola vez y hay que demostrar que bajarlo dos veces NO SE OBSERVA.
    //
    // Esta funcion decide exactamente eso. Contesta `true` solo para expresiones que leen
    // almacenamiento del MARCO ACTUAL --`this`, parametros, locals-- y constantes, combinadas con
    // operadores puros. Sobre esa clase la segunda evaluacion es identica a la primera por
    // construccion: al getter que corre en medio no se le pasa la direccion de ningun local ni
    // parametro del llamador, asi que no tiene via para cambiar lo que la segunda lectura vera.
    // No es una aproximacion conservadora sobre "efectos secundarios": es la clase de expresiones
    // para las que la doble bajada es un teorema, no una apuesta.
    //
    // Un CAMPO queda deliberadamente fuera aunque parezca inofensivo. `h.inner.Prop += 1` leeria
    // `h.inner` dos veces, y el getter de `Prop` --que corre entre las dos lecturas y si alcanza
    // los campos del objeto-- podria reescribirlo. C# evalua el receptor una vez y usa el valor
    // viejo; nosotros usariamos el nuevo. Rechazarlo con un diagnostico que dice donde esta el
    // problema es preferible a divergir en silencio.
    //
    // `offender` sale apuntando al primer subnodo que no cumple, para que el diagnostico senale
    // el rango de fuente exacto en vez de la asignacion entera.
    // ¿el divisor es un cero que el compilador ya conoce? Ver el comentario de la definicion:
    // cubre la constante plegada Y el local `const` que vale cero, que llegan por vias distintas.
    bool divisorIsConstantZero(
        const ast::AstNode* divisorSyntax,
        const ExpressionResult& divisor,
        std::size_t scopeOrdinal) const;

    bool isAccessorDestinationReevaluable(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        const ast::AstNode*& offender) const;

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
    std::unordered_set<std::size_t> analyzedCallableMembers_;
    void verifyOutParameterAssignments(diagnostics::DiagnosticEngine& diagnostics) const;
    // C7 "definite assignment". Three questions, one per helper:
    //
    //  - is this local's type one the analysis reports on at all? (see the definition: aggregates
    //    are excluded because the backend gives them defined zero storage, so a partially filled
    //    aggregate is a legitimate C-style idiom rather than a garbage read);
    //  - given an assignment target, which local -- if any -- does writing it assign?
    //  - and the report itself, called from the one place identifiers become reads.
    bool definiteAssignmentTracksLocal(std::size_t localOrdinal) const noexcept;
    bool typeKeyHasDefinedDefaultStorage(std::string_view typeKey) const noexcept;
    // C8 "type root": a union takes no compiler-provided Equals/GetHashCode -- see the refusal in
    // analyzeCall for why.
    bool aggregateIsUnion(std::string_view typeKey) const noexcept;
    // NSC-LIMIT-004. ¿Esta clave nombra un STRUCT? Ni una clase, ni una interfaz, ni un enum, ni
    // un delegado, ni un `T[N]`.
    //
    // No se reutiliza `typeKeyHasDefinedDefaultStorage` --que seria lo cercano-- porque esa
    // tambien cubre los arreglos de tamanio fijo, y un `int[4] == int[4]` no es lo que este ID
    // define. La pregunta aqui es exactamente la que hace el camino de `Struct.Equals`.
    bool typeKeyIsStruct(std::string_view typeKey) const noexcept;
    // SHR-013 "vistas y tiempo de vida". Las tres preguntas de un tipo vista, separadas porque
    // NO son la misma:
    //   * aggregateIsBorrowedView -- el tipo se declaro `borrowed struct`.
    //   * typeKeyIsViewLike       -- el tipo ES una vista o un ARREGLO de vistas. Un arreglo no
    //     copia a sus elementos: guardar vistas en un `T[]` es guardar vistas, y un arreglo que
    //     sobrevive al origen es el mismo uso-despues-de-liberar con un nivel de indireccion.
    //   * typeKeyCarriesLifetime  -- el valor lleva un tiempo de vida ajeno que puede escapar:
    //     una referencia, una cadena, un arreglo, un puntero o una vista. NO es lo mismo que
    //     "posee almacenamiento" (esa pregunta la contesta isReferenceTypeKey || builtin:string,
    //     y una vista contesta NO a esa): hasta el Bloque 3 las dos coincidian, y por eso el
    //     compilador no habia necesitado distinguirlas.
    // SHR-013: el nombre del local de este metodo del que la expresion toma prestado, si alguno.
    // Es la pieza que contesta "devolver una vista de un local".
    std::optional<std::string> viewRootLocalOfExpression(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal) const;
    bool aggregateIsBorrowedView(std::string_view typeKey) const noexcept;
    bool typeKeyIsViewLike(std::string_view typeKey) const noexcept;
    bool typeKeyCarriesLifetime(std::string_view typeKey) const noexcept;
    const ast::AstNode* definiteAssignmentTargetRoot(const ast::AstNode* target) const noexcept;
    void markAssignmentTarget(const ast::AstNode* target, std::size_t scopeOrdinal);
    void checkDefiniteAssignment(
        const ast::AstNode* expression,
        std::size_t localOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    // C7 "aggregate initializer": the element list a braced initializer fills, and the binder
    // that type-checks one against its target. Both are shared with the HIR lowering by shape
    // rather than by call: the lowerer needs offsets, which live in the layout model.
    std::optional<std::vector<std::string>> aggregateElementTypeKeys(
        std::string_view typeKey) const;
    ExpressionResult bindAggregateInitializer(
        const ast::AstNode* syntax,
        const std::string& targetTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
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
    // True only for a "type:" key that actually names an enum. Structs, classes and delegates
    // share that prefix, so the prefix alone answers nothing. Mirrors the identical predicate the
    // LLVM lowerer keeps for mapping an enum key to i32.
    bool isEnumTypeKey(std::string_view typeKey) const noexcept;
    // CC1 fases 4B/4C. "Este tipo puede vivir en memoria cruda?" -- es decir, salir de ambito o
    // liberarse sin que nadie tenga que llamar a nada. Un tipo con destructor, o con un campo
    // `owned`, NO puede: su ciclo de vida necesita que alguien lo cierre, y ni un buffer de pila
    // ni un bloque del monton crudo lo cierran.
    bool elementTypeHasTrivialLifetime(std::string_view typeKey) const noexcept;
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
    const ProjectGenericConstraintModel* genericConstraintModel_ = nullptr;
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
    // C7 "definite assignment": the locals DEFINITELY assigned at the current point of the walk.
    // It travels with the ownership flow state (see OwnershipFlowSnapshot) because it needs
    // exactly the same control-flow discipline -- it is joined by INTERSECTION at every merge, so
    // a local assigned on only one arm of an `if` is not assigned after it.
    std::unordered_set<std::size_t> assignedLocals_;
    // The identifier syntax node currently being written to rather than read from. An assignment
    // analyzes its own left-hand side through the ordinary expression path, so without this the
    // very statement that assigns a local would be reported as a read of it.
    const ast::AstNode* definiteAssignmentWriteTarget_ = nullptr;
    // One diagnostic per offending read: a statement can be analyzed more than once (the
    // unreachable-code probe in analyzeBlock does exactly that), and that must not multiply the
    // same message.
    std::unordered_set<const ast::AstNode*> reportedUnassignedReads_;
    bool ownershipFlowReachable_ = true;
    std::vector<OwnershipFlowSnapshot> throwFlowStates_;
    std::vector<MethodSwitchBinding> switchBindings_;
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
