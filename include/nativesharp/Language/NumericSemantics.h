#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
inline constexpr unsigned DecimalCoefficientBits = 96;
inline constexpr unsigned DecimalMaximumScale = 28;
inline constexpr std::uint64_t DecimalCoefficientHighMask = 0x00000000FFFFFFFFull;
inline constexpr std::uint64_t DecimalScaleMask = 0x0000003F00000000ull;
inline constexpr std::uint64_t DecimalSignMask = 0x8000000000000000ull;
inline constexpr std::uint64_t DecimalReservedMask = 0x7FFFFFC000000000ull;

// Stores the exact NativeSharp decimal payload as two little-endian 64-bit words.
// `low` contains coefficient bits 0..63. `high` contains coefficient bits 64..95,
// scale bits 96..101, reserved zero bits 102..126, and the sign at bit 127.
struct Decimal128Bits final
{
    std::uint64_t low = 0;
    std::uint64_t high = 0;
};

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

// NS304-T15-F02: the exact value of a decimal literal spelled in source. The digits and scale are
// recovered verbatim from the text (no binary floating-point round-trip), so `0.1` is exactly
// coefficient 1 / scale 1 and round-trips text→decimal→text. Scale must be 0..28 and the coefficient
// must fit 96 bits.
struct DecimalLiteralValue final
{
    std::uint64_t coefficientLow = 0;
    std::uint32_t coefficientHigh = 0;
    unsigned scale = 0;
    bool negative = false;
};
std::optional<DecimalLiteralValue> parseDecimalLiteral(
    std::string_view spelling,
    std::string& error) noexcept;

struct IntegerType final { unsigned bits = 32; bool isSigned = true; };
bool literalFits(std::uint64_t magnitude, bool negative, IntegerType type) noexcept;
std::optional<std::uint64_t> parseIntegerLiteralMagnitude(std::string_view spelling, std::string& error);

enum class IntegerLiteralBase : unsigned { Binary = 2, Octal = 8, Decimal = 10, Hexadecimal = 16 };
enum class IntegerLiteralSuffix
{
    None,
    I8, I16, I32, I64, ISize,
    U8, U16, U32, U64, USize
};

struct UntypedIntegerLiteral final
{
    std::uint64_t magnitude = 0;
    IntegerLiteralBase base = IntegerLiteralBase::Decimal;
    IntegerLiteralSuffix suffix = IntegerLiteralSuffix::None;
};

struct ResolvedIntegerLiteral final
{
    std::uint64_t magnitude = 0;
    bool negative = false;
    IntegerType type;
    bool usedContext = false;
};

std::optional<UntypedIntegerLiteral> parseUntypedIntegerLiteral(
    std::string_view spelling,
    std::string& error);
std::optional<ResolvedIntegerLiteral> resolveIntegerLiteral(
    const UntypedIntegerLiteral& literal,
    bool negative,
    std::optional<IntegerType> contextualType,
    std::string& error) noexcept;

enum class OverflowPolicy { Checked, Wrapping, Saturating, Unchecked };
std::optional<std::int64_t> addInteger(
    std::int64_t left,
    std::int64_t right,
    IntegerType type,
    OverflowPolicy policy) noexcept;

enum class IntegerArithmeticOperation
{
    Add,
    Subtract,
    Multiply,
    Divide,
    ShiftLeft,
    ShiftRight
};
enum class NumericEvaluationDomain { Runtime, Constant, SimdLane, Intrinsic };

struct IntegerArithmeticRequest final
{
    IntegerArithmeticOperation operation = IntegerArithmeticOperation::Add;
    std::uint64_t leftBits = 0;
    std::uint64_t rightBits = 0;
    IntegerType type;
    OverflowPolicy policy = OverflowPolicy::Checked;
    NumericEvaluationDomain domain = NumericEvaluationDomain::Runtime;
};

struct IntegerArithmeticResult final
{
    std::uint64_t bits = 0;
    bool overflowed = false;
    bool undefinedBehavior = false;
};

std::optional<IntegerArithmeticResult> evaluateIntegerArithmetic(
    const IntegerArithmeticRequest& request,
    std::string& error) noexcept;
