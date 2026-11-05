#include "nativesharp/Interop/CppNamespaceInventory.h"

#include <optional>
#include <set>
#include <system_error>
#include <utility>

namespace nativesharp::interop
{
CppNamespaceInventoryResult collectCppNamespaces(
    const compilation::ProjectGraph& finalGraph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppNamespaceInventoryEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t index = 0; index < finalGraph.projects().size(); ++index)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(
                group.projectFile, finalGraph.projects()[index].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppNamespaceInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = index;
        }
        if (!owner) return CppNamespaceInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppNamespaceInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->namespacesSnapshot();
            if (!declarations)
                return CppNamespaceInventoryFailure{group.projectFile, index, "C/C++ session is not available"};
            for (auto& declaration : *declarations)
                result.push_back({group.projectFile, index, *owner,
                    finalGraph.projects()[*owner].moduleName(), std::move(declaration)});
        }
    }
    return result;
}

CppVisibleNamespacesResult visibleCppNamespaces(std::span<const CppNamespaceInventoryEntry> entries)
{
    std::set<std::pair<std::size_t, std::string>> unique;
    for (const auto& entry : entries)
    {
        const auto& declaration = entry.declaration;
        if (declaration.isAnonymous || declaration.insideAnonymous) continue;
        std::string converted;
        const std::string& input = declaration.qualifiedName;
        if (input.empty())
            return CppNamespaceInventoryFailure{entry.projectFile, entry.referenceIndex, "empty namespace name"};
        for (std::size_t index = 0; index < input.size(); ++index)
        {
            if (input[index] == ':')
            {
                if (index + 1 >= input.size() || input[index + 1] != ':' || converted.empty() ||
                    converted.back() == '.' || index + 2 == input.size())
                    return CppNamespaceInventoryFailure{
                        entry.projectFile, entry.referenceIndex, "invalid qualified namespace name"};
                converted.push_back('.');
                ++index;
            }
            else if (input[index] == '.')
                return CppNamespaceInventoryFailure{
                    entry.projectFile, entry.referenceIndex, "invalid qualified namespace name"};
            else converted.push_back(input[index]);
        }
        unique.emplace(entry.projectOrdinal, std::move(converted));
    }
    return std::vector<std::pair<std::size_t, std::string>>(unique.begin(), unique.end());
}
}
