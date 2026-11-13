#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;

namespace
{
int escapeClassRank(language::EscapeClass item) noexcept
{
    switch (item)
    {
        case language::EscapeClass::NoEscape: return 0;
        case language::EscapeClass::Return: return 1;
        case language::EscapeClass::CallerOwned: return 2;
        case language::EscapeClass::Global: return 3;
        case language::EscapeClass::Unknown: return 4;
    }
    return 4;
}
}

MethodEscapeOrigin ProjectMethodBodyModel::referenceEscapeOriginOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    return ProjectReferenceEscapeEvaluator(*this, scopeOrdinal).evaluate(expression);
}

namespace
{
int originUnsafeRank(MethodEscapeOriginKind kind) noexcept
{
    switch (kind)
    {
        case MethodEscapeOriginKind::VariableLengthArray: return 4;
        case MethodEscapeOriginKind::CurrentFrameLocal: return 3;
        case MethodEscapeOriginKind::CurrentFrameParameter: return 2;
        case MethodEscapeOriginKind::CurrentReceiver: return 1;
        case MethodEscapeOriginKind::ExternalStorage: return 0;
    }
    return 0;
}
}

// ES: El evaluador de referencias responde a qué almacenamiento NOMBRA una expresión. Para un
// identificador de un local puntero por valor, eso es el slot del propio local, no a dónde apunta:
// pasar `block` (que guarda una dirección de un arreglo global) a un parámetro retenido no entrega
// el marco actual. Aquí, ese caso usa el origen registrado del valor del puntero.
// EN: The reference evaluator answers which storage an expression NAMES. For an identifier of a
// by-value pointer local that is the local's own slot, not where it points: passing `block` (which
// holds an address into a global array) to a retained parameter does not hand over the current
// frame. Here that case uses the recorded origin of the pointer's value.
MethodEscapeOrigin ProjectMethodBodyModel::pointerValueEscapeOriginOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    if (expression != nullptr && expression->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, expression->text());
        if (name.has_value() && name->kind == ScopeNameKind::Local &&
            name->semanticOrdinal < locals_.size())
        {
            const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
            if (!local.isReference && !local.isVariableLengthArray &&
                !local.variableLengthArrayRootLocalOrdinal.has_value() &&
                local.typeKey.ends_with("*"))
                return local.pointerValueOrigin;
        }
    }
    return referenceEscapeOriginOfExpression(expression, scopeOrdinal);
}

void ProjectMethodBodyModel::joinPointerValueOrigin(
    MethodEscapeOrigin& target,
    const MethodEscapeOrigin& source) noexcept
{
    if (originUnsafeRank(source.kind) > originUnsafeRank(target.kind)) target = source;
}

bool ProjectMethodBodyModel::parameterMayCarryEscape(
    const MethodParameterSymbol& parameter) const noexcept
{
    return parameter.passKind != ParameterPassKind::Value ||
        parameter.ownership == sema::OwnershipKind::Borrowed ||
        parameter.typeKey.ends_with("*") || !parameter.lifetimeName.empty() ||
        typeKeyIsViewLike(parameter.typeKey);
}

std::vector<language::EscapeClass>*
ProjectMethodBodyModel::mutableParameterEscapeSummary(
    std::size_t callableOrdinal) noexcept
{
    if (isolatedCallableOrdinal_.has_value() &&
        *isolatedCallableOrdinal_ == callableOrdinal)
    {
        return &isolatedParameterEscapeSummary_;
    }
    return callableOrdinal < parameterEscapeSummaries_.size()
        ? &parameterEscapeSummaries_[callableOrdinal]
        : nullptr;
}

void ProjectMethodBodyModel::noteParameterEscape(
    std::size_t callableOrdinal,
    std::size_t parameterOrdinal,
    language::EscapeClass escapeClass)
{
    std::vector<language::EscapeClass>* summary =
        mutableParameterEscapeSummary(callableOrdinal);
    if (summary == nullptr || parameterOrdinal >= summary->size()) return;
    const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
    if (callable == nullptr || parameterOrdinal >= callable->parameters.size() ||
        !parameterMayCarryEscape(callable->parameters[parameterOrdinal]))
    {
        return;
    }
    if (escapeClassRank(escapeClass) > escapeClassRank((*summary)[parameterOrdinal]))
        (*summary)[parameterOrdinal] = escapeClass;
}

void ProjectMethodBodyModel::finalizeEscapeSummaries(
    diagnostics::DiagnosticEngine& diagnostics)
{
    noteCapturedParameterEscapes();
    std::vector<std::size_t> callableCellOffsets;
    std::vector<std::size_t> cellCallableOrdinals;
    std::vector<std::size_t> cellParameterOrdinals;
    const std::size_t cellCount = initializeEscapeCells(
        callableCellOffsets, cellCallableOrdinals, cellParameterOrdinals);
    std::vector<std::vector<std::size_t>> dependents(cellCount);
    buildEscapeDependencies(callableCellOffsets, dependents);
    propagateEscapeDependencies(cellCallableOrdinals, cellParameterOrdinals, dependents);
    const bool hasDirectArrayReferenceWrite = std::ranges::any_of(
        directArrayReferenceWrites_, [](const auto& writes) { return !writes.empty(); });
    if (hasDirectArrayReferenceWrite)
    {
        finalizeArrayReferenceWriteSummaries();
        finalizeArrayReferenceOwnedEffects();
        finalizeArrayReferencePublicationSummaries();
    }
    else
    {
        // No tracked store can seed the restricted diagnostic. The missing
        // catalog remains explicitly unknown, not a proof of no effects.
        arrayReferenceWriteSummaries_.clear();
        arrayReferenceWritesUnknown_.clear();
        arrayReferenceOwnedEffects_.clear();
        arrayReferenceOnlyModeledPublication_.clear();
        arrayReferencePrivateDefaultConstruction_.clear();
        arrayReferenceLinearPublicationCoverage_.clear();
    }
    validateEscapeCalls(diagnostics);
}

