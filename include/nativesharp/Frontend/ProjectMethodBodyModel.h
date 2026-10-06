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
#include "nativesharp/Language/LanguageEdition.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Sema/ExpressionInfo.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectGenericConstraintModel;
class ProjectGenericModel;
class ProjectReferenceEscapeEvaluator;
class ProjectCallableResultMerger;
class ProjectCallableWaveReducer;

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
    // NativeSharp 2.0 item 269: `T[length] local;` is stack storage whose extent is
    // evaluated when the declaration executes.  Downstream expression typing uses the
    // pointer-shaped `typeKey`, while these two fields preserve the array declaration's
    // element type and count expression so HIR can synthesize the existing stack-allocation
    // operation without changing fields, parameters, or return types.
    bool isVariableLengthArray = false;
    std::string variableLengthArrayElementTypeKey;
    const ast::AstNode* variableLengthArrayCountSyntax = nullptr;
    // Source-order dimensions for multidimensional VLAs plus the compiler-generated locals
    // that snapshot each extent exactly once at the declaration.
    std::vector<const ast::AstNode*> variableLengthArrayExtentSyntaxes;
    std::vector<std::string> variableLengthArrayExtentTypeKeys;
    std::vector<std::size_t> variableLengthArrayExtentLocalOrdinals;
    // Set on the VLA itself and on local pointer aliases derived from it. This provenance is
    // consumed by return, assignment, and lambda-capture checks so frame storage cannot escape.
    std::optional<std::size_t> variableLengthArrayRootLocalOrdinal;
    // NativeSharp 2.0 item 312. typeKey is the logical element type; the physical local slot
    // stores an address to another exact-type, mutable location.
    bool isReference = false;
    // Item 315. A ref local keeps the escape origin of the address it aliases. This closes
    // transitive escapes (`ref alias = ref local; return ref alias`) without guessing again at
    // the return site and also propagates through reference-returning calls.
    MethodEscapeOrigin referenceEscapeOrigin;
    std::optional<std::size_t> referenceLifetimeScopeOrdinal;
    // NativeSharp 3.0 T06-F03. Mutable reference locals are exclusive borrows of a canonical
    // storage place.  The key is rooted in a local/parameter (L#/P#) and contains field/index
    // projections.  "U" represents an origin that cannot be proven and therefore overlaps
    // every other live mutable borrow; empty is reserved for non-reference locals.
    std::string referencePlaceKey;
    // A reborrow does not create a second independent alias.  It temporarily suspends this
    // source reference while the child reference is live, and preserves the original place.
    std::optional<std::size_t> referenceReborrowSourceLocalOrdinal;
    bool isConst = false;
    // T42: volatile local storage uses the same observable-load/store contract as volatile
    // fields. It is not atomic and supplies no inter-thread ordering.
    bool isVolatile = false;
    bool onePastEndPointer = false;
    bool pointerPointeeReadonly = false;
    bool pointerNullable = true;
    bool pointerHasProvenance = true;
    std::size_t pointerKnownAlignment = 1;
    unsigned pointerAddressSpace = 0;
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
    // NativeSharp 3.0 T06-F01. This local stores an edition-3 value type that did not opt into
    // Copy. It shares the existing path-sensitive moved-from sets with RAII owners, but it does
    // not acquire a cleanup region merely for being move-only.
    bool isMoveOnly = false;
    // T08-F03. Present for locals initialized from an explicit-capture closure. The delegate
    // carrier stays ABI-compatible; this semantic capability controls invocation rules.
    std::optional<language::CallableTrait> callableTrait;
    // T06-F02. A value local with an owned field or a (possibly nested) custom destructor owns
    // cleanup despite not being a reference-shaped `raii` local.
    bool requiresValueDrop = false;
    // NativeSharp 3.0 T06-F06. A `pin raii` owner keeps its ownership slot and object identity
    // fixed until lexical cleanup. It may be borrowed and destroyed in place, but never moved or
    // replaced after initialization.
    bool isPinned = false;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    // Item 320. A placement result owns object destruction while borrowing supplied bytes.
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
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
    NamespaceState,
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
    bool argumentIsFreshOwner,
    bool argumentIsNull) noexcept;

// Texto del diagnostico para un veredicto que no es Ok. Vive junto a la regla para que los dos
// resolutores digan lo mismo ante la misma causa.
std::string ownershipArgumentFitMessage(
    OwnershipArgumentFit fit,
    std::string_view parameterName,
    std::string_view parameterTypeKey);

struct MethodAggregateInitializerEntryBinding final
{
    const ast::AstNode* syntax = nullptr;
    bool spread = false;
    // T02-F06: a nontrivial aggregate may fill its unwritten fields from
    // `..move(source)`. The source stays alive in a partially moved state; these ordinals name
    // the root whose transferred fields must be emptied and rejected on subsequent reads.
    bool moveSpread = false;
    std::optional<std::size_t> moveSourceLocalOrdinal;
    std::optional<std::size_t> moveSourceParameterOrdinal;
    // A value entry has exactly one target. A spread owns every still-unwritten target and is
    // evaluated exactly once at its textual position.
    std::vector<std::size_t> targetElementOrdinals;
};

struct MethodAggregateInitializerBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::string targetTypeKey;
    bool generalized = false;
    std::vector<MethodAggregateInitializerEntryBinding> entries;
};

// NativeSharp 3.0 T02-F03. A deconstruction always consumes one tuple carrier. For a native
// tuple the carrier is the source expression itself; for a nominal source it is the result of the
// statically selected, parameterless instance Deconstruct() method recorded here. Keeping this
// decision in sema prevents HIR from repeating member lookup or changing overload/access rules.
struct MethodDeconstructionBinding final
{
    const ast::AstNode* syntax = nullptr;
    const ast::AstNode* sourceSyntax = nullptr;
    std::string sourceTypeKey;
    std::string tupleTypeKey;
    std::optional<std::size_t> selectedMemberOrdinal;
};

