#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::language
{
struct IntegerType final { unsigned bits = 32; bool isSigned = true; };
bool literalFits(std::uint64_t magnitude, bool negative, IntegerType type) noexcept;
std::optional<std::uint64_t> parseIntegerLiteralMagnitude(std::string_view spelling, std::string& error);

enum class OverflowPolicy { Checked, Wrapping, Saturating, Unchecked };
std::optional<std::int64_t> addInteger(
    std::int64_t left,
    std::int64_t right,
    IntegerType type,
    OverflowPolicy policy) noexcept;

enum class NumericKind { Integer, Floating };
struct NumericType final { NumericKind kind; unsigned bits; bool isSigned; };
bool implicitNumericConversionIsLossless(NumericType source, NumericType target) noexcept;

enum class FloatingPointMode { Strict, Precise, Fast };
struct FloatingPointContract final
{
    bool preservesSignedZero;
    bool preservesNanSemantics;
    bool allowsContraction;
    bool allowsReassociation;
};
FloatingPointContract floatingPointContract(FloatingPointMode mode) noexcept;

struct StrongNumericType final { std::string identity; NumericType representation; };
bool strongNumericHasRuntimeWrapper() noexcept;
bool strongNumericImplicitlyConverts(const StrongNumericType& source, const StrongNumericType& target) noexcept;

bool integerSemanticsSelfCheck() noexcept;
}
