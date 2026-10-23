#pragma once

#include "nativesharp/Frontend/StaticMemberKind.h"
#include "nativesharp/Frontend/StaticMemberInfo.h"
#include "nativesharp/Frontend/StaticTypeInfo.h"
#include "nativesharp/Frontend/StaticAttributeInfo.h"
#include "nativesharp/Frontend/StaticAvailabilityInfo.h"
#include "nativesharp/Frontend/StaticDeriveInfo.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ComptimeEngine.h"
#include "Internal/ComptimeAttributeDefinition.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMethodBodyModel;





// Availability is a compile-time contract attached to the canonical declaration ordinal.
// Messages and replacements remain structured; source-language revisions never gate a use.


// Compile-time-only typed catalogue. It owns copied scalar/string values and never publishes a
// runtime descriptor, registry, vtable, or linker-visible symbol.
class ProjectMetaprogrammingModel final
{
public:
    bool build(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        const ProjectMethodBodyModel& methodBodies,
        const ProjectObjectLayoutModel& layouts,
        std::span<const ProjectComptimeRecord> comptimeRecords,
        diagnostics::DiagnosticEngine& diagnostics);

    const StaticTypeInfo* findType(
        std::string_view identity,
        std::size_t requestingProjectOrdinal) const noexcept;
    std::span<const StaticMemberInfo> visibleMembers(
        const StaticTypeInfo& type,
        std::size_t requestingProjectOrdinal) const noexcept;
    std::span<const StaticTypeInfo> types() const noexcept;
    std::span<const StaticAttributeInfo> attributes() const noexcept;
    std::span<const StaticAttributeInfo> runtimeAttributes() const noexcept;
    std::span<const StaticAvailabilityInfo> availability() const noexcept;
    std::span<const StaticDeriveInfo> derives() const noexcept;
    const std::string& error() const noexcept;
    bool emitsRuntimeMetadata() const noexcept;
    bool isBuilt() const noexcept;

private:
    void reset();
    bool appendStaticType(
        std::size_t typeOrdinal,
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        const ProjectObjectLayoutModel& layouts);
    bool collectStaticTypes(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        const ProjectObjectLayoutModel& layouts);
    static void addBuiltinAttributeDefinitions(
        std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions);
    bool collectAttributeDefinitions(
        std::span<const ProjectComptimeRecord> comptimeRecords,
        std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions);
    // Declarations of one (project, source ordinal) as (start offset, ordinal), sorted, so the
    // first one at or after a directive is a binary search. Built once per collectAttributeUses.
    struct AttributeTargetCandidates final
    {
        std::vector<std::pair<std::size_t, std::size_t>> types;
        std::vector<std::pair<std::size_t, std::size_t>> members;
    };
    using AttributeTargetIndex =
        std::map<std::pair<std::size_t, std::size_t>, AttributeTargetCandidates>;
    static AttributeTargetIndex buildAttributeTargetIndex(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members);
    bool findAttributeTarget(
        const AttributeTargetIndex& targets,
        const ProjectMemberModel& members,
        const ProjectComptimeRecord& record,
        const ComptimeAttributeDirective& directive,
        std::optional<std::size_t>& targetType,
        std::optional<std::size_t>& targetMember) const;
    bool appendAttributeUse(
        const AttributeTargetIndex& targets,
        const ProjectMemberModel& members,
        const ProjectComptimeRecord& record,
        const ComptimeAttributeDirective& directive,
        const std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions,
        std::unordered_map<std::string, std::size_t>& nonRepeatableUses);
    bool collectAttributeUses(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        std::span<const ProjectComptimeRecord> comptimeRecords,
        const std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions);
    bool invalidAttributeArguments(
        const StaticAttributeInfo& attribute,
        std::string expected);
    std::size_t availabilityIndexForAttribute(
        const StaticAttributeInfo& attribute,
        std::unordered_map<std::size_t, std::size_t>& availabilityByType,
        std::unordered_map<std::size_t, std::size_t>& availabilityByMember);
    bool applyAvailabilityAttribute(
        const StaticAttributeInfo& attribute,
        std::unordered_map<std::size_t, std::size_t>& availabilityByType,
        std::unordered_map<std::size_t, std::size_t>& availabilityByMember);
    bool collectAvailabilityContracts(
        std::unordered_map<std::size_t, std::size_t>& availabilityByType,
        std::unordered_map<std::size_t, std::size_t>& availabilityByMember);
    bool attributeSuppressed(
        std::size_t typeOrdinal,
        std::string_view category) const;
    void reportAvailabilityError(
        std::string_view declaration,
        const StaticAvailabilityInfo& contract,
        std::string reason,
        SourceRange range,
        diagnostics::DiagnosticId errorId,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAvailabilityUse(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        const StaticAvailabilityInfo& contract,
        std::optional<std::size_t> enclosingType,
        SourceRange range,
        diagnostics::DiagnosticId errorId,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAvailabilityBindings(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        const ProjectMethodBodyModel& methodBodies,
        const std::unordered_map<std::size_t, std::size_t>& availabilityByType,
        const std::unordered_map<std::size_t, std::size_t>& availabilityByMember,
        diagnostics::DiagnosticEngine& diagnostics);
    bool fieldIgnored(std::size_t memberOrdinal) const;
    static bool derivedMemberCollides(
        const StaticTypeInfo& type,
        std::string_view name);
    bool appendDerive(
        const StaticTypeInfo& type,
        std::string_view trait,
        const std::vector<std::string>& fields);
    bool collectDerives(const ProjectNameLookup& names);

    std::vector<StaticTypeInfo> types_;
    std::vector<StaticAttributeInfo> attributes_;
    std::vector<StaticAttributeInfo> runtimeAttributes_;
    std::vector<StaticAvailabilityInfo> availability_;
    std::vector<StaticDeriveInfo> derives_;
    mutable std::vector<StaticMemberInfo> visibleScratch_;
    bool built_ = false;
    std::string error_;
};
}
