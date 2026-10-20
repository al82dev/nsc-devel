#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

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
using lexer::TokenKind;
using method_body_detail::pointerContract;

bool ProjectMethodBodyModel::collectDeconstructCandidates(
    const ast::AstNode* syntax,
    const ast::AstNode* sourceSyntax,
    const ExpressionResult& source,
    std::size_t receiverType,
    const ClassMemberSymbol& caller,
    bool& namedCandidate,
    bool& inaccessibleCandidate,
    std::vector<std::size_t>& validCandidates,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const std::size_t ordinal : memberCandidatesForType(receiverType, "Deconstruct"))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        namedCandidate = true;
        if (!ProjectMemberAccessibility::isAccessible(
                member, caller.declaringTypeOrdinal, receiverType,
                *nameLookup_, *inheritanceModel_))
        {
            inaccessibleCandidate = true;
            continue;
        }
        if (member.kind != ClassMemberKind::Method || member.isStatic ||
            !member.parameters.empty() || member.isReferenceReturn || member.isVirtual ||
            member.isAbstract ||
            !ProjectTypeConversion::tupleElementTypeKeys(member.declaredTypeKey).has_value())
        {
            continue;
        }
        if (source.readonlyReceiver && !member.isReadonly)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                sourceSyntax != nullptr ? sourceSyntax->range() : syntax->range(),
                "readonly deconstruction source requires a readonly Deconstruct() method");
            return false;
        }
        validCandidates.push_back(ordinal);
    }
    return true;
}

bool ProjectMethodBodyModel::validateSelectedDeconstruct(
    const ast::AstNode* syntax,
    const ast::AstNode* sourceSyntax,
    const ExpressionResult& source,
    const ClassMemberSymbol& selected,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!elementTypeHasTrivialLifetime(selected.declaredTypeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            sourceSyntax != nullptr ? sourceSyntax->range() : syntax->range(),
            "Deconstruct() returning owning or move-only tuple elements requires per-element "
            "move bindings and drop flags");
        return false;
    }
    if (!source.addressable && !elementTypeHasTrivialLifetime(source.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            sourceSyntax != nullptr ? sourceSyntax->range() : syntax->range(),
            "deconstructing a temporary move-only nominal value would require a hidden copy; "
            "bind it to storage before deconstruction");
        return false;
    }
    return true;
}

