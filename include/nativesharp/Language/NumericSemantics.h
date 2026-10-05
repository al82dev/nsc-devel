#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::language
{
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
    bool preservesNanPayload;
    bool preservesDenormals;
    bool constevalMatchesRuntime;
};
FloatingPointContract floatingPointContract(FloatingPointMode mode) noexcept;

struct StrongNumericType final { std::string identity; NumericType representation; };
bool strongNumericHasRuntimeWrapper() noexcept;
bool strongNumericImplicitlyConverts(const StrongNumericType& source, const StrongNumericType& target) noexcept;

bool integerSemanticsSelfCheck() noexcept;
}
