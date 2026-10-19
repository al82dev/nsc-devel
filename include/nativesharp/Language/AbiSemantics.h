#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "nativesharp/Language/AbiCallingConvention.h"
#include "nativesharp/Language/BitFieldContract.h"
#include "nativesharp/Language/FfiOwnership.h"
#include "nativesharp/Language/FfiParameterContract.h"
#include "nativesharp/Language/PointerContract.h"
#include "nativesharp/Language/PortableSimdType.h"
#include "nativesharp/Language/Representation.h"
#include "nativesharp/Language/RepresentationContract.h"
#include "nativesharp/Language/SimdContract.h"
#include "nativesharp/Language/SimdLaneKind.h"
#include "nativesharp/Language/UnsafeOperation.h"

namespace nativesharp::language
{
bool validateRepresentation(const RepresentationContract& contract, std::string& error);

std::optional<std::uint64_t> bitFieldMask(const BitFieldContract& contract) noexcept;

bool pointerCastRequiresUnsafe(const PointerContract& source, const PointerContract& target) noexcept;
bool pointerContractImplicitlyAssignable(
    const PointerContract& source,
    const PointerContract& target) noexcept;

bool callingConventionSupportedOnWindowsX64(AbiCallingConvention convention, bool variadic) noexcept;

bool unsafeOperationRequiresContext(UnsafeOperation operation) noexcept;
bool unsafeBlockTooBroad(std::size_t statementCount, bool hasReason) noexcept;

bool validateFfiParameter(
    const FfiParameterContract& parameter,
    std::size_t parameterCount,
    std::string& error);
bool ffiMayUnwindAcrossBoundary() noexcept;

bool validateSimd(const SimdContract& contract, std::string& error);
std::optional<PortableSimdType> portableSimdType(
    std::string_view laneTypeKey,
    std::size_t laneCount,
    bool mask,
    std::string& error);
std::string portableSimdTypeIdentity(const PortableSimdType& type);
std::optional<PortableSimdType> parsePortableSimdType(std::string_view identity) noexcept;
bool validateSimdShuffle(
    std::size_t laneCount,
    std::string_view commaSeparatedIndices,
    std::string& error);
std::uint64_t simdMaskLane(bool selected, std::size_t laneBits) noexcept;
bool simdScalarFallbackPreservesSemantics() noexcept;
}
