#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "../Internal/CppCRecordMetadataCodec.h"

#include <map>
#include <algorithm>
#include <set>
#include <tuple>
#include <utility>

namespace nativesharp::interop
{
std::variant<std::vector<std::size_t>, CppSignatureEnumFailure> selectCppDemandedEnumCandidates(
    std::span<const CppSignatureEnumDependency> dependencies,
    std::span<const CppSemanticDemandCandidates> demands, std::size_t functionCount,
    std::size_t enumCandidateCount)
{
    std::set<std::size_t> selectedFunctions;
    for (const auto& demand : demands)
    {
        for (const auto& group : demand.candidates.groups)
            selectedFunctions.insert(group.inventoryIndices.begin(), group.inventoryIndices.end());
        selectedFunctions.insert(demand.candidates.unprojectedIndices.begin(), demand.candidates.unprojectedIndices.end());
        if (demand.deferredConflict)
        {
            selectedFunctions.insert(demand.deferredConflict->firstIndex);
            selectedFunctions.insert(demand.deferredConflict->conflictingIndex);
        }
    }
    if (!selectedFunctions.empty() && *selectedFunctions.rbegin() >= functionCount)
        return CppSignatureEnumFailure{"demand references a function outside its inventory"};
    std::set<std::size_t> selectedEnums;
    for (const auto& dependency : dependencies)
    {
        if (dependency.functionIndex >= functionCount)
            return CppSignatureEnumFailure{"enum dependency references a function outside its inventory"};
        if (!selectedFunctions.contains(dependency.functionIndex) ||
            dependency.status != CppSignatureEnumStatus::Matched) continue;
        if (!dependency.candidateIndex || *dependency.candidateIndex >= enumCandidateCount)
            return CppSignatureEnumFailure{"enum dependency references a candidate outside its inventory"};
        selectedEnums.insert(*dependency.candidateIndex);
    }
    return std::vector<std::size_t>(selectedEnums.begin(), selectedEnums.end());
}
CppSemanticFunctionGroupsResult groupCppSemanticFunctions(
    std::span<const CppSemanticInventoryEntry> entries)
{
    using Key = std::tuple<std::size_t, std::string, std::string>;
    std::map<Key, std::size_t> groupByKey;
    std::map<Key, std::size_t> firstExternalByKey;
    std::map<std::pair<std::size_t, std::string>, std::size_t> firstExternalByUsr;
    CppSemanticFunctionGroups result;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const auto& entry = entries[index];
        const auto& declaration = entry.declaration;
        if (declaration.externalLinkage && declaration.externalSymbol && !declaration.externalSymbol->empty())
        {
            if (entry.targetTriple.empty() || declaration.canonicalFunctionType.empty() ||
                declaration.callingConvention.empty() || declaration.qualifiedName.empty())
                return CppSemanticFunctionGroupFailure{index, index, "function identity is incomplete"};
            if (declaration.clangUsr && !declaration.clangUsr->empty())
            {
                const auto [prior, inserted] = firstExternalByUsr.emplace(
                    std::make_pair(entry.projectOrdinal, *declaration.clangUsr), index);
                if (!inserted)
                {
                    const auto& original = entries[prior->second];
                    if (entry.targetTriple != original.targetTriple ||
                        declaration.externalSymbol != original.declaration.externalSymbol ||
                        declaration.canonicalFunctionType != original.declaration.canonicalFunctionType ||
                        declaration.callingConvention != original.declaration.callingConvention ||
                        declaration.languageLinkage != original.declaration.languageLinkage ||
                        declaration.dllImport != original.declaration.dllImport)
                        return CppSemanticFunctionGroupFailure{
                            prior->second, index, "conflicting declarations for one Clang USR"};
                }
            }
            Key key{entry.projectOrdinal, {}, *declaration.externalSymbol};
            const auto [first, inserted] = firstExternalByKey.emplace(std::move(key), index);
            if (!inserted)
            {
                const auto& original = entries[first->second];
                if (entry.targetTriple != original.targetTriple ||
                    declaration.canonicalFunctionType != original.declaration.canonicalFunctionType ||
                    declaration.callingConvention != original.declaration.callingConvention ||
                    declaration.languageLinkage != original.declaration.languageLinkage ||
                    declaration.dllImport != original.declaration.dllImport)
                    return CppSemanticFunctionGroupFailure{
                        first->second, index, "conflicting declarations for one external function"};
            }
        }
        const auto* symbol = std::get_if<frontend::NamespaceFunctionSymbol>(&entry.projection);
        if (!symbol)
        {
            result.unprojectedIndices.push_back(index);
            continue;
        }
        if (entry.targetTriple.empty() || declaration.canonicalFunctionType.empty() ||
            declaration.callingConvention.empty() || !declaration.externalSymbol ||
            symbol->projectOrdinal != entry.projectOrdinal)
            return CppSemanticFunctionGroupFailure{index, index, "function identity is incomplete"};
        Key key{symbol->projectOrdinal, declaration.qualifiedName, *declaration.externalSymbol};
        auto [it, inserted] = groupByKey.emplace(std::move(key), result.groups.size());
        if (inserted)
        {
            result.groups.push_back({*symbol, {index}});
            result.groups.back().symbol.cppClangUsr = declaration.clangUsr;
            if (result.groups.back().symbol.cppClangUsr && result.groups.back().symbol.cppClangUsr->empty())
                result.groups.back().symbol.cppClangUsr.reset();
            continue;
        }
        auto& group = result.groups[it->second];
        const auto first = group.inventoryIndices.front();
        const auto& original = entries[first];
        if (entry.targetTriple != original.targetTriple ||
            declaration.canonicalFunctionType != original.declaration.canonicalFunctionType ||
            declaration.callingConvention != original.declaration.callingConvention ||
            declaration.languageLinkage != original.declaration.languageLinkage ||
            declaration.dllImport != original.declaration.dllImport ||
            symbol->moduleName != group.symbol.moduleName ||
            symbol->cppCanonicalIdentity != group.symbol.cppCanonicalIdentity)
            return CppSemanticFunctionGroupFailure{first, index, "conflicting declarations for one external function"};
        if (declaration.clangUsr && !declaration.clangUsr->empty())
        {
            if (group.symbol.cppClangUsr && group.symbol.cppClangUsr != declaration.clangUsr)
                return CppSemanticFunctionGroupFailure{first, index, "conflicting Clang USRs for one function"};
            group.symbol.cppClangUsr = declaration.clangUsr;
        }
        group.inventoryIndices.push_back(index);
    }
    return result;
}

