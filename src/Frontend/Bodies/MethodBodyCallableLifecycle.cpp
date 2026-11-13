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
    arrayReferenceFlow_ = {};
    arrayReferenceFlow_.stopped = arrayReferenceStartStopped_;
    arrayReferenceEventLog_.clear();
    if (arrayReferenceReplaySource_ != nullptr) arrayReferenceFlow_.eventLog = &arrayReferenceEventLog_;
    arrayReferencePendingReturns_.clear();
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
    if (!callable.effects.contains(sema::Effect::Async) &&
        !callable.effects.contains(sema::Effect::Generator) && !ownershipFlowReachable_ &&
        std::ranges::none_of(structuredExits_, [&](const MethodStructuredExitBinding& exit)
        {
            return exit.methodMemberOrdinal == callableOrdinal && exit.kind == MethodStructuredExitKind::Return;
        }))
        callablesWithoutNormalExit_.insert(callableOrdinal);
    // NS304-T05-F05: a pinned local cannot cross a suspension — its fixed-address identity is not
    // preserved across the poll return (the local moves to the frame and back). A borrowed local
    // IS allowed when its source is re-localizable to a frame slot; the MIR drags the borrow's
    // source into the frame and rejects a borrow whose source is a parameter/field (no slot).
    if (callable.effects.contains(sema::Effect::Async) && !awaitExpressions_.empty())
    {
        for (const MethodLocalSymbol& local : locals())
        {
            // EN: Freeze 3.0 (plan item 136): the classifier names the pinned local.
            // ES: Congelamiento 3.0 (elemento 136 del plan): el clasificador nombra el local pin.
            if (knownLifeSourcesOfLocal(local).pinned)
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

// EN: Collects the expressions of `callable` whose value is copied as a reference: initializers of
//     its locals, the value of an `=` assignment, arguments of resolved method and namespace-function
//     calls, and returned expressions. Comparisons and other operators are not copies. The body is
//     walked iteratively without entering lambdas, which are callables of their own.
// ES: Reune las expresiones de `callable` cuyo valor se copia como referencia: inicializadores de
//     sus locales, el valor de una asignacion `=`, argumentos de llamadas resueltas a metodos y a
//     funciones de namespace, y expresiones retornadas. Las comparaciones y otros operadores no son
//     copias. El cuerpo se recorre iterativamente sin entrar en lambdas, que son callables propios.
// Complexity: O(body nodes + calls). Effects: none.
// EN: True when a block nested below `body` (not `body` itself) declares a local initialized by an
//     object creation, the shape whose scope cleanup the replay models. Iterative; lambdas are
//     callables of their own and are not entered.
// ES: Verdadero cuando un bloque anidado bajo `body` (no `body` mismo) declara un local inicializado
//     con una creacion de objeto, la forma cuya limpieza de ambito modela el replay. Iterativo; no
//     entra en lambdas, que son callables propios.
// Complexity: O(body nodes). Effects: none.
bool ProjectMethodBodyModel::hasNestedScopedOwnerDeclaration(const ast::AstNode* body)
{
    struct Item { const ast::AstNode* node; bool nested; };
    std::vector<Item> pending{{body, false}};
    while (!pending.empty())
    {
        const Item item = pending.back();
        pending.pop_back();
        const ast::AstNode* node = item.node;
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::BlockStatement)
        {
            for (const ast::AstNode* child : node->children())
            {
                if (item.nested && child != nullptr &&
                    child->kind() == ast::AstNodeKind::VariableDeclarationStatement && child->first() != nullptr &&
                    child->first()->kind() == ast::AstNodeKind::ObjectCreationExpression) return true;
                pending.push_back({child, true});
            }
            continue;
        }
        pending.push_back({node->first(), true});
        pending.push_back({node->second(), true});
        pending.push_back({node->third(), true});
        for (const ast::AstNode* child : node->children()) pending.push_back({child, true});
    }
    return false;
}

// EN: True when `body` contains a `move(...)` outside lambdas: a local owner may be handed to a field,
//     a static, another local or a call, so the conservative rule must replay the callable.
// ES: Verdadero cuando `body` contiene un `move(...)` fuera de lambdas: un dueno local puede pasar a un
//     campo, un static, otro local o una llamada, asi la regla conservadora debe reproducir el callable.
// Complexity: O(body nodes). Effects: none.
bool ProjectMethodBodyModel::containsMoveExpression(const ast::AstNode* body)
{
    std::vector<const ast::AstNode*> pending{body};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::MoveExpression) return true;
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
    return false;
}

// EN: Loop pass of the conservative array-reference rule (M2-05). The replay analyzes a loop body once,
//     so a use that precedes, in the body, a life event of the same loop never sees that event although
//     the next iteration runs the use after it. For every loop of `callable`, the life events logged
//     inside it that can reach the back-edge (not followed in their sequence by `return`, `throw`, or a
//     `break` of this loop) are applied to the uses inside it that the first pass left without a life
//     event: a use is upgraded to Unproven when an object it may name, or an object stored into the same
//     array inside the loop, ends its life inside the loop. A slot written before the use on every path
//     of the iteration names that iteration's value, so objects created inside the loop are then
//     excluded (each iteration creates a new one). The pass only adds Unproven; it never clears a
//     verdict and never produces the destroyed-object error.
// ES: Pasada de ciclos de la regla conservadora de referencias en arreglos (M2-05). El replay analiza un
//     cuerpo de ciclo una sola vez, asi que un uso que precede, en el cuerpo, a un evento de vida del
//     mismo ciclo nunca ve ese evento aunque la siguiente iteracion ejecute el uso despues de el. Para
//     cada ciclo de `callable`, los eventos de vida registrados dentro que pueden llegar al regreso del
//     ciclo (no seguidos en su secuencia por `return`, `throw` o un `break` de este ciclo) se aplican a
//     los usos dentro que la primera pasada dejo sin evento de vida: un uso pasa a Unproven cuando un
//     objeto que puede nombrar, o un objeto guardado en el mismo arreglo dentro del ciclo, termina su
//     vida dentro del ciclo. Una ranura escrita antes del uso en todo camino de la iteracion nombra el
//     valor de esa iteracion, asi que entonces se excluyen los objetos creados dentro del ciclo (cada
//     iteracion crea uno nuevo). La pasada solo agrega Unproven; nunca limpia un veredicto ni produce el
//     error de objeto destruido.
// Complexity: O(loops * (loop nodes + log entries + observations * candidates)). Effects:
//     arrayReferenceReplayObservations_ of `callable`.
void ProjectMethodBodyModel::applyArrayReferenceLoopLifeEvents(std::size_t callable)
{
    if (arrayReferenceEventLog_.life.empty()) return;
    const ClassMemberSymbol* symbol = methodSymbol(callable);
    if (symbol == nullptr || symbol->syntax == nullptr) return;
    const auto isLoop = [](const ast::AstNode* node)
    {
        const auto kind = node->kind();
        return kind == ast::AstNodeKind::WhileStatement || kind == ast::AstNodeKind::DoWhileStatement ||
            kind == ast::AstNodeKind::ForStatement || kind == ast::AstNodeKind::ForeachStatement;
    };
    const auto pushChildren = [](std::vector<const ast::AstNode*>& pending, const ast::AstNode* node)
    {
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    };
    std::vector<const ast::AstNode*> loops;
    {
        std::vector<const ast::AstNode*> pending{symbol->syntax};
        std::unordered_set<const ast::AstNode*> seen;
        while (!pending.empty())
        {
            const ast::AstNode* node = pending.back();
            pending.pop_back();
            if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression || !seen.insert(node).second)
                continue;
            if (isLoop(node) && !arrayReferenceNeverEntered_.contains(node->second())) loops.push_back(node);
            pushChildren(pending, node);
        }
    }
    for (const ast::AstNode* loop : loops)
    {
        // EN: Nodes of the loop and the nodes whose sequence ends in an exit of this loop.
        // ES: Nodos del ciclo y los nodos cuya secuencia termina en una salida de este ciclo.
        struct Item final
        {
            const ast::AstNode* node;
            bool nested;
            bool exits;
        };
        std::unordered_set<const void*> inside;
        std::unordered_set<const void*> exiting;
        std::vector<Item> pending;
        for (const ast::AstNode* child : {loop->first(), loop->second(), loop->third()})
            pending.push_back({child, false, false});
        for (const ast::AstNode* child : loop->children()) pending.push_back({child, false, false});
        while (!pending.empty())
        {
            const Item item = pending.back();
            pending.pop_back();
            if (item.node == nullptr || item.node->kind() == ast::AstNodeKind::LambdaExpression ||
                !inside.insert(item.node).second) continue;
            if (item.exits) exiting.insert(item.node);
            const bool nested = item.nested || isLoop(item.node) ||
                item.node->kind() == ast::AstNodeKind::SwitchStatement;
            const bool sequence = item.node->kind() == ast::AstNodeKind::BlockStatement ||
                item.node->kind() == ast::AstNodeKind::SwitchSection;
            for (const ast::AstNode* child : {item.node->first(), item.node->second(), item.node->third()})
                pending.push_back({child, nested, item.exits});
            bool laterExit = false;
            const auto& children = item.node->children();
            for (std::size_t i = children.size(); i-- > 0;)
            {
                pending.push_back({children[i], nested, item.exits || (sequence && laterExit)});
                const ast::AstNode* child = children[i];
                if (sequence && child != nullptr &&
                    (child->kind() == ast::AstNodeKind::ReturnStatement ||
                     child->kind() == ast::AstNodeKind::ThrowStatement ||
                     (child->kind() == ast::AstNodeKind::BreakStatement && !nested)))
                    laterExit = true;
            }
        }
        std::set<std::size_t> ended;
        for (const auto& [site, id] : arrayReferenceEventLog_.life)
            if (inside.contains(site) && !exiting.contains(site)) ended.insert(id);
        if (ended.empty()) continue;
        const auto createdInside = [&](std::size_t id)
        {
            return id < locals_.size() && locals_[id].syntax != nullptr && inside.contains(locals_[id].syntax);
        };
        for (ArrayReferenceReplayObservation& observation : arrayReferenceReplayObservations_)
        {
            if (observation.callableOrdinal != callable || !observation.arrayRoot.has_value() ||
                observation.useLife != ArrayReferenceUseLife::NoLifeEvent || !inside.contains(observation.syntax))
                continue;
            bool risky = false;
            for (const std::size_t id : observation.candidateObjects)
                if (ended.contains(id) && (!observation.mustWritten || !createdInside(id))) risky = true;
            if (!observation.mustWritten)
                for (const ArrayReferenceEventLog::Store& store : arrayReferenceEventLog_.stores)
                    if (store.slot.array == *observation.arrayRoot && inside.contains(store.site) &&
                        ended.contains(store.object) &&
                        (!store.slot.index.has_value() || !observation.arrayIndex.has_value() ||
                         store.slot.index == observation.arrayIndex)) risky = true;
            if (risky) observation.useLife = ArrayReferenceUseLife::Unproven;
        }
    }
}

