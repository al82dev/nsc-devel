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
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

void ProjectMethodBodyModel::recordExpressionType(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult& result)
{
    if (expression == nullptr || !result.valid || result.methodGroup || isErrorType(result.typeKey))
    {
        return;
    }
    // NSC-LIMIT-010: se copian TAMBIEN ownership, transferencia y origen. Antes se quedaban en
    // ExpressionResult, y ProjectConstructionModel --que solo ve estos bindings-- no podia
    // distinguir `move(duenio)` de `duenio`. Los campos se nombran en vez de ir por posicion: la
    // lista crecio a nueve, y un inicializador agregado posicional de nueve campos es un error de
    // una linea esperando a que alguien inserte un miembro en medio.
    MethodExpressionTypeBinding binding = makeExpressionTypeBinding(
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        result);
    const std::size_t bindingOrdinal = expressionTypes_.size();
    expressionTypes_.push_back(std::move(binding));
    expressionTypeBindingIndex_.record(methodMemberOrdinal, expression, bindingOrdinal);
}

MethodExpressionTypeBinding ProjectMethodBodyModel::makeExpressionTypeBinding(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult& result) const
{
    MethodExpressionTypeBinding binding;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.typeKey = result.typeKey;
    if (result.addressable)
    {
        binding.valueCategory = result.ownership == sema::OwnershipKind::Borrowed
            ? sema::ValueCategory::BorrowedValue
            : sema::ValueCategory::Place;
    }
    else if (!result.ownershipTransfer && !result.ownershipFreshOwner &&
             typeKeyIsStruct(result.typeKey) &&
             (expression->kind() == ast::AstNodeKind::CallExpression ||
              expression->kind() == ast::AstNodeKind::ObjectCreationExpression ||
              expression->kind() == ast::AstNodeKind::AggregateInitializerExpression ||
              expression->kind() == ast::AstNodeKind::TupleExpression ||
              expression->kind() == ast::AstNodeKind::WithExpression))
    {
        binding.valueCategory = sema::ValueCategory::Temporary;
    }
    else
    {
        binding.valueCategory = sema::ValueCategory::Value;
    }
    binding.integerConstant = result.integerConstant;
    binding.pointerPointeeReadonly = result.pointerPointeeReadonly;
    binding.ownership = result.ownership;
    binding.ownershipTransfer = result.ownershipTransfer;
    binding.ownershipFreshOwner = result.ownershipFreshOwner;
    binding.ownershipSourceLocalOrdinal = result.ownershipSourceLocalOrdinal;
    binding.ownershipSourceParameterOrdinal = result.ownershipSourceParameterOrdinal;
    binding.ownsStorageDestination = result.ownsStorageDestination;
    binding.objectLifetimeOwnership = result.objectLifetimeOwnership;
    binding.storageOwnership = result.storageOwnership;
    binding.borrowedElementSlots = result.borrowedElementSlots;
    binding.flowProvenNonNull = result.flowProvenNonNull;
    binding.flowIntegerExcludesZero = result.flowIntegerExcludesZero;
    binding.flowIntegerMinimum = result.flowIntegerMinimum;
    binding.flowIntegerMaximum = result.flowIntegerMaximum;
    binding.flowActiveVariantCase = result.flowActiveVariantCase;
    binding.flowRuntimeCheckElided = result.flowRuntimeCheckElided;
    return binding;
}


void ProjectMethodBodyModel::recordCallBinding(MethodCallBinding binding)
{
    if (arrayReferenceReplaySource_ != nullptr) recordArrayReferenceCall(binding);
    const std::size_t bindingOrdinal = calls_.size();
    const ast::AstNode* syntax = binding.syntax;
    const std::size_t callableOrdinal = binding.callerMethodMemberOrdinal;
    calls_.push_back(std::move(binding));
    callBindingIndex_.record(callableOrdinal, syntax, bindingOrdinal);
}

