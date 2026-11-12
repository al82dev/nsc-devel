#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
std::vector<ProjectGenericModel::TemplateState*>
ProjectGenericModel::resolveGenericInferenceCandidates(
    const ProjectSyntaxUnit& unit,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    const ast::AstNode* callee,
    const std::string& methodName)
{
    if (callee->kind() == ast::AstNodeKind::IdentifierExpression ||
        (callee->kind() == ast::AstNodeKind::MemberExpression && callee->first() != nullptr &&
         callee->first()->kind() == ast::AstNodeKind::ThisExpression))
    {
        const std::size_t dot = ownerName.find_last_of(".+");
        const std::string_view closedName = dot == std::string::npos
            ? std::string_view(ownerName) : std::string_view(ownerName).substr(dot + 1U);
        const ast::AstNode* ownerTemplate = nullptr;
        for (const GenericSpecializationRecord& specialization : specializations_)
        {
            if (specialization.specializedName != closedName ||
                specialization.templateOrdinal >= templateStates_.size()) continue;
            const TemplateState& owner = templateStates_[specialization.templateOrdinal];
            if (owner.record.projectOrdinal != unit.projectOrdinal() ||
                owner.record.namespaceName != namespaceName ||
                (owner.record.kind != GenericTemplateKind::Class &&
                 owner.record.kind != GenericTemplateKind::Struct)) continue;
            if (ownerTemplate != nullptr && ownerTemplate != specialization.templateSyntax) return {};
            ownerTemplate = specialization.templateSyntax;
        }
        if (ownerTemplate != nullptr)
        {
            std::vector<TemplateState*> candidates;
            if (const auto bucket = methodsByName_.find(methodName); bucket != methodsByName_.end())
                for (TemplateState* candidate : bucket->second)
                    if (candidate != nullptr && candidate->parent == ownerTemplate &&
                        templateVisible(unit.projectOrdinal(), candidate->record))
                        candidates.push_back(candidate);
            return candidates;
        }
    }
    if (callee->kind() == ast::AstNodeKind::MemberExpression && callee->first() != nullptr)
    {
        const auto bucket = methodsByName_.find(methodName);
        const bool hasGenericOwner = bucket != methodsByName_.end() &&
            std::ranges::any_of(bucket->second, [](const TemplateState* candidate) {
                return candidate != nullptr && candidate->parent != nullptr &&
                    !candidate->parent->genericParameters().empty();
            });
        const ast::TypeSyntax* actual = nullptr;
        if (hasGenericOwner && callee->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            actual = scope.findValueType(callee->first()->text());
            if (actual == nullptr) actual = callee->first()->typeSyntax();
        }
        else if (hasGenericOwner && callee->first()->kind() == ast::AstNodeKind::CallExpression)
        {
            const auto inferred = inferredCallReturnTypes_.find(callee->first());
            if (inferred != inferredCallReturnTypes_.end()) actual = inferred->second;
            else if (const auto ordinary = ordinaryCallableReturnTypes_.find(methodCalleeName(callee->first()));
                     ordinary != ordinaryCallableReturnTypes_.end()) actual = ordinary->second;
        }
        const ast::AstNode* receiverDeclaration = nullptr;
        const DeclaredTypeIdentity* ordinaryReceiver = nullptr;
        if (actual != nullptr && !actual->typeArguments().empty())
        {
            TypeResolution owner = resolveTypeTemplate(
                unit, namespaceName, actual->baseName(), actual->typeArguments().size(), scope);
            if (owner.kind == TypeResolutionKind::Unique && owner.state != nullptr)
                receiverDeclaration = owner.state->syntax;
        }
        else if (actual != nullptr)
        {
            ordinaryReceiver = resolveDeclaredTypeIdentity(actual, unit.projectOrdinal(), namespaceName, scope);
        }
        if (receiverDeclaration != nullptr || ordinaryReceiver != nullptr)
        {
            std::vector<TemplateState*> candidates;
            for (TemplateState* candidate : bucket->second)
            {
                const bool sameOwner = receiverDeclaration != nullptr
                    ? candidate->parent == receiverDeclaration
                    : candidate->parent != nullptr && candidate->parent->genericParameters().empty() &&
                        candidate->record.projectOrdinal == ordinaryReceiver->projectOrdinal &&
                        templateSourceSpelling(candidate->record.namespaceName, candidate->record.ownerName) ==
                            ordinaryReceiver->sourceName;
                if (sameOwner && templateVisible(unit.projectOrdinal(), candidate->record))
                    candidates.push_back(candidate);
            }
            return candidates;
        }
    }
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }
    QualifiedNameSiteContext siteContext = makeQualifiedNameSite(namespaceName, ownerName, scope);
    const QualifiedNameSite& site = siteContext.site;
    constexpr std::size_t kAny = std::numeric_limits<std::size_t>::max();
    std::vector<TemplateState*> named = templatesNamedBy(
        unit, site, requestedOwner, methodName, kAny, kAny);
    // NS304-T08-F06: an extension member access has a VALUE receiver, not an owner. When the
    // receiver spelling finds no template, the extension is a namespace-level declaration, so
    // retry with the lexical namespace as the owner.
    if (named.empty() && callee->kind() == ast::AstNodeKind::MemberExpression)
        named = templatesNamedBy(
            unit, site, templateSourceSpelling(namespaceName, {}), methodName, kAny, kAny);
    return named;
}

void ProjectGenericModel::inferNestedGenericCallArguments(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t inferenceDepth)
{
    // First collect bottom-up evidence without diagnosing a missing context. A later candidate may
    // provide an exact parameter type to the inner call (for example `int x = Id(Make())`).
    // The source AST is finite and inferenceDepth applies the same public bound as materialization.
    for (ast::AstNode* argument : call->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::CallExpression ||
            (!argument->typeSyntaxes().empty() || argument->flag())) continue;
        inferGenericCall(
            unit, argument, namespaceName, ownerName, scope, diagnostics, nullptr, false,
            inferenceDepth + 1U);
        if (!argument->typeSyntaxes().empty())
            processGenericCall(unit, argument, namespaceName, ownerName, scope, diagnostics);
    }
}

bool ProjectGenericModel::hasApplicableNonGenericOverload(
    const std::vector<TemplateState*>& candidates,
    const ast::AstNode* call,
    const std::string& methodName,
    const ast::TypeSyntax* expectedType) const
{
    // A potentially applicable non-generic overload remains an ordinary overload-resolution
    // problem. An exact expected type can, however, prove an overload irrelevant by return type;
    // adding such an unrelated declaration must not disable otherwise deterministic inference.
    for (const TemplateState* candidate : candidates)
    {
        if (candidate == nullptr || candidate->parent == nullptr) continue;
        for (const ast::AstNode* sibling : candidate->parent->children())
        {
            if (sibling != nullptr && sibling->kind() == ast::AstNodeKind::MethodDeclaration &&
                sibling->genericParameters().empty() && sibling->text() == methodName &&
                valueParameterCount(sibling) == call->children().size())
            {
                if (expectedType == nullptr || sibling->typeSyntax() == nullptr ||
                    typeSpelling(sibling->typeSyntax()) == typeSpelling(expectedType))
                    return true;
            }
        }
    }
    return false;
}

std::size_t ProjectGenericModel::genericPatternSpecificity(
    const ast::TypeSyntax* pattern,
    const std::unordered_set<std::string>& parameters) const
{
    if (pattern == nullptr) return 0;
    std::size_t score = parameters.contains(pattern->baseName()) &&
            pattern->typeArguments().empty()
        ? 0U : 4U;
    score += pattern->modifiers().size() * 2U;
    for (const ast::TypeSyntax* argument : pattern->typeArguments())
        score += genericPatternSpecificity(argument, parameters);
    return score;
}

