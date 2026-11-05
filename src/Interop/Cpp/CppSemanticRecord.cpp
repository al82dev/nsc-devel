#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppSemanticEnumType.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/CppNamespaceInventory.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::interop
{
std::optional<CppSemanticInstanceField> projectCppSemanticInstanceField(
    const CppRecordDeclaration& record, std::size_t fieldIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const frontend::ProjectNameLookup* lookup)
{
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || !record.sizeBits || !record.alignmentBits ||
        (record.kind == CppRecordKind::Union && record.isCxx) || fieldIndex >= record.fields.size()) return std::nullopt;
    const auto identity = projectCppRecordIdentity(record, moduleName);
    const auto& field = record.fields[fieldIndex];
    if (!identity || field.name.empty() || field.access != CppAccess::Public || field.bitWidth ||
        !field.offsetBits || *field.offsetBits % 8 != 0 || !field.type.sizeBits || !field.type.alignmentBits ||
        *field.type.alignmentBits == 0 || *record.alignmentBits % *field.type.alignmentBits != 0 ||
        *field.offsetBits % *field.type.alignmentBits != 0 ||
        *field.type.sizeBits == 0 || *field.offsetBits > *record.sizeBits ||
        *field.type.sizeBits > *record.sizeBits - *field.offsetBits) return std::nullopt;
    auto mapped = mapCppScalarType(field.type);
    if (!mapped && !record.isCxx)
        mapped = projectCppSemanticEnumType(field.type, projectOrdinal, moduleName, lookup);
    bool pointeeReadonly = false;
    if (field.type.kind == CppTypeKind::Pointer && field.type.sizeBits == 64 &&
        field.type.element && !field.type.element->isVolatile)
    {
        auto element = mapCppScalarType(*field.type.element);
        if (!element && !record.isCxx)
            element = projectCppSemanticEnumType(*field.type.element, projectOrdinal, moduleName, lookup);
        if (!element && lookup && field.type.element->kind == CppTypeKind::Record)
        {
            const auto target = projectCppRecordTypeIdentity(*field.type.element, moduleName);
            const auto ordinal = target ? lookup->typeOrdinalForIdentity(target->key()) : std::nullopt;
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            if (target && imported && imported->projectOrdinal == projectOrdinal)
                element = CppScalarTypeMapping{"type:" + target->key(), field.type.element->canonicalSpelling, false, false};
        }
        if (element)
        {
            mapped = CppScalarTypeMapping{element->nativeSharpTypeKey + "*", field.type.canonicalSpelling,
                                          field.type.isConst, field.type.isVolatile};
            pointeeReadonly = field.type.element->isConst;
        }
    }
    if (!mapped || field.type.kind == CppTypeKind::Void) return std::nullopt;
    return CppSemanticInstanceField{projectOrdinal, identity->key(), field.name,
                                    std::move(*mapped), *field.offsetBits / 8, pointeeReadonly};
}

frontend::CppImportedInstanceField toCppImportedInstanceField(const CppSemanticInstanceField& field)
{
    return {field.projectOrdinal, field.ownerIdentityKey, field.name, field.type.nativeSharpTypeKey,
            field.byteOffset, field.type.isConst, field.type.isVolatile, field.pointeeReadonly};
}

std::optional<CppSemanticStaticField> projectCppSemanticStaticField(
    const CppRecordDeclaration& record, std::size_t fieldIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const frontend::ProjectNameLookup* lookup)
{
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || fieldIndex >= record.staticFields.size())
        return std::nullopt;
    const auto identity = projectCppRecordIdentity(record, moduleName);
    const auto& field = record.staticFields[fieldIndex];
    if (!identity || field.name.empty() || field.access != CppAccess::Public || field.threadLocal ||
        !field.externalSymbol || field.externalSymbol->empty()) return std::nullopt;
    auto mapped = mapCppScalarType(field.type);
    bool pointeeReadonly = false;
    if (field.type.kind == CppTypeKind::Pointer && field.type.sizeBits == 64 &&
        field.type.element && !field.type.element->isVolatile)
    {
        auto element = mapCppScalarType(*field.type.element);
        if (!element && lookup && field.type.element->kind == CppTypeKind::Record)
        {
            const auto target = projectCppRecordTypeIdentity(*field.type.element, moduleName);
            const auto ordinal = target ? lookup->typeOrdinalForIdentity(target->key()) : std::nullopt;
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            if (target && imported && imported->projectOrdinal == projectOrdinal)
                element = CppScalarTypeMapping{"type:" + target->key(), field.type.element->canonicalSpelling, false, false};
        }
        if (element)
        {
            mapped = CppScalarTypeMapping{element->nativeSharpTypeKey + "*", field.type.canonicalSpelling,
                                          field.type.isConst, field.type.isVolatile};
            pointeeReadonly = field.type.element->isConst;
        }
    }
    if (!mapped || field.type.kind == CppTypeKind::Void) return std::nullopt;
    return CppSemanticStaticField{projectOrdinal, identity->key(), field.name, *field.externalSymbol,
                                  std::move(*mapped), field.dllImport, pointeeReadonly};
}

frontend::CppImportedStaticField toCppImportedStaticField(const CppSemanticStaticField& field)
{
    return {field.projectOrdinal, field.ownerIdentityKey, field.name, field.externalSymbol,
            field.type.nativeSharpTypeKey, field.type.isConst, field.type.isVolatile, field.dllImport, field.pointeeReadonly};
}

