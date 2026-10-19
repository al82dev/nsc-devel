#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <algorithm>
#include <unordered_map>

namespace nativesharp::frontend
{
namespace
{
// NS304-T11-F08: strip `<...>` generic arguments from an attribute identity so a use `Tag<int>`
// matches its `attribute Tag<T>` definition. The generic arguments themselves are validated
// (consteval, non-ref-like) at the use site below.
std::string attributeBaseName(std::string_view identity)
{
    const std::size_t generic = identity.find('<');
    return generic == std::string_view::npos
        ? std::string(identity) : std::string(identity.substr(0, generic));
}

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
    // NS304-T14-F01: a new ClassMemberKind without a visitor/introspection mapping must fail
    // loudly instead of silently collapsing to Field. The switch above is exhaustive for the
    // kinds admitted in v3.0.4; reaching this trap is the defensive gate the audit requested.
    std::abort();
}

std::string unquote(std::string_view value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return std::string(value.substr(1, value.size() - 2));
    return std::string(value);
}

}

void ProjectMetaprogrammingModel::reset()
{
    types_.clear();
    attributes_.clear();
    runtimeAttributes_.clear();
    availability_.clear();
    derives_.clear();
    visibleScratch_.clear();
    error_.clear();
    built_ = false;
}

bool ProjectMetaprogrammingModel::appendStaticType(
    std::size_t typeOrdinal,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectObjectLayoutModel& layouts)
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
        if (memberOrdinal >= members.members().size())
        {
            return false;
        }
        const ClassMemberSymbol& member = members.members()[memberOrdinal];
        StaticMemberInfo info;
        info.memberOrdinal = memberOrdinal;
        info.kind = staticKind(member.kind);
        info.name = member.name;
        info.typeKey = member.declaredTypeKey;
        info.signatureKey = member.signatureKey;
        info.isPublic = member.accessibility == ClassMemberAccessibility::Public;
        info.isStatic = member.isStatic;
        info.isReadonly = member.isReadonly;
        info.isConst = member.isConst;
        info.isVolatile = member.isVolatile;
        info.declaredOwnership = member.declaredOwnership;
        info.genericTemplateKey = member.genericTemplateKey;
        info.sourcePath = member.sourcePath;
        info.frameStorage = member.frameStorage;
        type.members.push_back(std::move(info));
    }
    std::ranges::sort(type.members, {}, &StaticMemberInfo::memberOrdinal);
    types_.push_back(std::move(type));
    return true;
}

bool ProjectMetaprogrammingModel::collectStaticTypes(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectObjectLayoutModel& layouts)
{
    types_.reserve(names.namedTypes().size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < names.namedTypes().size(); ++typeOrdinal)
    {
        if (!appendStaticType(typeOrdinal, names, members, layouts))
        {
            return false;
        }
    }
    std::ranges::sort(types_, {}, &StaticTypeInfo::typeOrdinal);
    return true;
}

void ProjectMetaprogrammingModel::addBuiltinAttributeDefinitions(
    std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions)
{
    const std::vector<ComptimeAttributeTarget> everyDeclaration = {
        ComptimeAttributeTarget::Type,
        ComptimeAttributeTarget::Field,
        ComptimeAttributeTarget::Method};
    // These names are owned by the language. Source code can use them without declaring a
    // user attribute and cannot silently replace their semantics.
    definitions.emplace("deprecated", internal::ComptimeAttributeDefinition{
        everyDeclaration, false, ComptimeAttributeRetention::CompileTime});
    definitions.emplace("since", internal::ComptimeAttributeDefinition{
        everyDeclaration, false, ComptimeAttributeRetention::CompileTime});
    definitions.emplace("removed", internal::ComptimeAttributeDefinition{
        everyDeclaration, false, ComptimeAttributeRetention::CompileTime});
    definitions.emplace("target", internal::ComptimeAttributeDefinition{
        everyDeclaration, false, ComptimeAttributeRetention::CompileTime});
    definitions.emplace("architecture", internal::ComptimeAttributeDefinition{
        everyDeclaration, false, ComptimeAttributeRetention::CompileTime});
    definitions.emplace("allow", internal::ComptimeAttributeDefinition{
        everyDeclaration, true, ComptimeAttributeRetention::CompileTime});
    // NS304-T13-F06: `[TrailingStorage(nameof(Length), nameof(Capacity))]` binds a `T[*]` flexible
    // field to its explicit Length/Capacity members. Fields only, non-repeatable, compile-time.
    definitions.emplace("TrailingStorage", internal::ComptimeAttributeDefinition{
        {ComptimeAttributeTarget::Field}, false, ComptimeAttributeRetention::CompileTime});
}

