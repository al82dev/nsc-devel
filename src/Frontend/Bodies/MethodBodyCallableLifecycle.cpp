#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "Support/OwnershipArgumentClassification.h"
#include "Support/OperatorClassification.h"
#include "Support/IntegralConstantArithmetic.h"
#include "Support/OwnershipFlowSnapshotQueries.h"


#include "MethodBodyInternals.h"
#include "../Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/IterationSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

bool ProjectMethodBodyModel::callableCanBeAnalyzed(
    const ProjectCallableAnalysisContext& context) const
{
    const std::size_t callableOrdinal = context.callableOrdinal();
    if (analyzedCallableMembers_.contains(callableOrdinal)) return false;
    const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
    if (callable == nullptr) return false;
    // NS304-T08-F06: an open generic namespace function's body is deferred to its specialization;
    // only the concrete clone (`Twice<I>`) is analyzed.
    if (callable->syntax != nullptr && !callable->syntax->genericParameters().empty())
        return false;
    const ProjectGenericConstraintModel* constraints = context.genericConstraintModel();
    if (callableOrdinal >= namespaceCallableBase_ || constraints == nullptr)
        return true;

    if (!constraints->specializationSatisfiesConstraints(callable->syntax))
        return false;
    const ProjectNameLookup& names = context.nameLookup();
    return callable->declaringTypeOrdinal >= names.namedTypes().size() ||
        constraints->specializationSatisfiesConstraints(
            names.namedTypes()[callable->declaringTypeOrdinal].syntax);
}

void ProjectMethodBodyModel::resetCallableScratch(const ClassMemberSymbol& callable)
{
    currentCallableLocalBase_ = locals_.size();
    currentCallableParameterCount_ = callable.parameters.size();
    movedRaiiLocals_.clear();
    maybeMovedRaiiLocals_.clear();
    movedOwnedParameters_.clear();
    maybeMovedOwnedParameters_.clear();
    invokedFnOnceLocals_.clear();
    maybeInvokedFnOnceLocals_.clear();
    partiallyMovedPlaces_.clear();
    maybePartiallyMovedPlaces_.clear();
    invalidLocalBorrows_.clear();
    maybeInvalidLocalBorrows_.clear();
    assignedLocals_.clear();
    activeUnionMembers_.clear();
    nonNullStorages_.clear();
    integerRanges_.clear();
    activeVariantCases_.clear();
    conditionFlowBySyntax_.clear();
    inUnevaluatedOperand_ = false;
    definiteAssignmentWriteTarget_ = nullptr;
    activeUnionWriteTarget_ = nullptr;
    ownershipFlowReachable_ = true;
    matchArmFallthrough_.clear();
    activeRequiredFieldAssignments_.clear();
    hasRequiredConstructorExit_ = false;
    requiredConstructorExitAssignments_.clear();
    partialProjectionReceiverDepth_ = 0;
    throwFlowStates_.clear();
    awaitExpressions_.clear();
    activeErrorHandlers_.clear();
}

void ProjectMethodBodyModel::recordRequiredConstructorNormalExit(
    std::size_t callableOrdinal)
{
    const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
    if (callable == nullptr || callable->kind != ClassMemberKind::Constructor ||
        callable->isStatic)
        return;

    if (!hasRequiredConstructorExit_)
    {
        requiredConstructorExitAssignments_ = activeRequiredFieldAssignments_;
        hasRequiredConstructorExit_ = true;
        return;
    }
    std::erase_if(
        requiredConstructorExitAssignments_,
        [&](std::size_t fieldOrdinal)
        {
            return !activeRequiredFieldAssignments_.contains(fieldOrdinal);
        });
}

void ProjectMethodBodyModel::finalizeRequiredConstructorAssignments(
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal)
{
    if (callable.kind != ClassMemberKind::Constructor || callable.isStatic) return;
    if (ownershipFlowReachable_) recordRequiredConstructorNormalExit(callableOrdinal);
    if (!hasRequiredConstructorExit_) return;

    constructorsWithNormalExit_.insert(callableOrdinal);
    for (const std::size_t fieldOrdinal : requiredConstructorExitAssignments_)
        assignedRequiredFields_.insert({callableOrdinal, fieldOrdinal});
}

