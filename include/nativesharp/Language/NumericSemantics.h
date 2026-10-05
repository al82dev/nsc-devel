#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

bool integerSemanticsSelfCheck() noexcept;
}
