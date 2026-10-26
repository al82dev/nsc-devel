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
#include "nativesharp/Frontend/MethodScopeKind.h"
#include "nativesharp/Frontend/MethodScope.h"
#include "nativesharp/Frontend/MethodLocalSymbol.h"
#include "nativesharp/Frontend/MethodNameBindingKind.h"
#include "nativesharp/Frontend/MethodNameBinding.h"
#include "nativesharp/Frontend/MethodShadowingKind.h"
#include "nativesharp/Frontend/MethodShadowingEvent.h"
#include "nativesharp/Frontend/OwnershipArgumentFit.h"
#include "nativesharp/Frontend/MethodAggregateInitializerEntryBinding.h"
#include "nativesharp/Frontend/MethodAggregateInitializerBinding.h"
#include "nativesharp/Frontend/MethodDeconstructionBinding.h"
#include "nativesharp/Frontend/MethodMatchExpressionBinding.h"
#include "nativesharp/Frontend/MethodLoopExpressionBinding.h"
#include "nativesharp/Frontend/MethodLoopBreakBinding.h"
#include "nativesharp/Frontend/MethodRuntimeTypeOperationKind.h"
#include "nativesharp/Frontend/MethodRuntimeTypeBinding.h"
#include "nativesharp/Frontend/MethodExceptionOperationKind.h"
#include "nativesharp/Frontend/MethodExceptionBinding.h"
#include "nativesharp/Frontend/MethodCleanupRegionKind.h"
#include "nativesharp/Frontend/MethodCleanupRegion.h"
#include "nativesharp/Frontend/MethodMoveBinding.h"
#include "nativesharp/Frontend/MethodOwnershipTransferKind.h"
#include "nativesharp/Frontend/MethodOwnershipTransferBinding.h"
#include "nativesharp/Frontend/MethodBorrowKind.h"
#include "nativesharp/Frontend/MethodBorrowBinding.h"
#include "nativesharp/Frontend/MethodLocalBorrowSourceKind.h"
#include "nativesharp/Frontend/MethodBorrowState.h"
#include "nativesharp/Frontend/MethodLocalBorrowBinding.h"
#include "nativesharp/Frontend/MethodBorrowedParameterReassignmentBinding.h"
#include "nativesharp/Frontend/MethodUsingBinding.h"
#include "nativesharp/Frontend/MethodSwitchSectionBinding.h"
#include "nativesharp/Frontend/MethodSwitchBinding.h"
#include "nativesharp/Frontend/MethodStructuredExitKind.h"
#include "nativesharp/Frontend/MethodStructuredExitBinding.h"
#include "nativesharp/Frontend/MethodCleanupContinuationKind.h"
#include "nativesharp/Frontend/MethodCleanupContinuation.h"
#include "nativesharp/Frontend/MethodCoreOperationKind.h"
#include "nativesharp/Frontend/MethodCoreBinding.h"
#include "nativesharp/Frontend/MethodVariableArrayIndexBinding.h"
#include "nativesharp/Frontend/MethodLambdaCaptureKind.h"
#include "nativesharp/Frontend/MethodLambdaCaptureSourceKind.h"
#include "nativesharp/Frontend/MethodLambdaCapture.h"
#include "nativesharp/Frontend/MethodLambdaValueBinding.h"
#include "nativesharp/Frontend/MethodLambdaBinding.h"
#include "nativesharp/Frontend/MethodDelegateOperationKind.h"
#include "nativesharp/Frontend/MethodDelegateBinding.h"
#include "nativesharp/Frontend/MethodEventOperationKind.h"
#include "nativesharp/Frontend/MethodEventBinding.h"
#include "nativesharp/Frontend/MethodOptionalCarrierBinding.h"
#include "nativesharp/Frontend/MethodResultPropagationBinding.h"
#include "nativesharp/Frontend/MethodRangeSliceBinding.h"
#include "nativesharp/Frontend/MethodCallResolutionKind.h"
#include "nativesharp/Frontend/MethodCallCandidate.h"
#include "nativesharp/Frontend/MethodCallBinding.h"
#include "nativesharp/Frontend/NamespaceFunctionCallBinding.h"
#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectConstructionSite.h"
#include "nativesharp/Frontend/ProjectOwnershipFlowStatistics.h"
#include "nativesharp/Frontend/ProjectEscapePropagationStatistics.h"
#include "nativesharp/Frontend/ProjectMethodBodyBuildOptions.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/MatchArmCoverage.h"
#include "nativesharp/Language/PointerContract.h"
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
#include <variant>
#include <vector>
#include <array>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct PatternWork;
class ProjectConversionClassifier;

struct ProjectOopHirModelTestAccess;

class ProjectGenericConstraintModel;
class ProjectGenericModel;
class ProjectReferenceEscapeEvaluator;
class ProjectCallableResultMerger;
class ProjectCallableWaveReducer;











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





// NativeSharp 3.0 T02-F03. A deconstruction always consumes one tuple carrier. For a native
// tuple the carrier is the source expression itself; for a nominal source it is the result of the
// statically selected, parameterless instance Deconstruct() method recorded here. Keeping this
// decision in sema prevents HIR from repeating member lookup or changing overload/access rules.




// T07-F05. One semantic contract for the value-producing loop and one for each break that exits
// it. Keeping the conversion on the break syntax means HIR never repeats type unification and a
// labelled break can target an outer loop without relying on the lowering stack's top frame.







































// `switch` / `case`. The body model is where a case label stops being an expression: it is folded
// to an integer, checked against the governing type, and checked for duplicates. Everything
// downstream reads these numbers, so no later pass has to know how to evaluate a constant.






































// E017 carrier bridge. `??` recognizes only the public NativeSharp.Core.Optional<T> protocol;
// lowering calls these methods and never reaches into the carrier's private fields or offsets.


// NativeSharp 3.0 T07-F01. Postfix `?` is bound against the compiler-owned
// two-case variant protocol instead of a type name. Keeping the complete
// protocol here lets every later layer consume one semantic decision.


// NativeSharp 3.0 T01-F04. A range index is structural sugar over two public instance
// methods on the receiver: `int Length()` and `Slice(int start, int length)`.  The binding
// keeps both selected methods together because lowering must evaluate the receiver exactly once
// and reuse that physical value for both calls.  Endpoints remain source syntax so their normal
// evaluation order and conversions survive into HIR.










class ProjectMethodBodyModel final
{
    friend struct ProjectOopHirModelTestAccess;
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
        const ProjectMethodBodyBuildOptions& options = {});

    std::span<const MethodScope> scopes() const noexcept;
    std::span<const MethodLocalSymbol> locals() const noexcept;
    const std::vector<const ast::AstNode*>& awaitExpressions() const noexcept;
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
    // EN: Freeze 3.0 (owned arrays): `delete[]` of an owned array field releases it and clears the
    //     field, so the container's destruction entry, which now releases owned array fields too,
    //     never releases the same block again.
    // ES: Congelamiento 3.0 (arreglos owned): `delete[]` de un campo de arreglo owned lo libera y
    //     deja el campo vacio, asi la entrada de destruccion del contenedor, que ahora tambien libera
    //     los campos de arreglo owned, nunca libera otra vez el mismo bloque.
    bool arrayDeleteClearsOwnedField(const ast::AstNode* syntax) const noexcept
    {
        return arrayDeleteClearsOwnedField_.contains(syntax);
    }
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
    bool aggregateAllowsSynthesizedMember(
        std::string_view typeKey,
        std::string_view memberName) const noexcept;
    std::span<const language::EscapeClass> parameterEscapeSummary(
        std::size_t callableOrdinal) const noexcept;
    const ProjectOwnershipFlowStatistics& ownershipFlowStatistics() const noexcept;
    const ProjectEscapePropagationStatistics& escapePropagationStatistics() const noexcept;
    const ProjectCallableAnalysisStatistics& callableAnalysisStatistics() const noexcept;
    bool isBuilt() const noexcept;
    // NS304-T08-F04: resolves a callable ordinal to a class member or a namespace-callable adapter.
    const ClassMemberSymbol* callableSymbol(std::size_t ordinal) const noexcept;
    // EN: Freeze 3.0 C-13: true when `errorTypeKey` is caught by one of `handlers` (catch types,
    //     "<any-exception>" for a catch-all) or propagates through `enclosingErrorTypeKey`.
    // ES: Congelamiento 3.0 C-13: verdadero si `errorTypeKey` lo atrapa uno de `handlers` (tipos de
    //     catch, "<any-exception>" para un catch-all) o se propaga por `enclosingErrorTypeKey`.
    bool declaredErrorIsCovered(
        std::string_view errorTypeKey,
        std::span<const std::string> handlers,
        std::string_view enclosingErrorTypeKey) const;

