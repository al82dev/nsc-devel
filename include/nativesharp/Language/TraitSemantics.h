#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/AssociatedProjection.h"
#include "nativesharp/Language/DispatchKind.h"
#include "nativesharp/Language/ExtensionResolution.h"
#include "nativesharp/Language/ImplCandidate.h"
#include "nativesharp/Language/OperatorTraitContract.h"
#include "nativesharp/Language/TraitContract.h"
#include "nativesharp/Language/TraitMethod.h"
#include "nativesharp/Language/TraitObjectLayout.h"

namespace nativesharp::language
{
bool validateTrait(const TraitContract& trait, std::string& error);

bool validateImplCoherence(std::span<const ImplCandidate> implementations, std::string& error);

bool normalizeAssociatedType(
    std::string input,
    std::span<const AssociatedProjection> projections,
    std::string& result,
    std::string& error);

bool validateOpaqueResultTypes(std::span<const std::string> concreteReturns, std::string& error);
bool isObjectSafe(std::span<const TraitMethod> methods, std::string& error);

TraitObjectLayout windowsX64TraitObjectLayout() noexcept;
std::string dispatchMetadata(DispatchKind kind, std::string_view traitIdentity);

ExtensionResolution resolveExtension(bool hasInstanceMember, std::size_t visibleExtensions) noexcept;

std::span<const OperatorTraitContract> operatorTraitContracts() noexcept;
const OperatorTraitContract* operatorTraitContract(
    std::string_view spelling,
    std::size_t arity) noexcept;
const OperatorTraitContract* operatorTraitContractForMethod(
    std::string_view methodName,
    std::size_t arity) noexcept;
std::string_view operatorTrait(std::string_view spelling, std::size_t arity) noexcept;
std::string_view operatorMethodName(std::string_view spelling, std::size_t arity) noexcept;

// Compatibility projection retained for the original T05 helper. Ambiguous unary/binary
// spellings select their binary contract, matching the historical `+ -> lang:Add` behavior.
std::string_view operatorTrait(std::string_view spelling) noexcept;
bool operatorCanBeOverloaded(std::string_view spelling) noexcept;
}
