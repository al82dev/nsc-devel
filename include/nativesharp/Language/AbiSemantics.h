#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::language
{
enum class Representation { Native, C, Transparent };
struct RepresentationContract final
{
    Representation representation = Representation::Native;
    std::optional<std::size_t> packing;
    std::optional<std::size_t> alignment;
    std::size_t nonZeroFields = 0;
};
bool validateRepresentation(const RepresentationContract& contract, std::string& error);

struct BitFieldContract final
{
    std::size_t storageBits = 0;
    std::size_t offset = 0;
    std::size_t width = 0;
    bool isSigned = false;
    bool isVolatile = false;
};
std::optional<std::uint64_t> bitFieldMask(const BitFieldContract& contract) noexcept;

struct PointerContract final
{
    bool nullable = true;
    bool mutablePointee = true;
    bool hasProvenance = true;
    std::size_t knownAlignment = 1;
    unsigned addressSpace = 0;
};
bool pointerCastRequiresUnsafe(const PointerContract& source, const PointerContract& target) noexcept;

enum class AbiCallingConvention { Native, Cdecl, StdCall, VectorCall, Win64 };
bool callingConventionSupportedOnWindowsX64(AbiCallingConvention convention, bool variadic) noexcept;

enum class UnsafeOperation { RawDereference, UnionInactiveRead, IntegerPointerCast, InlineAssembly, FfiCall };
bool unsafeOperationRequiresContext(UnsafeOperation operation) noexcept;
bool unsafeBlockTooBroad(std::size_t statementCount, bool hasReason) noexcept;

enum class FfiOwnership { Borrowed, Owned, Transferred };
struct FfiParameterContract final
{
    bool nullable = false;
    FfiOwnership ownership = FfiOwnership::Borrowed;
    std::optional<std::size_t> lengthParameter;
    bool callback = false;
    bool callbackMayEscape = false;
};
bool validateFfiParameter(
    const FfiParameterContract& parameter,
    std::size_t parameterCount,
    std::string& error);
bool ffiMayUnwindAcrossBoundary() noexcept;

struct SimdContract final { std::size_t laneBits; std::size_t laneCount; bool mask; };
bool validateSimd(const SimdContract& contract, std::string& error);
bool simdScalarFallbackPreservesSemantics() noexcept;
}
