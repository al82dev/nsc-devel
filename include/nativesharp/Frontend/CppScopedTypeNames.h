#pragma once

#include "nativesharp/Frontend/LexicalNamespaceEnvironment.h"
#include "nativesharp/Frontend/QualifiedNameLadder.h"
#include "nativesharp/Parser/ScopedKnownTypeNames.h"

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend
{
// Experimental: the caller supplies C++ type spellings and the complete set of namespaces
// visible to the source unit. Some native aliases and other complex using syntax remain unresolved.
inline std::optional<parser::ScopedKnownTypeNames> cppScopedTypeNames(
    std::span<const parser::LexicalNamespaceScope> scopes,
    std::span<const std::string> visibleNamespaces,
    std::span<const std::string> cppTypeNames)
{
    const auto environments = lexicalNamespaceEnvironments(scopes, visibleNamespaces);
    if (!environments) return std::nullopt;
    for (const auto& environment : *environments)
        if (!environment.unresolvedNamespaces.empty() ||
            !environment.unsupportedDirectives.empty()) return std::nullopt;

    const std::unordered_set<std::string> types(cppTypeNames.begin(), cppTypeNames.end());
    std::unordered_set<std::string> spellings;
    for (const auto& type : types)
    {
        if (type.empty() || type.front() == '.' || type.back() == '.' || type.find("..") != std::string::npos)
            return std::nullopt;
        spellings.insert(type);
        for (std::size_t dot = type.find('.'); dot != std::string::npos; dot = type.find('.', dot + 1))
            spellings.insert(type.substr(dot + 1));
    }

    std::vector<parser::ScopedKnownTypeNames::Scope> result;
    result.reserve(scopes.size());
    for (std::size_t i = 0; i < scopes.size(); ++i)
    {
        const auto& scope = scopes[i];
        std::unordered_map<std::string, std::string> namespaceAliases;
        std::unordered_set<std::string> typeAliases;
        const std::unordered_set<std::string> namespaces(visibleNamespaces.begin(), visibleNamespaces.end());
        for (const auto& alias : (*environments)[i].aliases)
        {
            // A builtin or a target with type modifiers is syntactically a type alias. The
            // ordinary parser/name lookup still validates and expands the complete target.
            if (alias.directive.explicitTypeSyntax)
            {
                typeAliases.insert(alias.directive.alias);
                continue;
            }
            std::string target;
            for (const auto& part : alias.directive.target)
            {
                if (!target.empty()) target.push_back('.');
                target += part;
            }
            const auto namespaceTarget = resolveQualifiedNamespaceTarget(alias.declarationNamespace, target,
                [&](const std::string& candidate) { return namespaces.contains(candidate); });
            if (namespaceTarget || namespaceAliases.contains(target))
            {
                namespaceAliases[alias.directive.alias] = target;
                continue;
            }
            bool typeTarget = typeAliases.contains(target);
            QualifiedNameSite declarationSite;
            declarationSite.namespaceSegments = alias.declarationNamespace;
            declarationSite.importedNamespaces = alias.declarationImports;
            declarationSite.aliases = &namespaceAliases;
            for (const auto& rung : qualifiedNameLadder(declarationSite, target))
                for (const auto& candidate : rung.spellings)
                    typeTarget |= types.contains(candidate);
            if (!typeTarget) return std::nullopt;
            typeAliases.insert(alias.directive.alias);
        }
        QualifiedNameSite site;
        site.namespaceSegments = scope.namespaceSegments;
        site.importedNamespaces = (*environments)[i].importedNamespaces;
        site.aliases = &namespaceAliases;
        parser::KnownTypeNameSet names;
        auto candidates = spellings;
        for (const auto& [alias, target] : namespaceAliases)
            for (const auto& spelling : spellings)
                candidates.insert(alias + "." + spelling);
        names.insert(typeAliases.begin(), typeAliases.end());
        for (const auto& spelling : candidates)
            for (const auto& rung : qualifiedNameLadder(site, spelling))
            {
                bool found = false;
                for (const auto& candidate : rung.spellings)
                    found |= types.contains(candidate);
                if (found) { names.insert(spelling); break; }
            }
        result.push_back({scope.file, scope.begin, scope.end, std::move(names)});
    }
    return parser::ScopedKnownTypeNames(std::move(result), true);
}
}
