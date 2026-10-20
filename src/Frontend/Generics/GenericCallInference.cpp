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
    for (std::size_t index = 0; viable && index < call->children().size(); ++index)
    {
        ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
        const ast::TypeSyntax* pattern = parameters[*mapping[index]]->typeSyntax();
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
            unit, candidate.syntax->typeSyntax(), expectedType,
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
    std::vector<const ast::AstNode*> parameters;
    std::vector<std::optional<std::size_t>> mapping;
    bool extensionCandidate = false;
    if (!mapGenericCallArguments(
            call, candidate, parameters, mapping, extensionCandidate))
        return false;
    const std::unordered_set<std::string> genericParameters(
        candidate.record.parameters.begin(), candidate.record.parameters.end());
    std::unordered_map<std::string, ast::TypeSyntax*> inferred;
    if (!inferGenericCandidateEvidence(
            unit, call, candidate, parameters, mapping, extensionCandidate,
            genericParameters, scope, expectedType, inferred))
        return false;
    if (candidateCount == 1U &&
        !applyGenericCandidateContext(
            unit, call, candidate, parameters, mapping, genericParameters,
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
                *candidate, call->typeSyntaxes(), diagnostics, sharedName).empty())
        {
            currentOverloadGroup_ = 0;
            return false;
        }
    }
    currentOverloadGroup_ = 0;
    if (callee != nullptr) callee->setText(sharedName);
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
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!normalizeGenericArguments(
            state.record, call->typeSyntaxes(), call->range(), diagnostics))
        return false;

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    currentInstantiationRange_ = call->range();
    const std::string specialized = ensureMethodSpecialization(
        state, call->typeSyntaxes(), diagnostics);
    if (specialized.empty()) return false;
    if (callee != nullptr) callee->setText(specialized);
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

    const GenericCallResolution resolution = resolveGenericCall(
        unit, site, requestedOwner, methodName, call->typeSyntaxes().size(), argumentCount);
    const std::vector<TemplateState*>& namedAtWinningRung = resolution.named;
    const std::vector<TemplateState*>& shared = resolution.shared;
    if (shared.size() > 1U)
    {
        return materializeSharedOverloads(
            unit, call, callee, methodName, namespaceName, shared, diagnostics);
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
    return materializeUniqueMethod(unit, call, callee, *state, namespaceName, diagnostics);
}
}