void ProjectMethodBodyModel::recordArrayReferenceCall(const MethodCallBinding& binding)
{
    // EN: A call inside a loop body that is never entered never runs.
    // ES: Una llamada dentro de un cuerpo de ciclo que nunca se ejecuta nunca corre.
    if (arrayReferenceNeverEntered_.contains(binding.syntax)) return;
    // EN: A call that receives an adopting owner (as receiver or argument) may replace or release
    //     what it adopted; a proven adoption below re-establishes it for this call's own transfer.
    // ES: Una llamada que recibe un dueno adoptante (como receptor o argumento) puede reemplazar o
    //     liberar lo que adopto; una adopcion demostrada abajo lo vuelve a establecer para la
    //     transferencia de esta misma llamada.
    if (!arrayReferenceFlow_.adopted.empty())
    {
        std::vector<const ast::AstNode*> reached(
            binding.selectedArgumentSyntaxes.begin(), binding.selectedArgumentSyntaxes.end());
        if (!binding.staticCall && binding.receiverSyntax != nullptr) reached.push_back(binding.receiverSyntax);
        for (const ast::AstNode* expression : reached)
        {
            const ast::AstNode* argument =
                expression != nullptr && expression->kind() == ast::AstNodeKind::MoveExpression
                ? expression->first() : expression;
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
            const std::optional<ScopeName> name = lookupScopeName(binding.scopeOrdinal, argument->text());
            if (name.has_value() && name->kind == ScopeNameKind::Local)
                arrayReferenceFlow_.releaseAdopted(name->semanticOrdinal);
        }
    }
    arrayReferenceFlow_.deadPathWitnesses.clear();
    arrayReferenceFlow_.deadPathEvidence.clear();
    arrayReferenceFlow_.noteTrace(ArrayReferenceTraceKind::Call, binding.syntax);
    const auto scalarCallCannotReachPrivateArrays = [&]
    {
        if (!arrayReferenceReplaySource_->arrayReferenceLinearPublicationCoverage(
                binding.callerMethodMemberOrdinal) || arrayReferenceFlow_.unknown ||
            !arrayReferencePendingReturns_.empty()) return false;
        for (const ast::AstNode* argument : binding.selectedArgumentSyntaxes)
        {
            if (argument == nullptr) return false;
            if (argument->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                argument->kind() == ast::AstNodeKind::BooleanLiteralExpression ||
                argument->kind() == ast::AstNodeKind::StringLiteralExpression) continue;
            if (argument->kind() == ast::AstNodeKind::CallExpression)
            {
                const MethodCallBinding* nested = arrayReferenceReplaySource_->callBinding(
                    argument, binding.callerMethodMemberOrdinal);
                if (nested == nullptr || !nested->selectedMemberOrdinal.has_value() ||
                    !arrayReferenceReplaySource_->arrayReferenceHasOnlyModeledPublication(
                        *nested->selectedMemberOrdinal)) return false;
            }
            else if (argument->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const auto name = lookupScopeName(binding.scopeOrdinal, argument->text());
                if (!name.has_value() || (name->kind != ScopeNameKind::Local &&
                                         name->kind != ScopeNameKind::Parameter)) return false;
            }
            else return false;
            const std::string_view type = recordedExpressionTypeKey(
                argument, binding.callerMethodMemberOrdinal);
            if (!ProjectTypeConversion::isIntegral(type) && type != "builtin:bool") return false;
        }
        if (binding.staticCall || binding.receiverSyntax == nullptr) return true;
        if (binding.receiverSyntax->kind() != ast::AstNodeKind::IdentifierExpression) return false;
        const auto receiver = lookupScopeName(binding.scopeOrdinal, binding.receiverSyntax->text());
        if (!receiver.has_value()) return false;
        if (receiver->kind == ScopeNameKind::Parameter) return true;
        if (receiver->kind != ScopeNameKind::Local ||
            arrayReferenceFlow_.arrayForLocal(receiver->semanticOrdinal).has_value()) return false;
        for (const auto& [slot, ids] : arrayReferenceFlow_.slots)
            if (slot.array == receiver->semanticOrdinal) return false;
        return true;
    };
    const auto invalidateTrackedFacts = [&]
    {
        // An unknown call cannot invalidate identities that do not yet exist.
        if (!arrayReferenceFlow_.objects.empty() || !arrayReferenceFlow_.owners.empty() ||
            !arrayReferenceFlow_.arrays.empty() || !arrayReferenceFlow_.slots.empty())
        {
            if (scalarCallCannotReachPrivateArrays())
            {
                arrayReferenceFlow_.unknownArrays.insert(
                    arrayReferenceFlow_.escapedArrays.begin(), arrayReferenceFlow_.escapedArrays.end());
                if (arrayReferenceFlow_.publicationCoverageUnknown)
                {
                    for (const auto& [id, life] : arrayReferenceFlow_.objects)
                        if (life != ArrayReferenceLife::Dead) arrayReferenceFlow_.unknownObjects.insert(id);
                }
                else
                {
                    for (const std::size_t id : arrayReferenceFlow_.exposedObjects)
                        if (const auto object = arrayReferenceFlow_.objects.find(id);
                            object != arrayReferenceFlow_.objects.end() &&
                            object->second != ArrayReferenceLife::Dead)
                            arrayReferenceFlow_.unknownObjects.insert(id);
                    for (const auto& [slot, ids] : arrayReferenceFlow_.slots)
                        if (arrayReferenceFlow_.escapedArrays.contains(slot.array))
                            for (const std::size_t id : ids)
                                if (const auto object = arrayReferenceFlow_.objects.find(id);
                                    object != arrayReferenceFlow_.objects.end() &&
                                    object->second != ArrayReferenceLife::Dead)
                                    arrayReferenceFlow_.unknownObjects.insert(id);
                }
                if (!binding.staticCall && binding.receiverSyntax != nullptr &&
                    binding.receiverSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const auto receiver = lookupScopeName(binding.scopeOrdinal, binding.receiverSyntax->text());
                    if (receiver.has_value() && receiver->kind == ScopeNameKind::Local)
                    {
                        const auto owner = arrayReferenceFlow_.owners.find(receiver->semanticOrdinal);
                        if (owner != arrayReferenceFlow_.owners.end())
                            arrayReferenceFlow_.unknownObjects.insert(owner->second.begin(), owner->second.end());
                    }
                }
                return;
            }
            arrayReferenceFlow_.publicationUnknown = true;
            arrayReferenceFlow_.markUnknown();
        }
    };
    // EN: A call without a usable summary may store any reference it receives into any array it
    //     receives, and a `move` of a tracked owner into it ends that owner without a proven outcome.
    //     Record both before the facts are invalidated, so the conservative rule still sees them.
    // ES: Una llamada sin resumen utilizable puede guardar cualquier referencia que recibe en
    //     cualquier arreglo que recibe, y un `move` de un dueno rastreado hacia ella termina ese
    //     dueno sin resultado demostrado. Ambos se registran antes de invalidar los hechos, para que
    //     la regla conservadora los siga viendo.
    const auto noteUnsummarizedArguments = [&]
    {
        std::vector<const ast::AstNode*> arguments(
            binding.selectedArgumentSyntaxes.begin(), binding.selectedArgumentSyntaxes.end());
        if (binding.syntax != nullptr && arguments.empty())
            for (const ast::AstNode* child : binding.syntax->children()) arguments.push_back(child);
        if (!binding.staticCall && binding.receiverSyntax != nullptr) arguments.push_back(binding.receiverSyntax);
        for (const ast::AstNode* expression : arguments)
        {
            if (expression == nullptr) continue;
            const bool moved = expression->kind() == ast::AstNodeKind::MoveExpression;
            const ast::AstNode* argument = moved ? expression->first() : expression;
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
            const std::optional<ScopeName> name = lookupScopeName(binding.scopeOrdinal, argument->text());
            if (!name.has_value() || name->kind != ScopeNameKind::Local) continue;
            const auto owner = arrayReferenceFlow_.owners.find(name->semanticOrdinal);
            if (owner != arrayReferenceFlow_.owners.end())
            {
                arrayReferenceFlow_.exposedObjects.insert(owner->second.begin(), owner->second.end());
                if (moved) arrayReferenceFlow_.releaseUnproven(name->semanticOrdinal);
            }
            if (const auto root = arrayReferenceFlow_.arrayForLocal(name->semanticOrdinal))
            {
                arrayReferenceFlow_.escapedArrays.insert(*root);
                arrayReferenceFlow_.unknownArrays.insert(*root);
            }
        }
    };
    if (binding.resolution != MethodCallResolutionKind::Resolved ||
        !binding.selectedMemberOrdinal.has_value() || binding.interfaceCall)
    {
        noteUnsummarizedArguments();
        invalidateTrackedFacts();
        return;
    }
    const std::size_t callee = *binding.selectedMemberOrdinal;
    const ClassMemberSymbol* target = methodSymbol(callee);
    if (target == nullptr || target->isVirtual || target->isOverride || target->isAbstract ||
        arrayReferenceReplaySource_->arrayReferenceWritesUnknown(callee))
    {
        noteUnsummarizedArguments();
        invalidateTrackedFacts();
        markArrayReferenceCallWithoutNormalExit(binding);
        return;
    }
    const auto argumentFor = [&](std::size_t parameter) -> const ast::AstNode*
    {
        const ast::AstNode* result = nullptr;
        for (std::size_t i = 0; i < binding.selectedArgumentParameterOrdinals.size() &&
             i < binding.selectedArgumentSyntaxes.size(); ++i)
        {
            if (binding.selectedArgumentParameterOrdinals[i] != parameter) continue;
            if (result != nullptr) return nullptr;
            result = binding.selectedArgumentSyntaxes[i];
        }
        return result;
    };
    const auto localFor = [&](const ast::AstNode* argument) -> std::optional<std::size_t>
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression)
            return std::nullopt;
        const std::optional<ScopeName> name = lookupScopeName(binding.scopeOrdinal, argument->text());
        return name.has_value() && name->kind == ScopeNameKind::Local
            ? std::optional<std::size_t>(name->semanticOrdinal) : std::nullopt;
    };
    const auto recordPossiblePublication = [&](const ast::AstNode* expression)
    {
        if (expression == nullptr) return;
        const ast::AstNode* argument = expression->kind() == ast::AstNodeKind::MoveExpression
            ? expression->first() : expression;
        if (argument == nullptr) { arrayReferenceFlow_.publicationCoverageUnknown = true; return; }
        if (argument->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
            argument->kind() == ast::AstNodeKind::BooleanLiteralExpression ||
            argument->kind() == ast::AstNodeKind::StringLiteralExpression) return;
        const auto local = localFor(argument);
        if (!local.has_value())
        {
            if (argument->kind() != ast::AstNodeKind::IdentifierExpression)
                arrayReferenceFlow_.publicationCoverageUnknown = true;
            return;
        }
        const auto owner = arrayReferenceFlow_.owners.find(*local);
        if (owner != arrayReferenceFlow_.owners.end())
        {
            arrayReferenceFlow_.publicationUnknown = true;
            arrayReferenceFlow_.exposedObjects.insert(owner->second.begin(), owner->second.end());
            for (const std::size_t id : owner->second)
            {
                const auto object = arrayReferenceFlow_.objects.find(id);
                if (object == arrayReferenceFlow_.objects.end() || object->second != ArrayReferenceLife::Dead)
                    arrayReferenceFlow_.unknownObjects.insert(id);
            }
        }
        if (const auto root = arrayReferenceFlow_.arrayForLocal(*local))
        {
            arrayReferenceFlow_.publicationUnknown = true;
            arrayReferenceFlow_.escapedArrays.insert(*root);
            arrayReferenceFlow_.unknownArrays.insert(*root);
        }
    };
    if (!arrayReferenceReplaySource_->arrayReferenceHasOnlyModeledPublication(callee))
    {
        recordPossiblePublication(binding.receiverSyntax);
        for (std::size_t i = 0; i < binding.selectedArgumentSyntaxes.size(); ++i)
        {
            const ast::AstNode* argument = binding.selectedArgumentSyntaxes[i];
            const bool provenForward = argument != nullptr &&
                argument->kind() == ast::AstNodeKind::MoveExpression &&
                i < binding.selectedArgumentParameterOrdinals.size() &&
                arrayReferenceReplaySource_->arrayReferenceOwnedEffect(
                    callee, binding.selectedArgumentParameterOrdinals[i]) ==
                    ArrayReferenceOwnedEffect::ReturnedSame;
            if (!provenForward) recordPossiblePublication(argument);
        }
    }
    for (const MethodDirectArrayReferenceWrite& effect :
         arrayReferenceReplaySource_->arrayReferenceWriteSummary(callee))
    {
        const auto source = localFor(argumentFor(effect.sourceParameterOrdinal));
        const auto array = localFor(argumentFor(effect.arrayParameterOrdinal));
        const auto root = array ? arrayReferenceFlow_.arrayForLocal(*array) : std::nullopt;
        if (!source.has_value() || !root.has_value() || *root >= locals_.size())
        {
            arrayReferenceFlow_.markUnknown();
            continue;
        }
        const MethodLocalSymbol& arrayLocal = locals_[*root];
        const ast::AstNode* initial = arrayLocal.syntax != nullptr ? arrayLocal.syntax->first() : nullptr;
        if (initial == nullptr || initial->kind() != ast::AstNodeKind::NewExpression || !initial->flag())
        {
            arrayReferenceFlow_.publicationUnknown = true;
            arrayReferenceFlow_.escapedArrays.insert(*root);
        }
        const auto owner = arrayReferenceFlow_.owners.find(*source);
        if (owner == arrayReferenceFlow_.owners.end())
        {
            arrayReferenceFlow_.markUnknown();
            continue;
        }
        const auto direct = arrayReferenceReplaySource_->directArrayReferenceWrites(callee);
        const bool provenWrite = arrayReferenceReplaySource_->arrayReferenceHasOnlyModeledPublication(callee) &&
            std::find(direct.begin(), direct.end(), effect) != direct.end();
        std::optional<std::int64_t> index = effect.index;
        bool specialized = false;
        bool potential = false;
        if (!index && effect.indexParameterOrdinal && provenWrite)
        {
            const std::size_t parameter = *effect.indexParameterOrdinal;
            for (std::size_t i = 0; i < binding.selectedArgumentParameterOrdinals.size() &&
                 i < binding.selectedArgumentSyntaxes.size() && i < binding.selectedConversions.size(); ++i)
            {
                const auto* syntax = binding.selectedArgumentSyntaxes[i];
                const ast::AstNode* literal = syntax;
                bool potentialParameter = false;
                if (syntax != nullptr && syntax->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const auto local = localFor(syntax);
                    if (!local || *local >= locals_.size() || locals_[*local].isStatic ||
                        locals_[*local].isReference || locals_[*local].syntax == nullptr)
                    {
                        literal = nullptr;
                        const auto name = lookupScopeName(binding.scopeOrdinal, syntax->text());
                        const auto* caller = methodSymbol(binding.callerMethodMemberOrdinal);
                        if (name && name->kind == ScopeNameKind::Parameter && caller != nullptr &&
                            name->semanticOrdinal < caller->parameters.size())
                        {
                            const auto& source = caller->parameters[name->semanticOrdinal];
                            potentialParameter = source.passKind == ParameterPassKind::Value &&
                                source.ownership == sema::OwnershipKind::None &&
                                ProjectTypeConversion::isIntegral(source.typeKey);
                        }
                    }
                    else literal = locals_[*local].syntax->first();
                }
                if (binding.selectedArgumentParameterOrdinals[i] == parameter && syntax != nullptr &&
                    binding.selectedConversions[i].kind == ProjectConversionKind::Identity &&
                    parameter < target->parameters.size() &&
                    target->parameters[parameter].passKind == ParameterPassKind::Value)
                {
                    if (potentialParameter) { index = 0; specialized = true; potential = true; }
                    else if (literal != nullptr && literal->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
                             literal->integerValue() <= 0x7fffffffffffffffULL)
                    { index = static_cast<std::int64_t>(literal->integerValue()); specialized = true; }
                }
            }
        }
        if (provenWrite) arrayReferenceFlow_.write({*root, index}, owner->second, specialized, potential);
        else arrayReferenceFlow_.writeMaybe({*root, index}, owner->second);
        arrayReferenceFlow_.noteTrace(ArrayReferenceTraceKind::Write, binding.syntax, std::nullopt,
            argumentFor(effect.sourceParameterOrdinal), argumentFor(effect.arrayParameterOrdinal), index);
    }
    // Only simple proven owned outcomes are applied. More complex control flow
    // remains unknown, rather than treating every move as a destruction.
    for (std::size_t i = 0; i < binding.selectedArgumentParameterOrdinals.size(); ++i)
    {
        const std::size_t parameter = binding.selectedArgumentParameterOrdinals[i];
        if (parameter >= target->parameters.size() ||
            target->parameters[parameter].ownership != sema::OwnershipKind::Owned) continue;
        const ast::AstNode* argument = argumentFor(parameter);
        const auto owner = argument != nullptr && argument->kind() == ast::AstNodeKind::MoveExpression
            ? localFor(argument->first()) : std::optional<std::size_t>{};
        // EN: Follow the move to its new owner: an instance method whose whole body is
        //     `field = move(parameter);` on an owned instance field of its own class hands the object to
        //     the receiver's object, which a tracked local owns.
        // ES: Seguir el move hasta su nuevo dueno: un metodo de instancia cuyo cuerpo completo es
        //     `campo = move(parametro);` sobre un campo owned de instancia de su propia clase entrega el
        //     objeto al objeto receptor, que posee un local rastreado.
        if (owner.has_value() && !binding.staticCall && binding.receiverSyntax != nullptr)
        {
            const auto receiver = localFor(binding.receiverSyntax);
            if (receiver.has_value() && *receiver != *owner && arrayReferenceFlow_.owners.contains(*receiver) &&
                arrayReferenceReplaySource_->arrayReferenceAdoptsParameter(callee, parameter))
            {
                arrayReferenceFlow_.adopt(*owner, *receiver);
                continue;
            }
        }
        const ArrayReferenceOwnedEffect effect =
            arrayReferenceReplaySource_->arrayReferenceOwnedEffect(callee, parameter);
        if (!owner.has_value() || effect == ArrayReferenceOwnedEffect::Unknown)
        {
            // EN: A move into an owned parameter of unknown effect does not prove a destruction,
            //     but nothing proves the object stays alive either.
            // ES: Un move a un parametro owned de efecto desconocido no demuestra una destruccion,
            //     pero tampoco demuestra que el objeto siga vivo.
            if (owner.has_value()) arrayReferenceFlow_.releaseUnproven(*owner);
            arrayReferenceFlow_.markUnknown();
            continue;
        }
        if (effect == ArrayReferenceOwnedEffect::Destroyed)
        {
            arrayReferenceFlow_.cleanup(*owner);
            arrayReferenceFlow_.noteTrace(ArrayReferenceTraceKind::Destruction, binding.syntax, std::nullopt,
                argument);
            // EN: The callee runs the destructor of the object's created type; if that chain never
            //     returns, neither does the call.
            // ES: El callee corre el destructor del tipo creado del objeto; si esa cadena no retorna,
            //     la llamada tampoco.
            const ast::AstNode* declaration = *owner < locals_.size() ? locals_[*owner].syntax : nullptr;
            const ast::AstNode* creation = declaration != nullptr ? declaration->first() : nullptr;
            const auto site = creation != nullptr ? arrayReferenceReplaySource_->constructionSiteIndex_.latest(
                creation, binding.callerMethodMemberOrdinal) : std::nullopt;
            if (site.has_value() && *site < arrayReferenceReplaySource_->constructionSites_.size() &&
                arrayReferenceReplaySource_->constructionSites_[*site].targetTypeOrdinal.has_value() &&
                arrayReferenceReplaySource_->arrayReferenceTypeDestructionStops(
                    *arrayReferenceReplaySource_->constructionSites_[*site].targetTypeOrdinal))
                arrayReferenceFlow_.stopped = true;
        }
        else if (binding.syntax != nullptr && !arrayReferencePendingReturns_.contains(binding.syntax))
            arrayReferencePendingReturns_.emplace(binding.syntax, *owner);
        else
        {
            arrayReferenceFlow_.releaseUnproven(*owner);
            arrayReferenceFlow_.markUnknown();
        }
    }
    markArrayReferenceCallWithoutNormalExit(binding);
}