CppSemanticFunctionGroupsResult selectCppSemanticFunctions(
    std::span<const CppSemanticInventoryEntry> entries, std::size_t projectOrdinal,
    std::string_view qualifiedName)
{
    std::set<std::string> symbols;
    std::set<std::string> usrs;
    for (const auto& entry : entries)
        if (entry.projectOrdinal == projectOrdinal && entry.declaration.qualifiedName == qualifiedName)
        {
            if (entry.declaration.externalSymbol && !entry.declaration.externalSymbol->empty())
                symbols.insert(*entry.declaration.externalSymbol);
            if (entry.declaration.clangUsr && !entry.declaration.clangUsr->empty())
                usrs.insert(*entry.declaration.clangUsr);
        }

    std::vector<CppSemanticInventoryEntry> selected;
    std::vector<std::size_t> originalIndices;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (entry.projectOrdinal != projectOrdinal) continue;
        if (entry.declaration.qualifiedName != qualifiedName &&
            (!entry.declaration.externalSymbol || !symbols.contains(*entry.declaration.externalSymbol)) &&
            (!entry.declaration.clangUsr || !usrs.contains(*entry.declaration.clangUsr)))
            continue;
        selected.push_back(entry);
        originalIndices.push_back(i);
    }
    auto grouped = groupCppSemanticFunctions(selected);
    if (auto* failure = std::get_if<CppSemanticFunctionGroupFailure>(&grouped))
    {
        failure->firstIndex = originalIndices[failure->firstIndex];
        failure->conflictingIndex = originalIndices[failure->conflictingIndex];
        return grouped;
    }
    auto& result = std::get<CppSemanticFunctionGroups>(grouped);
    std::erase_if(result.groups, [&](const auto& group) {
        return selected[group.inventoryIndices.front()].declaration.qualifiedName != qualifiedName;
    });
    std::erase_if(result.unprojectedIndices, [&](std::size_t index) {
        return selected[index].declaration.qualifiedName != qualifiedName;
    });
    for (auto& group : result.groups)
        for (auto& index : group.inventoryIndices) index = originalIndices[index];
    for (auto& index : result.unprojectedIndices) index = originalIndices[index];
    return grouped;
}

