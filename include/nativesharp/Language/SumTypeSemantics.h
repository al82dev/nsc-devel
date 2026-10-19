#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/ExhaustivenessResult.h"
#include "nativesharp/Language/MatchArmCoverage.h"
#include "nativesharp/Language/PatternBinding.h"
#include "nativesharp/Language/PatternBindingMode.h"
#include "nativesharp/Language/PatternKind.h"
#include "nativesharp/Language/VariantCase.h"
#include "nativesharp/Language/VariantLayout.h"

namespace nativesharp::language
{
std::optional<VariantLayout> computeWindowsX64VariantLayout(
    std::span<const VariantCase> cases) noexcept;

bool orPatternBindingsCompatible(
    std::span<const PatternBinding> left,
    std::span<const PatternBinding> right,
    std::string& error);

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