void ProjectMethodBodyModel::finalizeArrayReferenceWriteSummaries()
{
    // A direct summary contains may-writes. Propagate those effects from callee
    // parameters to caller parameters using the actual binding, not source order.
    struct Edge final
    {
        std::size_t caller;
        std::size_t callee;
        std::vector<std::optional<std::size_t>> parameterOrigins;
        bool unknownDispatch;
        std::size_t processedEffects = 0;
    };
    struct EffectHash final
    {
        std::size_t operator()(const MethodDirectArrayReferenceWrite& effect) const noexcept
        {
            std::size_t hash = std::hash<std::size_t>{}(effect.sourceParameterOrdinal);
            hash ^= std::hash<std::size_t>{}(effect.arrayParameterOrdinal) +
                static_cast<std::size_t>(0x9e3779b9U) + (hash << 6U) + (hash >> 2U);
            const std::size_t index = effect.index.has_value()
                ? std::hash<std::int64_t>{}(*effect.index) : static_cast<std::size_t>(0x85ebca6bU);
            hash ^= index + static_cast<std::size_t>(0x9e3779b9U) + (hash << 6U) + (hash >> 2U);
            const std::size_t symbolic = effect.indexParameterOrdinal.has_value()
                ? std::hash<std::size_t>{}(*effect.indexParameterOrdinal) : static_cast<std::size_t>(0xc2b2ae35U);
            return hash ^ (symbolic + static_cast<std::size_t>(0x9e3779b9U) + (hash << 6U) + (hash >> 2U));
        }
    };
    const std::size_t count = directArrayReferenceWrites_.size();
    arrayReferenceWriteSummaries_ = directArrayReferenceWrites_;
    std::vector<std::unordered_set<MethodDirectArrayReferenceWrite, EffectHash>> seen(count);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
        seen[ordinal].insert(arrayReferenceWriteSummaries_[ordinal].begin(),
            arrayReferenceWriteSummaries_[ordinal].end());
    arrayReferenceWritesUnknown_.assign(count, false);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
        arrayReferenceWritesUnknown_[ordinal] = !analyzedCallableMembers_.contains(ordinal) ||
            directArrayReferenceWritesUnknown_[ordinal];
    for (const MethodDelegateBinding& binding : delegateBindings_)
        if (binding.kind == MethodDelegateOperationKind::Invoke &&
            binding.methodMemberOrdinal < count)
            arrayReferenceWritesUnknown_[binding.methodMemberOrdinal] = true;
    for (const ProjectConstructionSite& site : constructionSites_)
        if (site.callableOrdinal < count)
            arrayReferenceWritesUnknown_[site.callableOrdinal] = true;
    std::vector<Edge> edges;
    std::vector<std::vector<std::size_t>> dependents(count);
    const auto addEdge = [&](std::size_t caller, std::size_t callee, std::size_t scope,
                             std::span<const std::size_t> parameters,
                             std::span<const ast::AstNode* const> arguments,
                             bool unknownDispatch)
    {
        if (caller >= count) return;
        if (callee >= count)
        {
            arrayReferenceWritesUnknown_[caller] = true;
            return;
        }
        const ClassMemberSymbol* target = methodSymbol(callee);
        unknownDispatch = unknownDispatch || target == nullptr ||
            (target != nullptr && (target->isVirtual || target->isOverride || target->isAbstract));
        std::vector<std::optional<std::size_t>> origins(target != nullptr ? target->parameters.size() : 0);
        std::vector<bool> used(origins.size(), false);
        for (std::size_t i = 0; i < parameters.size() && i < arguments.size(); ++i)
        {
            const std::size_t parameter = parameters[i];
            if (parameter >= origins.size()) continue;
            if (used[parameter]) { origins[parameter].reset(); continue; }
            used[parameter] = true;
            const ast::AstNode* argument = arguments[i];
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression)
                continue;
            const std::optional<ScopeName> name = lookupScopeName(scope, argument->text());
            if (name.has_value() && name->kind == ScopeNameKind::Parameter)
                origins[parameter] = name->semanticOrdinal;
        }
        dependents[callee].push_back(edges.size());
        edges.push_back({caller, callee, std::move(origins), unknownDispatch});
    };
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution != MethodCallResolutionKind::Resolved ||
            !call.selectedMemberOrdinal.has_value())
        {
            if (call.callerMethodMemberOrdinal < count)
                arrayReferenceWritesUnknown_[call.callerMethodMemberOrdinal] = true;
            continue;
        }
        addEdge(call.callerMethodMemberOrdinal, *call.selectedMemberOrdinal, call.scopeOrdinal,
            call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes,
            call.interfaceCall || call.selectedUsesExpandedParameterPack);
    }
    for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
    {
        if (call.resolution != MethodCallResolutionKind::Resolved ||
            !call.selectedFunctionOrdinal.has_value())
        {
            if (call.callerCallableOrdinal < count)
                arrayReferenceWritesUnknown_[call.callerCallableOrdinal] = true;
            continue;
        }
        addEdge(call.callerCallableOrdinal, namespaceCallableBase_ + *call.selectedFunctionOrdinal,
            call.scopeOrdinal, call.selectedArgumentParameterOrdinals,
            call.selectedArgumentSyntaxes, call.selectedUsesExpandedParameterPack);
    }
    std::vector<std::size_t> worklist;
    std::vector<bool> queued(count, true);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) worklist.push_back(ordinal);
    std::size_t cursor = 0;
    while (cursor < worklist.size())
    {
        const std::size_t callee = worklist[cursor++];
        queued[callee] = false;
        const std::size_t effectCount = arrayReferenceWriteSummaries_[callee].size();
        for (const std::size_t edgeOrdinal : dependents[callee])
        {
            Edge& edge = edges[edgeOrdinal];
            bool unknown = edge.unknownDispatch || arrayReferenceWritesUnknown_[callee];
            bool changed = false;
            const auto mapParameter = [&](std::size_t parameter) -> std::optional<std::size_t>
            {
                return parameter < edge.parameterOrigins.size()
                    ? edge.parameterOrigins[parameter] : std::nullopt;
            };
            for (std::size_t effectOrdinal = edge.processedEffects;
                 effectOrdinal < effectCount; ++effectOrdinal)
            {
                // Copy one element before a recursive edge can grow the vector.
                const MethodDirectArrayReferenceWrite effect =
                    arrayReferenceWriteSummaries_[callee][effectOrdinal];
                const auto source = mapParameter(effect.sourceParameterOrdinal);
                const auto array = mapParameter(effect.arrayParameterOrdinal);
                if (!source.has_value() || !array.has_value())
                {
                    unknown = true;
                    continue;
                }
                MethodDirectArrayReferenceWrite mapped{*source, *array, effect.index, std::nullopt};
                if (effect.indexParameterOrdinal) mapped.indexParameterOrdinal = mapParameter(*effect.indexParameterOrdinal);
                if (seen[edge.caller].insert(mapped).second)
                {
                    arrayReferenceWriteSummaries_[edge.caller].push_back(mapped);
                    changed = true;
                }
            }
            edge.processedEffects = effectCount;
            if (unknown && !arrayReferenceWritesUnknown_[edge.caller])
            {
                arrayReferenceWritesUnknown_[edge.caller] = true;
                changed = true;
            }
            if (changed && !queued[edge.caller])
            {
                queued[edge.caller] = true;
                worklist.push_back(edge.caller);
            }
        }
    }
    for (auto& summary : arrayReferenceWriteSummaries_)
        std::sort(summary.begin(), summary.end(), [](const auto& left, const auto& right)
        {
            if (left.sourceParameterOrdinal != right.sourceParameterOrdinal)
                return left.sourceParameterOrdinal < right.sourceParameterOrdinal;
            if (left.arrayParameterOrdinal != right.arrayParameterOrdinal)
                return left.arrayParameterOrdinal < right.arrayParameterOrdinal;
            if (left.index != right.index) return left.index < right.index;
            return left.indexParameterOrdinal < right.indexParameterOrdinal;
        });
}

