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
std::string constGenericArgumentMarker(
    std::string_view typeKey,
    std::string_view normalizedValue);
bool parseConstGenericArgumentMarker(
    std::string_view marker,
    std::string& typeKey,
    std::string& normalizedValue) noexcept;
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
    // NSC305-G08. `logicalDepth` is the caller-supplied instantiation depth (0 = derive from the
    // internal stack, the recursion-based default). The wave scheduler passes the demand's depth so
    // the depth limit and maximumObservedDepth survive the two-pass materialization.
    bool enter(std::string identity, std::size_t logicalDepth, std::string& error);
    bool enter(std::string identity, std::string& error);
    void leave() noexcept;
    std::size_t count() const noexcept { return count_; }
    std::size_t maximumDepth() const noexcept { return maximumDepth_; }
    std::size_t maximumCount() const noexcept { return maximumCount_; }
    std::size_t maximumObservedDepth() const noexcept { return maximumObservedDepth_; }
    std::span<const std::string> activeStack() const noexcept { return stack_; }
private:
    std::size_t maximumDepth_;
    std::size_t maximumCount_;
    std::size_t count_ = 0;
    std::size_t maximumObservedDepth_ = 0;
    std::vector<std::string> stack_;
};
}