CppSemanticDemandSelection selectCppSemanticDemands(
    std::span<const CppSemanticInventoryEntry> entries,
    std::span<const frontend::CppFunctionDemand> demands, bool deferConflicts)
{
    using Key = std::pair<std::size_t, std::string_view>;
    std::map<Key, std::vector<std::size_t>> byName;
    std::map<Key, std::vector<std::size_t>> bySymbol;
    std::map<Key, std::vector<std::size_t>> byUsr;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        byName[{entry.projectOrdinal, entry.declaration.qualifiedName}].push_back(i);
        if (entry.declaration.externalSymbol && !entry.declaration.externalSymbol->empty())
            bySymbol[{entry.projectOrdinal, *entry.declaration.externalSymbol}].push_back(i);
        if (entry.declaration.clangUsr && !entry.declaration.clangUsr->empty())
            byUsr[{entry.projectOrdinal, *entry.declaration.clangUsr}].push_back(i);
    }
    const auto lookup = [&](std::size_t project, const std::string& qualified)
        -> CppSemanticFunctionGroupsResult {
        const auto named = byName.find({project, qualified});
        if (named == byName.end()) return CppSemanticFunctionGroups{};
        std::vector<std::size_t> indices = named->second;
        for (const auto i : named->second)
        {
            const auto& external = entries[i].declaration.externalSymbol;
            if (external && !external->empty())
            {
                const auto aliases = bySymbol.find({project, *external});
                if (aliases != bySymbol.end())
                    indices.insert(indices.end(), aliases->second.begin(), aliases->second.end());
            }
            const auto& usr = entries[i].declaration.clangUsr;
            if (usr && !usr->empty())
            {
                const auto matching = byUsr.find({project, *usr});
                if (matching != byUsr.end())
                    indices.insert(indices.end(), matching->second.begin(), matching->second.end());
            }
        }
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        std::vector<CppSemanticInventoryEntry> selected;
        selected.reserve(indices.size());
        for (const auto i : indices) selected.push_back(entries[i]);
        auto grouped = groupCppSemanticFunctions(selected);
        if (auto* failure = std::get_if<CppSemanticFunctionGroupFailure>(&grouped))
        {
            failure->firstIndex = indices[failure->firstIndex];
            failure->conflictingIndex = indices[failure->conflictingIndex];
            return grouped;
        }
        auto& groups = std::get<CppSemanticFunctionGroups>(grouped);
        std::erase_if(groups.groups, [&](const auto& group) {
            return selected[group.inventoryIndices.front()].declaration.qualifiedName != qualified;
        });
        std::erase_if(groups.unprojectedIndices, [&](std::size_t i) {
            return selected[i].declaration.qualifiedName != qualified;
        });
        for (auto& group : groups.groups)
            for (auto& i : group.inventoryIndices) i = indices[i];
        for (auto& i : groups.unprojectedIndices) i = indices[i];
        return grouped;
    };
    std::vector<CppSemanticDemandCandidates> result;
    for (std::size_t index = 0; index < demands.size(); ++index)
    {
        const auto& demand = demands[index];
        if (demand.lookupName.empty()) continue;
        for (const auto project : demand.visibleProjects)
        {
            for (const auto& ns : demand.lookupNamespaces)
            {
                std::string qualified;
                for (const char c : ns) qualified += c == '.' ? "::" : std::string(1, c);
                if (!qualified.empty()) qualified += "::";
                qualified += demand.lookupName;
                auto selection = lookup(project, qualified);
                if (auto* conflict = std::get_if<CppSemanticFunctionGroupFailure>(&selection))
                {
                    if (!deferConflicts) return CppSemanticDemandFailure{index, std::move(*conflict)};
                    result.push_back({index, project, ns, {}, std::move(*conflict)});
                    continue;
                }
                auto& candidates = std::get<CppSemanticFunctionGroups>(selection);
                if (!candidates.groups.empty() || !candidates.unprojectedIndices.empty())
                    result.push_back({index, project, ns, std::move(candidates), std::nullopt});
            }
        }
    }
    return result;
}

