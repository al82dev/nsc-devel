#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct ConstraintSet final
{
    std::vector<std::string> atoms;
    std::string canonical;
};

bool normalizeConstraints(
    std::span<const std::string> constraints,
    ConstraintSet& result,
    std::string& error);

enum class ConstGenericKind { Integer, Boolean, Enumeration, Structural };

struct ConstGenericValue final
{
    ConstGenericKind kind = ConstGenericKind::Integer;
    std::string typeKey;
    std::string normalizedValue;
};

bool validateConstGeneric(const ConstGenericValue& value, std::string& error);
std::string genericInstantiationIdentity(
    std::string_view definition,
    std::span<const std::string> typeArguments,
    std::span<const ConstGenericValue> values);

bool validatePackExpansion(
    std::span<const std::size_t> packLengths,
    std::size_t expansionBudget,
    std::string& error) noexcept;

bool bidirectionalInferenceAccepts(
    std::string_view producedType,
    std::string_view expectedType,
    bool requiresArbitraryConversion) noexcept;

enum class AliasKind { Transparent, Opaque };
std::string aliasIdentity(
    AliasKind kind,
    std::string_view module,
    std::string_view name,
    std::string_view representation);

bool specializationStrictlyDominates(
    std::span<const std::string> candidateConstraints,
    std::span<const std::string> fallbackConstraints);

class InstantiationBudget final
{
public:
    InstantiationBudget(std::size_t maximumDepth, std::size_t maximumCount) noexcept;
    bool enter(std::string identity, std::string& error);
    void leave() noexcept;
    std::size_t count() const noexcept { return count_; }
private:
    std::size_t maximumDepth_;
    std::size_t maximumCount_;
    std::size_t count_ = 0;
    std::vector<std::string> stack_;
};
}