std::optional<MethodDeconstructionBinding> ProjectMethodBodyModel::bindDeconstructionSource(
    const ast::AstNode* syntax,
    const ast::AstNode* sourceSyntax,
    const ExpressionResult& source,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!source.valid) return std::nullopt;
    MethodDeconstructionBinding binding;
    binding.syntax = syntax;
    binding.sourceSyntax = sourceSyntax;
    binding.sourceTypeKey = source.typeKey;
    if (ProjectTypeConversion::tupleElementTypeKeys(source.typeKey).has_value())
    {
        binding.tupleTypeKey = source.typeKey;
        return binding;
    }

    const std::optional<std::size_t> receiverType =
        accessorReceiverTypeOrdinalForTypeKey(source.typeKey);
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (!receiverType.has_value() || caller == nullptr || memberModel_ == nullptr ||
        nameLookup_ == nullptr || inheritanceModel_ == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            sourceSyntax != nullptr ? sourceSyntax->range() : syntax->range(),
            "deconstruction requires a tuple or a nominal value with an accessible "
            "parameterless Deconstruct() method returning a tuple");
        return std::nullopt;
    }

    bool namedCandidate = false;
    bool inaccessibleCandidate = false;
    std::vector<std::size_t> validCandidates;
    if (!collectDeconstructCandidates(
            syntax, sourceSyntax, source, *receiverType, *caller,
            namedCandidate, inaccessibleCandidate, validCandidates, diagnostics))
    {
        return std::nullopt;
    }
    if (validCandidates.size() != 1)
    {
        const diagnostics::DiagnosticId id = validCandidates.size() > 1
            ? diagnostics::DiagnosticId::InvalidCall
            : inaccessibleCandidate && namedCandidate
                ? diagnostics::DiagnosticId::InaccessibleMember
                : diagnostics::DiagnosticId::InvalidOperand;
        const std::string message = validCandidates.size() > 1
            ? "nominal deconstruction has more than one valid Deconstruct() method"
            : inaccessibleCandidate && namedCandidate
                ? "Deconstruct() is not accessible in this context"
                : "nominal deconstruction requires one non-static, non-virtual, parameterless "
                  "Deconstruct() method returning a tuple by value";
        diagnostics.report(
            id,
            diagnostics::DiagnosticSeverity::Error,
            sourceSyntax != nullptr ? sourceSyntax->range() : syntax->range(),
            message);
        return std::nullopt;
    }

    const ClassMemberSymbol& selected = memberModel_->members()[validCandidates.front()];
    if (!validateSelectedDeconstruct(
            syntax, sourceSyntax, source, selected, diagnostics)) return std::nullopt;
    binding.tupleTypeKey = selected.declaredTypeKey;
    binding.selectedMemberOrdinal = validCandidates.front();
    return binding;
}
void ProjectMethodBodyModel::decodeMatchCoverageArm(
    std::string_view armText,
    std::vector<language::MatchArmCoverage>& coverageArms) const
{
    language::MatchArmCoverage arm;
    if (!armText.empty() && armText.front() == '~')
    {
        arm.hasGuard = true;
        armText.remove_prefix(1);
    }
    arm.wildcard = armText == "*";
    if (!arm.wildcard && armText != "?" && !armText.empty() && armText.front() == '#')
    {
        std::size_t lengthBegin = 0;
        while (lengthBegin <= armText.size())
        {
            const std::size_t lengthEnd = armText.find('|', lengthBegin);
            std::string_view item = armText.substr(
                lengthBegin,
                lengthEnd == std::string_view::npos
                    ? armText.size() - lengthBegin : lengthEnd - lengthBegin);
            if (!item.empty() && item.front() == '#') item.remove_prefix(1);
            const bool hasRest = !item.empty() && item.back() == '+';
            if (hasRest) item.remove_suffix(1);
            arm.listLengths.push_back({
                static_cast<std::size_t>(std::stoull(std::string(item))), hasRest});
            if (lengthEnd == std::string_view::npos) break;
            lengthBegin = lengthEnd + 1;
        }
    }
    else if (!arm.wildcard && armText != "?")
    {
        std::size_t caseBegin = 0;
        while (caseBegin <= armText.size())
        {
            const std::size_t caseEnd = armText.find('|', caseBegin);
            const std::string_view name = armText.substr(
                caseBegin,
                caseEnd == std::string_view::npos
                    ? armText.size() - caseBegin : caseEnd - caseBegin);
            if (!name.empty()) arm.cases.emplace_back(name);
            if (caseEnd == std::string_view::npos) break;
            caseBegin = caseEnd + 1;
        }
    }
    coverageArms.push_back(std::move(arm));
}

void ProjectMethodBodyModel::decodeMatchCoverage(
    const ast::AstNode* expression,
    std::vector<language::MatchArmCoverage>& coverageArms) const
{
    constexpr std::string_view coveragePrefix = "match-coverage:v1:";
    if (!expression->auxiliaryText().starts_with(coveragePrefix)) return;
    std::string_view encoded(expression->auxiliaryText());
    encoded.remove_prefix(coveragePrefix.size());
    std::size_t begin = 0;
    while (begin <= encoded.size())
    {
        const std::size_t end = encoded.find(';', begin);
        decodeMatchCoverageArm(
            encoded.substr(
                begin,
                end == std::string_view::npos ? encoded.size() - begin : end - begin),
            coverageArms);
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
}

void ProjectMethodBodyModel::collectMatchDomainNames(
    std::size_t typeOrdinal,
    const ast::AstNode* declaration,
    std::vector<std::string>& domain) const
{
    constexpr std::string_view casesMarker = "cases=";
    if (declaration != nullptr && declaration->auxiliaryText().starts_with("variant,"))
    {
        const std::string_view metadata(declaration->auxiliaryText());
        const std::size_t casesBegin = metadata.find(casesMarker);
        if (casesBegin != std::string_view::npos)
        {
            std::string_view cases = metadata.substr(casesBegin + casesMarker.size());
            std::size_t caseBegin = 0;
            while (caseBegin <= cases.size())
            {
                const std::size_t caseEnd = cases.find('|', caseBegin);
                std::string_view item = cases.substr(
                    caseBegin,
                    caseEnd == std::string_view::npos
                        ? cases.size() - caseBegin : caseEnd - caseBegin);
                if (const std::size_t payload = item.find(':');
                    payload != std::string_view::npos) item = item.substr(0, payload);
                if (!item.empty()) domain.emplace_back(item);
                if (caseEnd == std::string_view::npos) break;
                caseBegin = caseEnd + 1;
            }
        }
    }
    if (!domain.empty() || memberModel_ == nullptr) return;
    constexpr std::string_view factoryPrefix = "variant-case-factory:";
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.syntax == nullptr ||
            !member.syntax->auxiliaryText().starts_with(factoryPrefix)) continue;
        std::string_view marker(member.syntax->auxiliaryText());
        marker.remove_prefix(factoryPrefix.size());
        if (const std::size_t tag = marker.rfind(':');
            tag != std::string_view::npos) marker = marker.substr(0, tag);
        if (!marker.empty() && std::ranges::find(domain, marker) == domain.end())
        {
            domain.emplace_back(marker);
        }
    }
}