void ProjectMethodBodyModel::finalizeArrayReferenceOwnedEffects()
{
    // Only classify bodies whose complete control flow is syntactically proven
    // here. Everything else stays Unknown until structured effect analysis exists.
    const std::size_t count = directArrayReferenceWrites_.size();
    arrayReferenceOwnedEffects_.clear();
    arrayReferenceOwnedEffects_.resize(count);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const ClassMemberSymbol* callable = methodSymbol(ordinal);
        if (callable == nullptr) continue;
        auto& effects = arrayReferenceOwnedEffects_[ordinal];
        effects.assign(callable->parameters.size(), ArrayReferenceOwnedEffect::Unknown);
        if (!analyzedCallableMembers_.contains(ordinal) || callable->syntax == nullptr ||
            callable->syntax->first() == nullptr || callable->effects.contains(sema::Effect::Throws) ||
            callable->effects.contains(sema::Effect::Async) ||
            callable->effects.contains(sema::Effect::Generator)) continue;
        const ast::AstNode* body = callable->syntax->first();
        if (body->kind() != ast::AstNodeKind::BlockStatement) continue;
        if (body->children().size() > 1)
        {
            // A linear body may introduce scalar literal locals before or after
            // deleting its sole owned parameter. Calls, conversions and branches
            // remain unknown, since they can change whether the call returns.
            if (callable->isUnsafe || !callable->declaredErrorTypeKey.empty() ||
                callable->declaredTypeKey != "builtin:void" || callable->parameters.size() != 1 ||
                callable->parameters[0].ownership != sema::OwnershipKind::Owned ||
                callable->parameters[0].passKind != ParameterPassKind::Value ||
                memberModel_ == nullptr || nameLookup_ == nullptr) continue;
            bool valid = true;
            bool deleted = false;
            for (const ast::AstNode* item : body->children())
            {
                if (item == nullptr) { valid = false; break; }
                if (item->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                    item->typeSyntax() != nullptr && !item->isReference() &&
                    !item->hasModifier(lexer::TokenKind::KwStatic) && item->auxiliaryText().empty() &&
                    item->first() != nullptr &&
                    (item->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                     item->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression))
                {
                    const std::string type = memberModel_->canonicalTypeKey(item->typeSyntax(), *nameLookup_);
                    const MethodExpressionTypeBinding* value = expressionTypeBinding(item->first(), ordinal);
                    if ((type == "builtin:int" || type == "builtin:bool") && value != nullptr &&
                        value->typeKey == type) continue;
                }
                const ast::AstNode* expression = item->kind() == ast::AstNodeKind::ExpressionStatement
                    ? item->first() : nullptr;
                const ast::AstNode* operand = expression != nullptr &&
                        expression->kind() == ast::AstNodeKind::DeleteExpression && !expression->flag()
                    ? expression->first() : nullptr;
                const MethodNameBinding* name = operand != nullptr ? nameBinding(operand, ordinal) : nullptr;
                if (!deleted && operand != nullptr && operand->kind() == ast::AstNodeKind::IdentifierExpression &&
                    name != nullptr && name->kind == MethodNameBindingKind::Parameter && name->parameterOrdinal == 0)
                { deleted = true; continue; }
                valid = false;
                break;
            }
            if (valid && deleted) effects[0] = ArrayReferenceOwnedEffect::Destroyed;
            continue;
        }
        const ast::AstNode* statement = body->children().empty() ? nullptr : body->children().front();
        std::optional<std::size_t> returned;
        bool proven = statement == nullptr;
        if (statement != nullptr && statement->kind() == ast::AstNodeKind::ReturnStatement)
        {
            const ast::AstNode* value = statement->first();
            if (value == nullptr) proven = true;
            else if (value->kind() == ast::AstNodeKind::MoveExpression && value->first() != nullptr &&
                     value->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                for (std::size_t i = 0; i < callable->parameters.size(); ++i)
                    if (callable->parameters[i].name == value->first()->text() &&
                        callable->parameters[i].ownership == sema::OwnershipKind::Owned &&
                        callable->declaredOwnership == sema::OwnershipKind::Owned &&
                        callable->declaredTypeKey == callable->parameters[i].typeKey)
                        returned = i;
                proven = returned.has_value();
            }
        }
        else if (statement != nullptr && statement->kind() == ast::AstNodeKind::ExpressionStatement &&
                 statement->first() != nullptr &&
                 statement->first()->kind() == ast::AstNodeKind::DeleteExpression &&
                 !statement->first()->flag() && statement->first()->first() != nullptr &&
                 statement->first()->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const std::string& name = statement->first()->first()->text();
            for (const MethodParameterSymbol& parameter : callable->parameters)
                if (parameter.name == name && parameter.ownership == sema::OwnershipKind::Owned)
                    proven = true;
        }
        if (!proven) continue;
        for (std::size_t i = 0; i < callable->parameters.size(); ++i)
            if (callable->parameters[i].ownership == sema::OwnershipKind::Owned)
                effects[i] = returned == i ? ArrayReferenceOwnedEffect::ReturnedSame
                                          : ArrayReferenceOwnedEffect::Destroyed;
    }

    // Analyze finite branches that can only inspect boolean parameters, delete
    // the owned value, introduce literal scalar locals, or return. The owned
    // parameter cleanup destroys any still-live value at a normal exit.
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const auto* callable = methodSymbol(ordinal);
        if (callable == nullptr || !analyzedCallableMembers_.contains(ordinal) || !callable->isStatic ||
            callable->isUnsafe || callable->isPrecompiled || callable->isExternC || callable->isImportedC ||
            callable->isVariadic || callable->declaredTypeKey != "builtin:void" ||
            !callable->declaredErrorTypeKey.empty() || callable->effects.contains(sema::Effect::Throws) ||
            callable->effects.contains(sema::Effect::Async) || callable->effects.contains(sema::Effect::Generator) ||
            callable->syntax == nullptr || callable->syntax->first() == nullptr || memberModel_ == nullptr ||
            nameLookup_ == nullptr) continue;
        std::optional<std::size_t> owner;
        bool parametersSafe = true;
        for (std::size_t i = 0; i < callable->parameters.size(); ++i)
        {
            const auto& parameter = callable->parameters[i];
            if (parameter.passKind != ParameterPassKind::Value || parameter.isParameterPack)
            { parametersSafe = false; break; }
            if (parameter.ownership == sema::OwnershipKind::Owned)
            {
                if (owner) { parametersSafe = false; break; }
                owner = i;
            }
            else if (parameter.ownership != sema::OwnershipKind::None || parameter.typeKey != "builtin:bool")
            { parametersSafe = false; break; }
        }
        if (!parametersSafe || !owner || *owner >= arrayReferenceOwnedEffects_[ordinal].size() ||
            arrayReferenceOwnedEffects_[ordinal][*owner] != ArrayReferenceOwnedEffect::Unknown) continue;
        constexpr unsigned alive = 1U, dead = 2U;
        struct BranchFlow { unsigned normal; bool valid; bool sawDelete; bool sawBranch; };
        std::size_t visited = 0;
        std::function<BranchFlow(const ast::AstNode*, unsigned, std::size_t)> analyze;
        analyze = [&](const ast::AstNode* node, unsigned incoming, std::size_t depth) -> BranchFlow
        {
            if (incoming == 0) return {0, true, false, false};
            if (node == nullptr || depth > 128 || ++visited > 4096) return {0, false, false, false};
            if (node->kind() == ast::AstNodeKind::BlockStatement)
            {
                BranchFlow result{incoming, true, false, false};
                for (const auto* item : node->children())
                {
                    const auto next = analyze(item, result.normal, depth + 1);
                    if (!next.valid) return next;
                    result.normal = next.normal;
                    result.sawDelete = result.sawDelete || next.sawDelete;
                    result.sawBranch = result.sawBranch || next.sawBranch;
                }
                return result;
            }
            if (node->kind() == ast::AstNodeKind::ReturnStatement && node->first() == nullptr)
                return {0, true, false, false};
            if (node->kind() == ast::AstNodeKind::IfStatement && node->first() != nullptr && node->second() != nullptr)
            {
                const auto* condition = node->first();
                bool canTrue = true, canFalse = true;
                if (condition->kind() == ast::AstNodeKind::BooleanLiteralExpression)
                {
                    canTrue = condition->tokenKind() == lexer::TokenKind::KwTrue;
                    canFalse = condition->tokenKind() == lexer::TokenKind::KwFalse;
                    if (!canTrue && !canFalse) return {0, false, false, false};
                }
                else
                {
                    const auto* binding = nameBinding(condition, ordinal);
                    if (condition->kind() != ast::AstNodeKind::IdentifierExpression || binding == nullptr ||
                        binding->kind != MethodNameBindingKind::Parameter || !binding->parameterOrdinal ||
                        *binding->parameterOrdinal >= callable->parameters.size() ||
                        callable->parameters[*binding->parameterOrdinal].typeKey != "builtin:bool")
                        return {0, false, false, false};
                }
                const auto yes = canTrue ? analyze(node->second(), incoming, depth + 1) : BranchFlow{0, true, false, false};
                const auto no = !canFalse ? BranchFlow{0, true, false, false}
                    : node->third() != nullptr ? analyze(node->third(), incoming, depth + 1)
                                              : BranchFlow{incoming, true, false, false};
                return {yes.normal | no.normal, yes.valid && no.valid,
                    yes.sawDelete || no.sawDelete, true};
            }
            if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement && node->typeSyntax() != nullptr &&
                !node->isReference() && !node->hasModifier(lexer::TokenKind::KwStatic) && node->auxiliaryText().empty() &&
                node->first() != nullptr && (node->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                node->first()->kind() == ast::AstNodeKind::BooleanLiteralExpression))
            {
                const auto type = memberModel_->canonicalTypeKey(node->typeSyntax(), *nameLookup_);
                const auto* value = expressionTypeBinding(node->first(), ordinal);
                return {incoming, (type == "builtin:int" || type == "builtin:bool") && value != nullptr &&
                    value->typeKey == type, false, false};
            }
            const auto* expression = node->kind() == ast::AstNodeKind::ExpressionStatement ? node->first() : nullptr;
            const auto* argument = expression != nullptr && expression->kind() == ast::AstNodeKind::DeleteExpression &&
                !expression->flag() ? expression->first() : nullptr;
            const auto* binding = argument != nullptr ? nameBinding(argument, ordinal) : nullptr;
            if (argument != nullptr && argument->kind() == ast::AstNodeKind::IdentifierExpression &&
                binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                binding->parameterOrdinal == owner && (incoming & dead) == 0)
                return {dead, true, true, false};
            return {0, false, false, false};
        };
        const auto flow = analyze(callable->syntax->first(), alive, 0);
        if (flow.valid && flow.sawBranch && flow.sawDelete)
            arrayReferenceOwnedEffects_[ordinal][*owner] = ArrayReferenceOwnedEffect::Destroyed;
    }

    // Propagate a proven destruction through direct, single-argument forwarding
    // calls. Unseeded cycles and calls with unmodelled initialization stay unknown.
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return;
    std::unordered_map<std::size_t, bool> safeInitializers;
    const auto trivialInitialization = [&](std::size_t type)
    {
        if (const auto found = safeInitializers.find(type); found != safeInitializers.end()) return found->second;
        bool safe = type < nameLookup_->namedTypes().size() && !nameLookup_->namedTypes()[type].isPrecompiled;
        if (safe)
        {
            for (const auto& part : nameLookup_->namedTypes()[type].parts) if (part.isPrecompiled) safe = false;
            for (const auto memberOrdinal : memberModel_->membersOfType(type))
            {
                if (memberOrdinal >= memberModel_->members().size()) { safe = false; break; }
                const auto& member = memberModel_->members()[memberOrdinal];
                if (member.declaringTypeOrdinal != type || !member.isStatic) continue;
                if (member.kind == ClassMemberKind::Constructor ||
                    (member.kind == ClassMemberKind::Field && !member.isConst &&
                     (member.isPrecompiled || member.syntax == nullptr || member.syntax->first() != nullptr)))
                    safe = false;
            }
        }
        safeInitializers.emplace(type, safe);
        return safe;
    };
    const auto safeCallable = [&](const ClassMemberSymbol* member)
    {
        return member != nullptr && member->kind == ClassMemberKind::Method && member->isStatic &&
            !member->isPrecompiled && !member->isUnsafe && !member->isExternC && !member->isImportedC &&
            !member->isVirtual && !member->isOverride && !member->isAbstract && !member->isVariadic &&
            member->declaredTypeKey == "builtin:void" && member->declaredErrorTypeKey.empty() &&
            !member->effects.contains(sema::Effect::Throws) && !member->effects.contains(sema::Effect::Async) &&
            !member->effects.contains(sema::Effect::Generator) && member->parameters.size() == 1 &&
            member->parameters[0].ownership == sema::OwnershipKind::Owned &&
            member->parameters[0].passKind == ParameterPassKind::Value &&
            trivialInitialization(member->declaringTypeOrdinal);
    };
    std::vector<std::vector<std::size_t>> dependents(count);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const auto* caller = methodSymbol(ordinal);
        if (!safeCallable(caller) || !analyzedCallableMembers_.contains(ordinal) || caller->syntax == nullptr ||
            caller->syntax->first() == nullptr || caller->syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
            caller->syntax->first()->children().size() != 1) continue;
        const auto* statement = caller->syntax->first()->children()[0];
        const auto* syntax = statement != nullptr && statement->kind() == ast::AstNodeKind::ExpressionStatement
            ? statement->first() : nullptr;
        const auto* call = syntax != nullptr && syntax->kind() == ast::AstNodeKind::CallExpression
            ? callBinding(syntax, ordinal) : nullptr;
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved || !call->staticCall ||
            call->interfaceCall || !call->selectedMemberOrdinal || *call->selectedMemberOrdinal >= count ||
            !call->selectedDefaultArguments.empty() || call->selectedParameterPackOrdinal ||
            call->selectedUsesExpandedParameterPack || call->selectedArgumentSyntaxes.size() != 1 ||
            call->selectedArgumentParameterOrdinals.size() != 1 || call->selectedArgumentParameterOrdinals[0] != 0 ||
            call->selectedConversions.size() != 1 || call->selectedConversions[0].kind != ProjectConversionKind::Identity)
            continue;
        const auto* target = methodSymbol(*call->selectedMemberOrdinal);
        const auto* group = syntax->first();
        const auto* groupBinding = group != nullptr ? nameBinding(group, ordinal) : nullptr;
        if (group == nullptr || groupBinding == nullptr || groupBinding->kind != MethodNameBindingKind::StaticMethodGroup)
            continue;
        if (group->kind() == ast::AstNodeKind::MemberExpression)
        {
            const auto* qualifier = group->first();
            const auto* qualifierBinding = qualifier != nullptr ? nameBinding(qualifier, ordinal) : nullptr;
            if (qualifier == nullptr || qualifier->kind() != ast::AstNodeKind::IdentifierExpression ||
                qualifierBinding == nullptr || qualifierBinding->kind != MethodNameBindingKind::TypeReference) continue;
        }
        else if (group->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        const auto* move = call->selectedArgumentSyntaxes[0];
        const auto* argument = move != nullptr && move->kind() == ast::AstNodeKind::MoveExpression ? move->first() : nullptr;
        const auto* binding = argument != nullptr ? nameBinding(argument, ordinal) : nullptr;
        if (!safeCallable(target) || target->parameters[0].typeKey != caller->parameters[0].typeKey ||
            argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression ||
            binding == nullptr || binding->kind != MethodNameBindingKind::Parameter || binding->parameterOrdinal != 0)
            continue;
        dependents[*call->selectedMemberOrdinal].push_back(ordinal);
    }
    std::vector<std::size_t> worklist;
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
        if (arrayReferenceOwnedEffects_[ordinal].size() == 1 &&
            arrayReferenceOwnedEffects_[ordinal][0] == ArrayReferenceOwnedEffect::Destroyed)
            worklist.push_back(ordinal);
    for (std::size_t cursor = 0; cursor < worklist.size(); ++cursor)
        for (const auto caller : dependents[worklist[cursor]])
            if (arrayReferenceOwnedEffects_[caller][0] == ArrayReferenceOwnedEffect::Unknown)
            {
                arrayReferenceOwnedEffects_[caller][0] = ArrayReferenceOwnedEffect::Destroyed;
                worklist.push_back(caller);
            }
}

