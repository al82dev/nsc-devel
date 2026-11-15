#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
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
    if (expression != nullptr &&
        (expression->kind() == ast::AstNodeKind::ConditionalExpression ||
         (expression->kind() == ast::AstNodeKind::BinaryExpression &&
          (expression->tokenKind() == lexer::TokenKind::Plus ||
           expression->tokenKind() == lexer::TokenKind::Minus))))
    {
        MethodEscapeOrigin result;
        std::vector<const ast::AstNode*> pending{expression};
        while (!pending.empty())
        {
            const ast::AstNode* branch = pending.back();
            pending.pop_back();
            if (branch == nullptr) continue;
            if (branch->kind() == ast::AstNodeKind::ConditionalExpression)
            {
                pending.push_back(branch->second());
                pending.push_back(branch->third());
            }
            else if (branch->kind() == ast::AstNodeKind::BinaryExpression &&
                     (branch->tokenKind() == lexer::TokenKind::Plus ||
                      branch->tokenKind() == lexer::TokenKind::Minus))
            {
                const MethodExpressionTypeBinding* rightType = scopeOrdinal < scopes_.size()
                    ? expressionTypeBinding(branch->second(), scopes_[scopeOrdinal].methodMemberOrdinal)
                    : nullptr;
                if (rightType == nullptr)
                {
                    pending.push_back(branch->first());
                    pending.push_back(branch->second());
                }
                else
                {
                    pending.push_back(rightType->typeKey.ends_with("*")
                        ? branch->second() : branch->first());
                }
            }
            else
            {
                joinPointerValueOrigin(result, pointerValueEscapeOriginOfExpression(branch, scopeOrdinal));
            }
        }
        return result;
    }
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

bool ProjectMethodBodyModel::valueTypeCarriesNoAddress(std::string_view typeKey) const
{
    std::vector<std::string> pending{std::string(typeKey)};
    std::unordered_set<std::string> visited;
    while (!pending.empty())
    {
        const std::string key = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(key).second) continue;
        // A fixed-size array `T[N]` is N inline values of T (item 326's `byte[4] Bytes`).
        if (key.ends_with(']'))
        {
            const std::size_t open = key.rfind('[');
            const std::string_view extent = std::string_view(key).substr(open + 1U, key.size() - open - 2U);
            if (open == std::string::npos || extent.empty() ||
                !std::ranges::all_of(extent, [](char c) { return c >= '0' && c <= '9'; })) return false;
            pending.push_back(key.substr(0, open));
            continue;
        }
        if (key.starts_with("builtin:"))
        {
            if (key == "builtin:string" || key.find_first_of("*[]") != std::string::npos) return false;
            continue;
        }
        if (!key.starts_with("type:") || key.find_first_of("*[]<") != std::string::npos ||
            memberModel_ == nullptr || nameLookup_ == nullptr) return false;
        const std::optional<std::size_t> ordinal = nameLookup_->typeOrdinalForIdentity(key.substr(5));
        if (!ordinal.has_value() || *ordinal >= nameLookup_->namedTypes().size()) return false;
        const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*ordinal].kind;
        if (kind == NamedTypeDeclarationKind::Enum) continue;
        if (kind != NamedTypeDeclarationKind::Struct) return false;
        for (std::size_t member = 0; member < memberModel_->members().size(); ++member)
        {
            const ClassMemberSymbol& field = memberModel_->members()[member];
            if (field.declaringTypeOrdinal != *ordinal || field.kind != ClassMemberKind::Field || field.isStatic)
                continue;
            pending.push_back(field.declaredTypeKey);
        }
    }
    return true;
}