std::unordered_set<const ast::AstNode*> ProjectMethodBodyModel::collectArrayReferenceValueCopies(
    const ProjectMethodBodyModel& shard, std::size_t callable, const ClassMemberSymbol* caller,
    const std::unordered_set<const ast::AstNode*>& localInitializers)
{
    std::unordered_set<const ast::AstNode*> copies = localInitializers;
    for (const MethodCallBinding& call : shard.calls_)
        if (call.callerMethodMemberOrdinal == callable && call.resolution == MethodCallResolutionKind::Resolved)
            copies.insert(call.selectedArgumentSyntaxes.begin(), call.selectedArgumentSyntaxes.end());
    for (const NamespaceFunctionCallBinding& call : shard.namespaceFunctionCalls_)
        if (call.callerCallableOrdinal == callable && call.resolution == MethodCallResolutionKind::Resolved)
            copies.insert(call.selectedArgumentSyntaxes.begin(), call.selectedArgumentSyntaxes.end());
    if (caller == nullptr || caller->syntax == nullptr) return copies;
    std::vector<const ast::AstNode*> pending{caller->syntax->first()};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::ReturnStatement && node->first() != nullptr)
            copies.insert(node->first());
        if (node->kind() == ast::AstNodeKind::AssignmentExpression &&
            node->tokenKind() == lexer::TokenKind::Equal && node->second() != nullptr)
            copies.insert(node->second());
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
    copies.erase(nullptr);
    return copies;
}

// EN: Lowest destructor-chain question for the conservative rule: true when a destructor of
//     `typeOrdinal` or of one of its bases has no normal exit, so destroying an object of that static
//     type never completes (a derived destructor still runs its bases).
// ES: Pregunta minima de la cadena de destructores para la regla conservadora: verdadero cuando un
//     destructor de `typeOrdinal` o de una de sus bases no tiene salida normal, asi destruir un
//     objeto de ese tipo estatico nunca termina (un destructor derivado sigue corriendo sus bases).
// Complexity: O(depth * members). Effects: none.
bool ProjectMethodBodyModel::arrayReferenceTypeDestructionStops(std::size_t typeOrdinal) const
{
    if (memberModel_ == nullptr || inheritanceModel_ == nullptr) return false;
    std::optional<std::size_t> type = typeOrdinal;
    std::unordered_set<std::size_t> visited;
    while (type.has_value() && visited.insert(*type).second)
    {
        for (const std::size_t member : memberModel_->membersOfType(*type))
            if (member < memberModel_->members().size() &&
                memberModel_->members()[member].declaringTypeOrdinal == *type &&
                memberModel_->members()[member].kind == ClassMemberKind::Destructor &&
                arrayReferenceNonReturning_.contains(member)) return true;
        type = inheritanceModel_->baseTypeOrdinal(*type);
    }
    return false;
}