struct MethodMatchExpressionBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::string governingTypeKey;
    std::string resultTypeKey;
    bool referenceResult = false;
    // Present only when every arm is an unguarded whole-case variant pattern. The backend may
    // then load the compiler-owned tag once and emit an LLVM switch instead of repeated IsCase.
    std::optional<std::size_t> switchTagMemberOrdinal;
    std::vector<std::vector<std::int64_t>> armTags;
    std::vector<ProjectImplicitConversion> armConversions;
};

// T07-F05. One semantic contract for the value-producing loop and one for each break that exits
// it. Keeping the conversion on the break syntax means HIR never repeats type unification and a
// labelled break can target an outer loop without relying on the lowering stack's top frame.
struct MethodLoopExpressionBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string label;
    std::string resultTypeKey;
};

struct MethodLoopBreakBinding final
{
    const ast::AstNode* syntax = nullptr;
    const ast::AstNode* targetLoopSyntax = nullptr;
    std::string resultTypeKey;
    ProjectImplicitConversion conversion;
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
    bool hasFilter = false;
    bool filterAlwaysTrue = false;
    // NativeSharp 3.0 T07-F03. Declared errors from the protected region that can reach this
    // source-ordered type/wildcard pattern. `exhaustive` means this clause completes coverage of
    // every declared error still possible at the try boundary.
    std::vector<std::string> matchedErrorTypeKeys;
    bool exhaustive = false;
};

enum class MethodCleanupRegionKind
{
    Finally,
    UsingDispose,
    RaiiDestroy,
    OwnedParameterDestroy,
    ValueLocalDrop,
    ValueParameterDrop
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
    // A partial move keeps the containing owner alive while emptying one of its fields or
    // elements.  Retain that root owner separately from the moved slot so semantic validation
    // can enforce borrow exclusivity and dumps can make the decision observable.
    std::optional<std::size_t> partialSourceOwnerLocalOrdinal;
    std::optional<std::size_t> partialSourceOwnerParameterOrdinal;
    // Canonical source-level projection from the named root, e.g. {"Header", "Token"} for
    // `move(packet.Header.Token)`. Empty for whole-value moves and runtime-indexed elements.
    std::vector<std::string> partialFieldPath;
    std::string typeKey;
    // Stage 52: move(...) also accepts a native-array element (e.g. "move(items[i])"),
    // needed by generic collections (Queue<T>.Dequeue(), Stack<T>.Pop(), ...) to extract an
    // owned element out of backing storage. When true, sourceLocalOrdinal/sourceParameterOrdinal
    // are both unset -- HIR lowering re-derives the array/index sub-expressions directly from
    // `syntax->first()` (the IndexExpression being moved).
    //
    // `clearSourceStorage` records whether the SOURCE actually needs load+empty-store treatment.
    // Reference slots use null and move-only value aggregates use their zero/empty state; Copy
    // scalars degrade to a harmless plain read.
    bool sourceIsArrayElement = false;
    // Bloque 1: move(...) also accepts a direct `owned` field access -- `move(holder.slot)`, or
    // `move(slot)` through the implicit this. Same contract as the array-element case: both
    // source ordinals stay unset, HIR lowers the field access as an address and pairs the load
    // with a null-store into the vacated slot, so the container's own discharge (its destruction
    // entry, or a later overwrite's release) sees an empty slot instead of double-freeing the
    // value that was just moved out.
    bool sourceIsOwnedField = false;
    // The source storage must be cleared after the load whenever its bits carry a drop
    // obligation. Reference slots use null as their optimized drop flag; move-only value
    // aggregates are zeroed so every nested owning slot becomes empty as well.
    bool clearSourceStorage = false;
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
    // T41: a borrowed local may be reassigned along different control-flow paths.  Keep every
    // possible owner root; replacing the old root would make a later move/return check unsound.
    std::vector<std::size_t> possibleSourceOwnerLocalOrdinals;
    std::vector<std::size_t> possibleSourceOwnerParameterOrdinals;
    std::string typeKey;
};

struct MethodBorrowedParameterReassignmentBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t parameterOrdinal = 0;
    std::vector<std::size_t> possibleSourceOwnerLocalOrdinals;
    std::vector<std::size_t> possibleSourceOwnerParameterOrdinals;
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
    RangeArrayAllocation,
    ArrayLength,
    // NativeSharp 3.0 T31. Inline fixed arrays do not carry a runtime header. Their Length is
    // part of the canonical type key and therefore folds to a compile-time integer constant.
    FixedArrayLength,
    ArrayElement,
    RangeArrayElement,
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
    Panic
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
    // Tanda 5, items 304-306 -- present only for ordered AtomicInt32/AtomicInt64 operations.
    // overloads. The value is the stable NativeSharp MemoryOrder ABI value (0..4), deliberately
    // not an LLVMAtomicOrdering ordinal. HIR lowering consumes the source argument at compile
    // time and carries this value as node metadata, so an ordering can never become a runtime
    // integer passed accidentally to LLVM.
    std::optional<std::uint8_t> atomicMemoryOrder = std::nullopt;
    // Item 306 only: CompareExchange has an independent failure order. It remains absent on
    // every legacy overload and on all single-order atomic operations.
    std::optional<std::uint8_t> atomicFailureMemoryOrder = std::nullopt;

};

struct MethodVariableArrayIndexBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::vector<std::size_t> extentLocalOrdinals;
};



enum class MethodLambdaCaptureKind
{
    Local,
    Parameter,
    ThisReference
};

enum class MethodLambdaCaptureSourceKind
{
    OriginalLocal,
    OriginalParameter,
    OriginalThis,
    ParentLocal,
    ParentParameter,
    ParentCapture
};

struct MethodLambdaCapture final
{
    MethodLambdaCaptureKind kind = MethodLambdaCaptureKind::Local;
    std::string name;
    std::string typeKey;
    std::optional<std::size_t> localOrdinal;
    std::optional<std::size_t> parameterOrdinal;
    MethodLambdaCaptureSourceKind sourceKind = MethodLambdaCaptureSourceKind::OriginalLocal;
    std::optional<std::size_t> sourceOrdinal;
    // T08-F03. Reference modes share the source cell. Copy/move modes materialize an
    // independent cell when the closure value is created.
    language::CaptureMode mode = language::CaptureMode::Reference;
    // True only for a move capture which transfers a real destruction obligation into the
    // closure environment (RAII pointer/array/string or a value with drop glue).
    bool ownsCapturedValue = false;
    bool clearMovedSource = false;
};

