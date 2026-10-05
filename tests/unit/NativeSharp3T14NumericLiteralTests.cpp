#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const auto binary = parseUntypedIntegerLiteral("0b1111_0000u8", error);
    const auto octal = parseUntypedIntegerLiteral("0o755u16", error);
    const auto hex = parseUntypedIntegerLiteral("0xFFFF_FFFFu32", error);
    tests.expect(binary.has_value() && binary->magnitude == 240 &&
            binary->base == IntegerLiteralBase::Binary &&
            octal.has_value() && octal->magnitude == 493 &&
            hex.has_value() && hex->magnitude == 0xffffffffULL,
        "binary, octal, hexadecimal, separators, and suffixes parse before typing");

    const auto contextual = parseUntypedIntegerLiteral("255", error);
    const auto resolvedU8 = contextual.has_value()
        ? resolveIntegerLiteral(*contextual, false, IntegerType{8, false}, error)
        : std::nullopt;
    tests.expect(resolvedU8.has_value() && resolvedU8->usedContext &&
            resolvedU8->type.bits == 8 && !resolvedU8->type.isSigned,
        "an unsuffixed literal adopts a lossless contextual overload type");

    const auto i8Minimum = parseUntypedIntegerLiteral("128i8", error);
    tests.expect(i8Minimum.has_value() &&
            resolveIntegerLiteral(*i8Minimum, true, std::nullopt, error).has_value(),
        "unary minus is applied after parsing so the signed minimum is representable");
    tests.expect(i8Minimum.has_value() &&
            !resolveIntegerLiteral(*i8Minimum, false, std::nullopt, error).has_value() &&
            error.find("before conversion") != std::string::npos,
        "positive overflow is diagnosed against the suffixed width before conversion");

    const auto nativeWidth = parseUntypedIntegerLiteral("42usize", error);
    const auto resolvedNative = nativeWidth.has_value()
        ? resolveIntegerLiteral(*nativeWidth, false, std::nullopt, error)
        : std::nullopt;
    tests.expect(resolvedNative.has_value() && resolvedNative->type.bits == 64 &&
            !resolvedNative->type.isSigned,
        "usize resolves to the frozen Windows x64 native width");

    const auto defaultSmall = parseUntypedIntegerLiteral("2147483647", error);
    const auto defaultWide = parseUntypedIntegerLiteral("2147483648", error);
    tests.expect(defaultSmall.has_value() && defaultWide.has_value() &&
            resolveIntegerLiteral(*defaultSmall, false, std::nullopt, error)->type.bits == 32 &&
            resolveIntegerLiteral(*defaultWide, false, std::nullopt, error)->type.bits == 64,
        "unconstrained literals choose i32 then i64 deterministically");

    tests.expect(!parseUntypedIntegerLiteral("0x_FF", error).has_value() &&
            error.find("separators") != std::string::npos &&
            !parseUntypedIntegerLiteral("12__34", error).has_value(),
        "separators are accepted only between digits");
    tests.expect(!parseUntypedIntegerLiteral("256u8", error).has_value() ||
            !resolveIntegerLiteral(*parseUntypedIntegerLiteral("256u8", error), false,
                std::nullopt, error).has_value(),
        "every fixed-width suffix checks its boundary");
    tests.expect(!parseUntypedIntegerLiteral("0b102", error).has_value() &&
            error.find("invalid digit") != std::string::npos,
        "digits outside the selected base produce a causal diagnostic");
    tests.expect(!parseUntypedIntegerLiteral("-1", error).has_value() &&
            error.find("unary sign") != std::string::npos,
        "the lexer model keeps unary signs separate from initially untyped literals");
    return tests.result();
}