// EN: True when initializing type `typeOrdinal` never completes: a static constructor without a normal
//     exit, or a static field whose initializer is a call, by unqualified name, to the only static
//     method of the type with that name, and that method has no normal exit. A constructor and a
//     static method of the type run that initialization first (the rule the replay already assumed
//     for nontrivial static initialization).
// ES: Verdadero cuando inicializar el tipo `typeOrdinal` nunca termina: un constructor estatico sin
//     salida normal, o un campo estatico cuyo inicializador es una llamada, por nombre no calificado,
//     al unico metodo estatico del tipo con ese nombre, y ese metodo no tiene salida normal. Un
//     constructor y un metodo estatico del tipo corren primero esa inicializacion (la regla que el
//     replay ya suponia para la inicializacion estatica no trivial).
// Complexity: O(members of the type^2). Effects: none.
bool ProjectMethodBodyModel::arrayReferenceTypeInitializationStops(std::size_t typeOrdinal) const
{
    if (memberModel_ == nullptr) return false;
    for (const std::size_t ordinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.declaringTypeOrdinal != typeOrdinal || !member.isStatic || member.isPrecompiled) continue;
        if (member.kind == ClassMemberKind::Constructor && arrayReferenceNonReturning_.contains(ordinal)) return true;
        if (member.kind != ClassMemberKind::Field || member.syntax == nullptr) continue;
        const ast::AstNode* initializer = member.syntax->first();
        if (initializer == nullptr || initializer->kind() != ast::AstNodeKind::CallExpression ||
            !initializer->children().empty() || initializer->first() == nullptr ||
            initializer->first()->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        std::optional<std::size_t> method;
        bool ambiguous = false;
        for (const std::size_t candidate : memberModel_->membersOfType(typeOrdinal))
        {
            if (candidate >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& named = memberModel_->members()[candidate];
            if (named.declaringTypeOrdinal != typeOrdinal || named.kind != ClassMemberKind::Method ||
                named.name != initializer->first()->text()) continue;
            if (method.has_value() || !named.isStatic) ambiguous = true;
            method = candidate;
        }
        if (!ambiguous && method.has_value() && arrayReferenceNonReturning_.contains(*method)) return true;
    }
    return false;
}

// EN: True when `callable` is a source, non-virtual instance method whose body is exactly
//     `F = move(p);` or `this.F = move(p);`, where p is parameter `parameter` (owned) and F is an owned,
//     non-static field declared by the method's class. The object passed to p is then owned by the
//     receiver's object. The previous value of F is released by the assignment; it is another object.
// ES: Verdadero cuando `callable` es un metodo de instancia de fuente, no virtual, cuyo cuerpo es
//     exactamente `F = move(p);` o `this.F = move(p);`, con p el parametro `parameter` (owned) y F un
//     campo owned de instancia declarado por la clase del metodo. El objeto pasado a p queda entonces
//     en posesion del objeto receptor. El valor anterior de F lo libera la asignacion; es otro objeto.
// Complexity: O(members of the class). Effects: none.
bool ProjectMethodBodyModel::arrayReferenceAdoptsParameter(std::size_t callable, std::size_t parameter) const
{
    const ClassMemberSymbol* method = methodSymbol(callable);
    if (method == nullptr || memberModel_ == nullptr || method->isStatic || method->isPrecompiled ||
        method->isVirtual || method->isOverride || method->isAbstract || method->kind != ClassMemberKind::Method ||
        parameter >= method->parameters.size() ||
        method->parameters[parameter].ownership != sema::OwnershipKind::Owned || method->syntax == nullptr ||
        method->syntax->first() == nullptr || method->syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
        method->syntax->first()->children().size() != 1) return false;
    const ast::AstNode* statement = method->syntax->first()->children().front();
    const ast::AstNode* assignment = statement != nullptr &&
        statement->kind() == ast::AstNodeKind::ExpressionStatement ? statement->first() : nullptr;
    if (assignment == nullptr || assignment->kind() != ast::AstNodeKind::AssignmentExpression ||
        assignment->tokenKind() != lexer::TokenKind::Equal || assignment->second() == nullptr ||
        assignment->second()->kind() != ast::AstNodeKind::MoveExpression || assignment->second()->first() == nullptr ||
        assignment->second()->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
        assignment->second()->first()->text() != method->parameters[parameter].name) return false;
    const ast::AstNode* target = assignment->first();
    std::string_view fieldName;
    if (target != nullptr && target->kind() == ast::AstNodeKind::IdentifierExpression) fieldName = target->text();
    else if (target != nullptr && target->kind() == ast::AstNodeKind::MemberExpression && target->first() != nullptr &&
        target->first()->kind() == ast::AstNodeKind::ThisExpression) fieldName = target->text();
    if (fieldName.empty()) return false;
    for (const std::size_t ordinal : memberModel_->membersOfType(method->declaringTypeOrdinal))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& field = memberModel_->members()[ordinal];
        if (field.kind == ClassMemberKind::Field && field.name == fieldName && !field.isStatic &&
            field.declaringTypeOrdinal == method->declaringTypeOrdinal &&
            field.declaredOwnership == sema::OwnershipKind::Owned) return true;
    }
    return false;
}

// EN: True when `creation` is `new T()` without arguments, allocator or initializer and the only
//     constructor of T callable without arguments is a source constructor without a normal exit.
// ES: Verdadero cuando `creation` es `new T()` sin argumentos, asignador ni inicializador y el unico
//     constructor de T invocable sin argumentos es un constructor de fuente sin salida normal.
// Complexity: O(members of T). Effects: none.
bool ProjectMethodBodyModel::arrayReferenceCreationStops(const ast::AstNode* creation, std::size_t callable) const
{
    if (creation == nullptr || creation->kind() != ast::AstNodeKind::ObjectCreationExpression ||
        !creation->children().empty() || creation->first() != nullptr || creation->second() != nullptr ||
        creation->third() != nullptr || memberModel_ == nullptr) return false;
    const auto siteOrdinal = constructionSiteIndex_.latest(creation, callable);
    if (!siteOrdinal.has_value() || *siteOrdinal >= constructionSites_.size()) return false;
    const ProjectConstructionSite& site = constructionSites_[*siteOrdinal];
    if (!site.targetTypeOrdinal.has_value() || !site.argumentTypes.empty() || !site.allocatorTypeKey.empty())
        return false;
    if (arrayReferenceTypeInitializationStops(*site.targetTypeOrdinal)) return true;
    std::optional<std::size_t> constructor;
    for (const std::size_t member : memberModel_->membersOfType(*site.targetTypeOrdinal))
    {
        if (member >= memberModel_->members().size()) return false;
        const ClassMemberSymbol& candidate = memberModel_->members()[member];
        if (candidate.kind != ClassMemberKind::Constructor || candidate.isStatic ||
            candidate.declaringTypeOrdinal != *site.targetTypeOrdinal) continue;
        const bool acceptsNoArguments = std::ranges::all_of(candidate.parameters,
            [](const MethodParameterSymbol& parameter)
            { return parameter.defaultValueSyntax != nullptr || parameter.isParameterPack; });
        if (!acceptsNoArguments) continue;
        if (constructor.has_value()) return false;
        constructor = member;
    }
    return constructor.has_value() && !memberModel_->members()[*constructor].isPrecompiled &&
        arrayReferenceNonReturning_.contains(*constructor);
}

// EN: True when `statement`, executed, never completes: a direct resolved non-virtual call (as a
//     statement or as a local initializer) to a callable without a normal exit, a `delete` whose
//     operand type cannot be destroyed normally, or a `new T()` whose constructor never returns.
// ES: Verdadero cuando `statement`, al ejecutarse, nunca termina: una llamada directa resuelta no
//     virtual (como sentencia o inicializador de local) a un callable sin salida normal, un `delete`
//     cuyo tipo de operando no se destruye normalmente, o un `new T()` cuyo constructor no retorna.
// Complexity: O(members of the involved types). Effects: none.
bool ProjectMethodBodyModel::arrayReferenceStatementStops(const ast::AstNode* statement, std::size_t callable) const
{
    if (statement == nullptr) return false;
    if (statement->kind() == ast::AstNodeKind::WhileStatement && statement->first() != nullptr &&
        statement->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
        statement->first()->tokenKind() == lexer::TokenKind::KwTrue)
    {
        std::vector<const ast::AstNode*> pending{statement->second()};
        while (!pending.empty())
        {
            const ast::AstNode* node = pending.back();
            pending.pop_back();
            if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
            if (node->kind() == ast::AstNodeKind::BreakStatement || node->kind() == ast::AstNodeKind::ReturnStatement ||
                node->kind() == ast::AstNodeKind::ThrowStatement) return false;
            pending.push_back(node->first());
            pending.push_back(node->second());
            pending.push_back(node->third());
            for (const ast::AstNode* child : node->children()) pending.push_back(child);
        }
        return true;
    }
    const ast::AstNode* expression = nullptr;
    if (statement->kind() == ast::AstNodeKind::ExpressionStatement ||
        statement->kind() == ast::AstNodeKind::VariableDeclarationStatement) expression = statement->first();
    if (expression == nullptr) return false;
    if (expression->kind() == ast::AstNodeKind::CallExpression)
    {
        const MethodCallBinding* call = callBinding(expression, callable);
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved || call->interfaceCall ||
            !call->selectedMemberOrdinal.has_value()) return false;
        const ClassMemberSymbol* target = methodSymbol(*call->selectedMemberOrdinal);
        if (target != nullptr && target->isStatic &&
            arrayReferenceTypeInitializationStops(target->declaringTypeOrdinal)) return true;
        return target != nullptr && !target->isPrecompiled && !target->isVirtual && !target->isOverride &&
            !target->isAbstract && arrayReferenceNonReturning_.contains(*call->selectedMemberOrdinal);
    }
    if (expression->kind() == ast::AstNodeKind::DeleteExpression && !expression->flag())
    {
        const MethodExpressionTypeBinding* operand = expressionTypeBinding(expression->first(), callable);
        const auto type = operand != nullptr ? namedTypeOrdinalForTypeKey(operand->typeKey) : std::nullopt;
        return type.has_value() && arrayReferenceTypeDestructionStops(*type);
    }
    return arrayReferenceCreationStops(expression, callable);
}