void ProjectMethodBodyModel::collectMatchDomainTags(
    std::size_t typeOrdinal,
    std::span<const std::string> domain,
    std::optional<std::size_t>& switchTagMemberOrdinal,
    std::unordered_map<std::string, std::int64_t>& variantCaseTags) const
{
    for (std::size_t index = 0; index < domain.size(); ++index)
    {
        variantCaseTags.emplace(domain[index], static_cast<std::int64_t>(index));
    }
    if (memberModel_ == nullptr) return;
    constexpr std::string_view factoryPrefix = "variant-case-factory:";
    constexpr std::string_view testPrefix = "variant-case-test:";
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind == ClassMemberKind::Field && member.name == "__nsc_variant_tag")
        {
            switchTagMemberOrdinal = memberOrdinal;
            continue;
        }
        if (member.syntax == nullptr) continue;
        std::string_view marker(member.syntax->auxiliaryText());
        std::string_view prefix;
        if (marker.starts_with(factoryPrefix)) prefix = factoryPrefix;
        else if (marker.starts_with(testPrefix)) prefix = testPrefix;
        else continue;
        marker.remove_prefix(prefix.size());
        const std::size_t separator = marker.rfind(':');
        if (separator == std::string_view::npos || separator == 0U) continue;
        std::int64_t tag = 0;
        const auto parsed = std::from_chars(
            marker.data() + separator + 1U, marker.data() + marker.size(), tag);
        if (parsed.ec == std::errc{} && parsed.ptr == marker.data() + marker.size())
        {
            variantCaseTags[std::string(marker.substr(0, separator))] = tag;
        }
    }
}

void ProjectMethodBodyModel::collectMatchDomain(
    std::string_view typeKey,
    std::vector<std::string>& domain,
    std::optional<std::size_t>& switchTagMemberOrdinal,
    std::unordered_map<std::string, std::int64_t>& variantCaseTags) const
{
    if (nameLookup_ == nullptr) return;
    const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value() || *typeOrdinal >= nameLookup_->namedTypes().size()) return;
    collectMatchDomainNames(
        *typeOrdinal, nameLookup_->namedTypes()[*typeOrdinal].syntax, domain);
    collectMatchDomainTags(
        *typeOrdinal, domain, switchTagMemberOrdinal, variantCaseTags);
}

void ProjectMethodBodyModel::validateOpenMatchCoverage(
    const ast::AstNode* expression,
    std::span<const language::MatchArmCoverage> coverageArms,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool hasListLength = std::ranges::any_of(
        coverageArms, [](const language::MatchArmCoverage& arm)
        {
            return !arm.listLengths.empty();
        });
    if (!hasListLength)
    {
        if (!std::ranges::any_of(
                coverageArms, [](const language::MatchArmCoverage& arm)
                {
                    return arm.wildcard && !arm.hasGuard;
                }))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "match over an open or unresolved domain requires a final wildcard or binding arm");
        }
        return;
    }
    const language::ExhaustivenessResult result = language::analyzeListMatch(coverageArms);
    for (const std::size_t arm : result.unreachableArms)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "match arm #" + std::to_string(arm + 1) +
                " is unreachable for the list-length domain");
    }
    if (result.exhaustive) return;
    std::string message = "non-exhaustive match; missing list length";
    if (result.missingCases.empty())
    {
        message += ": a list pattern needs a rest or wildcard arm to cover unbounded lengths";
    }
    else
    {
        message += result.missingCases.size() == 1 ? ": " : "s: ";
        for (std::size_t index = 0; index < result.missingCases.size(); ++index)
        {
            if (index != 0) message += ", ";
            message += result.missingCases[index];
        }
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        message);
}

