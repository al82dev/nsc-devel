#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct VariantCase final
{
    std::string name;
    std::size_t payloadSize = 0;
    std::size_t payloadAlignment = 1;
};

struct VariantLayout final
{
    std::size_t tagSize = 0;
    std::size_t payloadOffset = 0;
    std::size_t size = 0;
    std::size_t alignment = 1;
};

std::optional<VariantLayout> computeWindowsX64VariantLayout(
    std::span<const VariantCase> cases) noexcept;

enum class PatternKind
{
    Wildcard,
    Literal,
    Binding,
    Tuple,
    Struct,
    Variant,
    Range,
    Or
};

enum class PatternBindingMode
{
    Value,
    Reference,
    Move,
    Copy,
    ReferenceMutable
};

struct PatternBinding final
{
    std::string name;
    std::string typeKey;
    PatternBindingMode mode = PatternBindingMode::Value;
};

bool orPatternBindingsCompatible(
    std::span<const PatternBinding> left,
    std::span<const PatternBinding> right,
    std::string& error);

struct MatchArmCoverage final
{
    struct ListLengthDomain final
    {
        std::size_t length = 0;
        bool hasRest = false;

        bool operator==(const ListLengthDomain&) const = default;
    };

    std::vector<std::string> cases;
    bool wildcard = false;
    bool hasGuard = false;
    // One source arm may join several list alternatives with OR. Each exact domain covers one
    // length; a rest domain covers every length greater than or equal to its fixed prefix.
    std::vector<ListLengthDomain> listLengths;
};

struct ExhaustivenessResult final
{
    bool exhaustive = false;
    std::vector<std::string> missingCases;
    std::vector<std::size_t> unreachableArms;
};

ExhaustivenessResult analyzeClosedMatch(
    std::span<const std::string> domain,
    std::span<const MatchArmCoverage> arms);

// NS304-T03-F05. Exhaustiveness por longitud para match sobre listas/arrays. Exhaustivo solo si
// un brazo sin guard cubre toda longitud >= restMin (rest) y TODAS las longitudes < restMin estan
// cubiertas por brazos de longitud exacta (o un wildcard sin guard cubre todo).
ExhaustivenessResult analyzeListMatch(std::span<const MatchArmCoverage> arms);

bool letElseGuaranteesInitialization(bool elseDiverges) noexcept;
bool canReadCUnionMember(bool activeMemberProven, bool inUnsafeContext) noexcept;
std::string variantMetadataIdentity(std::string_view qualifiedName, std::span<const VariantCase> cases);
}