std::vector<CppPublishedTemplateFunctionCandidates> selectCppPublishedTemplateFunctions(
    const frontend::ProjectCompilation& compilation, const LibraryMetadata& metadata,
    std::span<const CppSemanticInventoryEntry> inventory)
{
    std::set<std::string> published;
    for (const auto& item : metadata.genericTemplates) published.insert(item.canonicalKey);
    std::vector<frontend::CppFunctionDemand> demands;
    std::vector<std::size_t> originalIndices;
    const auto input = compilation.cppTemplateDemands();
    for (std::size_t i = 0; i < input.size(); ++i)
        if (published.contains(input[i].templateKey))
        {
            demands.push_back(input[i].demand);
            originalIndices.push_back(i);
        }
    auto selected = selectCppSemanticDemands(inventory, demands, true);
    auto& matches = std::get<std::vector<CppSemanticDemandCandidates>>(selected);
    std::vector<CppPublishedTemplateFunctionCandidates> result;
    result.reserve(matches.size());
    for (auto& match : matches)
    {
        const auto original = originalIndices[match.demandIndex];
        match.demandIndex = original;
        result.push_back({input[original].templateKey, original, std::move(match)});
    }
    return result;
}

bool captureCppTemplateFunctionCandidates(const compilation::ProjectGraph& graph,
    std::span<const CppPublishedTemplateFunctionCandidates> candidates,
    std::span<const CppSemanticInventoryEntry> inventory,
    std::vector<metadata_detail::CDependencyFunctionSnapshot>& output, std::string& error)
{
    std::vector<metadata_detail::CDependencyFunctionSnapshot> result;
    std::set<std::tuple<std::size_t, std::size_t, std::string>> seen;
    for (const auto& candidate : candidates)
        for (const auto& group : candidate.selection.candidates.groups)
            for (const auto index : group.inventoryIndices)
            {
                if (index >= inventory.size()) { error = "C function candidate index is invalid"; return false; }
                const auto& entry = inventory[index];
                if (entry.projectOrdinal >= graph.projects().size())
                { error = "C function candidate has no project owner"; return false; }
                const auto& project = graph.projects()[entry.projectOrdinal];
                if (entry.referenceIndex >= project.cppLibraryReferences().size())
                { error = "C function candidate has no source reference"; return false; }
                if (project.cppLibraryReferences()[entry.referenceIndex].language != CppLanguageMode::C ||
                    (!entry.declaration.externalLinkage && !entry.declaration.isCStaticInlineDefinition)) continue;
                const auto key = std::make_tuple(entry.projectOrdinal, entry.referenceIndex, group.symbol.mangledName);
                if (!seen.insert(key).second) continue;
                metadata_detail::CDependencyFunctionSnapshot snapshot;
                auto reference = "cpp-reference:" + std::to_string(entry.referenceIndex);
                const auto &runtime = project.cppLibraryReferences()[entry.referenceIndex].runtimePath;
                if (!runtime.empty())
                {
                    basic::Sha256Digest digest{};
                    std::uintmax_t size = 0;
                    if (!basic::sha256File(runtime, digest, size, error)) return false;
                    const auto name = runtime.filename().generic_u8string();
                    reference += "#dll:" + std::string(reinterpret_cast<const char*>(name.data()), name.size()) +
                        ":" + basic::sha256Hex(digest);
                }
                if (!metadata_detail::captureCDependencyFunction(inventory, group.symbol, entry.referenceIndex,
                                                                   reference, snapshot, error)) return false;
                result.push_back(std::move(snapshot));
            }
    output = std::move(result);
    error.clear();
    return true;
}