// EN: Closes callablesWithoutNormalExit_ over direct calls, deletes, constructions, type
//     initializations and `while (true)` loops without `break` or `return`: a callable has no normal
//     exit when one of its top-level statements never completes and no earlier top-level statement
//     contains a `return` (excluding lambdas). Least fixed point by repeated passes; terminates
//     because the set only grows and is bounded by the analyzed callables.
// ES: Cierra callablesWithoutNormalExit_ sobre llamadas directas, deletes, construcciones,
//     inicializaciones de tipo y ciclos `while (true)` sin `break` ni `return`: un callable no tiene
//     salida normal cuando una de sus sentencias de primer nivel nunca termina y ninguna sentencia de
//     primer nivel anterior contiene un `return` (sin contar lambdas). Menor punto fijo por pasadas
//     repetidas; termina porque el conjunto solo crece y esta acotado por los callables analizados.
// Complexity: O(passes * body nodes). Effects: arrayReferenceNonReturning_.
void ProjectMethodBodyModel::computeArrayReferenceNonReturning()
{
    arrayReferenceNonReturning_ = callablesWithoutNormalExit_;
    const auto containsReturn = [](const ast::AstNode* root)
    {
        std::vector<const ast::AstNode*> pending{root};
        while (!pending.empty())
        {
            const ast::AstNode* node = pending.back();
            pending.pop_back();
            if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
            if (node->kind() == ast::AstNodeKind::ReturnStatement) return true;
            pending.push_back(node->first());
            pending.push_back(node->second());
            pending.push_back(node->third());
            for (const ast::AstNode* child : node->children()) pending.push_back(child);
        }
        return false;
    };
    std::vector<std::pair<std::size_t, const ast::AstNode*>> candidates;
    for (const std::size_t ordinal : analyzedCallableMembers_)
    {
        const ClassMemberSymbol* symbol = methodSymbol(ordinal);
        if (symbol != nullptr && symbol->syntax != nullptr && symbol->syntax->first() != nullptr &&
            symbol->syntax->first()->kind() == ast::AstNodeKind::BlockStatement &&
            !arrayReferenceNonReturning_.contains(ordinal))
            candidates.emplace_back(ordinal, symbol->syntax->first());
    }
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& [ordinal, body] : candidates)
        {
            if (arrayReferenceNonReturning_.contains(ordinal)) continue;
            for (const ast::AstNode* statement : body->children())
            {
                if (arrayReferenceStatementStops(statement, ordinal))
                {
                    arrayReferenceNonReturning_.insert(ordinal);
                    changed = true;
                    break;
                }
                // EN: A statement that may return ends the scan: a later stop is not on every path.
                // ES: Una sentencia que puede retornar termina el recorrido: un paro posterior no esta
                //     en todo camino.
                if (containsReturn(statement)) break;
            }
        }
    }
}

// EN: Nodes inside the bodies of loops whose first condition test is statically false: `while
//     (false)` and `for (int i = A; i OP B; ...)` with integer literals A and B. Their statements never
//     run, so the replay records no event or use there. Iterative; lambdas are not entered.
// ES: Nodos dentro de los cuerpos de ciclos cuya primera prueba de condicion es falsa estaticamente:
//     `while (false)` y `for (int i = A; i OP B; ...)` con literales enteros A y B. Sus sentencias
//     nunca corren, asi el replay no registra eventos ni usos ahi. Iterativo; no entra en lambdas.
// Complexity: O(body nodes). Effects: none.
std::unordered_set<const ast::AstNode*> ProjectMethodBodyModel::collectNeverEnteredLoopBodies(
    const ast::AstNode* body)
{
    const auto literal = [](const ast::AstNode* node) -> std::optional<long long>
    {
        if (node == nullptr || node->kind() != ast::AstNodeKind::IntegerLiteralExpression) return std::nullopt;
        long long value = 0;
        for (const char digit : node->text())
        {
            if (digit < '0' || digit > '9' || value > 100000000LL) return std::nullopt;
            value = value * 10 + (digit - '0');
        }
        return node->text().empty() ? std::nullopt : std::optional<long long>(value);
    };
    std::unordered_set<const ast::AstNode*> result;
    std::vector<const ast::AstNode*> pending{body};
    std::vector<const ast::AstNode*> neverEntered;
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::WhileStatement && node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
            node->first()->tokenKind() == lexer::TokenKind::KwFalse) neverEntered.push_back(node->second());
        // EN: `bool x = false; while (x)`: the declaration right before the loop fixes the first test.
        // ES: `bool x = false; while (x)`: la declaracion justo antes del ciclo fija la primera prueba.
        if (node->kind() == ast::AstNodeKind::BlockStatement)
            for (std::size_t i = 1; i < node->children().size(); ++i)
            {
                const ast::AstNode* declaration = node->children()[i - 1];
                const ast::AstNode* loop = node->children()[i];
                if (declaration != nullptr && loop != nullptr &&
                    declaration->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                    declaration->first() != nullptr &&
                    declaration->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
                    declaration->first()->tokenKind() == lexer::TokenKind::KwFalse &&
                    loop->kind() == ast::AstNodeKind::WhileStatement && loop->first() != nullptr &&
                    loop->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                    loop->first()->text() == declaration->text()) neverEntered.push_back(loop->second());
            }
        if (node->kind() == ast::AstNodeKind::ForStatement && !node->children().empty() && node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement && node->second() != nullptr &&
            node->second()->kind() == ast::AstNodeKind::BinaryExpression && node->second()->first() != nullptr &&
            node->second()->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            node->second()->first()->text() == node->first()->text())
        {
            const auto start = literal(node->first()->first());
            const auto bound = literal(node->second()->second());
            const lexer::TokenKind op = node->second()->tokenKind();
            const bool entered = !start || !bound ||
                (op == lexer::TokenKind::Less && *start < *bound) ||
                (op == lexer::TokenKind::LessEqual && *start <= *bound) ||
                (op == lexer::TokenKind::Greater && *start > *bound) ||
                (op == lexer::TokenKind::GreaterEqual && *start >= *bound) ||
                (op != lexer::TokenKind::Less && op != lexer::TokenKind::LessEqual &&
                 op != lexer::TokenKind::Greater && op != lexer::TokenKind::GreaterEqual);
            if (!entered) neverEntered.push_back(node->children().front());
        }
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
    while (!neverEntered.empty())
    {
        const ast::AstNode* node = neverEntered.back();
        neverEntered.pop_back();
        if (node == nullptr || !result.insert(node).second) continue;
        neverEntered.push_back(node->first());
        neverEntered.push_back(node->second());
        neverEntered.push_back(node->third());
        for (const ast::AstNode* child : node->children()) neverEntered.push_back(child);
    }
    return result;
}

