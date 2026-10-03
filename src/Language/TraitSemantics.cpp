#include "nativesharp/Language/TraitSemantics.h"

#include <unordered_map>
#include <unordered_set>

namespace nativesharp::language
{
bool validateTrait(const TraitContract& trait, std::string& error)
{
    error.clear();
    if (trait.qualifiedName.empty()) { error = "trait requires a qualified identity"; return false; }
    if (trait.declaresInstanceStorage) { error = "traits cannot declare instance storage"; return false; }
    std::unordered_set<std::string> signatures;
    for (const TraitMethod& method : trait.methods)
        if (method.signature.empty() || !signatures.insert(method.signature).second)
        { error = "trait method signatures must be non-empty and unique"; return false; }
    return true;
}

bool validateImplCoherence(std::span<const ImplCandidate> implementations, std::string& error)
{
    error.clear();
    std::unordered_set<std::string> identities;
    for (const ImplCandidate& item : implementations)
    {
        if (!item.traitOwnedByModule && !item.typeOwnedByModule)
        { error = "orphan impl: the module owns neither trait nor type"; return false; }
        const std::string key = item.traitIdentity + " for " + item.typeIdentity;
        if (!identities.insert(key).second)
        { error = "duplicate coherent impl for " + key; return false; }
    }
    return true;
}

bool normalizeAssociatedType(
    std::string input,
    std::span<const AssociatedProjection> projections,
    std::string& result,
    std::string& error)
{
    error.clear();
    std::unordered_map<std::string, std::string> table;
    for (const auto& item : projections)
        if (!table.emplace(item.projection, item.replacement).second)
        { error = "ambiguous associated type projection"; return false; }
    std::unordered_set<std::string> seen;
    while (table.contains(input))
    {
        if (!seen.insert(input).second) { error = "cyclic associated type projection"; return false; }
        input = table.at(input);
    }
    result = std::move(input);
    return true;
}

bool validateOpaqueResultTypes(std::span<const std::string> concreteReturns, std::string& error)
{
    error.clear();
    if (concreteReturns.empty()) { error = "opaque result declaration has no return type"; return false; }
    for (const std::string& type : concreteReturns)
        if (type != concreteReturns.front())
        { error = "opaque result paths resolve to different concrete types"; return false; }
    return true;
}

bool isObjectSafe(std::span<const TraitMethod> methods, std::string& error)
{
    error.clear();
    for (const TraitMethod& method : methods)
        if (method.isGeneric || method.returnsSelfByValue)
        { error = "dyn trait methods cannot be generic or return Self by value"; return false; }
    return true;
}

TraitObjectLayout windowsX64TraitObjectLayout() noexcept
{
    // The reference points at the concrete object. Its first word points at the class descriptor,
    // whose interface map selects the stable trait method table. Conversion is therefore a
    // pointer-preserving view operation: no box, no retain cell, and no heap allocation.
    return {8, 8, true, true, false};
}

std::string dispatchMetadata(DispatchKind kind, std::string_view traitIdentity)
{
    return std::string(kind == DispatchKind::Static ? "static:" : "dyn:") + std::string(traitIdentity);
}

ExtensionResolution resolveExtension(bool hasInstanceMember, std::size_t visibleExtensions) noexcept
{
    if (hasInstanceMember) return ExtensionResolution::InstanceMember;
    if (visibleExtensions == 0) return ExtensionResolution::Missing;
    if (visibleExtensions == 1) return ExtensionResolution::UniqueExtension;
    return ExtensionResolution::Ambiguous;
}

std::string_view operatorTrait(std::string_view spelling) noexcept
{
    if (spelling == "+") return "lang:Add";
    if (spelling == "-") return "lang:Subtract";
    if (spelling == "*") return "lang:Multiply";
    if (spelling == "/") return "lang:Divide";
    if (spelling == "==") return "lang:Equality";
    return {};
}

bool operatorCanBeOverloaded(std::string_view spelling) noexcept
{
    return spelling != "&&" && spelling != "||" && spelling != "??";
}
}