bool ProjectGenericModel::mapGenericCallArguments(
    const ast::AstNode* call,
    const TemplateState& candidate,
    std::vector<const ast::AstNode*>& parameters,
    std::vector<std::optional<std::size_t>>& mapping,
    bool& extensionCandidate) const
{
    for (const ast::AstNode* child : candidate.syntax->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
            parameters.push_back(child);
    mapping.resize(call->children().size());
    std::vector<bool> used(parameters.size(), false);
    std::size_t nextPositional = 0;
    for (std::size_t argumentIndex = 0; argumentIndex < call->children().size(); ++argumentIndex)
    {
        const ast::AstNode* argument = call->children()[argumentIndex];
        std::optional<std::size_t> parameterIndex;
        if (argument != nullptr && !argument->argumentName().empty())
        {
            for (std::size_t index = 0; index < parameters.size(); ++index)
                if (parameters[index]->text() == argument->argumentName())
                {
                    parameterIndex = index;
                    break;
                }
        }
        else
        {
            while (nextPositional < used.size() && used[nextPositional]) ++nextPositional;
            if (nextPositional < used.size()) parameterIndex = nextPositional++;
        }
        if (!parameterIndex.has_value() || used[*parameterIndex]) return false;
        used[*parameterIndex] = true;
        mapping[argumentIndex] = parameterIndex;
    }
    // NS304-T08-F06: the extension receiver (first parameter, prepended as `ns3-extension`) is
    // not a call argument, so it must not trigger the "missing argument" check.
    extensionCandidate =
        candidate.syntax->auxiliaryText().find("ns3-extension") != std::string::npos;
    for (std::size_t index = 0; index < parameters.size(); ++index)
        if (!used[index] && parameters[index]->first() == nullptr &&
            !(extensionCandidate && index == 0))
            return false;
    return true;
}

bool ProjectGenericModel::inferGenericCandidateEvidence(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    TemplateState& candidate,
    const std::vector<const ast::AstNode*>& parameters,
    const std::vector<std::optional<std::size_t>>& mapping,
    bool extensionCandidate,
    const std::unordered_set<std::string>& genericParameters,
    const GenericLookupScope& scope,
    const ast::TypeSyntax* expectedType,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    bool viable = true;
    std::unordered_map<std::string, const ast::TypeSyntax*> ownerSubstitutions;
    if (candidate.parent != nullptr && !candidate.parent->genericParameters().empty() &&
        call->first() != nullptr && call->first()->kind() == ast::AstNodeKind::MemberExpression)
    {
        ast::TypeSyntax* receiver = inferExpressionType(unit, call->first()->first(), scope);
        if (receiver != nullptr &&
            receiver->typeArguments().size() == candidate.parent->genericParameters().size())
        {
            for (std::size_t i = 0; i < receiver->typeArguments().size(); ++i)
                if (!genericParameters.contains(candidate.parent->genericParameters()[i]))
                    ownerSubstitutions.emplace(
                        candidate.parent->genericParameters()[i], receiver->typeArguments()[i]);
        }
    }
    const auto ownerPattern = [&](const ast::TypeSyntax* pattern) -> const ast::TypeSyntax* {
        if (ownerSubstitutions.empty() || pattern == nullptr) return pattern;
        bool conflict = false;
        ast::TypeSyntax* substituted = cloneAliasType(unit.arena(), pattern, ownerSubstitutions, conflict);
        return conflict || substituted == nullptr ? pattern : substituted;
    };
    for (std::size_t index = 0; viable && index < call->children().size(); ++index)
    {
        ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
        const ast::TypeSyntax* pattern = ownerPattern(parameters[*mapping[index]]->typeSyntax());
        // `null`, an unresolved member, or another expression without a local type supplies no
        // evidence. It is not a contradiction: an exact expected result may still infer T.
        if (actual != nullptr)
            viable = inferTypeArguments(
                unit, pattern, actual, candidate.record, genericParameters, inferred);
        else if (call->children()[index] != nullptr &&
                 call->children()[index]->kind() == ast::AstNodeKind::LambdaExpression)
            viable = inferLambdaTypeArguments(
                unit, call->children()[index], pattern, candidate.record,
                genericParameters, scope, inferred);
    }
    // NS304-T08-F06: an extension receiver supplies the inference evidence when the call has no
    // explicit arguments (`b.Twice()` on `Box<I>` for `Twice<T>` over `Box<T>`).
    if (viable && call->children().empty() && call->first() != nullptr &&
        call->first()->kind() == ast::AstNodeKind::MemberExpression &&
        call->first()->first() != nullptr && !parameters.empty() && extensionCandidate)
    {
        ast::TypeSyntax* receiverActual = inferExpressionType(unit, call->first()->first(), scope);
        if (receiverActual != nullptr && parameters.front()->typeSyntax() != nullptr)
            viable = inferTypeArguments(
                unit, parameters.front()->typeSyntax(), receiverActual,
                candidate.record, genericParameters, inferred);
    }
    if (viable && expectedType != nullptr && candidate.syntax->typeSyntax() != nullptr)
        viable = inferTypeArguments(
            unit, ownerPattern(candidate.syntax->typeSyntax()), expectedType,
            candidate.record, genericParameters, inferred);
    return viable;
}

bool ProjectGenericModel::applyGenericCandidateContext(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    TemplateState& candidate,
    const std::vector<const ast::AstNode*>& parameters,
    const std::vector<std::optional<std::size_t>>& mapping,
    const std::unordered_set<std::string>& genericParameters,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t inferenceDepth,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    // With one visible generic template there is no candidate-state leak: any parameter type
    // made fully concrete by the first pass is safe context for an unresolved nested
    // expression. This is the top-down half of the ordered bidirectional algorithm.
    const std::function<bool(const ast::TypeSyntax*)> containsOpenParameter =
        [&](const ast::TypeSyntax* syntax)
        {
            if (syntax == nullptr) return false;
            if (genericParameters.contains(syntax->baseName()) &&
                syntax->typeArguments().empty())
                return true;
            return std::ranges::any_of(
                syntax->typeArguments(), containsOpenParameter);
        };
    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (const auto& [name, type] : inferred) substitutions.emplace(name, type);
    for (std::size_t index = 0; index < call->children().size(); ++index)
    {
        if (inferExpressionType(unit, call->children()[index], scope) != nullptr) continue;
        bool conflict = false;
        ast::TypeSyntax* contextualParameter = cloneAliasType(
            unit.arena(), parameters[*mapping[index]]->typeSyntax(), substitutions, conflict);
        if (conflict || contextualParameter == nullptr ||
            containsOpenParameter(contextualParameter))
            continue;
        if (!applyExpectedTypeToExpression(
                unit, call->children()[index], contextualParameter, namespaceName,
                ownerName, scope, diagnostics, inferenceDepth + 1U))
            return false;
        ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
        if (actual != nullptr &&
            !inferTypeArguments(
                unit, parameters[*mapping[index]]->typeSyntax(), actual,
                candidate.record, genericParameters, inferred))
            return false;
    }
    return true;
}

bool ProjectGenericModel::inferGenericCandidate(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    TemplateState& candidate,
    std::size_t candidateCount,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    const ast::TypeSyntax* expectedType,
    std::size_t inferenceDepth,
    std::vector<ast::TypeSyntax*>& arguments,
    std::size_t& specificity)
{
    if (candidate.syntax == nullptr) return false;
    std::optional<TemplateState> closedCandidate;
    const ast::AstNode* callee = call->first();
    if (candidate.parent != nullptr && !candidate.parent->genericParameters().empty() &&
        callee != nullptr &&
        (callee->kind() == ast::AstNodeKind::IdentifierExpression ||
         (callee->kind() == ast::AstNodeKind::MemberExpression && callee->first() != nullptr &&
          callee->first()->kind() == ast::AstNodeKind::ThisExpression)))
    {
        const std::size_t separator = ownerName.find_last_of(".+");
        const std::string_view closedName = separator == std::string::npos
            ? std::string_view(ownerName) : std::string_view(ownerName).substr(separator + 1U);
        for (const GenericSpecializationRecord& owner : specializations_)
        {
            if (owner.templateSyntax != candidate.parent || owner.specializedName != closedName ||
                owner.specializedSyntax == nullptr) continue;
            for (const ast::AstNode* member : owner.specializedSyntax->children())
                if (member != nullptr && isGenericMethodTemplateNode(member) &&
                    member->range().begin().fileId() == candidate.syntax->range().begin().fileId() &&
                    member->range().begin().offset() == candidate.syntax->range().begin().offset())
                {
                    closedCandidate = candidate;
                    closedCandidate->syntax = const_cast<ast::AstNode*>(member);
                    break;
                }
            break;
        }
    }
    TemplateState& inferenceCandidate = closedCandidate.has_value() ? *closedCandidate : candidate;
    std::vector<const ast::AstNode*> parameters;
    std::vector<std::optional<std::size_t>> mapping;
    bool extensionCandidate = false;
    if (!mapGenericCallArguments(
            call, inferenceCandidate, parameters, mapping, extensionCandidate))
        return false;
    const std::unordered_set<std::string> genericParameters(
        candidate.record.parameters.begin(), candidate.record.parameters.end());
    std::unordered_map<std::string, ast::TypeSyntax*> inferred;
    if (!inferGenericCandidateEvidence(
            unit, call, inferenceCandidate, parameters, mapping, extensionCandidate,
            genericParameters, scope, expectedType, inferred))
        return false;
    if (candidateCount == 1U &&
        !applyGenericCandidateContext(
            unit, call, inferenceCandidate, parameters, mapping, genericParameters,
            namespaceName, ownerName, scope, diagnostics, inferenceDepth, inferred))
        return false;
    if (inferred.size() != candidate.record.parameters.size()) return false;
    for (const ast::AstNode* parameter : parameters)
        specificity += genericPatternSpecificity(parameter->typeSyntax(), genericParameters);
    for (const std::string& parameter : candidate.record.parameters)
        arguments.push_back(inferred.at(parameter));
    return true;
}

void ProjectGenericModel::collectGenericCallInferences(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::vector<TemplateState*>& candidates,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    const ast::TypeSyntax* expectedType,
    std::size_t inferenceDepth,
    std::vector<std::tuple<
        TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>>& successes)
{
    for (TemplateState* candidate : candidates)
    {
        if (candidate == nullptr) continue;
        std::vector<ast::TypeSyntax*> arguments;
        std::size_t specificity = 0;
        if (inferGenericCandidate(
                unit, call, *candidate, candidates.size(), namespaceName, ownerName,
                scope, diagnostics, expectedType, inferenceDepth, arguments, specificity))
            successes.emplace_back(candidate, std::move(arguments), specificity);
    }
}

void ProjectGenericModel::recordGenericInferenceReturnType(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::vector<std::tuple<
        TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>>& successes)
{
    std::string returnSpelling;
    ast::TypeSyntax* inferredReturn = nullptr;
    bool commonReturn = true;
    for (const auto& success : successes)
    {
        TemplateState* candidate = std::get<0>(success);
        const std::vector<ast::TypeSyntax*>& arguments = std::get<1>(success);
        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
        for (std::size_t index = 0; index < arguments.size(); ++index)
            substitutions.emplace(candidate->record.parameters[index], arguments[index]);
        bool conflict = false;
        ast::TypeSyntax* candidateReturn = cloneAliasType(
            unit.arena(), candidate->syntax->typeSyntax(), substitutions, conflict);
        const std::string spelling = typeSpelling(candidateReturn);
        if (inferredReturn == nullptr)
        {
            inferredReturn = candidateReturn;
            returnSpelling = spelling;
        }
        else if (spelling != returnSpelling)
        {
            commonReturn = false;
        }
    }
    if (commonReturn && inferredReturn != nullptr)
        inferredCallReturnTypes_[call] = inferredReturn;
}

bool ProjectGenericModel::haveSameGenericInferenceArguments(
    const std::tuple<TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>& candidate,
    const std::tuple<TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>& first) const
{
    const std::vector<ast::TypeSyntax*>& candidateArguments = std::get<1>(candidate);
    const std::vector<ast::TypeSyntax*>& firstArguments = std::get<1>(first);
    if (candidateArguments.size() != firstArguments.size()) return false;
    for (std::size_t index = 0; index < candidateArguments.size(); ++index)
        if (typeSpelling(candidateArguments[index]) != typeSpelling(firstArguments[index]))
            return false;
    return true;
}

bool ProjectGenericModel::commitGenericCallInference(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& methodName,
    bool reportFailure,
    diagnostics::DiagnosticEngine& diagnostics,
    std::vector<std::tuple<
        TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>>& successes)
{
    if (successes.empty())
    {
        if (reportFailure)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "type arguments for generic method '" + methodName +
                    "' cannot be inferred from the local call arguments or exact expected type; "
                    "write them explicitly");
        return false;
    }
    std::size_t bestSpecificity = 0;
    for (const auto& inference : successes)
        bestSpecificity = std::max(bestSpecificity, std::get<2>(inference));
    std::erase_if(successes, [&](const auto& inference)
    {
        return std::get<2>(inference) != bestSpecificity;
    });
    recordGenericInferenceReturnType(unit, call, successes);
    for (const auto& success : successes)
    {
        if (haveSameGenericInferenceArguments(success, successes.front())) continue;
        if (reportFailure)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic method call '" + methodName +
                    "' has equally specific but incompatible type inferences; "
                    "write type arguments explicitly");
        return false;
    }
    for (ast::TypeSyntax* argument : std::get<1>(successes.front()))
    {
        bool conflict = false;
        call->addTypeSyntax(cloneAliasType(unit.arena(), argument, {}, conflict));
    }
    return true;
}

