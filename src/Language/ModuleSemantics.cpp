#include "nativesharp/Language/ModuleSemantics.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::language
{
std::string canonicalWindowsModuleIdentity(std::string_view package, std::string_view module)
{
    std::string result = std::string(package) + "::" + std::string(module);
    for (char& value : result)
    {
        if (value == '\\') value = '/';
        value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    }
    return result;
}

bool moduleGraphResolvable(std::span<const ModuleDependency> dependencies, std::string& error)
{
    error.clear();
    std::unordered_map<std::string, std::vector<std::string>> layout;
    for (const auto& edge : dependencies)
        if (edge.kind == ModuleDependencyKind::Layout) layout[edge.from].push_back(edge.to);
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    const auto visit = [&](const auto& self, const std::string& node) -> bool
    {
        if (visited.contains(node)) return true;
        if (!visiting.insert(node).second) return false;
        for (const std::string& next : layout[node]) if (!self(self, next)) return false;
        visiting.erase(node);
        visited.insert(node);
        return true;
    };
    for (const auto& [node, ignored] : layout)
    {
        (void)ignored;
        if (!visit(visit, node)) { error = "module layout dependency cycle is not resolvable"; return false; }
    }
    return true;
}

bool normalizeImports(
    std::span<const Import> imports,
    std::vector<Import>& result,
    std::string& error)
{
    error.clear();
    result.clear();
    result.reserve(imports.size());
    for (const Import& item : imports)
    {
        if (item.identity.empty())
        {
            error = "import target cannot be empty";
            return false;
        }
        if ((item.kind == ImportKind::Extensions || item.kind == ImportKind::RestrictedGlob) &&
            !item.alias.empty())
        {
            error = "extension and restricted-glob imports cannot declare an alias";
            return false;
        }
        const auto duplicate = std::ranges::find_if(result, [&](const Import& existing)
        {
            return existing.kind == item.kind && existing.identity == item.identity &&
                existing.alias == item.alias;
        });
        if (duplicate == result.end()) result.push_back(item);
    }
    std::ranges::sort(result, [](const Import& a, const Import& b)
    {
        return std::tie(a.alias, a.identity, a.kind) < std::tie(b.alias, b.identity, b.kind);
    });
    std::unordered_set<std::string> names;
    for (const Import& item : result)
    {
        // Namespace/extension/glob imports contribute a candidate scope rather than a local
        // declaration. Collisions are decided only when a name is requested from those scopes.
        if (item.alias.empty() &&
            (item.kind == ImportKind::Module || item.kind == ImportKind::Extensions ||
             item.kind == ImportKind::RestrictedGlob))
            continue;
        std::string visible = item.alias.empty()
            ? item.identity.substr(item.identity.rfind('.') + 1)
            : item.alias;
        std::ranges::transform(visible, visible.begin(), [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
        if (!names.insert(visible).second)
        {
            error = "import name collision: " + visible;
            return false;
        }
    }
    return true;
}

bool canAccess(
    Visibility visibility,
    bool sameModule,
    bool samePackage,
    bool explicitFriend,
    bool sameDeclaration) noexcept
{
    switch (visibility)
    {
        case Visibility::Private: return sameDeclaration || explicitFriend;
        case Visibility::Module: return sameModule || explicitFriend;
        case Visibility::Package: return samePackage || explicitFriend;
        case Visibility::Public: return true;
    }
    return false;
}

std::string reexportedIdentity(std::string_view originalIdentity, std::string_view)
{
    return std::string(originalIdentity);
}

AvailabilityStatus availabilityStatus(
    const Availability& availability,
    std::string_view architecture,
    bool deprecated) noexcept
{
    if (!availability.architecture.empty() && availability.architecture != architecture)
        return AvailabilityStatus::WrongTarget;
    return deprecated ? AvailabilityStatus::Deprecated : AvailabilityStatus::Available;
}

std::string publicApiFingerprintInput(std::span<const PublicApiSymbol> symbols)
{
    std::vector<PublicApiSymbol> published;
    for (const auto& symbol : symbols) if (symbol.isPublic) published.push_back(symbol);
    std::ranges::sort(published, {}, &PublicApiSymbol::identity);
    std::string result = "ns-api-v1|";
    for (const auto& symbol : published)
        result += symbol.identity + "|" + symbol.signature + "|" + symbol.effects + "|" +
            symbol.layoutContract + ";";
    return result;
}
}