bool ProjectMetaprogrammingModel::collectAttributeDefinitions(
    std::span<const ProjectComptimeRecord> comptimeRecords,
    std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions)
{
    for (const ProjectComptimeRecord& record : comptimeRecords)
    {
        for (const ComptimeAttributeDirective& directive : record.attributes)
        {
            if (!directive.isDefinition)
            {
                continue;
            }
            if (!definitions.emplace(
                attributeBaseName(directive.identity),
                internal::ComptimeAttributeDefinition{
                    directive.targets, directive.repeatable, directive.retention}).second)
            {
                error_ = "duplicate attribute definition '" + directive.identity + "'";
                return false;
            }
        }
    }
    return true;
}

bool ProjectMetaprogrammingModel::findAttributeTarget(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectComptimeRecord& record,
    const ComptimeAttributeDirective& directive,
    std::optional<std::size_t>& targetType,
    std::optional<std::size_t>& targetMember) const
{
    std::uint32_t nearest = UINT32_MAX;
    for (std::size_t typeOrdinal = 0; typeOrdinal < names.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = names.namedTypes()[typeOrdinal];
        if (type.projectOrdinal != record.projectOrdinal ||
            type.sourceOrdinal != record.sourceOrdinal || !type.range.isValid() ||
            type.range.begin().offset() < directive.offset)
        {
            continue;
        }
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
        {
            continue;
        }
        if (member.range.begin().offset() < nearest)
        {
            nearest = member.range.begin().offset();
            targetType = member.declaringTypeOrdinal;
            targetMember = memberOrdinal;
        }
    }
    return targetType.has_value();
}