struct MethodLambdaValueBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t lambdaOrdinal = 0;
    bool lambdaParameter = false;
    // E11: ref/out/in lambda parameters use the delegate's physical pointer parameter but expose
    // its pointee in the body, exactly like an ordinary method parameter.
    bool lambdaParameterByReference = false;
    std::size_t valueOrdinal = 0;
    std::string typeKey;
};

struct MethodLambdaBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t lambdaOrdinal = 0;
    std::optional<std::size_t> parentLambdaOrdinal;
    std::size_t ownerMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string delegateTypeKey;
    std::size_t delegateTypeOrdinal = 0;
    std::string returnTypeKey;
    std::string errorTypeKey;
    std::vector<std::string> parameterNames;
    std::vector<std::string> parameterTypeKeys;
    std::vector<std::string> parameterValueTypeKeys;
    std::vector<bool> parameterByReference;
    std::vector<MethodLambdaCapture> captures;
    language::CallableTrait callableTrait = language::CallableTrait::Fn;
    language::ClosureLayout closureLayout;
    bool explicitCaptureList = false;
    bool blockBody = false;
    bool anonymousNominal = false;
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
    // T08-F05. A method value keeps its category through HIR: a virtual/interface target is
    // resolved from the bound receiver when the native delegate cell is created.  A base-qualified
    // method value remains a direct target, matching ordinary base-call semantics.
    bool virtualDispatch = false;
    bool interfaceDispatch = false;
    bool ownsReceiver = false;
    std::optional<std::size_t> receiverSourceLocalOrdinal;
    std::optional<std::size_t> receiverSourceParameterOrdinal;
    // T08-F02. Non-empty only when invoking a zero-size anonymous non-capturing lambda. In that
    // case there is no delegate cell/function-pointer child to evaluate: HIR/MIR call this
    // generated static callable directly.
    std::string directLambdaName;
    // T08-F04. A capturing generic lambda can have several monomorphized entry points while
    // retaining one lexical environment. The callee expression supplies the delegate cell whose
    // receiver is passed to directLambdaName; its stored code pointer is intentionally ignored.
    bool directLambdaUsesEnvironment = false;
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

// E017 carrier bridge. `??` recognizes only the public NativeSharp.Core.Optional<T> protocol;
// lowering calls these methods and never reaches into the carrier's private fields or offsets.
struct MethodOptionalCarrierBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string carrierTypeKey;
    std::string valueTypeKey;
    std::size_t hasValueMemberOrdinal = 0;
    std::size_t uncheckedValueMemberOrdinal = 0;
};

// NativeSharp 3.0 T07-F01. Postfix `?` is bound against the compiler-owned
// two-case variant protocol instead of a type name. Keeping the complete
// protocol here lets every later layer consume one semantic decision.
struct MethodResultPropagationBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    const ast::AstNode* operandSyntax = nullptr;
    std::string sourceCarrierTypeKey;
    std::string successTypeKey;
    std::string sourceErrorTypeKey;
    std::string returnCarrierTypeKey;
    std::string returnErrorTypeKey;
    std::size_t sourceSuccessTag = 0;
    std::size_t sourceErrorTag = 0;
    std::size_t returnErrorTag = 0;
    std::size_t sourceSuccessTestMemberOrdinal = 0;
    std::size_t sourceSuccessExtractorMemberOrdinal = 0;
    std::size_t sourceErrorExtractorMemberOrdinal = 0;
    std::size_t returnErrorFactoryMemberOrdinal = 0;
    std::optional<std::size_t> errorConversionMemberOrdinal;
};

// NativeSharp 3.0 T01-F04. A range index is structural sugar over two public instance
// methods on the receiver: `int Length()` and `Slice(int start, int length)`.  The binding
// keeps both selected methods together because lowering must evaluate the receiver exactly once
// and reuse that physical value for both calls.  Endpoints remain source syntax so their normal
// evaluation order and conversions survive into HIR.
struct MethodRangeSliceBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    std::size_t lengthMemberOrdinal = 0;
    std::size_t sliceMemberOrdinal = 0;
    std::string resultTypeKey;
    const ast::AstNode* startSyntax = nullptr;
    const ast::AstNode* endSyntax = nullptr;
    ProjectImplicitConversion startConversion;
    ProjectImplicitConversion endConversion;
    bool hasStart = false;
    bool startFromEnd = false;
    bool hasEnd = false;
    bool endFromEnd = false;
    // The endpoints are one of 0..0, 0..Length or Length..Length. These are valid for every
    // protocol-conforming non-negative Length and need no runtime range guard, even at O0.
    bool boundsStaticallyProven = false;
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
    // One entry per explicit argument, in source evaluation order.  The value is the selected
    // callable's parameter ordinal.
    std::vector<std::size_t> argumentParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    // NativeSharp 3.0 T01-F02. One bit per explicit source argument. A true bit means the
    // argument is an element of the final params ReadOnlySpan<T>, not a prebuilt span.
    std::vector<bool> parameterPackElements;
    std::optional<std::size_t> parameterPackOrdinal;
    // Source argument ordinals whose fresh owned string is borrowed only for the duration of
    // this call. Used by namespace/free functions as well as member calls.
    std::vector<std::size_t> fullExpressionBorrowArgumentOrdinals;
    bool usesExpandedParameterPack = false;
    bool synthesizesEmptyParameterPack = false;
    std::size_t defaultedParameterCount = 0;
    bool isVariadicCandidate = false;
    std::optional<std::size_t> invalidVariadicArgumentOrdinal;
    std::optional<std::size_t> invalidMoveOnlyArgumentOrdinal;
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
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<bool> selectedParameterPackElements;
    std::optional<std::size_t> selectedParameterPackOrdinal;
    bool selectedUsesExpandedParameterPack = false;
    bool selectedSynthesizesEmptyParameterPack = false;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    // Source expressions in the same order as selectedArgumentParameterOrdinals. Retained so
    // T06-F07 can propagate escape contracts after the complete call graph has been analyzed.
    std::vector<const ast::AstNode*> selectedArgumentSyntaxes;
    std::string resultTypeKey;
};