std::vector<std::size_t> ProjectMethodBodyModel::createOwnedParameterCleanups(
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal,
    std::size_t rootScopeOrdinal)
{
    std::vector<std::size_t> cleanupOrdinals;
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < callable.parameters.size(); ++parameterOrdinal)
    {
        const MethodParameterSymbol& parameter = callable.parameters[parameterOrdinal];
        const bool valueDrop = parameter.passKind == ParameterPassKind::Value &&
            memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(parameter.typeKey);
        if (parameter.ownership != sema::OwnershipKind::Owned && !valueDrop) continue;
        const std::size_t cleanupOrdinal = createCleanupRegion(
            valueDrop ? MethodCleanupRegionKind::ValueParameterDrop
                      : MethodCleanupRegionKind::OwnedParameterDestroy,
            callable.syntax, callable.syntax, callableOrdinal, rootScopeOrdinal);
        cleanupRegions_[cleanupOrdinal].resourceParameterOrdinal = parameterOrdinal;
        activeCleanupRegions_.push_back(cleanupOrdinal);
        cleanupOrdinals.push_back(cleanupOrdinal);
    }
    return cleanupOrdinals;
}

void ProjectMethodBodyModel::analyzeConstructorInitializer(
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal,
    std::size_t rootScopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (callable.kind != ClassMemberKind::Constructor || callable.isStatic ||
        callable.syntax->second() == nullptr)
        return;
    const ast::AstNode* initializer = callable.syntax->second();
    if (initializer->kind() != ast::AstNodeKind::BaseConstructorInitializer &&
        initializer->kind() != ast::AstNodeKind::ThisConstructorInitializer)
        return;
    for (const ast::AstNode* argument : initializer->children())
    {
        analyzeExpression(
            argument, callableOrdinal, rootScopeOrdinal, diagnostics,
            /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
    }
}

void ProjectMethodBodyModel::closeOwnedParameterCleanups(
    std::span<const std::size_t> cleanupOrdinals,
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal,
    std::size_t rootScopeOrdinal)
{
    for (std::size_t index = cleanupOrdinals.size(); index > 0; --index)
    {
        recordNormalCleanupContinuation(
            cleanupOrdinals[index - 1], callable.syntax->first(), callableOrdinal, rootScopeOrdinal);
    }
}

void ProjectMethodBodyModel::analyzeCallable(
    const ProjectCallableAnalysisContext& context,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!callableCanBeAnalyzed(context)) return;
    const std::size_t callableOrdinal = context.callableOrdinal();
    const ClassMemberSymbol& callable = *methodSymbol(callableOrdinal);
    analyzedCallableMembers_.insert(callableOrdinal);
    resetCallableScratch(callable);
    const MethodScopeKind rootKind = callable.kind == ClassMemberKind::Constructor
        ? MethodScopeKind::Constructor
        : callable.kind == ClassMemberKind::Destructor
            ? MethodScopeKind::Destructor : MethodScopeKind::Method;
    const std::size_t rootScopeOrdinal = createScope(rootKind, callableOrdinal, std::nullopt);
    seedParameters(callableOrdinal, rootScopeOrdinal, diagnostics);
    const std::vector<std::size_t> cleanupOrdinals =
        createOwnedParameterCleanups(callable, callableOrdinal, rootScopeOrdinal);
    analyzeConstructorInitializer(callable, callableOrdinal, rootScopeOrdinal, diagnostics);
    analyzeBlock(
        callable.syntax->first(), callableOrdinal, rootScopeOrdinal, false, diagnostics);
    // NS304-T05-F05: a pinned local cannot cross a suspension — its fixed-address identity is not
    // preserved across the poll return (the local moves to the frame and back). A borrowed local
    // IS allowed when its source is re-localizable to a frame slot; the MIR drags the borrow's
    // source into the frame and rejects a borrow whose source is a parameter/field (no slot).
    if (callable.effects.contains(sema::Effect::Async) && !awaitExpressions_.empty())
    {
        for (const MethodLocalSymbol& local : locals())
        {
            if (local.isPinned)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    local.range,
                    "async function cannot hold a pinned local across a suspension "
                    "(NS304-T05-F05)");
            }
        }
    }
    reportMissingReturn(callable, diagnostics);
    finalizeRequiredConstructorAssignments(callable, callableOrdinal);
    closeOwnedParameterCleanups(
        cleanupOrdinals, callable, callableOrdinal, rootScopeOrdinal);
    activeCleanupRegions_.clear();
}