CppDeferredFunctionResult materializeCppDeferredFunctions(
    std::span<const CppSemanticInventoryEntry> entries,
    std::span<const frontend::CppFunctionDemand> demands,
    std::span<const CppSemanticDemandCandidates> selections)
{
    std::vector<CppDeferredFunctionCandidate> result;
    for (std::size_t i = 0; i < selections.size(); ++i)
    {
        const auto& selection = selections[i];
        if (selection.demandIndex >= demands.size())
            return CppDeferredFunctionFailure{i, "demand index is out of range"};
        const auto& demand = demands[selection.demandIndex];
        if (!demand.syntax || demand.lookupName.empty() ||
            std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), selection.projectOrdinal) ==
                demand.visibleProjects.end() ||
            std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), selection.namespaceName) ==
                demand.lookupNamespaces.end())
            return CppDeferredFunctionFailure{i, "selection does not belong to the demand"};
        std::string qualified;
        for (const char c : selection.namespaceName) qualified += c == '.' ? "::" : std::string(1, c);
        if (!qualified.empty()) qualified += "::";
        qualified += demand.lookupName;
        if (selection.deferredConflict)
        {
            const auto& conflict = *selection.deferredConflict;
            if (conflict.firstIndex >= entries.size() || conflict.conflictingIndex >= entries.size() ||
                conflict.message.empty() || entries[conflict.firstIndex].projectOrdinal != selection.projectOrdinal ||
                entries[conflict.conflictingIndex].projectOrdinal != selection.projectOrdinal ||
                !selection.candidates.groups.empty() || !selection.candidates.unprojectedIndices.empty())
                return CppDeferredFunctionFailure{i, "invalid deferred C/C++ conflict"};
            const auto anchor = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.projectOrdinal == selection.projectOrdinal && entry.declaration.qualifiedName == qualified;
            });
            if (anchor == entries.end())
                return CppDeferredFunctionFailure{i, "conflict does not belong to the demand"};
            const auto relevant = [&](const CppSemanticInventoryEntry& source) {
                if (source.declaration.qualifiedName == qualified) return true;
                return std::any_of(entries.begin(), entries.end(), [&](const auto& named) {
                    return named.projectOrdinal == selection.projectOrdinal &&
                        named.declaration.qualifiedName == qualified &&
                        ((source.declaration.externalSymbol && !source.declaration.externalSymbol->empty() &&
                          named.declaration.externalSymbol == source.declaration.externalSymbol) ||
                         (source.declaration.clangUsr && !source.declaration.clangUsr->empty() &&
                          named.declaration.clangUsr == source.declaration.clangUsr));
                });
            };
            if (!relevant(entries[conflict.firstIndex]) || !relevant(entries[conflict.conflictingIndex]))
                return CppDeferredFunctionFailure{i, "conflict declarations do not belong to the demand"};
            std::vector<CppConflictingFunctionOrigin> origins;
            for (const auto sourceIndex : {conflict.firstIndex, conflict.conflictingIndex})
            {
                if (!origins.empty() && sourceIndex == conflict.firstIndex) continue;
                const auto& source = entries[sourceIndex];
                origins.push_back({source.projectOrdinal, source.referenceIndex, source.projectFile, source.declaration});
            }
            result.push_back({demand.syntax, demand.directCall, demand.projectOrdinal, anchor->projectOrdinal,
                anchor->referenceIndex, anchor->projectFile, anchor->declaration,
                "conflicting C/C++ declarations: " + conflict.message, std::move(origins)});
            continue;
        }
        for (const auto entryIndex : selection.candidates.unprojectedIndices)
        {
            if (entryIndex >= entries.size())
                return CppDeferredFunctionFailure{i, "inventory index is out of range"};
            const auto& entry = entries[entryIndex];
            const auto* failure = std::get_if<CppSemanticFunctionFailure>(&entry.projection);
            if (!failure || failure->message.empty() || entry.projectOrdinal != selection.projectOrdinal ||
                entry.declaration.qualifiedName != qualified)
                return CppDeferredFunctionFailure{i, "inventory entry does not match the unprojected candidate"};
            result.push_back({demand.syntax, demand.directCall, demand.projectOrdinal, entry.projectOrdinal,
                              entry.referenceIndex, entry.projectFile, entry.declaration, failure->message, {}});
        }
    }
    return result;
}

