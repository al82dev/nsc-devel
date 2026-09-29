#include "nativesharp/Language/GenericSemantics.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace nativesharp::language
{
bool normalizeConstraints(
    std::span<const std::string> constraints,
    ConstraintSet& result,
    std::string& error)
{
    error.clear();
    result.atoms.assign(constraints.begin(), constraints.end());
    std::ranges::sort(result.atoms);
    result.atoms.erase(std::unique(result.atoms.begin(), result.atoms.end()), result.atoms.end());
    const auto has = [&](std::string_view atom)
    {
        return std::ranges::binary_search(result.atoms, std::string(atom));
    };
    if (has("copy") && has("move-only"))
    {
        error = "a generic parameter cannot require both copy and move-only";
        return false;
    }
    result.canonical.clear();
    for (const std::string& atom : result.atoms)
    {
        if (atom.empty()) { error = "constraint atom cannot be empty"; return false; }
        result.canonical += std::to_string(atom.size()) + ":" + atom + ";";
    }
    return true;
}

bool validateConstGeneric(const ConstGenericValue& value, std::string& error)
{
    error.clear();
    if (value.typeKey.empty() || value.normalizedValue.empty())
    {
        error = "const generic requires a type and deterministic normalized value";
        return false;
    }
    return true;
}

std::string genericInstantiationIdentity(
    std::string_view definition,
    std::span<const std::string> typeArguments,
    std::span<const ConstGenericValue> values)
{
    std::string result(definition);
    result += "<";
    for (const std::string& type : typeArguments) result += "T" + std::to_string(type.size()) + ":" + type + ";";
    for (const ConstGenericValue& value : values)
        result += "C" + std::to_string(static_cast<int>(value.kind)) + ":" + value.typeKey + "=" + value.normalizedValue + ";";
    result += ">";
    return result;
}

bool validatePackExpansion(
    std::span<const std::size_t> packLengths,
    std::size_t expansionBudget,
    std::string& error) noexcept
{
    error.clear();
    if (packLengths.empty()) return true;
    const std::size_t length = packLengths.front();
    if (length > expansionBudget) { error = "generic pack expansion exceeds its resource budget"; return false; }
    if (!std::ranges::all_of(packLengths, [&](std::size_t item) { return item == length; }))
    {
        error = "zipped generic packs have different lengths";
        return false;
    }
    return true;
}

bool bidirectionalInferenceAccepts(
    std::string_view producedType,
    std::string_view expectedType,
    bool requiresArbitraryConversion) noexcept
{
    return !requiresArbitraryConversion && producedType == expectedType;
}

std::string aliasIdentity(
    AliasKind kind,
    std::string_view module,
    std::string_view name,
    std::string_view representation)
{
    if (kind == AliasKind::Transparent) return std::string(representation);
    return "opaque:" + std::string(module) + "::" + std::string(name);
}

bool specializationStrictlyDominates(
    std::span<const std::string> candidateConstraints,
    std::span<const std::string> fallbackConstraints)
{
    std::unordered_set<std::string> candidate(candidateConstraints.begin(), candidateConstraints.end());
    return candidate.size() > fallbackConstraints.size() && std::ranges::all_of(
        fallbackConstraints, [&](const std::string& item) { return candidate.contains(item); });
}

InstantiationBudget::InstantiationBudget(
    std::size_t maximumDepth,
    std::size_t maximumCount) noexcept
    : maximumDepth_(maximumDepth), maximumCount_(maximumCount) {}

bool InstantiationBudget::enter(std::string identity, std::string& error)
{
    error.clear();
    const auto causalChain = [&]()
    {
        std::ostringstream stream;
        constexpr std::size_t kMaximumReportedFrames = 12;
        const std::size_t begin = stack_.size() > kMaximumReportedFrames
            ? stack_.size() - kMaximumReportedFrames : 0;
        if (begin != 0) stream << "... -> ";
        for (std::size_t index = begin; index < stack_.size(); ++index)
        {
            if (index != begin) stream << " -> ";
            stream << stack_[index];
        }
        if (!stack_.empty()) stream << " -> ";
        stream << identity;
        return stream.str();
    };
    if (stack_.size() >= maximumDepth_)
    {
        error = "generic instantiation depth limit (" + std::to_string(maximumDepth_) +
            ") exceeded; causal chain: " + causalChain();
        return false;
    }
    if (count_ >= maximumCount_)
    {
        error = "generic instantiation count limit (" + std::to_string(maximumCount_) +
            ") exceeded; causal chain: " + causalChain();
        return false;
    }
    if (std::ranges::find(stack_, identity) != stack_.end())
    {
        error = "recursive generic instantiation; causal chain: " + causalChain();
        return false;
    }
    stack_.push_back(std::move(identity));
    ++count_;
    maximumObservedDepth_ = std::max(maximumObservedDepth_, stack_.size());
    return true;
}

void InstantiationBudget::leave() noexcept
{
    if (!stack_.empty()) stack_.pop_back();
}
}
