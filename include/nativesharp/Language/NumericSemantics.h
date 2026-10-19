#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/Decimal128Bits.h"
#include "nativesharp/Language/DecimalArithmeticOperation.h"
#include "nativesharp/Language/DecimalLiteralValue.h"
#include "nativesharp/Language/IntegerArithmeticOperation.h"
#include "nativesharp/Language/IntegerArithmeticRequest.h"
#include "nativesharp/Language/IntegerArithmeticResult.h"
#include "nativesharp/Language/IntegerLiteralBase.h"
#include "nativesharp/Language/IntegerLiteralSuffix.h"
#include "nativesharp/Language/IntegerType.h"
#include "nativesharp/Language/NumericEvaluationDomain.h"
#include "nativesharp/Language/NumericConstant.h"
#include "nativesharp/Language/NumericConversionDecision.h"
#include "nativesharp/Language/NumericConversionKind.h"
#include "nativesharp/Language/NumericConversionUse.h"
#include "nativesharp/Language/NumericKind.h"
#include "nativesharp/Language/NumericType.h"
#include "nativesharp/Language/OverflowPolicy.h"
#include "nativesharp/Language/ResolvedIntegerLiteral.h"
#include "nativesharp/Language/FloatingPointContract.h"
#include "nativesharp/Language/FloatingPointMode.h"
#include "nativesharp/Language/StrongNumericDefinition.h"
#include "nativesharp/Language/StrongNumericOperator.h"
#include "nativesharp/Language/StrongNumericPlan.h"
#include "nativesharp/Language/StrongNumericType.h"
#include "nativesharp/Language/ConstantAllocation.h"
#include "nativesharp/Language/ConstantMemory.h"
#include "nativesharp/Language/ConstantPointer.h"
#include "nativesharp/Language/ConstantUnionState.h"
#include "nativesharp/Language/UntypedIntegerLiteral.h"

namespace nativesharp::language
{
inline constexpr unsigned DecimalCoefficientBits = 96;
inline constexpr unsigned DecimalMaximumScale = 28;
inline constexpr std::uint64_t DecimalCoefficientHighMask = 0x00000000FFFFFFFFull;
inline constexpr std::uint64_t DecimalScaleMask = 0x0000003F00000000ull;
inline constexpr std::uint64_t DecimalSignMask = 0x8000000000000000ull;
inline constexpr std::uint64_t DecimalReservedMask = 0x7FFFFFC000000000ull;

// Packs a finite decimal payload. Every successful result has zero reserved bits and scale <= 28.
std::optional<Decimal128Bits> makeDecimal128Bits(
    std::uint64_t coefficientLow,
    std::uint32_t coefficientHigh,
    unsigned scale,
    bool negative) noexcept;

// Rejects payloads that safe NativeSharp code cannot construct: reserved bits or scale above 28.
bool decimal128BitsAreValid(Decimal128Bits value) noexcept;

// Extracts the upper 32 coefficient bits without including scale, reserved, or sign bits.
std::uint32_t decimalCoefficientHigh(Decimal128Bits value) noexcept;

// Extracts the stored base-10 scale in the inclusive range 0..28 for valid payloads.
unsigned decimalScale(Decimal128Bits value) noexcept;

// Reports the independent sign bit. Zero may carry a sign before normalization.
bool decimalIsNegative(Decimal128Bits value) noexcept;

// Produces the numeric canonical form without mutating the stored operand. Trailing coefficient
// zeroes are removed while scale permits; every zero becomes positive with scale zero.
std::optional<Decimal128Bits> normalizeDecimal128Bits(Decimal128Bits value) noexcept;

// Compares finite decimal values numerically, so distinct stored scales compare equal after normalization.
bool decimal128ValuesEqual(Decimal128Bits left, Decimal128Bits right) noexcept;

// Hashes the normalized numeric value. Numerically equal valid decimals always receive the same hash.
std::optional<std::uint64_t> decimal128StableHash(Decimal128Bits value) noexcept;

std::optional<DecimalLiteralValue> parseDecimalLiteral(
    std::string_view spelling,
    std::string& error) noexcept;

std::optional<Decimal128Bits> evaluateDecimalArithmetic(
    const Decimal128Bits& left,
    const Decimal128Bits& right,
    DecimalArithmeticOperation operation,
    std::string& error) noexcept;

// NS304-T15-F08: the culture-free decimal→text emission ("±<int>.<frac>") — the same digit
// emission that String.ToString(decimal) lowers to. No locale and no binary floating point.
std::string decimalToStringText(const Decimal128Bits& value) noexcept;

bool literalFits(std::uint64_t magnitude, bool negative, IntegerType type) noexcept;
std::optional<std::uint64_t> parseIntegerLiteralMagnitude(std::string_view spelling, std::string& error);

std::optional<UntypedIntegerLiteral> parseUntypedIntegerLiteral(
    std::string_view spelling,
    std::string& error);
std::optional<ResolvedIntegerLiteral> resolveIntegerLiteral(
    const UntypedIntegerLiteral& literal,
    bool negative,
    std::optional<IntegerType> contextualType,
    std::string& error) noexcept;

std::optional<std::int64_t> addInteger(
    std::int64_t left,
    std::int64_t right,
    IntegerType type,
    OverflowPolicy policy) noexcept;

std::optional<IntegerArithmeticResult> evaluateIntegerArithmetic(
    const IntegerArithmeticRequest& request,
    std::string& error) noexcept;
std::int64_t integerResultAsSigned(
    const IntegerArithmeticResult& result,
    IntegerType type) noexcept;

bool implicitNumericConversionIsLossless(NumericType source, NumericType target) noexcept;

NumericConversionDecision classifyNumericConversion(
    NumericType source,
    NumericType target,
    std::optional<NumericConstant> constant,
    NumericConversionUse use) noexcept;

FloatingPointContract floatingPointContract(FloatingPointMode mode) noexcept;

bool strongNumericHasRuntimeWrapper() noexcept;
bool strongNumericImplicitlyConverts(const StrongNumericType& source, const StrongNumericType& target) noexcept;

std::optional<StrongNumericPlan> buildStrongNumericPlan(
    const StrongNumericDefinition& definition,
    std::string& error) noexcept;
bool strongNumericOperatorAllowed(
    const StrongNumericPlan& left,
    const StrongNumericPlan& right,
    StrongNumericOperator operation) noexcept;

bool activateConstantUnionMember(
    ConstantUnionState& state,
    std::string_view member,
    std::size_t memberSize,
    std::string& error);
bool readConstantUnionMember(
    const ConstantUnionState& state,
    std::string_view member,
    std::string& error) noexcept;

std::optional<IntegerArithmeticResult> evaluateConstantIntegerArithmetic(
    IntegerArithmeticRequest request,
    std::string& error) noexcept;
std::optional<IntegerArithmeticResult> executeConstantIntegerLoop(
    std::uint64_t seedBits,
    std::uint64_t operandBits,
    std::size_t iterations,
    std::size_t iterationBudget,
    IntegerArithmeticOperation operation,
    IntegerType type,
    OverflowPolicy policy,
    std::string& error) noexcept;

bool integerSemanticsSelfCheck() noexcept;
}
