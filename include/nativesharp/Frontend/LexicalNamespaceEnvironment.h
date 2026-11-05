#pragma once

#include "nativesharp/Frontend/QualifiedNamespaceTarget.h"
#include "nativesharp/Parser/LexicalNamespaceScopes.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend
{
struct LexicalNamespaceEnvironment final
{
    std::vector<std::string> importedNamespaces;
    struct Alias final
    {
        parser::LexicalUsing directive;
        std::vector<std::string> declarationNamespace;
        std::vector<std::string> declarationImports;
    };
    // These directives need further semantic handling; they are never silently discarded.
    std::vector<Alias> aliases;
    std::vector<parser::LexicalUsing> unresolvedNamespaces;
    std::vector<parser::LexicalUsing> unsupportedDirectives;
};

// The caller supplies exactly the namespaces visible to this unit's project, including native
// and C++ namespaces. The lexical inventory is parent-first and belongs to one source file.
inline std::optional<std::vector<LexicalNamespaceEnvironment>> lexicalNamespaceEnvironments(
    std::span<const parser::LexicalNamespaceScope> scopes,
    std::span<const std::string> visibleNamespaces)
{
    if (scopes.empty()) return std::nullopt;
    const std::unordered_set<std::string> visible(visibleNamespaces.begin(), visibleNamespaces.end());
    std::vector<LexicalNamespaceEnvironment> result;
    result.reserve(scopes.size());
    for (std::size_t i = 0; i < scopes.size(); ++i)
    {
        const auto& scope = scopes[i];
        if (!scope.file.isValid() || scope.file != scopes.front().file || scope.begin >= scope.end ||
            (i == 0 && scope.parent) || (i != 0 && (!scope.parent || *scope.parent >= i)))
            return std::nullopt;
        LexicalNamespaceEnvironment environment;
        if (scope.parent)
        {
            const auto& parent = scopes[*scope.parent];
            if (scope.begin < parent.begin || scope.end > parent.end) return std::nullopt;
            environment.importedNamespaces = result[*scope.parent].importedNamespaces;
            environment.aliases = result[*scope.parent].aliases;
        }
        for (const auto& directive : scope.directives)
        {
            if (!directive.complete)
            {
                environment.unsupportedDirectives.push_back(directive);
                continue;
            }
            if (!directive.alias.empty())
            {
                environment.aliases.push_back({directive, scope.namespaceSegments, environment.importedNamespaces});
                continue;
            }
            std::string target;
            for (const auto& part : directive.target)
            {
                if (!target.empty()) target.push_back('.');
                target += part;
            }
            const auto resolved = resolveQualifiedNamespaceTarget(scope.namespaceSegments, target,
                [&](const std::string& candidate) { return visible.contains(candidate); });
            if (!resolved)
            {
                environment.unresolvedNamespaces.push_back(directive);
                continue;
            }
            if (std::find(environment.importedNamespaces.begin(), environment.importedNamespaces.end(),
                          *resolved) == environment.importedNamespaces.end())
                environment.importedNamespaces.push_back(*resolved);
        }
        result.push_back(std::move(environment));
    }
    return result;
}
}
