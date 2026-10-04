#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"

#include <algorithm>

namespace nativesharp::frontend
{
namespace
{
StaticMemberKind staticKind(ClassMemberKind kind) noexcept
{
    switch (kind)
    {
    case ClassMemberKind::Field: return StaticMemberKind::Field;
    case ClassMemberKind::Method: return StaticMemberKind::Method;
    case ClassMemberKind::Constructor: return StaticMemberKind::Constructor;
    case ClassMemberKind::Destructor: return StaticMemberKind::Destructor;
    case ClassMemberKind::Event: return StaticMemberKind::Event;
    }
    return StaticMemberKind::Field;
}
}

bool ProjectMetaprogrammingModel::build(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectObjectLayoutModel& layouts)
{
    types_.clear();
    visibleScratch_.clear();
    built_ = false;
    if (!names.isBuilt() || !members.isBuilt() || !layouts.isBuilt()) return false;

    types_.reserve(names.namedTypes().size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < names.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& declaration = names.namedTypes()[typeOrdinal];
        StaticTypeInfo type;
        type.typeOrdinal = typeOrdinal;
        type.ownerProjectOrdinal = declaration.projectOrdinal;
        type.identity = declaration.identity.key();
        type.moduleName = declaration.identity.moduleName();
        type.kind = declaration.kind;
        type.isPublic = declaration.accessibility == NamedTypeAccessibility::Public;
        type.isOpenGeneric = declaration.identity.arity() != 0;
        type.isPrecompiled = declaration.isPrecompiled;
        if (const ClassObjectLayout* layout = layouts.layoutForType(typeOrdinal))
        {
            type.size = layout->size;
            type.alignment = layout->alignment;
        }
        else if (const StructValueLayout* layout = layouts.structLayoutForType(typeOrdinal))
        {
            type.size = layout->size;
            type.alignment = layout->alignment;
        }

        const auto ordinals = members.membersOfType(typeOrdinal);
        type.members.reserve(ordinals.size());
        for (const std::size_t memberOrdinal : ordinals)
        {
            if (memberOrdinal >= members.members().size()) return false;
            const ClassMemberSymbol& member = members.members()[memberOrdinal];
            type.members.push_back({memberOrdinal, staticKind(member.kind), member.name,
                member.declaredTypeKey, member.signatureKey,
                member.accessibility == ClassMemberAccessibility::Public, member.isStatic});
        }
        std::ranges::sort(type.members, {}, &StaticMemberInfo::memberOrdinal);
        types_.push_back(std::move(type));
    }
    std::ranges::sort(types_, {}, &StaticTypeInfo::typeOrdinal);
    built_ = true;
    return true;
}

const StaticTypeInfo* ProjectMetaprogrammingModel::findType(
    std::string_view identity,
    std::size_t requestingProjectOrdinal) const noexcept
{
    const auto found = std::ranges::find(types_, identity, &StaticTypeInfo::identity);
    if (found == types_.end()) return nullptr;
    if (!found->isPublic && requestingProjectOrdinal != found->ownerProjectOrdinal) return nullptr;
    return &*found;
}

std::span<const StaticMemberInfo> ProjectMetaprogrammingModel::visibleMembers(
    const StaticTypeInfo& type,
    std::size_t requestingProjectOrdinal) const noexcept
{
    visibleScratch_.clear();
    const bool sameModule = requestingProjectOrdinal == type.ownerProjectOrdinal;
    for (const StaticMemberInfo& member : type.members)
        if (sameModule || member.isPublic) visibleScratch_.push_back(member);
    return visibleScratch_;
}

std::span<const StaticTypeInfo> ProjectMetaprogrammingModel::types() const noexcept { return types_; }
bool ProjectMetaprogrammingModel::emitsRuntimeMetadata() const noexcept { return false; }
bool ProjectMetaprogrammingModel::isBuilt() const noexcept { return built_; }
}