void ProjectMethodBodyModel::replayArrayReferenceFlow(bool candidatesOnly)
{
    arrayReferenceReplayObservations_.clear();
    arrayReferenceForObservations_.clear();
    if (nameLookup_ == nullptr || memberModel_ == nullptr || namespaceFunctionModel_ == nullptr ||
        inheritanceModel_ == nullptr || interfaceModel_ == nullptr) return;
    std::vector<std::size_t> ordinals;
    if (candidatesOnly)
    {
        if (arrayReferenceWriteSummaries_.empty() && ownershipTransferBindings_.empty()) return;
        std::unordered_set<std::size_t> withWrites;
        for (const MethodCallBinding& call : calls_)
            if (call.resolution == MethodCallResolutionKind::Resolved &&
                call.selectedMemberOrdinal.has_value() &&
                !arrayReferenceWriteSummary(*call.selectedMemberOrdinal).empty())
                withWrites.insert(call.callerMethodMemberOrdinal);
        for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
            if (call.resolution == MethodCallResolutionKind::Resolved &&
                call.selectedFunctionOrdinal.has_value() &&
                !arrayReferenceWriteSummary(namespaceCallableBase_ + *call.selectedFunctionOrdinal).empty())
                withWrites.insert(call.callerCallableOrdinal);
        std::unordered_set<std::size_t> candidates;
        std::unordered_map<std::size_t, bool> loopCandidateCache;
        const auto hasLoopCandidate = [&](std::size_t ordinal)
        {
            if (const auto found = loopCandidateCache.find(ordinal); found != loopCandidateCache.end()) return found->second;
            const auto* callable = methodSymbol(ordinal);
            if (callable == nullptr || callable->syntax == nullptr || callable->syntax->first() == nullptr)
            { loopCandidateCache.emplace(ordinal, false); return false; }
            std::vector<const ast::AstNode*> pending{callable->syntax->first()};
            std::size_t visited = 0;
            while (!pending.empty() && visited++ < 4096)
            {
                const auto* node = pending.back(); pending.pop_back();
                if (node == nullptr) continue;
                if (node->kind() == ast::AstNodeKind::ForStatement ||
                    (node->kind() == ast::AstNodeKind::WhileStatement && node->first() != nullptr &&
                     ((node->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
                       node->first()->tokenKind() == lexer::TokenKind::KwTrue) ||
                      (node->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                       nameBinding(node->first(), ordinal) != nullptr &&
                       (nameBinding(node->first(), ordinal)->kind == MethodNameBindingKind::Parameter ||
                        nameBinding(node->first(), ordinal)->kind == MethodNameBindingKind::Local) &&
                       nameBinding(node->first(), ordinal)->typeKey == "builtin:bool"))))
                { loopCandidateCache.emplace(ordinal, true); return true; }
                pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                for (const auto* child : node->children()) pending.push_back(child);
            }
            loopCandidateCache.emplace(ordinal, false); return false;
        };
        for (const MethodOwnershipTransferBinding& transfer : ownershipTransferBindings_)
            if (transfer.kind == MethodOwnershipTransferKind::MoveLocalToOwnedParameter)
                if (withWrites.contains(transfer.methodMemberOrdinal) &&
                    analyzedCallableMembers_.contains(transfer.methodMemberOrdinal) &&
                    (arrayReferenceLinearPublicationCoverage(transfer.methodMemberOrdinal) ||
                     hasLoopCandidate(transfer.methodMemberOrdinal)))
                    candidates.insert(transfer.methodMemberOrdinal);
        // EN: Conservative rule: every caller that declares a local native array and moves a local
        //     owner into an owned parameter is replayed, even when the store is indirect or the body
        //     is outside the linear subset; its later element uses are then assessed by life events.
        // ES: Regla conservadora: se reproduce todo llamador que declara un arreglo nativo local y
        //     mueve un dueno local a un parametro owned, aunque el guardado sea indirecto o el cuerpo
        //     quede fuera del subconjunto lineal; sus usos posteriores se evaluan por eventos de vida.
        std::unordered_set<std::size_t> withArrayLocals;
        for (const MethodLocalSymbol& local : locals_)
            if (!local.isStatic && ProjectTypeConversion::isNativeArray(local.typeKey))
                withArrayLocals.insert(local.methodMemberOrdinal);
        for (const MethodOwnershipTransferBinding& transfer : ownershipTransferBindings_)
            if (transfer.kind == MethodOwnershipTransferKind::MoveLocalToOwnedParameter &&
                withArrayLocals.contains(transfer.methodMemberOrdinal) &&
                analyzedCallableMembers_.contains(transfer.methodMemberOrdinal))
                candidates.insert(transfer.methodMemberOrdinal);
        // EN: A covered caller that stores into an array and destroys a `raii` local at the end of
        //     a nested block is replayed too (scope cleanup, see replayArrayReferenceScopeExit).
        // ES: Tambien se reproduce un llamador cubierto que guarda en un arreglo y destruye un local
        //     `raii` al final de un bloque anidado (limpieza de ambito, ver
        //     replayArrayReferenceScopeExit).
        for (const std::size_t ordinal : withWrites)
        {
            const ClassMemberSymbol* callable = methodSymbol(ordinal);
            if (callable != nullptr && callable->syntax != nullptr &&
                analyzedCallableMembers_.contains(ordinal) && arrayReferenceLinearPublicationCoverage(ordinal) &&
                hasNestedScopedOwnerDeclaration(callable->syntax->first()))
                candidates.insert(ordinal);
        }
        for (const std::size_t ordinal : withArrayLocals)
        {
            const ClassMemberSymbol* callable = methodSymbol(ordinal);
            if (callable != nullptr && callable->syntax != nullptr && analyzedCallableMembers_.contains(ordinal) &&
                (hasNestedScopedOwnerDeclaration(callable->syntax->first()) ||
                 containsMoveExpression(callable->syntax->first())))
                candidates.insert(ordinal);
        }
        ordinals.assign(candidates.begin(), candidates.end());
    }
    else ordinals.assign(analyzedCallableMembers_.begin(), analyzedCallableMembers_.end());
    std::ranges::sort(ordinals);
    if (!ordinals.empty()) computeArrayReferenceNonReturning();
    for (const std::size_t ordinal : ordinals)
    {
        const ProjectCallableAnalysisContext context(
            ordinal, 0, *nameLookup_, *memberModel_, *namespaceFunctionModel_,
            *inheritanceModel_, *interfaceModel_, genericModel_, genericConstraintModel_);
        std::unique_ptr<ProjectMethodBodyModel> shard = createCallableShard(context);
        shard->arrayReferenceReplaySource_ = this;
        if (const ClassMemberSymbol* replayed = methodSymbol(ordinal);
            replayed != nullptr && replayed->syntax != nullptr)
        {
            shard->arrayReferenceNeverEntered_ = collectNeverEnteredLoopBodies(replayed->syntax->first());
            // EN: A static method or constructor first initializes its type; if that never completes,
            //     nothing in the body runs.
            // ES: Un metodo estatico o constructor primero inicializa su tipo; si eso nunca termina,
            //     nada del cuerpo corre.
            shard->arrayReferenceStartStopped_ =
                (replayed->isStatic || replayed->kind == ClassMemberKind::Constructor) &&
                arrayReferenceTypeInitializationStops(replayed->declaringTypeOrdinal);
        }
        diagnostics::DiagnosticEngine ignored(std::numeric_limits<std::size_t>::max());
        shard->analyzeCallable(context, ignored);
        shard->applyArrayReferenceLoopLifeEvents(ordinal);
        std::unordered_set<const ast::AstNode*> instanceReceivers;
        for (const MethodCallBinding& call : shard->calls_)
            if (call.receiverSyntax != nullptr && !call.staticCall &&
                call.resolution == MethodCallResolutionKind::Resolved &&
                call.selectedMemberOrdinal.has_value())
                instanceReceivers.insert(call.receiverSyntax);
        std::unordered_set<const ast::AstNode*> fieldInitializers;
        std::unordered_set<const ast::AstNode*> fieldWriteTargets;
        std::unordered_set<const ast::AstNode*> localInitializers;
        for (const MethodLocalSymbol& local : shard->locals_)
            if (local.methodMemberOrdinal == ordinal && local.syntax != nullptr &&
                local.syntax->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                local.syntax->first() != nullptr)
                localInitializers.insert(local.syntax->first());
        const ClassMemberSymbol* caller = methodSymbol(ordinal);
        if (caller != nullptr && caller->syntax != nullptr && caller->syntax->first() != nullptr &&
            caller->syntax->first()->kind() == ast::AstNodeKind::BlockStatement)
            for (const ast::AstNode* statement : caller->syntax->first()->children())
                if (statement != nullptr && statement->kind() == ast::AstNodeKind::ExpressionStatement &&
                    statement->first() != nullptr && statement->first()->kind() == ast::AstNodeKind::AssignmentExpression &&
                    statement->first()->tokenKind() == lexer::TokenKind::Equal && statement->first()->first() != nullptr)
                    fieldWriteTargets.insert(statement->first()->first());
        std::unordered_set<const ast::AstNode*> valueCopies =
            collectArrayReferenceValueCopies(*shard, ordinal, caller, localInitializers);
        std::unordered_set<const ast::AstNode*> fieldWrites;
        for (const MethodNameBinding& binding : shard->nameBindings_)
            if (binding.kind == MethodNameBindingKind::ExplicitObjectField && binding.syntax != nullptr && binding.receiverSyntax != nullptr)
            {
                if (localInitializers.contains(binding.syntax)) fieldInitializers.insert(binding.receiverSyntax);
                if (fieldWriteTargets.contains(binding.syntax)) fieldWrites.insert(binding.receiverSyntax);
            }
        for (ArrayReferenceReplayObservation& observation : shard->arrayReferenceReplayObservations_)
        {
            observation.instanceMethodReceiver = instanceReceivers.contains(observation.syntax);
            observation.directFieldInitializer = fieldInitializers.contains(observation.syntax);
            observation.directFieldWrite = fieldWrites.contains(observation.syntax);
            observation.valueCopy = valueCopies.contains(observation.syntax);
        }
        arrayReferenceReplayObservations_.insert(arrayReferenceReplayObservations_.end(),
            shard->arrayReferenceReplayObservations_.begin(),
            shard->arrayReferenceReplayObservations_.end());
        arrayReferenceForObservations_.insert(arrayReferenceForObservations_.end(),
            shard->arrayReferenceForObservations_.begin(), shard->arrayReferenceForObservations_.end());
    }
}

