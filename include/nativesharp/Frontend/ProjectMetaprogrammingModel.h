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

// Compile-time-only typed catalogue. It owns copied scalar/string values and never publishes a
// runtime descriptor, registry, vtable, or linker-visible symbol.
class ProjectMetaprogrammingModel final
{
public:
    bool build(
        const ProjectNameLookup& names,
        const ProjectMemberModel& members,
        const ProjectObjectLayoutModel& layouts,
        std::span<const ProjectComptimeRecord> comptimeRecords);

    const StaticTypeInfo* findType(
        std::string_view identity,
        std::size_t requestingProjectOrdinal) const noexcept;
    std::span<const StaticMemberInfo> visibleMembers(
        const StaticTypeInfo& type,
        std::size_t requestingProjectOrdinal) const noexcept;
    std::span<const StaticTypeInfo> types() const noexcept;
    std::span<const StaticAttributeInfo> attributes() const noexcept;
    std::span<const StaticAttributeInfo> runtimeAttributes() const noexcept;
    const std::string& error() const noexcept;
    bool emitsRuntimeMetadata() const noexcept;
    bool isBuilt() const noexcept;

private:
    std::vector<StaticTypeInfo> types_;
    std::vector<StaticAttributeInfo> attributes_;
    std::vector<StaticAttributeInfo> runtimeAttributes_;
    mutable std::vector<StaticMemberInfo> visibleScratch_;
    bool built_ = false;
    std::string error_;
};
}