std::unique_ptr<ProjectMethodBodyModel> ProjectMethodBodyModel::createCallableShard(
    const ProjectCallableAnalysisContext& context) const
{
    auto shard = std::make_unique<ProjectMethodBodyModel>();
    shard->nameLookup_ = &context.nameLookup();
    shard->memberModel_ = &context.memberModel();
    shard->namespaceFunctionModel_ = &context.namespaceFunctionModel();
    shard->inheritanceModel_ = &context.inheritanceModel();
    shard->interfaceModel_ = &context.interfaceModel();
    shard->genericModel_ = context.genericModel();
    shard->genericConstraintModel_ = context.genericConstraintModel();
    shard->coreExceptionTypeOrdinal_ = coreExceptionTypeOrdinal_;
    shard->coreDisposableTypeOrdinal_ = coreDisposableTypeOrdinal_;
    shard->coreStringTypeOrdinal_ = coreStringTypeOrdinal_;
    shard->coreStringViewTypeKey_ = coreStringViewTypeKey_;
    shard->coreThreadStartTypeKey_ = coreThreadStartTypeKey_;
    shard->coreMemoryOrderTypeKey_ = coreMemoryOrderTypeKey_;
    shard->coreReadOnlyByteSpanTypeKey_ = coreReadOnlyByteSpanTypeKey_;
    shard->namespaceCallableBase_ = namespaceCallableBase_;
    shard->namespaceCallableAdaptersView_ = namespaceCallableAdapters();
    shard->isolatedCallableOrdinal_ = context.callableOrdinal();
    if (context.callableOrdinal() < parameterEscapeSummaries_.size())
    {
        shard->isolatedParameterEscapeSummary_ =
            parameterEscapeSummaries_[context.callableOrdinal()];
    }
    return shard;
}

ProjectCallableAnalysisResult ProjectMethodBodyModel::analyzeCallableIsolated(
    const ProjectCallableAnalysisContext& context) const
{
    const ClassMemberSymbol* callable = methodSymbol(context.callableOrdinal());
    std::string traceDetail = "wave=" + std::to_string(context.waveOrdinal()) +
        "|ordinal=" + std::to_string(context.callableOrdinal());
    if (callable != nullptr)
    {
        traceDetail += "|name=" + callable->name + "|signature=" + callable->signatureKey +
            "|source=" + callable->sourcePath;
    }
    writeMethodBodyTrace("CALLABLE-BEGIN", traceDetail);
    std::unique_ptr<ProjectMethodBodyModel> shard = createCallableShard(context);
    diagnostics::DiagnosticEngine localDiagnostics(
        std::numeric_limits<std::size_t>::max());
    shard->analyzeCallable(context, localDiagnostics);
    ProjectCallableAnalysisResult result(
        context.callableOrdinal(), context.waveOrdinal(),
        std::move(shard), std::move(localDiagnostics));
    collectCallableDiscoveries(result);
    result.normalizeDiscoveredCallables();
    writeMethodBodyTrace("CALLABLE-END", traceDetail);
    return result;
}