bool ProjectMetaprogrammingModel::appendAttributeUse(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectComptimeRecord& record,
    const ComptimeAttributeDirective& directive,
    const std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions,
    std::unordered_map<std::string, std::size_t>& nonRepeatableUses)
{
    const auto definition = definitions.find(attributeBaseName(directive.identity));
    if (definition == definitions.end())
    {
        error_ = "attribute use refers to undefined attribute '" + directive.identity + "'";
        return false;
    }
    std::optional<std::size_t> targetType;
    std::optional<std::size_t> targetMember;
    if (!findAttributeTarget(
        names, members, record, directive, targetType, targetMember))
    {
        error_ = "attribute '" + directive.identity + "' has no following declaration target";
        return false;
    }
    ComptimeAttributeTarget targetKind = ComptimeAttributeTarget::Type;
    if (targetMember.has_value())
    {
        const ClassMemberKind kind = members.members()[*targetMember].kind;
        targetKind = kind == ClassMemberKind::Field
            ? ComptimeAttributeTarget::Field
            : ComptimeAttributeTarget::Method;
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
    StaticAttributeInfo attribute{
        directive.identity,
        directive.constantArguments,
        definition->second.retention,
        targetType,
        targetMember};
    attributes_.push_back(attribute);
    if (attribute.retention == ComptimeAttributeRetention::NativeRuntimeSection)
    {
        runtimeAttributes_.push_back(std::move(attribute));
    }
    return true;
}

bool ProjectMetaprogrammingModel::collectAttributeUses(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::span<const ProjectComptimeRecord> comptimeRecords,
    const std::unordered_map<std::string, internal::ComptimeAttributeDefinition>& definitions)
{
    std::unordered_map<std::string, std::size_t> nonRepeatableUses;
    for (const ProjectComptimeRecord& record : comptimeRecords)
    {
        for (const ComptimeAttributeDirective& directive : record.attributes)
        {
            if (directive.isDefinition)
            {
                continue;
            }
            if (!appendAttributeUse(
                names, members, record, directive, definitions, nonRepeatableUses))
            {
                return false;
            }
        }
    }
    return true;
}

bool ProjectMetaprogrammingModel::invalidAttributeArguments(
    const StaticAttributeInfo& attribute,
    std::string expected)
{
    error_ = "attribute '" + attribute.identity + "' expects " + std::move(expected);
    return false;
}

std::size_t ProjectMetaprogrammingModel::availabilityIndexForAttribute(
    const StaticAttributeInfo& attribute,
    std::unordered_map<std::size_t, std::size_t>& availabilityByType,
    std::unordered_map<std::size_t, std::size_t>& availabilityByMember)
{
    const std::size_t* existing = nullptr;
    if (attribute.memberOrdinal.has_value())
    {
        const auto found = availabilityByMember.find(*attribute.memberOrdinal);
        if (found != availabilityByMember.end())
        {
            existing = &found->second;
        }
    }
    else if (attribute.typeOrdinal.has_value())
    {
        const auto found = availabilityByType.find(*attribute.typeOrdinal);
        if (found != availabilityByType.end())
        {
            existing = &found->second;
        }
    }
    const std::size_t index = existing != nullptr ? *existing : availability_.size();
    if (existing == nullptr)
    {
        StaticAvailabilityInfo created;
        created.typeOrdinal = attribute.typeOrdinal;
        created.memberOrdinal = attribute.memberOrdinal;
        availability_.push_back(std::move(created));
        if (attribute.memberOrdinal.has_value())
            availabilityByMember.emplace(*attribute.memberOrdinal, index);
        else
            availabilityByType.emplace(*attribute.typeOrdinal, index);
    }
    return index;
}

bool ProjectMetaprogrammingModel::applyAvailabilityAttribute(
    const StaticAttributeInfo& attribute,
    std::unordered_map<std::size_t, std::size_t>& availabilityByType,
    std::unordered_map<std::size_t, std::size_t>& availabilityByMember)
{
    // Historical since/removed annotations remain parseable metadata, but they never create
    // an availability gate in the cumulative language.
    if (attribute.identity == "since" || attribute.identity == "removed")
    {
        return true;
    }
    if (attribute.identity != "deprecated" && attribute.identity != "target" &&
        attribute.identity != "architecture")
    {
        return true;
    }
    StaticAvailabilityInfo& contract = availability_[availabilityIndexForAttribute(
        attribute, availabilityByType, availabilityByMember)];
    if (attribute.identity == "deprecated")
    {
        if (attribute.constantArguments.size() > 2)
            return invalidAttributeArguments(
                attribute, "zero to two string arguments: message, replacement");
        for (const std::string& argument : attribute.constantArguments)
            if (argument.size() < 2 || argument.front() != '"' || argument.back() != '"')
                return invalidAttributeArguments(attribute, "string arguments");
        contract.deprecated = true;
        if (!attribute.constantArguments.empty())
            contract.message = unquote(attribute.constantArguments[0]);
        if (attribute.constantArguments.size() == 2)
            contract.replacement = unquote(attribute.constantArguments[1]);
        return true;
    }
    if (attribute.constantArguments.size() != 1 ||
        attribute.constantArguments[0].size() < 2 ||
        attribute.constantArguments[0].front() != '"' ||
        attribute.constantArguments[0].back() != '"')
    {
        return invalidAttributeArguments(attribute, "one string argument");
    }
    if (attribute.identity == "target")
        contract.target = unquote(attribute.constantArguments[0]);
    else
        contract.architecture = unquote(attribute.constantArguments[0]);
    return true;
}

bool ProjectMetaprogrammingModel::collectAvailabilityContracts(
    std::unordered_map<std::size_t, std::size_t>& availabilityByType,
    std::unordered_map<std::size_t, std::size_t>& availabilityByMember)
{
    for (const StaticAttributeInfo& attribute : attributes_)
    {
        if (!applyAvailabilityAttribute(attribute, availabilityByType, availabilityByMember))
        {
            return false;
        }
    }
    return true;
}

bool ProjectMetaprogrammingModel::attributeSuppressed(
    std::size_t typeOrdinal,
    std::string_view category) const
{
    for (const StaticAttributeInfo& attribute : attributes_)
    {
        if (attribute.identity == "allow" && attribute.typeOrdinal == typeOrdinal &&
            !attribute.memberOrdinal.has_value() && attribute.constantArguments.size() == 1 &&
            unquote(attribute.constantArguments[0]) == category)
        {
            return true;
        }
    }
    return false;
}

void ProjectMetaprogrammingModel::reportAvailabilityError(
    std::string_view declaration,
    const StaticAvailabilityInfo& contract,
    std::string reason,
    SourceRange range,
    diagnostics::DiagnosticId errorId,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string message =
        "declaration '" + std::string(declaration) + "' " + std::move(reason) +
        (contract.message.empty() ? std::string{} : ": " + contract.message) +
        (contract.replacement.empty() ? std::string{} : "; use '" + contract.replacement + "'");
    if (error_.empty())
    {
        error_ = message;
    }
    diagnostics.report(errorId, diagnostics::DiagnosticSeverity::Error, range, message);
}

bool ProjectMetaprogrammingModel::validateAvailabilityUse(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const StaticAvailabilityInfo& contract,
    std::optional<std::size_t> enclosingType,
    SourceRange range,
    diagnostics::DiagnosticId errorId,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string declaration = contract.memberOrdinal.has_value()
        ? members.members()[*contract.memberOrdinal].name
        : names.namedTypes()[*contract.typeOrdinal].identity.key();
    bool ok = true;
    if (!contract.target.empty() && contract.target != "windows")
    {
        reportAvailabilityError(
            declaration,
            contract,
            "is unavailable for target windows",
            range,
            errorId,
            diagnostics);
        ok = false;
    }
    if (!contract.architecture.empty() && contract.architecture != "x64")
    {
        reportAvailabilityError(
            declaration,
            contract,
            "is unavailable for architecture x64",
            range,
            errorId,
            diagnostics);
        ok = false;
    }
    if (contract.deprecated &&
        (!enclosingType.has_value() || !attributeSuppressed(*enclosingType, "deprecated")))
    {
        diagnostics.report(
            errorId,
            diagnostics::DiagnosticSeverity::Warning,
            range,
            "declaration '" + declaration + "' is deprecated" +
                (contract.message.empty() ? std::string{} : ": " + contract.message) +
                (contract.replacement.empty() ? std::string{} : "; use '" + contract.replacement + "'"));
    }
    return ok;
}

bool ProjectMetaprogrammingModel::validateAvailabilityBindings(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectMethodBodyModel& methodBodies,
    const std::unordered_map<std::size_t, std::size_t>& availabilityByType,
    const std::unordered_map<std::size_t, std::size_t>& availabilityByMember,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    for (const TypeNameBinding& binding : names.bindings())
    {
        if (!binding.targetTypeOrdinal.has_value()) continue;
        const auto found = availabilityByType.find(*binding.targetTypeOrdinal);
        if (found == availabilityByType.end() || binding.syntax == nullptr) continue;
        if (!validateAvailabilityUse(
            names,
            members,
            availability_[found->second],
            binding.enclosingTypeOrdinals.empty()
                ? std::optional<std::size_t>{}
                : binding.enclosingTypeOrdinals.back(),
            binding.syntax->range(),
            diagnostics::DiagnosticId::InvalidType,
            diagnostics))
            ok = false;
    }
    for (const MethodCallBinding& call : methodBodies.calls())
    {
        if (!call.selectedMemberOrdinal.has_value() || call.syntax == nullptr ||
            call.callerMethodMemberOrdinal >= members.members().size()) continue;
        const auto found = availabilityByMember.find(*call.selectedMemberOrdinal);
        if (found == availabilityByMember.end()) continue;
        const ClassMemberSymbol& caller = members.members()[call.callerMethodMemberOrdinal];
        if (!validateAvailabilityUse(
            names,
            members,
            availability_[found->second],
            caller.declaringTypeOrdinal,
            call.syntax->range(),
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics))
            ok = false;
    }
    return ok;
}

bool ProjectMetaprogrammingModel::fieldIgnored(std::size_t memberOrdinal) const
{
    for (const StaticAttributeInfo& attribute : attributes_)
    {
        if (attribute.memberOrdinal == memberOrdinal &&
            (attribute.identity == "NativeSharp.DeriveIgnore" ||
             attribute.identity.ends_with(".DeriveIgnore")))
        {
            return true;
        }
    }
    return false;
}

bool ProjectMetaprogrammingModel::derivedMemberCollides(
    const StaticTypeInfo& type,
    std::string_view name)
{
    for (const StaticMemberInfo& member : type.members)
    {
        if (!member.isStatic && member.name == name)
        {
            return true;
        }
    }
    return false;
}

bool ProjectMetaprogrammingModel::appendDerive(
    const StaticTypeInfo& type,
    std::string_view trait,
    const std::vector<std::string>& fields)
{
    std::vector<std::string> generated;
    if (trait == "Equality") generated = {"Equals", "operator==", "operator!="};
    else if (trait == "Hash") generated = {"GetHashCode"};
    else if (trait == "Print") generated = {"ToString"};
    else if (trait == "Clone") generated = {"Clone"};
    // NS304-T12-F06: a derive must not generate a member whose spelling collides with an
    // existing hand-written member of the same type (the same spelling+scope rule the
    // partial merge uses). Report the collision instead of silently double-declaring.
    for (const std::string& name : generated)
    {
        if (derivedMemberCollides(type, name))
        {
            error_ = "derive '" + std::string(trait) + "' generates member '" + name +
                "' which collides with an existing member in type '" + type.identity + "'";
            return false;
        }
    }
    derives_.push_back({
        type.typeOrdinal,
        std::string(trait),
        language::deriveDependencyKey(type.identity + "::" + std::string(trait), fields),
        fields,
        std::move(generated)});
    return true;
}

bool ProjectMetaprogrammingModel::collectDerives(const ProjectNameLookup& names)
{
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
                fieldIgnored(member.memberOrdinal)) continue;
            fields.push_back(member.name + ":" + member.typeKey);
        }
        std::size_t itemBegin = 0;
        while (itemBegin <= list.size())
        {
            const std::size_t separator = list.find('|', itemBegin);
            const std::string_view trait = list.substr(itemBegin, separator - itemBegin);
            if (!appendDerive(type, trait, fields))
            {
                return false;
            }
            if (separator == std::string_view::npos) break;
            itemBegin = separator + 1;
        }
    }
    return true;
}

