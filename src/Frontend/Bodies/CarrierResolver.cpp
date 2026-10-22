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

bool ProjectMethodBodyModel::scanResultCarrierContracts(
    std::size_t typeOrdinal,
    std::string_view typeKey,
    std::array<std::string, 2>& payloadTypeKeys,
    std::array<std::optional<std::size_t>, 2>& tags,
    std::array<std::optional<std::size_t>, 2>& factories,
    std::array<std::optional<std::size_t>, 2>& tests,
    std::array<std::optional<std::size_t>, 2>& extractors,
    bool& sawUnexpectedCase) const
{
    constexpr std::string_view factoryPrefix = "variant-case-factory:";
    constexpr std::string_view testPrefix = "variant-case-test:";
    constexpr std::string_view extractorPrefix = "variant-case-extract:";
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method || member.syntax == nullptr) continue;
        const std::string_view marker = member.syntax->auxiliaryText();
        const std::string_view prefix = marker.starts_with(factoryPrefix)
            ? factoryPrefix : marker.starts_with(testPrefix)
                ? testPrefix : extractorPrefix;
        const auto decoded = decodeResultCarrierMarker(marker, prefix);
        if (!decoded.has_value()) continue;
        const std::optional<std::size_t> index = decoded->first == "Ok"
            ? std::optional<std::size_t>{0U}
            : decoded->first == "Error" ? std::optional<std::size_t>{1U} : std::nullopt;
        if (!index.has_value())
        {
            if (prefix == factoryPrefix) sawUnexpectedCase = true;
            continue;
        }
        if (prefix == factoryPrefix)
        {
            if (factories[*index].has_value() || !member.isStatic ||
                member.parameters.size() != 1U || member.declaredTypeKey != typeKey ||
                (tags[*index].has_value() && *tags[*index] != decoded->second)) return false;
            factories[*index] = memberOrdinal;
            payloadTypeKeys[*index] = member.parameters.front().typeKey;
        }
        else if (prefix == testPrefix)
        {
            if (tests[*index].has_value() || member.isStatic ||
                !member.parameters.empty() || member.declaredTypeKey != "builtin:bool" ||
                (tags[*index].has_value() && *tags[*index] != decoded->second)) return false;
            tests[*index] = memberOrdinal;
        }
        else
        {
            if (extractors[*index].has_value() || member.isStatic ||
                !member.parameters.empty() ||
                (tags[*index].has_value() && *tags[*index] != decoded->second)) return false;
            extractors[*index] = memberOrdinal;
            if (payloadTypeKeys[*index].empty()) payloadTypeKeys[*index] = member.declaredTypeKey;
            else if (payloadTypeKeys[*index] != member.declaredTypeKey) return false;
        }
        tags[*index] = decoded->second;
    }
    return true;
}