bool provideExperimentalCppStaticFields(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedStaticField>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined);
    std::vector<frontend::CppImportedStaticField> result;
    for (const auto& group : groups.groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal) continue;
        for (const auto index : {*selected})
        {
            const auto& entry = entries[index];
            if (!entry.declaration.complete) continue;
            for (std::size_t i = 0; i < entry.declaration.staticFields.size(); ++i)
                if (auto field = projectCppSemanticStaticField(entry.declaration, i, group.projectOrdinal,
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &lookup))
                    result.push_back(toCppImportedStaticField(*field));
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool provideExperimentalCppInstanceFields(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedInstanceField>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined);
    std::vector<frontend::CppImportedInstanceField> result;
    for (const auto& group : groups.groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal ||
            (!owner->objectLayout && !owner->memberAccessLayout)) continue;
        for (const auto index : {*selected})
        {
            const auto& entry = entries[index];
            if (!entry.declaration.complete) continue;
            for (std::size_t fieldIndex = 0; fieldIndex < entry.declaration.fields.size(); ++fieldIndex)
                if (auto field = projectCppSemanticInstanceField(entry.declaration, fieldIndex, group.projectOrdinal,
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &lookup))
                    result.push_back(toCppImportedInstanceField(*field));
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

std::optional<compilation::TypeIdentity> projectCppRecordIdentity(
    const CppRecordDeclaration& record, std::string_view moduleName)
{
    if (record.anonymousContext || record.specializationContext || record.name.empty() || moduleName.empty())
        return std::nullopt;
    std::vector<compilation::TypeNameSegment> enclosing;
    enclosing.reserve(record.containingRecords.size());
    for (const auto& name : record.containingRecords) enclosing.push_back({name, 0});
    compilation::TypeIdentity identity(std::string(moduleName), record.namespaceSegments,
                                       std::move(enclosing), record.name);
    if (!identity.isValid()) return std::nullopt;
    return identity;
}

std::optional<compilation::TypeIdentity> projectCppRecordTypeIdentity(
    const CppType& type, std::string_view moduleName)
{
    if (type.kind != CppTypeKind::Record || !type.recordIdentity || moduleName.empty()) return std::nullopt;
    const auto& record = *type.recordIdentity;
    if (record.anonymousContext || record.specializationContext || record.name.empty()) return std::nullopt;
    std::vector<compilation::TypeNameSegment> enclosing;
    enclosing.reserve(record.containingRecords.size());
    for (const auto& name : record.containingRecords) enclosing.push_back({name, 0});
    compilation::TypeIdentity identity(std::string(moduleName), record.namespaceSegments,
                                       std::move(enclosing), record.name);
    if (!identity.isValid()) return std::nullopt;
    return identity;
}

CppSemanticRecordsResult collectCppSemanticRecords(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticRecordEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t index = 0; index < graph.projects().size(); ++index)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[index].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppSemanticRecordFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = index;
        }
        if (!owner) return CppSemanticRecordFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticRecordFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->recordsSnapshot();
            if (!declarations) return CppSemanticRecordFailure{group.projectFile, index, "C/C++ session is not available"};
            const auto target = session->optionsSnapshot().targetTriple;
            for (auto& declaration : *declarations)
            {
                auto identity = projectCppRecordIdentity(declaration, graph.projects()[*owner].moduleName());
                result.push_back({group.projectFile, index, *owner, target,
                                  std::move(declaration), std::move(identity)});
            }
        }
    }
    return result;
}

CppRecordGroups groupCppSemanticRecords(std::span<const CppSemanticRecordEntry> inventory)
{
    CppRecordGroups result;
    std::map<std::pair<std::size_t, std::string>, std::size_t> indexByIdentity;
    for (std::size_t index = 0; index < inventory.size(); ++index)
    {
        const auto& entry = inventory[index];
        if (!entry.identity) { result.unprojectedIndices.push_back(index); continue; }
        const auto key = std::make_pair(entry.projectOrdinal, entry.identity->key());
        const auto [where, inserted] = indexByIdentity.emplace(key, result.groups.size());
        if (inserted)
            result.groups.push_back({entry.projectOrdinal, *entry.identity, {}, CppRecordGroupStatus::ForwardOnly});
        result.groups[where->second].inventoryIndices.push_back(index);
    }
    for (auto& group : result.groups)
    {
        const CppSemanticRecordEntry* firstDefinition = nullptr;
        const auto& firstEntry = inventory[group.inventoryIndices.front()];
        std::size_t definitions = 0;
        bool conflict = false;
        for (const auto index : group.inventoryIndices)
        {
            const auto& entry = inventory[index];
            conflict |= entry.targetTriple != firstEntry.targetTriple;
            if (!entry.declaration.complete) continue;
            ++definitions;
            if (!firstDefinition) { firstDefinition = &entry; continue; }
            const auto& a = firstDefinition->declaration;
            const auto& b = entry.declaration;
            conflict |= (a.sizeBits && b.sizeBits && a.sizeBits != b.sizeBits) ||
                (a.alignmentBits && b.alignmentBits && a.alignmentBits != b.alignmentBits) ||
                (a.odrHash && b.odrHash && a.odrHash != b.odrHash);
        }
        group.status = conflict ? CppRecordGroupStatus::Conflict :
            definitions == 0 ? CppRecordGroupStatus::ForwardOnly :
            definitions == 1 ? CppRecordGroupStatus::OneDefinition : CppRecordGroupStatus::MultipleDefinitionsUnverified;
    }
    return result;
}