void ProjectMethodBodyModel::collectCallableDiscoveries(
    ProjectCallableAnalysisResult& result)
{
    const ProjectMethodBodyModel& shard = result.shard();
    for (const MethodCallBinding& call : shard.calls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedMemberOrdinal.has_value())
            result.addDiscoveredCallable(*call.selectedMemberOrdinal);
    }
    for (const MethodDelegateBinding& binding : shard.delegateBindings_)
    {
        if (binding.kind == MethodDelegateOperationKind::Create &&
            binding.selectedMemberOrdinal.has_value())
            result.addDiscoveredCallable(*binding.selectedMemberOrdinal);
    }
    for (const MethodOptionalCarrierBinding& binding : shard.optionalCarrierBindings_)
    {
        result.addDiscoveredCallable(binding.hasValueMemberOrdinal);
        result.addDiscoveredCallable(binding.uncheckedValueMemberOrdinal);
    }
    for (const MethodResultPropagationBinding& binding : shard.resultPropagationBindings_)
    {
        result.addDiscoveredCallable(binding.sourceSuccessTestMemberOrdinal);
        result.addDiscoveredCallable(binding.sourceSuccessExtractorMemberOrdinal);
        result.addDiscoveredCallable(binding.sourceErrorExtractorMemberOrdinal);
        result.addDiscoveredCallable(binding.returnErrorFactoryMemberOrdinal);
        if (binding.errorConversionMemberOrdinal.has_value())
            result.addDiscoveredCallable(*binding.errorConversionMemberOrdinal);
    }
    for (const std::size_t factoryOrdinal : shard.resultFactoryDiscoveries_)
        result.addDiscoveredCallable(factoryOrdinal);
}


// C4 "language references": an `out` parameter is an OBLIGATION, not a permission -- the caller
// is allowed to pass an uninitialized variable precisely because the callee promises to fill it.
// This is the check that makes the promise mean something.
//
// PRECISION about what this does and does not prove, because the difference matters: it verifies
// that the body assigns the parameter AT LEAST ONCE somewhere. It does NOT prove that every
// return path assigns it -- that is path-sensitive definite assignment, which needs the CFG and
// is a piece of work of its own (this compiler has a CFG, but the body model runs before it).
// So `out int x` with `if (c) { x = 1; }` and no else is accepted here and would be rejected by
// a full analysis. What the check DOES catch is the mistake people actually make: declaring an
// out parameter and never writing it at all, which would hand the caller whatever bytes the
// stack slot happened to hold. Tightening this to full path coverage is a descendant item, and
// it is named as such in the round's documentation rather than left implied.
void ProjectMethodBodyModel::verifyOutParameterAssignments(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return;
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method || member.isAbstract) continue;
        if (member.syntax == nullptr || member.syntax->first() == nullptr) continue;
        if (!analyzedCallableMembers_.contains(memberOrdinal)) continue;
        for (std::size_t index = 0; index < member.parameters.size(); ++index)
        {
            if (member.parameters[index].passKind != ParameterPassKind::Out) continue;
            if (assignedOutParameters_.contains({memberOrdinal, index})) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                member.parameters[index].range,
                "out parameter '" + member.parameters[index].name +
                    "' is never assigned in the body of '" + member.name + "'");
        }
    }
}

namespace
{
// EN: Collects the await expressions and reports whether a bare `yield;` appears in `root`,
//     without entering lambdas (each lambda is a callable of its own).
// ES: Reune las expresiones await e informa si aparece un `yield;` en `root`, sin entrar en
//     lambdas (cada lambda es un callable propio).
void collectAwaitSites(
    const ast::AstNode* root,
    std::vector<const ast::AstNode*>& awaits,
    bool& suspends)
{
    std::unordered_set<const ast::AstNode*> visited;
    std::vector<const ast::AstNode*> pending = {root};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || !visited.insert(node).second) continue;
        if (node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::SuspendStatement) suspends = true;
        if (node->kind() == ast::AstNodeKind::AwaitExpression) awaits.push_back(node);
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
        pending.push_back(node->first());
        pending.push_back(node->second());
    }
    std::sort(awaits.begin(), awaits.end(), [](const ast::AstNode* left, const ast::AstNode* right)
    {
        return left->range().begin().offset() < right->range().begin().offset();
    });
}