bool ProjectMetaprogrammingModel::build(
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const ProjectMethodBodyModel& methodBodies,
    const ProjectObjectLayoutModel& layouts,
    std::span<const ProjectComptimeRecord> comptimeRecords,
    diagnostics::DiagnosticEngine& diagnostics)
{
    reset();
    if (!names.isBuilt() || !members.isBuilt() || !layouts.isBuilt())
    {
        return false;
    }
    if (!collectStaticTypes(names, members, layouts))
    {
        return false;
    }

    std::unordered_map<std::string, internal::ComptimeAttributeDefinition> definitions;
    addBuiltinAttributeDefinitions(definitions);
    if (!collectAttributeDefinitions(comptimeRecords, definitions))
    {
        return false;
    }

    if (!collectAttributeUses(names, members, comptimeRecords, definitions))
    {
        return false;
    }

    std::unordered_map<std::size_t, std::size_t> availabilityByType;
    std::unordered_map<std::size_t, std::size_t> availabilityByMember;
    if (!collectAvailabilityContracts(availabilityByType, availabilityByMember))
    {
        return false;
    }

    if (!validateAvailabilityBindings(
        names,
        members,
        methodBodies,
        availabilityByType,
        availabilityByMember,
        diagnostics))
    {
        if (error_.empty()) error_ = "availability validation failed";
        return false;
    }

    if (!collectDerives(names))
    {
        return false;
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
std::span<const StaticAvailabilityInfo> ProjectMetaprogrammingModel::availability() const noexcept
{
    return availability_;
}
std::span<const StaticDeriveInfo> ProjectMetaprogrammingModel::derives() const noexcept
{
    return derives_;
}
const std::string& ProjectMetaprogrammingModel::error() const noexcept { return error_; }
bool ProjectMetaprogrammingModel::emitsRuntimeMetadata() const noexcept { return false; }
bool ProjectMetaprogrammingModel::isBuilt() const noexcept { return built_; }
}