// EN: True when the read of `observation` is never reached because an event on its dead-path evidence
//     (a constructor, a scope cleanup, a call or an owned destruction) has no normal exit, or a static
//     initializer that may run first is not trivial. Extracted unchanged from
//     diagnoseDeadArrayReferenceReceivers so that the conservative rule applies the same reachability.
// ES: Verdadero cuando la lectura de `observation` nunca se alcanza porque un evento de su evidencia de
//     camino muerto (un constructor, una limpieza de ambito, una llamada o una destruccion owned) no
//     tiene salida normal, o un inicializador estatico que puede correr antes no es trivial. Extraida
//     sin cambios de diagnoseDeadArrayReferenceReceivers para que la regla conservadora aplique la misma
//     alcanzabilidad.
// Complexity: O(evidence events * members). Effects: fills the `nontrivialInitializers` cache.
bool ProjectMethodBodyModel::arrayReferenceReadStopsBefore(const ArrayReferenceReplayObservation& observation,
    std::unordered_map<std::size_t, bool>& nontrivialInitializers) const
{
    const auto mayHaveNontrivialInitialization = [&](std::size_t type)
    {
        if (const auto found = nontrivialInitializers.find(type); found != nontrivialInitializers.end())
            return found->second;
        bool unknown = nameLookup_ == nullptr || memberModel_ == nullptr ||
            type >= nameLookup_->namedTypes().size() || nameLookup_->namedTypes()[type].isPrecompiled;
        if (!unknown)
        {
            for (const auto& part : nameLookup_->namedTypes()[type].parts) if (part.isPrecompiled) unknown = true;
            for (const auto ordinal : memberModel_->membersOfType(type))
            {
                if (ordinal >= memberModel_->members().size()) { unknown = true; break; }
                const auto& member = memberModel_->members()[ordinal];
                if (member.declaringTypeOrdinal != type || !member.isStatic) continue;
                if (member.kind == ClassMemberKind::Constructor ||
                    (member.kind == ClassMemberKind::Field && !member.isConst &&
                     (member.isPrecompiled || member.syntax == nullptr || member.syntax->first() != nullptr)))
                    unknown = true;
            }
        }
        nontrivialInitializers.emplace(type, unknown);
        return unknown;
    };
    if (!observation.deadPathEvidence.has_value() || memberModel_ == nullptr || inheritanceModel_ == nullptr)
        return false;
    const auto* caller = methodSymbol(observation.callableOrdinal);
    if (caller == nullptr || mayHaveNontrivialInitialization(caller->declaringTypeOrdinal)) return true;
    for (const ArrayReferenceTraceEvent& event : observation.deadPathEvidence->events)
    {
        if (event.kind == ArrayReferenceTraceKind::Construction && event.syntax != nullptr &&
            event.syntax->kind() == ast::AstNodeKind::ObjectCreationExpression &&
            event.syntax->children().empty() && event.syntax->first() == nullptr &&
            event.syntax->second() == nullptr && event.syntax->third() == nullptr)
        {
            const auto siteOrdinal = constructionSiteIndex_.latest(event.syntax, observation.callableOrdinal);
            if (!siteOrdinal.has_value() || *siteOrdinal >= constructionSites_.size()) continue;
            const ProjectConstructionSite& site = constructionSites_[*siteOrdinal];
            if (!site.targetTypeOrdinal.has_value() || !site.argumentTypes.empty() ||
                !site.allocatorTypeKey.empty()) continue;
            if (mayHaveNontrivialInitialization(*site.targetTypeOrdinal)) return true;
            std::optional<std::size_t> constructor;
            bool ambiguous = false;
            for (const std::size_t member : memberModel_->membersOfType(*site.targetTypeOrdinal))
            {
                if (member >= memberModel_->members().size()) { ambiguous = true; break; }
                const ClassMemberSymbol& candidate = memberModel_->members()[member];
                if (candidate.kind != ClassMemberKind::Constructor || candidate.isStatic ||
                    candidate.declaringTypeOrdinal != *site.targetTypeOrdinal) continue;
                if (!candidate.parameters.empty())
                {
                    const bool acceptsNoArguments = std::ranges::all_of(
                        candidate.parameters, [](const MethodParameterSymbol& parameter)
                        { return parameter.defaultValueSyntax != nullptr || parameter.isParameterPack; });
                    if (acceptsNoArguments) { ambiguous = true; break; }
                    continue;
                }
                if (constructor.has_value()) { ambiguous = true; break; }
                constructor = member;
            }
            if (!ambiguous && constructor.has_value() &&
                memberModel_->members()[*constructor].parameters.empty() &&
                !memberModel_->members()[*constructor].isPrecompiled &&
                callablesWithoutNormalExit_.contains(*constructor)) return true;
            continue;
        }
        // EN: Scope cleanup runs the destructor chain of the local's created type; a destructor
        //     without a normal exit means the later read is never reached.
        // ES: La limpieza de ambito ejecuta la cadena de destructores del tipo creado del local;
        //     un destructor sin salida normal significa que la lectura posterior no se alcanza.
        if (event.kind == ArrayReferenceTraceKind::Destruction && event.syntax != nullptr &&
            event.syntax->kind() == ast::AstNodeKind::BlockStatement)
        {
            const ast::AstNode* creation = event.source != nullptr ? event.source->first() : nullptr;
            const auto site = creation != nullptr
                ? constructionSiteIndex_.latest(creation, observation.callableOrdinal) : std::nullopt;
            if (!site.has_value() || *site >= constructionSites_.size() ||
                !constructionSites_[*site].targetTypeOrdinal.has_value()) return true;
            std::optional<std::size_t> type = constructionSites_[*site].targetTypeOrdinal;
            std::unordered_set<std::size_t> visitedTypes;
            while (type && visitedTypes.insert(*type).second)
            {
                for (const std::size_t member : memberModel_->membersOfType(*type))
                    if (member < memberModel_->members().size() &&
                        memberModel_->members()[member].declaringTypeOrdinal == *type &&
                        memberModel_->members()[member].kind == ClassMemberKind::Destructor &&
                        callablesWithoutNormalExit_.contains(member)) return true;
                type = inheritanceModel_->baseTypeOrdinal(*type);
            }
            continue;
        }
        if (event.kind != ArrayReferenceTraceKind::Call || event.syntax == nullptr) continue;
        const MethodCallBinding* call = callBinding(event.syntax, observation.callableOrdinal);
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved || call->interfaceCall ||
            !call->selectedMemberOrdinal.has_value()) continue;
        const std::size_t ordinal = *call->selectedMemberOrdinal;
        const ClassMemberSymbol* target = methodSymbol(ordinal);
        if (target == nullptr || target->isPrecompiled || target->isVirtual || target->isOverride ||
            target->isAbstract) continue;
        if (call->staticCall && mayHaveNontrivialInitialization(target->declaringTypeOrdinal)) return true;
        if (callablesWithoutNormalExit_.contains(ordinal)) return true;
        for (const std::size_t parameter : call->selectedArgumentParameterOrdinals)
        {
            if (parameter >= target->parameters.size() ||
                target->parameters[parameter].ownership != sema::OwnershipKind::Owned ||
                arrayReferenceOwnedEffect(ordinal, parameter) != ArrayReferenceOwnedEffect::Destroyed) continue;
            auto type = inheritanceModel_->typeOrdinalForTypeKey(target->parameters[parameter].typeKey);
            for (std::size_t i = 0; i < call->selectedArgumentParameterOrdinals.size() &&
                 i < call->selectedArgumentSyntaxes.size(); ++i)
            {
                if (call->selectedArgumentParameterOrdinals[i] != parameter) continue;
                const auto* move = call->selectedArgumentSyntaxes[i];
                const auto* operand = move != nullptr && move->kind() == ast::AstNodeKind::MoveExpression
                    ? move->first() : nullptr;
                const auto* owner = operand != nullptr ? nameBinding(operand, observation.callableOrdinal) : nullptr;
                if (owner == nullptr || owner->kind != MethodNameBindingKind::Local || !owner->localOrdinal ||
                    *owner->localOrdinal >= locals_.size()) continue;
                const auto* declaration = locals_[*owner->localOrdinal].syntax;
                const auto* creation = declaration != nullptr ? declaration->first() : nullptr;
                if (creation == nullptr || creation->kind() != ast::AstNodeKind::ObjectCreationExpression) continue;
                const auto site = constructionSiteIndex_.latest(creation, observation.callableOrdinal);
                if (site && *site < constructionSites_.size() && constructionSites_[*site].targetTypeOrdinal)
                    type = constructionSites_[*site].targetTypeOrdinal;
            }
            std::unordered_set<std::size_t> visitedTypes;
            while (type && visitedTypes.insert(*type).second)
            {
                for (const std::size_t member : memberModel_->membersOfType(*type))
                    if (member < memberModel_->members().size() &&
                        memberModel_->members()[member].declaringTypeOrdinal == *type &&
                        memberModel_->members()[member].kind == ClassMemberKind::Destructor &&
                        callablesWithoutNormalExit_.contains(member)) return true;
                type = inheritanceModel_->baseTypeOrdinal(*type);
            }
        }
    }
    return false;
}