void ProjectMethodBodyModel::validateClosedMatchCoverage(
    const ast::AstNode* expression,
    std::span<const std::string> domain,
    std::span<const language::MatchArmCoverage> coverageArms,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const language::ExhaustivenessResult coverage =
        language::analyzeClosedMatch(domain, coverageArms);
    for (const std::size_t arm : coverage.unreachableArms)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "match arm #" + std::to_string(arm + 1) +
                " is unreachable for the closed variant domain");
    }
    if (coverage.exhaustive) return;
    std::string message = "non-exhaustive match; missing variant case";
    message += coverage.missingCases.size() == 1 ? ": " : "s: ";
    for (std::size_t index = 0; index < coverage.missingCases.size(); ++index)
    {
        if (index != 0) message += ", ";
        message += coverage.missingCases[index];
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        std::move(message));
}

void ProjectMethodBodyModel::validateMatchCoverage(
    const ast::AstNode* expression,
    std::span<const std::string> domain,
    std::span<const language::MatchArmCoverage> coverageArms,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (coverageArms.empty()) return;
    if (domain.empty())
    {
        validateOpenMatchCoverage(expression, coverageArms, diagnostics);
        return;
    }
    validateClosedMatchCoverage(expression, domain, coverageArms, diagnostics);
}

bool ProjectMethodBodyModel::analyzeMatchArmHeader(
    const ast::AstNode* arm,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    OwnershipFlowSnapshot& remaining,
    std::size_t& armScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    restoreOwnershipFlow(remaining);
    const ExpressionResult structural = analyzeExpression(
        arm->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (structural.valid && structural.typeKey != "builtin:bool" &&
        structural.typeKey != "builtin:never")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            arm->first()->range(),
            "a match arm pattern must produce bool");
    }
    OwnershipFlowSnapshot selected = captureOwnershipFlow();
    if (const auto flow = conditionFlowBySyntax_.find(arm->first());
        flow != conditionFlowBySyntax_.end())
    {
        selected = flow->second.whenTrue;
        remaining = flow->second.whenFalse;
    }
    restoreOwnershipFlow(selected);
    armScope = createScope(MethodScopeKind::Block, methodMemberOrdinal, scopeOrdinal);
    for (const ast::AstNode* binding : arm->children())
    {
        // NS304-T03-F04: los bindings del arm incluyen DeconstructionDeclarationStatement
        // (posicional/tuple); analyzeStatement maneja tanto variables como deconstruccion.
        analyzeStatement(binding, methodMemberOrdinal, armScope, diagnostics);
    }
    if (arm->second() == nullptr) return true;
    const ExpressionResult guard = analyzeExpression(
        arm->second(), methodMemberOrdinal, armScope, diagnostics);
    if (guard.valid && guard.typeKey != "builtin:bool" && guard.typeKey != "builtin:never")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            arm->second()->range(),
            "a match arm guard must have type bool");
    }
    OwnershipFlowSnapshot guarded = captureOwnershipFlow();
    if (const auto flow = conditionFlowBySyntax_.find(arm->second());
        flow != conditionFlowBySyntax_.end())
    {
        guarded = flow->second.whenTrue;
        remaining = joinOwnershipFlow(remaining, flow->second.whenFalse);
    }
    restoreOwnershipFlow(guarded);
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMatchExpressionArm(
    const ast::AstNode* arm,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    OwnershipFlowSnapshot& remaining,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::size_t armScope = 0;
    analyzeMatchArmHeader(
        arm, methodMemberOrdinal, scopeOrdinal, remaining, armScope, diagnostics);
    ExpressionResult value = analyzeExpression(
        arm->third(), methodMemberOrdinal, armScope, diagnostics,
        /*allowMethodGroup=*/false, /*allowEvent=*/false,
        /*allowMove=*/true);
    if (arm->flag() && value.valid && (!value.assignable || !value.addressable))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            arm->third()->range(),
            "a ref match arm must yield an addressable storage location");
        value.valid = false;
    }
    return value;
}

bool ProjectMethodBodyModel::analyzeMatchExpressionArms(
    const ast::AstNode* expression,
    const ExpressionResult& governing,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::vector<ExpressionResult>& values,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const OwnershipFlowSnapshot entry = captureOwnershipFlow();
    OwnershipFlowSnapshot remaining = entry;
    std::vector<OwnershipFlowSnapshot> exits;
    values.reserve(expression->children().size());
    exits.reserve(expression->children().size());
    activeMatchValues_.push_back(governing);
    for (const ast::AstNode* arm : expression->children())
    {
        if (arm == nullptr || arm->kind() != ast::AstNodeKind::MatchArmExpression ||
            arm->first() == nullptr || arm->third() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "malformed match expression arm");
            values.push_back(ExpressionResult{});
            continue;
        }
        values.push_back(analyzeMatchExpressionArm(
            arm, methodMemberOrdinal, scopeOrdinal, remaining, diagnostics));
        exits.push_back(captureOwnershipFlow());
    }
    activeMatchValues_.pop_back();
    restoreMatchExitFlow(entry, exits);
    return !values.empty() && !std::ranges::any_of(
        values, [](const ExpressionResult& value)
        {
            return !value.valid;
        });
}

