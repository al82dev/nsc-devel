#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include <algorithm>
#include <optional>
#include <set>
#include <system_error>
#include <tuple>
#include <utility>

namespace nativesharp::interop
{
namespace
{
std::optional<CppScalarTypeMapping> mapGlobalType(const CppType& type)
{
    if (type.kind != CppTypeKind::Pointer) return mapCppScalarType(type);
    const CppType* current = &type;
    std::size_t depth = 0;
    while (current->kind == CppTypeKind::Pointer)
    {
        if (current->canonicalSpelling.empty() || current->sizeBits != 64 || !current->element)
            return std::nullopt;
        current = current->element.get();
        if (current->isVolatile || (current->isConst &&
            (depth != 0 || current->kind == CppTypeKind::Pointer))) return std::nullopt;
        ++depth;
    }
    const auto element = mapCppScalarType(*current);
    if (!element) return std::nullopt;
    return CppScalarTypeMapping{element->nativeSharpTypeKey + std::string(depth, '*'),
                                type.canonicalSpelling, type.isConst, type.isVolatile};
}
}

CppSemanticGlobalResult projectCppSemanticGlobal(
    const CppGlobalDeclaration& global, std::size_t projectOrdinal, std::string_view moduleName)
{
    auto failure = [](std::string reason) -> CppSemanticGlobalResult {
        return CppSemanticGlobalFailure{std::move(reason)};
    };
    if (moduleName.empty() || global.name.empty() || global.qualifiedName.empty() ||
        !global.externalLinkage || !global.externalSymbol || global.externalSymbol->empty())
        return failure("global requires an external symbol and a project owner");
    if (global.threadLocal) return failure("thread-local C/C++ global is not yet projected");
    const auto type = mapGlobalType(global.type);
    if (!type || global.type.kind == CppTypeKind::Void)
        return failure("C/C++ global type is not yet mapped");
    std::string cppNamespace;
    if (global.qualifiedName != global.name)
    {
        const auto separator = global.qualifiedName.rfind("::");
        if (separator == std::string::npos || global.qualifiedName.substr(separator + 2) != global.name)
            return failure("C/C++ qualified name does not match its global name");
        cppNamespace = global.qualifiedName.substr(0, separator);
    }
    std::string nsNamespace;
    for (std::size_t i = 0; i < cppNamespace.size(); ++i)
        if (cppNamespace[i] == ':' && i + 1 < cppNamespace.size() && cppNamespace[i + 1] == ':')
        { nsNamespace.push_back('.'); ++i; }
        else nsNamespace.push_back(cppNamespace[i]);
    return CppSemanticGlobal{projectOrdinal, std::string(moduleName), std::move(nsNamespace),
                             global.name, type->nativeSharpTypeKey, type->canonicalCppType,
                             *global.externalSymbol, global.type.isVolatile, global.type.isConst,
                             global.type.kind == CppTypeKind::Pointer && global.type.element &&
                                 global.type.element->isConst, global.dllImport};
}

CppSemanticGlobalInventoryResult collectCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticGlobalEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            if (!std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error) || error) continue;
            if (owner) return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner) return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->globalsSnapshot();
            if (!declarations) return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ session is not available"};
            const auto options = session->optionsSnapshot();
            for (auto& declaration : *declarations)
            {
                auto projection = projectCppSemanticGlobal(declaration, *owner, graph.projects()[*owner].moduleName());
                result.push_back({group.projectFile, index, *owner, options.targetTriple,
                                  std::move(declaration), std::move(projection)});
            }
        }
    }
    return result;
}

CppSemanticGlobalSelection selectCppSemanticGlobal(
    std::span<const CppSemanticGlobalEntry> entries, std::size_t projectOrdinal,
    std::string_view qualifiedName)
{
    CppSemanticGlobalSelection result;
    std::vector<std::string_view> symbols;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (entry.projectOrdinal != projectOrdinal || entry.declaration.qualifiedName != qualifiedName) continue;
        if (!entry.declaration.externalLinkage)
        {
            result.internalIndices.push_back(i);
            continue;
        }
        result.declarationIndices.push_back(i);
        if (std::holds_alternative<CppSemanticGlobalFailure>(entry.projection))
            result.unsupportedIndices.push_back(i);
        if (entry.declaration.externalSymbol && !entry.declaration.externalSymbol->empty())
            symbols.push_back(*entry.declaration.externalSymbol);
    }
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (entry.projectOrdinal != projectOrdinal || !entry.declaration.externalLinkage ||
            entry.declaration.qualifiedName == qualifiedName || !entry.declaration.externalSymbol) continue;
        if (std::find(symbols.begin(), symbols.end(), *entry.declaration.externalSymbol) != symbols.end())
            result.aliasIndices.push_back(i);
    }
    if (result.declarationIndices.empty()) return result;
    const auto& first = entries[result.declarationIndices.front()];
    const auto compatible = [&](const CppSemanticGlobalEntry& entry) {
        const auto& a = first.declaration;
        const auto& b = entry.declaration;
        return first.targetTriple == entry.targetTriple && a.externalSymbol == b.externalSymbol &&
            a.threadLocal == b.threadLocal && a.dllImport == b.dllImport && a.type.kind == b.type.kind &&
            a.type.canonicalSpelling == b.type.canonicalSpelling &&
            a.type.unqualifiedCanonicalSpelling == b.type.unqualifiedCanonicalSpelling &&
            a.type.isConst == b.type.isConst && a.type.isVolatile == b.type.isVolatile &&
            a.type.sizeBits == b.type.sizeBits && a.type.alignmentBits == b.type.alignmentBits;
    };
    for (const auto i : result.declarationIndices)
        if (!compatible(entries[i]))
            result.conflict = "incompatible C/C++ global declarations";
    for (const auto i : result.aliasIndices)
        if (!compatible(entries[i]))
            result.conflict = "incompatible C/C++ global symbol alias";
    if (result.conflict.empty() && result.unsupportedIndices.empty())
        if (const auto* candidate = std::get_if<CppSemanticGlobal>(&first.projection))
            result.candidate = *candidate;
    return result;
}