// EN: The initializer of the only local named `name` in `body`, when no assignment writes it; that
//     initializer is then the future's origin. Anything else is an unknown origin.
// ES: El inicializador del unico local llamado `name` en `body`, si ninguna asignacion lo escribe;
//     ese inicializador es el origen del future. Cualquier otro caso es origen desconocido.
const ast::AstNode* uniqueLocalInitializer(const ast::AstNode* body, std::string_view name)
{
    const ast::AstNode* initializer = nullptr;
    std::size_t declarations = 0;
    std::unordered_set<const ast::AstNode*> visited;
    std::vector<const ast::AstNode*> pending = {body};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || !visited.insert(node).second) continue;
        if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement && node->text() == name)
        {
            ++declarations;
            initializer = node->first();
        }
        if (node->kind() == ast::AstNodeKind::AssignmentExpression && node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            node->first()->text() == name) return nullptr;
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
        pending.push_back(node->first());
        pending.push_back(node->second());
    }
    return declarations == 1 ? initializer : nullptr;
}

// EN: Freeze 3.0 (await without recursion). The expression whose call binding names the producer of
//     an awaited future: a local is replaced by its unique initializer, and `it.MoveNextAsync()` over
//     a local iterator by the origin of `it` (the generator that created it). Returns nullptr for an
//     unknown origin. A local whose origin leads back to itself is an unknown origin; the recursive
//     version never terminated on it.
// ES: Congelamiento 3.0 (await sin recursion). La expresion cuyo binding de llamada nombra al
//     productor de un future esperado: un local se sustituye por su inicializador unico, y
//     `it.MoveNextAsync()` sobre un iterador local por el origen de `it` (el generador que lo creo).
//     Devuelve nullptr si el origen es desconocido. Un local cuyo origen vuelve a si mismo es origen
//     desconocido; la version recursiva nunca terminaba con el.
const ast::AstNode* awaitOperandOrigin(const ast::AstNode* operand, const ast::AstNode* callerBody)
{
    std::unordered_set<const ast::AstNode*> visited;
    while (operand != nullptr)
    {
        if (!visited.insert(operand).second) return nullptr;
        if (operand->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            operand = uniqueLocalInitializer(callerBody, operand->text());
            if (operand == nullptr) return nullptr;
        }
        const bool localIteratorStep = operand->kind() == ast::AstNodeKind::CallExpression &&
            operand->first() != nullptr && operand->first()->kind() == ast::AstNodeKind::MemberExpression &&
            operand->first()->text() == "MoveNextAsync" && operand->first()->first() != nullptr &&
            operand->first()->first()->kind() == ast::AstNodeKind::IdentifierExpression;
        if (!localIteratorStep) return operand;
        operand = operand->first()->first();
    }
    return nullptr;
}

// EN: The producer named by a resolved call binding; nullopt means an unknown producer.
// ES: El productor que nombra un binding de llamada resuelto; nullopt es productor desconocido.
std::optional<std::size_t> boundAwaitProducer(const MethodCallBinding* binding)
{
    if (binding == nullptr || binding->resolution != MethodCallResolutionKind::Resolved) return std::nullopt;
    return binding->selectedMemberOrdinal;
}

// EN: One producer under analysis in ProjectMethodBodyModel::awaitProducerCanSuspend: its body, its
//     await sites in source order, the next site to examine and whether it is already known to
//     suspend. The stack of frames replaces the former mutual recursion through
//     awaitOperandCanSuspend.
// ES: Un productor en analisis en ProjectMethodBodyModel::awaitProducerCanSuspend: su cuerpo, sus
//     awaits en orden de fuente, el siguiente a examinar y si ya se sabe que se suspende. La pila de
//     frames reemplaza la antigua recursion mutua a traves de awaitOperandCanSuspend.
struct AwaitProducerFrame final
{
    std::size_t ordinal = 0;
    const ast::AstNode* body = nullptr;
    std::vector<const ast::AstNode*> awaits;
    std::size_t next = 0;
    bool suspends = false;
};
}