static bool provideCppSemanticFunctionsImpl(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup* lookup,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error)
{
    auto inventory = collectCppSemanticFunctions(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
    {
        error = "C/C++ function collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    auto& entries = std::get<std::vector<CppSemanticInventoryEntry>>(inventory);
    if (lookup)
    {
        if (!lookup->isBuilt()) { error = "C/C++ record function projection requires completed name lookup"; return false; }
        auto records = collectCppSemanticRecords(graph, projects);
        if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&records))
        {
            error = "C/C++ record collection failed for project '" + failure->projectFile.generic_string() +
                "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
            return false;
        }
        const auto& recordEntries = std::get<std::vector<CppSemanticRecordEntry>>(records);
        auto groups = groupCppSemanticRecords(recordEntries);
        auto refined = refineCppRecordGroupsForPointerTransport(recordEntries, groups, projects);
        if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
        { error = "C/C++ record comparison failed: " + failure->message; return false; }
        groups = std::move(std::get<CppRecordGroups>(refined));
        std::erase_if(groups.groups, [&](const auto& group) {
            const auto ordinal = lookup->typeOrdinalForIdentity(group.identity.key());
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            const bool reconciled = lookup->isReconciledCppTypeIdentity(group.identity.key());
            return !imported || (!reconciled && (imported->projectOrdinal != group.projectOrdinal ||
                imported->identity.key() != group.identity.key()));
        });
        for (auto& entry : entries)
            if (std::holds_alternative<CppSemanticFunctionFailure>(entry.projection) &&
                entry.projectOrdinal < graph.projects().size())
                entry.projection = projectCppSemanticFunctionWithRecords(
                    entry.declaration, entry.projectOrdinal, graph.projects()[entry.projectOrdinal].moduleName(), groups, lookup);
    }
    auto selection = selectCppSemanticDemands(entries, demands, true);
    if (const auto* failure = std::get_if<CppSemanticDemandFailure>(&selection))
    {
        error = "C/C++ function conflict for demand " + std::to_string(failure->demandIndex) +
            ": " + failure->conflict.message;
        return false;
    }
    auto& matches = std::get<std::vector<CppSemanticDemandCandidates>>(selection);
    auto materialized = materializeCppDeferredFunctions(entries, demands, matches);
    if (const auto* failure = std::get_if<CppDeferredFunctionFailure>(&materialized))
    {
        error = "C/C++ deferred function selection " + std::to_string(failure->selectionIndex) +
            ": " + failure->message;
        return false;
    }
    std::vector<frontend::NamespaceFunctionSymbol> result;
    std::set<std::tuple<std::size_t, std::string, std::string>> seen;
    for (auto& match : matches)
        for (auto& group : match.candidates.groups)
        {
            auto& symbol = group.symbol;
            if (seen.emplace(symbol.projectOrdinal, symbol.namespaceName, symbol.mangledName).second)
                result.push_back(std::move(symbol));
        }
    projected = std::move(result);
    deferred = std::move(std::get<std::vector<CppDeferredFunctionCandidate>>(materialized));
    error.clear();
    return true;
}