void ProjectMethodBodyModel::restoreMatchExitFlow(
    const OwnershipFlowSnapshot& entry,
    std::span<const OwnershipFlowSnapshot> exits)
{
    if (exits.empty())
    {
        restoreOwnershipFlow(entry);
        return;
    }
    OwnershipFlowSnapshot joined = exits.front();
    for (std::size_t index = 1; index < exits.size(); ++index)
    {
        joined = joinOwnershipFlow(joined, exits[index]);
    }
    restoreOwnershipFlow(joined);
}

bool ProjectMethodBodyModel::validateMatchReferenceResults(
    const ast::AstNode* expression,
    bool& referenceResult,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    referenceResult = expression->children().front()->flag();
    if (!std::ranges::any_of(
            expression->children(), [&](const ast::AstNode* arm)
            {
                return arm != nullptr && arm->flag() != referenceResult;
            }))
    {
        return true;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::NS3020InvalidValueCategory,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "match expression arms cannot mix ref and value results");
    return false;
}

bool ProjectMethodBodyModel::resolveMatchResultType(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> values,
    bool referenceResult,
    std::string& commonType,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const ExpressionResult& value : values)
    {
        if (value.typeKey == "builtin:never") continue;
        if (commonType.empty())
        {
            commonType = value.typeKey;
            continue;
        }
        if (value.typeKey == commonType) continue;
        if (referenceResult)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "all ref match arms must have the same storage type");
            return false;
        }
        const std::optional<std::string> numeric = ProjectTypeConversion::commonNumericType(
            commonType, value.typeKey, std::nullopt, value.integerConstant);
        if (numeric.has_value())
        {
            commonType = *numeric;
            continue;
        }
        if (classifyImplicit(value.typeKey, commonType, value.integerConstant).exists()) continue;
        if (classifyImplicit(commonType, value.typeKey).exists())
        {
            commonType = value.typeKey;
            continue;
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "match expression arms have no common type between '" + commonType +
                "' and '" + value.typeKey + "'");
        return false;
    }
    if (commonType.empty()) commonType = "builtin:never";
    return true;
}

bool ProjectMethodBodyModel::matchExpressionContainsRestView(const ast::AstNode* node) const
{
    if (node == nullptr) return false;
    if (node->kind() == ast::AstNodeKind::CallExpression &&
        node->auxiliaryText() == "list-pattern-rest")
    {
        return true;
    }
    if (matchExpressionContainsRestView(node->first()) ||
        matchExpressionContainsRestView(node->second()) ||
        matchExpressionContainsRestView(node->third()))
    {
        return true;
    }
    return std::ranges::any_of(
        node->children(),
        [&](const ast::AstNode* child)
        {
            return matchExpressionContainsRestView(child);
        });
}