void ProjectMethodBodyModel::reportMissingReturn(
    const ClassMemberSymbol& callable,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // EN: Freeze 3.0 C-05 (found while porting the single-file cases): the 3.x pipeline accepted
    //     a method whose body falls off its end without a value, and the backend closed that path
    //     with `unreachable`, so the call had undefined behavior. A generator's end is its normal
    //     completion, and an expression body always yields its value.
    // ES: Congelamiento 3.0 C-05 (hallazgo al portar los casos de un archivo): el pipeline 3.x
    //     aceptaba un metodo cuyo cuerpo llega al final sin valor, y el backend cerraba esa ruta
    //     con `unreachable`, asi que la llamada tenia comportamiento indefinido. El final de un
    //     generador es su terminacion normal, y un cuerpo de expresion siempre produce su valor.
    if (!ownershipFlowReachable_ || callable.kind != ClassMemberKind::Method || callable.syntax == nullptr ||
        callable.syntax->first() == nullptr ||
        callable.syntax->first()->kind() != ast::AstNodeKind::BlockStatement) return;
    // EN: An async method's body yields the future's inner type; `async void` ends normally.
    // ES: El cuerpo de un metodo async produce el tipo interior del future; `async void` termina
    //     normalmente.
    const std::string result = effectiveReturnTypeKey(callable);
    if (result.empty() || result == "builtin:void" || result == "builtin:never" ||
        language::iteratorInnerTypeKey(result).has_value() ||
        language::asyncIteratorInnerTypeKey(result).has_value()) return;
    diagnostics.report(
        diagnostics::DiagnosticId::NotAllPathsReturn,
        diagnostics::DiagnosticSeverity::Error,
        callable.syntax->range(),
        "not all reachable control paths return a value from function '" + callable.name + "'");
}

bool ProjectMethodBodyModel::awaitOperandCanSuspend(
    const ast::AstNode* operand,
    const ast::AstNode* callerBody,
    std::size_t callerOrdinal,
    std::unordered_map<std::size_t, int>& memo) const
{
    // EN: An unknown origin or an unresolved call can suspend; otherwise the producer decides.
    // ES: Un origen desconocido o una llamada sin resolver puede suspenderse; si no, decide el
    //     productor.
    const ast::AstNode* origin = awaitOperandOrigin(operand, callerBody);
    if (origin == nullptr) return true;
    const std::optional<std::size_t> producer = boundAwaitProducer(callBinding(origin, callerOrdinal));
    if (!producer.has_value()) return true;
    return awaitProducerCanSuspend(*producer, memo);
}

bool ProjectMethodBodyModel::awaitProducerCanSuspend(
    std::size_t producerOrdinal,
    std::unordered_map<std::size_t, int>& memo) const
{
    // EN: Freeze 3.0 (await without recursion). Depth-first walk with an explicit stack and the same
    //     order as the former recursion: a producer's awaits are examined in source order, the walk
    //     stops at the first that suspends, and `memo` keeps 0 (being visited, read as "does not
    //     suspend" so cycles terminate), 1 (does not suspend) or 2 (suspends). A producer that is not
    //     a source async callable analyzed here suspends without opening a frame.
    // ES: Congelamiento 3.0 (await sin recursion). Recorrido en profundidad con pila explicita y el
    //     mismo orden que la recursion anterior: los awaits de un productor se examinan en orden de
    //     fuente, el recorrido se detiene en el primero que se suspende, y `memo` guarda 0 (en
    //     visita, leido como "no se suspende" para que los ciclos terminen), 1 (no se suspende) o 2
    //     (se suspende). Un productor que no es un callable async de fuente analizado aqui se
    //     suspende sin abrir frame.
    const auto known = memo.find(producerOrdinal);
    if (known != memo.end()) return known->second == 2;
    std::vector<AwaitProducerFrame> frames;
    std::optional<std::size_t> opening = producerOrdinal;
    for (;;)
    {
        if (opening.has_value())
        {
            const std::size_t ordinal = *opening;
            opening.reset();
            memo[ordinal] = 0;
            const ClassMemberSymbol* producer = methodSymbol(ordinal);
            const bool analyzable = producer != nullptr && producer->syntax != nullptr &&
                producer->syntax->first() != nullptr && !producer->isPrecompiled &&
                (producer->effects.contains(sema::Effect::Async) ||
                    language::asyncIteratorInnerTypeKey(producer->declaredTypeKey).has_value()) &&
                analyzedCallableMembers_.contains(ordinal);
            if (!analyzable)
            {
                memo[ordinal] = 2;
                if (frames.empty()) return true;
                frames.back().suspends = true;
                continue;
            }
            AwaitProducerFrame frame;
            frame.ordinal = ordinal;
            frame.body = producer->syntax->first();
            collectAwaitSites(frame.body, frame.awaits, frame.suspends);
            frames.push_back(std::move(frame));
            continue;
        }
        AwaitProducerFrame& top = frames.back();
        if (top.suspends || top.next == top.awaits.size())
        {
            const bool finished = top.suspends;
            memo[top.ordinal] = finished ? 2 : 1;
            frames.pop_back();
            if (frames.empty()) return finished;
            frames.back().suspends = finished;
            continue;
        }
        const ast::AstNode* await = top.awaits[top.next++];
        const ast::AstNode* origin = awaitOperandOrigin(await->first(), top.body);
        const std::optional<std::size_t> producer = origin != nullptr
            ? boundAwaitProducer(callBinding(origin, top.ordinal))
            : std::nullopt;
        if (!producer.has_value())
        {
            top.suspends = true;
            continue;
        }
        const auto visited = memo.find(*producer);
        if (visited != memo.end())
        {
            top.suspends = visited->second == 2;
            continue;
        }
        opening = *producer;
    }
}

