#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ComptimeEngine.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMethodBodyModel;

enum class StaticMemberKind { Field, Method, Constructor, Destructor, Event };

struct StaticMemberInfo final
{
    std::size_t memberOrdinal = 0;
    StaticMemberKind kind = StaticMemberKind::Field;
    std::string name;
    std::string typeKey;
    std::string signatureKey;
    bool isPublic = false;
    bool isStatic = false;
};

struct StaticTypeInfo final
{
    std::size_t typeOrdinal = 0;
    std::size_t ownerProjectOrdinal = 0;
    std::string identity;
    std::string moduleName;
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    bool isPublic = false;
    bool isOpenGeneric = false;
    bool isPrecompiled = false;
    std::optional<std::uint64_t> size;
    std::optional<std::uint64_t> alignment;
    std::vector<StaticMemberInfo> members;
};

struct StaticAttributeInfo final
{
    std::string identity;
    std::vector<std::string> constantArguments;
    ComptimeAttributeRetention retention = ComptimeAttributeRetention::CompileTime;
    std::optional<std::size_t> typeOrdinal;
    std::optional<std::size_t> memberOrdinal;
};

// NativeSharp 3.0 T12-F05. Availability is a compile-time semantic contract attached to the
// canonical declaration ordinal.  Messages and replacements are retained as structured values;
// consumers never have to scrape display text to decide whether a use is legal.
struct StaticAvailabilityInfo final
{
    std::optional<std::size_t> typeOrdinal;
    std::optional<std::size_t> memberOrdinal;
    std::optional<int> sinceEdition;
    std::optional<int> removedEdition;
    bool deprecated = false;
    std::string message;
    std::string replacement;
    std::string target;
    std::string architecture;
};

struct StaticDeriveInfo final
{
    std::size_t typeOrdinal = 0;
    std::string traitName;
    std::string dependencyKey;
    std::vector<std::string> participatingFields;
    std::vector<std::string> generatedMembers;
};

// Compile-time-only typed catalogue. It owns copied scalar/string values and never publishes a
// runtime descriptor, registry, vtable, or linker-visible symbol.
class ProjectMetaprogrammingModel final
{
public:
    bool build(
        const compilation::ProjectGraph& graph,
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