void ProjectMethodBodyModel::diagnoseDeadArrayReferenceReceivers(
    diagnostics::DiagnosticEngine& diagnostics, bool fieldsOnly) const
{
    std::unordered_map<std::size_t, bool> nontrivialInitializers;
    const auto stopsBeforeRead = [&](const ArrayReferenceReplayObservation& observation)
    {
        return arrayReferenceReadStopsBefore(observation, nontrivialInitializers);
    };
    std::set<std::pair<std::size_t, const ast::AstNode*>> reported;
    for (const ArrayReferenceReplayObservation& observation : arrayReferenceReplayObservations_)
    {
        if (observation.syntax == nullptr || observation.state != ArrayReferenceRead::Dead ||
            observation.requiresLateValidation ||
            (fieldsOnly ? (!observation.directFieldInitializer && !observation.directFieldWrite)
                        : (!observation.instanceMethodReceiver && !observation.valueCopy)) ||
            !arrayReferenceLinearPublicationCoverage(observation.callableOrdinal) ||
            stopsBeforeRead(observation) ||
            !reported.insert({observation.callableOrdinal, observation.syntax}).second) continue;
        arrayReferenceReportedUses_.insert({observation.callableOrdinal, observation.syntax});
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            observation.syntax->range(),
            fieldsOnly
                ? (observation.directFieldWrite
                    ? "reference to a destroyed object cannot be used to write an instance field"
                    : "reference to a destroyed object cannot be used to read an instance field")
                : (observation.instanceMethodReceiver
                    ? "reference to a destroyed object cannot be used as an instance method receiver"
                    : "reference to a destroyed object cannot be copied, passed or returned"));
    }
}

void ProjectMethodBodyModel::diagnoseDeadArrayReferenceFields(diagnostics::DiagnosticEngine& diagnostics) const
{
    diagnoseDeadArrayReferenceReceivers(diagnostics, true);
}

void ProjectMethodBodyModel::noteArrayReferenceReportedUse(
    std::size_t callableOrdinal, const ast::AstNode* syntax) const
{
    arrayReferenceReportedUses_.insert({callableOrdinal, syntax});
}

// EN: Element reads of `callable` that are not uses of the referenced object: an operand of `==` or
//     `!=` (comparing the reference does not touch the object) and the target of an assignment
//     (the slot is replaced, not read). Iterative; lambdas are callables of their own.
// ES: Lecturas de elementos de `callable` que no usan el objeto referenciado: un operando de `==` o
//     `!=` (comparar la referencia no toca el objeto) y el destino de una asignacion (la ranura se
//     reemplaza, no se lee). Iterativo; las lambdas son callables propios.
// Complexity: O(body nodes). Effects: none.
std::unordered_set<const ast::AstNode*> ProjectMethodBodyModel::arrayReferenceNonUseReads(
    std::size_t callable) const
{
    std::unordered_set<const ast::AstNode*> result;
    const ClassMemberSymbol* symbol = methodSymbol(callable);
    if (symbol == nullptr || symbol->syntax == nullptr) return result;
    std::vector<const ast::AstNode*> pending{symbol->syntax->first()};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node->kind() == ast::AstNodeKind::LambdaExpression) continue;
        if (node->kind() == ast::AstNodeKind::BinaryExpression &&
            (node->tokenKind() == lexer::TokenKind::EqualEqual || node->tokenKind() == lexer::TokenKind::BangEqual))
        {
            result.insert(node->first());
            result.insert(node->second());
        }
        if (node->kind() == ast::AstNodeKind::AssignmentExpression &&
            node->tokenKind() == lexer::TokenKind::Equal) result.insert(node->first());
        // EN: `element.Field = Stop();` evaluates the value before storing: the object is never used.
        // ES: `elemento.Campo = Detener();` evalua el valor antes de guardar: el objeto nunca se usa.
        if (node->kind() == ast::AstNodeKind::AssignmentExpression && node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::MemberExpression && node->second() != nullptr)
        {
            const ast::AstNode* value = node->second();
            const MethodCallBinding* call = value->kind() == ast::AstNodeKind::CallExpression
                ? callBinding(value, callable) : nullptr;
            const ClassMemberSymbol* target = call != nullptr && call->selectedMemberOrdinal.has_value() &&
                call->resolution == MethodCallResolutionKind::Resolved && !call->interfaceCall
                ? methodSymbol(*call->selectedMemberOrdinal) : nullptr;
            if (target != nullptr && !target->isVirtual && !target->isOverride && !target->isAbstract &&
                arrayReferenceNonReturning_.contains(*call->selectedMemberOrdinal))
                result.insert(node->first()->first());
        }
        pending.push_back(node->first());
        pending.push_back(node->second());
        pending.push_back(node->third());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
    }
    result.erase(nullptr);
    return result;
}

// EN: Conservative array-reference lifetime rule (docs/3.0/main-fixes/04). Runs after every precise
//     proof. A use of an array element (any read except a `==`/`!=` operand or an assignment target)
//     whose possible objects had a life event is rejected: definitely destroyed -> "reference to a
//     destroyed object"; destroyed on some path, or owner transferred without a proven outcome ->
//     "cannot prove ... still alive". A use whose possible objects had no life event, a stored
//     reference that is never used, and a read that is never reached are accepted.
// ES: Regla conservadora de vida de referencias en arreglos (docs/3.0/main-fixes/04). Corre despues
//     de toda prueba precisa. Un uso de un elemento (toda lectura salvo un operando de `==`/`!=` o un
//     destino de asignacion) cuyos objetos posibles tuvieron un evento de vida se rechaza: destruido
//     con certeza -> "reference to a destroyed object"; destruido en algun camino, o dueno
//     transferido sin resultado demostrado -> "cannot prove ... still alive". Se aceptan un uso cuyos
//     objetos posibles no tuvieron evento de vida, una referencia guardada que no se usa y una
//     lectura que nunca se alcanza.
// Complexity: O(observations * log observations + body nodes of the callables involved).
// Effects: diagnostics; records reported uses.
void ProjectMethodBodyModel::diagnoseUnprovenArrayReferences(diagnostics::DiagnosticEngine& diagnostics) const
{
    std::unordered_map<std::size_t, std::unordered_set<const ast::AstNode*>> nonUses;
    for (const ArrayReferenceReplayObservation& observation : arrayReferenceReplayObservations_)
    {
        if (observation.syntax == nullptr || observation.useLife == ArrayReferenceUseLife::NoLifeEvent ||
            arrayReferenceReportedUses_.contains({observation.callableOrdinal, observation.syntax})) continue;
        auto found = nonUses.find(observation.callableOrdinal);
        if (found == nonUses.end())
            found = nonUses.emplace(observation.callableOrdinal,
                arrayReferenceNonUseReads(observation.callableOrdinal)).first;
        if (found->second.contains(observation.syntax)) continue;
        arrayReferenceReportedUses_.insert({observation.callableOrdinal, observation.syntax});
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            observation.syntax->range(),
            observation.useLife == ArrayReferenceUseLife::Dead
                ? "reference to a destroyed object cannot be used"
                : "cannot prove that the referenced object is still alive when this array element is used");
    }
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

