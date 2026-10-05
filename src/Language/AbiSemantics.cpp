#include "nativesharp/Language/AbiSemantics.h"

#include <charconv>

namespace nativesharp::language
{
namespace
{
bool powerOfTwo(std::size_t value) noexcept { return value != 0 && (value & (value - 1U)) == 0; }
}

bool validateRepresentation(const RepresentationContract& contract, std::string& error)
{
    error.clear();
    if (contract.packing.has_value() && (!powerOfTwo(*contract.packing) || *contract.packing > 16))
    { error = "packing must be a power of two no larger than 16 on Windows x64"; return false; }
    if (contract.alignment.has_value() && (!powerOfTwo(*contract.alignment) || *contract.alignment > 16))
    { error = "alignment must be a power of two no larger than 16 on Windows x64"; return false; }
    if (contract.representation == Representation::Transparent && contract.nonZeroFields != 1)
    { error = "repr(transparent) requires exactly one non-zero-sized field"; return false; }
    if (contract.representation == Representation::Transparent && contract.packing.has_value())
    { error = "repr(transparent) cannot alter packing"; return false; }
    return true;
}

std::optional<std::uint64_t> bitFieldMask(const BitFieldContract& contract) noexcept
{
    if ((contract.storageBits != 8 && contract.storageBits != 16 && contract.storageBits != 32 &&
         contract.storageBits != 64) || contract.width == 0 || contract.width > contract.storageBits ||
        contract.offset > contract.storageBits - contract.width) return std::nullopt;
    const std::uint64_t low = contract.width == 64 ? ~std::uint64_t{0} :
        ((std::uint64_t{1} << contract.width) - 1U);
    return low << contract.offset;
}

bool pointerCastRequiresUnsafe(const PointerContract& source, const PointerContract& target) noexcept
{
    return (source.nullable && !target.nullable) ||
        (!source.mutablePointee && target.mutablePointee) ||
        (!source.hasProvenance && target.hasProvenance) ||
        source.knownAlignment < target.knownAlignment || source.addressSpace != target.addressSpace;
}

bool pointerContractImplicitlyAssignable(
    const PointerContract& source,
    const PointerContract& target) noexcept
{
    return (!source.nullable || target.nullable) &&
        (!target.mutablePointee || source.mutablePointee) &&
        (!target.hasProvenance || source.hasProvenance) &&
        source.knownAlignment >= target.knownAlignment &&
        source.addressSpace == target.addressSpace;
}

bool callingConventionSupportedOnWindowsX64(
    AbiCallingConvention convention,
    bool variadic) noexcept
{
    if (variadic) return convention == AbiCallingConvention::Cdecl || convention == AbiCallingConvention::Win64;
    return true;
}

bool unsafeOperationRequiresContext(UnsafeOperation) noexcept { return true; }

bool unsafeBlockTooBroad(std::size_t statementCount, bool hasReason) noexcept
{
    return statementCount > 8 && !hasReason;
}

bool validateFfiParameter(
    const FfiParameterContract& parameter,
    std::size_t parameterCount,
    std::string& error)
{
    error.clear();
    if (parameter.lengthParameter.has_value() && *parameter.lengthParameter >= parameterCount)
    { error = "FFI buffer length parameter index is out of range"; return false; }
    if (parameter.callbackMayEscape && (!parameter.callback || parameter.ownership == FfiOwnership::Borrowed))
    { error = "escaping callback requires callback shape and transferred/owned lifetime"; return false; }
    return true;
}

bool ffiMayUnwindAcrossBoundary() noexcept { return false; }

bool validateSimd(const SimdContract& contract, std::string& error)
{
    error.clear();
    if (!powerOfTwo(contract.laneBits) || contract.laneBits < 8 || contract.laneBits > 64 ||
        !powerOfTwo(contract.laneCount) || contract.laneCount < 2 ||
        contract.laneBits * contract.laneCount > 512)
    { error = "portable SIMD requires power-of-two lanes with total width <= 512 bits"; return false; }
    return true;
}

std::optional<PortableSimdType> portableSimdType(
    std::string_view laneTypeKey,
    std::size_t laneCount,
    bool mask,
    std::string& error)
{
    PortableSimdType result;
    result.laneTypeKey = std::string(laneTypeKey);
    result.laneCount = laneCount;
    result.mask = mask;
    if (laneTypeKey == "builtin:sbyte") { result.laneBits = 8; result.laneKind = SimdLaneKind::SignedInteger; }
    else if (laneTypeKey == "builtin:byte") { result.laneBits = 8; result.laneKind = SimdLaneKind::UnsignedInteger; }
    else if (laneTypeKey == "builtin:short") { result.laneBits = 16; result.laneKind = SimdLaneKind::SignedInteger; }
    else if (laneTypeKey == "builtin:ushort") { result.laneBits = 16; result.laneKind = SimdLaneKind::UnsignedInteger; }
    else if (laneTypeKey == "builtin:int") { result.laneBits = 32; result.laneKind = SimdLaneKind::SignedInteger; }
    else if (laneTypeKey == "builtin:uint") { result.laneBits = 32; result.laneKind = SimdLaneKind::UnsignedInteger; }
    else if (laneTypeKey == "builtin:long") { result.laneBits = 64; result.laneKind = SimdLaneKind::SignedInteger; }
    else if (laneTypeKey == "builtin:ulong") { result.laneBits = 64; result.laneKind = SimdLaneKind::UnsignedInteger; }
    else if (laneTypeKey == "builtin:float") { result.laneBits = 32; result.laneKind = SimdLaneKind::FloatingPoint; }
    else if (laneTypeKey == "builtin:double") { result.laneBits = 64; result.laneKind = SimdLaneKind::FloatingPoint; }
    else { error = "portable SIMD lane type must be an 8/16/32/64-bit integer, float, or double"; return std::nullopt; }
    if (mask) result.laneKind = SimdLaneKind::Mask;
    if (!validateSimd(SimdContract{result.laneBits, laneCount, mask}, error)) return std::nullopt;
    return result;
}

std::string portableSimdTypeIdentity(const PortableSimdType& type)
{
    return std::string(type.mask ? "mask<" : "simd<") + type.laneTypeKey + "," +
        std::to_string(type.laneCount) + ">";
}

std::optional<PortableSimdType> parsePortableSimdType(std::string_view identity) noexcept
{
    const bool mask = identity.starts_with("mask<");
    if (!mask && !identity.starts_with("simd<")) return std::nullopt;
    if (!identity.ends_with('>')) return std::nullopt;
    const std::size_t open = identity.find('<');
    const std::size_t comma = identity.rfind(',');
    if (comma == std::string_view::npos || comma <= open + 1 || comma + 1 >= identity.size() - 1)
        return std::nullopt;
    std::size_t count = 0;
    const std::string_view countText = identity.substr(comma + 1, identity.size() - comma - 2);
    const auto parsed = std::from_chars(countText.data(), countText.data() + countText.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != countText.data() + countText.size()) return std::nullopt;
    std::string error;
    return portableSimdType(identity.substr(open + 1, comma - open - 1), count, mask, error);
}

bool validateSimdShuffle(
    std::size_t laneCount,
    std::string_view commaSeparatedIndices,
    std::string& error)
{
    std::size_t seen = 0;
    while (!commaSeparatedIndices.empty())
    {
        const std::size_t comma = commaSeparatedIndices.find(',');
        const std::string_view field = commaSeparatedIndices.substr(0, comma);
        std::size_t index = 0;
        const auto parsed = std::from_chars(field.data(), field.data() + field.size(), index);
        if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() || index >= laneCount)
        { error = "SIMD shuffle indices must be compile-time lane indices in range"; return false; }
        ++seen;
        if (comma == std::string_view::npos) break;
        commaSeparatedIndices.remove_prefix(comma + 1);
    }
    if (seen != laneCount) { error = "SIMD shuffle must specify exactly one index per result lane"; return false; }
    error.clear();
    return true;
}

std::uint64_t simdMaskLane(bool selected, std::size_t laneBits) noexcept
{
    if (!selected || laneBits == 0 || laneBits > 64) return 0;
    return laneBits == 64 ? ~std::uint64_t{0} : ((std::uint64_t{1} << laneBits) - 1U);
}

bool simdScalarFallbackPreservesSemantics() noexcept { return true; }
}