bool ProjectGenericModel::inferGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    const ast::TypeSyntax* expectedType,
    bool reportFailure,
    std::size_t inferenceDepth)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression ||
        !call->typeSyntaxes().empty()) return true;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.infer-generic-call",
        genericNodeDetail(unit, *call) + " depth=" + std::to_string(inferenceDepth));
#endif // INSTRUMENTATION CODE
    if (inferenceDepth > DefaultMaximumInstantiationDepth)
    {
        if (reportFailure)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic inference exceeded the bounded expression depth (" +
                    std::to_string(DefaultMaximumInstantiationDepth) +
                    "); write type arguments explicitly");
        return false;
    }
    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    if (callee == nullptr || methodName.empty()) return true;
    const std::vector<TemplateState*> candidates = resolveGenericInferenceCandidates(
        unit, namespaceName, ownerName, scope, callee, methodName);
    if (candidates.empty()) return true;
    inferNestedGenericCallArguments(
        unit, call, namespaceName, ownerName, scope, diagnostics, inferenceDepth);
    if (hasApplicableNonGenericOverload(candidates, call, methodName, expectedType)) return true;
    std::vector<std::tuple<
        TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>> successes;
    collectGenericCallInferences(
        unit, call, candidates, namespaceName, ownerName, scope, diagnostics,
        expectedType, inferenceDepth, successes);
    return commitGenericCallInference(
        unit, call, methodName, reportFailure, diagnostics, successes);
}

// NSC305-G08. Materializes every shared-overload candidate of a generic call (see the header).
bool ProjectGenericModel::materializeSharedOverloads(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    ast::AstNode* callee,
    const std::string& methodName,
    const std::string& namespaceName,
    const std::vector<TemplateState*>& shared,
    ast::AstNode* closedOwner,
    const std::string& closedOwnerKey,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::string sharedName = methodName + "__g" +
        std::to_string(call->typeSyntaxes().size());
    for (const ast::TypeSyntax* argument : call->typeSyntaxes())
        sharedName += "__" + sanitize(typeSpelling(argument));

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    currentInstantiationRange_ = call->range();
    // NSC-LIMIT-007: las que salen de aqui son ALTERNATIVAS entre si, no obligaciones
    // simultaneas. Se marcan con un grupo comun para que la validacion de constraints exija
    // que fallen todas antes de decir nada.
    currentOverloadGroup_ = ++nextOverloadGroup_;
    for (TemplateState* candidate : shared)
    {
        if (!genericArityAccepts(candidate->record, call->typeSyntaxes().size())) continue;
        if (!normalizeGenericArguments(
                candidate->record, call->typeSyntaxes(), call->range(), diagnostics))
        {
            currentOverloadGroup_ = 0;
            return false;
        }
        if (ensureMethodSpecialization(
                *candidate, call->typeSyntaxes(), diagnostics, sharedName, closedOwner, closedOwnerKey).empty())
        {
            currentOverloadGroup_ = 0;
            return false;
        }
    }
    currentOverloadGroup_ = 0;
    if (callee != nullptr) callee->setText(sharedName);
    preserveCallTypeArguments(call);
    call->clearTypeSyntaxes();
    call->setFlag(false);
    return true;
}