// LKF-017/LKF-020: whether a pointer-typed expression evaluates to an address in the current frame:
// `stackalloc` storage, the address of a local or by-value parameter, pointer arithmetic or a
// conditional over such a value, or a pointer local whose initializer is one. Unlike
// pointerValueEscapeOriginOfExpression, a cast from an integer or any other non-pointer operand is
// not followed, so `(byte*)address` built from a heap address is not mistaken for frame storage.
// Later assignments to a pointer local are not followed; only definite frame addresses are reported.
bool ProjectMethodBodyModel::pointerExpressionAddressesCurrentFrame(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    std::vector<const ast::AstNode*> pending{expression};
    std::unordered_set<const ast::AstNode*> visited;
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || !visited.insert(node).second) continue;
        if (node->kind() == ast::AstNodeKind::StackallocExpression) return true;
        if (node->kind() == ast::AstNodeKind::UnaryExpression && node->tokenKind() == lexer::TokenKind::Amp)
        {
            // An element of a fixed-size array local lives inline in the frame; a dynamic array's
            // elements live on the heap whichever local holds the array.
            const ast::AstNode* element = node->first();
            while (element != nullptr && element->kind() == ast::AstNodeKind::IndexExpression &&
                   element->first() != nullptr && element->first()->kind() == ast::AstNodeKind::IndexExpression)
                element = element->first();
            if (element != nullptr && element->kind() == ast::AstNodeKind::IndexExpression &&
                element->first() != nullptr && element->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const std::optional<ScopeName> array = lookupScopeName(scopeOrdinal, element->first()->text());
                if (array.has_value() && array->kind == ScopeNameKind::Local && array->semanticOrdinal < locals_.size())
                {
                    const MethodLocalSymbol& arrayLocal = locals_[array->semanticOrdinal];
                    if (!arrayLocal.isReference && arrayLocal.typeKey.ends_with("]") &&
                        !ProjectTypeConversion::isNativeArray(arrayLocal.typeKey)) return true;
                }
            }
            const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(node->first(), scopeOrdinal);
            if (origin.kind == MethodEscapeOriginKind::CurrentFrameLocal ||
                origin.kind == MethodEscapeOriginKind::CurrentFrameParameter ||
                origin.kind == MethodEscapeOriginKind::VariableLengthArray) return true;
            continue;
        }
        if (node->kind() == ast::AstNodeKind::ConditionalExpression)
        {
            pending.push_back(node->second());
            pending.push_back(node->third());
            continue;
        }
        if (node->kind() == ast::AstNodeKind::BinaryExpression &&
            (node->tokenKind() == lexer::TokenKind::Plus || node->tokenKind() == lexer::TokenKind::Minus))
        {
            pending.push_back(node->first());
            pending.push_back(node->second());
            continue;
        }
        if (node->kind() == ast::AstNodeKind::CastExpression)
        {
            pending.push_back(node->first());
            continue;
        }
        if (node->kind() != ast::AstNodeKind::IdentifierExpression) continue;
        const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, node->text());
        if (!name.has_value() || name->kind != ScopeNameKind::Local || name->semanticOrdinal >= locals_.size())
            continue;
        const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
        if (local.isReference || !local.typeKey.ends_with("*") || local.syntax == nullptr ||
            local.syntax->kind() != ast::AstNodeKind::VariableDeclarationStatement) continue;
        pending.push_back(local.syntax->first());
    }
    return false;
}

void ProjectMethodBodyModel::joinPointerValueOrigin(
    MethodEscapeOrigin& target,
    const MethodEscapeOrigin& source) noexcept
{
    if (originUnsafeRank(source.kind) > originUnsafeRank(target.kind)) target = source;
}

