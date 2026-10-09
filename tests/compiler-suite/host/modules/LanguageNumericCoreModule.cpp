#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/NumericSemantics.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-016", "Host.Language", "unit", "PR",
        {"language", "numeric"}, std::move(function)});
}

void registerLiteralFitsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-192", [](TestContext& context) {
        constexpr std::array widths = {1U, 8U, 16U, 32U, 64U};
        for (const unsigned bits : widths)
        {
            const std::uint64_t unsignedMax = bits == 64 ? (std::numeric_limits<std::uint64_t>::max)() :
                (std::uint64_t{1} << bits) - 1U;
            const std::uint64_t signedMax = bits == 64 ?
                static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) :
                (std::uint64_t{1} << (bits - 1U)) - 1U;
            context.expect(literalFits(unsignedMax, false, {bits, false}) &&
                    !literalFits(1, true, {bits, false}),
                "TL-LANG-192 unsigned boundaries and negative rejection must be bit-exact");
            context.expect(literalFits(signedMax, false, {bits, true}) &&
                    literalFits(signedMax + 1U, true, {bits, true}) &&
                    !literalFits(signedMax + 1U, false, {bits, true}),
                "TL-LANG-192 signed positive and negative magnitude boundaries must be bit-exact");
            if (bits < 64)
                context.expect(!literalFits(unsignedMax + 1U, false, {bits, false}),
                    "TL-LANG-192 unsigned overflow immediately above the width must be rejected");
        }
        context.expect(!literalFits(0, false, {0, true}) && !literalFits(0, false, {65, true}),
            "TL-LANG-192 widths outside one through sixty-four must be rejected");
    });
}

void registerMagnitudeValidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-193", [](TestContext& context) {
        std::string error;
        context.expect(parseIntegerLiteralMagnitude("4_294_967_295", error) == 4294967295ULL &&
                parseIntegerLiteralMagnitude("0xFF_ff", error) == 65535 &&
                parseIntegerLiteralMagnitude("0b1010_0101", error) == 165 && error.empty(),
            "TL-LANG-193 decimal, hexadecimal, binary, and separators must parse exactly");
    });
}

void registerMagnitudeInvalidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-194", [](TestContext& context) {
        constexpr const char* expected = "invalid or overflowing integer literal";
        const std::array invalid = {"0o77", "0x", "0b2", "_", "18446744073709551616"};
        std::string error;
        for (const char* spelling : invalid)
            context.expect(!parseIntegerLiteralMagnitude(spelling, error).has_value() && error == expected,
                "TL-LANG-194 unsupported base, prefix, digit, separator, and overflow errors must be exact");
    });
}

void registerUntypedValidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-195", [](TestContext& context) {
        struct Sample final { const char* text; std::uint64_t value; IntegerLiteralBase base;
            IntegerLiteralSuffix suffix; };
        constexpr std::array samples = {
            Sample{"0b1111_0000u8", 240, IntegerLiteralBase::Binary, IntegerLiteralSuffix::U8},
            Sample{"0o755u16", 493, IntegerLiteralBase::Octal, IntegerLiteralSuffix::U16},
            Sample{"42isize", 42, IntegerLiteralBase::Decimal, IntegerLiteralSuffix::ISize},
            Sample{"0xFFFF_FFFFu32", 0xffffffffULL, IntegerLiteralBase::Hexadecimal,
                IntegerLiteralSuffix::U32}};
        std::string error;
        for (const Sample& sample : samples)
        {
            const auto result = parseUntypedIntegerLiteral(sample.text, error);
            context.expect(result.has_value() && result->magnitude == sample.value &&
                    result->base == sample.base && result->suffix == sample.suffix && error.empty(),
                "TL-LANG-195 base, magnitude, suffix, and separators must parse exactly");
        }
    });
}

void registerUntypedInvalidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-196", [](TestContext& context) {
        std::string error;
        context.expect(!parseUntypedIntegerLiteral("", error) &&
                error == "integer literal spelling is unsigned; unary sign is a separate expression" &&
                !parseUntypedIntegerLiteral("-1", error) &&
                error == "integer literal spelling is unsigned; unary sign is a separate expression",
            "TL-LANG-196 empty and signed spellings must report the exact sign-boundary error");
        for (const char* spelling : {"0x_FF", "12__34", "1_"})
            context.expect(!parseUntypedIntegerLiteral(spelling, error) &&
                    error == "integer literal separators must appear between digits",
                "TL-LANG-196 invalid separator placement must report the exact error");
        for (const char* spelling : {"0b102", "0xGG", "18446744073709551616"})
            context.expect(!parseUntypedIntegerLiteral(spelling, error) &&
                    error == "invalid digit or magnitude overflow in integer literal",
                "TL-LANG-196 invalid digits and magnitude overflow must report the exact error");
    });
}

void registerResolveValidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-197", [](TestContext& context) {
        std::string error;
        const UntypedIntegerLiteral plain{255, IntegerLiteralBase::Decimal, IntegerLiteralSuffix::None};
        const auto contextual = resolveIntegerLiteral(plain, false, IntegerType{8, false}, error);
        const auto default32 = resolveIntegerLiteral({2147483647, IntegerLiteralBase::Decimal,
            IntegerLiteralSuffix::None}, false, std::nullopt, error);
        const auto default64 = resolveIntegerLiteral({2147483648, IntegerLiteralBase::Decimal,
            IntegerLiteralSuffix::None}, false, std::nullopt, error);
        const auto minimum = resolveIntegerLiteral({128, IntegerLiteralBase::Decimal,
            IntegerLiteralSuffix::I8}, true, std::nullopt, error);
        context.expect(contextual && contextual->usedContext && contextual->type.bits == 8 &&
                !contextual->type.isSigned && default32 && default32->type.bits == 32 &&
                default64 && default64->type.bits == 64 && minimum && minimum->negative,
            "TL-LANG-197 context, defaults, suffix, sign, and resolved fields must be exact");
    });
}

void registerResolveInvalidCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-198", [](TestContext& context) {
        std::string error;
        const UntypedIntegerLiteral suffixed{256, IntegerLiteralBase::Decimal, IntegerLiteralSuffix::U8};
        context.expect(!resolveIntegerLiteral(suffixed, false, std::nullopt, error) &&
                error == "integer literal overflows its resolved type before conversion",
            "TL-LANG-198 fixed-width overflow must report the exact diagnostic");
        const UntypedIntegerLiteral unsignedOne{1, IntegerLiteralBase::Decimal, IntegerLiteralSuffix::U8};
        context.expect(!resolveIntegerLiteral(unsignedOne, true, std::nullopt, error) &&
                error == "integer literal overflows its resolved type before conversion",
            "TL-LANG-198 a negative unsigned literal must fail before conversion");
        context.expect(!resolveIntegerLiteral(unsignedOne, false, IntegerType{16, false}, error) &&
                error == "integer literal suffix conflicts with the contextual type",
            "TL-LANG-198 suffix/context conflict must take diagnostic priority");
    });
}

void registerAddCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-199", [](TestContext& context) {
        constexpr IntegerType i8{8, true};
        context.expect(!addInteger(127, 1, i8, OverflowPolicy::Checked) &&
                addInteger(127, 1, i8, OverflowPolicy::Wrapping) == -128 &&
                addInteger(127, 1, i8, OverflowPolicy::Unchecked) == -128 &&
                addInteger(127, 1, i8, OverflowPolicy::Saturating) == 127,
            "TL-LANG-199 positive overflow must follow all four policies exactly");
        context.expect(!addInteger(-128, -1, i8, OverflowPolicy::Checked) &&
                addInteger(-128, -1, i8, OverflowPolicy::Wrapping) == 127 &&
                addInteger(-128, -1, i8, OverflowPolicy::Saturating) == -128,
            "TL-LANG-199 negative overflow must wrap or saturate exactly");
    });
}

void registerArithmeticCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-200", [](TestContext& context) {
        constexpr std::array widths = {1U, 8U, 16U, 32U, 64U};
        std::string error;
        for (const unsigned width : widths)
        {
            const std::uint64_t maximum = width == 64 ?
                static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) :
                (std::uint64_t{1} << (width - 1U)) - 1U;
            for (const IntegerArithmeticOperation operation : {IntegerArithmeticOperation::Add,
                    IntegerArithmeticOperation::Subtract, IntegerArithmeticOperation::Multiply})
            {
                const auto result = evaluateIntegerArithmetic(
                    {operation, maximum, operation == IntegerArithmeticOperation::Subtract ? 1U : 2U,
                        {width, true}, OverflowPolicy::Wrapping, NumericEvaluationDomain::Runtime}, error);
                context.expect(result.has_value() && error.empty(),
                    "TL-LANG-200 add, subtract, and multiply must evaluate at every supported width");
            }
        }
        const auto unchecked = evaluateIntegerArithmetic({IntegerArithmeticOperation::Add, 127, 1,
            {8, true}, OverflowPolicy::Unchecked, NumericEvaluationDomain::Runtime}, error);
        context.expect(unchecked && unchecked->bits == 128 && unchecked->overflowed &&
                unchecked->undefinedBehavior,
            "TL-LANG-200 unchecked signed overflow must expose wrapped bits, overflow, and UB");
    });
}

void registerDivisionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-201", [](TestContext& context) {
        std::string error = "stale";
        const auto quotient = evaluateIntegerArithmetic({IntegerArithmeticOperation::Divide, 255, 3,
            {8, false}, OverflowPolicy::Checked, NumericEvaluationDomain::Runtime}, error);
        context.expect(quotient && quotient->bits == 85 && !quotient->overflowed &&
                !quotient->undefinedBehavior && error.empty(),
            "TL-LANG-201 ordinary unsigned division must produce exact bits without overflow");

        for (const OverflowPolicy policy : {OverflowPolicy::Checked, OverflowPolicy::Wrapping,
                 OverflowPolicy::Saturating, OverflowPolicy::Unchecked})
        {
            context.expect(!evaluateIntegerArithmetic({IntegerArithmeticOperation::Divide, 7, 0,
                    {8, true}, policy, NumericEvaluationDomain::Runtime}, error) &&
                    error == "integer division by zero",
                "TL-LANG-201 division by zero must fail identically under every overflow policy");
        }

        const auto checked = evaluateIntegerArithmetic({IntegerArithmeticOperation::Divide, 0x80, 0xff,
            {8, true}, OverflowPolicy::Checked, NumericEvaluationDomain::Runtime}, error);
        context.expect(!checked && error == "checked integer operation overflowed",
            "TL-LANG-201 checked signed minimum divided by negative one must reject overflow");

        const auto wrapping = evaluateIntegerArithmetic({IntegerArithmeticOperation::Divide, 0x80, 0xff,
            {8, true}, OverflowPolicy::Wrapping, NumericEvaluationDomain::Runtime}, error);
        context.expect(wrapping && wrapping->bits == 0x80 && wrapping->overflowed &&
                !wrapping->undefinedBehavior && error.empty(),
            "TL-LANG-201 wrapping signed division overflow must preserve wrapped bits and evidence");

        const auto saturating = evaluateIntegerArithmetic({IntegerArithmeticOperation::Divide, 0x80, 0xff,
            {8, true}, OverflowPolicy::Saturating, NumericEvaluationDomain::Runtime}, error);
        context.expect(saturating && saturating->bits == 0x7f && saturating->overflowed &&
                !saturating->undefinedBehavior && error.empty(),
            "TL-LANG-201 saturating signed division overflow must clamp to the signed maximum");

        const auto unchecked = evaluateIntegerArithmetic({IntegerArithmeticOperation::Divide, 0x80, 0xff,
            {8, true}, OverflowPolicy::Unchecked, NumericEvaluationDomain::Runtime}, error);
        context.expect(unchecked && unchecked->bits == 0x80 && unchecked->overflowed &&
                unchecked->undefinedBehavior && error.empty(),
            "TL-LANG-201 unchecked signed division overflow must expose undefined behavior");
    });
}

void registerShiftCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-202", [](TestContext& context) {
        std::string error;
        for (const std::uint64_t shift : {0ULL, 7ULL})
        {
            const auto result = evaluateIntegerArithmetic({IntegerArithmeticOperation::ShiftLeft, 1, shift,
                {8, false}, OverflowPolicy::Checked, NumericEvaluationDomain::Runtime}, error);
            context.expect(result && result->bits == (1ULL << shift) && !result->overflowed,
                "TL-LANG-202 in-range shifts must preserve exact bits without overflow");
        }
        for (const std::uint64_t shift : {8ULL, 9ULL})
        {
            context.expect(!evaluateIntegerArithmetic({IntegerArithmeticOperation::ShiftLeft, 1, shift,
                    {8, false}, OverflowPolicy::Checked, NumericEvaluationDomain::Runtime}, error) &&
                    error == "integer shift count is outside the operand width",
                "TL-LANG-202 checked out-of-range shifts must report the exact error");
            const auto wrapped = evaluateIntegerArithmetic({IntegerArithmeticOperation::ShiftLeft, 1, shift,
                {8, false}, OverflowPolicy::Wrapping, NumericEvaluationDomain::Runtime}, error);
            context.expect(wrapped && wrapped->bits == (1ULL << (shift % 8U)) && wrapped->overflowed,
                "TL-LANG-202 wrapping shifts must reduce the count and retain overflow evidence");
        }
    });
}

void registerSignedResultCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-203", [](TestContext& context) {
        context.expect(integerResultAsSigned({0, false, false}, {1, true}) == 0 &&
                integerResultAsSigned({1, false, false}, {1, true}) == -1 &&
                integerResultAsSigned({0x7f, false, false}, {8, true}) == 127 &&
                integerResultAsSigned({0x80, false, false}, {8, true}) == -128 &&
                integerResultAsSigned({0xffffffffffffffffULL, false, false}, {64, true}) == -1,
            "TL-LANG-203 sign-bit interpretation and extension must be exact at boundary widths");
    });
}
}

void registerLanguageNumericCoreModule(TestRegistry& registry)
{
    registerLiteralFitsCase(registry);
    registerMagnitudeValidCase(registry);
    registerMagnitudeInvalidCase(registry);
    registerUntypedValidCase(registry);
    registerUntypedInvalidCase(registry);
    registerResolveValidCase(registry);
    registerResolveInvalidCase(registry);
    registerAddCase(registry);
    registerArithmeticCase(registry);
    registerDivisionCase(registry);
    registerShiftCase(registry);
    registerSignedResultCase(registry);
}
}