std::int64_t integerResultAsSigned(
    const IntegerArithmeticResult& result,
    IntegerType type) noexcept;

enum class NumericKind { Integer, Floating, Enum };
struct NumericType final { NumericKind kind; unsigned bits; bool isSigned; };
bool implicitNumericConversionIsLossless(NumericType source, NumericType target) noexcept;

enum class NumericConversionKind
{
    Identity,
    ImplicitLossless,
    ImplicitConstant,
    ExplicitNarrowing,
    ExplicitSignChange,
    ExplicitFloatInteger,
    ExplicitEnumUnderlying,
    Invalid
};
enum class NumericConversionUse { Assignment, Overload, ConstGeneric, EnumUnderlying, SimdSplat };

struct NumericConstant final
{
    std::uint64_t magnitude = 0;
    bool negative = false;
    bool floating = false;
};

struct NumericConversionDecision final
{
    NumericConversionKind kind = NumericConversionKind::Invalid;
    unsigned overloadRank = 255;
    bool implicit = false;
    bool lossless = false;
    bool requiresOverflowPolicy = false;
};

NumericConversionDecision classifyNumericConversion(
    NumericType source,
    NumericType target,
    std::optional<NumericConstant> constant,
    NumericConversionUse use) noexcept;

enum class FloatingPointMode { Strict, Precise, Fast };
struct FloatingPointContract final
{
    bool preservesSignedZero;
    bool preservesNanSemantics;
    bool allowsContraction;
    bool allowsReassociation;
    bool preservesNanPayload;
    bool preservesDenormals;
    bool constevalMatchesRuntime;
};
FloatingPointContract floatingPointContract(FloatingPointMode mode) noexcept;

struct StrongNumericType final { std::string identity; NumericType representation; };
bool strongNumericHasRuntimeWrapper() noexcept;
bool strongNumericImplicitlyConverts(const StrongNumericType& source, const StrongNumericType& target) noexcept;

enum class StrongNumericOperator { Add, Subtract, Multiply, Divide, Compare };

struct StrongNumericDefinition final
{
    std::string identity;
    NumericType representation{NumericKind::Integer, 32, true};
    std::size_t fieldCount = 1;
    bool reprTransparent = false;
    bool ffiExported = false;
    bool derivesCopy = false;
    bool derivesMove = true;
    std::size_t constGenericArity = 0;
    std::vector<StrongNumericOperator> operators;
};

struct StrongNumericPlan final
{
    StrongNumericType type;
    std::size_t size = 0;
    std::size_t alignment = 1;
    std::size_t constGenericArity = 0;
    bool abiTransparent = false;
    bool zeroRuntimeOverhead = true;
    bool constructionIsExplicit = true;
    bool extractionIsExplicit = true;
    bool derivesCopy = false;
    bool derivesMove = true;
    std::vector<StrongNumericOperator> operators;
};

std::optional<StrongNumericPlan> buildStrongNumericPlan(
    const StrongNumericDefinition& definition,
    std::string& error) noexcept;
bool strongNumericOperatorAllowed(
    const StrongNumericPlan& left,
    const StrongNumericPlan& right,
    StrongNumericOperator operation) noexcept;

struct ConstantPointer final
{
    std::uint64_t allocation = 0;
    std::size_t offset = 0;
};

struct ConstantAllocation final
{
    std::uint64_t identity = 0;
    std::size_t size = 0;
    std::size_t alignment = 1;
    bool live = true;
};

class ConstantMemory final
{
public:
    std::optional<ConstantPointer> allocate(
        std::size_t size,
        std::size_t alignment,
        std::string& error);
    std::optional<ConstantPointer> offset(
        ConstantPointer pointer,
        std::ptrdiff_t delta,
        std::string& error) const;
    bool validateDereference(
        ConstantPointer pointer,
        std::size_t width,
        std::string& error) const;
    bool deallocate(std::uint64_t identity, std::string& error);
private:
    std::uint64_t nextIdentity_ = 1;
    std::vector<ConstantAllocation> allocations_;
};

struct ConstantUnionState final
{
    std::string activeMember;
    std::size_t storageSize = 0;
};
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