CppRecordStructuralComparisonResult compareCppRecordDefinitions(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups,
    std::span<const CppProjectSessions> projects)
{
    std::vector<CppRecordStructuralComparison> result;
    std::vector<bool> claimed(inventory.size());
    std::set<std::pair<std::size_t, std::string>> identities;
    for (std::size_t groupIndex = 0; groupIndex < groups.groups.size(); ++groupIndex)
    {
        const auto& group = groups.groups[groupIndex];
        if (!group.identity.isValid() || group.inventoryIndices.empty() ||
            !identities.emplace(group.projectOrdinal, group.identity.key()).second)
            return CppRecordStructuralComparisonFailure{"invalid or duplicate record group"};
        std::vector<const CppInteropSession*> definitions;
        std::string qualifiedName;
        for (const auto index : group.inventoryIndices)
        {
            if (index >= inventory.size() || claimed[index])
                return CppRecordStructuralComparisonFailure{"invalid or duplicate record inventory index"};
            claimed[index] = true;
            const auto& entry = inventory[index];
            if (!entry.identity || entry.identity->key() != group.identity.key() ||
                entry.projectOrdinal != group.projectOrdinal || entry.declaration.qualifiedName.empty())
                return CppRecordStructuralComparisonFailure{"record inventory does not match group identity"};
            if (qualifiedName.empty()) qualifiedName = entry.declaration.qualifiedName;
            else if (qualifiedName != entry.declaration.qualifiedName)
                return CppRecordStructuralComparisonFailure{"record group has inconsistent qualified names"};
            const CppProjectSessions* owner = nullptr;
            for (const auto& candidate : projects)
            {
                std::error_code ec;
                if (std::filesystem::equivalent(candidate.projectFile, entry.projectFile, ec) && !ec)
                {
                    if (owner) return CppRecordStructuralComparisonFailure{"ambiguous session owner"};
                    owner = &candidate;
                }
            }
            if (!owner || entry.referenceIndex >= owner->sessions.size() || !owner->sessions[entry.referenceIndex])
                return CppRecordStructuralComparisonFailure{"record session is missing"};
            if (owner->sessions[entry.referenceIndex]->optionsSnapshot().targetTriple != entry.targetTriple)
                return CppRecordStructuralComparisonFailure{"record target does not match its session"};
            if (entry.declaration.complete) definitions.push_back(owner->sessions[entry.referenceIndex].get());
        }
        if (definitions.size() < 2) continue;
        CppRecordStructuralVerdict verdict = CppRecordStructuralVerdict::Equivalent;
        for (std::size_t i = 1; i < definitions.size(); ++i)
        {
            const auto comparison = definitions.front()->structurallyEquivalentRecord(*definitions[i], qualifiedName);
            if (comparison && !*comparison) verdict = CppRecordStructuralVerdict::Different;
            else if (!comparison && verdict != CppRecordStructuralVerdict::Different)
                verdict = CppRecordStructuralVerdict::Unavailable;
        }
        result.push_back({groupIndex, definitions.size(), verdict});
    }
    return result;
}

namespace {
bool sameMemberType(const CppType& a, const CppType& b, unsigned depth = 0)
{
    if (depth > 64 || a.kind != b.kind || a.canonicalSpelling != b.canonicalSpelling ||
        a.unqualifiedCanonicalSpelling != b.unqualifiedCanonicalSpelling ||
        a.isConst != b.isConst || a.isVolatile != b.isVolatile || a.sizeBits != b.sizeBits ||
        a.alignmentBits != b.alignmentBits || a.arrayCount != b.arrayCount ||
        static_cast<bool>(a.element) != static_cast<bool>(b.element)) return false;
    return !a.element || sameMemberType(*a.element, *b.element, depth + 1);
}
bool sameCapturedMembers(const CppRecordDeclaration& a, const CppRecordDeclaration& b)
{
    if (a.kind != b.kind || a.access != b.access || a.implicitLifetimeMembers != b.implicitLifetimeMembers || a.bases.size() != b.bases.size() ||
        a.fields.size() != b.fields.size() || a.staticFields.size() != b.staticFields.size() ||
        a.methods.size() != b.methods.size()) return false;
    for (std::size_t i = 0; i < a.bases.size(); ++i)
        if (a.bases[i].access != b.bases[i].access || a.bases[i].isVirtual != b.bases[i].isVirtual ||
            a.bases[i].offsetBits != b.bases[i].offsetBits || !sameMemberType(a.bases[i].type, b.bases[i].type)) return false;
    for (std::size_t i = 0; i < a.fields.size(); ++i)
        if (a.fields[i].name != b.fields[i].name || a.fields[i].access != b.fields[i].access ||
            a.fields[i].offsetBits != b.fields[i].offsetBits || a.fields[i].bitWidth != b.fields[i].bitWidth ||
            !sameMemberType(a.fields[i].type, b.fields[i].type)) return false;
    for (std::size_t i = 0; i < a.staticFields.size(); ++i)
        if (a.staticFields[i].name != b.staticFields[i].name || a.staticFields[i].access != b.staticFields[i].access ||
            a.staticFields[i].threadLocal != b.staticFields[i].threadLocal || a.staticFields[i].dllImport != b.staticFields[i].dllImport ||
            a.staticFields[i].externalSymbol != b.staticFields[i].externalSymbol ||
            !sameMemberType(a.staticFields[i].type, b.staticFields[i].type)) return false;
    for (std::size_t i = 0; i < a.methods.size(); ++i)
    {
        const auto& x = a.methods[i]; const auto& y = b.methods[i];
        if (x.kind != y.kind || x.name != y.name || x.qualifiedName != y.qualifiedName ||
            x.canonicalFunctionType != y.canonicalFunctionType || x.callingConvention != y.callingConvention ||
            x.refQualifier != y.refQualifier || x.access != y.access || x.isStatic != y.isStatic ||
            x.isConst != y.isConst || x.isVolatile != y.isVolatile || x.isNonThrowing != y.isNonThrowing ||
            x.dllImport != y.dllImport || x.isVirtual != y.isVirtual || x.isPureVirtual != y.isPureVirtual ||
            x.isDeleted != y.isDeleted || x.variadic != y.variadic || x.externalSymbol != y.externalSymbol ||
            x.parameters.size() != y.parameters.size() || !sameMemberType(x.returnType, y.returnType)) return false;
        for (std::size_t p = 0; p < x.parameters.size(); ++p)
            if (x.parameters[p].name != y.parameters[p].name ||
                x.parameters[p].hasDefaultArgument != y.parameters[p].hasDefaultArgument ||
                !sameMemberType(x.parameters[p].type, y.parameters[p].type)) return false;
    }
    return true;
}
}