bool provideCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error)
{
    auto inventory = collectCppSemanticGlobals(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
    {
        error = "C/C++ global collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    const auto& entries = std::get<std::vector<CppSemanticGlobalEntry>>(inventory);
    std::set<std::pair<std::size_t, std::string_view>> availableNames;
    for (const auto& entry : entries)
        if (entry.declaration.externalLinkage)
            availableNames.emplace(entry.projectOrdinal, entry.declaration.qualifiedName);
    std::set<std::tuple<std::size_t, std::string, std::string>> seen;
    std::vector<frontend::NamespaceStateSymbol> result;
    for (const auto& demand : demands)
        for (const auto owner : demand.visibleProjects)
            for (const auto& ns : demand.lookupNamespaces)
            {
                if (owner >= graph.projects().size() || demand.lookupName.empty() ||
                    !seen.emplace(owner, ns, demand.lookupName).second) continue;
                std::string qualified;
                for (const char c : ns) qualified += c == '.' ? "::" : std::string(1, c);
                if (!qualified.empty()) qualified += "::";
                qualified += demand.lookupName;
                if (!availableNames.contains({owner, qualified})) continue;
                const auto selected = selectCppSemanticGlobal(entries, owner, qualified);
                if (selected.declarationIndices.empty()) continue;
                frontend::NamespaceStateSymbol state;
                state.projectOrdinal = owner;
                state.moduleName = graph.projects()[owner].moduleName();
                state.namespaceName = ns;
                state.name = demand.lookupName;
                state.isCppExternal = true;
                state.accessibility = frontend::NamespaceFunctionAccessibility::Public;
                if (selected.candidate)
                {
                    state.declaredTypeKey = selected.candidate->typeKey;
                    state.storageName = selected.candidate->externalSymbol;
                    state.isVolatile = selected.candidate->isVolatile;
                    state.isCppReadonly = selected.candidate->isConst;
                    state.cppPointerPointeeReadonly = selected.candidate->pointerPointeeReadonly;
                    state.cppDllImport = selected.candidate->dllImport;
                }
                else
                {
                    state.declaredTypeKey = "builtin:int"; // Never emitted; body analysis reports the reason.
                    if (!selected.conflict.empty()) state.cppImportError = selected.conflict;
                    else if (!selected.unsupportedIndices.empty())
                        state.cppImportError = std::get<CppSemanticGlobalFailure>(
                            entries[selected.unsupportedIndices.front()].projection).message;
                    else state.cppImportError = "C/C++ global could not be projected";
                }
                result.push_back(std::move(state));
            }
    states = std::move(result);
    error.clear();
    return true;
}

CppSemanticEnumInventoryResult collectCppSemanticEnums(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticEnumEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner) return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->enumsSnapshot();
            if (!declarations)
                return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ enum snapshot is not available"};
            const auto target = session->optionsSnapshot().targetTriple;
            for (auto& declaration : *declarations)
                result.push_back({group.projectFile, index, *owner, target, std::move(declaration)});
        }
    }
    return result;
}

CppSemanticTypeAliasInventoryResult collectCppSemanticTypeAliases(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticTypeAliasEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner) return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->typeAliasesSnapshot();
            if (!declarations)
                return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ alias snapshot is not available"};
            const auto target = session->optionsSnapshot().targetTriple;
            for (auto& declaration : *declarations)
                result.push_back({group.projectFile, index, *owner, target, std::move(declaration)});
        }
    }
    return result;
}

CppSemanticInventoryResult collectCppSemanticFunctions(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticInventoryEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error);
            if (error || !same) continue;
            if (owner)
                return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner)
            return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session)
                return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->freeFunctionsSnapshot();
            if (!declarations)
                return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ session is not available"};
            const auto options = session->optionsSnapshot();
            for (auto& declaration : *declarations)
            {
                auto projection = projectCppSemanticFunction(
                    declaration, *owner, graph.projects()[*owner].moduleName());
                result.push_back({group.projectFile, index, *owner, options.targetTriple,
                                  std::move(declaration), std::move(projection)});
            }
        }
    }
    return result;
}
}