bool ProjectMethodBodyModel::parameterMayCarryEscape(
    const MethodParameterSymbol& parameter) const
{
    return parameter.passKind != ParameterPassKind::Value ||
        parameter.ownership == sema::OwnershipKind::Borrowed ||
        parameter.typeKey.ends_with("*") || !parameter.lifetimeName.empty() ||
        typeKeyIsViewLike(parameter.typeKey) || tupleTypeKeyContainsView(parameter.typeKey);
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
    if (callable == nullptr || parameterOrdinal >= callable->parameters.size())
        return;
    const MethodParameterSymbol& parameter = callable->parameters[parameterOrdinal];
    if (!parameterMayCarryEscape(parameter) &&
        !(escapeClass == language::EscapeClass::Return && isReferenceTypeKey(parameter.typeKey)))
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
    // Preserve the origin of unknown NativeSharp contracts through source
    // wrappers without changing the treatment of imported C calls.
    parameterUnknownNativeEscape_.clear();
    parameterUnknownNativeEscape_.resize(parameterEscapeSummaries_.size());
    std::vector<bool> marked(cellCount, false);
    std::vector<std::size_t> worklist;
    for (std::size_t callable = 0; callable < parameterEscapeSummaries_.size(); ++callable)
    {
        parameterUnknownNativeEscape_[callable].resize(parameterEscapeSummaries_[callable].size(), false);
        const ClassMemberSymbol* symbol = methodSymbol(callable);
        bool native = symbol != nullptr && symbol->isPrecompiled && !symbol->isImportedC && !symbol->isExternC;
        if (callable >= namespaceCallableBase_ && namespaceFunctionModel_ != nullptr)
        {
            const std::size_t function = callable - namespaceCallableBase_;
            if (function < namespaceFunctionModel_->functions().size())
            {
                const auto& declaration = namespaceFunctionModel_->functions()[function];
                native = declaration.isPrecompiled && !declaration.isExternC;
            }
        }
        if (!native) continue;
        for (std::size_t parameter = 0; parameter < parameterEscapeSummaries_[callable].size(); ++parameter)
            if (parameterEscapeSummaries_[callable][parameter] == language::EscapeClass::Unknown)
            {
                const std::size_t cell = callableCellOffsets[callable] + parameter;
                marked[cell] = true;
                worklist.push_back(cell);
            }
    }
    for (std::size_t cursor = 0; cursor < worklist.size(); ++cursor)
    {
        const std::size_t cell = worklist[cursor];
        parameterUnknownNativeEscape_[cellCallableOrdinals[cell]][cellParameterOrdinals[cell]] = true;
        for (const std::size_t dependent : dependents[cell])
            if (!marked[dependent])
            {
                marked[dependent] = true;
                worklist.push_back(dependent);
            }
    }
    validateEscapeCalls(diagnostics);
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
    const ClassMemberSymbol* callee = methodSymbol(calleeOrdinal);
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < parameterOrdinals.size() &&
         argumentOrdinal < argumentSyntaxes.size(); ++argumentOrdinal)
    {
        const std::size_t calleeParameter = parameterOrdinals[argumentOrdinal];
        if (calleeParameter >= parameterEscapeSummaries_[calleeOrdinal].size() ||
            argumentSyntaxes[argumentOrdinal] == nullptr) continue;
        const bool pointerValue = callee != nullptr && calleeParameter < callee->parameters.size() &&
            callee->parameters[calleeParameter].passKind == ParameterPassKind::Value &&
            callee->parameters[calleeParameter].typeKey.ends_with("*");
        if (pointerValue)
        {
            std::vector<std::size_t> pending;
            std::vector<const ast::AstNode*> expressions{argumentSyntaxes[argumentOrdinal]};
            std::unordered_set<std::size_t> visited;
            std::unordered_set<const ast::AstNode*> seenExpressions;
            while (!pending.empty() || !expressions.empty())
            {
                if (!expressions.empty())
                {
                    const ast::AstNode* expression = expressions.back();
                    expressions.pop_back();
                    if (expression == nullptr || !seenExpressions.insert(expression).second) continue;
                    const MethodNameBinding* binding = nameBinding(expression, callerOrdinal);
                    if (binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                        binding->parameterOrdinal.has_value() && *binding->parameterOrdinal < caller->parameters.size() &&
                        caller->parameters[*binding->parameterOrdinal].typeKey.ends_with("*"))
                        dependents[callableCellOffsets[calleeOrdinal] + calleeParameter].push_back(
                            callableCellOffsets[callerOrdinal] + *binding->parameterOrdinal);
                    else if (binding != nullptr && binding->kind == MethodNameBindingKind::Local &&
                        binding->localOrdinal.has_value() && *binding->localOrdinal < locals_.size() &&
                        locals_[*binding->localOrdinal].typeKey.ends_with("*"))
                        pending.push_back(*binding->localOrdinal);
                    if (expression->first() != nullptr) expressions.push_back(expression->first());
                    if (expression->second() != nullptr) expressions.push_back(expression->second());
                    if (expression->third() != nullptr) expressions.push_back(expression->third());
                    for (const ast::AstNode* child : expression->children())
                        if (child != nullptr) expressions.push_back(child);
                    continue;
                }
                const std::size_t localOrdinal = pending.back();
                pending.pop_back();
                if (localOrdinal >= locals_.size() || !visited.insert(localOrdinal).second) continue;
                const MethodLocalSymbol& local = locals_[localOrdinal];
                for (const std::size_t parameter : local.possibleReferenceSourceParameters)
                    if (parameter < caller->parameters.size() && caller->parameters[parameter].typeKey.ends_with("*"))
                        dependents[callableCellOffsets[calleeOrdinal] + calleeParameter].push_back(
                            callableCellOffsets[callerOrdinal] + parameter);
                pending.insert(pending.end(), local.possibleReferenceSourceLocals.begin(),
                               local.possibleReferenceSourceLocals.end());
                if (local.syntax != nullptr && local.syntax->first() != nullptr)
                    expressions.push_back(local.syntax->first());
            }
        }
        const MethodEscapeOrigin origin = pointerValue
            ? pointerValueEscapeOriginOfExpression(argumentSyntaxes[argumentOrdinal], scopeOrdinal)
            : referenceEscapeOriginOfExpression(argumentSyntaxes[argumentOrdinal], scopeOrdinal);
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
        const ClassMemberSymbol* calleeSymbol = methodSymbol(calleeOrdinal);
        bool precompiledNative = calleeSymbol != nullptr && calleeSymbol->isPrecompiled &&
            !calleeSymbol->isImportedC && !calleeSymbol->isExternC;
        if (calleeOrdinal >= namespaceCallableBase_ && namespaceFunctionModel_ != nullptr)
        {
            const std::size_t functionOrdinal = calleeOrdinal - namespaceCallableBase_;
            if (functionOrdinal < namespaceFunctionModel_->functions().size())
            {
                const auto& function = namespaceFunctionModel_->functions()[functionOrdinal];
                precompiledNative = function.isPrecompiled && !function.isExternC;
            }
        }
        if (calleeSymbol != nullptr && precompiledNative &&
            targetParameter < calleeSymbol->parameters.size() &&
            tupleTypeKeyContainsView(
                calleeSymbol->parameters[targetParameter].passKind == ParameterPassKind::Value
                    ? calleeSymbol->parameters[targetParameter].typeKey
                    : calleeSymbol->parameters[targetParameter].pointeeTypeKey))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                argumentSyntaxes[argumentOrdinal]->range(),
                "argument " + std::to_string(argumentOrdinal + 1U) + " of '" +
                    std::string(calleeName) +
                    "' contains a borrowed view whose non-escaping contract cannot be verified in the precompiled method");
            continue;
        }
        const language::EscapeClass contract = callee[targetParameter];
        const bool forwardedNativeUnknown = calleeOrdinal < parameterUnknownNativeEscape_.size() &&
            targetParameter < parameterUnknownNativeEscape_[calleeOrdinal].size() &&
            parameterUnknownNativeEscape_[calleeOrdinal][targetParameter];
        const bool unknownNativeContract = contract == language::EscapeClass::Unknown &&
            (precompiledNative || forwardedNativeUnknown);
        if (contract != language::EscapeClass::Global && !unknownNativeContract) continue;
        const bool pointerValue = calleeSymbol != nullptr &&
            targetParameter < calleeSymbol->parameters.size() &&
            calleeSymbol->parameters[targetParameter].passKind == ParameterPassKind::Value &&
            calleeSymbol->parameters[targetParameter].typeKey.ends_with("*");
        // EN: A copy union or plain struct passed by value carries no frame address; the check
        //     rejected `Api.Read(value)` across a NativeSharp DLL (nsc.ns3.t03.c-unions, LKF-295).
        // ES: Una unión copy o un struct plano por valor no lleva direcciones del marco; la regla
        //     rechazaba `Api.Read(value)` a través de una DLL NativeSharp (nsc.ns3.t03.c-unions, LKF-295).
        if (calleeSymbol != nullptr && targetParameter < calleeSymbol->parameters.size() &&
            calleeSymbol->parameters[targetParameter].passKind == ParameterPassKind::Value &&
            valueTypeCarriesNoAddress(calleeSymbol->parameters[targetParameter].typeKey))
            continue;
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
                (unknownNativeContract ? " has an unknown escape contract" :
                    " is retained by the callee's published escape contract"));
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