std::optional<std::size_t> selectCppRecordMemberDefinition(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group)
{
    if (group.status != CppRecordGroupStatus::OneDefinition &&
        group.status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible) return std::nullopt;
    std::optional<std::size_t> first;
    for (const auto i : group.inventoryIndices)
    {
        if (i >= inventory.size()) return std::nullopt;
        const auto& item = inventory[i].declaration;
        if (!item.complete) continue;
        if (!first) { first = i; continue; }
        const auto& prior = inventory[*first].declaration;
        if (!prior.location || !item.location || prior.location->line != item.location->line ||
            prior.location->column != item.location->column || !sameCapturedMembers(prior, item)) return std::nullopt;
        std::error_code ec;
        if (!std::filesystem::equivalent(prior.location->file, item.location->file, ec) || ec) return std::nullopt;
    }
    return first;
}

CppRecordRefinementResult refineCppRecordGroupsForPointerTransport(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups,
    std::span<const CppProjectSessions> projects)
{
    auto compared = compareCppRecordDefinitions(inventory, groups, projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&compared)) return *failure;
    auto result = groups;
    for (const auto& item : std::get<std::vector<CppRecordStructuralComparison>>(compared))
    {
        auto& group = result.groups[item.groupIndex];
        if (group.status != CppRecordGroupStatus::MultipleDefinitionsUnverified ||
            item.verdict != CppRecordStructuralVerdict::Equivalent) continue;
        const CppRecordDeclaration* first = nullptr;
        bool consistent = true;
        for (const auto index : group.inventoryIndices)
        {
            const auto& record = inventory[index].declaration;
            if (!record.complete) continue;
            if (!record.odrHash || !record.sizeBits || !record.alignmentBits)
            { consistent = false; break; }
            if (!first) first = &record;
            else if (record.odrHash != first->odrHash || record.sizeBits != first->sizeBits ||
                     record.alignmentBits != first->alignmentBits)
            { consistent = false; break; }
        }
        if (consistent && first) group.status = CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
    }
    return result;
}

CppSignatureRecordResult collectCppSignatureRecordDependencies(
    const compilation::ProjectGraph& graph, std::span<const CppSemanticInventoryEntry> functions,
    const CppRecordGroups& groups)
{
    std::map<std::pair<std::size_t, std::string>, std::size_t> byIdentity;
    for (std::size_t index = 0; index < groups.groups.size(); ++index)
    {
        const auto& group = groups.groups[index];
        if (group.projectOrdinal >= graph.projects().size() || !group.identity.isValid() ||
            group.identity.moduleName() != graph.projects()[group.projectOrdinal].moduleName() ||
            !byIdentity.emplace(std::make_pair(group.projectOrdinal, group.identity.key()), index).second)
            return CppSignatureRecordFailure{"invalid or duplicate record group identity"};
    }
    std::vector<CppSignatureRecordDependency> result;
    for (std::size_t functionIndex = 0; functionIndex < functions.size(); ++functionIndex)
    {
        const auto& function = functions[functionIndex];
        if (function.projectOrdinal >= graph.projects().size())
            return CppSignatureRecordFailure{"function project is not in graph"};
        const auto module = graph.projects()[function.projectOrdinal].moduleName();
        auto collect = [&](const CppType& initial, std::optional<std::size_t> parameter) {
            const CppType* type = &initial;
            std::size_t depth = 0;
            while (type && depth <= 64)
            {
                if (type->kind == CppTypeKind::Record)
                {
                    const auto identity = projectCppRecordTypeIdentity(*type, module);
                    if (!identity)
                        result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Unprojectable, std::nullopt});
                    else
                    {
                        const auto found = byIdentity.find({function.projectOrdinal, identity->key()});
                        if (found == byIdentity.end())
                            result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Missing, std::nullopt});
                        else
                            result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Matched, found->second});
                    }
                    return true;
                }
                if (type->kind != CppTypeKind::Pointer && type->kind != CppTypeKind::LValueReference &&
                    type->kind != CppTypeKind::RValueReference && type->kind != CppTypeKind::Array)
                    return true;
                type = type->element.get();
                ++depth;
            }
            return false;
        };
        if (!collect(function.declaration.returnType, std::nullopt))
            return CppSignatureRecordFailure{"signature type nesting is incomplete or exceeds limit"};
        for (std::size_t index = 0; index < function.declaration.parameters.size(); ++index)
            if (!collect(function.declaration.parameters[index].type, index))
                return CppSignatureRecordFailure{"signature type nesting is incomplete or exceeds limit"};
    }
    return result;
}