private:
#include "nativesharp/Frontend/Internal/MethodBody/ScopeNameKind.h"
#include "nativesharp/Frontend/Internal/MethodBody/ScopeName.h"
#include "nativesharp/Frontend/Internal/MethodBody/ConditionFlowStates.h"
#include "nativesharp/Frontend/Internal/MethodBody/ExpressionResult.h"
#include "nativesharp/Frontend/Internal/MethodBody/ExpressionAnalysisRequest.h"
#include "nativesharp/Frontend/Internal/MethodBody/ExpressionAnalysisCacheKey.h"
#include "nativesharp/Frontend/Internal/MethodBody/ExpressionAnalysisCacheKeyHash.h"
#include "nativesharp/Frontend/Internal/MethodBody/ExpressionAnalysisCacheEntry.h"
#include "nativesharp/Frontend/Internal/MethodBody/ExpressionAnalysisFrame.h"
#include "nativesharp/Frontend/Internal/MethodBody/IterativeExpressionAnalysis.h"
#include "nativesharp/Frontend/Internal/MethodBody/LoopContext.h"
#include "nativesharp/Frontend/Internal/MethodBody/FinallyContext.h"
#include "nativesharp/Frontend/Internal/MethodBody/DeferCaptureContext.h"
    std::size_t createScope(
        MethodScopeKind kind,
        std::size_t methodMemberOrdinal,
        std::optional<std::size_t> parentScopeOrdinal);
    bool scopeContains(std::size_t ancestorScopeOrdinal, std::size_t scopeOrdinal) const noexcept;
    OwnershipFlowSnapshot captureOwnershipFlow() const;
    void restoreOwnershipFlow(const OwnershipFlowSnapshot& state);
    void restoreLocalOwnershipFlags(const OwnershipFlowSnapshot& state);
    void restoreParameterOwnershipFlags(const OwnershipFlowSnapshot& state);
    void restoreAssignedLocals(const OwnershipFlowSnapshot& state);
    void restoreOwnershipStorageFacts(
        const OwnershipFlowSnapshot& state,
        std::span<const std::pair<std::size_t, std::size_t>> activeAfterSnapshot);
    OwnershipFlowSnapshot joinOwnershipFlow(
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right) const;
    static void mergeRequiredOwnershipExits(
        OwnershipFlowSnapshot& result,
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right);
    static std::uint8_t joinedLocalFlagsAt(
        const OwnershipFlowSnapshot& snapshot,
        std::size_t ordinal) noexcept;
    static std::uint8_t joinOwnershipTriStateBits(
        std::uint8_t leftFlags,
        std::uint8_t rightFlags,
        std::uint8_t definiteBit,
        std::uint8_t maybeBit) noexcept;
    static void joinLocalOwnershipFlags(
        OwnershipFlowSnapshot& result,
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right);
    static void joinParameterOwnershipFlags(
        OwnershipFlowSnapshot& result,
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right);
    static void joinPartialOwnershipState(
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right,
        OwnershipFlowSnapshot& result);
    static void joinRequiredOwnershipFields(
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right,
        OwnershipFlowSnapshot& result);
    static void joinOwnershipStorageFact(
        OwnershipFlowSnapshot& result,
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right,
        std::size_t storageKey);
    static void joinOwnershipStorageFacts(
        OwnershipFlowSnapshot& result,
        const OwnershipFlowSnapshot& left,
        const OwnershipFlowSnapshot& right);

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
    std::optional<std::int64_t> storageIntegerConstant(
        const ast::AstNode* syntax,
        std::size_t scopeOrdinal,
        std::size_t callableOrdinal) const;
    std::string_view recordedExpressionTypeKey(
        const ast::AstNode* syntax,
        std::size_t callableOrdinal) const;
    std::optional<std::string> storagePlaceKeyForReferenceCall(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        bool* precise,
        bool& handled) const;
    std::optional<std::string> storageIdentifierPlaceKey(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        bool* precise) const;
    std::optional<std::string> storageMemberPlaceKey(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        std::size_t callableOrdinal,
        bool* precise) const;
    void appendStorageIndexSegment(
        std::string& key,
        const ast::AstNode* index,
        std::size_t scopeOrdinal,
        std::size_t callableOrdinal,
        bool structurallySplittable,
        bool& indicesPrecise) const;
    std::optional<std::string> storageIndexPlaceKey(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        std::size_t callableOrdinal,
        bool* precise) const;
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
    struct BlockFrame;
    enum class IfStage;
    struct IfFrame;
    enum class LoopStage;
    struct LoopFrame;
    enum class ForStage;
    struct ForFrame;
    enum class ForeachStage;
    struct ForeachFrame;
    enum class UsingStage;
    struct UsingFrame;
    enum class SwitchStage;
    struct SwitchFrame;
    enum class TryStage;
    struct TryFrame;
    using StatementFrame = std::variant<
        BlockFrame,
        IfFrame,
        LoopFrame,
        ForFrame,
        ForeachFrame,
        UsingFrame,
        SwitchFrame,
        TryFrame>;
    void pushBlockFrame(
        std::vector<StatementFrame>& frames,
        const ast::AstNode* nestedBlock,
        std::size_t parentScope,
        bool createScopeForBlock,
        std::size_t methodMemberOrdinal,
        std::optional<OwnershipFlowSnapshot> restoreAfterExit = std::nullopt);
    void processIfFrame(
        std::vector<StatementFrame>& frames,
        IfFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void beginLoopFrame(
        std::vector<StatementFrame>& frames,
        LoopFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void finishLoopFrame(
        std::vector<StatementFrame>& frames,
        LoopFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processLoopFrame(
        std::vector<StatementFrame>& frames,
        LoopFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void beginForFrame(
        std::vector<StatementFrame>& frames,
        ForFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void finishForFrame(
        std::vector<StatementFrame>& frames,
        ForFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processForFrame(
        std::vector<StatementFrame>& frames,
        ForFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processForeachFrame(
        std::vector<StatementFrame>& frames,
        ForeachFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processUsingFrame(
        std::vector<StatementFrame>& frames,
        UsingFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processSwitchFrame(
        std::vector<StatementFrame>& frames,
        SwitchFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processTryFrame(
        std::vector<StatementFrame>& frames,
        TryFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processBlockStatement(
        std::vector<StatementFrame>& frames,
        BlockFrame& frame,
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateBlockScopedBorrowExit(
        const ast::AstNode* block,
        std::size_t activeScope,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::vector<language::MatchArmCoverage> parseBlockMatchCoverage(
        const ast::AstNode* block) const;
    std::vector<std::string> blockMatchCoverageDomain(
        const ast::AstNode* block,
        std::size_t activeScope) const;
    static bool matchCoverageHasListLength(
        const std::vector<language::MatchArmCoverage>& arms) noexcept;
    static bool matchCoverageHasUnguardedWildcard(
        const std::vector<language::MatchArmCoverage>& arms) noexcept;
    void reportOpenBlockMatchCoverage(
        const ast::AstNode* block,
        const std::vector<language::MatchArmCoverage>& arms,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void reportClosedBlockMatchCoverage(
        const ast::AstNode* block,
        const std::vector<language::MatchArmCoverage>& arms,
        const std::vector<std::string>& domain,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void analyzeBlockMatchCoverage(
        const ast::AstNode* block,
        std::size_t activeScope,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool blockMatchIsExhaustive(const ast::AstNode* block, std::size_t activeScope) const;
    void recordMatchArmExit(const ast::AstNode* block);
    void closeExhaustiveMatch(const ast::AstNode* block, std::size_t activeScope);
    void finishBlockFrame(
        std::vector<StatementFrame>& frames,
        BlockFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void processBlockFrame(
        std::vector<StatementFrame>& frames,
        BlockFrame& frame,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeStructuredStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeTerminalStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeStandaloneStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeDeconstructionDeclarationStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool handleDeconstructionPatternShape(
        const PatternWork& work,
        std::vector<PatternWork>& patternWork,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void declareDeconstructionPatternBinding(
        PatternWork& work,
        std::unordered_set<std::string>& declaredHere,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeIfStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeWhileStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeWhileCondition(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isDoWhile);
    OwnershipFlowSnapshot analyzeWhileBody(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        LoopContext& loopContext);
    OwnershipFlowSnapshot analyzeDoWhileCondition(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        const OwnershipFlowSnapshot& backEdgeState);
    void validateWhileLocalBackEdge(
        const ast::AstNode* statement,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        std::size_t localCountAtBodyEntry,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateWhileParameterBackEdge(
        const ast::AstNode* statement,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        const ClassMemberSymbol* loopMethod,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateWhilePartialMoveBackEdge(
        const ast::AstNode* statement,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        diagnostics::DiagnosticEngine& diagnostics) const;
    OwnershipFlowSnapshot completeWhileFlow(
        bool isDoWhile,
        bool conditionAlwaysTrue,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        const OwnershipFlowSnapshot& afterCondition,
        const LoopContext& loopContext) const;
    void analyzeReturnStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeStaticAssertStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeInlineAssemblyStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeInlineAssemblyOperand(
        const ast::AstNode* operand,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeYieldStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeSuspendStatement(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeVariable(
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool inferVariableType(
        std::optional<ExpressionResult>& precomputedVarInitializer,
        std::string& varInferredTypeKey,
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isVarInferred);
    bool analyzeVariableLengthArray(
        bool& isVariableLengthArray,
        std::string& elementTypeKey,
        const ast::AstNode*& countSyntax,
        std::vector<const ast::AstNode*>& extentSyntaxes,
        std::vector<std::string>& extentTypeKeys,
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isVarInferred);
    bool inspectVariableLengthArrayModifiers(
        std::size_t& fixedArrayModifierCount,
        bool& hasRuntimeExtent,
        const ast::AstNode* variable,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool resolveVariableLengthArrayShape(
        std::string& elementTypeKey,
        std::vector<const ast::AstNode*>& extentSyntaxes,
        const ast::AstNode* variable,
        std::size_t fixedArrayModifierCount,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateVariableLengthArrayElements(
        std::vector<std::string>& extentTypeKeys,
        const std::string& elementTypeKey,
        const std::vector<const ast::AstNode*>& extentSyntaxes,
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool classifyVariableReferenceLocal(
        bool& isPinnedReferenceLocal,
        bool& isReadonlyReference,
        bool& isPinnedLocal,
        const ast::AstNode* variable) const;
    bool analyzeVariablePreamble(
        std::optional<ExpressionResult>& precomputedVarInitializer,
        std::string& varInferredTypeKey,
        bool& isVariableLengthArray,
        std::string& variableLengthArrayElementTypeKey,
        const ast::AstNode*& variableLengthArrayCountSyntax,
        std::vector<const ast::AstNode*>& variableLengthArrayExtentSyntaxes,
        std::vector<std::string>& variableLengthArrayExtentTypeKeys,
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isVarInferred);
    void validateVariableStorage(
        const ast::AstNode* variable,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isStaticLocal,
        bool isReferenceLocal,
        bool isVariableLengthArray) const;
    bool variableRequiresRaii(
        const ast::AstNode* variable,
        std::string_view typeKey,
        bool isForeachEnumerator,
        bool isDeferValueCapture,
        bool ownedQualifierOwnsStorage,
        bool managedDelegate) const;
    void validateVariableQualifiers(
        const ast::AstNode* variable,
        std::string_view typeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        sema::OwnershipKind declaredOwnership,
        bool isReferenceLocal,
        bool isReadonlyReference,
        bool isForeachEnumerator,
        bool isRaii,
        bool isBorrowed,
        bool isVolatile,
        bool managedDelegate,
        bool raiiOwnsNativeArrayBuffer,
        const char* ownerSpelling) const;
    void validateVariableStorageQualifiers(
        const ast::AstNode* variable,
        std::string_view typeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        sema::OwnershipKind declaredOwnership,
        bool isReferenceLocal,
        bool isReadonlyReference,
        bool isRaii,
        bool isBorrowed,
        bool isVolatile) const;
    void validateVariableLifetimeQualifiers(
        const ast::AstNode* variable,
        std::string_view typeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isForeachEnumerator,
        bool isRaii,
        bool isBorrowed,
        bool managedDelegate,
        bool raiiOwnsNativeArrayBuffer,
        const char* ownerSpelling) const;
    ExpressionResult evaluateVariableInitializer(
        std::optional<ExpressionResult>& precomputedVarInitializer,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isVarInferred,
        bool isRaii,
        bool isMoveOnly);
    // EN: Freeze 3.0 C-01. True when `syntax` is `new T(...)` of an ownable type whose result
    //     is a fresh owner. A local without raii/owned and a discarded expression give it no
    //     owner, so it leaks deterministically. A parameter, return or field without `owned`
    //     stays the manual-ownership form (the receiver may `delete` it), see ExpressionResult.
    // ES: Congelamiento 3.0 C-01. Verdadero cuando `syntax` es `new T(...)` de un tipo poseible
    //     cuyo resultado es un dueno nuevo. Un local sin raii/owned y una expresion descartada no
    //     le dan dueno, asi que se fuga siempre. Un parametro, retorno o campo sin `owned` sigue
    //     siendo la forma de propiedad manual (quien lo recibe puede hacer `delete`).
    bool isFreshObjectCreation(
        const ast::AstNode* syntax,
        const ExpressionResult& result) const;
    void reportLostOwnership(
        const ast::AstNode* syntax,
        std::string_view context,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateVariableOwnershipInitializer(
        const ExpressionResult& initializer,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isMoveOnly,
        bool isRaii,
        bool isBorrowed,
        bool isForeachEnumerator,
        bool managedDelegate,
        bool raiiOwnsNativeArrayBuffer) const;
    void analyzeReferenceVariableInitializer(
        MethodEscapeOrigin& pendingReferenceEscapeOrigin,
        std::string& pendingReferencePlaceKey,
        std::optional<std::size_t>& pendingReferenceReborrowSourceLocalOrdinal,
        const ExpressionResult& initializer,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isReadonlyReference,
        bool isPinnedReferenceLocal) const;
    void validateVariableInitializerConversion(
        const ExpressionResult& initializer,
        const ast::AstNode* variable,
        std::string_view typeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        const language::PointerContract& declaredPointerContract,
        bool declaredPointerPointeeReadonly,
        bool isVarInferred) const;
    void configureBorrowedVariableSource(
        MethodLocalBorrowBinding& borrow,
        const ScopeName& source,
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void analyzeBorrowedVariableInitializer(
        std::optional<MethodLocalBorrowBinding>& pendingLocalBorrow,
        const ExpressionResult& initializer,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool isRaii,
        bool raiiBorrowedElementArray) const;
    void analyzeVariableInitializer(
        std::optional<ExpressionResult>& precomputedVarInitializer,
        std::optional<MethodLocalBorrowBinding>& pendingLocalBorrow,
        MethodEscapeOrigin& pendingReferenceEscapeOrigin,
        std::string& pendingReferencePlaceKey,
        std::optional<std::size_t>& pendingReferenceReborrowSourceLocalOrdinal,
        sema::OwnershipKind& pendingObjectLifetimeOwnership,
        sema::OwnershipKind& pendingStorageOwnership,
        std::optional<language::CallableTrait>& pendingCallableTrait,
        bool& pendingOnePastEndPointer,
        bool& pendingPointerPointeeReadonly,
        bool& pendingPointerNullable,
        bool& pendingPointerHasProvenance,
        std::size_t& pendingPointerKnownAlignment,
        unsigned& pendingPointerAddressSpace,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        const language::PointerContract& declaredPointerContract,
        bool declaredPointerPointeeReadonly,
        bool isVarInferred,
        bool isReferenceLocal,
        bool isReadonlyReference,
        bool isPinnedReferenceLocal,
        bool isMoveOnly,
        bool isRaii,
        bool isBorrowed,
        bool isForeachEnumerator,
        bool managedDelegate,
        bool raiiOwnsNativeArrayBuffer,
        bool raiiBorrowedElementArray);
    bool rejectDuplicateVariable(
        const ast::AstNode* variable,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeVariableLocal(
        MethodLocalSymbol& local,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        const MethodEscapeOrigin& pendingReferenceEscapeOrigin,
        std::string pendingReferencePlaceKey,
        std::optional<std::size_t> pendingReferenceReborrowSourceLocalOrdinal,
        sema::OwnershipKind pendingObjectLifetimeOwnership,
        sema::OwnershipKind pendingStorageOwnership,
        std::optional<language::CallableTrait> pendingCallableTrait,
        const std::vector<const ast::AstNode*>& variableLengthArrayExtentSyntaxes,
        const std::vector<std::string>& variableLengthArrayExtentTypeKeys,
        std::string_view variableLengthArrayElementTypeKey,
        const ast::AstNode* variableLengthArrayCountSyntax,
        bool pendingOnePastEndPointer,
        bool pendingPointerPointeeReadonly,
        bool pendingPointerNullable,
        bool pendingPointerHasProvenance,
        std::size_t pendingPointerKnownAlignment,
        unsigned pendingPointerAddressSpace,
        bool isStaticLocal,
        bool isReferenceLocal,
        bool isReadonlyReference,
        bool isVariableLengthArray,
        bool isVolatile,
        bool isRaii,
        bool isScope,
        bool isMoveOnly,
        bool isPinnedLocal,
        bool isBorrowed,
        bool hasPendingLocalBorrow);
    void configureVariableLocalConstant(
        MethodLocalSymbol& local,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t methodMemberOrdinal) const;
    void configureVariableLocalView(
        MethodLocalSymbol& local,
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal) const;
    void recordVariableShadowing(
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    std::size_t registerVariableLocal(
        MethodLocalSymbol local,
        const ast::AstNode* variable,
        std::size_t scopeOrdinal,
        bool isVariableLengthArray);
    void createVariableLengthArrayExtentLocals(
        const ast::AstNode* variable,
        const std::vector<const ast::AstNode*>& extentSyntaxes,
        const std::vector<std::string>& extentTypeKeys,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t localOrdinal);
    void updateVariableAssignmentState(
        const ast::AstNode* variable,
        std::size_t localOrdinal,
        bool isVariableLengthArray);
    void updateVariableActiveUnionMember(
        const ast::AstNode* variable,
        std::string_view typeKey,
        std::size_t scopeOrdinal,
        std::size_t localOrdinal);
    void finalizeVariableLocal(
        std::optional<MethodLocalBorrowBinding>& pendingLocalBorrow,
        const ast::AstNode* variable,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t localOrdinal,
        bool isStaticLocal,
        bool isRaii,
        bool isScope,
        bool isMoveOnly);
    void finishVariableAnalysis(
        std::optional<ExpressionResult>& precomputedVarInitializer,
        const std::vector<const ast::AstNode*>& variableLengthArrayExtentSyntaxes,
        const std::vector<std::string>& variableLengthArrayExtentTypeKeys,
        const std::string& variableLengthArrayElementTypeKey,
        const ast::AstNode* variableLengthArrayCountSyntax,
        const ast::AstNode* variable,
        const std::string& typeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        sema::OwnershipKind declaredOwnership,
        bool isVarInferred,
        bool isStaticLocal,
        bool isReferenceLocal,
        bool isReadonlyReference,
        bool isPinnedReferenceLocal,
        bool isPinnedLocal,
        bool isMoveOnly,
        bool isRaii,
        bool isScope,
        bool isBorrowed,
        bool isForeachEnumerator,
        bool managedDelegate,
        bool isVariableLengthArray,
        bool isVolatile,
        const char* ownerSpelling);
    void analyzeForInitializer(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t forScope,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeForCondition(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t forScope,
        diagnostics::DiagnosticEngine& diagnostics);
    OwnershipFlowSnapshot analyzeForUpdate(
        const ast::AstNode* statement,
        OwnershipFlowSnapshot backEdgeState,
        std::size_t methodMemberOrdinal,
        std::size_t forScope,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateForBackEdgeLocals(
        const ast::AstNode* statement,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        std::size_t localCountAtBodyEntry,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateForBackEdgeParameters(
        const ast::AstNode* statement,
        const ClassMemberSymbol* loopMethod,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateForBackEdgePartialMoves(
        const ast::AstNode* statement,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdgeState,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void analyzeFor(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
#include "nativesharp/Frontend/Internal/MethodBody/ForeachAnalysisState.h"
    void configureForeachSourceOwnership(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t foreachScope);
    ast::AstNode* chooseForeachCandidate(
        const ast::AstNode* statement,
        std::string_view sourceTypeKey,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool beginForeachIterationBorrow(
        const ast::AstNode* statement,
        const ast::AstNode* chosenCandidate,
        std::size_t foreachScope);
    std::optional<ForeachAnalysisState> beginForeach(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void finishForeach(
        const ForeachAnalysisState& state,
        std::size_t methodMemberOrdinal);
    void analyzeForeach(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
#include "nativesharp/Frontend/Internal/MethodBody/SwitchAnalysisState.h"
    std::optional<SwitchAnalysisState> beginSwitch(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeSwitchSectionLabels(
        const ast::AstNode* section,
        const ExpressionResult& governing,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::vector<std::int64_t>& seenValues,
        const ast::AstNode*& defaultLabel,
        MethodSwitchSectionBinding& sectionBinding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateSwitchLabelConversion(
        const ast::AstNode* label,
        const ExpressionResult& governing,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::optional<std::int64_t> foldSwitchLabel(
        const ast::AstNode* label,
        const ExpressionResult& governing,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool recordSwitchLabel(
        const ast::AstNode* label,
        std::int64_t canonical,
        std::vector<std::int64_t>& seenValues,
        MethodSwitchSectionBinding& sectionBinding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void finishSwitch(SwitchAnalysisState&& state);
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

#include "nativesharp/Frontend/Internal/MethodBody/ConstantMemberResolver.h"

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
    std::string effectiveReturnTypeKey(const ClassMemberSymbol& method) const;
    bool analyzeLambdaReturn(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rejectFinallyReturn(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeSimpleReturnRestrictions(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        std::string_view declaredReturnTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool returnLifetimeOutlives(
        const ClassMemberSymbol& method,
        std::string_view longer,
        std::string_view shorter) const;
    bool validateReferenceReturnLifetime(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const MethodEscapeOrigin& escape,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeReferenceReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rejectVariableArrayReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool reportDirectOwnerReturnWithoutMove(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzeReturnValue(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rejectRefFieldReturn(
        const ast::AstNode* statement,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool rejectBoundMethodReturn(
        const ast::AstNode* statement,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    static language::FunctionType returnFunctionType(const ClassMemberSymbol& callable);
    bool validateMustTailReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ProjectImplicitConversion& conversion,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validatePointerReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void noteReturnEscape(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool validateViewReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateScopeReturn(
        const ast::AstNode* statement,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateBorrowedReturnValue(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        bool returnTypeOwnable,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool configureBorrowedReturnFromLocal(
        MethodBorrowBinding& borrow,
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool configureBorrowedReturnSource(
        MethodBorrowBinding& borrow,
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeBorrowedReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool returnTypeOwnable,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rejectBorrowedUnqualifiedReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void analyzeOwnedReturn(
        const ast::AstNode* statement,
        const ClassMemberSymbol& method,
        const ExpressionResult& value,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool directOwnerReturnWithoutMove,
        bool returnTypeOwnable,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> loopExitTargetIndex(
        const ast::AstNode* statement,
        MethodStructuredExitKind kind,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateLoopExitAgainstFinally(
        const ast::AstNode* statement,
        MethodStructuredExitKind kind,
        std::size_t targetLoopIndex,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool recordLoopBreak(
        const ast::AstNode* statement,
        std::size_t targetLoopIndex,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeLoopExit(
        const ast::AstNode* statement,
        MethodStructuredExitKind kind,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
#include "nativesharp/Frontend/Internal/MethodBody/TryAnalysisState.h"
    std::optional<TryAnalysisState> beginTry(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void completeTryBody(TryAnalysisState& state);
    MethodExceptionBinding initializeCatchBinding(
        const ast::AstNode* catchClause,
        std::size_t methodMemberOrdinal,
        std::size_t catchScope) const;
    void validateCatchType(
        TryAnalysisState& state,
        const ast::AstNode* catchClause,
        MethodExceptionBinding& binding,
        std::size_t methodMemberOrdinal,
        std::size_t catchScope,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeCatchFilter(
        const ast::AstNode* catchClause,
        const MethodExceptionBinding& binding,
        std::size_t methodMemberOrdinal,
        std::size_t catchScope,
        diagnostics::DiagnosticEngine& diagnostics);
    void activateCatchContext(
        TryAnalysisState& state,
        const ast::AstNode* catchClause,
        MethodExceptionBinding binding,
        std::size_t catchScope);
    bool prepareNextCatch(
        TryAnalysisState& state,
        const ast::AstNode*& body,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void finishCurrentCatch(TryAnalysisState& state);
    bool prepareFinally(TryAnalysisState& state, const ast::AstNode*& body);
    void finishFinally(TryAnalysisState& state);
    void analyzeTry(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
#include "nativesharp/Frontend/Internal/MethodBody/UsingAnalysisState.h"
    std::optional<UsingAnalysisState> beginUsing(
        const ast::AstNode* statement,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateUsingInitializer(
        const ast::AstNode* statement,
        std::string_view resourceTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t usingScope,
        diagnostics::DiagnosticEngine& diagnostics);
    bool resolveUsingDispose(
        const ast::AstNode* statement,
        std::string_view resourceTypeKey,
        std::optional<std::size_t>& disposableOrdinal,
        std::optional<std::size_t>& disposeContract,
        std::optional<std::size_t>& resourceTypeOrdinal,
        std::optional<std::size_t>& implementation,
        bool& interfaceDispatch,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordUsingCleanup(
        const ast::AstNode* statement,
        std::string_view resourceTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t usingScope,
        std::size_t localOrdinal,
        std::size_t resourceTypeOrdinal,
        std::size_t disposableOrdinal,
        std::size_t disposeContract,
        std::optional<std::size_t> implementation,
        bool interfaceDispatch);
    void finishUsing(const UsingAnalysisState& state);
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
    // Freeze 3.0 C-01: throw operand ownership (see StatementAnalyzer.cpp).
    bool isCatchLocalExpression(const ast::AstNode* expression, std::size_t scopeOrdinal) const;
    void validateThrowOwnership(
        const ast::AstNode* operand,
        const ExpressionResult& value,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::string delegateErrorTypeKey(std::string_view delegateTypeKey) const;
    // EN: Freeze 3.0 C-13: handle-or-propagate check for a call site of a callable `throws E`.
    // ES: Congelamiento 3.0 C-13: chequeo manejar-o-propagar en la llamada a un callable `throws E`.
    void requireDeclaredErrorHandled(
        const ast::AstNode* site,
        std::string_view errorTypeKey,
        std::string_view calleeName,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    // EN: Freeze 3.0 (checked throw): a `throw` or `throw;` follows the same rule as a call.
    // ES: Congelamiento 3.0 (throw chequeado): un `throw` o `throw;` sigue la misma regla que una
    //     llamada.
    void requireThrownErrorHandled(
        const ast::AstNode* site,
        std::string_view errorTypeKey,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::vector<std::string> activeErrorHandlers() const;
    std::string enclosingErrorTypeKey(std::size_t methodMemberOrdinal) const;
    std::vector<std::string> tryErrorHandlers(const ast::AstNode* statement) const;
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
    ExpressionResult analyzeExpressionKindLoopExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindMatchExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindMatchValueExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindMatchArmExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindAwaitExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindIdentifierExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindThisExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindBaseExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindIntegerLiteralExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindFloatingLiteralExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindCharacterLiteralExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindOverflowPolicyExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindNameOfExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindStringLiteralExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindInterpolatedStringExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindBooleanLiteralExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindNullLiteralExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindLambdaExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindMoveExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    bool acceptMovedMethodLocal(
        ExpressionResult& movedMethod,
        const ast::AstNode* expression,
        const ScopeName& source,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool acceptMovedMethodParameter(
        ExpressionResult& movedMethod,
        const ast::AstNode* expression,
        const ScopeName& source,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeMovedMethodValue(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool moveAllowed);
    std::optional<ScopeName> partialMoveRoot(
        const ast::AstNode* storage,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal) const;
    bool rejectInvalidPartialMove(
        const std::optional<ScopeName>& root,
        const ast::AstNode* placeExpression,
        const ast::AstNode* moveExpression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeMovedArrayElement(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool moveAllowed);
    bool validateMovedArrayElementSource(
        const ExpressionResult& element,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordMovedArrayElement(
        ExpressionResult& result,
        const ExpressionResult& element,
        const ast::AstNode* expression,
        const std::optional<ScopeName>& partialRoot,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool validateOwnedFieldMovePlace(
        std::vector<std::string>& fieldPath,
        const ast::AstNode* expression,
        const std::optional<ScopeName>& partialRoot,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool tryOwnedFieldMoveSource(
        ExpressionResult& result,
        const ExpressionResult& fieldSource,
        const ast::AstNode* expression,
        const std::optional<ScopeName>& partialRoot,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeMovedIteratorCurrent(
        ExpressionResult& result,
        const ExpressionResult& member,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeMovedMember(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool moveAllowed);
    bool validateLocalMoveState(
        const MethodLocalSymbol& local,
        const ast::AstNode* expression,
        std::size_t sourceOrdinal,
        std::size_t scopeOrdinal,
        bool sourceRequiresMove,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool completeLocalMove(
        ExpressionResult& result,
        const MethodLocalSymbol& local,
        const ast::AstNode* expression,
        std::size_t sourceOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool sourceRequiresMove,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeNamedLocalMove(
        ExpressionResult& result,
        bool& copyShapedMove,
        const ast::AstNode* expression,
        const ScopeName& source,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool compilerVariantResultTransfer,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateParameterMoveState(
        const MethodParameterSymbol& parameter,
        const ast::AstNode* expression,
        std::size_t sourceOrdinal,
        std::size_t scopeOrdinal,
        bool sourceRequiresMove,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool completeParameterMove(
        ExpressionResult& result,
        const MethodParameterSymbol& parameter,
        const ast::AstNode* expression,
        std::size_t sourceOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool sourceRequiresMove,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeNamedParameterMove(
        ExpressionResult& result,
        bool& copyShapedMove,
        const ast::AstNode* expression,
        const ScopeName& source,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool compilerVariantResultTransfer,
        diagnostics::DiagnosticEngine& diagnostics);
    bool moveExpressionAllowed(const ast::AstNode* expression, bool allowMove) const;
    void completeMoveResult(ExpressionResult& result, bool copyShapedMove) const;
    ExpressionResult analyzeExpressionKindNullConditionalMemberExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindMemberExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindPointerMemberExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindCallExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    bool validateResultPropagationContext(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string& returnTypeKey);
    ExpressionResult analyzeExpressionKindResultPropagationExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindUnaryExpressionOrPostfixExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindBinaryExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindNullCoalescingExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindTupleExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindWithExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindConditionalExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindAssignmentExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindTypeTestExpressionOrSafeCastExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindCastExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindFromEndExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindRangeExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindIndexExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    void analyzeIndexOperands(
        ExpressionResult& base,
        std::vector<ExpressionResult>& indices,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeNativeArrayIndex(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const std::vector<ExpressionResult>& indices,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeNativeStringIndex(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const std::vector<ExpressionResult>& indices,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::vector<std::size_t> rangeProtocolCandidates(
        std::size_t receiverType,
        std::size_t accessingTypeOrdinal,
        std::string_view name) const;
    ExpressionResult analyzedRangeEndpoint(
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal) const;
    bool resolveRangeProtocol(
        ExpressionResult& sliceResult,
        MethodCallBinding& lengthCall,
        MethodCallBinding& sliceCall,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        std::size_t receiverType,
        std::size_t accessingTypeOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateRangeProtocolMembers(
        const ast::AstNode* rangeSyntax,
        const MethodCallBinding& lengthCall,
        const MethodCallBinding& sliceCall,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateRangeEndpoints(
        ProjectImplicitConversion& startConversion,
        ProjectImplicitConversion& endConversion,
        ExpressionResult& startResult,
        ExpressionResult& endResult,
        const ast::AstNode* rangeSyntax,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordRangeSliceBinding(
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const ExpressionResult& sliceResult,
        std::size_t receiverType,
        const MethodCallBinding& lengthCall,
        const MethodCallBinding& sliceCall,
        const ExpressionResult& startResult,
        const ExpressionResult& endResult,
        const ProjectImplicitConversion& startConversion,
        const ProjectImplicitConversion& endConversion,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool analyzeRangeIndexer(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        std::size_t receiverType,
        std::size_t accessingTypeOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeUserIndexer(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const std::vector<ExpressionResult>& indices,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateFixedArrayIndices(
        std::string& indexedType,
        bool& fixedOnePastAddress,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const std::vector<ExpressionResult>& indices,
        diagnostics::DiagnosticEngine& diagnostics) const;
    const MethodLocalSymbol* variableArrayLocal(
        const ast::AstNode* expression,
        const ExpressionResult& base,
        std::size_t scopeOrdinal) const;
    bool analyzeVariableArrayIndex(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const std::vector<ExpressionResult>& indices,
        const MethodLocalSymbol* variableArrayLocal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeRawIndex(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const std::vector<ExpressionResult>& indices,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult
    analyzeExpressionKindSizeofTypeExpressionOrSizeofExpressionOrAlignofTypeExpressionOrAlignofExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindOffsetofExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindDefaultValueExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindObjectCreationExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindPlacementConstructionExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindNewExpressionOrStackallocExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindDeleteExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    bool validateDestroyTarget(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzeExpressionKindDestroyExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindNullConditionalIndexExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeExpressionKindCollectionExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);

    ExpressionResult analyzeExpressionCore(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup,
        bool allowEvent,
        bool allowMove,
        bool allowConstructorPropertyWrite);
    bool analyzeExpressionCoreFlowKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeExpressionCorePrimaryKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeExpressionCoreLiteralKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeExpressionCoreAccessKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeExpressionCoreOperatorKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeExpressionCoreTypeKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeExpressionCoreConstructionKinds(
        ExpressionResult& result,
        const ExpressionAnalysisRequest& request,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> interpolationHandlerMember(
        std::size_t typeOrdinal,
        std::string_view name) const;
    bool bindInterpolationHandler(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::string_view handlerTarget,
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool resolveResultPropagationErrorConversion(
        const MethodResultPropagationBinding& source,
        const MethodResultPropagationBinding& destination,
        const ast::AstNode* expression,
        std::optional<std::size_t>& errorConversion,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordResultPropagationBinding(
        MethodResultPropagationBinding source,
        const MethodResultPropagationBinding& destination,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::string_view returnTypeKey,
        std::optional<std::size_t> errorConversion,
        ExpressionResult& result);
    const ClassMemberSymbol* withUpdateField(
        std::size_t sourceTypeOrdinal,
        std::string_view fieldName) const;
    void bindWithUpdate(
        MethodAggregateInitializerBinding& binding,
        std::vector<bool>& assigned,
        bool& valid,
        const ast::AstNode* expression,
        const ast::AstNode* update,
        const ExpressionResult& source,
        std::span<const std::string> elements,
        std::span<const std::string> names,
        std::optional<std::size_t> sourceTypeOrdinal,
        const ClassMemberSymbol* caller,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::vector<ExpressionResult> analyzeNullConditionalIndices(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeNullConditionalNativeArray(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        std::span<const ExpressionResult> indices,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeNullConditionalUserIndex(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& base,
        const std::vector<ExpressionResult>& indices,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::span<ast::TypeSyntax* const> nameOfGenericArguments(
        const ast::AstNode* expression) const noexcept;
    bool resolveNameOfTypeCandidate(
        const ast::AstNode* expression,
        const ast::AstNode* target,
        std::span<ast::TypeSyntax* const> genericArguments,
        bool& typeCandidateFound,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::optional<std::size_t> nameOfOwnerTypeOrdinal(
        const ast::AstNode* target,
        std::size_t methodMemberOrdinal) const;
    bool resolveNameOfConcreteGenericMethod(
        const ast::AstNode* expression,
        std::size_t ownerTypeOrdinal,
        std::size_t accessingTypeOrdinal,
        std::size_t genericArgumentCount,
        bool& genericMethodNameFound) const;
    bool resolveNameOfTemplateGenericMethod(
        const ast::AstNode* expression,
        std::size_t ownerTypeOrdinal,
        const ClassMemberSymbol* caller,
        std::size_t genericArgumentCount,
        bool& genericMethodNameFound) const;
    bool resolveNameOfValueSymbol(
        const ast::AstNode* expression,
        const ast::AstNode* target,
        std::span<ast::TypeSyntax* const> genericArguments,
        bool genericMethodNameFound,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeConditionalBranches(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        ExpressionResult& whenTrue,
        ExpressionResult& whenFalse);
    static void adoptConditionalBranch(
        ExpressionResult& result,
        const ExpressionResult& branch);
    static void mergeConditionalOwnership(
        ExpressionResult& result,
        const ExpressionResult& whenTrue,
        const ExpressionResult& whenFalse);
    static void adoptConditionalNullableBranch(
        ExpressionResult& result,
        const ExpressionResult& nonNull);
    bool resolveConditionalDivergence(
        ExpressionResult& result,
        const ExpressionResult& whenTrue,
        const ExpressionResult& whenFalse) const;
    bool resolveConditionalMatchingType(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& whenTrue,
        const ExpressionResult& whenFalse,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void resolveConditionalSpecialType(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& whenTrue,
        const ExpressionResult& whenFalse,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void configureAllocatorConstruction(
        ProjectConstructionSite& site,
        const ast::AstNode* expression,
        const ExpressionResult& storage) const;
    bool analyzeConstructionArguments(
        ProjectConstructionSite& site,
        const ast::AstNode* expression,
        std::size_t constructorArgumentCount,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::string configureAllocatorResultProfile(
        ProjectConstructionSite& site,
        const ast::AstNode* expression);
    void completePlacementConstructionResult(
        ExpressionResult& result,
        const NamedTypeDeclaration* target,
        std::string_view targetType,
        std::string_view requiredStorageType,
        std::string_view resultCarrierTypeKey,
        bool trailingConstruction,
        bool resultFailureProfile,
        bool allocatorConstruction,
        bool constructible,
        bool storageValid,
        bool argumentsValid) const;
    std::optional<std::size_t> collectionBuilderMember(
        std::size_t typeOrdinal,
        std::string_view name,
        bool wantStatic) const;
    bool bindCollectionBuilder(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view explicitTarget,
        std::size_t typeOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeCollectionSpreadSegments(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view explicitTarget,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectCollectionSpreadSegments(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::vector<const ast::AstNode*>& syntaxes,
        std::vector<bool>& spreads,
        std::vector<ExpressionResult>& values,
        std::vector<std::string>& elementTypes,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateCollectionSpreadSegments(
        const ast::AstNode* expression,
        std::span<const ast::AstNode* const> syntaxes,
        const std::vector<bool>& spreads,
        std::span<const ExpressionResult> values,
        std::span<const std::string> elementTypes,
        std::string_view element,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void completeCollectionArrayResult(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view element,
        bool elementsValid,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void analyzeCollectionElements(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view explicitTarget,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateNamedCast(
        ExpressionResult& result,
        const ExpressionResult& operand,
        const ast::AstNode* expression,
        std::string_view namedCast,
        bool targetPointeeReadonly,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool applyUserDefinedCast(
        ExpressionResult& result,
        const ExpressionResult& operand,
        const ast::AstNode* expression,
        const ProjectConversionClassifier& classifier,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateFunctionPointerCast(
        ExpressionResult& result,
        const ExpressionResult& operand,
        const ast::AstNode* expression,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateReferenceCast(
        ExpressionResult& result,
        const ExpressionResult& operand,
        const ast::AstNode* expression,
        const ProjectConversionClassifier& classifier,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeRangeArrayAllocation(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view element,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeNativeArrayAllocation(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view element,
        const ExpressionResult& count,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateRawAllocationElement(
        const ast::AstNode* expression,
        std::string_view element,
        bool isStackalloc,
        bool isRawNew,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateStackalloc(
        const ast::AstNode* expression,
        const ExpressionResult& count,
        bool allocationValid,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordRawAllocation(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::string_view element,
        bool allocationValid,
        bool isStackalloc,
        bool isRawNew,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
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
    void applyIdentifierFlowFacts(ExpressionResult& result, std::size_t storageKey) const;
    ExpressionResult analyzeLocalIdentifier(
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeParameterIdentifier(
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateLocalIdentifierRead(
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        const MethodLocalSymbol& local,
        diagnostics::DiagnosticEngine& diagnostics);
    bool applyBorrowedLocalIdentifierOwnership(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        const MethodLocalSymbol& local,
        diagnostics::DiagnosticEngine& diagnostics);
    bool applyLocalIdentifierOwnership(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        const MethodLocalSymbol& local,
        diagnostics::DiagnosticEngine& diagnostics);
    void completeLocalIdentifier(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        const MethodLocalSymbol& local,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool validateParameterIdentifierRead(
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        const MethodParameterSymbol& parameter,
        diagnostics::DiagnosticEngine& diagnostics);
    void initializeParameterIdentifierResult(
        ExpressionResult& result,
        const ScopeName& scopeName,
        const MethodParameterSymbol& parameter);
    bool applyBorrowedParameterReassignment(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void completeParameterIdentifier(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ScopeName& scopeName,
        const MethodParameterSymbol& parameter,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void collectIdentifierMemberCandidates(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t accessingTypeOrdinal,
        bool staticContext,
        std::vector<std::size_t>& fields,
        std::vector<std::size_t>& events,
        std::vector<std::size_t>& methods,
        bool& inaccessibleFound,
        bool& uncapturedReceiverFound) const;
    ExpressionResult analyzeImplicitEventIdentifier(
        const ast::AstNode* expression,
        const std::vector<std::size_t>& events,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t accessingTypeOrdinal,
        const std::string& thisTypeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowEvent);
    ExpressionResult analyzeImplicitFieldIdentifier(
        const ast::AstNode* expression,
        const std::vector<std::size_t>& fields,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t accessingTypeOrdinal,
        const std::string& thisTypeKey);
    void collectPropertyAccessorCandidates(
        std::string_view accessorName,
        bool getter,
        std::size_t methodMemberOrdinal,
        std::size_t accessingTypeOrdinal,
        bool staticContext,
        std::vector<std::size_t>& output) const;
    ExpressionResult analyzePropertyIdentifier(
        const ast::AstNode* expression,
        const std::vector<std::size_t>& propertyGetters,
        const std::vector<std::size_t>& propertySetters,
        const ClassMemberSymbol& caller,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t accessingTypeOrdinal,
        const std::string& thisTypeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowConstructorPropertyWrite);
    ExpressionResult analyzeMethodGroupIdentifier(
        const ast::AstNode* expression,
        const std::vector<std::size_t>& methods,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t accessingTypeOrdinal,
        const std::string& thisTypeKey,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    std::optional<ExpressionResult> analyzeNamespaceIdentifierFallback(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    ExpressionResult analyzeMemberOrNamespaceIdentifier(
        const ast::AstNode* expression,
        const ClassMemberSymbol& caller,
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
    bool analyzeQualifiedNamespaceMember(
        ExpressionResult& result,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    bool memberValueShadowsType(
        const ast::AstNode* receiverSyntax,
        std::size_t methodMemberOrdinal) const;
    bool resolveWrittenMemberType(
        std::optional<std::size_t>& typeOrdinal,
        std::string& writtenTypeName,
        const ast::AstNode* receiverSyntax,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void configureTypeMemberReceiver(
        ExpressionResult& receiver,
        const ast::AstNode* expression,
        std::size_t typeOrdinal,
        std::string_view writtenTypeName,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool analyzeBaseMemberReceiver(
        ExpressionResult& receiver,
        const ast::AstNode* expression,
        std::size_t accessingTypeOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool resolveMemberReceiver(
        ExpressionResult& receiver,
        bool& typeReceiver,
        const ast::AstNode* expression,
        std::size_t accessingTypeOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeBuiltinMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        bool typeReceiver,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool allowMethodGroup);
    bool analyzeBuiltinLengthMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        bool typeReceiver,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool analyzeNativeStringFacadeMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        bool typeReceiver,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    bool bindExtensionMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        bool typeReceiver,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    void collectExtensionMemberCandidates(
        std::vector<std::size_t>& methods,
        const ExpressionResult& receiver,
        bool typeReceiver,
        const NamespaceFunctionReferenceBinding& extension);
    void finishMemberLookup(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        bool typeReceiver,
        bool inaccessibleFound,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    void classifyMemberReceiverKinds(
        bool& receiverIsStruct,
        bool& receiverIsEnumValue,
        const ExpressionResult& receiver,
        bool typeReceiver,
        std::size_t receiverType) const;
    bool validateMemberReceiverType(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        bool typeReceiver,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    void collectAccessibleMemberCandidates(
        std::vector<std::size_t>& fields,
        std::vector<std::size_t>& events,
        std::vector<std::size_t>& methods,
        bool& inaccessibleFound,
        const ExpressionResult& receiver,
        bool typeReceiver,
        std::size_t receiverType,
        std::size_t accessingTypeOrdinal,
        std::string_view memberName) const;
    bool rejectIncompleteMemberReceiver(
        const ast::AstNode* expression,
        const std::vector<std::size_t>& fields,
        bool typeReceiver,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeEventMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        const ast::AstNode* expression,
        const std::vector<std::size_t>& events,
        bool typeReceiver,
        std::size_t receiverType,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowEvent);
    bool validateFieldMemberMoveState(
        ExpressionResult& result,
        const ast::AstNode* expression,
        bool isProjectionReceiver,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeFieldMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        const ast::AstNode* expression,
        const std::vector<std::size_t>& fields,
        bool typeReceiver,
        bool isProjectionReceiver,
        std::size_t receiverType,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzePropertyMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        const ast::AstNode* expression,
        const ClassMemberSymbol& caller,
        bool typeReceiver,
        std::size_t receiverType,
        std::size_t accessingTypeOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowConstructorPropertyWrite);
    bool analyzeListPatternLengthMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        const ast::AstNode* expression,
        const std::vector<std::size_t>& methods,
        bool typeReceiver,
        std::size_t receiverType,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeMethodGroupMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        const ast::AstNode* expression,
        const std::vector<std::size_t>& methods,
        bool typeReceiver,
        std::size_t receiverType,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    bool analyzeSynthesizedRootMember(
        ExpressionResult& result,
        const ExpressionResult& receiver,
        const ast::AstNode* expression,
        bool typeReceiver,
        bool receiverIsStruct,
        bool receiverIsEnumValue,
        bool inaccessibleFound,
        std::size_t receiverType,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool allowMethodGroup);
    ExpressionResult analyzeCall(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeAssertionIntrinsicCall(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAssertionIntrinsicArguments(
        const ast::AstNode* expression,
        std::string_view assertionIntrinsic,
        bool hasCondition,
        bool isPanic,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeNamespaceCallHead(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeListPatternRestCall(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzePreCallKinds(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeNativeStringPatternRestCall(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeArrayPatternRestCall(
        const ast::AstNode* expression,
        const ExpressionResult& restReceiver,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeEnumRootCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeIteratorFacadeCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeStructRootCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    static std::optional<MethodCoreOperationKind> structRootOperation(
        std::string_view memberName,
        const std::string& structTypeKey,
        std::string& resultTypeKey,
        std::size_t& expectedArity);
    bool validateStructRootOperation(
        const ast::AstNode* expression,
        MethodCoreOperationKind structKind,
        const std::string& memberName,
        const std::string& structTypeKey,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::optional<std::size_t> callNamedStorageKey(
        const ast::AstNode* syntax,
        std::size_t scopeOrdinal) const;
    static std::optional<std::size_t> callMarkerCase(std::string_view marker);
    void seedCallLambdaTargets(
        std::vector<std::string>& lambdaTargetTypes,
        const ast::AstNode* expression,
        const std::vector<std::string>& parameterTypes) const;
    void analyzeCallArguments(
        std::vector<ExpressionResult>& arguments,
        bool& allArgumentsValid,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const MethodCallBinding& call,
        std::span<const std::string> lambdaTargetTypes,
        bool coreStringForwarding,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    const ast::AstNode* resolvedCallArgumentSyntax(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::size_t ordinal,
        bool coreStringForwarding) const;
    bool callIsFullExpressionBorrow(
        const ClassMemberSymbol& candidate,
        std::size_t argumentOrdinal,
        const ExpressionResult& argument) const;
    static bool callIsStringLiteralBorrow(
        const ClassMemberSymbol& candidate,
        std::size_t parameterOrdinal,
        const ExpressionResult& argument) noexcept;
    std::optional<ExpressionResult> analyzeGenericLambdaSpecializedCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::vector<ExpressionResult>& arguments,
        bool allArgumentsValid,
        MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeEventInvocationCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::vector<ExpressionResult>& arguments,
        bool allArgumentsValid,
        MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool bindEventInvocationArguments(
        const ast::AstNode* expression,
        std::span<const ExpressionResult> arguments,
        std::span<const std::string> parameterTypes,
        MethodEventBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::optional<ExpressionResult> analyzeDelegateInvocationCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::vector<ExpressionResult>& arguments,
        bool allArgumentsValid,
        MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectDelegateDefaultArguments(
        std::vector<const ast::AstNode*>& defaultedSyntaxes,
        std::vector<std::string>& defaultedTypeKeys,
        const ast::AstNode* expression,
        std::size_t argumentCount,
        std::span<const std::string> parameterTypes,
        std::size_t delegateOrdinal,
        bool variadic,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool bindDelegateInvocationArguments(
        const ast::AstNode* expression,
        std::span<const ExpressionResult> arguments,
        std::span<const std::string> parameterTypes,
        MethodDelegateBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateCallableTraitInvocation(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeSpecialCallKinds(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::vector<ExpressionResult>& arguments,
        bool allArgumentsValid,
        MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool isMemoryOrderTypeKey(std::string_view typeKey) const;
    bool selectCallCandidate(
        std::size_t& selectedIndex,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> arguments,
        std::span<const ExpressionResult> resolvedArguments,
        bool coreStringForwarding,
        MethodCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics);
    void evaluateCallCandidates(
        bool& hasMatchingArity,
        std::vector<const ast::AstNode*>& constraintRejectedSyntaxes,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> resolvedArguments,
        bool coreStringForwarding,
        MethodCallBinding& call);
    bool prepareCallCandidateMapping(
        MethodCallCandidate& candidateBinding,
        const ClassMemberSymbol& candidate,
        const ast::AstNode* expression,
        std::span<const ExpressionResult> resolvedArguments,
        bool coreStringForwarding) const;
    void evaluateCallCandidateArguments(
        MethodCallCandidate& candidateBinding,
        const ClassMemberSymbol& candidate,
        std::span<const ExpressionResult> resolvedArguments) const;
    void evaluateCallCandidateArgument(
        MethodCallCandidate& candidateBinding,
        const ClassMemberSymbol& candidate,
        const ExpressionResult& argument,
        std::size_t argumentOrdinal) const;
    void finishCallCandidateEvaluation(
        std::vector<const ast::AstNode*>& constraintRejectedSyntaxes,
        MethodCallCandidate& candidateBinding,
        const ClassMemberSymbol& candidate,
        std::size_t candidateOrdinal,
        MethodCallBinding& call) const;
    bool collectViableCallCandidates(
        std::vector<std::size_t>& viableEvaluationOrdinals,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> arguments,
        std::span<const ExpressionResult> resolvedArguments,
        bool coreStringForwarding,
        bool hasMatchingArity,
        std::span<const ast::AstNode* const> constraintRejectedSyntaxes,
        MethodCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics);
    bool reportTerminalCallCandidateDiagnostic(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        bool coreStringForwarding,
        std::span<const ast::AstNode* const> constraintRejectedSyntaxes,
        MethodCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics);
    bool reportCallOwnershipDiagnostic(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> resolvedArguments,
        bool coreStringForwarding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool reportCandidateOwnershipDiagnostic(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const ClassMemberSymbol& candidate,
        std::span<const ExpressionResult> resolvedArguments,
        bool coreStringForwarding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool reportArgumentOwnershipDiagnostic(
        const ast::AstNode* expression,
        const ast::AstNode* argumentSyntax,
        const MethodParameterSymbol& parameter,
        const ExpressionResult& argument,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool reportCallPassKindDiagnostic(
        const ast::AstNode* expression,
        std::span<const ExpressionResult> arguments,
        const MethodCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool chooseNonDominatedCallCandidate(
        std::size_t& selectedIndex,
        const ast::AstNode* expression,
        std::span<const std::size_t> viableEvaluationOrdinals,
        MethodCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> prepareCallCallee(
        const ast::AstNode* expression,
        MethodCallBinding& call,
        ExpressionResult& callee,
        bool& nullConditionalCall,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void configureCallLambdaTargets(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        MethodCallBinding& call,
        bool& coreStringForwarding,
        std::vector<std::string>& lambdaTargetTypes);
    void configureMethodGroupLambdaTargets(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        bool coreStringForwarding,
        std::vector<std::string>& lambdaTargetTypes);
    std::vector<ExpressionResult> prepareResolvedCallArguments(
        const ExpressionResult& callee,
        std::span<const ExpressionResult> arguments,
        bool coreStringForwarding) const;
    ExpressionResult completeSelectedCall(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> arguments,
        std::vector<ExpressionResult>& resolvedArguments,
        bool coreStringForwarding,
        bool nullConditionalCall,
        std::size_t selectedIndex,
        MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateSelectedCallReceiver(
        std::optional<std::size_t>& provenVariantCase,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const MethodCallCandidate& selected,
        MethodCallBinding& call,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateActiveIterationCallReceiver(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const ClassMemberSymbol& selectedMember,
        MethodCallBinding& call,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void updateSelectedVariantFlow(
        std::optional<std::size_t>& provenVariantCase,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const ClassMemberSymbol& selectedMember,
        std::size_t scopeOrdinal);
    void invalidateCallArgumentFlow(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> resolvedArguments,
        const MethodCallCandidate& selected,
        bool coreStringForwarding,
        std::size_t scopeOrdinal);
    void updateScopedChildBorrowsForCall(
        const ast::AstNode* expression,
        const MethodCallCandidate& selected,
        const MethodCallBinding& call,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void registerScopedChildBorrows(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool selectedCallUsesThreadExecutor(const MethodCallCandidate& selected) const;
    void validateThreadExecutorCaptures(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool callTypeIsSend(const std::string& typeKey) const;
    bool callTypeIsSync(const std::string& typeKey) const;
    void configureSelectedCallBinding(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        const MethodCallCandidate& selected,
        bool coreStringForwarding,
        MethodCallBinding& call) const;
    void recordSelectedOutArguments(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> resolvedArguments,
        const MethodCallCandidate& selected,
        const ClassMemberSymbol& selectedMember,
        bool coreStringForwarding,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recordSelectedOwnershipTransfers(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> resolvedArguments,
        const MethodCallCandidate& selected,
        const ClassMemberSymbol& selectedMember,
        bool coreStringForwarding,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recordSelectedBorrows(
        const ast::AstNode* expression,
        const ExpressionResult& callee,
        std::span<const ExpressionResult> resolvedArguments,
        const MethodCallCandidate& selected,
        const ClassMemberSymbol& selectedMember,
        bool coreStringForwarding,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void finalizeSelectedCallResult(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const MethodCallBinding& call,
        bool nullConditionalCall,
        std::optional<std::size_t> provenVariantCase,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool callResultIsNullable(std::string_view typeKey) const;
    void applySelectedCoreOperations(
        const ast::AstNode* expression,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        MethodCallBinding& call,
        ExpressionResult& result,
        diagnostics::DiagnosticEngine& diagnostics);
    void recognizeCoreStringOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreFloat128Operations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreInt128Operations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreScalarOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreHalfOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreVector256Operations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreConsoleRuntimeOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreClockEnvironmentOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreClockOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreFilePathOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreFileDirectoryOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    static std::optional<MethodCoreOperationKind> basicFileDirectoryOperation(
        const ClassMemberSymbol& selectedMember,
        bool isFile);
    static std::optional<MethodCoreOperationKind> fileContentOperation(
        const ClassMemberSymbol& selectedMember,
        bool isFile);
    static std::optional<MethodCoreOperationKind> directoryEnumerationOperation(
        const ClassMemberSymbol& selectedMember,
        bool isFile);
    void recognizeCoreThreadOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreAtomicOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        ExpressionResult& result,
        diagnostics::DiagnosticEngine& diagnostics);
    void recognizeCoreAtomicValueOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        ExpressionResult& result,
        diagnostics::DiagnosticEngine& diagnostics);
    void classifyOrderedAtomicCall(
        bool& orderedLoad,
        bool& orderedStore,
        bool& orderedRmw,
        bool& orderedCompareExchange,
        const ClassMemberSymbol& selectedMember,
        std::size_t parameterOffset,
        const std::string& valueTypeKey) const;
    static std::optional<MethodCoreOperationKind> atomicOperationKind(
        const ClassMemberSymbol& selectedMember,
        std::size_t parameterOffset,
        const std::string& valueTypeKey,
        bool isInt32,
        bool isPointer,
        bool orderedLoad,
        bool orderedStore,
        bool orderedRmw,
        bool orderedCompareExchange);
    void validateAtomicCompareExchangeOrders(
        std::optional<MethodCoreOperationKind>& atomicKind,
        std::optional<std::uint8_t>& atomicMemoryOrder,
        std::optional<std::uint8_t>& atomicFailureMemoryOrder,
        const ast::AstNode* expression,
        const ClassMemberSymbol& selectedMember,
        std::size_t parameterOffset,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateAtomicMemoryOrder(
        std::optional<MethodCoreOperationKind>& atomicKind,
        std::optional<std::uint8_t>& atomicMemoryOrder,
        const ast::AstNode* expression,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        bool orderedLoad,
        bool orderedStore,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void bindAtomicOperation(
        MethodCoreOperationKind atomicKind,
        std::optional<std::uint8_t> atomicMemoryOrder,
        std::optional<std::uint8_t> atomicFailureMemoryOrder,
        const ast::AstNode* expression,
        const ClassMemberSymbol& selectedMember,
        const std::string& valueTypeKey,
        bool atomicWrapper,
        bool publicInterlocked,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreMutexMonitorOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreMutexOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreSemaphoreEventOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreSemaphoreOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreDiagnosticsOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void recognizeCoreProcessOperations(
        const ast::AstNode* expression,
        const NamedTypeDeclaration& declaringType,
        const ClassMemberSymbol& selectedMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    ExpressionResult analyzeMatchExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void decodeMatchCoverage(
        const ast::AstNode* expression,
        std::vector<language::MatchArmCoverage>& coverageArms) const;
    void decodeMatchCoverageArm(
        std::string_view armText,
        std::vector<language::MatchArmCoverage>& coverageArms) const;
    void collectMatchDomain(
        std::string_view typeKey,
        std::vector<std::string>& domain,
        std::optional<std::size_t>& switchTagMemberOrdinal,
        std::unordered_map<std::string, std::int64_t>& variantCaseTags) const;
    void collectMatchDomainNames(
        std::size_t typeOrdinal,
        const ast::AstNode* declaration,
        std::vector<std::string>& domain) const;
    void collectMatchDomainTags(
        std::size_t typeOrdinal,
        std::span<const std::string> domain,
        std::optional<std::size_t>& switchTagMemberOrdinal,
        std::unordered_map<std::string, std::int64_t>& variantCaseTags) const;
    void validateMatchCoverage(
        const ast::AstNode* expression,
        std::span<const std::string> domain,
        std::span<const language::MatchArmCoverage> coverageArms,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateOpenMatchCoverage(
        const ast::AstNode* expression,
        std::span<const language::MatchArmCoverage> coverageArms,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateClosedMatchCoverage(
        const ast::AstNode* expression,
        std::span<const std::string> domain,
        std::span<const language::MatchArmCoverage> coverageArms,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeMatchArmHeader(
        const ast::AstNode* arm,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        OwnershipFlowSnapshot& remaining,
        std::size_t& armScope,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeMatchExpressionArm(
        const ast::AstNode* arm,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        OwnershipFlowSnapshot& remaining,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeMatchExpressionArms(
        const ast::AstNode* expression,
        const ExpressionResult& governing,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::vector<ExpressionResult>& values,
        diagnostics::DiagnosticEngine& diagnostics);
    void restoreMatchExitFlow(
        const OwnershipFlowSnapshot& entry,
        std::span<const OwnershipFlowSnapshot> exits);
    bool validateMatchReferenceResults(
        const ast::AstNode* expression,
        bool& referenceResult,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool resolveMatchResultType(
        const ast::AstNode* expression,
        std::span<const ExpressionResult> values,
        bool referenceResult,
        std::string& commonType,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool matchExpressionContainsRestView(const ast::AstNode* node) const;
    void recordMatchExpressionBinding(
        const ast::AstNode* expression,
        const ExpressionResult& governing,
        std::string_view commonType,
        bool referenceResult,
        std::span<const language::MatchArmCoverage> coverageArms,
        std::optional<std::size_t> switchTagMemberOrdinal,
        const std::unordered_map<std::string, std::int64_t>& variantCaseTags,
        std::span<const ExpressionResult> values);
    void applyMatchExpressionOwnership(
        ExpressionResult& result,
        std::span<const ExpressionResult> values) const;
    ExpressionResult analyzeLoopExpression(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateLoopExpressionSyntax(
        const ast::AstNode* expression,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateLoopExpressionBackEdgeLocals(
        const ast::AstNode* expression,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdge,
        std::size_t localCountAtBodyEntry,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateLoopExpressionBackEdgeParameters(
        const ast::AstNode* expression,
        const ClassMemberSymbol* loopMethod,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdge,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateLoopExpressionBackEdgePlaces(
        const ast::AstNode* expression,
        const OwnershipFlowSnapshot& loopEntry,
        const OwnershipFlowSnapshot& backEdge,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::optional<std::string> loopExpressionCommonType(
        const ast::AstNode* expression,
        const LoopContext& completed,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordLoopExpressionBindings(
        const ast::AstNode* expression,
        const LoopContext& completed,
        std::string_view commonType,
        std::size_t methodMemberOrdinal,
        std::size_t loopScope);
    ExpressionResult loopExpressionResult(
        const LoopContext& completed,
        std::string commonType) const;
    ExpressionResult analyzeNamespaceFunctionCall(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        const ExpressionResult* forwardedReceiver = nullptr,
        const ast::AstNode* extraArgumentSyntax = nullptr);
    void storeNamespaceFunctionCall(
        const ast::AstNode* expression,
        const NamespaceFunctionCallBinding& call);
    std::vector<ast::AstNode*> namespaceFunctionArgumentSyntaxes(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        const ExpressionResult* forwardedReceiver,
        const ast::AstNode* extraArgumentSyntax,
        std::size_t explicitArgumentCount) const;
    ExpressionResult bindNamespaceFunctionMethodGroupArgument(
        const ast::AstNode* argument,
        ExpressionResult analyzed,
        const NamespaceFunctionReferenceBinding& reference,
        std::span<ast::AstNode* const> argumentSyntaxes,
        std::size_t argumentOrdinal,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        bool hasForwardedReceiver,
        diagnostics::DiagnosticEngine& diagnostics);
    std::vector<ExpressionResult> analyzeNamespaceFunctionArguments(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        std::span<ast::AstNode* const> argumentSyntaxes,
        std::size_t explicitArgumentCount,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        const ExpressionResult* forwardedReceiver,
        const ast::AstNode* extraArgumentSyntax,
        bool& argumentsValid,
        diagnostics::DiagnosticEngine& diagnostics);
    void normalizeNamespaceFunctionParameterPack(
        const ClassMemberSymbol& candidate,
        std::span<const ExpressionResult> arguments,
        MethodCallCandidate& evaluated) const;
    std::optional<ProjectImplicitConversion> evaluateNamespaceVariadicCandidateArgument(
        std::span<const ExpressionResult> arguments,
        std::size_t argumentOrdinal,
        std::size_t parameterOrdinal,
        const ClassMemberSymbol& candidate,
        MethodCallCandidate& evaluated) const;
    void validateNamespaceFunctionCandidateArgumentOwnership(
        const MethodParameterSymbol& parameter,
        std::span<const ExpressionResult> arguments,
        std::size_t argumentOrdinal,
        bool packElement,
        MethodCallCandidate& evaluated) const;
    ProjectImplicitConversion evaluateNamespaceFunctionCandidateArgument(
        const ClassMemberSymbol& candidate,
        std::span<const ExpressionResult> arguments,
        std::size_t argumentOrdinal,
        bool hasForwardedReceiver,
        MethodCallCandidate& evaluated) const;
    MethodCallCandidate evaluateNamespaceFunctionCandidate(
        std::size_t functionOrdinal,
        std::span<ast::AstNode* const> argumentSyntaxes,
        std::span<const ExpressionResult> arguments,
        bool hasForwardedReceiver,
        bool& matchingArity) const;
    void evaluateNamespaceFunctionCandidates(
        const NamespaceFunctionReferenceBinding& reference,
        std::span<ast::AstNode* const> argumentSyntaxes,
        std::span<const ExpressionResult> arguments,
        bool hasForwardedReceiver,
        NamespaceFunctionCallBinding& call,
        bool& matchingArity) const;
    void reportNoViableNamespaceFunctionCall(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        std::span<ast::AstNode* const> argumentSyntaxes,
        const NamespaceFunctionCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::vector<std::size_t> viableNamespaceFunctionCandidates(
        const NamespaceFunctionCallBinding& call) const;
    std::vector<std::size_t> nonDominatedNamespaceFunctionCandidates(
        const NamespaceFunctionCallBinding& call,
        std::span<const std::size_t> viable) const;
    std::optional<std::size_t> selectNamespaceFunctionCandidate(
        const ast::AstNode* expression,
        const NamespaceFunctionReferenceBinding& reference,
        std::span<ast::AstNode* const> argumentSyntaxes,
        std::size_t explicitArgumentCount,
        bool matchingArity,
        NamespaceFunctionCallBinding& call,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateNamespaceFunctionDefinition(
        const ast::AstNode* expression,
        std::size_t functionOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void checkNamespaceFunctionArgumentPassKinds(
        const ast::AstNode* expression,
        const ClassMemberSymbol& callable,
        std::size_t functionOrdinal,
        const std::vector<ExpressionResult>& arguments,
        const MethodCallCandidate& selected,
        bool hasForwardedReceiver,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordNamespaceFunctionBorrows(
        const ast::AstNode* expression,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        std::size_t functionOrdinal,
        std::span<ast::AstNode* const> argumentSyntaxes,
        const std::vector<ExpressionResult>& arguments,
        const MethodCallCandidate& selected);
    void recordNamespaceFunctionDefaults(
        const ClassMemberSymbol& callable,
        const MethodCallCandidate& selected,
        NamespaceFunctionCallBinding& call) const;
    ExpressionResult completeNamespaceFunctionCall(
        const ast::AstNode* expression,
        std::size_t callerCallableOrdinal,
        std::size_t scopeOrdinal,
        std::size_t functionOrdinal,
        std::span<ast::AstNode* const> argumentSyntaxes,
        const std::vector<ExpressionResult>& arguments,
        const MethodCallCandidate& selected,
        NamespaceFunctionCallBinding& call,
        bool hasForwardedReceiver,
        diagnostics::DiagnosticEngine& diagnostics);
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
    std::optional<ExpressionResult> analyzeUnaryOperand(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeOverloadedUnary(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeLogicalNot(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeAddressOf(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeDereference(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeIncrementOrDecrement(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeUnaryNumeric(
        const ast::AstNode* expression,
        const ExpressionResult& operand,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeBinary(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeLogicalBinary(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeLogicalBinaryStep(
        ExpressionResult& accumulated,
        const ast::AstNode* logical,
        lexer::TokenKind operation,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeStringBinary(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool analyzeOverloadedBinary(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzePortableSimdBinary(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeEnumBinary(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzeStructComparison(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> binaryFlowStorageKey(
        const ast::AstNode* syntax,
        std::size_t scopeOrdinal) const;
    static std::optional<std::int64_t> binarySignedConstant(
        const std::optional<ProjectIntegerConstant>& constant);
    static void refineBinaryIntegerRange(
        OwnershipFlowSnapshot& edge,
        std::size_t key,
        std::optional<std::int64_t> minimum,
        std::optional<std::int64_t> maximum,
        bool excludesZero);
    void publishBinaryNullFlow(
        ConditionFlowStates& flow,
        bool& published,
        const ast::AstNode* expression,
        lexer::TokenKind operation,
        std::size_t scopeOrdinal) const;
    void publishBinaryIntegerFlow(
        ConditionFlowStates& flow,
        bool& published,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t scopeOrdinal) const;
    bool analyzeBinaryComparison(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeShiftBinary(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool analyzePointerArithmetic(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeArithmeticBinary(
        ExpressionResult& result,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        lexer::TokenKind operation,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzeAssignment(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordReferenceFieldAssignmentOrigin(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void reportScopeAssignmentEscape(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool assignmentConflictsWithIterationBorrow(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void invalidateAssignedStorage(
        const ast::AstNode* target,
        std::size_t scopeOrdinal);
    bool assignmentNullCarrier(std::string_view typeKey) const;
    ExpressionResult analyzeNullCoalescingAssignment(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindAssignmentDeconstructionTarget(
        bool& valid,
        const ast::AstNode* expression,
        const ast::AstNode* target,
        const std::string& sourceTypeKey,
        const std::string& path,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeTupleDeconstructionAssignment(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordAssignedOutParameter(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    void classifyDirectAssignmentTarget(
        ExpressionResult& left,
        std::optional<std::size_t>& raiiTargetLocal,
        std::optional<std::size_t>& moveOnlyTargetLocal,
        std::optional<std::size_t>& moveOnlyTargetParameter,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool analyzeAssignmentLeft(
        ExpressionResult& left,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void trackRequiredFieldAssignment(
        const ExpressionResult& left,
        std::size_t methodMemberOrdinal);
    void classifyAssignedUnion(
        std::optional<std::size_t>& assignedUnionStorage,
        std::optional<std::size_t>& assignedUnionMember,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal) const;
    void classifyArrayElementAssignmentTarget(
        bool& arrayElementMoveTarget,
        bool& ownedArrayElementOwnsStorage,
        bool& borrowedArrayElementTarget,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const std::optional<std::size_t>& raiiTargetLocal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void classifyOwnedFieldAssignmentTarget(
        bool& ownedFieldMoveTarget,
        bool& genericFieldMoveTarget,
        bool& ownedFieldOwnsStorage,
        std::string& ownedFieldName,
        const ExpressionResult& left,
        const std::optional<std::size_t>& raiiTargetLocal,
        bool arrayElementMoveTarget,
        std::size_t methodMemberOrdinal) const;
    std::optional<std::size_t> assignmentGetterBindingIndex(
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal) const;
    ExpressionResult analyzeEventAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateEventAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzeEventHandler(
        const ast::AstNode* expression,
        const ClassMemberSymbol& event,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordEventAssignmentBinding(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ClassMemberSymbol& event,
        std::size_t backingMemberOrdinal,
        const ProjectImplicitConversion& conversion,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    bool rejectUnavailableAssignmentOwner(
        const ast::AstNode* expression,
        const std::optional<std::size_t>& raiiTargetLocal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzeAssignmentRight(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const std::optional<std::size_t>& raiiTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetParameter,
        bool arrayElementMoveTarget,
        bool ownedFieldMoveTarget,
        bool genericFieldMoveTarget,
        bool ownedSetterMoveTarget,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rejectMoveOnlyAssignmentCopy(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void classifyNamedAssignmentTarget(
        std::optional<std::size_t>& assignmentTargetLocal,
        std::optional<std::size_t>& assignmentTargetParameter,
        const ast::AstNode* expression,
        std::size_t scopeOrdinal) const;
    bool validateAssignmentTargetStorage(
        const ast::AstNode* expression,
        const ExpressionResult& right,
        const std::optional<std::size_t>& assignmentTargetLocal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordViewAssignmentRoot(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal);
    static void appendUniqueOrdinal(
        std::vector<std::size_t>& values,
        std::size_t value);
    bool classifyBorrowedAssignmentDestination(
        bool& borrowedReassignment,
        MethodLocalBorrowBinding*& targetLocalBorrow,
        const ClassMemberSymbol*& enclosingMethod,
        const ast::AstNode* expression,
        const std::optional<std::size_t>& assignmentTargetLocal,
        const std::optional<std::size_t>& assignmentTargetParameter,
        std::size_t methodMemberOrdinal);
    void collectBorrowedAssignmentOwners(
        std::vector<std::size_t>& ownerLocals,
        std::vector<std::size_t>& ownerParameters,
        const ast::AstNode* expression,
        const ClassMemberSymbol* enclosingMethod,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal) const;
    bool validateBorrowedAssignmentOwnerScopes(
        const std::vector<std::size_t>& ownerLocals,
        const ast::AstNode* expression,
        const MethodLocalBorrowBinding* targetLocalBorrow,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void recordBorrowedAssignmentOwners(
        const std::vector<std::size_t>& ownerLocals,
        const std::vector<std::size_t>& ownerParameters,
        const ast::AstNode* expression,
        MethodLocalBorrowBinding* targetLocalBorrow,
        const std::optional<std::size_t>& assignmentTargetParameter,
        std::size_t methodMemberOrdinal);
    bool analyzeBorrowedReassignment(
        bool& borrowedReassignment,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        const std::optional<std::size_t>& assignmentTargetLocal,
        const std::optional<std::size_t>& assignmentTargetParameter,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rejectBorrowedAssignmentEscape(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        bool borrowedReassignment,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool rejectOwnerCopyAssignment(
        const ast::AstNode* expression,
        bool borrowedReassignment,
        bool borrowedArrayElementTarget,
        const std::optional<std::size_t>& raiiTargetLocal,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateOwningStorageAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& right,
        bool ownedFieldOwnsStorage,
        bool ownedArrayElementOwnsStorage,
        const std::string& ownedFieldName,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateRaiiAssignmentTarget(
        const ast::AstNode* expression,
        const std::optional<std::size_t>& raiiTargetLocal,
        bool raiiStringAppend,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateMoveOnlyAssignmentTarget(
        const ast::AstNode* expression,
        const std::optional<std::size_t>& moveOnlyTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetParameter,
        diagnostics::DiagnosticEngine& diagnostics) const;
    const MethodCallBinding* assignmentSetterGetter(
        const std::optional<std::size_t>& getterBindingIndex) const;
    bool validatePropertySetterAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool collectPropertySetterCandidates(
        std::vector<std::size_t>& setters,
        std::string& setterName,
        const ast::AstNode* expression,
        const MethodCallBinding& getterBinding,
        const ClassMemberSymbol& getter,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void appendRecordedSetterExpression(
        std::vector<ExpressionResult>& setterArguments,
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal) const;
    void collectPropertySetterArguments(
        std::vector<ExpressionResult>& setterArguments,
        const ast::AstNode* expression,
        const ClassMemberSymbol& getter,
        std::size_t methodMemberOrdinal) const;
    bool validatePropertySetterReevaluation(
        const ast::AstNode* expression,
        const MethodCallBinding& getterBinding,
        const ClassMemberSymbol& getter,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool preparePropertySetterValue(
        ExpressionResult& setterValue,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        const MethodCallBinding& getterBinding,
        const ClassMemberSymbol& getter,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzePropertySetterAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        const MethodCallBinding& getterBinding,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<ExpressionResult> analyzeExtensionPropertyAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult analyzeNullConditionalIndexAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        bool ownedArrayElementOwnsStorage,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult analyzeNullConditionalCompoundAssignment(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        bool ownedFieldOwnsStorage,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validatePlainAssignmentConversion(
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const ExpressionResult& right,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void updatePlainAssignmentState(
        const ast::AstNode* expression,
        const ExpressionResult& right,
        const std::optional<std::size_t>& raiiTargetLocal,
        const std::optional<std::size_t>& assignmentTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetParameter,
        const std::optional<std::size_t>& assignedUnionStorage,
        const std::optional<std::size_t>& assignedUnionMember,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
    ExpressionResult finishPlainAssignment(
        const ast::AstNode* expression,
        ExpressionResult left,
        const ExpressionResult& right,
        const std::optional<std::size_t>& raiiTargetLocal,
        const std::optional<std::size_t>& assignmentTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetParameter,
        const std::optional<std::size_t>& assignedUnionStorage,
        const std::optional<std::size_t>& assignedUnionMember,
        bool ownsStorageDestination,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult finishPointerCompoundAssignment(
        const ast::AstNode* expression,
        ExpressionResult left,
        const ExpressionResult& right,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult finishNumericCompoundAssignment(
        const ast::AstNode* expression,
        ExpressionResult left,
        const ExpressionResult& right,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool prepareAssignmentTarget(
        ExpressionResult& left,
        std::optional<std::size_t>& raiiTargetLocal,
        std::optional<std::size_t>& moveOnlyTargetLocal,
        std::optional<std::size_t>& moveOnlyTargetParameter,
        std::optional<std::size_t>& assignedUnionStorage,
        std::optional<std::size_t>& assignedUnionMember,
        bool& arrayElementMoveTarget,
        bool& ownedArrayElementOwnsStorage,
        bool& borrowedArrayElementTarget,
        bool& ownedFieldMoveTarget,
        bool& genericFieldMoveTarget,
        bool& ownedFieldOwnsStorage,
        std::string& ownedFieldName,
        std::optional<std::size_t>& getterBindingIndex,
        bool& ownedSetterMoveTarget,
        const ast::AstNode* expression,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool prepareAssignmentValue(
        ExpressionResult& right,
        std::optional<std::size_t>& assignmentTargetLocal,
        std::optional<std::size_t>& assignmentTargetParameter,
        bool& borrowedReassignment,
        bool& delegateTarget,
        bool& raiiStringAppend,
        bool& ownsStorageDestination,
        const ast::AstNode* expression,
        const ExpressionResult& left,
        const std::optional<std::size_t>& raiiTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetParameter,
        bool arrayElementMoveTarget,
        bool ownedArrayElementOwnsStorage,
        bool borrowedArrayElementTarget,
        bool ownedFieldMoveTarget,
        bool genericFieldMoveTarget,
        bool ownedFieldOwnsStorage,
        const std::string& ownedFieldName,
        bool ownedSetterMoveTarget,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult finishAssignment(
        const ast::AstNode* expression,
        ExpressionResult left,
        const ExpressionResult& right,
        const std::optional<std::size_t>& getterBindingIndex,
        const std::optional<std::size_t>& raiiTargetLocal,
        const std::optional<std::size_t>& assignmentTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetLocal,
        const std::optional<std::size_t>& moveOnlyTargetParameter,
        const std::optional<std::size_t>& assignedUnionStorage,
        const std::optional<std::size_t>& assignedUnionMember,
        bool ownedFieldOwnsStorage,
        bool ownedArrayElementOwnsStorage,
        bool raiiStringAppend,
        bool ownsStorageDestination,
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
    bool evaluateSyntheticCandidates(
        MethodCallBinding& call,
        const std::vector<ExpressionResult>& arguments);
    std::vector<std::size_t> viableSyntheticCandidates(
        const MethodCallBinding& call) const;
    void diagnoseSyntheticNoViable(
        const ast::AstNode* syntax,
        const MethodCallBinding& call,
        const std::vector<ExpressionResult>& arguments,
        bool implicitOperatorCall,
        diagnostics::DiagnosticEngine& diagnostics);
    std::vector<std::size_t> nonDominatedSyntheticCandidates(
        const MethodCallBinding& call,
        const std::vector<std::size_t>& viableEvaluationOrdinals) const;
    bool validateSyntheticReadonlyReceiver(
        const ast::AstNode* syntax,
        MethodCallBinding& call,
        const MethodCallCandidate& selected,
        bool diagnoseFailure,
        bool readonlyReceiver,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult finishSyntheticCall(
        const ast::AstNode* syntax,
        MethodCallBinding& call,
        const MethodCallCandidate& selected,
        const std::vector<ExpressionResult>& arguments,
        bool diagnoseFailure,
        diagnostics::DiagnosticEngine& diagnostics);
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
    bool resolveLambdaDelegateSignature(
        const ast::AstNode* syntax,
        std::string_view delegateTypeKey,
        bool inferAnonymous,
        std::string& returnType,
        std::vector<std::string>& parameterTypes,
        std::size_t& delegateOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    MethodLambdaBinding createLambdaBinding(
        const ast::AstNode* syntax,
        std::string_view delegateTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        std::size_t lambdaOrdinal,
        std::size_t delegateOrdinal,
        bool inferAnonymous,
        std::string_view returnType,
        const std::vector<std::string>& parameterTypes) const;
    std::vector<const ast::AstNode*> delegateLambdaParameters(
        std::size_t delegateOrdinal) const;
    std::string lambdaParameterValueType(
        std::string_view parameterType,
        bool byReference) const;
    bool bindLambdaParameter(
        const ast::AstNode* syntax,
        const ast::AstNode* parameterSyntax,
        const ast::AstNode* delegateParameter,
        std::string_view parameterType,
        std::size_t parameterOrdinal,
        std::size_t methodMemberOrdinal,
        std::size_t lambdaScope,
        bool inferAnonymous,
        std::unordered_set<std::string>& parameterNames,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool bindLambdaParameters(
        const ast::AstNode* syntax,
        std::span<const ast::AstNode* const> delegateParameters,
        const std::vector<std::string>& parameterTypes,
        std::size_t methodMemberOrdinal,
        std::size_t lambdaScope,
        bool inferAnonymous,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindLocalFunctionSelf(
        const ast::AstNode* syntax,
        std::span<const std::string> parameterTypes,
        std::string_view returnType,
        std::size_t methodMemberOrdinal,
        std::size_t lambdaScope,
        const MethodLambdaBinding& binding);
    bool analyzeLambdaSyntaxBody(
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal,
        std::size_t lambdaScope,
        bool inferAnonymous,
        std::string& returnType,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool analyzeLambdaBodyAndCaptures(
        const ast::AstNode* syntax,
        std::size_t methodMemberOrdinal,
        std::size_t lambdaScope,
        bool inferAnonymous,
        std::string& returnType,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateStaticLambdaCaptures(
        const ast::AstNode* syntax,
        const MethodLambdaBinding& binding,
        bool bodyValid,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool computeLambdaClosureLayout(
        const ast::AstNode* syntax,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateLambdaEnvironmentRestrictions(
        const ast::AstNode* syntax,
        std::string_view delegateTypeKey,
        std::size_t methodMemberOrdinal,
        bool inferAnonymous,
        const MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::string lambdaResultTypeKey(
        const ast::AstNode* syntax,
        std::span<const std::string> parameterTypes,
        std::string_view returnType,
        std::size_t methodMemberOrdinal,
        bool inferAnonymous,
        bool bodyValid,
        MethodLambdaBinding& binding) const;
    bool collectLambdaCaptures(
        MethodLambdaBinding& binding,
        std::size_t nameBindingBegin,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::unordered_map<std::string, language::CaptureMode> explicitLambdaCaptureModes(
        MethodLambdaBinding& binding) const;
    std::optional<language::CaptureMode> requestedLambdaCaptureMode(
        const MethodLambdaBinding& binding,
        const std::unordered_map<std::string, language::CaptureMode>& explicitModes,
        std::unordered_set<std::string>& reportedImplicitCaptures,
        std::string_view name,
        SourceRange range,
        bool& safe,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::size_t addLambdaCapture(
        MethodLambdaBinding& target,
        MethodLambdaCapture capture) const;
    MethodLambdaCapture routeLambdaCaptureThroughParents(
        MethodLambdaCapture capture);
    bool validateLambdaLocalCaptureLifetime(
        const MethodNameBinding& name,
        const MethodLocalSymbol& local,
        language::CaptureMode mode,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateLambdaLocalCapture(
        const MethodNameBinding& name,
        const MethodLocalSymbol& local,
        language::CaptureMode mode,
        const MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool captureLambdaLocal(
        const MethodNameBinding& name,
        const MethodLocalSymbol& local,
        language::CaptureMode mode,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateLambdaParameterCaptureLifetime(
        const MethodNameBinding& name,
        const MethodParameterSymbol& parameter,
        language::CaptureMode mode,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateLambdaParameterCapture(
        const MethodNameBinding& name,
        const MethodParameterSymbol& parameter,
        language::CaptureMode mode,
        const MethodLambdaBinding& binding,
        bool& moveTracked,
        diagnostics::DiagnosticEngine& diagnostics);
    bool captureLambdaParameter(
        const MethodNameBinding& name,
        const MethodParameterSymbol& parameter,
        language::CaptureMode mode,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool captureLambdaThis(
        const MethodNameBinding& name,
        language::CaptureMode mode,
        std::size_t methodMemberOrdinal,
        MethodLambdaBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    bool methodNameBindingNeedsThis(const MethodNameBinding& name) const noexcept;
    void collectLambdaNameCapture(
        const MethodNameBinding& name,
        const std::unordered_map<std::string, language::CaptureMode>& explicitModes,
        std::unordered_set<std::string>& reportedImplicitCaptures,
        std::size_t methodMemberOrdinal,
        MethodLambdaBinding& binding,
        bool& safe,
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
    std::vector<std::size_t> compatibleDelegateMethods(
        const ExpressionResult& methodGroup,
        const std::vector<std::string>& parameterTypes,
        std::string_view returnType,
        std::string_view expectedErrorType) const;
    bool validateDelegateMethodTarget(
        const ast::AstNode* syntax,
        const ExpressionResult& methodGroup,
        std::string_view delegateTypeKey,
        std::size_t selected,
        const ClassMemberSymbol& target,
        bool& interfaceDispatch,
        bool& virtualDispatch,
        diagnostics::DiagnosticEngine& diagnostics) const;
    ExpressionResult recordMethodGroupDelegateBinding(
        const ast::AstNode* syntax,
        const ExpressionResult& methodGroup,
        std::string_view delegateTypeKey,
        std::size_t delegateOrdinal,
        std::size_t selected,
        const ClassMemberSymbol& target,
        bool interfaceDispatch,
        bool virtualDispatch,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal);
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
    std::vector<language::EscapeClass>* mutableParameterEscapeSummary(
        std::size_t callableOrdinal) noexcept;
    void noteParameterEscape(
        std::size_t callableOrdinal,
        std::size_t parameterOrdinal,
        language::EscapeClass escapeClass);
    void finalizeEscapeSummaries(diagnostics::DiagnosticEngine& diagnostics);
    void noteCapturedParameterEscapes();
    std::size_t initializeEscapeCells(
        std::vector<std::size_t>& callableCellOffsets,
        std::vector<std::size_t>& cellCallableOrdinals,
        std::vector<std::size_t>& cellParameterOrdinals);
    void addEscapeDependencies(
        std::size_t callerOrdinal,
        std::size_t scopeOrdinal,
        std::size_t calleeOrdinal,
        std::span<const std::size_t> parameterOrdinals,
        std::span<const ast::AstNode* const> argumentSyntaxes,
        std::span<const std::size_t> callableCellOffsets,
        std::vector<std::vector<std::size_t>>& dependents);
    void buildEscapeDependencies(
        std::span<const std::size_t> callableCellOffsets,
        std::vector<std::vector<std::size_t>>& dependents);
    void propagateEscapeDependencies(
        std::span<const std::size_t> cellCallableOrdinals,
        std::span<const std::size_t> cellParameterOrdinals,
        const std::vector<std::vector<std::size_t>>& dependents);
    void validateEscapeCall(
        std::size_t scopeOrdinal,
        std::string_view calleeName,
        std::size_t calleeOrdinal,
        std::span<const std::size_t> parameterOrdinals,
        std::span<const ast::AstNode* const> argumentSyntaxes,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateEscapeCalls(diagnostics::DiagnosticEngine& diagnostics);
    // C4 "language references": (method member ordinal, parameter ordinal) pairs an `out`
    // parameter was assigned through. See verifyOutParameterAssignments for what is checked with
    // it and for the precise limit of that check.
    std::set<std::pair<std::size_t, std::size_t>> assignedOutParameters_;
    // NS304-T09-F01: pairs proven assigned on every normal exit of a constructor. The active
    // per-path state lives in OwnershipFlowSnapshot; this set is only the finalized result.
    std::set<std::pair<std::size_t, std::size_t>> assignedRequiredFields_;
    std::unordered_set<std::size_t> constructorsWithNormalExit_;
    std::unordered_set<std::size_t> activeRequiredFieldAssignments_;
    bool hasRequiredConstructorExit_ = false;
    std::unordered_set<std::size_t> requiredConstructorExitAssignments_;
    // NS304-T09-F07: tipo objetivo activo durante el análisis de un `new()` target-typed.
    std::string activeTargetTypeKey_;
    // NS304-T10-F05: true mientras se analiza el LHS de un null-conditional write.
    bool activeNullConditionalWrite_ = false;
    std::unordered_set<std::size_t> analyzedCallableMembers_;
    void verifyOutParameterAssignments(diagnostics::DiagnosticEngine& diagnostics) const;
    void verifyRequiredFieldAssignments(diagnostics::DiagnosticEngine& diagnostics) const;
    // EN: Freeze 3.0 C-02 and D-04. A synchronous `await` (outside an async callable) polls its
    //     future once, so a producer known to suspend is a compile error. A source async producer
    //     suspends when its body has `yield;` or awaits, directly or through other producers, one
    //     that does. The origins of a future are the producers named by the calls that can reach it
    //     (a local's initializers and assignments, the generator of a local iterator). An unknown
    //     origin (parameter, field, a future returned by a non-async or precompiled callable) is
    //     not "suspends": it is accepted, and if it is still pending after the poll the program
    //     panics (DEC-04). `memo` holds 1 (does not suspend) or 2 (suspends).
    // ES: Congelamiento 3.0 C-02 y D-04. Un `await` sincrono (fuera de un callable async) hace un
    //     solo poll de su future, asi que un productor que se sabe que se suspende es error de
    //     compilacion. Un productor async de fuente se suspende si su cuerpo tiene `yield;` o
    //     espera, directamente o a traves de otros productores, a uno que se suspende. Los origenes
    //     de un future son los productores que nombran las llamadas que pueden llegar a el (los
    //     inicializadores y asignaciones de un local, el generador de un iterador local). Un origen
    //     desconocido (parametro, campo, un future devuelto por un callable no async o precompilado)
    //     no es «se suspende»: se acepta, y si sigue pendiente despues del poll el programa entra
    //     en panico (DEC-04). `memo` guarda 1 (no se suspende) o 2 (se suspende).
    void verifySynchronousAwaits(diagnostics::DiagnosticEngine& diagnostics) const;
    // EN: Freeze 3.0 C-05 (found): a method with a result whose block body can reach its end.
    // ES: Congelamiento 3.0 C-05 (hallazgo): un metodo con resultado cuyo cuerpo puede llegar al final.
    void reportMissingReturn(
        const ClassMemberSymbol& callable, diagnostics::DiagnosticEngine& diagnostics) const;
    bool awaitProducerCanSuspend(
        std::size_t producerOrdinal,
        std::unordered_map<std::size_t, int>& memo) const;
    bool awaitOperandCanSuspend(
        const ast::AstNode* operand,
        const ast::AstNode* callerBody,
        std::size_t callerOrdinal,
        std::unordered_map<std::size_t, int>& memo) const;
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
    // NativeSharp 3.0 records synthesize field-wise operations only when every value nested in
    // the physical aggregate can honor the same contract. Equality/hash cannot tunnel through a
    // record that did not opt in, and Clone is a plain value copy so it cannot duplicate a
    // destructor or owned storage. `reason` names the first offending field path.
    bool aggregateSupportsSynthesizedMember(
        std::string_view typeKey,
        std::string_view memberName,
        std::string& reason) const;
    bool aggregateSupportsSynthesizedMemberVisit(
        std::string_view currentTypeKey,
        std::string_view requiredDerive,
        std::string_view memberName,
        std::string& reason,
        std::unordered_set<std::string>& activeTypes,
        const std::string& path,
        bool nested) const;
    bool aggregateTupleSupportsSynthesizedMember(
        std::span<const std::string> elements,
        std::string_view requiredDerive,
        std::string_view memberName,
        std::string& reason,
        std::unordered_set<std::string>& activeTypes,
        const std::string& path) const;
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
    // ES: Origen del VALOR de una expresión puntero (a dónde apunta), ver
    // MethodLocalSymbol::pointerValueOrigin. EN: Origin of a pointer expression's VALUE (where it
    // points), see MethodLocalSymbol::pointerValueOrigin.
    MethodEscapeOrigin pointerValueEscapeOriginOfExpression(
        const ast::AstNode* expression,
        std::size_t scopeOrdinal) const;
    static void joinPointerValueOrigin(
        MethodEscapeOrigin& target,
        const MethodEscapeOrigin& source) noexcept;
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
    void validateAggregateInitializerCompleteness(
        const ast::AstNode* syntax,
        std::string_view targetTypeKey,
        const MethodAggregateInitializerBinding& binding,
        std::span<const std::size_t> missing,
        bool nontrivialTarget,
        bool hasMoveSpread,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics) const;
    std::vector<std::optional<std::size_t>> aggregateElementMemberOrdinals(
        std::string_view targetTypeKey,
        std::size_t elementCount,
        std::optional<std::size_t>& targetTypeOrdinal) const;
    bool aggregateInitializerIsGeneralized(const ast::AstNode* syntax) const;
    void reserveNamedAggregateTargets(
        const ast::AstNode* syntax,
        std::string_view targetTypeKey,
        std::span<const std::string> names,
        std::vector<bool>& reservedByName,
        std::vector<std::optional<std::size_t>>& namedTargets,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool resolveAggregateMoveSpreadRoot(
        const ast::AstNode* sourceValue,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        MethodAggregateInitializerEntryBinding& entry,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateAggregateMoveSpread(
        const ast::AstNode* element,
        const ast::AstNode* sourceValue,
        std::string_view targetTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        MethodAggregateInitializerEntryBinding& entry,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindAggregateSpreadEntry(
        const ast::AstNode* element,
        std::string_view targetTypeKey,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool allowMoveSpread,
        bool nontrivialTarget,
        MethodAggregateInitializerEntryBinding& entry,
        bool& hasMoveSpread,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> resolveAggregateEntryTarget(
        const ast::AstNode* syntax,
        const ast::AstNode* element,
        std::span<const std::string> elements,
        const std::vector<bool>& assigned,
        const std::vector<bool>& reservedByName,
        std::optional<std::size_t> namedTarget,
        std::size_t& nextPositional,
        std::string_view targetTypeKey,
        bool generalized,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateAggregateEntryAccessibility(
        const ast::AstNode* element,
        std::size_t target,
        std::span<const std::optional<std::size_t>> elementMemberOrdinals,
        std::optional<std::size_t> targetTypeOrdinal,
        std::size_t methodMemberOrdinal,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool bindAggregateValueEntry(
        const ast::AstNode* element,
        std::size_t target,
        std::string_view elementTypeKey,
        std::string_view targetTypeKey,
        std::span<const std::string> names,
        std::span<const std::optional<std::size_t>> elementMemberOrdinals,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics);
    void markAggregateMoveSpreadFields(
        const ast::AstNode* syntax,
        std::span<const std::string> names,
        std::span<const std::size_t> missing,
        MethodAggregateInitializerEntryBinding& spread,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics);
    void finalizeAggregateSpread(
        const ast::AstNode* syntax,
        std::span<const std::string> names,
        std::vector<bool>& assigned,
        std::vector<std::size_t>& missing,
        std::optional<std::size_t> spreadEntry,
        MethodAggregateInitializerBinding& binding,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics);
    std::vector<std::size_t> bindAggregateInitializerEntries(
        const ast::AstNode* syntax,
        std::string_view targetTypeKey,
        std::span<const std::string> elements,
        std::span<const std::string> names,
        std::span<const std::optional<std::size_t>> elementMemberOrdinals,
        std::optional<std::size_t> targetTypeOrdinal,
        const std::vector<bool>& reservedByName,
        std::span<const std::optional<std::size_t>> namedTargets,
        std::size_t methodMemberOrdinal,
        std::size_t scopeOrdinal,
        bool allowMoveSpread,
        bool nontrivialTarget,
        bool& hasMoveSpread,
        MethodAggregateInitializerBinding& binding,
        bool& valid,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<MethodDeconstructionBinding> bindDeconstructionSource(
        const ast::AstNode* syntax,
        const ast::AstNode* sourceSyntax,
        const ExpressionResult& source,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectDeconstructCandidates(
        const ast::AstNode* syntax,
        const ast::AstNode* sourceSyntax,
        const ExpressionResult& source,
        std::size_t receiverType,
        const ClassMemberSymbol& caller,
        bool& namedCandidate,
        bool& inaccessibleCandidate,
        std::vector<std::size_t>& validCandidates,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateSelectedDeconstruct(
        const ast::AstNode* syntax,
        const ast::AstNode* sourceSyntax,
        const ExpressionResult& source,
        const ClassMemberSymbol& selected,
        diagnostics::DiagnosticEngine& diagnostics) const;
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
    bool scanResultCarrierContracts(
        std::size_t typeOrdinal,
        std::string_view typeKey,
        std::array<std::string, 2>& payloadTypeKeys,
        std::array<std::optional<std::size_t>, 2>& tags,
        std::array<std::optional<std::size_t>, 2>& factories,
        std::array<std::optional<std::size_t>, 2>& tests,
        std::array<std::optional<std::size_t>, 2>& extractors,
        bool& sawUnexpectedCase) const;
    // True only for a "type:" key that actually names an enum. Structs, classes and delegates
    // share that prefix, so the prefix alone answers nothing. Mirrors the identical predicate the
    // LLVM lowerer keeps for mapping an enum key to i32.
    const NamedTypeDeclaration* namedTypeForTypeKey(std::string_view typeKey) const noexcept;
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
    void initializeNamespaceCallableAdapters(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel);
    [[nodiscard]] std::optional<std::size_t> resolveKnownCoreTypeOrdinal(
        std::string_view fullyQualifiedName,
        NamedTypeDeclarationKind kind) const noexcept;
    void initializeKnownCoreTypes();
    void validateDeclaredErrorTypes(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeParameterEscapeSummaries(const ProjectMemberModel& memberModel);
    bool isDeferredGenericMethod(
        std::size_t ordinal,
        const ProjectMemberModel& memberModel,
        const ProjectNameLookup& nameLookup) const;
    bool declaringTypeIsPubliclyReachable(
        std::size_t typeOrdinal,
        const ProjectNameLookup& nameLookup,
        std::optional<std::size_t> publicSurfaceProjectOrdinal) const;
    void seedDemandedGenericMethods(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInterfaceModel& interfaceModel,
        std::optional<std::size_t> publicSurfaceProjectOrdinal,
        std::unordered_set<std::size_t>& demandedGenericMethods) const;
    std::vector<std::size_t> createCallableWorklist(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const std::unordered_set<std::size_t>& demandedGenericMethods) const;
    bool analyzeCallableWaves(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectGenericModel* genericModel,
        const ProjectGenericConstraintModel* genericConstraintModel,
        const ProjectMethodBodyBuildOptions& options,
        std::unordered_set<std::size_t>& demandedGenericMethods,
        std::vector<std::size_t>& callableWorklist,
        diagnostics::DiagnosticEngine& diagnostics);
    std::vector<std::size_t> collectEligibleCallables(
        const std::vector<std::size_t>& callableWorklist,
        std::size_t& workCursor,
        std::size_t waveOrdinal,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectGenericModel* genericModel,
        const ProjectGenericConstraintModel* genericConstraintModel);
    void mergeCallableWave(
        std::vector<ProjectCallableAnalysisResult> waveResults,
        std::size_t waveOrdinal,
        std::size_t analyzedCount,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectMethodBodyBuildOptions& options,
        std::unordered_set<std::size_t>& demandedGenericMethods,
        std::vector<std::size_t>& callableWorklist,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeFieldInitializers(
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics);
    void analyzeFieldInitializer(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& field,
        diagnostics::DiagnosticEngine& diagnostics);
    ExpressionResult bindFieldInitializerExpression(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& field,
        std::size_t rootScopeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateFieldInitializerConversion(
        const ClassMemberSymbol& field,
        const ExpressionResult& initializer,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateConstantFieldInitializers(
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateRequiredInliningCycles(
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void resetCallableScratch(const ClassMemberSymbol& callable);
    void recordRequiredConstructorNormalExit(std::size_t callableOrdinal);
    void finalizeRequiredConstructorAssignments(
        const ClassMemberSymbol& callable, std::size_t callableOrdinal);
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
    std::optional<std::size_t> coreExceptionTypeOrdinal_;
    std::optional<std::size_t> coreDisposableTypeOrdinal_;
    std::optional<std::size_t> coreStringTypeOrdinal_;
    std::string coreStringViewTypeKey_;
    std::string coreThreadStartTypeKey_;
    std::string coreMemoryOrderTypeKey_;
    std::string coreReadOnlyByteSpanTypeKey_;
    std::vector<MethodScope> scopes_;
    std::vector<std::vector<ScopeName>> scopeNames_;
    std::vector<MethodLocalSymbol> locals_;
    // NS304-T05-F03: puntos `await` en orden fuente (indice de estado = posicion + 1). Los consume
    // el lowering de state machine (HIR/MIR) y buildAsyncFunctionPlan.
    std::vector<const ast::AstNode*> awaitExpressions_;
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
#include "nativesharp/Frontend/Internal/MethodBody/ActiveIterationBorrow.h"
    std::vector<ActiveIterationBorrow> activeIterationBorrows_;
    std::unordered_set<const ast::AstNode*> arrayDeleteClearsOwnedField_;
    std::unordered_set<std::size_t> movedRaiiLocals_;
    std::unordered_set<std::size_t> maybeMovedRaiiLocals_;
    std::unordered_set<std::size_t> movedOwnedParameters_;
    std::unordered_set<std::size_t> maybeMovedOwnedParameters_;
    std::unordered_set<std::size_t> invokedFnOnceLocals_;
    std::unordered_set<std::size_t> maybeInvokedFnOnceLocals_;
    std::unordered_set<std::string> partiallyMovedPlaces_;
    std::unordered_set<std::string> maybePartiallyMovedPlaces_;
    // NS304-T07-F04: the place keys of async-iterators whose move-only item was already moved out
    // via move(it.Current). A second move on the same iterator is diagnosed.
    std::unordered_set<std::string> movedIteratorItems_;
    std::unordered_set<std::size_t> invalidLocalBorrows_;
    std::unordered_set<std::size_t> maybeInvalidLocalBorrows_;
    // NS304-T07-F06: locals whose address a TaskScope.Spawn(Child(ref value)) captured; alive until
    // a matching Join discharges them. Block exit while present rejects the dangling Future.
    std::unordered_set<std::size_t> scopedChildBorrows_;
    // NS304-T11-F07: per-field storage origin recorded by a `field = ref storage` assignment, keyed
    // by the ref field's member ordinal. The escape evaluator reads it so `ref`-returning member
    // access propagates the storage origin (not the receiver struct value).
    std::unordered_map<std::size_t, MethodEscapeOrigin> refFieldOrigins_;
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
    // One diagnostic per offending read: a statement can be analyzed more than once (the
    // unreachable-code probe in analyzeBlock does exactly that), and that must not multiply the
    // same message.
    std::unordered_set<const ast::AstNode*> reportedUnassignedReads_;
    bool inUnevaluatedOperand_ = false;
    bool formingAddressOperand_ = false;
    bool ownershipFlowReachable_ = true;
    // EN: Freeze 3.0 C-05. Per lowered match statement (its `$__nsc_match_N` temporary), whether
    //     some arm body reached its end; an exhaustive match whose arms all exit ends the flow.
    // ES: Congelamiento 3.0 C-05. Por cada match bajado (su temporal `$__nsc_match_N`), si algun
    //     cuerpo de brazo llego a su final; un match exhaustivo cuyos brazos salen todos termina
    //     el flujo.
    std::unordered_map<std::string, bool> matchArmFallthrough_;
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
    std::vector<std::string> currentCatchErrorTypes_;
    // EN: Freeze 3.0 C-13: unfiltered catch types of each try body being analyzed, outermost first.
    // ES: Congelamiento 3.0 C-13: tipos de catch sin filtro de cada cuerpo try en analisis.
    std::vector<std::vector<std::string>> activeErrorHandlers_;
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
    // NS304-T13-F08: Result<T,E> Ok/Error factory ordinals a placement `failure: result` calls.
    std::vector<std::size_t> resultFactoryDiscoveries_;
    std::vector<MethodRangeSliceBinding> rangeSliceBindings_;
    std::vector<MethodCallBinding> calls_;
    ProjectCallableBindingIndex callBindingIndex_;
    std::vector<NamespaceFunctionCallBinding> namespaceFunctionCalls_;
    std::unordered_map<const ast::AstNode*, std::size_t> namespaceFunctionCallBySyntax_;
    std::vector<std::vector<language::EscapeClass>> parameterEscapeSummaries_;
    std::optional<std::size_t> isolatedCallableOrdinal_;
    std::vector<language::EscapeClass> isolatedParameterEscapeSummary_;
    std::size_t currentCallableLocalBase_ = 0;
    std::size_t currentCallableParameterCount_ = 0;
    mutable ProjectOwnershipFlowStatistics ownershipFlowStatistics_;
    mutable ProjectEscapePropagationStatistics escapePropagationStatistics_;
    ProjectCallableAnalysisStatistics callableAnalysisStatistics_;
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