// NSC305-G08. Normalizes and materializes the single resolved method (see the header).
bool ProjectGenericModel::materializeUniqueMethod(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    ast::AstNode* callee,
    TemplateState& state,
    const std::string& namespaceName,
    ast::AstNode* closedOwner,
    const std::string& closedOwnerKey,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!normalizeGenericArguments(
            state.record, call->typeSyntaxes(), call->range(), diagnostics))
        return false;

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    currentInstantiationRange_ = call->range();
    const std::string specialized = ensureMethodSpecialization(
        state, call->typeSyntaxes(), diagnostics, {}, closedOwner, closedOwnerKey);
    if (specialized.empty()) return false;
    if (callee != nullptr) callee->setText(specialized);
    preserveCallTypeArguments(call);
    call->clearTypeSyntaxes();
    call->setFlag(false);
    return true;
}

bool ProjectGenericModel::processGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression) return false;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.process-generic-call", genericNodeDetail(unit, *call));
#endif // INSTRUMENTATION CODE
    for (ast::TypeSyntax* argument : call->typeSyntaxes())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
    }

    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    // La escritura del receptor, o --si no hay receptor escrito-- la del tipo desde el que se
    // llama, que es como se nombra a un metodo estatico del propio tipo.
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }

    // ID 132. Los argumentos de VALOR de la llamada. Los hijos de un nodo de llamada son sus
    // argumentos; el receptor viaja en el callee, no aqui.
    const std::size_t argumentCount = call->children().size();

    // ID 132, SEGUNDA MITAD -- las que comparten cuenta de parametros y difieren en los TIPOS.
    //
    // Filtrar por cuenta de argumentos separa `Cuenta<T>(T[])` de `Cuenta<T>(T[], int, int)`. Lo
    // que NO puede separar es `F<T>(T)` de `F<T>(T[])`: los dos toman uno, y elegir necesita el
    // TIPO del argumento -- que esta pasada no conoce, porque corre antes del modelo semantico.
    //
    // ANTES SE RENDIA: "duplicate generic template identity", y habia que renombrar.
    // AHORA SE MATERIALIZAN TODAS con el MISMO nombre especializado, y a partir de ese momento son
    // sobrecargas ORDINARIAS -- metodos no genericos con el mismo nombre y listas de parametros
    // distintas, que este compilador resuelve por tipos desde siempre.
    //
    // No se adivina nada: se le pasa el problema a la capa que tiene los datos. Y si dos
    // especializaciones acaban con la MISMA lista de parametros --`F<int>(int)` y `F<T>(int)`--
    // el choque lo detecta la deteccion de metodos duplicados de siempre, que es su sitio.
    // EL SITIO DE LA LLAMADA, en la forma que entiende la escala compartida.
    QualifiedNameSiteContext siteContext = makeQualifiedNameSite(namespaceName, ownerName, scope);
    const QualifiedNameSite& site = siteContext.site;

    GenericCallResolution resolution = resolveGenericCall(
        unit, site, requestedOwner, methodName, call->typeSyntaxes().size(), argumentCount);
    ast::AstNode* closedOwner = nullptr;
    std::string closedOwnerKey;
    if (callee != nullptr &&
        (callee->kind() == ast::AstNodeKind::IdentifierExpression ||
         (callee->kind() == ast::AstNodeKind::MemberExpression && callee->first() != nullptr &&
          callee->first()->kind() == ast::AstNodeKind::ThisExpression)))
    {
        const std::size_t separator = ownerName.find_last_of(".+");
        const std::string_view closedName = separator == std::string::npos
            ? std::string_view(ownerName) : std::string_view(ownerName).substr(separator + 1U);
        for (const GenericSpecializationRecord& owner : specializations_)
            if (owner.specializedName == closedName && owner.specializedSyntax != nullptr &&
                owner.templateOrdinal < templateStates_.size() &&
                templateStates_[owner.templateOrdinal].record.projectOrdinal == unit.projectOrdinal() &&
                templateStates_[owner.templateOrdinal].record.namespaceName == namespaceName)
            {
                closedOwner = const_cast<ast::AstNode*>(owner.specializedSyntax);
                closedOwnerKey = owner.specializationKey;
                break;
            }
        const std::vector<TemplateState*> candidates = resolveGenericInferenceCandidates(
            unit, namespaceName, ownerName, scope, callee, methodName);
        const bool knownClosedOwner = std::ranges::any_of(candidates, [&](const TemplateState* candidate) {
            return candidate != nullptr && candidate->parent != nullptr &&
                !candidate->parent->genericParameters().empty() &&
                std::ranges::any_of(specializations_, [&](const GenericSpecializationRecord& owner) {
                    return owner.specializedName == closedName && owner.templateSyntax == candidate->parent;
                });
        });
        if (knownClosedOwner)
        {
            resolution = GenericCallResolution{};
            std::vector<TemplateState*> matching;
            for (TemplateState* candidate : candidates)
            {
                if (valueParameterCount(candidate->syntax) != argumentCount) continue;
                matching.push_back(candidate);
                if (genericArityAccepts(candidate->record, call->typeSyntaxes().size()))
                    resolution.named.push_back(candidate);
            }
            resolution.shared = collectMethodTemplatesFromNamed(resolution.named);
            if (resolution.shared.size() == 1U) resolution.unique = resolution.shared.front();
            else if (resolution.named.empty() && matching.size() == 1U) resolution.unique = matching.front();
        }
    }
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const auto bucket = methodsByName_.find(methodName);
        const bool hasGenericOwner = bucket != methodsByName_.end() &&
            std::ranges::any_of(bucket->second, [](const TemplateState* candidate) {
                return candidate != nullptr && candidate->parent != nullptr &&
                    !candidate->parent->genericParameters().empty();
            });
        ProjectSyntaxUnit* receiverUnit = &unit;
        std::string receiverNamespace = namespaceName;
        GenericLookupScope fieldScope;
        const GenericLookupScope* receiverScope = &scope;
        ast::TypeSyntax* actual = hasGenericOwner ? inferExpressionType(unit, callee->first(), scope) : nullptr;
        if (actual == nullptr && hasGenericOwner && callee->first() != nullptr &&
            callee->first()->kind() == ast::AstNodeKind::IndexExpression)
        {
            ast::TypeSyntax* collection = inferExpressionType(unit, callee->first()->first(), scope);
            const GenericSpecializationRecord* thisSpecialization = nullptr;
            if (collection == nullptr && callee->first()->first() != nullptr && !ownerName.empty() &&
                callee->first()->first()->kind() == ast::AstNodeKind::ThisExpression)
            {
                std::string name = qualifiedName(namespaceName, ownerName);
                std::replace(name.begin(), name.end(), '+', '.');
                const std::string identity = std::string(unit.moduleName()) + "::" + name;
                const DeclaredTypeIdentity* declaration =
                    resolveModuleQualifiedDeclaredTypeIdentity(identity, unit.projectOrdinal());
                if (declaration != nullptr && declaration->projectOrdinal == unit.projectOrdinal())
                    collection = unit.arena().make<ast::TypeSyntax>(
                        callee->first()->first()->range(), lexer::TokenKind::Identifier, identity);
                if (collection == nullptr)
                {
                    for (const GenericSpecializationRecord& specialization : specializations_)
                    {
                        const std::string repeatedOwner =
                            specialization.specializedName + "+" + specialization.specializedName;
                        if ((specialization.specializedName != ownerName && repeatedOwner != ownerName) ||
                            specialization.specializedSyntax == nullptr ||
                            specialization.templateOrdinal >= templateStates_.size()) continue;
                        const TemplateState& state = templateStates_[specialization.templateOrdinal];
                        if (state.record.projectOrdinal != unit.projectOrdinal() ||
                            state.record.namespaceName != namespaceName || !state.record.ownerName.empty()) continue;
                        if (thisSpecialization != nullptr) { thisSpecialization = nullptr; break; }
                        thisSpecialization = &specialization;
                    }
                }
            }
            if (collection != nullptr && !collection->modifiers().empty() &&
                collection->modifiers().back().kind == ast::TypeModifierSyntaxKind::DynamicArray)
                actual = cloneType(unit.arena(), collection, {}, collection->modifiers().size() - 1U);
            else if ((collection != nullptr && collection->modifiers().empty()) || thisSpecialization != nullptr)
            {
                const ast::AstNode* owner = nullptr;
                TemplateState* containerTemplate = nullptr;
                ast::TypeSyntax* containerCopy = nullptr;
                if (thisSpecialization != nullptr)
                {
                    owner = thisSpecialization->specializedSyntax;
                    containerTemplate = &templateStates_[thisSpecialization->templateOrdinal];
                }
                else if (collection->typeArguments().empty())
                {
                    const DeclaredTypeIdentity* declaration =
                        resolveDeclaredTypeIdentity(collection, unit.projectOrdinal(), namespaceName, scope);
                    owner = declaration != nullptr ? declaration->syntax : nullptr;
                }
                else
                {
                    TypeResolution resolved = resolveTypeTemplate(
                        unit, namespaceName, collection->baseName(), collection->typeArguments().size(), scope);
                    if (resolved.kind == TypeResolutionKind::Unique && resolved.state != nullptr)
                    {
                        bool conflict = false;
                        containerCopy = cloneAliasType(unit.arena(), collection, {}, conflict);
                        if (containerCopy != nullptr && !conflict && !hasOpenGenericParameter(containerCopy, scope))
                        {
                            for (ast::TypeSyntax* argument : containerCopy->typeArguments())
                                if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
                            if (!normalizeGenericArguments(resolved.state->record, containerCopy->typeArguments(),
                                                           containerCopy->range(), diagnostics)) return false;
                            containerTemplate = resolved.state;
                            owner = containerTemplate->syntax;
                        }
                    }
                }
                auto context = declaredFieldContexts_.find(owner);
                std::vector<std::pair<const ast::AstNode*, const ast::AstNode*>> indexers;
                bool completeIndexerSet = true;
                const ast::AstNode* accessingOwner = nullptr;
                if (!ownerName.empty())
                {
                    std::string sourceName = qualifiedName(namespaceName, ownerName);
                    std::replace(sourceName.begin(), sourceName.end(), '+', '.');
                    auto callers = declaredTypeIdentitiesBySourceName_.find(sourceName);
                    if (callers != declaredTypeIdentitiesBySourceName_.end())
                        for (std::size_t position : callers->second)
                        {
                            const DeclaredTypeIdentity& caller = declaredTypeIdentities_[position];
                            if (caller.projectOrdinal != unit.projectOrdinal()) continue;
                            if (accessingOwner != nullptr) { accessingOwner = nullptr; break; }
                            accessingOwner = caller.syntax;
                        }
                }
                std::unordered_set<const ast::AstNode*> visitedOwners;
                while (owner != nullptr && visitedOwners.insert(owner).second &&
                       ((context != declaredFieldContexts_.end() && context->second.unit != nullptr) ||
                        (containerTemplate != nullptr && containerTemplate->unit != nullptr)))
                {
                    bool hasDeclaredIndexer = false;
                    if (!owner->typeSyntaxes().empty()) completeIndexerSet = false;
                    for (const ast::AstNode* member : owner->children())
                    {
                        if (member == nullptr || member->kind() != ast::AstNodeKind::IndexerDeclaration ||
                            member->typeSyntax() == nullptr) continue;
                        hasDeclaredIndexer = true;
                        for (const ast::AstNode* accessor : member->children())
                        {
                            if (accessor == nullptr || accessor->text() != "get_Item") continue;
                            if (containerTemplate == nullptr && owner->genericParameters().empty() &&
                                accessor->hasModifier(lexer::TokenKind::KwPrivate) &&
                                accessingOwner != nullptr && accessingOwner != owner) continue;
                            if (valueParameterCount(accessor) != callee->first()->children().size())
                            {
                                completeIndexerSet = false;
                                continue;
                            }
                            if (!accessor->hasModifier(lexer::TokenKind::KwPublic) ||
                                !accessor->genericConstraints().empty() || accessor->isReference() ||
                                accessor->hasModifier(lexer::TokenKind::KwReadonly)) completeIndexerSet = false;
                            for (const ast::AstNode* parameter : accessor->children())
                                if (parameter != nullptr && parameter->kind() == ast::AstNodeKind::ParameterDeclaration &&
                                    (parameter->isVariadic() || parameter->isReference() || parameter->first() != nullptr))
                                    completeIndexerSet = false;
                            indexers.emplace_back(member, accessor);
                        }
                    }
                    if (hasDeclaredIndexer || context == declaredFieldContexts_.end() ||
                        context->second.unit == nullptr || owner->kind() != ast::AstNodeKind::ClassDeclaration) break;
                    GenericLookupScope baseScope;
                    baseScope.importedNamespaces.write() = context->second.imports;
                    baseScope.aliases.write() = context->second.aliases;
                    const ast::AstNode* baseOwner = nullptr;
                    TemplateState* baseTemplate = nullptr;
                    ast::TypeSyntax* baseCopy = nullptr;
                    for (const ast::TypeSyntax* base : owner->typeSyntaxes())
                    {
                        if (base == nullptr) continue;
                        if (!base->typeArguments().empty())
                        {
                            std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
                            auto previousPacks = std::move(activePackSubstitutions_);
                            if (containerTemplate != nullptr && containerCopy != nullptr)
                                prepareSpecializationSubstitutions(*containerTemplate, containerCopy->typeArguments(),
                                                                   substitutions);
                            ast::TypeSyntax* candidate = cloneType(context->second.unit->arena(), base, substitutions);
                            activePackSubstitutions_ = std::move(previousPacks);
                            TypeResolution resolved = resolveTypeTemplate(*context->second.unit,
                                context->second.namespaceName, candidate->baseName(), candidate->typeArguments().size(),
                                baseScope);
                            if (resolved.kind != TypeResolutionKind::Unique || resolved.state == nullptr ||
                                resolved.state->syntax == nullptr ||
                                resolved.state->syntax->kind() != ast::AstNodeKind::ClassDeclaration) continue;
                            if (baseOwner != nullptr) { baseOwner = nullptr; break; }
                            for (ast::TypeSyntax* argument : candidate->typeArguments())
                                if (!processTypeSyntax(*context->second.unit, argument,
                                                       context->second.namespaceName, baseScope, diagnostics)) return false;
                            if (!normalizeGenericArguments(resolved.state->record, candidate->typeArguments(),
                                                           candidate->range(), diagnostics)) return false;
                            baseOwner = resolved.state->syntax;
                            baseTemplate = resolved.state;
                            baseCopy = candidate;
                        }
                        else
                        {
                            const DeclaredTypeIdentity* resolved = resolveDeclaredTypeIdentity(
                                base, context->second.unit->projectOrdinal(), context->second.namespaceName, baseScope);
                            if (resolved == nullptr || resolved->syntax == nullptr ||
                                resolved->syntax->kind() != ast::AstNodeKind::ClassDeclaration) continue;
                            if (baseOwner != nullptr) { baseOwner = nullptr; break; }
                            baseOwner = resolved->syntax;
                        }
                    }
                    owner = baseOwner;
                    containerTemplate = baseTemplate;
                    containerCopy = baseCopy;
                    context = declaredFieldContexts_.find(owner);
                }
                const ast::AstNode* indexer = indexers.size() == 1U ? indexers.front().first : nullptr;
                if (indexers.size() > 1U)
                {
                    GenericLookupScope declarationScope;
                    ProjectSyntaxUnit* declarationUnit = nullptr;
                    std::string declarationNamespace;
                    if (containerTemplate != nullptr)
                    {
                        declarationUnit = containerTemplate->unit;
                        declarationNamespace = containerTemplate->record.namespaceName;
                        declarationScope.importedNamespaces.write() = containerTemplate->record.declarationImports;
                        declarationScope.aliases.write().insert(containerTemplate->record.declarationAliases.begin(),
                                                                containerTemplate->record.declarationAliases.end());
                    }
                    else if (context != declaredFieldContexts_.end())
                    {
                        declarationUnit = context->second.unit;
                        declarationNamespace = context->second.namespaceName;
                        declarationScope.importedNamespaces.write() = context->second.imports;
                        declarationScope.aliases.write() = context->second.aliases;
                    }
                    bool unknown = declarationUnit == nullptr;
                    for (const auto& [member, accessor] : indexers)
                    {
                        bool exact = true;
                        std::size_t argument = 0;
                        for (const ast::AstNode* parameter : accessor->children())
                        {
                            if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
                                continue;
                            ast::TypeSyntax* type = inferExpressionType(unit, callee->first()->children()[argument++], scope);
                            const ast::TypeSyntax* expected = parameter->typeSyntax();
                            if (type == nullptr || expected == nullptr || parameter->isReference() ||
                                type->baseTokenKind() == lexer::TokenKind::Invalid ||
                                expected->baseTokenKind() == lexer::TokenKind::Invalid ||
                                !type->typeArguments().empty() || !expected->typeArguments().empty() ||
                                !type->modifiers().empty() || !expected->modifiers().empty())
                            {
                                unknown = true;
                                exact = false;
                                break;
                            }
                            const bool actualNominal = type->baseTokenKind() == lexer::TokenKind::Identifier;
                            const bool expectedNominal = expected->baseTokenKind() == lexer::TokenKind::Identifier;
                            const DeclaredTypeIdentity* actualIdentity = actualNominal
                                ? resolveDeclaredTypeIdentity(type, unit.projectOrdinal(), namespaceName, scope) : nullptr;
                            const DeclaredTypeIdentity* expectedIdentity = expectedNominal && declarationUnit != nullptr
                                ? resolveDeclaredTypeIdentity(expected, declarationUnit->projectOrdinal(),
                                                              declarationNamespace, declarationScope) : nullptr;
                            if ((actualNominal && actualIdentity == nullptr) ||
                                (expectedNominal && expectedIdentity == nullptr)) unknown = true;
                            if (actualNominal || expectedNominal)
                                exact = actualIdentity != nullptr && expectedIdentity != nullptr &&
                                    actualIdentity->identityKey == expectedIdentity->identityKey;
                            else exact = type->baseTokenKind() == expected->baseTokenKind();
                            if (!exact) break;
                        }
                        if (!exact) continue;
                        if (indexer != nullptr) { indexer = nullptr; unknown = true; break; }
                        indexer = member;
                    }
                    if (unknown) indexer = nullptr;
                    if (indexer == nullptr && !unknown)
                    {
                        struct NumericCandidate final
                        {
                            const ast::AstNode* member;
                            std::vector<ProjectImplicitConversion> conversions;
                        };
                        std::vector<NumericCandidate> viable;
                        bool numericOnly = true;
                        for (const auto& [member, accessor] : indexers)
                        {
                            NumericCandidate candidate{member, {}};
                            bool candidateViable = true;
                            std::size_t argument = 0;
                            for (const ast::AstNode* parameter : accessor->children())
                            {
                                if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
                                    continue;
                                const ast::AstNode* argumentExpression = callee->first()->children()[argument++];
                                ast::TypeSyntax* type = inferExpressionType(unit, argumentExpression, scope);
                                const ast::TypeSyntax* expected = parameter->typeSyntax();
                                if (type == nullptr || expected == nullptr || parameter->isReference() ||
                                    type->baseTokenKind() == lexer::TokenKind::Identifier ||
                                    expected->baseTokenKind() == lexer::TokenKind::Identifier ||
                                    !type->typeArguments().empty() || !expected->typeArguments().empty() ||
                                    !type->modifiers().empty() || !expected->modifiers().empty())
                                {
                                    numericOnly = false;
                                    break;
                                }
                                const std::string sourceKey = "builtin:" + type->baseName();
                                const std::string targetKey = "builtin:" + expected->baseName();
                                if (!ProjectTypeConversion::isNumeric(sourceKey) ||
                                    !ProjectTypeConversion::isNumeric(targetKey))
                                {
                                    numericOnly = false;
                                    break;
                                }
                                std::optional<ProjectIntegerConstant> constant;
                                if (argumentExpression->kind() == ast::AstNodeKind::IntegerLiteralExpression)
                                {
                                    const auto value = ProjectIntegerEvaluator(nullptr).evaluate(argumentExpression);
                                    if (!value.has_value()) { numericOnly = false; break; }
                                    const bool isSigned = ProjectIntegerEvaluator::isSignedInteger(value->typeKey);
                                    const bool negative = isSigned && value->asSigned() < 0;
                                    constant = ProjectIntegerConstant{
                                        negative ? ~value->bits + 1U : value->bits, !isSigned, negative};
                                }
                                ProjectImplicitConversion conversion =
                                    ProjectTypeConversion::classifyImplicit(sourceKey, targetKey, constant);
                                if (!conversion.exists())
                                {
                                    const bool knownValueBinding =
                                        argumentExpression->kind() == ast::AstNodeKind::IdentifierExpression &&
                                        scope.findValueType(argumentExpression->text()) != nullptr;
                                    if (!constant.has_value() && !knownValueBinding) numericOnly = false;
                                    candidateViable = false;
                                    break;
                                }
                                candidate.conversions.push_back(std::move(conversion));
                            }
                            if (!numericOnly) break;
                            if (candidateViable) viable.push_back(std::move(candidate));
                        }
                        const auto better = [](const NumericCandidate& left, const NumericCandidate& right) {
                            bool strictlyBetter = false;
                            for (std::size_t i = 0; i < left.conversions.size(); ++i)
                            {
                                const auto& a = left.conversions[i];
                                const auto& b = right.conversions[i];
                                if (a.rank > b.rank) return false;
                                if (a.rank < b.rank) strictlyBetter = true;
                                if (a.rank != b.rank || a.targetTypeKey == b.targetTypeKey) continue;
                                const bool aToB = ProjectTypeConversion::classifyImplicit(a.targetTypeKey, b.targetTypeKey).exists();
                                const bool bToA = ProjectTypeConversion::classifyImplicit(b.targetTypeKey, a.targetTypeKey).exists();
                                if (bToA && !aToB) return false;
                                if (aToB && !bToA) strictlyBetter = true;
                            }
                            return strictlyBetter;
                        };
                        if (numericOnly)
                            for (std::size_t i = 0; i < viable.size(); ++i)
                            {
                                bool dominated = false;
                                for (std::size_t j = 0; j < viable.size(); ++j)
                                    if (i != j && better(viable[j], viable[i])) { dominated = true; break; }
                                if (dominated) continue;
                                if (indexer != nullptr)
                                {
                                    indexer = nullptr;
                                    if (completeIndexerSet)
                                    {
                                        diagnostics.report(diagnostics::DiagnosticId::InvalidCall,
                                            diagnostics::DiagnosticSeverity::Error, callee->first()->range(),
                                            "ambiguous indexer call to 'get_Item'");
                                        return false;
                                    }
                                    break;
                                }
                                indexer = viable[i].member;
                            }
                    }
                }
                if (indexer != nullptr)
                {
                    if (containerTemplate != nullptr)
                    {
                        receiverUnit = containerTemplate->unit;
                        receiverNamespace = containerTemplate->record.namespaceName;
                        fieldScope.importedNamespaces.write() = containerTemplate->record.declarationImports;
                        fieldScope.aliases.write().insert(containerTemplate->record.declarationAliases.begin(),
                                                          containerTemplate->record.declarationAliases.end());
                    }
                    else
                    {
                        receiverUnit = context->second.unit;
                        receiverNamespace = context->second.namespaceName;
                        fieldScope.importedNamespaces.write() = context->second.imports;
                        fieldScope.aliases.write() = context->second.aliases;
                    }
                    receiverScope = &fieldScope;
                    if (containerTemplate != nullptr && containerCopy != nullptr)
                    {
                        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
                        auto previousPacks = std::move(activePackSubstitutions_);
                        prepareSpecializationSubstitutions(*containerTemplate, containerCopy->typeArguments(), substitutions);
                        actual = cloneType(receiverUnit->arena(), indexer->typeSyntax(), substitutions);
                        activePackSubstitutions_ = std::move(previousPacks);
                    }
                    else actual = cloneInferredType(*receiverUnit, indexer->typeSyntax());
                }
            }
        }
        if (actual == nullptr && hasGenericOwner && callee->first() != nullptr &&
            callee->first()->kind() == ast::AstNodeKind::MemberExpression)
        {
            const ast::AstNode* access = callee->first();
            ast::TypeSyntax* containingType = inferExpressionType(unit, access->first(), scope);
            if (containingType == nullptr && access->first() != nullptr &&
                access->first()->kind() == ast::AstNodeKind::IndexExpression)
            {
                ast::TypeSyntax* collection = inferExpressionType(unit, access->first()->first(), scope);
                if (collection != nullptr && !collection->modifiers().empty() &&
                    collection->modifiers().back().kind == ast::TypeModifierSyntaxKind::DynamicArray)
                    containingType = cloneType(unit.arena(), collection, {}, collection->modifiers().size() - 1U);
                else if (collection != nullptr && collection->modifiers().empty())
                {
                    const ast::AstNode* containerSyntax = nullptr;
                    TemplateState* indexedTemplate = nullptr;
                    ast::TypeSyntax* indexedCopy = nullptr;
                    if (collection->typeArguments().empty())
                    {
                        const DeclaredTypeIdentity* container =
                            resolveDeclaredTypeIdentity(collection, unit.projectOrdinal(), namespaceName, scope);
                        if (container != nullptr) containerSyntax = container->syntax;
                    }
                    else
                    {
                        const TypeResolution resolved = resolveTypeTemplate(
                            unit, namespaceName, collection->baseName(), collection->typeArguments().size(), scope);
                        if (resolved.kind == TypeResolutionKind::Unique && resolved.state != nullptr)
                        {
                            bool conflict = false;
                            indexedCopy = cloneAliasType(unit.arena(), collection, {}, conflict);
                            if (indexedCopy != nullptr && !conflict && !hasOpenGenericParameter(indexedCopy, scope))
                            {
                                for (ast::TypeSyntax* argument : indexedCopy->typeArguments())
                                    if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics))
                                        return false;
                                if (!normalizeGenericArguments(resolved.state->record, indexedCopy->typeArguments(),
                                                               indexedCopy->range(), diagnostics)) return false;
                                indexedTemplate = resolved.state;
                                containerSyntax = indexedTemplate->syntax;
                            }
                        }
                    }
                    const ast::AstNode* indexer = nullptr;
                    const ast::AstNode* getter = nullptr;
                    bool multiple = false;
                    if (containerSyntax != nullptr && containerSyntax->typeSyntaxes().empty())
                        for (const ast::AstNode* member : containerSyntax->children())
                        {
                            if (member == nullptr || member->kind() != ast::AstNodeKind::IndexerDeclaration)
                                continue;
                            if (indexer != nullptr) { multiple = true; break; }
                            indexer = member;
                            for (const ast::AstNode* accessor : member->children())
                                if (accessor != nullptr && accessor->text() == "get_Item" &&
                                    accessor->hasModifier(lexer::TokenKind::KwPublic) &&
                                    valueParameterCount(accessor) == access->first()->children().size())
                                    getter = accessor;
                        }
                    const auto context = declaredFieldContexts_.find(containerSyntax);
                    if (!multiple && indexer != nullptr && getter != nullptr && indexer->typeSyntax() != nullptr &&
                        ((context != declaredFieldContexts_.end() && context->second.unit != nullptr) ||
                         (indexedTemplate != nullptr && indexedTemplate->unit != nullptr)))
                    {
                        if (indexedTemplate != nullptr)
                        {
                            receiverUnit = indexedTemplate->unit;
                            receiverNamespace = indexedTemplate->record.namespaceName;
                            fieldScope.importedNamespaces.write() = indexedTemplate->record.declarationImports;
                            fieldScope.aliases.write().insert(indexedTemplate->record.declarationAliases.begin(),
                                                              indexedTemplate->record.declarationAliases.end());
                        }
                        else
                        {
                            receiverUnit = context->second.unit;
                            receiverNamespace = context->second.namespaceName;
                            fieldScope.importedNamespaces.write() = context->second.imports;
                            fieldScope.aliases.write() = context->second.aliases;
                        }
                        receiverScope = &fieldScope;
                        if (indexedTemplate != nullptr && indexedCopy != nullptr)
                        {
                            std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
                            auto previousPacks = std::move(activePackSubstitutions_);
                            prepareSpecializationSubstitutions(
                                *indexedTemplate, indexedCopy->typeArguments(), substitutions);
                            containingType = cloneType(receiverUnit->arena(), indexer->typeSyntax(), substitutions);
                            activePackSubstitutions_ = std::move(previousPacks);
                        }
                        else containingType = cloneInferredType(*receiverUnit, indexer->typeSyntax());
                    }
                }
            }
            const DeclaredTypeIdentity* declaration = containingType == nullptr ? nullptr :
                resolveDeclaredTypeIdentity(
                    containingType, receiverUnit->projectOrdinal(), receiverNamespace, *receiverScope);
            TemplateState* containingTemplate = nullptr;
            ast::TypeSyntax* containingCopy = nullptr;
            if (containingType != nullptr && !containingType->typeArguments().empty())
            {
                TypeResolution containing = resolveTypeTemplate(
                    *receiverUnit, receiverNamespace, containingType->baseName(),
                    containingType->typeArguments().size(), *receiverScope);
                if (containing.kind == TypeResolutionKind::Unique && containing.state != nullptr)
                {
                    bool conflict = false;
                    containingCopy = cloneAliasType(receiverUnit->arena(), containingType, {}, conflict);
                    if (containingCopy != nullptr && !conflict &&
                        !hasOpenGenericParameter(containingCopy, *receiverScope))
                    {
                        for (ast::TypeSyntax* argument : containingCopy->typeArguments())
                            if (!processTypeSyntax(
                                    *receiverUnit, argument, receiverNamespace, *receiverScope, diagnostics)) return false;
                        if (!normalizeGenericArguments(containing.state->record, containingCopy->typeArguments(),
                                                       containingCopy->range(), diagnostics)) return false;
                        containingTemplate = containing.state;
                    }
                }
            }
            const ast::AstNode* selectedField = nullptr;
            const ast::AstNode* containingSyntax = containingTemplate != nullptr ? containingTemplate->syntax :
                (declaration != nullptr ? declaration->syntax : nullptr);
            std::unordered_set<const ast::AstNode*> visitedOwners;
            while (containingSyntax != nullptr && visitedOwners.insert(containingSyntax).second)
            {
                for (const ast::AstNode* member : containingSyntax->children())
                    if (member != nullptr && member->kind() == ast::AstNodeKind::FieldDeclaration &&
                        member->text() == access->text() && member->typeSyntax() != nullptr)
                    {
                        if (selectedField != nullptr) { selectedField = nullptr; break; }
                        selectedField = member;
                    }
                if (selectedField != nullptr) break;
                const auto ownerContext = declaredFieldContexts_.find(containingSyntax);
                if (ownerContext == declaredFieldContexts_.end() || ownerContext->second.unit == nullptr) break;
                GenericLookupScope baseScope;
                baseScope.importedNamespaces.write() = ownerContext->second.imports;
                baseScope.aliases.write() = ownerContext->second.aliases;
                const ast::AstNode* baseSyntax = nullptr;
                TemplateState* baseTemplate = nullptr;
                ast::TypeSyntax* baseCopy = nullptr;
                for (const ast::TypeSyntax* base : containingSyntax->typeSyntaxes())
                {
                    if (base != nullptr && !base->typeArguments().empty())
                    {
                        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
                        auto previousPacks = std::move(activePackSubstitutions_);
                        if (containingTemplate != nullptr && containingCopy != nullptr)
                            prepareSpecializationSubstitutions(
                                *containingTemplate, containingCopy->typeArguments(), substitutions);
                        ast::TypeSyntax* candidate = cloneType(ownerContext->second.unit->arena(), base, substitutions);
                        activePackSubstitutions_ = std::move(previousPacks);
                        TypeResolution resolved = resolveTypeTemplate(
                            *ownerContext->second.unit, ownerContext->second.namespaceName,
                            candidate->baseName(), candidate->typeArguments().size(), baseScope);
                        if (resolved.kind != TypeResolutionKind::Unique || resolved.state == nullptr ||
                            resolved.state->syntax == nullptr ||
                            resolved.state->syntax->kind() != ast::AstNodeKind::ClassDeclaration) continue;
                        if (baseSyntax != nullptr) { baseSyntax = nullptr; break; }
                        for (ast::TypeSyntax* argument : candidate->typeArguments())
                            if (!processTypeSyntax(*ownerContext->second.unit, argument,
                                                   ownerContext->second.namespaceName, baseScope, diagnostics)) return false;
                        if (!normalizeGenericArguments(resolved.state->record, candidate->typeArguments(),
                                                       candidate->range(), diagnostics)) return false;
                        baseSyntax = resolved.state->syntax;
                        baseTemplate = resolved.state;
                        baseCopy = candidate;
                        continue;
                    }
                    const DeclaredTypeIdentity* resolved = resolveDeclaredTypeIdentity(
                        base, ownerContext->second.unit->projectOrdinal(), ownerContext->second.namespaceName, baseScope);
                    if (resolved == nullptr || resolved->syntax == nullptr ||
                        resolved->syntax->kind() != ast::AstNodeKind::ClassDeclaration) continue;
                    if (baseSyntax != nullptr) { baseSyntax = nullptr; break; }
                    baseSyntax = resolved->syntax;
                }
                containingSyntax = baseSyntax;
                containingTemplate = baseTemplate;
                containingCopy = baseCopy;
            }
            if (selectedField != nullptr)
            {
                const auto context = declaredFieldContexts_.find(selectedField);
                if (context != declaredFieldContexts_.end() && context->second.unit != nullptr)
                {
                    receiverUnit = context->second.unit;
                    receiverNamespace = context->second.namespaceName;
                    fieldScope.importedNamespaces.write() = context->second.imports;
                    fieldScope.aliases.write() = context->second.aliases;
                }
                else if (containingTemplate != nullptr && containingTemplate->unit != nullptr)
                {
                    receiverUnit = containingTemplate->unit;
                    receiverNamespace = containingTemplate->record.namespaceName;
                    fieldScope.importedNamespaces.write() = containingTemplate->record.declarationImports;
                    fieldScope.aliases.write().insert(containingTemplate->record.declarationAliases.begin(),
                                                      containingTemplate->record.declarationAliases.end());
                }
                else selectedField = nullptr;
                if (selectedField != nullptr)
                {
                    receiverScope = &fieldScope;
                    if (containingTemplate != nullptr && containingCopy != nullptr)
                    {
                        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
                        auto previousPacks = std::move(activePackSubstitutions_);
                        prepareSpecializationSubstitutions(
                            *containingTemplate, containingCopy->typeArguments(), substitutions);
                        actual = cloneType(receiverUnit->arena(), selectedField->typeSyntax(), substitutions);
                        activePackSubstitutions_ = std::move(previousPacks);
                    }
                    else actual = cloneInferredType(*receiverUnit, selectedField->typeSyntax());
                }
            }
        }
        if (actual == nullptr && hasGenericOwner && callee->first() != nullptr)
            actual = callee->first()->typeSyntax();
        if (actual != nullptr && !actual->typeArguments().empty())
        {
            TypeResolution owner = resolveTypeTemplate(
                *receiverUnit, receiverNamespace, actual->baseName(), actual->typeArguments().size(), *receiverScope);
            if (owner.kind == TypeResolutionKind::Unique && owner.state != nullptr)
            {
                bool conflict = false;
                ast::TypeSyntax* receiverCopy = cloneAliasType(receiverUnit->arena(), actual, {}, conflict);
                if (receiverCopy != nullptr && !conflict && !hasOpenGenericParameter(receiverCopy, *receiverScope))
                {
                    for (ast::TypeSyntax* argument : receiverCopy->typeArguments())
                        if (!processTypeSyntax(*receiverUnit, argument, receiverNamespace, *receiverScope, diagnostics)) return false;
                    if (!normalizeGenericArguments(owner.state->record, receiverCopy->typeArguments(),
                                                   receiverCopy->range(), diagnostics) ||
                        !materializeTypeSyntax(*receiverUnit, receiverCopy, *owner.state, receiverNamespace, diagnostics,
                                               &closedOwnerKey))
                        return false;
                }
                if (!closedOwnerKey.empty())
                    if (const auto found = specializationPlanner_.request(closedOwnerKey))
                    {
                        const GenericSpecializationRecord& instance = specializations_[*found];
                        closedOwner = const_cast<ast::AstNode*>(instance.specializedSyntax);
                        if (closedOwner == nullptr && instance.templateOrdinal < templateStates_.size())
                        {
                            const GenericTemplateRecord& ownerRecord = templateStates_[instance.templateOrdinal].record;
                            const std::string physicalName = ownerRecord.moduleName + "::" +
                                qualifiedName(ownerRecord.namespaceName,
                                              appendOwner(ownerRecord.ownerName, instance.specializedName));
                            const DeclaredTypeIdentity* physical = resolveModuleQualifiedDeclaredTypeIdentity(
                                physicalName, receiverUnit->projectOrdinal());
                            if (physical != nullptr)
                                closedOwner = const_cast<ast::AstNode*>(physical->syntax);
                        }
                    }
            }
        }
        const ast::AstNode* receiverDeclaration = nullptr;
        const DeclaredTypeIdentity* ordinaryReceiver = nullptr;
        if (actual != nullptr && !actual->typeArguments().empty())
        {
            TypeResolution owner = resolveTypeTemplate(
                *receiverUnit, receiverNamespace, actual->baseName(), actual->typeArguments().size(), *receiverScope);
            if (owner.kind == TypeResolutionKind::Unique && owner.state != nullptr)
                receiverDeclaration = owner.state->syntax;
        }
        else if (actual != nullptr)
        {
            ordinaryReceiver = resolveDeclaredTypeIdentity(actual, unit.projectOrdinal(), namespaceName, scope);
            if (ordinaryReceiver != nullptr && ordinaryReceiver->syntax != nullptr)
                for (const GenericSpecializationRecord& imported : specializations_)
                {
                    if (imported.specializedSyntax != nullptr || imported.templateSyntax == nullptr ||
                        imported.templateOrdinal >= templateStates_.size() ||
                        imported.specializedName != ordinaryReceiver->syntax->text()) continue;
                    const GenericTemplateRecord& owner = templateStates_[imported.templateOrdinal].record;
                    const std::string expected = owner.moduleName + "::" +
                        qualifiedName(owner.namespaceName, appendOwner(owner.ownerName, imported.specializedName));
                    if (expected != ordinaryReceiver->identityKey) continue;
                    receiverDeclaration = imported.templateSyntax;
                    closedOwner = const_cast<ast::AstNode*>(ordinaryReceiver->syntax);
                    closedOwnerKey = imported.specializationKey;
                    ordinaryReceiver = nullptr;
                    break;
                }
        }
        if (receiverDeclaration != nullptr || ordinaryReceiver != nullptr)
        {
            resolution = GenericCallResolution{};
            std::vector<TemplateState*> sameOwnerAndArguments;
            for (TemplateState* candidate : bucket->second)
            {
                const bool sameOwner = receiverDeclaration != nullptr
                    ? candidate->parent == receiverDeclaration
                    : candidate->parent != nullptr && candidate->parent->genericParameters().empty() &&
                        candidate->record.projectOrdinal == ordinaryReceiver->projectOrdinal &&
                        templateSourceSpelling(candidate->record.namespaceName, candidate->record.ownerName) ==
                            ordinaryReceiver->sourceName;
                if (!sameOwner ||
                    !templateVisible(unit.projectOrdinal(), candidate->record) ||
                    valueParameterCount(candidate->syntax) != argumentCount) continue;
                sameOwnerAndArguments.push_back(candidate);
                if (!genericArityAccepts(candidate->record, call->typeSyntaxes().size())) continue;
                resolution.named.push_back(candidate);
            }
            resolution.shared = collectMethodTemplatesFromNamed(resolution.named);
            if (resolution.shared.size() == 1U) resolution.unique = resolution.shared.front();
            else if (resolution.named.empty() && sameOwnerAndArguments.size() == 1U)
                resolution.unique = sameOwnerAndArguments.front();
        }
    }
    const std::vector<TemplateState*>& namedAtWinningRung = resolution.named;
    const std::vector<TemplateState*>& shared = resolution.shared;
    if (shared.size() > 1U)
    {
        return materializeSharedOverloads(
            unit, call, callee, methodName, namespaceName, shared, closedOwner, closedOwnerKey, diagnostics);
    }

    TemplateState* state = resolution.unique;
    if (state == nullptr)
    {
        const bool ambiguous = namedAtWinningRung.size() > 1U;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            ambiguous
                ? "generic call '" + methodName + "<...>' is ambiguous between " +
                    std::to_string(namedAtWinningRung.size()) +
                    " visible Stage 44 generic method templates"
                : "generic call '" + methodName +
                    "<...>' has no visible Stage 44 generic method template");
        return false;
    }
    if (!genericArityAccepts(state->record, call->typeSyntaxes().size()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "generic method '" + methodName + "' expects " +
                (trailingPackIndex(state->record).has_value()
                    ? "at least " + std::to_string(*trailingPackIndex(state->record))
                    : std::to_string(state->record.parameters.size())) +
                " type argument(s) but received " +
                std::to_string(call->typeSyntaxes().size()));
        return false;
    }
    return materializeUniqueMethod(unit, call, callee, *state, namespaceName,
                                   closedOwner, closedOwnerKey, diagnostics);
}
}
