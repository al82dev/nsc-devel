#include "nativesharp/Language/SumTypeSemantics.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::language
{
namespace
{
std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1U) / alignment * alignment;
}

bool validAlignment(std::size_t value) noexcept
{
    return value != 0 && value <= 16 && (value & (value - 1U)) == 0;
}
}

std::optional<VariantLayout> computeWindowsX64VariantLayout(
    std::span<const VariantCase> cases) noexcept
{
    if (cases.empty()) return std::nullopt;
    VariantLayout result;
    result.tagSize = cases.size() <= 0x100U ? 1U : cases.size() <= 0x10000U ? 2U : 4U;
    std::size_t payloadSize = 0;
    std::size_t payloadAlignment = 1;
    for (const VariantCase& item : cases)
    {
        if (item.name.empty() || !validAlignment(item.payloadAlignment)) return std::nullopt;
        payloadSize = std::max(payloadSize, item.payloadSize);
        payloadAlignment = std::max(payloadAlignment, item.payloadAlignment);
    }
    result.alignment = std::max(result.tagSize, payloadAlignment);
    result.payloadOffset = alignUp(result.tagSize, payloadAlignment);
    if (payloadSize > std::numeric_limits<std::size_t>::max() - result.payloadOffset)
        return std::nullopt;
    result.size = alignUp(result.payloadOffset + payloadSize, result.alignment);
    return result;
}

bool orPatternBindingsCompatible(
    std::span<const PatternBinding> left,
    std::span<const PatternBinding> right,
    std::string& error)
{
    error.clear();
    if (left.size() != right.size())
    {
        error = "OR-pattern alternatives bind a different number of names";
        return false;
    }
    std::unordered_map<std::string, const PatternBinding*> expected;
    for (const PatternBinding& binding : left) expected.emplace(binding.name, &binding);
    for (const PatternBinding& binding : right)
    {
        const auto found = expected.find(binding.name);
        if (found == expected.end() || found->second->typeKey != binding.typeKey ||
            found->second->mode != binding.mode)
        {
            error = "OR-pattern alternatives must bind identical names, types, and modes";
            return false;
        }
    }
    return true;
}

ExhaustivenessResult analyzeClosedMatch(
    std::span<const std::string> domain,
    std::span<const MatchArmCoverage> arms)
{
    ExhaustivenessResult result;
    std::unordered_set<std::string> covered;
    bool unguardedWildcard = false;
    for (std::size_t index = 0; index < arms.size(); ++index)
    {
        const MatchArmCoverage& arm = arms[index];
        const bool useful = !unguardedWildcard && (arm.wildcard || std::ranges::any_of(
            arm.cases, [&](const std::string& value) { return !covered.contains(value); }));
        if (!useful) result.unreachableArms.push_back(index);
        if (arm.hasGuard) continue;
        if (arm.wildcard) unguardedWildcard = true;
        covered.insert(arm.cases.begin(), arm.cases.end());
    }
    if (!unguardedWildcard)
    {
        for (const std::string& value : domain)
            if (!covered.contains(value)) result.missingCases.push_back(value);
    }
    result.exhaustive = unguardedWildcard || result.missingCases.empty();
    return result;
}

bool letElseGuaranteesInitialization(bool elseDiverges) noexcept { return elseDiverges; }

bool canReadCUnionMember(bool activeMemberProven, bool inUnsafeContext) noexcept
{
    return activeMemberProven || inUnsafeContext;
}

std::string variantMetadataIdentity(std::string_view qualifiedName, std::span<const VariantCase> cases)
{
    std::string result(qualifiedName);
    result += "|variant|";
    for (const VariantCase& item : cases)
    {
        result += std::to_string(item.name.size());
        result += ':';
        result += item.name;
        result += ';';
    }
    return result;
}
}