void ProjectMethodBodyModel::finalizeArrayReferencePublicationSummaries()
{
    const std::size_t count = directArrayReferenceWrites_.size();
    arrayReferenceOnlyModeledPublication_.assign(count, false);
    const auto scalarBinding = [&](const ast::AstNode* node, std::size_t callable,
                                   MethodNameBindingKind kind)
    {
        if (node == nullptr || node->kind() != ast::AstNodeKind::IdentifierExpression) return false;
        const MethodNameBinding* binding = nameBinding(node, callable);
        return binding != nullptr && binding->kind == kind &&
            (ProjectTypeConversion::isIntegral(binding->typeKey) || binding->typeKey == "builtin:bool");
    };
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const ClassMemberSymbol* callable = methodSymbol(ordinal);
        if (callable == nullptr || !analyzedCallableMembers_.contains(ordinal) ||
            callable->syntax == nullptr || callable->syntax->first() == nullptr ||
            callable->effects.contains(sema::Effect::Throws) ||
            callable->effects.contains(sema::Effect::Async) ||
            callable->effects.contains(sema::Effect::Generator)) continue;
        bool owned = false;
        for (const MethodParameterSymbol& parameter : callable->parameters)
            owned = owned || parameter.ownership == sema::OwnershipKind::Owned ||
                (memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(parameter.typeKey));
        if (owned) continue;
        const ast::AstNode* body = callable->syntax->first();
        if (body->kind() != ast::AstNodeKind::BlockStatement || body->children().size() > 1) continue;
        if (body->children().empty())
        {
            arrayReferenceOnlyModeledPublication_[ordinal] = true;
            continue;
        }
        const ast::AstNode* statement = body->children().front();
        if (statement == nullptr) continue;
        if (statement->kind() == ast::AstNodeKind::ReturnStatement &&
            (ProjectTypeConversion::isIntegral(callable->declaredTypeKey) ||
             callable->declaredTypeKey == "builtin:bool"))
        {
            const ast::AstNode* value = statement->first();
            arrayReferenceOnlyModeledPublication_[ordinal] = value != nullptr &&
                (value->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                 value->kind() == ast::AstNodeKind::BooleanLiteralExpression ||
                 scalarBinding(value, ordinal, MethodNameBindingKind::ImplicitField) ||
                 scalarBinding(value, ordinal, MethodNameBindingKind::Parameter));
            continue;
        }
        if (statement->kind() != ast::AstNodeKind::ExpressionStatement ||
            statement->first() == nullptr ||
            statement->first()->kind() != ast::AstNodeKind::AssignmentExpression) continue;
        const ast::AstNode* assignment = statement->first();
        if (assignment->tokenKind() != lexer::TokenKind::Equal || assignment->first() == nullptr ||
            assignment->second() == nullptr) continue;
        if (scalarBinding(assignment->first(), ordinal, MethodNameBindingKind::ImplicitField) &&
            (scalarBinding(assignment->second(), ordinal, MethodNameBindingKind::Parameter) ||
             assignment->second()->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
             assignment->second()->kind() == ast::AstNodeKind::BooleanLiteralExpression))
        {
            arrayReferenceOnlyModeledPublication_[ordinal] = true;
            continue;
        }
        const ast::AstNode* index = assignment->first();
        if (index->kind() != ast::AstNodeKind::IndexExpression ||
            directArrayReferenceWritesUnknown_[ordinal] ||
            directArrayReferenceWrites_[ordinal].size() != 1) continue;
        const ast::AstNode* subscript = index->children().size() == 1
            ? index->children().front() : (index->children().empty() ? index->second() : nullptr);
        arrayReferenceOnlyModeledPublication_[ordinal] = subscript != nullptr &&
            (subscript->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
             scalarBinding(subscript, ordinal, MethodNameBindingKind::Parameter));
    }

    // Construction proof is intentionally narrower than the language: source
    // classes without a base, fields or user constructors only. A precompiled
    // root, base initializer or field can hide publication and stays unknown.
    const std::size_t types = nameLookup_ != nullptr ? nameLookup_->namedTypes().size() : 0;
    arrayReferencePrivateDefaultConstruction_.assign(types, false);
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr) return;
    for (std::size_t type = 0; type < types; ++type)
    {
        const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[type];
        if (declaration.kind != NamedTypeDeclarationKind::Class || declaration.isPrecompiled ||
            inheritanceModel_->baseTypeOrdinal(type).has_value()) continue;
        bool safe = true;
        for (const NamedTypeDeclarationPart& part : declaration.parts)
            if (part.isPrecompiled) safe = false;
        arrayReferencePrivateDefaultConstruction_[type] = safe;
    }
    // Each member disqualifies at most its declaring class; scanning members
    // once avoids a classes-times-members walk in projects with many types.
    for (const ClassMemberSymbol& member : memberModel_->members())
        if (member.declaringTypeOrdinal < types &&
            ((member.kind == ClassMemberKind::Field && !member.isStatic) ||
             (member.kind == ClassMemberKind::Constructor && !member.isCompilerGenerated)))
            arrayReferencePrivateDefaultConstruction_[member.declaringTypeOrdinal] = false;

    // A narrow proof that replay sees every possible publication in the caller.
    // Straight-line bodies and one restricted parameter-controlled branch may
    // qualify. Unsupported syntax keeps the proof false.
    arrayReferenceLinearPublicationCoverage_.assign(count, false);
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
    {
        const ClassMemberSymbol* callable = methodSymbol(ordinal);
        if (callable == nullptr || !analyzedCallableMembers_.contains(ordinal) ||
            callable->syntax == nullptr || callable->syntax->first() == nullptr ||
            callable->effects.contains(sema::Effect::Throws) ||
            callable->effects.contains(sema::Effect::Async) ||
            callable->effects.contains(sema::Effect::Generator)) continue;
        const ast::AstNode* body = callable->syntax->first();
        if (body->kind() != ast::AstNodeKind::BlockStatement) continue;
        const auto simpleValue = [&](const ast::AstNode* expression)
        {
            if (expression == nullptr) return false;
            if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                expression->kind() == ast::AstNodeKind::BooleanLiteralExpression ||
                expression->kind() == ast::AstNodeKind::StringLiteralExpression) return true;
            if (expression->kind() != ast::AstNodeKind::IdentifierExpression) return false;
            const MethodNameBinding* name = nameBinding(expression, ordinal);
            return name != nullptr && (name->kind == MethodNameBindingKind::Local ||
                                       name->kind == MethodNameBindingKind::Parameter);
        };
        const auto simpleIntegral = [&](const ast::AstNode* expression)
        {
            if (!simpleValue(expression)) return false;
            const std::string_view type = recordedExpressionTypeKey(expression, ordinal);
            return ProjectTypeConversion::isIntegral(type) || type == "builtin:bool";
        };
        const auto simpleScalarReturn = [&](const ast::AstNode* expression)
        {
            if (simpleIntegral(expression)) return true;
            return expression != nullptr && expression->kind() == ast::AstNodeKind::BinaryExpression &&
                expression->tokenKind() == lexer::TokenKind::Plus &&
                ProjectTypeConversion::isIntegral(recordedExpressionTypeKey(expression, ordinal)) &&
                simpleIntegral(expression->first()) && simpleIntegral(expression->second());
        };
        const auto simpleScalarArrayField = [&](const ast::AstNode* expression, const std::string& type)
        {
            if (expression == nullptr || expression->kind() != ast::AstNodeKind::MemberExpression)
                return false;
            const MethodNameBinding* field = nameBinding(expression, ordinal);
            if (field == nullptr || field->kind != MethodNameBindingKind::ExplicitObjectField ||
                field->typeKey != type || field->candidateMemberOrdinals.size() != 1 ||
                field->candidateMemberOrdinals[0] >= memberModel_->members().size()) return false;
            const auto& member = memberModel_->members()[field->candidateMemberOrdinals[0]];
            const ast::AstNode* receiver = field->receiverSyntax;
            if (member.kind != ClassMemberKind::Field || member.isStatic || member.isVolatile ||
                receiver == nullptr || receiver != expression->first() ||
                receiver->kind() != ast::AstNodeKind::IndexExpression || !simpleValue(receiver->first())) return false;
            const ast::AstNode* index = receiver->children().size() == 1
                ? receiver->children().front()
                : (receiver->children().empty() ? receiver->second() : nullptr);
            return index != nullptr && index->kind() == ast::AstNodeKind::IntegerLiteralExpression;
        };
        std::function<bool(const ast::AstNode*)> simpleCall;
        simpleCall = [&](const ast::AstNode* expression)
        {
            if (expression == nullptr || expression->kind() != ast::AstNodeKind::CallExpression)
                return false;
            const MethodCallBinding* call = callBinding(expression, ordinal);
            if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved ||
                !call->selectedMemberOrdinal.has_value() || call->selectedUsesExpandedParameterPack)
                return false;
            for (std::size_t i = 0; i < call->selectedArgumentSyntaxes.size(); ++i)
            {
                const ast::AstNode* argument = call->selectedArgumentSyntaxes[i];
                if (simpleValue(argument)) continue;
                if (argument != nullptr && argument->kind() == ast::AstNodeKind::MoveExpression &&
                    simpleValue(argument->first()) && i < call->selectedArgumentParameterOrdinals.size() &&
                    arrayReferenceOwnedEffect(*call->selectedMemberOrdinal,
                        call->selectedArgumentParameterOrdinals[i]) != ArrayReferenceOwnedEffect::Unknown)
                    continue;
                if (argument != nullptr && argument->kind() == ast::AstNodeKind::CallExpression)
                {
                    const MethodCallBinding* nested = callBinding(argument, ordinal);
                    if (nested != nullptr && nested->selectedMemberOrdinal.has_value() &&
                        arrayReferenceHasOnlyModeledPublication(*nested->selectedMemberOrdinal) &&
                        ProjectTypeConversion::isIntegral(nested->resultTypeKey) && simpleCall(argument))
                        continue;
                }
                return false;
            }
            if (call->staticCall || call->receiverSyntax == nullptr) return true;
            if (simpleValue(call->receiverSyntax)) return true;
            const ast::AstNode* receiver = call->receiverSyntax;
            if (receiver->kind() != ast::AstNodeKind::IndexExpression ||
                !simpleValue(receiver->first())) return false;
            const ast::AstNode* index = receiver->children().size() == 1
                ? receiver->children().front()
                : (receiver->children().empty() ? receiver->second() : nullptr);
            return index != nullptr && (index->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                arrayReferenceStableLocalIndex(index, ordinal).has_value());
        };
        bool covered = true;
        bool sawIf = false;
        for (std::size_t i = 0; i < body->children().size() && covered; ++i)
        {
            const ast::AstNode* statement = body->children()[i];
            if (statement == nullptr) { covered = false; break; }
            if (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                statement->typeSyntax() != nullptr && statement->first() != nullptr)
            {
                const std::string type = memberModel_->canonicalTypeKey(statement->typeSyntax(), *nameLookup_);
                const ast::AstNode* initial = statement->first();
                if (isReferenceTypeKey(type))
                {
                    covered = initial->kind() == ast::AstNodeKind::ObjectCreationExpression;
                    for (const ast::AstNode* argument : initial->children())
                        covered = covered && simpleIntegral(argument);
                }
                else if (ProjectTypeConversion::isNativeArray(type))
                {
                    covered = initial->kind() == ast::AstNodeKind::NewExpression && initial->flag() &&
                        initial->first() != nullptr &&
                        initial->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression;
                    if (!covered && initial->kind() == ast::AstNodeKind::IdentifierExpression &&
                        !statement->isReference() && !statement->hasModifier(lexer::TokenKind::KwStatic) &&
                        !statement->hasModifier(lexer::TokenKind::KwRaii))
                    {
                        const auto* source = nameBinding(initial, ordinal);
                        const auto* value = expressionTypeBinding(initial, ordinal);
                        covered = source != nullptr && source->kind == MethodNameBindingKind::Local &&
                            source->localOrdinal && *source->localOrdinal < locals_.size() &&
                            !locals_[*source->localOrdinal].isReference && !locals_[*source->localOrdinal].isStatic &&
                            locals_[*source->localOrdinal].typeKey == type && value != nullptr && value->typeKey == type;
                    }
                }
                else if (ProjectTypeConversion::isIntegral(type) || type == "builtin:bool")
                    covered = simpleValue(initial) || simpleCall(initial) || simpleScalarArrayField(initial, type);
                else covered = false;
                continue;
            }
            if (statement->kind() == ast::AstNodeKind::ExpressionStatement && statement->first() != nullptr)
            {
                const ast::AstNode* expression = statement->first();
                if (expression->kind() == ast::AstNodeKind::AssignmentExpression &&
                    expression->tokenKind() == lexer::TokenKind::Equal && expression->second() != nullptr &&
                    (expression->second()->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                     expression->second()->kind() == ast::AstNodeKind::BooleanLiteralExpression))
                {
                    const std::string type(recordedExpressionTypeKey(expression->second(), ordinal));
                    covered = (ProjectTypeConversion::isIntegral(type) || type == "builtin:bool") &&
                        simpleScalarArrayField(expression->first(), type);
                    continue;
                }
                if (expression->kind() == ast::AstNodeKind::DeleteExpression && expression->flag())
                {
                    covered = simpleValue(expression->first()) && i + 1 < body->children().size();
                    for (std::size_t tail = i + 1; tail < body->children().size() && covered; ++tail)
                    {
                        const ast::AstNode* next = body->children()[tail];
                        if (next != nullptr && next->kind() == ast::AstNodeKind::ReturnStatement)
                            covered = tail + 1 == body->children().size() && simpleScalarReturn(next->first());
                        else covered = next != nullptr &&
                            next->kind() == ast::AstNodeKind::ExpressionStatement &&
                            next->first() != nullptr &&
                            next->first()->kind() == ast::AstNodeKind::DeleteExpression &&
                            next->first()->flag() && simpleValue(next->first()->first());
                    }
                }
                else covered = simpleCall(expression);
                continue;
            }
            if (statement->kind() == ast::AstNodeKind::ReturnStatement)
            {
                covered = i + 1 == body->children().size() && simpleScalarReturn(statement->first()) &&
                    (ProjectTypeConversion::isIntegral(callable->declaredTypeKey) ||
                     callable->declaredTypeKey == "builtin:bool");
                continue;
            }
            if (statement->kind() == ast::AstNodeKind::IfStatement && !sawIf)
            {
                sawIf = true;
                const ast::AstNode* condition = statement->first();
                const MethodNameBinding* name = condition != nullptr &&
                        condition->kind() == ast::AstNodeKind::IdentifierExpression
                    ? nameBinding(condition, ordinal) : nullptr;
                covered = name != nullptr && name->kind == MethodNameBindingKind::Parameter &&
                    name->typeKey == "builtin:bool" && name->parameterOrdinal.has_value() &&
                    *name->parameterOrdinal < callable->parameters.size() && !callable->isUnsafe &&
                    callable->parameters[*name->parameterOrdinal].passKind == ParameterPassKind::Value;
                if (covered)
                {
                    // The branch parameter must remain unconstrained and inaccessible to
                    // other operations in this restricted callable, including ref calls.
                    std::vector<const ast::AstNode*> pending{body};
                    while (!pending.empty() && covered)
                    {
                        const ast::AstNode* node = pending.back();
                        pending.pop_back();
                        if (node == nullptr || node == condition) continue;
                        if (node->kind() == ast::AstNodeKind::IdentifierExpression)
                        {
                            const MethodNameBinding* use = nameBinding(node, ordinal);
                            if (use != nullptr && use->kind == MethodNameBindingKind::Parameter &&
                                use->parameterOrdinal == name->parameterOrdinal) covered = false;
                        }
                        pending.push_back(node->first());
                        pending.push_back(node->second());
                        pending.push_back(node->third());
                        for (const ast::AstNode* child : node->children()) pending.push_back(child);
                    }
                }
                const auto coveredBranch = [&](const ast::AstNode* branch)
                {
                    if (branch == nullptr) return true;
                    if (branch->kind() != ast::AstNodeKind::BlockStatement) return false;
                    for (const ast::AstNode* item : branch->children())
                        if (item == nullptr || item->kind() != ast::AstNodeKind::ExpressionStatement ||
                            !simpleCall(item->first())) return false;
                    return true;
                };
                covered = covered && statement->second() != nullptr &&
                    coveredBranch(statement->second()) && coveredBranch(statement->third());
                continue;
            }
            covered = false;
        }
        arrayReferenceLinearPublicationCoverage_[ordinal] = covered;
    }
}

