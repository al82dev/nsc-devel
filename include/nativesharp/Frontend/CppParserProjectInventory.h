#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Parser/LexicalNamespaceScopes.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct CppParserProjectInventory final
{
    std::vector<std::size_t> visibleProjects;
    std::vector<std::string> namespaces;
    std::vector<std::string> cppTypes;
};

// Inputs are captured after the graph has reached its final project ordinals and the source units
// have been lexed. Namespace existence and type spellings are intentionally separate inventories.
inline std::optional<std::vector<CppParserProjectInventory>> cppParserProjectInventories(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const CppImportedTypeDeclaration> types,
    std::span<const std::pair<std::size_t, std::string>> cppNamespaces,
    std::span<const CppImportedTypeAlias> aliases = {})
{
    const auto projects = graph.projects();
    auto pathKey = [](const std::filesystem::path& path) {
        std::error_code error;
        auto normalized = std::filesystem::weakly_canonical(path, error);
        if (error) normalized = std::filesystem::absolute(path).lexically_normal();
        auto key = normalized.generic_string();
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return key;
    };
    std::unordered_map<std::string, std::size_t> projectByPath;
    for (std::size_t i = 0; i < projects.size(); ++i) projectByPath.emplace(pathKey(projects[i].projectFile()), i);
    std::vector<std::vector<std::size_t>> dependencies(projects.size());
    for (std::size_t i = 0; i < projects.size(); ++i)
        for (const auto& reference : projects[i].projectReferences())
            if (const auto found = projectByPath.find(pathKey(reference)); found != projectByPath.end())
                dependencies[i].push_back(found->second);

    std::vector<std::set<std::string>> namespacesByProject(projects.size());
    auto addNamespace = [&](std::size_t owner, const std::string& name) {
        if (owner >= projects.size() || name.empty() || name.front() == '.' || name.back() == '.' ||
            name.find("..") != std::string::npos) return false;
        std::size_t dot = name.find('.');
        while (dot != std::string::npos)
        {
            namespacesByProject[owner].insert(name.substr(0, dot));
            dot = name.find('.', dot + 1);
        }
        namespacesByProject[owner].insert(name);
        return true;
    };
    for (const auto& unit : units)
    {
        if (!unit || unit->projectOrdinal() >= projects.size()) return std::nullopt;
        if (!unit->isLexicallyValid()) continue;
        for (const auto& scope : parser::lexicalNamespaceScopes(unit->tokens()))
        {
            std::string name;
            for (const auto& segment : scope.namespaceSegments)
            {
                if (!name.empty()) name.push_back('.');
                name += segment;
            }
            if (!name.empty() && !addNamespace(unit->projectOrdinal(), name)) return std::nullopt;
        }
    }
    for (const auto& [owner, name] : cppNamespaces)
        if (!addNamespace(owner, name)) return std::nullopt;
    std::vector<std::set<std::string>> typesByProject(projects.size());
    for (const auto& type : types)
    {
        if (type.projectOrdinal >= projects.size() || !type.identity.isValid() ||
            type.identity.moduleName() != projects[type.projectOrdinal].moduleName()) return std::nullopt;
        typesByProject[type.projectOrdinal].insert(type.identity.fullyQualifiedName());
        std::string name;
        for (const auto& segment : type.identity.namespaceSegments())
        {
            if (!name.empty()) name.push_back('.');
            name += segment;
        }
        if (!name.empty() && !addNamespace(type.projectOrdinal, name)) return std::nullopt;
    }
    for (const auto& alias : aliases)
    {
        if (alias.projectOrdinal >= projects.size() || alias.declaration.name.empty()) return std::nullopt;
        std::string name = alias.declaration.qualifiedName;
        if (name.empty() || alias.declaration.insideAnonymousNamespace) continue;
        std::size_t separator = 0;
        while ((separator = name.find("::", separator)) != std::string::npos)
            name.replace(separator, 2, ".");
        typesByProject[alias.projectOrdinal].insert(name);
        const auto dot = name.rfind('.');
        if (dot != std::string::npos && !addNamespace(alias.projectOrdinal, name.substr(0, dot)))
            return std::nullopt;
    }
    std::vector<CppParserProjectInventory> result(projects.size());
    for (std::size_t owner = 0; owner < projects.size(); ++owner)
    {
        std::vector<bool> visible(projects.size());
        std::vector<std::size_t> pending{owner};
        visible[owner] = true;
        while (!pending.empty())
        {
            const auto current = pending.back(); pending.pop_back();
            for (const auto dependency : dependencies[current])
                if (!visible[dependency]) { visible[dependency] = true; pending.push_back(dependency); }
        }
        std::set<std::string> namespaces;
        std::set<std::string> typeNames;
        for (std::size_t i = 0; i < projects.size(); ++i)
            if (visible[i])
            {
                result[owner].visibleProjects.push_back(i);
                namespaces.insert(namespacesByProject[i].begin(), namespacesByProject[i].end());
                typeNames.insert(typesByProject[i].begin(), typesByProject[i].end());
            }
        result[owner].namespaces.assign(namespaces.begin(), namespaces.end());
        result[owner].cppTypes.assign(typeNames.begin(), typeNames.end());
    }
    return result;
}
}