void ProjectMethodBodyModel::verifySynchronousAwaits(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::vector<std::size_t> callers(analyzedCallableMembers_.begin(), analyzedCallableMembers_.end());
    std::sort(callers.begin(), callers.end());
    std::unordered_map<std::size_t, int> memo;
    for (const std::size_t callerOrdinal : callers)
    {
        const ClassMemberSymbol* caller = methodSymbol(callerOrdinal);
        if (caller == nullptr || caller->syntax == nullptr || caller->syntax->first() == nullptr ||
            caller->isPrecompiled || caller->effects.contains(sema::Effect::Async)) continue;
        std::vector<const ast::AstNode*> awaits;
        bool ignored = false;
        collectAwaitSites(caller->syntax->first(), awaits, ignored);
        for (const ast::AstNode* await : awaits)
        {
            if (!awaitOperandCanSuspend(
                    await->first(), caller->syntax->first(), callerOrdinal, memo)) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::SynchronousAwaitCanSuspend,
                diagnostics::DiagnosticSeverity::Error,
                await->range(),
                "synchronous await of a producer that can suspend: the producer must be awaited "
                "from an async context; declare '" + caller->name + "' async (an entry point "
                "can be 'static async int Main()')");
        }
    }
}

void ProjectMethodBodyModel::verifyRequiredFieldAssignments(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NS304-T09-F01: every instance `required` field must be assigned on every normal exit of
    // every constructor. assignedRequiredFields_ contains only the finalized path intersection.
    // Object initializers remain outside this constructor-body check.
    if (memberModel_ == nullptr) return;
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Constructor || member.isStatic) continue;
        if (!analyzedCallableMembers_.contains(memberOrdinal)) continue;
        // A constructor that can only throw/terminate never produces an object whose required
        // fields could be observed. Normal returns and fallthrough are recorded explicitly.
        if (!constructorsWithNormalExit_.contains(memberOrdinal)) continue;
        std::vector<std::size_t> fallbackFieldOrdinals;
        std::span<const std::size_t> fieldOrdinals =
            memberModel_->membersDeclaredByType(member.declaringTypeOrdinal);
        if (!memberModel_->hasMembersDeclaredByTypeIndex())
        {
            for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
            {
                if (memberModel_->members()[ordinal].declaringTypeOrdinal ==
                    member.declaringTypeOrdinal)
                    fallbackFieldOrdinals.push_back(ordinal);
            }
            fieldOrdinals = fallbackFieldOrdinals;
        }
        for (const std::size_t fieldOrdinal : fieldOrdinals)
        {
            if (fieldOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& field = memberModel_->members()[fieldOrdinal];
            if (field.kind != ClassMemberKind::Field || !field.isRequired || field.isStatic) continue;
            if (assignedRequiredFields_.contains({memberOrdinal, fieldOrdinal})) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax != nullptr ? field.syntax->range() : member.syntax->range(),
                "required field '" + field.name + "' is never assigned in constructor '" +
                    member.name + "'");
        }
    }
}
}