CppRecordObservationResult prepareCppRecordObservation(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups)
{
    CppRecordObservationInputs result;
    std::vector<bool> claimed(inventory.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.groups.size(); ++groupIndex)
    {
        const auto& group = groups.groups[groupIndex];
        if (group.inventoryIndices.empty() || !group.identity.isValid())
            return CppRecordObservationFailure{"empty or invalid record group"};
        std::string sourcePath;
        std::optional<frontend::CppImportedObjectLayout> objectLayout;
        bool deferred = !group.identity.containingTypes().empty();
        for (const auto entryIndex : group.inventoryIndices)
        {
            if (entryIndex >= inventory.size() || claimed[entryIndex])
                return CppRecordObservationFailure{"invalid or repeated record inventory index"};
            claimed[entryIndex] = true;
            const auto& entry = inventory[entryIndex];
            if (!entry.identity || entry.projectOrdinal != group.projectOrdinal ||
                entry.identity->key() != group.identity.key())
                return CppRecordObservationFailure{"record inventory does not match group identity"};
            if (entry.declaration.location && sourcePath.empty())
                sourcePath = entry.declaration.location->file.generic_string();
            if (group.status == CppRecordGroupStatus::OneDefinition && entry.declaration.complete &&
                entry.declaration.sizeBits && entry.declaration.alignmentBits)
            {
                const auto sizeBits = *entry.declaration.sizeBits;
                const auto alignmentBits = *entry.declaration.alignmentBits;
                if (sizeBits == 0 || alignmentBits == 0 || sizeBits % 8 != 0 || alignmentBits % 8 != 0)
                    return CppRecordObservationFailure{"record dimensions are not representable in bytes"};
                const auto size = sizeBits / 8;
                const auto alignment = alignmentBits / 8;
                if ((alignment & (alignment - 1)) != 0 || size % alignment != 0 || objectLayout)
                    return CppRecordObservationFailure{"record dimensions or definition count are invalid"};
                objectLayout = frontend::CppImportedObjectLayout{size, alignment};
            }
        }
        if (deferred || sourcePath.empty())
        {
            result.deferredGroups.push_back(groupIndex);
            continue;
        }
        // This struct is only a name-lookup placeholder; it does not describe the C++ ABI.
        std::optional<frontend::CppImportedObjectLayout> memberAccessLayout;
        if (group.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible)
            if (const auto chosen = selectCppRecordMemberDefinition(inventory, group))
            {
                const auto& record = inventory[*chosen].declaration;
                if (record.sizeBits && record.alignmentBits && *record.sizeBits % 8 == 0 && *record.alignmentBits % 8 == 0)
                    memberAccessLayout = frontend::CppImportedObjectLayout{*record.sizeBits / 8, *record.alignmentBits / 8};
            }
        result.types.push_back({group.identity, frontend::NamedTypeDeclarationKind::Struct,
                                frontend::NamedTypeAccessibility::Public, group.projectOrdinal,
                                std::move(sourcePath), objectLayout, memberAccessLayout});
        result.groupByImportedType.push_back(groupIndex);
    }
    return result;
}

bool provideExperimentalCppRecordTypes(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    {
        error = "C/C++ record collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    const auto& inventory = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    const auto groups = groupCppSemanticRecords(inventory);
    auto prepared = prepareCppRecordObservation(inventory, groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared))
    {
        error = "C/C++ record preparation failed: " + failure->message;
        return false;
    }
    auto& inputs = std::get<CppRecordObservationInputs>(prepared);
    for (const auto index : inputs.groupByImportedType)
        if (groups.groups[index].status != CppRecordGroupStatus::ForwardOnly &&
            groups.groups[index].status != CppRecordGroupStatus::OneDefinition)
        {
            error = "C/C++ record has conflicting or unverified definitions: " + groups.groups[index].identity.key();
            return false;
        }
    output = std::move(inputs.types);
    error.clear();
    return true;
}

bool provideExperimentalCppRecordCandidates(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    {
        error = failure->message;
        return false;
    }
    const auto& inventory = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    const auto groups = groupCppSemanticRecords(inventory);
    auto prepared = prepareCppRecordObservation(inventory, groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared))
    {
        error = failure->message;
        return false;
    }
    output = std::move(std::get<CppRecordObservationInputs>(prepared).types);
    error.clear();
    return true;
}