void ProjectMethodBodyModel::noteCapturedParameterEscapes()
{
    // A closure value can be returned, stored or passed onward. Until T08 gives captures an
    // explicit non-escaping callable type, a captured parameter is a global/heap escape.
    for (const MethodLambdaBinding& lambda : lambdaBindings_)
    {
        for (const MethodLambdaCapture& capture : lambda.captures)
        {
            if (capture.kind == MethodLambdaCaptureKind::Parameter &&
                capture.parameterOrdinal.has_value())
            {
                noteParameterEscape(
                    lambda.ownerMemberOrdinal,
                    *capture.parameterOrdinal,
                    language::EscapeClass::Global);
            }
        }
    }
}

std::size_t ProjectMethodBodyModel::initializeEscapeCells(
    std::vector<std::size_t>& callableCellOffsets,
    std::vector<std::size_t>& cellCallableOrdinals,
    std::vector<std::size_t>& cellParameterOrdinals)
{
    // Build one cell per (callable, parameter), then reverse edges from a callee cell to the
    // caller cells that depend on it. Argument origins are classified exactly once here. The old
    // fixed-point loop reclassified every argument and rescanned every call after every change.
    callableCellOffsets.assign(parameterEscapeSummaries_.size() + 1U, 0U);
    for (std::size_t callableOrdinal = 0;
         callableOrdinal < parameterEscapeSummaries_.size();
         ++callableOrdinal)
    {
        callableCellOffsets[callableOrdinal + 1U] =
            callableCellOffsets[callableOrdinal] +
            parameterEscapeSummaries_[callableOrdinal].size();
    }
    const std::size_t cellCount = callableCellOffsets.back();
    escapePropagationStatistics_.cells = cellCount;
    cellCallableOrdinals.assign(cellCount, 0U);
    cellParameterOrdinals.assign(cellCount, 0U);
    for (std::size_t callableOrdinal = 0;
         callableOrdinal < parameterEscapeSummaries_.size();
         ++callableOrdinal)
    {
        for (std::size_t parameterOrdinal = 0;
             parameterOrdinal < parameterEscapeSummaries_[callableOrdinal].size();
             ++parameterOrdinal)
        {
            const std::size_t cell = callableCellOffsets[callableOrdinal] + parameterOrdinal;
            cellCallableOrdinals[cell] = callableOrdinal;
            cellParameterOrdinals[cell] = parameterOrdinal;
        }
    }
    return cellCount;
}

