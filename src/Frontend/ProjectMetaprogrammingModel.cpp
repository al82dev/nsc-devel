#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <algorithm>
#include <unordered_map>

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
    const ProjectObjectLayoutModel& layouts,
    std::span<const ProjectComptimeRecord> comptimeRecords)
{
    types_.clear();
    attributes_.clear();
    runtimeAttributes_.clear();
    derives_.clear();
    visibleScratch_.clear();
    error_.clear();
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

    struct Definition final
    {
        std::vector<ComptimeAttributeTarget> targets;
        bool repeatable = false;
        ComptimeAttributeRetention retention = ComptimeAttributeRetention::CompileTime;
    };
    std::unordered_map<std::string, Definition> definitions;
    for (const ProjectComptimeRecord& record : comptimeRecords)
    {
        for (const ComptimeAttributeDirective& directive : record.attributes)
        {
            if (!directive.isDefinition) continue;
            if (!definitions.emplace(directive.identity,
                    Definition{directive.targets, directive.repeatable, directive.retention}).second)
            {
                error_ = "duplicate attribute definition '" + directive.identity + "'";
                return false;
            }
        }
    }

    std::unordered_map<std::string, std::size_t> nonRepeatableUses;
    for (const ProjectComptimeRecord& record : comptimeRecords)
    {
        for (const ComptimeAttributeDirective& directive : record.attributes)
        {
            if (directive.isDefinition) continue;
            const auto definition = definitions.find(directive.identity);
            if (definition == definitions.end())
            {
                error_ = "attribute use refers to undefined attribute '" + directive.identity + "'";
                return false;
            }

            std::optional<std::size_t> targetType;
            std::optional<std::size_t> targetMember;
            std::uint32_t nearest = UINT32_MAX;
            for (std::size_t typeOrdinal = 0; typeOrdinal < names.namedTypes().size(); ++typeOrdinal)
            {
                const NamedTypeDeclaration& type = names.namedTypes()[typeOrdinal];
                if (type.projectOrdinal != record.projectOrdinal ||
                    type.sourceOrdinal != record.sourceOrdinal || !type.range.isValid() ||
                    type.range.begin().offset() < directive.offset) continue;
                if (type.range.begin().offset() < nearest)
                {
                    nearest = type.range.begin().offset();
                    targetType = typeOrdinal;
                    targetMember.reset();
                }
            }
            for (std::size_t memberOrdinal = 0; memberOrdinal < members.members().size(); ++memberOrdinal)
            {
                const ClassMemberSymbol& member = members.members()[memberOrdinal];
                if (member.sourceOrdinal != record.sourceOrdinal || !member.range.isValid() ||
                    member.range.begin().offset() < directive.offset ||
                    member.declaringTypeOrdinal >= names.namedTypes().size() ||
                    names.namedTypes()[member.declaringTypeOrdinal].projectOrdinal != record.projectOrdinal)
                    continue;
                if (member.range.begin().offset() < nearest)
                {
                    nearest = member.range.begin().offset();
                    targetType = member.declaringTypeOrdinal;
                    targetMember = memberOrdinal;
                }
            }
            if (!targetType.has_value())
            {
                error_ = "attribute '" + directive.identity + "' has no following declaration target";
                return false;
            }

            ComptimeAttributeTarget targetKind = ComptimeAttributeTarget::Type;
            if (targetMember.has_value())
            {
                const ClassMemberKind kind = members.members()[*targetMember].kind;
                targetKind = kind == ClassMemberKind::Field
                    ? ComptimeAttributeTarget::Field : ComptimeAttributeTarget::Method;
            }
            if (std::ranges::find(definition->second.targets, targetKind) ==
                definition->second.targets.end())
            {
                error_ = "attribute '" + directive.identity + "' is not valid on this declaration target";
                return false;
            }
            const std::string targetKey = directive.identity + "@" +
                (targetMember.has_value() ? "member:" + std::to_string(*targetMember)
                                          : "type:" + std::to_string(*targetType));
            if (!definition->second.repeatable && !nonRepeatableUses.emplace(targetKey, 1).second)
            {
                error_ = "non-repeatable attribute '" + directive.identity +
                    "' appears more than once on one declaration";
                return false;
            }
            StaticAttributeInfo attribute{directive.identity, directive.constantArguments,
                definition->second.retention, targetType, targetMember};
            attributes_.push_back(attribute);
            if (attribute.retention == ComptimeAttributeRetention::NativeRuntimeSection)
                runtimeAttributes_.push_back(std::move(attribute));
        }
    }

    const auto ignoredField = [&](std::size_t memberOrdinal)
    {
        return std::ranges::any_of(attributes_, [&](const StaticAttributeInfo& attribute)
        {
            return attribute.memberOrdinal == memberOrdinal &&
                (attribute.identity == "NativeSharp.DeriveIgnore" ||
                 attribute.identity.ends_with(".DeriveIgnore"));
        });
    };
    for (const StaticTypeInfo& type : types_)
    {
        const NamedTypeDeclaration& declaration = names.namedTypes()[type.typeOrdinal];
        if (declaration.syntax == nullptr) continue;
        const std::string_view metadata = declaration.syntax->auxiliaryText();
        const std::size_t marker = metadata.find("derive=");
        if (marker == std::string_view::npos) continue;
        const std::size_t begin = marker + 7;
        const std::size_t end = metadata.find(',', begin);
        const std::string_view list = metadata.substr(begin, end - begin);
        std::vector<std::string> fields;
        for (const StaticMemberInfo& member : type.members)
        {
            if (member.kind != StaticMemberKind::Field || member.isStatic ||
                ignoredField(member.memberOrdinal)) continue;
            fields.push_back(member.name + ":" + member.typeKey);
        }
        std::size_t itemBegin = 0;
        while (itemBegin <= list.size())
        {
            const std::size_t separator = list.find('|', itemBegin);
            const std::string trait(list.substr(itemBegin, separator - itemBegin));
            std::vector<std::string> generated;
            if (trait == "Equality") generated = {"Equals", "operator==", "operator!="};
            else if (trait == "Hash") generated = {"GetHashCode"};
            else if (trait == "Print") generated = {"ToString"};
            else if (trait == "Clone") generated = {"Clone"};
            derives_.push_back({type.typeOrdinal, trait,
                language::deriveDependencyKey(type.identity + "::" + trait, fields),
                fields, std::move(generated)});
            if (separator == std::string_view::npos) break;
            itemBegin = separator + 1;
        }
    }
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
std::span<const StaticAttributeInfo> ProjectMetaprogrammingModel::attributes() const noexcept
{
    return attributes_;
}
std::span<const StaticAttributeInfo> ProjectMetaprogrammingModel::runtimeAttributes() const noexcept
{
    return runtimeAttributes_;
}
std::span<const StaticDeriveInfo> ProjectMetaprogrammingModel::derives() const noexcept
{
    return derives_;
}
const std::string& ProjectMetaprogrammingModel::error() const noexcept { return error_; }
bool ProjectMetaprogrammingModel::emitsRuntimeMetadata() const noexcept { return false; }
bool ProjectMetaprogrammingModel::isBuilt() const noexcept { return built_; }
}
