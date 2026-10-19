#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/Availability.h"
#include "nativesharp/Language/AvailabilityStatus.h"
#include "nativesharp/Language/Import.h"
#include "nativesharp/Language/ImportKind.h"
#include "nativesharp/Language/ModuleDependency.h"
#include "nativesharp/Language/ModuleDependencyKind.h"
#include "nativesharp/Language/PublicApiSymbol.h"
#include "nativesharp/Language/Visibility.h"

namespace nativesharp::language
{
std::string canonicalWindowsModuleIdentity(std::string_view package, std::string_view module);

bool moduleGraphResolvable(std::span<const ModuleDependency> dependencies, std::string& error);

bool normalizeImports(std::span<const Import> imports, std::vector<Import>& result, std::string& error);

bool canAccess(
    Visibility visibility,
    bool sameModule,
    bool samePackage,
    bool explicitFriend,
    bool sameDeclaration = false) noexcept;

std::string reexportedIdentity(std::string_view originalIdentity, std::string_view facadeName);

AvailabilityStatus availabilityStatus(
    const Availability& availability,
    std::string_view architecture,
    bool deprecated) noexcept;

std::string publicApiFingerprintInput(std::span<const PublicApiSymbol> symbols);
}