void ProjectMethodBodyModel::addEscapeDependencies(
    std::size_t callerOrdinal,
    std::size_t scopeOrdinal,
    std::size_t calleeOrdinal,
    std::span<const std::size_t> parameterOrdinals,
    std::span<const ast::AstNode* const> argumentSyntaxes,
    std::span<const std::size_t> callableCellOffsets,
    std::vector<std::vector<std::size_t>>& dependents)
{
    if (callerOrdinal >= parameterEscapeSummaries_.size() ||
        calleeOrdinal >= parameterEscapeSummaries_.size()) return;
    const ClassMemberSymbol* caller = methodSymbol(callerOrdinal);
    if (caller == nullptr) return;
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < parameterOrdinals.size() &&
         argumentOrdinal < argumentSyntaxes.size(); ++argumentOrdinal)
    {
        const std::size_t calleeParameter = parameterOrdinals[argumentOrdinal];
        if (calleeParameter >= parameterEscapeSummaries_[calleeOrdinal].size() ||
            argumentSyntaxes[argumentOrdinal] == nullptr) continue;
        const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(
            argumentSyntaxes[argumentOrdinal], scopeOrdinal);
        ++escapePropagationStatistics_.argumentOriginsClassified;
        if (!origin.ordinal.has_value() ||
            (origin.kind != MethodEscapeOriginKind::ExternalStorage &&
             origin.kind != MethodEscapeOriginKind::CurrentFrameParameter) ||
            *origin.ordinal >= caller->parameters.size() ||
            !parameterMayCarryEscape(caller->parameters[*origin.ordinal])) continue;
        const std::size_t sourceCell = callableCellOffsets[calleeOrdinal] + calleeParameter;
        const std::size_t dependentCell = callableCellOffsets[callerOrdinal] + *origin.ordinal;
        dependents[sourceCell].push_back(dependentCell);
    }
}

