#include "nativesharp/Language/AbiSemantics.h"

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

bool simdScalarFallbackPreservesSemantics() noexcept { return true; }
}