// EN: Freeze 3.0 D-04. Appends to `origins` every value written to a local named `name` in `body`:
//     the initializer of each declaration and the right side of each assignment. Nothing is
//     appended for a parameter or a field, whose origin is unknown here.
// ES: Congelamiento 3.0 D-04. Agrega a `origins` todo valor escrito en un local llamado `name` en
//     `body`: el inicializador de cada declaracion y el lado derecho de cada asignacion. Para un
//     parametro o un campo, cuyo origen aqui es desconocido, no agrega nada.
void appendLocalOrigins(
    const ast::AstNode* body,
    std::string_view name,
    std::vector<const ast::AstNode*>& origins)
{
    std::unordered_set<const ast::AstNode*> visited;
    std::vector<const ast::AstNode*> pending = {body};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || !visited.insert(node).second) continue;
        if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement && node->text() == name &&
            node->first() != nullptr) origins.push_back(node->first());
        if (node->kind() == ast::AstNodeKind::AssignmentExpression && node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            node->first()->text() == name && node->second() != nullptr) origins.push_back(node->second());
        for (const ast::AstNode* child : node->children()) pending.push_back(child);
        pending.push_back(node->first());
        pending.push_back(node->second());
    }
}

// EN: Freeze 3.0 D-04. The expressions whose call bindings may name the producer of an awaited
//     future: a local is replaced by every value written to it, and `it.MoveNextAsync()` over a
//     local iterator by the origins of `it` (the generator that created it). Iterative; each
//     expression is expanded once, so a local whose origin leads back to itself terminates.
// ES: Congelamiento 3.0 D-04. Las expresiones cuyos bindings de llamada pueden nombrar al
//     productor de un future esperado: un local se sustituye por cada valor escrito en el, y
//     `it.MoveNextAsync()` sobre un iterador local por los origenes de `it` (el generador que lo
//     creo). Iterativo; cada expresion se expande una vez, asi que un local cuyo origen vuelve a si
//     mismo termina.
void collectAwaitOperandOrigins(
    const ast::AstNode* operand,
    const ast::AstNode* callerBody,
    std::vector<const ast::AstNode*>& origins)
{
    std::unordered_set<const ast::AstNode*> visited;
    std::vector<const ast::AstNode*> pending = {operand};
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || !visited.insert(node).second) continue;
        if (node->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            appendLocalOrigins(callerBody, node->text(), pending);
            continue;
        }
        const bool localIteratorStep = node->kind() == ast::AstNodeKind::CallExpression &&
            node->first() != nullptr && node->first()->kind() == ast::AstNodeKind::MemberExpression &&
            node->first()->text() == "MoveNextAsync" && node->first()->first() != nullptr &&
            node->first()->first()->kind() == ast::AstNodeKind::IdentifierExpression;
        if (localIteratorStep)
        {
            pending.push_back(node->first()->first());
            continue;
        }
        origins.push_back(node);
    }
}

// EN: The producer named by a resolved call binding; nullopt means an unknown producer.
// ES: El productor que nombra un binding de llamada resuelto; nullopt es productor desconocido.
std::optional<std::size_t> boundAwaitProducer(const MethodCallBinding* binding)
{
    if (binding == nullptr || binding->resolution != MethodCallResolutionKind::Resolved) return std::nullopt;
    return binding->selectedMemberOrdinal;
}
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
    // EN: Freeze 3.0 D-04: the await can suspend only when some known origin names a producer that
    //     suspends; an unknown origin contributes nothing.
    // ES: Congelamiento 3.0 D-04: el await puede suspenderse solo si algun origen conocido nombra a
    //     un productor que se suspende; un origen desconocido no aporta nada.
    std::vector<const ast::AstNode*> origins;
    collectAwaitOperandOrigins(operand, callerBody, origins);
    for (const ast::AstNode* origin : origins)
    {
        const std::optional<std::size_t> producer = boundAwaitProducer(callBinding(origin, callerOrdinal));
        if (producer.has_value() && awaitProducerCanSuspend(*producer, memo)) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::awaitProducerCanSuspend(
    std::size_t producerOrdinal,
    std::unordered_map<std::size_t, int>& memo) const
{
    // EN: Freeze 3.0 D-04. Reachability over the await graph with an explicit worklist: the producer
    //     suspends when a source async producer with `yield;` is reachable through the producers its
    //     awaits name. The answer does not depend on the order in which callers are checked (the
    //     former cycle rule read a producer under visit as "does not suspend" and kept that answer).
    //     A producer that is not a source async callable analyzed here is an unknown origin and is
    //     not expanded. When nothing suspends, every visited producer is memoized as 1.
    // ES: Congelamiento 3.0 D-04. Alcanzabilidad sobre el grafo de awaits con lista de trabajo
    //     explicita: el productor se suspende si alcanza, a traves de los productores que nombran sus
    //     awaits, un productor async de fuente con `yield;`. La respuesta no depende del orden en que
    //     se revisan los llamadores (la regla anterior de ciclos leia un productor en visita como «no
    //     se suspende» y conservaba esa respuesta). Un productor que no es un callable async de fuente
    //     analizado aqui es origen desconocido y no se expande. Si nada se suspende, todo productor
    //     visitado queda en el memo como 1.
    const auto known = memo.find(producerOrdinal);
    if (known != memo.end()) return known->second == 2;
    std::unordered_set<std::size_t> visited;
    std::vector<std::size_t> pending = {producerOrdinal};
    bool suspends = false;
    while (!pending.empty() && !suspends)
    {
        const std::size_t ordinal = pending.back();
        pending.pop_back();
        if (!visited.insert(ordinal).second) continue;
        const auto answered = memo.find(ordinal);
        if (answered != memo.end())
        {
            suspends = answered->second == 2;
            continue;
        }
        const ClassMemberSymbol* producer = methodSymbol(ordinal);
        const bool analyzable = producer != nullptr && producer->syntax != nullptr &&
            producer->syntax->first() != nullptr && !producer->isPrecompiled &&
            (producer->effects.contains(sema::Effect::Async) ||
                language::asyncIteratorInnerTypeKey(producer->declaredTypeKey).has_value()) &&
            analyzedCallableMembers_.contains(ordinal);
        if (!analyzable) continue;
        std::vector<const ast::AstNode*> awaits;
        collectAwaitSites(producer->syntax->first(), awaits, suspends);
        for (const ast::AstNode* await : awaits)
        {
            std::vector<const ast::AstNode*> origins;
            collectAwaitOperandOrigins(await->first(), producer->syntax->first(), origins);
            for (const ast::AstNode* origin : origins)
            {
                const std::optional<std::size_t> awaited = boundAwaitProducer(callBinding(origin, ordinal));
                if (awaited.has_value()) pending.push_back(*awaited);
            }
        }
    }
    if (suspends)
    {
        memo[producerOrdinal] = 2;
        return true;
    }
    for (const std::size_t ordinal : visited) memo[ordinal] = 1;
    return false;
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
