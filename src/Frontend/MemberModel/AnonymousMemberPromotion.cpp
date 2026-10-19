#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
ClassMemberAccessibility ProjectMemberModel::intersectAnonymousMemberAccessibility(
    ClassMemberAccessibility carrier,
    ClassMemberAccessibility member) noexcept
{
    // Item 326. Promotion is a semantic alias, not physical flattening. Every unnamed aggregate
    // still owns one compiler-generated carrier field so its pack/align/size, union overlap and
    // tail padding remain authoritative. The aliases live outside membersByType_: ordinary model
    // consumers therefore continue to see exactly the physical fields, while findMembers() adds
    // the promoted surface for source resolution.
    if (carrier == member) return carrier;
    if (carrier == ClassMemberAccessibility::Private ||
        member == ClassMemberAccessibility::Private)
        return ClassMemberAccessibility::Private;
    if (carrier == ClassMemberAccessibility::Public) return member;
    if (member == ClassMemberAccessibility::Public) return carrier;
    // `internal` intersected with `protected` has no single NativeSharp spelling. Private is
    // the conservative result: promotion must never make a member more visible than either
    // edge of its path.
    return ClassMemberAccessibility::Private;
}

void ProjectMemberModel::promoteAnonymousSourceMember(
    std::size_t typeOrdinal,
    std::size_t carrierOrdinal,
    std::size_t sourceOrdinal,
    const ProjectNameLookup& nameLookup,
    std::unordered_map<std::string, std::size_t>& directNames,
    std::unordered_map<std::string, std::size_t>& promotedNames,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (sourceOrdinal >= members_.size()) return;
    const ClassMemberSymbol source = members_[sourceOrdinal];
    if (source.isCompilerGenerated && !source.isAnonymousMemberPromotion) return;
    if (source.kind != ClassMemberKind::Field || source.isStatic)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            source.range,
            "an unnamed anonymous aggregate can promote instance fields only");
        return;
    }

    ClassMemberSymbol alias = source;
    alias.accessibility = intersectAnonymousMemberAccessibility(
        members_[carrierOrdinal].accessibility, source.accessibility);
    alias.declaringTypeOrdinal = typeOrdinal;
    alias.partOrdinal = members_[carrierOrdinal].partOrdinal;
    alias.declarationOrdinal = members_[carrierOrdinal].declarationOrdinal;
    alias.syntax = nullptr;
    alias.isCompilerGenerated = true;
    alias.isAnonymousMemberPromotion = true;
    alias.promotedSourceMemberOrdinal = source.isAnonymousMemberPromotion
        ? source.promotedSourceMemberOrdinal
        : std::optional<std::size_t>(sourceOrdinal);
    alias.anonymousCarrierMemberOrdinals.clear();
    alias.anonymousCarrierMemberOrdinals.push_back(carrierOrdinal);
    alias.anonymousCarrierMemberOrdinals.insert(
        alias.anonymousCarrierMemberOrdinals.end(),
        source.anonymousCarrierMemberOrdinals.begin(),
        source.anonymousCarrierMemberOrdinals.end());
    alias.explicitFieldOffset.reset();
    alias.explicitFieldAlignment.reset();
    alias.declaringTypeIsStruct = typeOrdinal < nameLookup.namedTypes().size() &&
        nameLookup.namedTypes()[typeOrdinal].kind == NamedTypeDeclarationKind::Struct;

    const auto direct = directNames.find(alias.name);
    const auto previousPromotion = promotedNames.find(alias.name);
    if (direct != directNames.end() || previousPromotion != promotedNames.end())
    {
        const std::size_t previousOrdinal = direct != directNames.end()
            ? direct->second : previousPromotion->second;
        if (previousOrdinal < members_.size())
            reportDuplicate(
                alias,
                members_[previousOrdinal],
                diagnostics,
                "promoted anonymous member name '" + alias.name + "' conflicts in type '" +
                    nameLookup.namedTypes()[typeOrdinal].identity.fullyQualifiedName() + "'");
        return;
    }
    const std::size_t aliasOrdinal = members_.size();
    members_.push_back(std::move(alias));
    promotedMembersByType_[typeOrdinal].push_back(aliasOrdinal);
    promotedNames.emplace(members_[aliasOrdinal].name, aliasOrdinal);
}