// EN: In a replay shard, the path after a call to a source callable that never returns normally is
//     not reached: the replay state is marked stopped (ArrayReferenceFlowState::stopped), so its uses
//     and life events are ignored. Only direct calls to a resolved, non-virtual, non-interface target
//     in computeArrayReferenceNonReturning() are considered; the ordinary flow is not changed.
// ES: En un shard de replay, el camino posterior a una llamada a un callable de fuente que nunca
//     retorna normalmente no se alcanza: el estado del replay se marca detenido
//     (ArrayReferenceFlowState::stopped), asi se ignoran sus usos y eventos de vida. Solo se
//     consideran llamadas directas a un destino resuelto, no virtual y no de interfaz que este en
//     computeArrayReferenceNonReturning(); el flujo ordinario no cambia.
// Complexity: O(1). Effects: arrayReferenceFlow_.stopped of the replay shard.
void ProjectMethodBodyModel::markArrayReferenceCallWithoutNormalExit(const MethodCallBinding& binding)
{
    if (arrayReferenceReplaySource_ == nullptr || binding.resolution != MethodCallResolutionKind::Resolved ||
        !binding.selectedMemberOrdinal.has_value() || binding.interfaceCall) return;
    const ClassMemberSymbol* target = methodSymbol(*binding.selectedMemberOrdinal);
    if (target != nullptr && target->isStatic &&
        arrayReferenceReplaySource_->arrayReferenceTypeInitializationStops(target->declaringTypeOrdinal))
    {
        arrayReferenceFlow_.stopped = true;
        return;
    }
    if (target == nullptr || target->isPrecompiled || target->isVirtual || target->isOverride ||
        target->isAbstract) return;
    if (arrayReferenceReplaySource_->arrayReferenceNonReturning_.contains(*binding.selectedMemberOrdinal))
        arrayReferenceFlow_.stopped = true;
}

void ProjectMethodBodyModel::recordNameBinding(MethodNameBinding binding)
{
    const std::size_t bindingOrdinal = nameBindings_.size();
    const ast::AstNode* syntax = binding.syntax;
    const std::size_t callableOrdinal = binding.methodMemberOrdinal;
    nameBindings_.push_back(std::move(binding));
    nameBindingIndex_.record(callableOrdinal, syntax, bindingOrdinal);
}

void ProjectMethodBodyModel::reportPrevious(
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string message) const
{
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        range,
        std::move(message));
}

}