void ProjectMethodBodyModel::buildEscapeDependencies(
    std::span<const std::size_t> callableCellOffsets,
    std::vector<std::vector<std::size_t>>& dependents)
{
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedMemberOrdinal.has_value())
            addEscapeDependencies(
                call.callerMethodMemberOrdinal,
                call.scopeOrdinal,
                *call.selectedMemberOrdinal,
                call.selectedArgumentParameterOrdinals,
                call.selectedArgumentSyntaxes,
                callableCellOffsets,
                dependents);
    }
    for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedFunctionOrdinal.has_value())
            addEscapeDependencies(
                call.callerCallableOrdinal,
                call.scopeOrdinal,
                namespaceCallableBase_ + *call.selectedFunctionOrdinal,
                call.selectedArgumentParameterOrdinals,
                call.selectedArgumentSyntaxes,
                callableCellOffsets,
                dependents);
    }
    for (std::vector<std::size_t>& targets : dependents)
    {
        std::ranges::sort(targets);
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
        escapePropagationStatistics_.edges += targets.size();
    }
}

void ProjectMethodBodyModel::propagateEscapeDependencies(
    std::span<const std::size_t> cellCallableOrdinals,
    std::span<const std::size_t> cellParameterOrdinals,
    const std::vector<std::vector<std::size_t>>& dependents)
{
    std::vector<std::size_t> worklist;
    worklist.reserve(dependents.size());
    std::vector<bool> queued(dependents.size(), true);
    for (std::size_t cell = 0; cell < dependents.size(); ++cell) worklist.push_back(cell);
    std::size_t cursor = 0;
    while (cursor < worklist.size())
    {
        const std::size_t sourceCell = worklist[cursor++];
        ++escapePropagationStatistics_.cellsEvaluated;
        queued[sourceCell] = false;
        const std::size_t sourceCallable = cellCallableOrdinals[sourceCell];
        const std::size_t sourceParameter = cellParameterOrdinals[sourceCell];
        const language::EscapeClass sourceClass =
            parameterEscapeSummaries_[sourceCallable][sourceParameter];
        for (const std::size_t dependentCell : dependents[sourceCell])
        {
            ++escapePropagationStatistics_.edgeEvaluations;
            const std::size_t targetCallable = cellCallableOrdinals[dependentCell];
            const std::size_t targetParameter = cellParameterOrdinals[dependentCell];
            language::EscapeClass& targetClass =
                parameterEscapeSummaries_[targetCallable][targetParameter];
            if (escapeClassRank(sourceClass) <= escapeClassRank(targetClass)) continue;
            targetClass = sourceClass;
            ++escapePropagationStatistics_.summaryUpdates;
            if (!queued[dependentCell])
            {
                queued[dependentCell] = true;
                worklist.push_back(dependentCell);
            }
        }
    }
}