struct NamespaceFunctionCallBinding final
{
    MethodCallResolutionKind resolution = MethodCallResolutionKind::NoViableCandidate;
    const ast::AstNode* syntax = nullptr;
    std::size_t callerCallableOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string writtenName;
    bool forwardedReceiver = false;
    const ast::AstNode* receiverSyntax = nullptr;
    std::vector<std::size_t> candidateFunctionOrdinals;
    std::vector<MethodCallCandidate> evaluatedCandidates;
    std::vector<std::size_t> viableCandidateFunctionOrdinals;
    std::optional<std::size_t> selectedFunctionOrdinal;
    std::vector<ProjectImplicitConversion> selectedConversions;
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<bool> selectedParameterPackElements;
    std::optional<std::size_t> selectedParameterPackOrdinal;
    bool selectedUsesExpandedParameterPack = false;
    bool selectedSynthesizesEmptyParameterPack = false;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    std::vector<const ast::AstNode*> selectedArgumentSyntaxes;
    std::string resultTypeKey;
};

class ProjectMethodBodyModel final
{
    friend class ProjectReferenceEscapeEvaluator;
    friend class ProjectCallableResultMerger;
    friend class ProjectCallableWaveReducer;

public:
    // NSC-LIMIT-007, tercera vuelta. El modelo de constraints entra aqui porque la unica fase que
    // puede decidir si una violacion de constraint es un error del programa es esta: la que
    // resuelve sobrecargas conociendo los tipos de los argumentos. Es un puntero y admite null
    // --una compilacion sin genericos no necesita el modelo-- y su ausencia significa
    // exactamente «ninguna candidata tiene constraints que consultar».
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectGenericModel* genericModel,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericConstraintModel* genericConstraintModel = nullptr,
        std::optional<std::size_t> publicSurfaceProjectOrdinal = std::nullopt,
        language::LanguageEdition languageEdition = language::LanguageEdition::V2,
        const ProjectMethodBodyBuildOptions& options = {});

    std::span<const MethodScope> scopes() const noexcept;
    std::span<const MethodLocalSymbol> locals() const noexcept;
    std::span<const MethodNameBinding> nameBindings() const noexcept;
    const MethodNameBinding* nameBinding(
        const ast::AstNode* syntax,
        std::size_t callableOrdinal) const noexcept;
    std::span<const MethodShadowingEvent> shadowingEvents() const noexcept;
    std::span<const MethodExpressionTypeBinding> expressionTypes() const noexcept;
    const MethodExpressionTypeBinding* expressionTypeBinding(
        const ast::AstNode* syntax,
        std::size_t callableOrdinal) const noexcept;
    std::span<const ProjectConstructionSite> constructionSites() const noexcept;
    const MethodAggregateInitializerBinding* aggregateInitializerBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodDeconstructionBinding* deconstructionBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodMatchExpressionBinding* matchExpressionBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodLoopExpressionBinding* loopExpressionBinding(
        const ast::AstNode* syntax) const noexcept;
    const MethodLoopBreakBinding* loopBreakBinding(
        const ast::AstNode* syntax) const noexcept;
    const std::string* staticInterpolationValue(const ast::AstNode* syntax) const noexcept;
    // NSC-LIMIT-010. "Este tipo tiene algo que poseer": una referencia de clase/interfaz o una
    // cadena nativa. Publico porque ProjectConstructionModel lo necesita para aplicar la MISMA
    // regla de propiedad que las llamadas ordinarias, y volver a escribirlo alli seria la segunda
    // definicion de ownable -- que es la forma exacta del defecto que este ID cierra.
    bool typeKeyIsOwnable(std::string_view typeKey) const noexcept;
    std::span<const MethodRuntimeTypeBinding> runtimeTypeBindings() const noexcept;
    std::span<const MethodCoreBinding> coreBindings() const noexcept;
    const MethodVariableArrayIndexBinding* variableArrayIndexBinding(
        const ast::AstNode* syntax) const noexcept;
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
    std::span<const MethodOptionalCarrierBinding> optionalCarrierBindings() const noexcept;
    std::span<const MethodResultPropagationBinding> resultPropagationBindings() const noexcept;
    std::span<const MethodRangeSliceBinding> rangeSliceBindings() const noexcept;
    std::span<const MethodCallBinding> calls() const noexcept;
    const MethodCallBinding* callBinding(
        const ast::AstNode* syntax,
        std::size_t callableOrdinal) const noexcept;
    std::span<const NamespaceFunctionCallBinding> namespaceFunctionCalls() const noexcept;
    const NamespaceFunctionCallBinding* namespaceFunctionCall(
        const ast::AstNode* syntax) const noexcept;
    // NSC-LIMIT-018. A generic specialization exposes all member signatures. Executables analyze
    // bodies reached from real source; libraries additionally analyze every public/protected body
    // in their exported closed types. HIR uses this exact set so an unbound body is never lowered.
    bool callableBodyAnalyzed(std::size_t memberOrdinal) const noexcept;
    bool namespaceFunctionBodyAnalyzed(std::size_t functionOrdinal) const noexcept;
    std::span<const language::EscapeClass> parameterEscapeSummary(
        std::size_t callableOrdinal) const noexcept;
    const ProjectOwnershipFlowStatistics& ownershipFlowStatistics() const noexcept;
    const ProjectEscapePropagationStatistics& escapePropagationStatistics() const noexcept;
    const ProjectCallableAnalysisStatistics& callableAnalysisStatistics() const noexcept;
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

    struct ConditionFlowStates final
    {
        OwnershipFlowSnapshot whenTrue;
        OwnershipFlowSnapshot whenFalse;
    };

    struct ExpressionResult final
    {
        std::string typeKey = "<error>";
        bool valid = false;
        bool methodGroup = false;
        // T08-F05. Set only by move(receiver.Method). The method group remains target-typed, but
        // its delegate cell becomes the unique owner of the receiver and must clear a named source.
        bool methodGroupMovesReceiver = false;
        bool boundMethodBorrowsReceiver = false;
        bool assignable = false;
        // True only for storage-backed expressions (locals, parameters, fields, raw elements,
        // and dereferences). Properties may be assignable through a setter but are not memory
        // locations and therefore cannot satisfy an inline-assembly `m` constraint.
        bool addressable = false;
        // True when the value is a struct location reached through `in`, a readonly field, or
        // the implicit receiver of a readonly struct member. Reads remain legal; writes and calls
        // to mutating instance members do not.
        bool readonlyReceiver = false;
        // Source qualifier for raw pointers. Kept outside typeKey because LLVM pointers are
        // opaque and the qualifier has no ABI/layout effect; semantic assignment/dereference
        // still observes it.
        bool pointerPointeeReadonly = false;
        bool pointerNullable = true;
        bool pointerHasProvenance = true;
        std::size_t pointerKnownAlignment = 1;
        unsigned pointerAddressSpace = 0;
        bool onePastEndAddress = false;
        bool onePastEndPointer = false;
        std::vector<std::size_t> methodCandidates;
        std::optional<ProjectIntegerConstant> integerConstant;
        std::optional<language::CallableTrait> callableTrait;
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
        bool ownsStorageDestination = false;
        sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
        sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
        std::optional<std::size_t> borrowedLocalOrdinal;
        const ast::AstNode* receiverSyntax = nullptr;
        std::string receiverTypeKey;
        std::optional<std::size_t> receiverTypeOrdinal;
        bool baseReceiver = false;
        bool extensionMethodGroup = false;
        std::optional<std::size_t> eventMemberOrdinal;
        std::optional<std::size_t> assignmentMemberOrdinal;
        // C4 "language references": the `ref`/`out`/`in` keyword written at THIS argument
        // position, if any. Value for every ordinary argument. It is checked against the
        // parameter's own pass kind after overload resolution, so the call site and the signature
        // must agree -- which is the point of requiring the keyword at all: a reader of the call
        // can see that the argument may change without going to read the signature.
        ParameterPassKind argumentPassKind = ParameterPassKind::Value;
        bool flowProvenNonNull = false;
        bool flowIntegerExcludesZero = false;
        std::optional<std::int64_t> flowIntegerMinimum;
        std::optional<std::int64_t> flowIntegerMaximum;
        std::optional<std::size_t> flowActiveVariantCase;
        bool flowRuntimeCheckElided = false;
    };

    struct ExpressionAnalysisRequest final
    {
        const ast::AstNode* syntax = nullptr;
        bool allowMethodGroup = false;
        bool allowEvent = false;
        bool allowMove = false;
        bool allowConstructorPropertyWrite = false;
    };

    struct ExpressionAnalysisCacheEntry final
    {
        ExpressionAnalysisRequest request;
        ExpressionResult result;
    };

    struct ExpressionAnalysisFrame final
    {
        ExpressionAnalysisRequest request;
        bool expanded = false;
    };

    struct IterativeExpressionAnalysis final
    {
        std::vector<ExpressionAnalysisCacheEntry> results;
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
        const ast::AstNode* syntax = nullptr;
        std::string label;
        bool isExpression = false;
        struct BreakValue final
        {
            const ast::AstNode* syntax = nullptr;
            ExpressionResult value;
        };
        std::vector<BreakValue> breakValues;
    };

    struct FinallyContext final
    {
        std::size_t cleanupRegionOrdinal = 0;
        std::size_t loopDepthAtEntry = 0;
    };

    // T06-F05. The parser lowers defer to try/finally, but the source capture boundary remains
    // normative: a deferred body may only reach outer locals/parameters named in its explicit
    // capture list. Generated aliases live immediately outside the marked body scope.
    struct DeferCaptureContext final
    {
        std::size_t boundaryScopeOrdinal = 0;
        std::unordered_set<std::string> allowedNames;
        bool capturesThis = false;
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
    std::optional<std::string> partialMovePlaceKey(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        std::vector<std::string>* fieldPath = nullptr) const;
    std::optional<std::string> storagePlaceKey(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        bool* precise = nullptr) const;
    static bool placeContains(std::string_view ancestor, std::string_view descendant) noexcept;
    static bool placesOverlap(std::string_view left, std::string_view right) noexcept;
    std::optional<std::size_t> referenceRootLocalOrdinal(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal) const noexcept;
    bool referenceLocalDescendsFrom(
        std::size_t localOrdinal,
        std::size_t possibleAncestorOrdinal) const noexcept;
    std::optional<std::size_t> conflictingMutableReference(
        std::string_view place,
        std::size_t scopeOrdinal,
        std::optional<std::size_t> accessThroughReference = std::nullopt,
        std::optional<std::size_t> reborrowSource = std::nullopt) const noexcept;
    bool placeOrAncestorIsPartiallyMoved(std::string_view place) const;
    bool placeOrAncestorIsMaybePartiallyMoved(std::string_view place) const;
    bool placeHasMovedDescendant(std::string_view place) const;
    bool placeHasMaybeMovedDescendant(std::string_view place) const;
    void reinitializePartialPlace(std::string_view place);
    MethodBorrowState localBorrowFlowState(std::size_t borrowOrdinal) const noexcept;
    void invalidateHistoricalBorrowsOfOwnerLocal(std::size_t localOrdinal);
    void invalidateHistoricalBorrowsOfOwnedParameter(std::size_t parameterOrdinal);
    bool hasActiveBorrowOfOwnerLocal(
        std::size_t localOrdinal,
        std::size_t scopeOrdinal,
        std::optional<std::string_view> accessedPlace = std::nullopt) const noexcept;
    bool hasActiveBorrowOfOwnedParameter(
        std::size_t parameterOrdinal,
        std::size_t scopeOrdinal,
        std::optional<std::string_view> accessedPlace = std::nullopt) const noexcept;
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
        std::optional<std::size_t> declaringTypeOrdinal,
        std::size_t callableOrdinal) const;

    // Adaptador que le da al evaluador compartido la resolucion de miembros y de tipos de ESTE
    // modelo, sin que el evaluador tenga que conocer el modelo entero.
    class ConstantMemberResolver final : public ProjectConstantMemberResolver
    {
    public:
        ConstantMemberResolver(
            const ProjectMethodBodyModel& model,
            std::size_t callableOrdinal) noexcept
            : model_(model), callableOrdinal_(callableOrdinal)
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
        std::size_t callableOrdinal_ = 0;
    };

    const MethodNameBinding* recordedNameBinding(
        const ast::AstNode* syntax,
        std::size_t callableOrdinal) const noexcept;
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
    bool validateErrorPropagation(
        std::string_view errorTypeKey,
        const ast::AstNode* syntax,
        std::size_t callerCallableOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string_view operation);
    bool errorIsHandled(std::string_view errorTypeKey) const;
    std::string delegateErrorTypeKey(std::string_view delegateTypeKey) const;
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
        bool allowMove = false,
        bool allowConstructorPropertyWrite = false);
    ExpressionResult analyzeExpressionCore(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionIteratively(
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool shouldAnalyzeExpressionIteratively(const ExpressionAnalysisRequest& request) const;
    void appendIterativeExpressionChildren(
        const ExpressionAnalysisRequest& request,
        std::vector<ExpressionAnalysisRequest>& children) const;
    const ExpressionResult* cachedExpressionResult(
        const ExpressionAnalysisRequest& request) const noexcept;
    ExpressionResult analyzeIdentifier(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeNamespaceState(
        const ast::AstNode* expression,
        const NamespaceStateReferenceBinding& reference,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeMember(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeCall(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeMatchExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeLoopExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeNamespaceFunctionCall(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        const ExpressionResult* forwardedReceiver = nullptr);
    ExpressionResult analyzeNamespaceFunctionGroup(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
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
        bool diagnoseFailure = true,
        bool readonlyReceiver = false);
    std::vector<std::size_t> stage38AccessorCandidates(
        std::size_t receiverTypeOrdinal,
        std::string_view accessorName,
        bool propertyAccessor,
        bool getter,
        bool typeReceiver,
        std::size_t accessingTypeOrdinal,
        std::size_t protectedReceiverTypeOrdinal) const;
    std::vector<std::size_t> stage38OperatorCandidates(
        std::string_view operatorTraitKey,
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
    bool contextHasReadonlyReceiver(std::size_t memberOrdinal) const noexcept;
    bool readonlyFieldAssignable(
        const ClassMemberSymbol& field,
        std::size_t contextMemberOrdinal) const noexcept;
    std::optional<std::size_t> automaticPropertyBackingField(
        std::size_t typeOrdinal,
        std::string_view propertyName,
        bool isStatic) const noexcept;
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
    bool methodGroupExactlyMatchesDelegate(
        const ExpressionResult& methodGroup,
        std::string_view delegateTypeKey) const;
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
    bool reportTupleConversionFailure(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ProjectImplicitConversion classifyCVariadicPromotion(std::string_view sourceTypeKey) const;
    std::string implicitThisTypeKey(std::size_t methodMemberOrdinal) const;
    // C4 "language references": enforce that a call writes the same `ref`/`out`/`in` keyword the
    // selected member's parameters declare.
    void checkArgumentPassKinds(
        const ast::AstNode* syntax,
        std::string_view methodName,
        std::size_t memberOrdinal,
        const std::vector<ExpressionResult>& arguments,
        diagnostics::DiagnosticEngine& diagnostics,
        std::span<const std::size_t> parameterOrdinals = {}) const;
    bool parameterMayCarryEscape(const MethodParameterSymbol& parameter) const noexcept;
    void noteParameterEscape(
        std::size_t callableOrdinal,
        std::size_t parameterOrdinal,
        language::EscapeClass escapeClass);
    void finalizeEscapeSummaries(diagnostics::DiagnosticEngine& diagnostics);
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
    bool aggregateRequiresActiveMember(std::string_view typeKey) const noexcept;
    // NSC-LIMIT-004. ¿Esta clave nombra un STRUCT? Ni una clase, ni una interfaz, ni un enum, ni
    // un delegado, ni un `T[N]`.
    //
    // No se reutiliza `typeKeyHasDefinedDefaultStorage` --que seria lo cercano-- porque esa
    // tambien cubre los arreglos de tamanio fijo, y un `int[4] == int[4]` no es lo que este ID
    // define. La pregunta aqui es exactamente la que hace el camino de `Struct.Equals`.
    bool typeKeyIsStruct(std::string_view typeKey) const noexcept;
    bool aggregateAllowsSynthesizedMember(
        std::string_view typeKey,
        std::string_view memberName) const noexcept;
    // NativeSharp 3.0 records synthesize field-wise operations only when every value nested in
    // the physical aggregate can honor the same contract. Equality/hash cannot tunnel through a
    // record that did not opt in, and Clone is a plain value copy so it cannot duplicate a
    // destructor or owned storage. `reason` names the first offending field path.
    bool aggregateSupportsSynthesizedMember(
        std::string_view typeKey,
        std::string_view memberName,
        std::string& reason) const;
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
    // T01-F02. An expanded params ReadOnlySpan<T> owns no heap storage: its elements live in an
    // entry-block stack slot in the caller. A view returned by the callee must therefore retain
    // that frame-bound origin instead of looking like an unrooted borrowed-returning call.
    bool expressionUsesExpandedParameterPack(
        const ast::AstNode* expression,
        std::size_t callableOrdinal) const;
    std::optional<std::size_t> variableArrayRootLocalOfExpression(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal) const;
    MethodEscapeOrigin referenceEscapeOriginOfExpression(
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
    std::optional<std::vector<std::string>> aggregateElementNames(
        std::string_view typeKey) const;
    ExpressionResult bindAggregateInitializer(
        const ast::AstNode* syntax,
        const std::string& targetTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMoveSpread = false);
    std::optional<MethodDeconstructionBinding> bindDeconstructionSource(
        const ast::AstNode* syntax,
        const ast::AstNode* sourceSyntax,
        const ExpressionResult& source,
        std::size_t methodMemberOrdinal,
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
    std::optional<MethodOptionalCarrierBinding> resolveOptionalCarrier(
        std::string_view typeKey) const;
    std::optional<MethodResultPropagationBinding> resolveResultCarrier(
        std::string_view typeKey) const;
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
    std::span<const ClassMemberSymbol> namespaceCallableAdapters() const noexcept;
    bool callableBelongsToGenericTypeSpecialization(
        std::size_t methodMemberOrdinal) const noexcept;
    bool betterCandidate(
        const MethodCallCandidate& left,
        const MethodCallCandidate& right) const;
    void recordExpressionType(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        const ExpressionResult& result);
    MethodExpressionTypeBinding makeExpressionTypeBinding(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        const ExpressionResult& result) const;
    void recordConstructionSite(ProjectConstructionSite site);
    void recordCallBinding(MethodCallBinding binding);
    void recordNameBinding(MethodNameBinding binding);
    bool deferCaptureAllowsLocal(
        std::string_view name, std::size_t localScopeOrdinal) const noexcept;
    bool deferCaptureAllowsParameter(std::string_view name) const noexcept;
    bool deferCaptureAllowsThis() const noexcept;
    void reportPrevious(
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string message) const;
    bool callableCanBeAnalyzed(const ProjectCallableAnalysisContext& context) const;
    void resetCallableScratch(const ClassMemberSymbol& callable);
    std::vector<std::size_t> createOwnedParameterCleanups(
        const ClassMemberSymbol& callable,
        std::size_t callableOrdinal,
        std::size_t rootScopeOrdinal);
    void analyzeConstructorInitializer(
        const ClassMemberSymbol& callable,
        std::size_t callableOrdinal,
        std::size_t rootScopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void closeOwnedParameterCleanups(
        std::span<const std::size_t> cleanupOrdinals,
        const ClassMemberSymbol& callable,
        std::size_t callableOrdinal,
        std::size_t rootScopeOrdinal);
    void analyzeCallable(
        const ProjectCallableAnalysisContext& context,
        diagnostics::DiagnosticEngine& diagnostics);
    [[nodiscard]] std::unique_ptr<ProjectMethodBodyModel> createCallableShard(
        const ProjectCallableAnalysisContext& context) const;
    [[nodiscard]] ProjectCallableAnalysisResult analyzeCallableIsolated(
        const ProjectCallableAnalysisContext& context) const;
    static void collectCallableDiscoveries(ProjectCallableAnalysisResult& result);

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectNamespaceFunctionModel* namespaceFunctionModel_ = nullptr;
    std::size_t namespaceCallableBase_ = 0;
    std::vector<ClassMemberSymbol> namespaceCallableAdapters_;
    std::span<const ClassMemberSymbol> namespaceCallableAdaptersView_;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectGenericModel* genericModel_ = nullptr;
    const ProjectGenericConstraintModel* genericConstraintModel_ = nullptr;
    std::vector<MethodScope> scopes_;
    std::vector<std::vector<ScopeName>> scopeNames_;
    std::vector<MethodLocalSymbol> locals_;
    std::vector<MethodNameBinding> nameBindings_;
    ProjectCallableBindingIndex nameBindingIndex_;
    std::vector<MethodShadowingEvent> shadowingEvents_;
    std::vector<MethodExpressionTypeBinding> expressionTypes_;
    ProjectCallableBindingIndex expressionTypeBindingIndex_;
    std::vector<ProjectConstructionSite> constructionSites_;
    ProjectCallableBindingIndex constructionSiteIndex_;
    std::unordered_map<std::size_t, std::size_t> nextConstructionSequenceByCallable_;
    std::vector<MethodAggregateInitializerBinding> aggregateInitializerBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> aggregateInitializerBindingBySyntax_;
    std::vector<MethodDeconstructionBinding> deconstructionBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> deconstructionBindingBySyntax_;
    std::vector<MethodMatchExpressionBinding> matchExpressionBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> matchExpressionBindingBySyntax_;
    std::vector<MethodLoopExpressionBinding> loopExpressionBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> loopExpressionBindingBySyntax_;
    std::vector<MethodLoopBreakBinding> loopBreakBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> loopBreakBindingBySyntax_;
    std::vector<ExpressionResult> activeMatchValues_;
    std::unordered_map<const ast::AstNode*, std::string> staticInterpolationValues_;
    std::vector<MethodRuntimeTypeBinding> runtimeTypeBindings_;
    std::vector<MethodCoreBinding> coreBindings_;
    std::vector<MethodVariableArrayIndexBinding> variableArrayIndexBindings_;
    std::vector<MethodExceptionBinding> exceptionBindings_;
    std::vector<MethodCleanupRegion> cleanupRegions_;
    std::vector<MethodUsingBinding> usingBindings_;
    std::vector<MethodMoveBinding> moveBindings_;
    std::vector<MethodOwnershipTransferBinding> ownershipTransferBindings_;
    std::vector<MethodBorrowBinding> borrowBindings_;
    std::vector<MethodLocalBorrowBinding> localBorrowBindings_;
    std::vector<MethodBorrowedParameterReassignmentBinding>
        borrowedParameterReassignmentBindings_;
    // T09-F04. A ref/ref-mut foreach lends element storage from its source for the whole loop.
    // Keep that lexical loan active while the synthesized candidate block is analyzed so an
    // assignment or mutating receiver call cannot invalidate the iterator behind the yielded
    // reference. The place key is the original source, not the hidden foreach snapshot.
    struct ActiveIterationBorrow final
    {
        std::string sourcePlace;
        bool mutableElements = false;
    };
    std::vector<ActiveIterationBorrow> activeIterationBorrows_;
    std::unordered_set<std::size_t> movedRaiiLocals_;
    std::unordered_set<std::size_t> maybeMovedRaiiLocals_;
    std::unordered_set<std::size_t> movedOwnedParameters_;
    std::unordered_set<std::size_t> maybeMovedOwnedParameters_;
    std::unordered_set<std::size_t> invokedFnOnceLocals_;
    std::unordered_set<std::size_t> maybeInvokedFnOnceLocals_;
    std::unordered_set<std::string> partiallyMovedPlaces_;
    std::unordered_set<std::string> maybePartiallyMovedPlaces_;
    std::unordered_set<std::size_t> invalidLocalBorrows_;
    std::unordered_set<std::size_t> maybeInvalidLocalBorrows_;
    // C7 "definite assignment": the locals DEFINITELY assigned at the current point of the walk.
    // It travels with the ownership flow state (see OwnershipFlowSnapshot) because it needs
    // exactly the same control-flow discipline -- it is joined by INTERSECTION at every merge, so
    // a local assigned on only one arm of an `if` is not assigned after it.
    std::unordered_set<std::size_t> assignedLocals_;
    std::unordered_map<std::size_t, std::size_t> activeUnionMembers_;
    std::unordered_set<std::size_t> nonNullStorages_;
    std::unordered_map<std::size_t, OwnershipFlowIntegerRange> integerRanges_;
    std::unordered_map<std::size_t, std::size_t> activeVariantCases_;
    // State on each truth edge of a boolean expression. A collapsed post-expression snapshot is
    // insufficient for `if (a || Fill(out x)) return; use(x);`: the continuing false edge proves
    // the RHS ran even though the join of true+false does not. Entries are private analysis
    // scratch, keyed by syntax and rebuilt for each callable.
    std::unordered_map<const ast::AstNode*, ConditionFlowStates> conditionFlowBySyntax_;
    // The identifier syntax node currently being written to rather than read from. An assignment
    // analyzes its own left-hand side through the ordinary expression path, so without this the
    // very statement that assigns a local would be reported as a read of it.
    const ast::AstNode* definiteAssignmentWriteTarget_ = nullptr;
    const ast::AstNode* activeUnionWriteTarget_ = nullptr;
    std::size_t unsafeBlockDepth_ = 0;
    // One diagnostic per offending read: a statement can be analyzed more than once (the
    // unreachable-code probe in analyzeBlock does exactly that), and that must not multiply the
    // same message.
    std::unordered_set<const ast::AstNode*> reportedUnassignedReads_;
    bool inUnevaluatedOperand_ = false;
    bool formingAddressOperand_ = false;
    bool ownershipFlowReachable_ = true;
    // While a MemberExpression recursively analyzes its receiver, a partially moved aggregate
    // root may be inspected only long enough to select a still-live field. The completed member
    // path is validated immediately afterward; methods/properties still require the whole value.
    std::size_t partialProjectionReceiverDepth_ = 0;
    // The direct member operand of move(...) must be bound as a place before its moved-state is
    // diagnosed. The move rule then distinguishes an already-moved path from a maybe-moved path
    // and emits one precise diagnostic; ordinary reads never enter this context.
    std::size_t partialMoveSourceDepth_ = 0;
    IterativeExpressionAnalysis* iterativeExpressionAnalysis_ = nullptr;
    std::vector<OwnershipFlowSnapshot> throwFlowStates_;
    struct ActiveErrorHandlers final
    {
        bool catchAll = false;
        std::vector<std::string> typeKeys;
    };
    struct ActiveTryErrorSet final
    {
        std::vector<std::string> typeKeys;
    };
    std::vector<ActiveErrorHandlers> activeErrorHandlers_;
    std::vector<ActiveTryErrorSet> activeTryErrorSets_;
    std::vector<std::string> currentCatchErrorTypes_;
    std::vector<MethodSwitchBinding> switchBindings_;
    std::vector<MethodStructuredExitBinding> structuredExits_;
    std::vector<MethodCleanupContinuation> cleanupContinuations_;
    std::vector<std::size_t> activeCleanupRegions_;
    std::vector<LoopContext> loopStack_;
    std::vector<FinallyContext> finallyStack_;
    std::vector<DeferCaptureContext> deferCaptureStack_;
    std::vector<MethodLambdaBinding> lambdaBindings_;
    std::vector<MethodLambdaValueBinding> lambdaValueBindings_;
    std::vector<MethodLambdaBinding*> activeLambdaBindings_;
    std::unordered_map<std::size_t, std::size_t> lambdaByNameBindingOrdinal_;
    std::size_t nextLambdaOrdinal_ = 0;
    std::vector<MethodDelegateBinding> delegateBindings_;
    std::optional<std::size_t> currentLambdaOrdinal_;
    std::string currentLambdaReturnTypeKey_;
    std::string currentLambdaErrorTypeKey_;
    std::size_t currentCatchDepth_ = 0;
    std::vector<MethodEventBinding> eventBindings_;
    std::vector<MethodOptionalCarrierBinding> optionalCarrierBindings_;
    std::vector<MethodResultPropagationBinding> resultPropagationBindings_;
    std::vector<MethodRangeSliceBinding> rangeSliceBindings_;
    std::vector<MethodCallBinding> calls_;
    ProjectCallableBindingIndex callBindingIndex_;
    std::vector<NamespaceFunctionCallBinding> namespaceFunctionCalls_;
    std::unordered_map<const ast::AstNode*, std::size_t> namespaceFunctionCallBySyntax_;
    std::vector<std::vector<language::EscapeClass>> parameterEscapeSummaries_;
    std::size_t currentCallableLocalBase_ = 0;
    std::size_t currentCallableParameterCount_ = 0;
    mutable ProjectOwnershipFlowStatistics ownershipFlowStatistics_;
    mutable ProjectEscapePropagationStatistics escapePropagationStatistics_;
    ProjectCallableAnalysisStatistics callableAnalysisStatistics_;
    bool built_ = false;
    language::LanguageEdition languageEdition_ = language::LanguageEdition::V2;
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
const char* methodLambdaCaptureModeName(language::CaptureMode mode) noexcept;
const char* methodLambdaCallableTraitName(language::CallableTrait trait) noexcept;
const char* methodBorrowKindName(MethodBorrowKind kind) noexcept;
const char* methodLocalBorrowSourceKindName(MethodLocalBorrowSourceKind kind) noexcept;
const char* methodBorrowStateName(MethodBorrowState state) noexcept;
const char* methodLambdaCaptureKindName(MethodLambdaCaptureKind kind) noexcept;
const char* methodLambdaCaptureSourceKindName(MethodLambdaCaptureSourceKind kind) noexcept;
const char* methodDelegateOperationKindName(MethodDelegateOperationKind kind) noexcept;
const char* methodEventOperationKindName(MethodEventOperationKind kind) noexcept;
}
