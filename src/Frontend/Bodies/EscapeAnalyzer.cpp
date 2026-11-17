#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ArrayReferenceLatePathProof.h"

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
        case language::EscapeClass::RefSlotNoEscape: return 4;
        case language::EscapeClass::ObjectPointerGlobal: return 4;
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
    verifiedNoEscapeOutImplementations_.clear();
    struct PointerFieldSource final
    {
        std::size_t objectLocal;
        std::size_t scope;
        const ast::AstNode* value;
    };
    std::vector<PointerFieldSource> pointerFieldSources;
    struct PointerStaticStore final
    {
        std::size_t callable;
        std::size_t local;
        const ast::AstNode* value;
    };
    std::vector<PointerStaticStore> pointerStaticStores;
    std::unordered_map<const ast::AstNode*, std::vector<std::size_t>> pointerCallDestinations;
    std::unordered_map<const ast::AstNode*, std::vector<std::size_t>> objectConstructionDestinations;
    std::vector<MethodEscapeOrigin> objectPointerOrigins(locals_.size());
    std::vector<std::vector<std::size_t>> objectParameterOrigins(locals_.size());
    std::vector<std::vector<std::vector<std::size_t>>> outObjectParameters(parameterEscapeSummaries_.size());
    std::vector<std::vector<std::vector<std::size_t>>> outPointerLocalSources(parameterEscapeSummaries_.size());
    for (std::size_t callable = 0; callable < parameterEscapeSummaries_.size(); ++callable)
    {
        outObjectParameters[callable].resize(parameterEscapeSummaries_[callable].size());
        outPointerLocalSources[callable].resize(parameterEscapeSummaries_[callable].size());
        const ClassMemberSymbol* member = methodSymbol(callable);
        if (member != nullptr && member->isPrecompiled &&
            member->pointerOutputObjectOrigins.size() == outObjectParameters[callable].size())
            for (std::size_t output = 0; output < member->pointerOutputObjectOrigins.size(); ++output)
                for (const std::uint32_t origin : member->pointerOutputObjectOrigins[output])
                    outObjectParameters[callable][output].push_back(origin);
    }
    for (std::size_t local = 0; local < locals_.size(); ++local)
    {
        if (!locals_[local].isReference && locals_[local].typeKey.ends_with("*") &&
            locals_[local].syntax != nullptr && locals_[local].syntax->first() != nullptr)
            pointerCallDestinations[locals_[local].syntax->first()].push_back(local);
        if (!locals_[local].isReference && isReferenceTypeKey(locals_[local].typeKey) &&
            locals_[local].syntax != nullptr && locals_[local].syntax->first() != nullptr &&
            locals_[local].syntax->first()->kind() == ast::AstNodeKind::ObjectCreationExpression)
            objectConstructionDestinations[locals_[local].syntax->first()].push_back(local);
    }
    // Keep known frame addresses stored in local objects. A callee receiving
    // such an object can copy that address to an out/ref pointer argument.
    for (std::size_t callable = 0; callable < parameterEscapeSummaries_.size(); ++callable)
    {
        const ClassMemberSymbol* member = methodSymbol(callable);
        if (member == nullptr || member->isPrecompiled || member->syntax == nullptr ||
            member->syntax->first() == nullptr || !callableBodyAnalyzed(callable)) continue;
        std::vector<const ast::AstNode*> pending{member->syntax->first()};
        std::unordered_set<const ast::AstNode*> seen;
        while (!pending.empty())
        {
            const ast::AstNode* node = pending.back(); pending.pop_back();
            if (node == nullptr || !seen.insert(node).second) continue;
            if (node->kind() == ast::AstNodeKind::AssignmentExpression &&
                node->tokenKind() == lexer::TokenKind::Equal && node->first() != nullptr &&
                node->second() != nullptr && node->second()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const MethodNameBinding* destination = nameBinding(node->first(), callable);
                const MethodNameBinding* source = nameBinding(node->second(), callable);
                if (destination != nullptr && (destination->kind == MethodNameBindingKind::StaticField ||
                    destination->kind == MethodNameBindingKind::NamespaceState) && source != nullptr &&
                    source->kind == MethodNameBindingKind::Local && source->localOrdinal.has_value() &&
                    *source->localOrdinal < locals_.size() && locals_[*source->localOrdinal].typeKey.ends_with("*"))
                    pointerStaticStores.push_back({callable, *source->localOrdinal, node->second()});
            }
            if (node->kind() == ast::AstNodeKind::AssignmentExpression &&
                node->tokenKind() == lexer::TokenKind::Equal && node->first() != nullptr &&
                node->second() != nullptr && node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const MethodNameBinding* destination = nameBinding(node->first(), callable);
                if (destination != nullptr && destination->kind == MethodNameBindingKind::Local &&
                    destination->localOrdinal.has_value() && *destination->localOrdinal < locals_.size() &&
                    !locals_[*destination->localOrdinal].isReference &&
                    locals_[*destination->localOrdinal].typeKey.ends_with("*"))
                    pointerCallDestinations[node->second()].push_back(*destination->localOrdinal);
                if (destination != nullptr && destination->kind == MethodNameBindingKind::Local &&
                    destination->localOrdinal.has_value() && *destination->localOrdinal < locals_.size() &&
                    !locals_[*destination->localOrdinal].isReference &&
                    isReferenceTypeKey(locals_[*destination->localOrdinal].typeKey) &&
                    node->second()->kind() == ast::AstNodeKind::ObjectCreationExpression)
                    objectConstructionDestinations[node->second()].push_back(*destination->localOrdinal);
            }
            if (node->kind() == ast::AstNodeKind::AssignmentExpression &&
                node->tokenKind() == lexer::TokenKind::Equal && node->first() != nullptr &&
                node->second() != nullptr && node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const MethodNameBinding* output = nameBinding(node->first(), callable);
                if (output != nullptr && output->kind == MethodNameBindingKind::Parameter &&
                    output->parameterOrdinal.has_value() && *output->parameterOrdinal < member->parameters.size())
                {
                    const std::size_t out = *output->parameterOrdinal;
                    const auto& outParameter = member->parameters[out];
                    if ((outParameter.passKind == ParameterPassKind::Out || outParameter.passKind == ParameterPassKind::Ref) &&
                        outParameter.pointeeTypeKey.ends_with("*") && out < outPointerLocalSources[callable].size() &&
                        node->second()->kind() == ast::AstNodeKind::IdentifierExpression)
                    {
                        const MethodNameBinding* source = nameBinding(node->second(), callable);
                        if (source != nullptr && source->kind == MethodNameBindingKind::Local &&
                            source->localOrdinal.has_value() && *source->localOrdinal < locals_.size() &&
                            locals_[*source->localOrdinal].typeKey.ends_with("*"))
                        {
                            auto& sources = outPointerLocalSources[callable][out];
                            if (std::ranges::find(sources, *source->localOrdinal) == sources.end())
                                sources.push_back(*source->localOrdinal);
                        }
                    }
                    if ((outParameter.passKind == ParameterPassKind::Out || outParameter.passKind == ParameterPassKind::Ref) &&
                        outParameter.pointeeTypeKey.ends_with("*") && out < outObjectParameters[callable].size() &&
                        node->second()->kind() == ast::AstNodeKind::MemberExpression)
                    {
                        const MethodNameBinding* field = nameBinding(node->second(), callable);
                        const bool directField = field != nullptr && field->kind == MethodNameBindingKind::ExplicitObjectField &&
                            field->candidateMemberOrdinals.size() == 1U && memberModel_ != nullptr &&
                            field->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                            memberModel_->members()[field->candidateMemberOrdinals.front()].kind == ClassMemberKind::Field;
                        const ast::AstNode* input = directField ? node->second()->first() : nullptr;
                        const MethodNameBinding* binding = input != nullptr && input->kind() == ast::AstNodeKind::IdentifierExpression
                            ? nameBinding(input, callable) : nullptr;
                        if (binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                            binding->parameterOrdinal.has_value() && *binding->parameterOrdinal < member->parameters.size() &&
                            isReferenceTypeKey(member->parameters[*binding->parameterOrdinal].typeKey))
                        {
                            auto& sources = outObjectParameters[callable][out];
                            if (std::ranges::find(sources, *binding->parameterOrdinal) == sources.end())
                                sources.push_back(*binding->parameterOrdinal);
                        }
                    }
                }
            }
            if (node->kind() == ast::AstNodeKind::AssignmentExpression &&
                node->tokenKind() == lexer::TokenKind::Equal && node->first() != nullptr &&
                node->second() != nullptr && node->first()->kind() == ast::AstNodeKind::MemberExpression &&
                recordedExpressionTypeKey(node->first(), callable).ends_with("*"))
            {
                const ast::AstNode* root = node->first();
                while (root != nullptr && root->kind() == ast::AstNodeKind::MemberExpression)
                    root = root->first();
                const MethodNameBinding* binding = root != nullptr ? nameBinding(root, callable) : nullptr;
                if (binding != nullptr && binding->kind == MethodNameBindingKind::Local &&
                    binding->localOrdinal.has_value() && *binding->localOrdinal < locals_.size())
                    pointerFieldSources.push_back({*binding->localOrdinal, binding->scopeOrdinal, node->second()});
            }
            pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
            for (const ast::AstNode* child : node->children()) pending.push_back(child);
        }
    }
    // A callee can copy a pointer argument into a ref/out pointer slot. Preserve
    // that possible origin before checking later calls which may retain the slot's
    // value. Iterate because one call can forward the result of another call.
    const auto propagateRefPointerOrigins = [&](std::size_t scope, std::size_t calleeOrdinal,
        const ClassMemberSymbol* callee, std::span<const std::size_t> parameters,
        std::span<const ast::AstNode* const> arguments)
    {
        if (callee == nullptr) return false;
        bool changed = false;
        for (std::size_t target = 0; target < arguments.size() && target < parameters.size(); ++target)
        {
            const std::size_t ordinal = parameters[target];
            if (ordinal >= callee->parameters.size()) continue;
            const auto& parameter = callee->parameters[ordinal];
            if ((parameter.passKind != ParameterPassKind::Ref && parameter.passKind != ParameterPassKind::Out) ||
                !parameter.pointeeTypeKey.ends_with("*")) continue;
            const ast::AstNode* argument = arguments[target];
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::UnaryExpression ||
                argument->first() == nullptr || argument->first()->kind() != ast::AstNodeKind::IdentifierExpression)
                continue;
            const auto name = lookupScopeName(scope, argument->first()->text());
            if (name.has_value() && name->kind == ScopeNameKind::Parameter && scope < scopes_.size())
            {
                const std::size_t caller = scopes_[scope].methodMemberOrdinal;
                const ClassMemberSymbol* owner = methodSymbol(caller);
                const std::size_t output = name->semanticOrdinal;
                if (owner != nullptr && output < owner->parameters.size() &&
                    caller < outObjectParameters.size() && output < outObjectParameters[caller].size() &&
                    (owner->parameters[output].passKind == ParameterPassKind::Out ||
                     owner->parameters[output].passKind == ParameterPassKind::Ref) &&
                    owner->parameters[output].pointeeTypeKey.ends_with("*"))
                {
                    const auto propagateSource = [&](std::size_t sourceParameter)
                    {
                        for (std::size_t source = 0; source < arguments.size() && source < parameters.size(); ++source)
                        {
                            if (parameters[source] != sourceParameter || arguments[source] == nullptr ||
                                arguments[source]->kind() != ast::AstNodeKind::IdentifierExpression) continue;
                            const auto input = lookupScopeName(scope, arguments[source]->text());
                            if (!input.has_value()) continue;
                            auto& origins = outObjectParameters[caller][output];
                            const auto add = [&](std::size_t value)
                            {
                                if (std::ranges::find(origins, value) == origins.end())
                                { origins.push_back(value); changed = true; }
                            };
                            if (input->kind == ScopeNameKind::Parameter) add(input->semanticOrdinal);
                            else if (input->semanticOrdinal < locals_.size())
                            {
                                for (const std::size_t value : locals_[input->semanticOrdinal].possibleReferenceSourceParameters)
                                    add(value);
                                for (const std::size_t value : objectParameterOrigins[input->semanticOrdinal]) add(value);
                            }
                        }
                    };
                    if (calleeOrdinal < outObjectParameters.size() && ordinal < outObjectParameters[calleeOrdinal].size())
                    {
                        // A recursive forwarding edge can update the same cell.
                        const auto inputs = outObjectParameters[calleeOrdinal][ordinal];
                        for (const std::size_t input : inputs) propagateSource(input);
                    }
                    if (callee->isPrecompiled && !callee->isImportedC && !callee->isExternC)
                        for (std::size_t input = 0; input < callee->parameters.size(); ++input)
                            if (callee->parameters[input].passKind == ParameterPassKind::Value &&
                                isReferenceTypeKey(callee->parameters[input].typeKey)) propagateSource(input);
                }
                continue;
            }
            if (!name.has_value() || name->kind != ScopeNameKind::Local ||
                name->semanticOrdinal >= locals_.size()) continue;
            MethodLocalSymbol& local = locals_[name->semanticOrdinal];
            if (local.isReference || !local.typeKey.ends_with("*")) continue;
            if (calleeOrdinal < outObjectParameters.size() && ordinal < outObjectParameters[calleeOrdinal].size())
                for (const std::size_t sourceParameter : outObjectParameters[calleeOrdinal][ordinal])
                    for (std::size_t source = 0; source < arguments.size() && source < parameters.size(); ++source)
                    {
                        if (parameters[source] != sourceParameter || arguments[source] == nullptr ||
                            arguments[source]->kind() != ast::AstNodeKind::IdentifierExpression) continue;
                        const auto sourceName = lookupScopeName(scope, arguments[source]->text());
                        if (!sourceName.has_value()) continue;
                        auto& origins = objectParameterOrigins[name->semanticOrdinal];
                        const auto add = [&](std::size_t value)
                        {
                            if (std::ranges::find(origins, value) == origins.end())
                            { origins.push_back(value); changed = true; }
                        };
                        if (sourceName->kind == ScopeNameKind::Parameter) add(sourceName->semanticOrdinal);
                        else if (sourceName->semanticOrdinal < locals_.size())
                        {
                            for (const std::size_t value : locals_[sourceName->semanticOrdinal].possibleReferenceSourceParameters)
                                add(value);
                            for (const std::size_t value : objectParameterOrigins[sourceName->semanticOrdinal]) add(value);
                        }
                    }
            // A precompiled body cannot be inspected here. Its out pointer may
            // have come from any reference-shaped input object; preserve that
            // dependency until a producer contract can prove a narrower source.
            if (callee->isPrecompiled && !callee->isImportedC && !callee->isExternC)
                for (std::size_t source = 0; source < arguments.size() && source < parameters.size(); ++source)
                {
                    if (source == target || parameters[source] >= callee->parameters.size() ||
                        callee->parameters[parameters[source]].passKind != ParameterPassKind::Value ||
                        !isReferenceTypeKey(callee->parameters[parameters[source]].typeKey) ||
                        arguments[source] == nullptr ||
                        arguments[source]->kind() != ast::AstNodeKind::IdentifierExpression) continue;
                    const auto sourceName = lookupScopeName(scope, arguments[source]->text());
                    if (!sourceName.has_value()) continue;
                    auto& origins = objectParameterOrigins[name->semanticOrdinal];
                    const auto add = [&](std::size_t value)
                    {
                        if (std::ranges::find(origins, value) == origins.end())
                        { origins.push_back(value); changed = true; }
                    };
                    if (sourceName->kind == ScopeNameKind::Parameter) add(sourceName->semanticOrdinal);
                    else if (sourceName->semanticOrdinal < locals_.size())
                    {
                        for (const std::size_t value : locals_[sourceName->semanticOrdinal].possibleReferenceSourceParameters)
                            add(value);
                        for (const std::size_t value : objectParameterOrigins[sourceName->semanticOrdinal]) add(value);
                    }
                }
            for (std::size_t source = 0; source < arguments.size() && source < parameters.size(); ++source)
            {
                if (source == target || parameters[source] >= callee->parameters.size()) continue;
                const auto& sourceParameter = callee->parameters[parameters[source]];
                if (sourceParameter.passKind == ParameterPassKind::Out) continue;
                const bool pointerSource = sourceParameter.passKind == ParameterPassKind::Value
                    ? sourceParameter.typeKey.ends_with("*")
                    : sourceParameter.pointeeTypeKey.ends_with("*");
                const ast::AstNode* value = arguments[source];
                if (value != nullptr && value->kind() == ast::AstNodeKind::UnaryExpression &&
                    value->auxiliaryText().starts_with("pass:")) value = value->first();
                MethodEscapeOrigin origin;
                if (pointerSource) origin = pointerValueEscapeOriginOfExpression(value, scope);
                else if (value != nullptr && value->kind() == ast::AstNodeKind::IdentifierExpression)
                {
                    const auto sourceName = lookupScopeName(scope, value->text());
                    if (sourceName.has_value() && sourceName->kind == ScopeNameKind::Local &&
                        sourceName->semanticOrdinal < objectPointerOrigins.size())
                        origin = objectPointerOrigins[sourceName->semanticOrdinal];
                }
                if (originUnsafeRank(origin.kind) > originUnsafeRank(local.pointerValueOrigin.kind))
                {
                    local.pointerValueOrigin = origin;
                    changed = true;
                }
            }
        }
        return changed;
    };
    bool pointerOriginsChanged;
    do
    {
        pointerOriginsChanged = false;
        for (const PointerFieldSource& field : pointerFieldSources)
        {
            const MethodEscapeOrigin origin = pointerValueEscapeOriginOfExpression(field.value, field.scope);
            if (originUnsafeRank(origin.kind) > originUnsafeRank(objectPointerOrigins[field.objectLocal].kind))
            {
                objectPointerOrigins[field.objectLocal] = origin;
                pointerOriginsChanged = true;
            }
        }
        // A constructor can retain a pointer supplied by the caller. Keep its
        // origin on the created object before following allocator results.
        for (const ProjectConstructionSite& site : constructionSites_)
        {
            const auto targets = objectConstructionDestinations.find(site.syntax);
            if (targets == objectConstructionDestinations.end()) continue;
            for (const std::size_t target : targets->second)
            {
                if (target >= locals_.size() || locals_[target].methodMemberOrdinal != site.callableOrdinal) continue;
                for (const MethodExpressionTypeBinding& argument : site.argumentTypes)
                {
                    if (argument.syntax == nullptr) continue;
                    MethodEscapeOrigin origin;
                    if (argument.typeKey.ends_with("*"))
                        origin = pointerValueEscapeOriginOfExpression(argument.syntax, argument.scopeOrdinal);
                    else if (isReferenceTypeKey(argument.typeKey) &&
                        argument.syntax->kind() == ast::AstNodeKind::IdentifierExpression)
                    {
                        const auto source = lookupScopeName(argument.scopeOrdinal, argument.syntax->text());
                        if (!source.has_value()) continue;
                        auto& parameters = objectParameterOrigins[target];
                        const auto add = [&](std::size_t parameter)
                        {
                            if (std::ranges::find(parameters, parameter) == parameters.end())
                            { parameters.push_back(parameter); pointerOriginsChanged = true; }
                        };
                        if (source->kind == ScopeNameKind::Parameter) add(source->semanticOrdinal);
                        else if (source->kind == ScopeNameKind::Local && source->semanticOrdinal < locals_.size())
                        {
                            origin = objectPointerOrigins[source->semanticOrdinal];
                            for (const std::size_t parameter : objectParameterOrigins[source->semanticOrdinal]) add(parameter);
                            for (const std::size_t parameter : locals_[source->semanticOrdinal].possibleReferenceSourceParameters)
                                add(parameter);
                        }
                    }
                    else continue;
                    if (originUnsafeRank(origin.kind) > originUnsafeRank(objectPointerOrigins[target].kind))
                    { objectPointerOrigins[target] = origin; pointerOriginsChanged = true; }
                }
            }
        }
        for (std::size_t ordinal = 0; ordinal < locals_.size(); ++ordinal)
            for (const std::size_t source : locals_[ordinal].possibleReferenceSourceLocals)
                if (source < objectPointerOrigins.size())
                {
                    if (originUnsafeRank(objectPointerOrigins[source].kind) > originUnsafeRank(objectPointerOrigins[ordinal].kind))
                    { objectPointerOrigins[ordinal] = objectPointerOrigins[source]; pointerOriginsChanged = true; }
                    for (const std::size_t value : objectParameterOrigins[source])
                        if (std::ranges::find(objectParameterOrigins[ordinal], value) == objectParameterOrigins[ordinal].end())
                        { objectParameterOrigins[ordinal].push_back(value); pointerOriginsChanged = true; }
                }
        // An allocator or other instance method may return an address stored
        // in its receiver. Preserve the receiver's possible origins on a
        // pointer local receiving that result; an unknown body is not proof
        // that the result came from the heap.
        for (const MethodCallBinding& call : calls_)
        {
            if (call.staticCall || call.receiverSyntax == nullptr || call.syntax == nullptr ||
                !call.resultTypeKey.ends_with("*") ||
                call.receiverSyntax->kind() != ast::AstNodeKind::IdentifierExpression) continue;
            const auto targets = pointerCallDestinations.find(call.syntax);
            if (targets == pointerCallDestinations.end()) continue;
            const auto receiver = lookupScopeName(call.scopeOrdinal, call.receiverSyntax->text());
            if (!receiver.has_value()) continue;
            for (const std::size_t target : targets->second)
            {
                if (target >= locals_.size()) continue;
                auto& origins = objectParameterOrigins[target];
                const auto add = [&](std::size_t parameter)
                {
                    if (std::ranges::find(origins, parameter) == origins.end())
                    { origins.push_back(parameter); pointerOriginsChanged = true; }
                };
                if (receiver->kind == ScopeNameKind::Parameter) add(receiver->semanticOrdinal);
                else if (receiver->kind == ScopeNameKind::Local && receiver->semanticOrdinal < locals_.size())
                {
                    const std::size_t source = receiver->semanticOrdinal;
                    if (originUnsafeRank(objectPointerOrigins[source].kind) >
                        originUnsafeRank(locals_[target].pointerValueOrigin.kind))
                    {
                        locals_[target].pointerValueOrigin = objectPointerOrigins[source];
                        pointerOriginsChanged = true;
                    }
                    for (const std::size_t parameter : locals_[source].possibleReferenceSourceParameters) add(parameter);
                    for (const std::size_t parameter : objectParameterOrigins[source]) add(parameter);
                }
            }
        }
        // An out/ref pointer can be assigned from a local produced by a
        // receiver call. Carry that receiver dependency to callers as well.
        for (std::size_t callable = 0; callable < outPointerLocalSources.size(); ++callable)
            for (std::size_t output = 0; output < outPointerLocalSources[callable].size(); ++output)
                for (const std::size_t local : outPointerLocalSources[callable][output])
                    if (local < objectParameterOrigins.size())
                        for (const std::size_t parameter : objectParameterOrigins[local])
                        {
                            auto& origins = outObjectParameters[callable][output];
                            if (std::ranges::find(origins, parameter) == origins.end())
                            { origins.push_back(parameter); pointerOriginsChanged = true; }
                        }
        for (const MethodCallBinding& call : calls_)
            if (call.resolution == MethodCallResolutionKind::Resolved && call.selectedMemberOrdinal.has_value())
                pointerOriginsChanged |= propagateRefPointerOrigins(call.scopeOrdinal, *call.selectedMemberOrdinal,
                    methodSymbol(*call.selectedMemberOrdinal), call.selectedArgumentParameterOrdinals,
                    call.selectedArgumentSyntaxes);
        for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
            if (call.resolution == MethodCallResolutionKind::Resolved && call.selectedFunctionOrdinal.has_value())
                pointerOriginsChanged |= propagateRefPointerOrigins(call.scopeOrdinal, namespaceCallableBase_ + *call.selectedFunctionOrdinal,
                    methodSymbol(namespaceCallableBase_ + *call.selectedFunctionOrdinal),
                    call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes);
    } while (pointerOriginsChanged);
    pointerOutputObjectOrigins_ = std::move(outObjectParameters);
    noteCapturedParameterEscapes();
    std::vector<std::size_t> callableCellOffsets;
    std::vector<std::size_t> cellCallableOrdinals;
    std::vector<std::size_t> cellParameterOrdinals;
    const std::size_t cellCount = initializeEscapeCells(
        callableCellOffsets, cellCallableOrdinals, cellParameterOrdinals);
    std::vector<std::vector<std::size_t>> dependents(cellCount);
    buildEscapeDependencies(callableCellOffsets, dependents);
    propagateEscapeDependencies(cellCallableOrdinals, cellParameterOrdinals, dependents);
    // A published interface guarantee is meaningful only if every implementation
    // selected for a table also satisfies it. Unknown is not a proof. Restrict
    // this check to the explicit out contract introduced for this path.
    if (interfaceModel_ != nullptr && memberModel_ != nullptr)
    {
        std::vector<std::vector<std::size_t>> sources;
        std::vector<std::optional<bool>> rawAddressByCallable;
        std::unordered_set<std::size_t> indirectCallables;
        for (const MethodDelegateBinding& binding : delegateBindings_)
            if (binding.kind == MethodDelegateOperationKind::Invoke)
                indirectCallables.insert(binding.methodMemberOrdinal);
        for (const MethodEventBinding& binding : eventBindings_)
            if (binding.kind == MethodEventOperationKind::Invoke)
                indirectCallables.insert(binding.methodMemberOrdinal);
        // Constructor argument escape is not part of the dependency graph below.
        // Until it is, a body containing a construction cannot certify this contract.
        for (const ProjectConstructionSite& site : constructionSites_)
            indirectCallables.insert(site.callableOrdinal);
        for (const MethodCallBinding& call : calls_)
            if (call.selectedMemberOrdinal.has_value() &&
                *call.selectedMemberOrdinal < memberModel_->members().size() &&
                (memberModel_->members()[*call.selectedMemberOrdinal].isUserOperator ||
                 memberModel_->members()[*call.selectedMemberOrdinal].isUserConversion))
                indirectCallables.insert(call.callerMethodMemberOrdinal);
        const auto takesRawAddress = [&](std::size_t ordinal)
        {
            if (ordinal >= rawAddressByCallable.size()) return true;
            if (rawAddressByCallable[ordinal].has_value()) return *rawAddressByCallable[ordinal];
            const ClassMemberSymbol* method = methodSymbol(ordinal);
            if (method == nullptr) return true;
            bool result = false;
            if (!method->isPrecompiled && method->syntax != nullptr)
            {
                std::vector<const ast::AstNode*> pending{method->syntax->first()};
                std::unordered_set<const ast::AstNode*> visited;
                while (!pending.empty())
                {
                    const ast::AstNode* node = pending.back();
                    pending.pop_back();
                    if (node == nullptr || !visited.insert(node).second) continue;
                    if (node->kind() == ast::AstNodeKind::InlineAssemblyStatement ||
                        (node->kind() == ast::AstNodeKind::UnaryExpression &&
                         node->tokenKind() == lexer::TokenKind::Amp &&
                         !node->auxiliaryText().starts_with("pass:")))
                    {
                        result = true;
                        break;
                    }
                    pending.push_back(node->first());
                    pending.push_back(node->second());
                    pending.push_back(node->third());
                    for (const ast::AstNode* child : node->children()) pending.push_back(child);
                }
            }
            rawAddressByCallable[ordinal] = result;
            return result;
        };
        std::string unprovenDetail;
        const auto hasUnprovenAddressFlow = [&](std::size_t callable, std::size_t parameter)
        {
            if (callable >= parameterEscapeSummaries_.size() ||
                parameter >= parameterEscapeSummaries_[callable].size()) return true;
            if (sources.empty())
            {
                sources.resize(cellCount);
                rawAddressByCallable.resize(parameterEscapeSummaries_.size());
                for (std::size_t source = 0; source < dependents.size(); ++source)
                    for (const std::size_t dependent : dependents[source])
                        if (dependent < sources.size()) sources[dependent].push_back(source);
            }
            std::vector<std::size_t> pending{callableCellOffsets[callable] + parameter};
            std::unordered_set<std::size_t> visited;
            while (!pending.empty())
            {
                const std::size_t cell = pending.back();
                pending.pop_back();
                if (cell >= cellCount) return true;
                if (!visited.insert(cell).second) continue;
                const std::size_t ordinal = cellCallableOrdinals[cell];
                const ClassMemberSymbol* method = methodSymbol(ordinal);
                if (method == nullptr || takesRawAddress(ordinal) ||
                    indirectCallables.contains(ordinal) ||
                    method->effects.contains(sema::Effect::Async) ||
                    method->effects.contains(sema::Effect::Generator) ||
                    (!method->isPrecompiled && method->syntax != nullptr &&
                     method->syntax->first() != nullptr && !callableBodyAnalyzed(ordinal)))
                {
                    unprovenDetail = method != nullptr ? method->signatureKey : "missing method";
                    unprovenDetail += takesRawAddress(ordinal) ? ":raw" :
                        indirectCallables.contains(ordinal) ? ":indirect" : ":effect-or-body";
                    return true;
                }
                if (method->isPrecompiled &&
                    !(method->isAbstract && interfaceModel_->isInterface(method->declaringTypeOrdinal)))
                {
                    const std::size_t calleeParameter = cellParameterOrdinals[cell];
                    if (!method->verifiedNoEscapeOut || calleeParameter >= method->parameters.size() ||
                        method->parameters[calleeParameter].passKind != ParameterPassKind::Out ||
                        method->parameters[calleeParameter].pointeeTypeKey != "builtin:long")
                    {
                        unprovenDetail = method->signatureKey + ":precompiled";
                        return true;
                    }
                }
                pending.insert(pending.end(), sources[cell].begin(), sources[cell].end());
            }
            return false;
        };
        // An analyzed generic specialization may publish an out-long summary
        // only after checking all address flows, including forwarded calls. A
        // failed proof remains unknown and is propagated to its callers.
        for (std::size_t callable = 0; callable < parameterEscapeSummaries_.size(); ++callable)
        {
            const ClassMemberSymbol* member = methodSymbol(callable);
            if (member == nullptr || member->isPrecompiled || !callableBodyAnalyzed(callable) ||
                (!member->genericTemplateOrdinal.has_value() && member->genericTemplateKey.empty()) ||
                !member->parameterEscapeSummary.empty()) continue;
            for (std::size_t parameter = 0; parameter < member->parameters.size(); ++parameter)
            {
                if (member->parameters[parameter].passKind != ParameterPassKind::Out ||
                    member->parameters[parameter].pointeeTypeKey != "builtin:long" ||
                    parameter >= parameterEscapeSummaries_[callable].size() ||
                    parameterEscapeSummaries_[callable][parameter] != language::EscapeClass::NoEscape) continue;
                if (hasUnprovenAddressFlow(callable, parameter))
                    parameterEscapeSummaries_[callable][parameter] = language::EscapeClass::Unknown;
            }
        }
        propagateEscapeDependencies(cellCallableOrdinals, cellParameterOrdinals, dependents);
        for (std::size_t callable = 0; callable < parameterEscapeSummaries_.size(); ++callable)
        {
            const ClassMemberSymbol* member = methodSymbol(callable);
            if (member == nullptr || member->isPrecompiled || !callableBodyAnalyzed(callable) ||
                (!member->genericTemplateOrdinal.has_value() && member->genericTemplateKey.empty()) ||
                !member->parameterEscapeSummary.empty()) continue;
            bool hasProven = false;
            bool allProven = true;
            for (std::size_t parameter = 0; parameter < member->parameters.size(); ++parameter)
            {
                if (member->parameters[parameter].passKind != ParameterPassKind::Out ||
                    member->parameters[parameter].pointeeTypeKey != "builtin:long" ||
                    parameter >= parameterEscapeSummaries_[callable].size() ||
                    parameterEscapeSummaries_[callable][parameter] != language::EscapeClass::NoEscape) continue;
                hasProven = true;
                if (hasUnprovenAddressFlow(callable, parameter)) allProven = false;
            }
            if (hasProven && allProven) verifiedNoEscapeOutImplementations_.insert(callable);
        }
        for (std::size_t callable = 0; callable < parameterEscapeSummaries_.size(); ++callable)
        {
            const ClassMemberSymbol* member = methodSymbol(callable);
            if (member == nullptr || member->isPrecompiled) continue;
            for (const MethodParameterSymbol& parameter : member->parameters)
            {
                if (parameter.syntax == nullptr ||
                    parameter.syntax->auxiliaryText().find("|escape:noescape") == std::string::npos)
                    continue;
                if (parameter.passKind != ParameterPassKind::Out || parameter.pointeeTypeKey != "builtin:long" ||
                    callable >= namespaceCallableBase_ ||
                    !interfaceModel_->isInterface(member->declaringTypeOrdinal) ||
                    (member->syntax != nullptr && member->syntax->first() != nullptr))
                    diagnostics.report(diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error, parameter.range,
                        "noescape is supported only on an out long parameter of an abstract interface method");
            }
        }
        for (const ClassInterfaceTablePlan& plan : interfaceModel_->tablePlans())
        {
            if (plan.interfaceMethodMemberOrdinals.size() != plan.implementationMemberOrdinals.size())
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error, SourceRange{},
                    "cannot validate escape contracts in an incomplete interface table");
                continue;
            }
            const std::size_t slots = std::min(plan.interfaceMethodMemberOrdinals.size(),
                plan.implementationMemberOrdinals.size());
            for (std::size_t slot = 0; slot < slots; ++slot)
            {
                const std::size_t requirementOrdinal = plan.interfaceMethodMemberOrdinals[slot];
                if (!plan.implementationMemberOrdinals[slot].has_value() ||
                    requirementOrdinal >= memberModel_->members().size()) continue;
                const std::size_t implementationOrdinal = *plan.implementationMemberOrdinals[slot];
                if (implementationOrdinal == requirementOrdinal ||
                    implementationOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& requirement = memberModel_->members()[requirementOrdinal];
                const ClassMemberSymbol& implementation = memberModel_->members()[implementationOrdinal];
                const bool published = requirement.isPrecompiled &&
                    requirement.parameterEscapeSummary.size() == requirement.parameters.size();
                bool hasOutContract = false;
                bool validOutContracts = true;
                for (std::size_t parameter = 0; parameter < requirement.parameters.size(); ++parameter)
                {
                    const auto pass = requirement.parameters[parameter].passKind;
                    const bool declared = !requirement.isPrecompiled &&
                        requirement.parameters[parameter].syntax != nullptr &&
                        requirement.parameters[parameter].syntax->auxiliaryText().find("|escape:noescape") !=
                            std::string::npos;
                    if (pass != ParameterPassKind::Out ||
                        requirement.parameters[parameter].pointeeTypeKey != "builtin:long" ||
                        !(declared || (published && requirement.parameterEscapeSummary[parameter] ==
                            language::EscapeClass::NoEscape)))
                        continue;
                    hasOutContract = true;
                    const bool unprovenFlow = hasUnprovenAddressFlow(implementationOrdinal, parameter);
                    const bool analyzed = implementation.isPrecompiled ? implementation.verifiedNoEscapeOut :
                        callableBodyAnalyzed(implementationOrdinal);
                    const bool hasSummary = implementationOrdinal < parameterEscapeSummaries_.size() &&
                        parameter < parameterEscapeSummaries_[implementationOrdinal].size();
                    if (!unprovenFlow &&
                        !implementation.effects.contains(sema::Effect::Async) &&
                        !implementation.effects.contains(sema::Effect::Generator) &&
                        analyzed && hasSummary &&
                        parameterEscapeSummaries_[implementationOrdinal][parameter] == language::EscapeClass::NoEscape)
                        continue;
                    validOutContracts = false;
                    diagnostics.report(diagnostics::DiagnosticId::InvalidOverride,
                        diagnostics::DiagnosticSeverity::Error, implementation.range,
                        "interface implementation '" + implementation.signatureKey +
                            "' cannot prove the non-escaping contract of parameter " +
                            std::to_string(parameter + 1U) + " (flow=" +
                            (unprovenFlow ? unprovenDetail : "proven") + ", body=" +
                            (analyzed ? "analyzed" : "unavailable") + ", summary=" +
                            (hasSummary ? std::to_string(static_cast<int>(parameterEscapeSummaries_[implementationOrdinal][parameter])) :
                                std::string("missing")) + ")");
                }
                // The binary certificate covers every no-escape out long summary of
                // this method, not only the slot required by this interface.
                if (hasOutContract && validOutContracts && !implementation.isPrecompiled &&
                    implementationOrdinal < parameterEscapeSummaries_.size())
                {
                    bool allNoEscapeOutsVerified = true;
                    for (std::size_t parameter = 0; parameter < implementation.parameters.size(); ++parameter)
                        if (implementation.parameters[parameter].passKind == ParameterPassKind::Out &&
                            implementation.parameters[parameter].pointeeTypeKey == "builtin:long" &&
                            parameter < parameterEscapeSummaries_[implementationOrdinal].size() &&
                            parameterEscapeSummaries_[implementationOrdinal][parameter] == language::EscapeClass::NoEscape &&
                            hasUnprovenAddressFlow(implementationOrdinal, parameter))
                            allNoEscapeOutsVerified = false;
                    if (allNoEscapeOutsVerified) verifiedNoEscapeOutImplementations_.insert(implementationOrdinal);
                }
            }
        }
    }
    // A ref pointer has two independent escape questions: the address of its
    // slot and the pointer value stored in it. Publish the narrow slot fact only
    // for analyzed static bodies where address-taking, reference aliases and
    // forwarding the slot cannot occur. The pointer value remains unknown.
    for (std::size_t callable = 0; callable < namespaceCallableBase_ &&
         callable < parameterEscapeSummaries_.size(); ++callable)
    {
        const ClassMemberSymbol* member = methodSymbol(callable);
        if (member == nullptr || !member->isStatic || member->isPrecompiled || member->isImportedC ||
            member->isExternC || member->isReferenceReturn || member->syntax == nullptr || member->syntax->first() == nullptr ||
            !callableBodyAnalyzed(callable) || member->effects.contains(sema::Effect::Async) ||
            member->effects.contains(sema::Effect::Generator) || member->isUserOperator ||
            member->isUserConversion || member->genericTemplateOrdinal.has_value() ||
            !member->genericTemplateKey.empty()) continue;
        if (std::ranges::none_of(member->parameters, [](const MethodParameterSymbol& item)
            { return item.passKind == ParameterPassKind::Ref && item.pointeeTypeKey.ends_with("*"); }))
            continue;
        if (std::ranges::any_of(locals_, [&](const MethodLocalSymbol& local)
            { return local.methodMemberOrdinal == callable && local.isReference; }) ||
            std::ranges::any_of(constructionSites_, [&](const ProjectConstructionSite& site)
            { return site.callableOrdinal == callable; }) ||
            std::ranges::any_of(delegateBindings_, [&](const MethodDelegateBinding& binding)
            { return binding.methodMemberOrdinal == callable && binding.kind == MethodDelegateOperationKind::Invoke; }) ||
            std::ranges::any_of(eventBindings_, [&](const MethodEventBinding& binding)
            { return binding.methodMemberOrdinal == callable && binding.kind == MethodEventOperationKind::Invoke; }))
            continue;
        for (std::size_t parameter = 0; parameter < member->parameters.size() &&
             parameter < parameterEscapeSummaries_[callable].size(); ++parameter)
        {
            if (member->parameters[parameter].passKind != ParameterPassKind::Ref ||
                !member->parameters[parameter].pointeeTypeKey.ends_with("*")) continue;
            const auto containsParameter = [&](const ast::AstNode* root)
            {
                std::vector<const ast::AstNode*> pending{root};
                std::unordered_set<const ast::AstNode*> seen;
                while (!pending.empty())
                {
                    const ast::AstNode* node = pending.back(); pending.pop_back();
                    if (node == nullptr || !seen.insert(node).second) continue;
                    const MethodNameBinding* binding = nameBinding(node, callable);
                    if (binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                        binding->parameterOrdinal == parameter) return true;
                    pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                    for (const ast::AstNode* child : node->children()) pending.push_back(child);
                }
                return false;
            };
            bool proven = true;
            std::vector<const ast::AstNode*> pending{member->syntax->first()};
            std::unordered_set<const ast::AstNode*> seen;
            while (!pending.empty() && proven)
            {
                const ast::AstNode* node = pending.back(); pending.pop_back();
                if (node == nullptr || !seen.insert(node).second) continue;
                if (node->isReference() || node->kind() == ast::AstNodeKind::InlineAssemblyStatement ||
                    node->kind() == ast::AstNodeKind::LambdaExpression ||
                    node->kind() == ast::AstNodeKind::FunctionDeclaration ||
                    node->kind() == ast::AstNodeKind::MethodDeclaration ||
                    (node->kind() == ast::AstNodeKind::UnaryExpression &&
                     ((node->tokenKind() == lexer::TokenKind::Amp &&
                       !node->auxiliaryText().starts_with("pass:")) ||
                      (node->auxiliaryText().starts_with("pass:") && containsParameter(node)))))
                { proven = false; break; }
                pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                for (const ast::AstNode* child : node->children()) pending.push_back(child);
            }
            const auto safeArguments = [&](const ClassMemberSymbol* target,
                std::span<const std::size_t> ordinals, std::span<const ast::AstNode* const> args)
            {
                if (target == nullptr || target->isUserOperator || target->isUserConversion) return false;
                for (std::size_t i = 0; i < ordinals.size() && i < args.size(); ++i)
                    if (ordinals[i] >= target->parameters.size() ||
                        (target->parameters[ordinals[i]].passKind != ParameterPassKind::Value &&
                         containsParameter(args[i]))) return false;
                return true;
            };
            for (const MethodCallBinding& call : calls_)
                if (call.callerMethodMemberOrdinal == callable &&
                    (call.resolution != MethodCallResolutionKind::Resolved || !call.selectedMemberOrdinal.has_value() ||
                     !safeArguments(methodSymbol(*call.selectedMemberOrdinal),
                         call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes))) proven = false;
            for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
                if (call.callerCallableOrdinal == callable &&
                    (call.resolution != MethodCallResolutionKind::Resolved || !call.selectedFunctionOrdinal.has_value() ||
                     !safeArguments(methodSymbol(namespaceCallableBase_ + *call.selectedFunctionOrdinal),
                         call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes))) proven = false;
            if (proven && parameterEscapeSummaries_[callable][parameter] != language::EscapeClass::NoEscape)
                parameterEscapeSummaries_[callable][parameter] = language::EscapeClass::RefSlotNoEscape;
        }
    }
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
            if (parameterEscapeSummaries_[callable][parameter] == language::EscapeClass::Unknown ||
                parameterEscapeSummaries_[callable][parameter] == language::EscapeClass::RefSlotNoEscape ||
                parameterEscapeSummaries_[callable][parameter] == language::EscapeClass::ObjectPointerGlobal)
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
    // Keep a separate source-only summary for a pointer stored inside an
    // object parameter. Retaining that pointer is not the same as retaining
    // the object itself, so it must not become an ordinary Global summary.
    std::vector<std::vector<bool>> objectPointerSinks(parameterEscapeSummaries_.size());
    for (std::size_t i = 0; i < objectPointerSinks.size(); ++i)
    {
        objectPointerSinks[i].resize(parameterEscapeSummaries_[i].size(), false);
        for (std::size_t p = 0; p < parameterEscapeSummaries_[i].size(); ++p)
            if (parameterEscapeSummaries_[i][p] == language::EscapeClass::ObjectPointerGlobal)
                objectPointerSinks[i][p] = true;
    }
    for (const auto& store : pointerStaticStores)
    {
        if (store.callable < objectPointerSinks.size() && store.local < objectParameterOrigins.size())
            for (const std::size_t parameter : objectParameterOrigins[store.local])
                if (parameter < objectPointerSinks[store.callable].size())
                    objectPointerSinks[store.callable][parameter] = true;
        if (store.local < locals_.size())
        {
            const auto origin = locals_[store.local].pointerValueOrigin.kind;
            if (origin == MethodEscapeOriginKind::CurrentFrameLocal ||
                origin == MethodEscapeOriginKind::VariableLengthArray)
                diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error, store.value->range(),
                    "pointer to current-frame storage cannot escape into static storage");
        }
    }
    const auto forEachResolvedCall = [&](auto&& visit)
    {
        for (const MethodCallBinding& call : calls_)
            if (call.resolution == MethodCallResolutionKind::Resolved && call.selectedMemberOrdinal.has_value())
                visit(call.callerMethodMemberOrdinal, call.scopeOrdinal, *call.selectedMemberOrdinal,
                    call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes);
        for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
            if (call.resolution == MethodCallResolutionKind::Resolved && call.selectedFunctionOrdinal.has_value())
                visit(call.callerCallableOrdinal, call.scopeOrdinal, namespaceCallableBase_ + *call.selectedFunctionOrdinal,
                    call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes);
    };
    forEachResolvedCall([&](std::size_t caller, std::size_t scope, std::size_t callee,
        std::span<const std::size_t> ordinals, std::span<const ast::AstNode* const> args)
    {
        const ClassMemberSymbol* target = methodSymbol(callee);
        if (target == nullptr || caller >= objectPointerSinks.size() || callee >= parameterEscapeSummaries_.size()) return;
        for (std::size_t i = 0; i < ordinals.size() && i < args.size(); ++i)
        {
            const std::size_t p = ordinals[i];
            if (p >= target->parameters.size() || p >= parameterEscapeSummaries_[callee].size() ||
                target->parameters[p].passKind == ParameterPassKind::Out) continue;
            const auto& formal = target->parameters[p];
            if (!(formal.passKind == ParameterPassKind::Value ? formal.typeKey.ends_with("*") :
                  formal.pointeeTypeKey.ends_with("*"))) continue;
            const language::EscapeClass contract = parameterEscapeSummaries_[callee][p];
            const bool unknownNative = callee < parameterUnknownNativeEscape_.size() &&
                p < parameterUnknownNativeEscape_[callee].size() && parameterUnknownNativeEscape_[callee][p];
            if (contract != language::EscapeClass::Global && !unknownNative) continue;
            const ast::AstNode* argument = args[i];
            if (argument != nullptr && argument->kind() == ast::AstNodeKind::UnaryExpression &&
                argument->auxiliaryText().starts_with("pass:")) argument = argument->first();
            if (argument == nullptr || argument->kind() != ast::AstNodeKind::IdentifierExpression) continue;
            const auto local = lookupScopeName(scope, argument->text());
            if (!local.has_value() || local->kind != ScopeNameKind::Local || local->semanticOrdinal >= locals_.size()) continue;
            for (const std::size_t origin : objectParameterOrigins[local->semanticOrdinal])
                if (origin < objectPointerSinks[caller].size()) objectPointerSinks[caller][origin] = true;
        }
    });
    bool objectSinksChanged;
    do
    {
        objectSinksChanged = false;
        forEachResolvedCall([&](std::size_t caller, std::size_t scope, std::size_t callee,
            std::span<const std::size_t> ordinals, std::span<const ast::AstNode* const> args)
        {
            if (caller >= objectPointerSinks.size() || callee >= objectPointerSinks.size()) return;
            for (std::size_t i = 0; i < ordinals.size() && i < args.size(); ++i)
            {
                const std::size_t p = ordinals[i];
                if (p >= objectPointerSinks[callee].size() || !objectPointerSinks[callee][p] ||
                    args[i] == nullptr || args[i]->kind() != ast::AstNodeKind::IdentifierExpression) continue;
                const auto source = lookupScopeName(scope, args[i]->text());
                if (!source.has_value()) continue;
                const auto mark = [&](std::size_t parameter)
                {
                    if (parameter < objectPointerSinks[caller].size() && !objectPointerSinks[caller][parameter])
                    { objectPointerSinks[caller][parameter] = true; objectSinksChanged = true; }
                };
                if (source->kind == ScopeNameKind::Parameter) mark(source->semanticOrdinal);
                else if (source->semanticOrdinal < locals_.size())
                {
                    for (const std::size_t parameter : locals_[source->semanticOrdinal].possibleReferenceSourceParameters)
                        mark(parameter);
                    for (const std::size_t parameter : objectParameterOrigins[source->semanticOrdinal]) mark(parameter);
                }
            }
        });
    } while (objectSinksChanged);
    // A pointer borrowed from an object's field and the object reference itself
    // have different lifetimes. Prove the latter stays inside the source call
    // before treating a local owner as safe merely because its field is global.
    const auto isDirectPointerFieldReader = [&](const MethodCallBinding* call)
    {
        if (call == nullptr || call->resolution != MethodCallResolutionKind::Resolved ||
            !call->selectedMemberOrdinal.has_value() || call->staticCall || call->interfaceCall) return false;
        const std::size_t ordinal = *call->selectedMemberOrdinal;
        const ClassMemberSymbol* target = methodSymbol(ordinal);
        if (target == nullptr || target->isStatic || target->isPrecompiled || target->isVirtual ||
            target->isOverride || target->isAbstract || target->isUnsafe || target->isReferenceReturn ||
            !target->declaredTypeKey.ends_with("*") || !target->parameters.empty() ||
            target->syntax == nullptr || target->syntax->first() == nullptr || !callableBodyAnalyzed(ordinal))
            return false;
        const ast::AstNode* body = target->syntax->first();
        if (body->kind() != ast::AstNodeKind::BlockStatement || body->children().size() != 1U ||
            body->children().front() == nullptr ||
            body->children().front()->kind() != ast::AstNodeKind::ReturnStatement) return false;
        const ast::AstNode* value = body->children().front()->first();
        const MethodNameBinding* field = value == nullptr ? nullptr : nameBinding(value, ordinal);
        return field != nullptr && (field->kind == MethodNameBindingKind::ImplicitField ||
            field->kind == MethodNameBindingKind::ExplicitThisField) && field->typeKey.ends_with("*") &&
            field->candidateMemberOrdinals.size() == 1U && memberModel_ != nullptr &&
            field->candidateMemberOrdinals.front() < memberModel_->members().size() &&
            memberModel_->members()[field->candidateMemberOrdinals.front()].kind == ClassMemberKind::Field &&
            !memberModel_->members()[field->candidateMemberOrdinals.front()].isReferenceReturn;
    };
    std::vector<std::uint8_t> objectIdentityState(cellCount, 0U);
    std::function<bool(std::size_t, std::size_t)> objectIdentityMayEscape;
    objectIdentityMayEscape = [&](std::size_t callable, std::size_t parameter)
    {
        if (callable >= parameterEscapeSummaries_.size() || parameter >= parameterEscapeSummaries_[callable].size())
            return true;
        const std::size_t cell = callableCellOffsets[callable] + parameter;
        if (objectIdentityState[cell] != 0U) return objectIdentityState[cell] != 2U;
        objectIdentityState[cell] = 1U;
        const ClassMemberSymbol* method = methodSymbol(callable);
        if (method == nullptr || method->isPrecompiled || !method->isStatic || method->isUnsafe ||
            method->isReferenceReturn || method->syntax == nullptr || method->syntax->first() == nullptr ||
            method->genericTemplateOrdinal.has_value() || !method->genericTemplateKey.empty() ||
            !callableBodyAnalyzed(callable) || method->effects.contains(sema::Effect::Async) ||
            method->effects.contains(sema::Effect::Generator) || parameter >= method->parameters.size() ||
            method->parameters[parameter].passKind != ParameterPassKind::Value ||
            !isReferenceTypeKey(method->parameters[parameter].typeKey))
        { objectIdentityState[cell] = 3U; return true; }
        struct IdentityVisit final { const ast::AstNode* node; const ast::AstNode* parent; const ast::AstNode* grand; };
        std::vector<IdentityVisit> pending{{method->syntax->first(), nullptr, nullptr}};
        while (!pending.empty())
        {
            const IdentityVisit visit = pending.back(); pending.pop_back();
            const ast::AstNode* node = visit.node;
            if (node == nullptr) continue;
            if (node->kind() == ast::AstNodeKind::LambdaExpression ||
                node->kind() == ast::AstNodeKind::InlineAssemblyStatement || node->auxiliaryText() == "unsafe-block")
            { objectIdentityState[cell] = 3U; return true; }
            const MethodNameBinding* binding = nameBinding(node, callable);
            if (binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                binding->parameterOrdinal == parameter)
            {
                bool safe = false;
                if (visit.parent != nullptr && visit.parent->kind() == ast::AstNodeKind::MemberExpression &&
                    visit.parent->first() == node && visit.grand != nullptr &&
                    visit.grand->kind() == ast::AstNodeKind::CallExpression &&
                    visit.grand->first() == visit.parent)
                    safe = isDirectPointerFieldReader(callBinding(visit.grand, callable));
                if (visit.parent != nullptr && visit.parent->kind() == ast::AstNodeKind::MemberExpression &&
                    visit.parent->first() == node && visit.grand != nullptr && !visit.grand->isReference() &&
                    !(visit.grand->kind() == ast::AstNodeKind::UnaryExpression &&
                      visit.grand->tokenKind() == lexer::TokenKind::Amp))
                {
                    const MethodNameBinding* field = nameBinding(visit.parent, callable);
                    safe = safe || (field != nullptr && field->kind == MethodNameBindingKind::ExplicitObjectField &&
                        field->typeKey.ends_with("*") && field->candidateMemberOrdinals.size() == 1U &&
                        memberModel_ != nullptr && field->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                        memberModel_->members()[field->candidateMemberOrdinals.front()].kind == ClassMemberKind::Field &&
                        !memberModel_->members()[field->candidateMemberOrdinals.front()].isReferenceReturn);
                    if (safe && visit.grand->kind() == ast::AstNodeKind::CallExpression &&
                        visit.grand->first() != visit.parent)
                    {
                        const MethodCallBinding* fieldCall = callBinding(visit.grand, callable);
                        bool valueArgument = false;
                        if (fieldCall != nullptr && fieldCall->resolution == MethodCallResolutionKind::Resolved &&
                            fieldCall->selectedMemberOrdinal.has_value())
                        {
                            const ClassMemberSymbol* target = methodSymbol(*fieldCall->selectedMemberOrdinal);
                            for (std::size_t i = 0; target != nullptr && i < fieldCall->selectedArgumentSyntaxes.size() &&
                                 i < fieldCall->selectedArgumentParameterOrdinals.size(); ++i)
                                if (fieldCall->selectedArgumentSyntaxes[i] == visit.parent &&
                                    fieldCall->selectedArgumentParameterOrdinals[i] < target->parameters.size() &&
                                    target->parameters[fieldCall->selectedArgumentParameterOrdinals[i]].passKind == ParameterPassKind::Value)
                                    valueArgument = true;
                        }
                        safe = valueArgument;
                    }
                }
                if (!safe && visit.parent != nullptr && visit.parent->kind() == ast::AstNodeKind::CallExpression)
                {
                    const MethodCallBinding* call = callBinding(visit.parent, callable);
                    if (call != nullptr && call->resolution == MethodCallResolutionKind::Resolved &&
                        call->selectedMemberOrdinal.has_value())
                    {
                        const ClassMemberSymbol* target = methodSymbol(*call->selectedMemberOrdinal);
                        for (std::size_t i = 0; i < call->selectedArgumentSyntaxes.size() &&
                             i < call->selectedArgumentParameterOrdinals.size(); ++i)
                            if (call->selectedArgumentSyntaxes[i] == node && target != nullptr && target->isStatic &&
                                !target->isVirtual && !target->isOverride && !target->isAbstract &&
                                call->selectedArgumentParameterOrdinals[i] < target->parameters.size() &&
                                target->parameters[call->selectedArgumentParameterOrdinals[i]].passKind == ParameterPassKind::Value &&
                                !objectIdentityMayEscape(*call->selectedMemberOrdinal,
                                    call->selectedArgumentParameterOrdinals[i])) safe = true;
                    }
                }
                if (!safe) { objectIdentityState[cell] = 3U; return true; }
            }
            pending.push_back({node->first(), node, visit.parent});
            pending.push_back({node->second(), node, visit.parent});
            pending.push_back({node->third(), node, visit.parent});
            for (const ast::AstNode* child : node->children()) pending.push_back({child, node, visit.parent});
        }
        objectIdentityState[cell] = 2U;
        return false;
    };
    // A source wrapper can call an interface on a fresh, exact binary type.
    // Use the producer's existing receiver certificate for every dispatched
    // implementation; an open interface or missing certificate remains unknown.
    const auto exactBinaryReceiverDoesNotEscape = [&](std::size_t callable, std::size_t parameter,
        std::size_t localOrdinal, const ast::AstNode* argument)
    {
        if (localOrdinal >= locals_.size() || memberModel_ == nullptr || interfaceModel_ == nullptr ||
            argument == nullptr) return false;
        const MethodLocalSymbol& local = locals_[localOrdinal];
        const ClassMemberSymbol* wrapper = methodSymbol(callable);
        const bool identitySafeLifecycle = memberModel_->isIdentitySafeLifecycleTypeKey(local.typeKey);
        const bool strongLifecycle = memberModel_->isNonEscapingLifecycleTypeKey(local.typeKey);
        if (!local.isRaii || local.isReference || local.syntax == nullptr || local.syntax->first() == nullptr ||
            local.syntax->first()->kind() != ast::AstNodeKind::ObjectCreationExpression ||
            (!local.syntax->first()->children().empty() && !identitySafeLifecycle) ||
            !local.possibleReferenceSourceLocals.empty() ||
            !local.possibleReferenceSourceParameters.empty() || local.mayAliasUnknownReference ||
            (!strongLifecycle && !identitySafeLifecycle) || wrapper == nullptr ||
            wrapper->isPrecompiled || !wrapper->isStatic || wrapper->isUnsafe || wrapper->isReferenceReturn ||
            wrapper->genericTemplateOrdinal.has_value() || !wrapper->genericTemplateKey.empty() ||
            wrapper->isUserOperator || wrapper->isUserConversion ||
            wrapper->syntax == nullptr || wrapper->syntax->first() == nullptr || !callableBodyAnalyzed(callable) ||
            wrapper->effects.contains(sema::Effect::Async) || wrapper->effects.contains(sema::Effect::Generator) ||
            parameter >= wrapper->parameters.size() || wrapper->parameters[parameter].passKind != ParameterPassKind::Value)
            return false;
        const auto exact = namedTypeOrdinalForTypeKey(local.typeKey);
        if (!exact.has_value()) return false;
        std::size_t exactConstructions = 0;
        for (const ProjectConstructionSite& site : constructionSites_)
            if (site.syntax == local.syntax->first() && site.callableOrdinal == local.methodMemberOrdinal)
            {
                if (site.kind != ProjectConstructionSiteKind::Object || site.targetTypeOrdinal != exact ||
                    (!site.argumentTypes.empty() && !identitySafeLifecycle)) return false;
                ++exactConstructions;
            }
        if (exactConstructions != 1U) return false;
        struct Visit { const ast::AstNode* node; const ast::AstNode* parent; const ast::AstNode* grand; };
        const ClassMemberSymbol* caller = methodSymbol(local.methodMemberOrdinal);
        if (caller == nullptr || caller->syntax == nullptr || caller->syntax->first() == nullptr || caller->isUnsafe ||
            caller->effects.contains(sema::Effect::Async) || caller->effects.contains(sema::Effect::Generator)) return false;
        std::unordered_map<const ast::AstNode*, Visit> callerNodes;
        std::vector<Visit> callerPending{{caller->syntax->first(), nullptr, nullptr}};
        while (!callerPending.empty())
        {
            const Visit visit = callerPending.back(); callerPending.pop_back();
            if (visit.node == nullptr) continue;
            const auto [found, inserted] = callerNodes.emplace(visit.node, visit);
            if (!inserted)
            {
                if (found->second.parent != visit.parent) return false;
                continue;
            }
            if (visit.node->kind() == ast::AstNodeKind::LambdaExpression ||
                visit.node->kind() == ast::AstNodeKind::InlineAssemblyStatement ||
                visit.node->auxiliaryText() == "unsafe-block") return false;
            callerPending.push_back({visit.node->first(), visit.node, visit.parent});
            callerPending.push_back({visit.node->second(), visit.node, visit.parent});
            callerPending.push_back({visit.node->third(), visit.node, visit.parent});
            for (const ast::AstNode* child : visit.node->children())
                callerPending.push_back({child, visit.node, visit.parent});
        }
        bool currentUseSeen = false;
        for (const MethodNameBinding& binding : nameBindings_)
            if (binding.kind == MethodNameBindingKind::Local && binding.localOrdinal == localOrdinal &&
                binding.methodMemberOrdinal == local.methodMemberOrdinal)
            {
                if (binding.syntax == argument) { currentUseSeen = true; continue; }
                const auto found = callerNodes.find(binding.syntax);
                if (found == callerNodes.end()) return false;
                const Visit& use = found->second;
                bool safeUse = false;
                if (use.parent != nullptr && use.parent->kind() == ast::AstNodeKind::CallExpression)
                {
                    const MethodCallBinding* call = callBinding(use.parent, local.methodMemberOrdinal);
                    if (call != nullptr && call->resolution == MethodCallResolutionKind::Resolved &&
                        call->selectedMemberOrdinal == callable)
                        for (std::size_t i = 0; i < call->selectedArgumentSyntaxes.size() &&
                             i < call->selectedArgumentParameterOrdinals.size(); ++i)
                            if (call->selectedArgumentSyntaxes[i] == binding.syntax &&
                                call->selectedArgumentParameterOrdinals[i] == parameter) safeUse = true;
                }
                if (!safeUse && use.parent != nullptr && use.parent->kind() == ast::AstNodeKind::MemberExpression &&
                    use.parent->first() == binding.syntax && use.grand != nullptr &&
                    use.grand->kind() == ast::AstNodeKind::CallExpression && use.grand->first() == use.parent)
                {
                    const MethodCallBinding* call = callBinding(use.grand, local.methodMemberOrdinal);
                    const ClassMemberSymbol* target = call != nullptr &&
                        call->resolution == MethodCallResolutionKind::Resolved && call->selectedMemberOrdinal.has_value()
                        ? methodSymbol(*call->selectedMemberOrdinal) : nullptr;
                    safeUse = call != nullptr && !call->interfaceCall && !call->staticCall && target != nullptr &&
                        target->isPrecompiled && !target->isVirtual && !target->isOverride && !target->isAbstract &&
                        !target->isUnsafe && !target->isReferenceReturn && target->receiverNoReferenceFieldEffects;
                }
                if (!safeUse) return false;
            }
        if (!currentUseSeen) return false;
        std::vector<Visit> pending{{wrapper->syntax->first(), nullptr, nullptr}};
        std::unordered_set<const ast::AstNode*> seen;
        bool observed = false;
        while (!pending.empty())
        {
            const Visit visit = pending.back(); pending.pop_back();
            if (visit.node == nullptr || !seen.insert(visit.node).second) continue;
            if (visit.node->kind() == ast::AstNodeKind::LambdaExpression ||
                visit.node->kind() == ast::AstNodeKind::InlineAssemblyStatement ||
                visit.node->auxiliaryText() == "unsafe-block") return false;
            const MethodNameBinding* binding = nameBinding(visit.node, callable);
            if (binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                binding->parameterOrdinal == parameter)
            {
                if (visit.parent == nullptr || visit.parent->kind() != ast::AstNodeKind::MemberExpression ||
                    visit.parent->first() != visit.node || visit.grand == nullptr ||
                    visit.grand->kind() != ast::AstNodeKind::CallExpression || visit.grand->first() != visit.parent)
                    return false;
                const MethodCallBinding* call = callBinding(visit.grand, callable);
                if (call == nullptr || !call->interfaceCall || call->resolution != MethodCallResolutionKind::Resolved ||
                    !call->selectedMemberOrdinal.has_value()) return false;
                std::optional<std::size_t> implementation;
                for (const ClassInterfaceTablePlan& table : interfaceModel_->tablePlans())
                {
                    if (table.classTypeOrdinal != *exact) continue;
                    for (std::size_t i = 0; i < table.interfaceMethodMemberOrdinals.size() &&
                         i < table.implementationMemberOrdinals.size(); ++i)
                        if (table.interfaceMethodMemberOrdinals[i] == *call->selectedMemberOrdinal)
                        {
                            if (implementation.has_value() || !table.implementationMemberOrdinals[i].has_value()) return false;
                            implementation = *table.implementationMemberOrdinals[i];
                        }
                }
                const ClassMemberSymbol* target = implementation.has_value() ? methodSymbol(*implementation) : nullptr;
                if (target == nullptr || !target->isPrecompiled || target->isAbstract || target->isUnsafe ||
                    target->isReferenceReturn || !target->receiverNoReferenceFieldEffects) return false;
                observed = true;
            }
            pending.push_back({visit.node->first(), visit.node, visit.parent});
            pending.push_back({visit.node->second(), visit.node, visit.parent});
            pending.push_back({visit.node->third(), visit.node, visit.parent});
            for (const ast::AstNode* child : visit.node->children()) pending.push_back({child, visit.node, visit.parent});
        }
        return observed;
    };
    for (std::size_t callable = 0; callable < namespaceCallableBase_ && callable < objectPointerSinks.size(); ++callable)
    {
        const ClassMemberSymbol* method = methodSymbol(callable);
        if (method == nullptr || method->isPrecompiled) continue;
        for (std::size_t p = 0; p < objectPointerSinks[callable].size(); ++p)
            if (objectPointerSinks[callable][p] &&
                parameterEscapeSummaries_[callable][p] == language::EscapeClass::NoEscape)
                parameterEscapeSummaries_[callable][p] = objectIdentityMayEscape(callable, p)
                    ? language::EscapeClass::Unknown : language::EscapeClass::ObjectPointerGlobal;
    }
    for (std::size_t callable = namespaceCallableBase_; callable < objectPointerSinks.size(); ++callable)
    {
        const ClassMemberSymbol* function = methodSymbol(callable);
        if (function == nullptr || function->isPrecompiled) continue;
        for (std::size_t p = 0; p < objectPointerSinks[callable].size(); ++p)
            if (objectPointerSinks[callable][p] && parameterEscapeSummaries_[callable][p] == language::EscapeClass::NoEscape)
                parameterEscapeSummaries_[callable][p] = language::EscapeClass::Unknown;
    }
    const auto singlePointerObjectHoldsStaticAddress = [&](std::size_t localOrdinal,
        const ast::AstNode* argument, std::size_t callee)
    {
        if (localOrdinal >= locals_.size() || argument == nullptr || memberModel_ == nullptr ||
            inheritanceModel_ == nullptr) return false;
        const MethodLocalSymbol& local = locals_[localOrdinal];
        const ClassMemberSymbol* caller = methodSymbol(local.methodMemberOrdinal);
        const auto type = namedTypeOrdinalForTypeKey(local.typeKey);
        if (!local.isRaii || local.isReference || local.syntax == nullptr || local.syntax->first() == nullptr ||
            local.syntax->first()->kind() != ast::AstNodeKind::ObjectCreationExpression ||
            !local.syntax->first()->children().empty() || !local.possibleReferenceSourceLocals.empty() ||
            !local.possibleReferenceSourceParameters.empty() || local.mayAliasUnknownReference ||
            !memberModel_->isNonEscapingLifecycleTypeKey(local.typeKey) || !type.has_value() ||
            inheritanceModel_->baseTypeOrdinal(*type).has_value() || caller == nullptr ||
            caller->syntax == nullptr || caller->syntax->first() == nullptr ||
            caller->syntax->first()->kind() != ast::AstNodeKind::BlockStatement ||
            caller->effects.contains(sema::Effect::Async) || caller->effects.contains(sema::Effect::Generator))
            return false;
        std::size_t matchingConstructions = 0;
        for (const auto& site : constructionSites_)
            if (site.syntax == local.syntax->first() && site.callableOrdinal == local.methodMemberOrdinal)
            {
                if (site.kind != ProjectConstructionSiteKind::Object || site.targetTypeOrdinal != type ||
                    !site.argumentTypes.empty()) return false;
                ++matchingConstructions;
            }
        if (matchingConstructions != 1U) return false;
        std::optional<std::size_t> fieldOrdinal;
        for (const std::size_t ordinal : memberModel_->membersOfType(*type))
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const auto& field = memberModel_->members()[ordinal];
            if (field.kind != ClassMemberKind::Field || field.isStatic) continue;
            if (fieldOrdinal.has_value() || !field.declaredTypeKey.ends_with("*") || field.isVolatile ||
                field.isReferenceReturn || field.accessibility != ClassMemberAccessibility::Public) return false;
            fieldOrdinal = ordinal;
        }
        if (!fieldOrdinal.has_value()) return false;
        const ast::AstNode* fieldBase = nullptr;
        bool declared = false, assigned = false, called = false;
        for (const ast::AstNode* statement : caller->syntax->first()->children())
        {
            if (statement == local.syntax) { if (declared) return false; declared = true; continue; }
            if (statement == nullptr || statement->kind() != ast::AstNodeKind::ExpressionStatement ||
                statement->first() == nullptr) continue;
            const ast::AstNode* expression = statement->first();
            if (expression->kind() == ast::AstNodeKind::AssignmentExpression &&
                expression->tokenKind() == lexer::TokenKind::Equal && expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::MemberExpression && expression->second() != nullptr)
            {
                const auto* field = nameBinding(expression->first(), local.methodMemberOrdinal);
                const auto* base = nameBinding(expression->first()->first(), local.methodMemberOrdinal);
                if (field == nullptr || base == nullptr || base->kind != MethodNameBindingKind::Local ||
                    base->localOrdinal != localOrdinal) continue;
                if (!declared || assigned || called || field->kind != MethodNameBindingKind::ExplicitObjectField ||
                    field->candidateMemberOrdinals.size() != 1U || field->candidateMemberOrdinals.front() != *fieldOrdinal)
                    return false;
                const ast::AstNode* value = expression->second();
                if (value->kind() != ast::AstNodeKind::UnaryExpression || value->tokenKind() != lexer::TokenKind::Amp ||
                    value->first() == nullptr) return false;
                const MethodNameBinding* global = nameBinding(value->first(), local.methodMemberOrdinal);
                if (global == nullptr || global->kind != MethodNameBindingKind::StaticField ||
                    global->candidateMemberOrdinals.size() != 1U ||
                    global->candidateMemberOrdinals.front() >= memberModel_->members().size() ||
                    !memberModel_->members()[global->candidateMemberOrdinals.front()].isStatic) return false;
                fieldBase = expression->first()->first();
                assigned = true;
            }
            else if (expression->kind() == ast::AstNodeKind::CallExpression)
            {
                const MethodCallBinding* call = callBinding(expression, local.methodMemberOrdinal);
                if (call == nullptr || call->selectedMemberOrdinal != callee ||
                    std::ranges::find(call->selectedArgumentSyntaxes, argument) == call->selectedArgumentSyntaxes.end())
                    continue;
                if (!declared || !assigned || called) return false;
                called = true;
            }
        }
        if (!declared || !assigned || !called || fieldBase == nullptr) return false;
        std::size_t uses = 0;
        for (const MethodNameBinding& binding : nameBindings_)
            if (binding.kind == MethodNameBindingKind::Local && binding.localOrdinal == localOrdinal &&
                binding.methodMemberOrdinal == local.methodMemberOrdinal)
            {
                if (binding.syntax != fieldBase && binding.syntax != argument) return false;
                ++uses;
            }
        return uses == 2U;
    };
    forEachResolvedCall([&](std::size_t, std::size_t scope, std::size_t callee,
        std::span<const std::size_t> ordinals, std::span<const ast::AstNode* const> args)
    {
        if (callee >= objectPointerSinks.size()) return;
        for (std::size_t i = 0; i < ordinals.size() && i < args.size(); ++i)
        {
            const std::size_t p = ordinals[i];
            if (p >= objectPointerSinks[callee].size() || !objectPointerSinks[callee][p] || args[i] == nullptr) continue;
            const ClassMemberSymbol* method = methodSymbol(callee);
            const bool binaryObjectPointer = method != nullptr && method->isPrecompiled &&
                p < parameterEscapeSummaries_[callee].size() &&
                parameterEscapeSummaries_[callee][p] == language::EscapeClass::ObjectPointerGlobal;
            const auto source = args[i]->kind() == ast::AstNodeKind::IdentifierExpression
                ? lookupScopeName(scope, args[i]->text()) : std::nullopt;
            if (binaryObjectPointer && !source.has_value() && args[i]->kind() != ast::AstNodeKind::NullLiteralExpression)
            {
                diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error, args[i]->range(),
                    "argument contains a pointer whose origin is unknown to the binary escape contract");
                continue;
            }
            if (!source.has_value() || source->kind != ScopeNameKind::Local ||
                source->semanticOrdinal >= objectPointerOrigins.size()) continue;
            const auto kind = objectPointerOrigins[source->semanticOrdinal].kind;
            const bool pointerFrame = kind == MethodEscapeOriginKind::CurrentFrameLocal ||
                kind == MethodEscapeOriginKind::VariableLengthArray;
            const bool unknownBinaryPointer = binaryObjectPointer && !pointerFrame &&
                !singlePointerObjectHoldsStaticAddress(source->semanticOrdinal, args[i], callee);
            const bool scopedObjectEscape = locals_[source->semanticOrdinal].isRaii &&
                !binaryObjectPointer && objectIdentityMayEscape(callee, p) &&
                !exactBinaryReceiverDoesNotEscape(callee, p, source->semanticOrdinal, args[i]);
            if (pointerFrame || scopedObjectEscape || unknownBinaryPointer)
                diagnostics.report(diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error, args[i]->range(),
                    pointerFrame ? "argument contains a pointer to current-frame storage that reaches a retained escape contract" :
                        unknownBinaryPointer ? "argument contains a pointer whose origin is unknown to the binary escape contract" :
                        "argument is a scoped object that reaches an unproven escape contract");
        }
    });
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
        // EN: A read of one element of a simple local native array through a literal or stable local
        //     index. Reading cannot write a slot or publish the array; a later write or call that
        //     receives the value is still modeled (or made unknown) by the replay itself.
        // ES: Lectura de un elemento de un arreglo nativo local simple con indice literal o local
        //     estable. Leer no escribe una ranura ni publica el arreglo; una escritura o llamada
        //     posterior que reciba el valor sigue modelada (o vuelta desconocida) por el replay.
        const auto simpleIndexRead = [&](const ast::AstNode* expression)
        {
            if (expression == nullptr || expression->kind() != ast::AstNodeKind::IndexExpression ||
                !simpleValue(expression->first())) return false;
            const MethodNameBinding* array = nameBinding(expression->first(), ordinal);
            if (array == nullptr || array->kind != MethodNameBindingKind::Local ||
                !ProjectTypeConversion::isNativeArray(array->typeKey)) return false;
            const ast::AstNode* index = expression->children().size() == 1
                ? expression->children().front()
                : (expression->children().empty() ? expression->second() : nullptr);
            const std::string_view type = recordedExpressionTypeKey(expression, ordinal);
            return index != nullptr && isReferenceTypeKey(std::string(type)) &&
                !ProjectTypeConversion::isNativeArray(type) &&
                (index->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
                 arrayReferenceStableLocalIndex(index, ordinal).has_value());
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
                if (simpleValue(argument) || simpleIndexRead(argument)) continue;
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
                arrayReferenceStableLocalIndex(index, ordinal).has_value() ||
                isUnconstrainedArrayIndexParameter(*this, index, ordinal));
        };
        // EN: A plain nested block is straight-line code; its statements are checked in order as if
        //     written inline. Blocks with a marker (declaration lists, unsafe, defer, match) are not
        //     flattened and keep the proof false. Scope cleanup is modeled by the replay itself.
        // ES: Un bloque anidado simple es codigo lineal; sus sentencias se revisan en orden como si
        //     estuvieran en linea. Los bloques con marca (listas de declaracion, unsafe, defer, match)
        //     no se aplanan y dejan la prueba en falso. La limpieza de ambito la modela el replay.
        std::vector<const ast::AstNode*> linear;
        std::vector<const ast::AstNode*> flatten(body->children().rbegin(), body->children().rend());
        while (!flatten.empty())
        {
            const ast::AstNode* item = flatten.back();
            flatten.pop_back();
            if (item != nullptr && item->kind() == ast::AstNodeKind::BlockStatement && item->auxiliaryText().empty())
            {
                const auto children = item->children();
                for (auto child = children.rbegin(); child != children.rend(); ++child) flatten.push_back(*child);
                continue;
            }
            linear.push_back(item);
        }
        bool covered = true;
        bool sawIf = false;
        for (std::size_t i = 0; i < linear.size() && covered; ++i)
        {
            const ast::AstNode* statement = linear[i];
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
                    if (!covered)
                        covered = initial->kind() == ast::AstNodeKind::NullLiteralExpression ||
                            (simpleIndexRead(initial) && !statement->isReference() &&
                             !statement->hasModifier(lexer::TokenKind::KwStatic) &&
                             !statement->hasModifier(lexer::TokenKind::KwRaii));
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
                if (expression->kind() == ast::AstNodeKind::AssignmentExpression &&
                    expression->tokenKind() == lexer::TokenKind::Equal && simpleIndexRead(expression->second()))
                {
                    const MethodNameBinding* target = expression->first() != nullptr &&
                            expression->first()->kind() == ast::AstNodeKind::IdentifierExpression
                        ? nameBinding(expression->first(), ordinal) : nullptr;
                    covered = target != nullptr && target->kind == MethodNameBindingKind::Local &&
                        target->localOrdinal.has_value() && *target->localOrdinal < locals_.size() &&
                        !locals_[*target->localOrdinal].isReference && !locals_[*target->localOrdinal].isStatic &&
                        !locals_[*target->localOrdinal].isRaii && isReferenceTypeKey(target->typeKey) &&
                        !ProjectTypeConversion::isNativeArray(target->typeKey);
                    continue;
                }
                if (expression->kind() == ast::AstNodeKind::DeleteExpression && expression->flag())
                {
                    covered = simpleValue(expression->first()) && i + 1 < linear.size();
                    for (std::size_t tail = i + 1; tail < linear.size() && covered; ++tail)
                    {
                        const ast::AstNode* next = linear[tail];
                        if (next != nullptr && next->kind() == ast::AstNodeKind::ReturnStatement)
                            covered = tail + 1 == linear.size() && simpleScalarReturn(next->first());
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
                covered = i + 1 == linear.size() && simpleScalarReturn(statement->first()) &&
                    (ProjectTypeConversion::isIntegral(callable->declaredTypeKey) ||
                     callable->declaredTypeKey == "builtin:bool");
                if (!covered)
                    covered = i + 1 == linear.size() && !statement->isReference() &&
                        !callable->isReferenceReturn && simpleIndexRead(statement->first()) &&
                        isReferenceTypeKey(callable->declaredTypeKey) &&
                        !ProjectTypeConversion::isNativeArray(callable->declaredTypeKey);
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
            using PointerExpression = std::pair<const ast::AstNode*, bool>;
            std::vector<PointerExpression> expressions{{argumentSyntaxes[argumentOrdinal], false}};
            std::unordered_set<std::size_t> visited;
            std::unordered_set<const ast::AstNode*> seenExpressions;
            std::unordered_set<const ast::AstNode*> seenAddressExpressions;
            while (!pending.empty() || !expressions.empty())
            {
                if (!expressions.empty())
                {
                    const auto [expression, insideAddress] = expressions.back();
                    expressions.pop_back();
                    if (expression == nullptr ||
                        !(insideAddress ? seenAddressExpressions : seenExpressions).insert(expression).second) continue;
                    const MethodNameBinding* binding = nameBinding(expression, callerOrdinal);
                    const bool address = insideAddress ||
                        (expression->kind() == ast::AstNodeKind::UnaryExpression &&
                         expression->tokenKind() == lexer::TokenKind::Amp &&
                         !expression->auxiliaryText().starts_with("pass:"));
                    const auto pointerParameter = [&](std::size_t ordinal)
                    {
                        const auto& parameter = caller->parameters[ordinal];
                        return parameter.passKind == ParameterPassKind::Value
                            ? parameter.typeKey.ends_with("*")
                            : parameter.pointeeTypeKey.ends_with("*") || address;
                    };
                    if (binding != nullptr && binding->kind == MethodNameBindingKind::Parameter &&
                        binding->parameterOrdinal.has_value() && *binding->parameterOrdinal < caller->parameters.size() &&
                        pointerParameter(*binding->parameterOrdinal))
                        dependents[callableCellOffsets[calleeOrdinal] + calleeParameter].push_back(
                            callableCellOffsets[callerOrdinal] + *binding->parameterOrdinal);
                    else if (binding != nullptr && binding->kind == MethodNameBindingKind::Local &&
                        binding->localOrdinal.has_value() && *binding->localOrdinal < locals_.size() &&
                        locals_[*binding->localOrdinal].typeKey.ends_with("*"))
                        pending.push_back(*binding->localOrdinal);
                    if (expression->first() != nullptr) expressions.emplace_back(expression->first(), address);
                    if (expression->second() != nullptr) expressions.emplace_back(expression->second(), address);
                    if (expression->third() != nullptr) expressions.emplace_back(expression->third(), address);
                    for (const ast::AstNode* child : expression->children())
                        if (child != nullptr) expressions.emplace_back(child, address);
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
                    expressions.emplace_back(local.syntax->first(), false);
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
        const language::EscapeClass publishedClass =
            parameterEscapeSummaries_[sourceCallable][sourceParameter];
        // Slot non-escape describes the callee's own formal slot only; it is
        // not evidence that a wrapper does not forward its caller's slot.
        const language::EscapeClass sourceClass = (publishedClass == language::EscapeClass::RefSlotNoEscape ||
            publishedClass == language::EscapeClass::ObjectPointerGlobal)
            ? language::EscapeClass::Unknown : publishedClass;
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
        // A proven slot-only contract is safe for a local whose value is still
        // definitely null. This intentionally handles only a first use in a
        // loop-free body; later or unproven writes remain unknown.
        if (contract == language::EscapeClass::RefSlotNoEscape &&
            calleeSymbol != nullptr && targetParameter < calleeSymbol->parameters.size() &&
            calleeSymbol->parameters[targetParameter].passKind == ParameterPassKind::Ref &&
            calleeSymbol->parameters[targetParameter].pointeeTypeKey.ends_with("*") &&
            scopeOrdinal < scopes_.size())
        {
            const ast::AstNode* argument = argumentSyntaxes[argumentOrdinal];
            const ast::AstNode* nameNode = argument != nullptr &&
                argument->kind() == ast::AstNodeKind::UnaryExpression &&
                argument->auxiliaryText() == "pass:ref" ? argument->first() : nullptr;
            const std::size_t caller = scopes_[scopeOrdinal].methodMemberOrdinal;
            const ClassMemberSymbol* callerSymbol = methodSymbol(caller);
            const auto name = nameNode != nullptr && nameNode->kind() == ast::AstNodeKind::IdentifierExpression
                ? lookupScopeName(scopeOrdinal, nameNode->text()) : std::nullopt;
            bool nullBeforeCall = name.has_value() && name->kind == ScopeNameKind::Local &&
                name->semanticOrdinal < locals_.size() && callerSymbol != nullptr &&
                callerSymbol->syntax != nullptr && callerSymbol->syntax->first() != nullptr &&
                argument->range().isValid() && !callerSymbol->effects.contains(sema::Effect::Async) &&
                !callerSymbol->effects.contains(sema::Effect::Generator);
            if (nullBeforeCall)
            {
                const MethodLocalSymbol& local = locals_[name->semanticOrdinal];
                nullBeforeCall = !local.isReference && !local.isStatic && local.syntax != nullptr &&
                    local.syntax->first() != nullptr &&
                    local.syntax->first()->kind() == ast::AstNodeKind::NullLiteralExpression &&
                    local.syntax->range().isValid() &&
                    local.syntax->range().begin().fileId() == argument->range().begin().fileId() &&
                    local.syntax->range().begin().offset() < argument->range().begin().offset();
            }
            if (nullBeforeCall)
            {
                std::vector<const ast::AstNode*> pending{callerSymbol->syntax->first()};
                std::unordered_set<const ast::AstNode*> seen;
                while (!pending.empty() && nullBeforeCall)
                {
                    const ast::AstNode* node = pending.back(); pending.pop_back();
                    if (node == nullptr || !seen.insert(node).second) continue;
                    switch (node->kind())
                    {
                        case ast::AstNodeKind::WhileStatement:
                        case ast::AstNodeKind::DoWhileStatement:
                        case ast::AstNodeKind::ForStatement:
                        case ast::AstNodeKind::ForeachStatement:
                        case ast::AstNodeKind::LoopExpression:
                        case ast::AstNodeKind::LambdaExpression:
                        case ast::AstNodeKind::FunctionDeclaration:
                        case ast::AstNodeKind::MethodDeclaration:
                        case ast::AstNodeKind::InlineAssemblyStatement:
                            nullBeforeCall = false; continue;
                        default: break;
                    }
                    if (node->kind() == ast::AstNodeKind::IdentifierExpression)
                    {
                        const MethodNameBinding* binding = nameBinding(node, caller);
                        if (binding == nullptr && node != nameNode && node->text() == nameNode->text())
                        { nullBeforeCall = false; continue; }
                        if (binding != nullptr && binding->kind == MethodNameBindingKind::Local &&
                            binding->localOrdinal == name->semanticOrdinal)
                        {
                            if (!node->range().isValid() ||
                                node->range().begin().fileId() != argument->range().begin().fileId())
                            { nullBeforeCall = false; continue; }
                            const std::uint32_t offset = node->range().begin().offset();
                            const auto argRange = argument->range();
                            if (offset < argRange.begin().offset() ||
                                (offset >= argRange.end().offset() &&
                                 std::ranges::any_of(argumentSyntaxes, [&](const ast::AstNode* other)
                                 {
                                     return other != nullptr && other != argument && other->range().isValid() &&
                                         other->range().begin().fileId() == node->range().begin().fileId() &&
                                         offset >= other->range().begin().offset() &&
                                         offset < other->range().end().offset();
                                 }))) nullBeforeCall = false;
                        }
                    }
                    pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                    for (const ast::AstNode* child : node->children()) pending.push_back(child);
                }
            }
            if (nullBeforeCall) continue;
        }
        const bool forwardedNativeUnknown = calleeOrdinal < parameterUnknownNativeEscape_.size() &&
            targetParameter < parameterUnknownNativeEscape_[calleeOrdinal].size() &&
            parameterUnknownNativeEscape_[calleeOrdinal][targetParameter];
        // RefSlotNoEscape proves only that the ref slot itself is not retained.
        // The pointer held in the slot remains unknown for both imported and
        // locally analyzed bodies, including transported inline bodies.
        const bool unknownNativeContract = contract == language::EscapeClass::RefSlotNoEscape ||
            (contract == language::EscapeClass::Unknown && (precompiledNative || forwardedNativeUnknown));
        if (contract != language::EscapeClass::Global && !unknownNativeContract) continue;
        const bool pointerValue = calleeSymbol != nullptr &&
            targetParameter < calleeSymbol->parameters.size() &&
            calleeSymbol->parameters[targetParameter].passKind == ParameterPassKind::Value &&
            calleeSymbol->parameters[targetParameter].typeKey.ends_with("*");
        // EN: A copy union or plain struct passed by value carries no frame address; the check
        //     rejected `Api.Read(value)` across a NativeSharp DLL (nsc.language.t03.c-unions, LKF-295).
        // ES: Una unión copy o un struct plano por valor no lleva direcciones del marco; la regla
        //     rechazaba `Api.Read(value)` a través de una DLL NativeSharp (nsc.language.t03.c-unions, LKF-295).
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