void ProjectMethodBodyModel::validateEscapeCall(
    std::size_t scopeOrdinal,
    std::string_view calleeName,
    std::size_t calleeOrdinal,
    std::span<const std::size_t> parameterOrdinals,
    std::span<const ast::AstNode* const> argumentSyntaxes,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (calleeOrdinal >= parameterEscapeSummaries_.size()) return;
    const std::vector<language::EscapeClass>& callee = parameterEscapeSummaries_[calleeOrdinal];
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < parameterOrdinals.size() &&
         argumentOrdinal < argumentSyntaxes.size(); ++argumentOrdinal)
    {
        const std::size_t targetParameter = parameterOrdinals[argumentOrdinal];
        if (targetParameter >= callee.size() || argumentSyntaxes[argumentOrdinal] == nullptr)
            continue;
        const language::EscapeClass contract = callee[targetParameter];
        if (contract != language::EscapeClass::Global) continue;
        const ClassMemberSymbol* calleeSymbol = methodSymbol(calleeOrdinal);
        const bool pointerValue = calleeSymbol != nullptr &&
            targetParameter < calleeSymbol->parameters.size() &&
            calleeSymbol->parameters[targetParameter].passKind == ParameterPassKind::Value &&
            calleeSymbol->parameters[targetParameter].typeKey.ends_with("*");
        const MethodEscapeOrigin origin = pointerValue
            ? pointerValueEscapeOriginOfExpression(argumentSyntaxes[argumentOrdinal], scopeOrdinal)
            : referenceEscapeOriginOfExpression(argumentSyntaxes[argumentOrdinal], scopeOrdinal);
        if (origin.kind != MethodEscapeOriginKind::CurrentFrameLocal &&
            origin.kind != MethodEscapeOriginKind::VariableLengthArray) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntaxes[argumentOrdinal]->range(),
            "argument " + std::to_string(argumentOrdinal + 1U) + " of '" +
                std::string(calleeName) + "' refers to current-frame storage, but parameter " +
                std::to_string(targetParameter + 1U) +
                " is retained by the callee's published escape contract");
    }
}

void ProjectMethodBodyModel::validateEscapeCalls(
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedMemberOrdinal.has_value())
            validateEscapeCall(
                call.scopeOrdinal,
                call.methodName,
                *call.selectedMemberOrdinal,
                call.selectedArgumentParameterOrdinals,
                call.selectedArgumentSyntaxes,
                diagnostics);
    }
    for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedFunctionOrdinal.has_value())
            validateEscapeCall(
                call.scopeOrdinal,
                call.writtenName,
                namespaceCallableBase_ + *call.selectedFunctionOrdinal,
                call.selectedArgumentParameterOrdinals,
                call.selectedArgumentSyntaxes,
                diagnostics);
    }
}
}