CppRecordBindingResult selectCppRecordBindings(
    const frontend::ProjectNameLookup& lookup, const CppRecordGroups& groups,
    std::span<const std::size_t> groupByImportedType)
{
    std::vector<std::optional<std::size_t>> groupByOrdinal(lookup.namedTypes().size());
    std::vector<bool> seen(groupByImportedType.size());
    std::unordered_set<std::size_t> seenGroups;
    for (std::size_t ordinal = 0; ordinal < groupByOrdinal.size(); ++ordinal)
    {
        const auto imported = lookup.cppImportedTypeIndex(ordinal);
        if (!imported) continue;
        if (*imported >= groupByImportedType.size())
        {
            const auto* type = lookup.cppImportedType(ordinal);
            if (type && type->kind == frontend::NamedTypeDeclarationKind::Enum) continue;
        }
        if (*imported >= groupByImportedType.size() || seen[*imported])
            return CppRecordBindingFailure{"invalid imported type mapping"};
        const auto groupIndex = groupByImportedType[*imported];
        if (groupIndex >= groups.groups.size() || !seenGroups.insert(groupIndex).second)
            return CppRecordBindingFailure{"invalid or duplicate record group"};
        const auto& declaration = lookup.namedTypes()[ordinal];
        const auto& group = groups.groups[groupIndex];
        if (declaration.projectOrdinal != group.projectOrdinal || declaration.identity.key() != group.identity.key())
            return CppRecordBindingFailure{"record group does not match installed type"};
        seen[*imported] = true;
        groupByOrdinal[ordinal] = groupIndex;
    }
    if (std::find(seen.begin(), seen.end(), false) != seen.end())
        return CppRecordBindingFailure{"imported type mapping was not installed"};
    CppRecordBindingSelection result;
    auto append = [&](std::vector<std::size_t>& destination, std::size_t ordinal) {
        if (ordinal >= groupByOrdinal.size() || !groupByOrdinal[ordinal]) return;
        destination.push_back(*groupByOrdinal[ordinal]);
    };
    for (const auto& binding : lookup.bindings())
        if (binding.kind == frontend::TypeNameBindingKind::Resolved && binding.targetTypeOrdinal)
            append(result.referencedGroups, *binding.targetTypeOrdinal);
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals)
            append(result.expressionCandidateGroups, ordinal);
    auto unique = [](std::vector<std::size_t>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    unique(result.referencedGroups);
    unique(result.expressionCandidateGroups);
    return result;
}

CppRecordObservationResult selectExperimentalCppRecordTypes(const CppRecordObservation& observation)
{
    auto prepared = prepareCppRecordObservation(observation.inventory, observation.groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared)) return *failure;
    auto inputs = std::move(std::get<CppRecordObservationInputs>(prepared));
    const auto count = observation.groups.groups.size();
    std::vector<bool> collision(count), demanded(count);
    std::set<std::pair<std::size_t, std::string>> identities;
    for (const auto& group : observation.groups.groups)
        if (!identities.emplace(group.projectOrdinal, group.identity.key()).second)
            return CppRecordObservationFailure{"duplicate record group identity"};
    auto mark = [count](const std::vector<std::size_t>& indices, std::vector<bool>& destination) {
        for (const auto index : indices)
        {
            if (index >= count || destination[index]) return false;
            destination[index] = true;
        }
        return true;
    };
    if (!mark(observation.collidingGroups, collision))
        return CppRecordObservationFailure{"invalid or duplicate colliding record group"};
    std::vector<bool> referencedCollision(count), expressionCollision(count);
    if (!mark(observation.referencedCollidingGroups, referencedCollision) ||
        !mark(observation.expressionCollidingGroups, expressionCollision))
        return CppRecordObservationFailure{"invalid or duplicate observed native collision"};
    for (std::size_t i = 0; i < count; ++i)
        if ((referencedCollision[i] || expressionCollision[i]) && !collision[i])
            return CppRecordObservationFailure{"observed native collision is not a colliding group"};
    std::vector<bool> referenced(count), expression(count);
    if (!mark(observation.selection.referencedGroups, referenced) ||
        !mark(observation.selection.expressionCandidateGroups, expression))
        return CppRecordObservationFailure{"invalid or duplicate observed record group"};
    for (std::size_t i = 0; i < count; ++i) demanded[i] = referenced[i] || expression[i];

    CppRecordObservationInputs selected;
    selected.deferredGroups = std::move(inputs.deferredGroups);
    for (const auto groupIndex : selected.deferredGroups)
        if (groupIndex >= count || demanded[groupIndex])
            return CppRecordObservationFailure{"referenced C/C++ record is not representable for name lookup"};
    for (std::size_t i = 0; i < inputs.types.size(); ++i)
    {
        const auto groupIndex = inputs.groupByImportedType[i];
        if (collision[groupIndex])
        {
            if (demanded[groupIndex])
                return CppRecordObservationFailure{"colliding record cannot be selected as an imported type"};
            continue;
        }
        const auto status = observation.groups.groups[groupIndex].status;
        if (status != CppRecordGroupStatus::ForwardOnly && status != CppRecordGroupStatus::OneDefinition &&
            status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible)
        {
            if (demanded[groupIndex])
                return CppRecordObservationFailure{"referenced C/C++ record has conflicting or unverified definitions: " +
                                                   observation.groups.groups[groupIndex].identity.key()};
            selected.deferredGroups.push_back(groupIndex);
            continue;
        }
        selected.types.push_back(std::move(inputs.types[i]));
        selected.groupByImportedType.push_back(groupIndex);
    }
    return selected;
}