void ProjectMemberModel::promoteAnonymousCarrierMembers(
    std::size_t typeOrdinal,
    std::size_t carrierOrdinal,
    const ProjectNameLookup& nameLookup,
    std::unordered_map<std::string, std::size_t>& directNames,
    std::unordered_map<std::string, std::size_t>& promotedNames,
    std::vector<AnonymousMemberPromotionVisitState>& states,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (carrierOrdinal >= members_.size()) return;
    const ClassMemberSymbol& carrierBeforeVisit = members_[carrierOrdinal];
    if (carrierBeforeVisit.kind != ClassMemberKind::Field || carrierBeforeVisit.isStatic ||
        carrierBeforeVisit.syntax == nullptr ||
        carrierBeforeVisit.syntax->auxiliaryText().find("anonymous-member-carrier") ==
            std::string::npos)
        return;
    const std::optional<std::size_t> nestedTypeOrdinal =
        typeOrdinalForKey(carrierBeforeVisit.declaredTypeKey);
    if (!nestedTypeOrdinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            carrierBeforeVisit.range,
            "anonymous-member carrier has no synthesized aggregate type");
        return;
    }
    visitAnonymousMemberPromotions(*nestedTypeOrdinal, nameLookup, states, diagnostics);
    std::vector<std::size_t> candidates = membersByType_[*nestedTypeOrdinal];
    candidates.insert(
        candidates.end(),
        promotedMembersByType_[*nestedTypeOrdinal].begin(),
        promotedMembersByType_[*nestedTypeOrdinal].end());
    for (const std::size_t sourceOrdinal : candidates)
        promoteAnonymousSourceMember(
            typeOrdinal,
            carrierOrdinal,
            sourceOrdinal,
            nameLookup,
            directNames,
            promotedNames,
            diagnostics);
}

void ProjectMemberModel::visitAnonymousMemberPromotions(
    std::size_t typeOrdinal,
    const ProjectNameLookup& nameLookup,
    std::vector<AnonymousMemberPromotionVisitState>& states,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (typeOrdinal >= states.size() ||
        states[typeOrdinal] == AnonymousMemberPromotionVisitState::Complete)
        return;
    if (states[typeOrdinal] == AnonymousMemberPromotionVisitState::Visiting) return;
    states[typeOrdinal] = AnonymousMemberPromotionVisitState::Visiting;

    std::unordered_map<std::string, std::size_t> directNames;
    for (const std::size_t ordinal : membersByType_[typeOrdinal])
    {
        if (ordinal >= members_.size()) continue;
        const ClassMemberSymbol& member = members_[ordinal];
        if (!member.isCompilerGenerated) directNames.emplace(member.name, ordinal);
    }
    std::unordered_map<std::string, std::size_t> promotedNames;
    const std::vector<std::size_t> physicalMembers = membersByType_[typeOrdinal];
    for (const std::size_t carrierOrdinal : physicalMembers)
        promoteAnonymousCarrierMembers(
            typeOrdinal,
            carrierOrdinal,
            nameLookup,
            directNames,
            promotedNames,
            states,
            diagnostics);
    states[typeOrdinal] = AnonymousMemberPromotionVisitState::Complete;
}

bool ProjectMemberModel::collectAnonymousMemberPromotions(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<AnonymousMemberPromotionVisitState> states(
        nameLookup.namedTypes().size(), AnonymousMemberPromotionVisitState::Unvisited);
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
        visitAnonymousMemberPromotions(typeOrdinal, nameLookup, states, diagnostics);
    return !diagnostics.hasErrors();
}
}