bool provideCppSemanticFunctions(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error)
{
    return provideCppSemanticFunctionsImpl(graph, projects, nullptr, demands, projected, deferred, error);
}

bool provideExperimentalCppFunctionsWithRecords(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error)
{
    return provideCppSemanticFunctionsImpl(graph, projects, &lookup, demands, projected, deferred, error);
}

bool provideCppFunctionsWithPrecompiledCatalogs(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceFunctionSymbol>& projected,
    std::vector<CppDeferredFunctionCandidate>& deferred, std::string& error)
{
    std::vector<frontend::NamespaceFunctionSymbol> result;
    std::vector<CppDeferredFunctionCandidate> unresolved;
    if (!provideExperimentalCppFunctionsWithRecords(graph, projects, lookup, demands, result, unresolved, error))
        return false;
    std::map<std::string, std::size_t> owners;
    std::set<std::string> ambiguous;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
        if (!owners.emplace(graph.projects()[i].moduleName(), i).second)
            ambiguous.insert(graph.projects()[i].moduleName());
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return false;
        for (const auto& function : dependencies.functions)
        {
            const auto owner = owners.find(function.moduleName);
            if (owner == owners.end() || ambiguous.contains(function.moduleName))
            { error = "precompiled C function owner is missing or ambiguous: " + function.moduleName; return false; }
            const bool demanded = std::any_of(demands.begin(), demands.end(), [&](const auto& demand) {
                return demand.lookupName == function.declaration.name &&
                    std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), owner->second) != demand.visibleProjects.end() &&
                    std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), "") != demand.lookupNamespaces.end();
            });
            if (!demanded) continue;
            auto restored = projectCppPrecompiledCFunction(function, records, owner->second,
                function.moduleName, "x86_64-pc-windows-msvc", lookup);
            const auto* failure = std::get_if<CppSemanticFunctionFailure>(&restored);
            if (failure) { error = "precompiled C function cannot be projected: " + failure->message; return false; }
            auto value = std::move(std::get<frontend::NamespaceFunctionSymbol>(restored));
            const auto dllMarker = function.referenceIdentity.rfind("#dll:");
            if (dllMarker != std::string::npos)
                value.cppDllRuntimeIdentity = function.referenceIdentity.substr(dllMarker + 5);
            if (function.declaration.externalLinkage && function.declaration.languageLinkage == CppLanguageLinkage::C &&
                value.cppClangUsr && !value.cppClangUsr->empty())
            {
                const auto conflict = std::find_if(result.begin(), result.end(), [&](const auto& other) {
                    return other.mangledName == value.mangledName && other.cppClangUsr == value.cppClangUsr &&
                        other.cppCanonicalIdentity != value.cppCanonicalIdentity;
                });
                if (conflict != result.end())
                { error = "conflicting C function declarations across precompiled library and header: " + value.name; return false; }
            }
            const auto existing = std::find_if(result.begin(), result.end(), [&](const auto& other) {
                return other.projectOrdinal == value.projectOrdinal && other.mangledName == value.mangledName;
            });
            if (existing != result.end())
            {
                if (existing->cppCanonicalIdentity != value.cppCanonicalIdentity ||
                    existing->cppClangUsr != value.cppClangUsr)
                { error = "conflicting precompiled C function: " + value.name; return false; }
                continue;
            }
            result.push_back(std::move(value));
        }
    }
    projected = std::move(result);
    deferred = std::move(unresolved);
    error.clear();
    return true;
}
}