CppRecordObservationRunResult observeCppRecords(const frontend::ProjectCompilation& compilation)
{
    const auto* graph = compilation.projectGraph();
    if (!graph) return CppRecordObservationFailure{"project syntax has not been prepared"};
    auto collected = collectCppSemanticRecords(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
        return CppRecordObservationFailure{failure->message};
    CppRecordObservation result;
    result.inventory = std::move(std::get<std::vector<CppSemanticRecordEntry>>(collected));
    result.groups = groupCppSemanticRecords(result.inventory);
    auto refined = refineCppRecordGroupsForPointerTransport(result.inventory, result.groups, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
        return CppRecordObservationFailure{failure->message};
    result.groups = std::move(std::get<CppRecordGroups>(refined));
    auto prepared = prepareCppRecordObservation(result.inventory, result.groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared)) return *failure;
    auto inputs = std::move(std::get<CppRecordObservationInputs>(prepared));
    result.deferredGroups = inputs.deferredGroups;
    auto namespaceInventory = collectCppNamespaces(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&namespaceInventory))
        return CppRecordObservationFailure{failure->message};
    auto visible = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(namespaceInventory));
    if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&visible))
        return CppRecordObservationFailure{failure->message};
    diagnostics::DiagnosticEngine diagnostics;
    std::vector<std::unique_ptr<frontend::ProjectSyntaxUnit>> units;
    std::string error;
    if (!compilation.cloneSyntaxForObservation(units, diagnostics, error))
        return CppRecordObservationFailure{error};
    auto copySnapshots = [&](std::vector<frontend::PrecompiledSnapshotUnit>& snapshots) {
        for (const auto& original : compilation.precompiledSnapshots())
        {
            if (original.unitOrdinal >= units.size() || !units[original.unitOrdinal] ||
                units[original.unitOrdinal]->fileId() != original.fileId)
                return false;
            auto copy = original;
            copy.index.build(units[original.unitOrdinal]->syntaxTree());
            if (copy.index.nodes().size() != original.index.nodes().size() ||
                copy.index.types().size() != original.index.types().size() ||
                copy.index.kindDigest() != original.index.kindDigest())
                return false;
            snapshots.push_back(std::move(copy));
        }
        return true;
    };
    std::vector<frontend::PrecompiledSnapshotUnit> nativeSnapshots;
    if (!copySnapshots(nativeSnapshots))
        return CppRecordObservationFailure{"precompiled snapshot does not match observation syntax"};
    const auto& namespaces = std::get<std::vector<std::pair<std::size_t, std::string>>>(visible);
    frontend::ProjectNameLookup nativeLookup;
    nativeLookup.setPrecompiledSnapshots(nativeSnapshots);
    if (!nativeLookup.setImportedNamespaces(namespaces))
        return CppRecordObservationFailure{"cannot configure native observation lookup"};
    nativeLookup.build(*graph, units, diagnostics);
    if (!nativeLookup.hasTypeIndex())
        return CppRecordObservationFailure{"cannot index NativeSharp declarations for observation"};
    std::vector<frontend::CppImportedTypeDeclaration> filteredTypes;
    std::vector<std::size_t> filteredGroups;
    for (std::size_t index = 0; index < inputs.types.size(); ++index)
    {
        const auto& imported = inputs.types[index];
        const auto nativeOrdinal = nativeLookup.typeOrdinalForIdentity(imported.identity.key());
        const auto* native = nativeOrdinal ? nativeLookup.typeForOrdinal(*nativeOrdinal) : nullptr;
        if (native && native->projectOrdinal == imported.projectOrdinal)
            result.collidingGroups.push_back(inputs.groupByImportedType[index]);
        else
        {
            filteredTypes.push_back(imported);
            filteredGroups.push_back(inputs.groupByImportedType[index]);
        }
    }
    units.clear();
    diagnostics::DiagnosticEngine finalDiagnostics;
    if (!compilation.cloneSyntaxForObservation(units, finalDiagnostics, error))
        return CppRecordObservationFailure{error};
    std::vector<frontend::PrecompiledSnapshotUnit> finalSnapshots;
    if (!copySnapshots(finalSnapshots))
        return CppRecordObservationFailure{"precompiled snapshot does not match observation syntax"};
    frontend::ProjectNameLookup lookup;
    lookup.setPrecompiledSnapshots(finalSnapshots);
    std::vector<frontend::CppImportedTypeAlias> aliases;
    if (!provideCppTypeAliases(*graph, compilation.cppProjectSessions(), aliases, error))
        return CppRecordObservationFailure{std::move(error)};
    std::vector<frontend::CppImportedTypeDeclaration> enums;
    if (!provideCppEnumTypes(*graph, compilation.cppProjectSessions(), enums, error))
        return CppRecordObservationFailure{std::move(error)};
    std::set<std::string> collidingEnumIdentities;
    for (auto& enumeration : enums)
    {
        const auto ordinal = nativeLookup.typeOrdinalForIdentity(enumeration.identity.key());
        const auto* native = ordinal ? nativeLookup.typeForOrdinal(*ordinal) : nullptr;
        const bool collidesWithRecord = std::any_of(filteredTypes.begin(), filteredTypes.end(),
            [&](const auto& record) { return record.identity.key() == enumeration.identity.key(); });
        if (native || collidesWithRecord) collidingEnumIdentities.insert(enumeration.identity.key());
        else filteredTypes.push_back(std::move(enumeration));
    }
    result.collidingEnumIdentities.assign(collidingEnumIdentities.begin(), collidingEnumIdentities.end());
    if (!lookup.setImportedNamespaces(std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(visible))) ||
        !lookup.setCppImportedTypes(std::move(filteredTypes)) ||
        !lookup.setCppImportedTypeAliases(std::move(aliases)))
        return CppRecordObservationFailure{"cannot configure observation lookup"};
    if (!lookup.build(*graph, units, finalDiagnostics))
    {
        error = "record observation name lookup failed";
        if (!finalDiagnostics.diagnostics().empty()) error += ": " + finalDiagnostics.diagnostics().front().message();
        return CppRecordObservationFailure{std::move(error)};
    }
    const auto enumCollision = [&](std::size_t ordinal) {
        return ordinal < lookup.namedTypes().size() &&
            collidingEnumIdentities.contains(lookup.namedTypes()[ordinal].identity.key());
    };
    for (const auto& binding : lookup.bindings())
        if (binding.targetTypeOrdinal && enumCollision(*binding.targetTypeOrdinal))
            return CppRecordObservationFailure{"C enum conflicts with another used type: " + binding.writtenName};
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals)
            if (enumCollision(ordinal))
                return CppRecordObservationFailure{"C enum conflicts with another used expression type"};
    std::unordered_map<std::size_t, std::size_t> collisionByOrdinal;
    for (const auto groupIndex : result.collidingGroups)
    {
        const auto ordinal = lookup.typeOrdinalForIdentity(result.groups.groups[groupIndex].identity.key());
        if (!ordinal || !collisionByOrdinal.emplace(*ordinal, groupIndex).second)
            return CppRecordObservationFailure{"colliding NativeSharp type is missing or maps to multiple groups"};
    }
    for (const auto& binding : lookup.bindings())
        if (binding.kind == frontend::TypeNameBindingKind::Resolved && binding.targetTypeOrdinal)
            if (const auto found = collisionByOrdinal.find(*binding.targetTypeOrdinal); found != collisionByOrdinal.end())
                result.referencedCollidingGroups.push_back(found->second);
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals)
            if (const auto found = collisionByOrdinal.find(ordinal); found != collisionByOrdinal.end())
                result.expressionCollidingGroups.push_back(found->second);
    auto deduplicate = [](std::vector<std::size_t>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    deduplicate(result.referencedCollidingGroups);
    deduplicate(result.expressionCollidingGroups);
    auto selected = selectCppRecordBindings(lookup, result.groups, filteredGroups);
    if (const auto* failure = std::get_if<CppRecordBindingFailure>(&selected))
        return CppRecordObservationFailure{failure->message};
    result.selection = std::move(std::get<CppRecordBindingSelection>(selected));
    std::set<std::string> enumIdentities;
    const auto addEnum = [&](std::size_t ordinal) {
        const auto* type = lookup.cppImportedType(ordinal);
        if (type && type->kind == frontend::NamedTypeDeclarationKind::Enum &&
            enumIdentities.insert(type->identity.key()).second)
            result.referencedEnumTypes.push_back(*type);
    };
    for (const auto& binding : lookup.bindings())
        if (binding.targetTypeOrdinal) addEnum(*binding.targetTypeOrdinal);
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals) addEnum(ordinal);
    return result;
}