std::optional<MethodResultPropagationBinding>
ProjectMethodBodyModel::resolveResultCarrier(std::string_view typeKey) const
{
    if (memberModel_ == nullptr) return std::nullopt;
    const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value()) return std::nullopt;

    std::array<std::string, 2> payloadTypeKeys;
    std::array<std::optional<std::size_t>, 2> tags;
    std::array<std::optional<std::size_t>, 2> factories;
    std::array<std::optional<std::size_t>, 2> tests;
    std::array<std::optional<std::size_t>, 2> extractors;
    bool sawUnexpectedCase = false;
    if (!scanResultCarrierContracts(
            *typeOrdinal, typeKey, payloadTypeKeys, tags, factories, tests,
            extractors, sawUnexpectedCase)) return std::nullopt;
    if (sawUnexpectedCase || !factories[0].has_value() || !tests[0].has_value() ||
        !extractors[0].has_value() || !tags[0].has_value() ||
        !factories[1].has_value() || !tests[1].has_value() || !extractors[1].has_value() ||
        !tags[1].has_value() || payloadTypeKeys[0].empty() ||
        payloadTypeKeys[1].empty() || *tags[0] == *tags[1]) return std::nullopt;

    MethodResultPropagationBinding binding;
    binding.sourceCarrierTypeKey = std::string(typeKey);
    binding.successTypeKey = std::move(payloadTypeKeys[0]);
    binding.sourceErrorTypeKey = std::move(payloadTypeKeys[1]);
    binding.sourceSuccessTag = *tags[0];
    binding.sourceErrorTag = *tags[1];
    binding.sourceSuccessTestMemberOrdinal = *tests[0];
    binding.sourceSuccessExtractorMemberOrdinal = *extractors[0];
    binding.sourceErrorExtractorMemberOrdinal = *extractors[1];
    binding.sourceSuccessFactoryMemberOrdinal = *factories[0];
    binding.returnErrorFactoryMemberOrdinal = *factories[1];
    return binding;
}
std::optional<MethodOptionalCarrierBinding>
ProjectMethodBodyModel::resolveOptionalCarrier(std::string_view typeKey) const
{
    if (genericModel_ == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr)
        return std::nullopt;

    const GenericSpecializationRecord* specialization =
        genericModel_->specializationForTypeKey(typeKey);
    if (specialization == nullptr || specialization->kind != GenericTemplateKind::Struct ||
        specialization->typeArguments.size() != 1U)
    {
        return std::nullopt;
    }

    const GenericTemplateRecord* source =
        genericModel_->templateForCanonicalKey(specialization->templateKey);
    if (source == nullptr || source->namespaceName != "NativeSharp.Core" ||
        !source->ownerName.empty() || source->name != "Optional" ||
        source->parameters.size() != 1U)
    {
        return std::nullopt;
    }

    const std::optional<std::size_t> typeOrdinal =
        accessorReceiverTypeOrdinalForTypeKey(typeKey);
    if (!typeOrdinal.has_value()) return std::nullopt;

    const std::string valueTypeKey = ProjectMemberModel::canonicalGenericArgumentKey(
        specialization->typeArguments.front(), *nameLookup_);
    if (valueTypeKey.starts_with("unresolved:")) return std::nullopt;

    std::optional<std::size_t> hasValueMemberOrdinal;
    std::optional<std::size_t> uncheckedValueMemberOrdinal;
    const std::span<const ClassMemberSymbol> members = memberModel_->members();
    std::vector<std::size_t> fallbackMemberOrdinals;
    std::span<const std::size_t> memberOrdinals =
        memberModel_->membersDeclaredByType(*typeOrdinal);
    if (!memberModel_->hasMembersDeclaredByTypeIndex())
    {
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
        {
            if (members[ordinal].declaringTypeOrdinal == *typeOrdinal)
                fallbackMemberOrdinals.push_back(ordinal);
        }
        memberOrdinals = fallbackMemberOrdinals;
    }
    for (const std::size_t ordinal : memberOrdinals)
    {
        if (ordinal >= members.size()) continue;
        const ClassMemberSymbol& member = members[ordinal];
        if (member.kind != ClassMemberKind::Method || member.isStatic ||
            member.accessibility != ClassMemberAccessibility::Public ||
            !member.parameters.empty())
        {
            continue;
        }

        if (member.name == "HasValue" && member.declaredTypeKey == "builtin:bool")
        {
            if (hasValueMemberOrdinal.has_value()) return std::nullopt;
            hasValueMemberOrdinal = ordinal;
        }
        else if (member.name == "UncheckedValue" && member.declaredTypeKey == valueTypeKey)
        {
            if (uncheckedValueMemberOrdinal.has_value()) return std::nullopt;
            uncheckedValueMemberOrdinal = ordinal;
        }
    }

    if (!hasValueMemberOrdinal.has_value() || !uncheckedValueMemberOrdinal.has_value())
        return std::nullopt;

    MethodOptionalCarrierBinding binding;
    binding.carrierTypeKey = std::string(typeKey);
    binding.valueTypeKey = valueTypeKey;
    binding.hasValueMemberOrdinal = *hasValueMemberOrdinal;
    binding.uncheckedValueMemberOrdinal = *uncheckedValueMemberOrdinal;
    return binding;
}

}
