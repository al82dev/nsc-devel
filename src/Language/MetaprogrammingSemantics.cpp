#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include <algorithm>
#include <unordered_set>

namespace nativesharp::language
{
std::vector<IntrospectionMember> visibleIntrospectionMembers(
    std::span<const IntrospectionMember> members,
    bool sameModule)
{
    std::vector<IntrospectionMember> result;
    for (const auto& member : members)
        if (sameModule || member.isPublic) result.push_back(member);
    std::ranges::sort(result, {}, &IntrospectionMember::sourceOrdinal);
    return result;
}

bool introspectionRequiresRuntimeMetadata() noexcept { return false; }

bool validateAttributes(std::span<const AttributeUse> attributes, std::string& error)
{
    error.clear();
    std::unordered_set<std::string> seen;
    for (const AttributeUse& attribute : attributes)
    {
        if (attribute.identity.empty() || attribute.target.empty())
        { error = "attribute requires identity and target"; return false; }
        const std::string key = attribute.identity + "@" + attribute.target;
        if (!attribute.repeatable && !seen.insert(key).second)
        { error = "non-repeatable attribute appears more than once"; return false; }
    }
    return true;
}

std::string deriveDependencyKey(
    std::string_view generatorIdentity,
    std::span<const std::string> fieldIdentities)
{
    std::vector<std::string> sorted(fieldIdentities.begin(), fieldIdentities.end());
    std::ranges::sort(sorted);
    std::string result = "derive:" + std::string(generatorIdentity) + "|";
    for (const std::string& field : sorted) result += std::to_string(field.size()) + ":" + field + ";";
    return result;
}

bool sameBinding(const HygienicName& left, const HygienicName& right) noexcept
{
    return left.spelling == right.spelling && left.scopeId == right.scopeId;
}

bool capabilityAllowedInHermeticProfile(ComptimeCapability capability) noexcept
{
    return capability == ComptimeCapability::ReadDeclaredFile ||
        capability == ComptimeCapability::ReadDeclaredEnvironment;
}

std::string comptimeInputIdentity(std::span<const ComptimeInput> inputs)
{
    std::vector<ComptimeInput> sorted(inputs.begin(), inputs.end());
    std::ranges::sort(sorted, {}, &ComptimeInput::identity);
    std::string result = "comptime-input-v1|";
    for (const auto& item : sorted) result += item.identity + "=" + item.contentDigest + ";";
    return result;
}

bool validPackageDiagnosticCode(std::string_view package, std::string_view code) noexcept
{
    return !package.empty() && !code.empty() && !code.starts_with("NS3") && code.starts_with(package);
}

ComptimeBudget::ComptimeBudget(
    std::size_t steps,
    std::size_t bytes,
    std::size_t depth) noexcept
    : steps_(steps), bytes_(bytes), depth_(depth) {}

bool ComptimeBudget::consume(
    std::size_t steps,
    std::size_t bytes,
    std::size_t depth,
    std::string& error)
{
    error.clear();
    if (steps > steps_ || bytes > bytes_ || depth > depth_)
    { error = "compile-time evaluation exceeded its deterministic resource budget"; return false; }
    steps_ -= steps;
    bytes_ -= bytes;
    return true;
}
}