bool selectExperimentalCppRecordTypesForCompilation(
    const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    auto observed = observeCppRecords(compilation);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&observed))
    {
        error = failure->message;
        return false;
    }
    auto selected = selectExperimentalCppRecordTypes(std::get<CppRecordObservation>(observed));
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&selected))
    {
        error = failure->message;
        return false;
    }
    output = std::move(std::get<CppRecordObservationInputs>(selected).types);
    const auto& observation = std::get<CppRecordObservation>(observed);
    std::vector<frontend::CppImportedTypeDeclaration> enumCandidates;
    const auto* graph = compilation.projectGraph();
    if (!graph) { error = "project graph is unavailable for C enum dependencies"; return false; }
    if (!provideCppEnumTypes(*graph, compilation.cppProjectSessions(), enumCandidates, error)) return false;
    std::set<std::string> selectedIdentities;
    for (const auto& type : output) selectedIdentities.insert(type.identity.key());
    const auto selectedRecordIdentities = selectedIdentities;
    const auto addEnum = [&](const frontend::CppImportedTypeDeclaration& type) {
        if (selectedIdentities.insert(type.identity.key()).second) output.push_back(type);
    };
    for (const auto& enumeration : observation.referencedEnumTypes) addEnum(enumeration);
    for (const auto& group : observation.groups.groups)
    {
        if (!selectedIdentities.contains(group.identity.key())) continue;
        const auto definition = selectCppRecordMemberDefinition(observation.inventory, group);
        if (!definition || *definition >= observation.inventory.size()) continue;
        const auto& record = observation.inventory[*definition].declaration;
        if (record.isCxx) continue;
        for (const auto& field : record.fields)
        {
            const CppType* type = &field.type;
            for (std::size_t depth = 0; type && depth < 64 &&
                 (type->kind == CppTypeKind::Pointer || type->kind == CppTypeKind::Array); ++depth)
                type = type->element.get();
            if (!type || type->kind != CppTypeKind::Enumeration || !type->enumIdentity ||
                type->enumIdentity->anonymousContext) continue;
            const compilation::TypeIdentity identity(group.identity.moduleName(), {}, {}, type->enumIdentity->name);
            const auto candidate = std::find_if(enumCandidates.begin(), enumCandidates.end(), [&](const auto& item) {
                return item.projectOrdinal == group.projectOrdinal && item.identity.key() == identity.key();
            });
            if (candidate == enumCandidates.end()) continue;
            if (!candidate->deferredImportError.empty()) { error = candidate->deferredImportError; return false; }
            if (std::find(observation.collidingEnumIdentities.begin(), observation.collidingEnumIdentities.end(),
                          candidate->identity.key()) != observation.collidingEnumIdentities.end())
            { error = "C enum field conflicts with another type: " + candidate->identity.name(); return false; }
            if (selectedRecordIdentities.contains(candidate->identity.key()))
            { error = "C enum field conflicts with a selected record: " + candidate->identity.name(); return false; }
            addEnum(*candidate);
        }
    }
    error.clear();
    return true;
}
}