void ProjectMethodBodyModel::recordMatchExpressionBinding(
    const ast::AstNode* expression,
    const ExpressionResult& governing,
    std::string_view commonType,
    bool referenceResult,
    std::span<const language::MatchArmCoverage> coverageArms,
    std::optional<std::size_t> switchTagMemberOrdinal,
    const std::unordered_map<std::string, std::int64_t>& variantCaseTags,
    std::span<const ExpressionResult> values)
{
    MethodMatchExpressionBinding binding;
    binding.syntax = expression;
    binding.governingTypeKey = governing.typeKey;
    binding.resultTypeKey = commonType;
    binding.referenceResult = referenceResult;
    const bool fixedArrayGoverning =
        ProjectMemberModel::fixedArrayElementTypeKey(governing.typeKey).has_value();
    // A fixed array is an inline value. Snapshotting it would make a synthesized Span rest view
    // point at the snapshot instead of the caller-visible storage. Reuse the existing governing
    // address contract only when an addressable fixed-array match actually contains such a view;
    // ordinary value matches keep their one-time value snapshot.
    const bool fixedArrayRestView = fixedArrayGoverning && governing.addressable &&
        matchExpressionContainsRestView(expression);
    binding.governingReference = expression->flag() || fixedArrayRestView;
    if (switchTagMemberOrdinal.has_value() &&
        coverageArms.size() == expression->children().size() &&
        std::ranges::all_of(
            coverageArms, [](const language::MatchArmCoverage& arm)
            {
                return !arm.hasGuard && !arm.wildcard && !arm.cases.empty();
            }))
    {
        binding.armTags.resize(coverageArms.size());
        bool completeTagMap = true;
        for (std::size_t armIndex = 0; armIndex < coverageArms.size(); ++armIndex)
        {
            for (const std::string& caseName : coverageArms[armIndex].cases)
            {
                const auto tag = variantCaseTags.find(caseName);
                if (tag == variantCaseTags.end())
                {
                    completeTagMap = false;
                    break;
                }
                binding.armTags[armIndex].push_back(tag->second);
            }
            if (!completeTagMap) break;
        }
        if (completeTagMap) binding.switchTagMemberOrdinal = switchTagMemberOrdinal;
        else binding.armTags.clear();
    }
    binding.armConversions.reserve(values.size());
    for (const ExpressionResult& value : values)
    {
        if (value.typeKey == "builtin:never" || referenceResult)
        {
            binding.armConversions.push_back(ProjectImplicitConversion{});
        }
        else
        {
            binding.armConversions.push_back(
                classifyImplicit(value.typeKey, commonType, value.integerConstant));
        }
    }
    matchExpressionBindingBySyntax_[expression] = matchExpressionBindings_.size();
    matchExpressionBindings_.push_back(std::move(binding));
}

void ProjectMethodBodyModel::applyMatchExpressionOwnership(
    ExpressionResult& result,
    std::span<const ExpressionResult> values) const
{
    const sema::OwnershipKind ownership = values.front().ownership;
    if (!std::ranges::all_of(
            values, [&](const ExpressionResult& value)
            {
                return value.typeKey == "builtin:never" || value.ownership == ownership;
            }))
    {
        return;
    }
    result.ownership = ownership;
    result.ownershipTransfer = std::ranges::all_of(
        values, [](const ExpressionResult& value)
        {
            return value.typeKey == "builtin:never" || value.ownershipTransfer;
        });
    result.ownershipFreshOwner = std::ranges::all_of(
        values, [](const ExpressionResult& value)
        {
            return value.typeKey == "builtin:never" || value.ownershipFreshOwner;
        });
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeMatchExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (expression == nullptr || expression->first() == nullptr ||
        expression->children().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression == nullptr ? SourceRange() : expression->range(),
            "a match expression requires a governing value and at least one arm");
        return result;
    }
    const ExpressionResult governing = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (!governing.valid) return result;

    // Exhaustiveness is a semantic property of the governing type.  Statement-match originally
    // recovered that type through its compiler-generated local; an expression already has the
    // typed governing value, so it must not depend on a synthetic name being visible.
    std::vector<language::MatchArmCoverage> coverageArms;
    decodeMatchCoverage(expression, coverageArms);
    std::vector<std::string> domain;
    std::optional<std::size_t> switchTagMemberOrdinal;
    std::unordered_map<std::string, std::int64_t> variantCaseTags;
    collectMatchDomain(
        governing.typeKey, domain, switchTagMemberOrdinal, variantCaseTags);
    validateMatchCoverage(expression, domain, coverageArms, diagnostics);

    std::vector<ExpressionResult> values;
    if (!analyzeMatchExpressionArms(
            expression, governing, methodMemberOrdinal, scopeOrdinal, values, diagnostics))
    {
        return result;
    }
    bool referenceResult = false;
    if (!validateMatchReferenceResults(
            expression, referenceResult, diagnostics)) return result;
    std::string commonType;
    if (!resolveMatchResultType(
            expression, values, referenceResult, commonType, diagnostics))
    {
        return result;
    }
    recordMatchExpressionBinding(
        expression, governing, commonType, referenceResult, coverageArms,
        switchTagMemberOrdinal, variantCaseTags, values);
    result.typeKey = commonType;
    result.valid = true;
    result.assignable = referenceResult;
    result.addressable = referenceResult;
    result.readonlyReceiver = referenceResult && std::ranges::any_of(
        values, [](const ExpressionResult& value)
        {
            return value.readonlyReceiver;
        });
    applyMatchExpressionOwnership(result, values);
    return result;
}

}
