#include "SemaConversionEvaluationTestSupport.h"

#include <tuple>

namespace nsc::suite
{
namespace
{
using namespace sema_conversion_evaluation_support;

ExpressionInfo evaluateUnary(
    SemaFixture& fixture,
    TokenKind operation,
    const ExpressionInfo& operand,
    Type* resultType)
{
    AstNode* node = fixture.node(AstNodeKind::UnaryExpression);
    node->setTokenKind(operation);
    ExpressionInfo result;
    result.type = resultType;
    return fixture.analyzer.evaluateConstantUnary(node, operand, result);
}

ExpressionInfo evaluateBinary(
    SemaFixture& fixture,
    TokenKind operation,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    Type* resultType)
{
    AstNode* node = fixture.node(AstNodeKind::BinaryExpression);
    node->setTokenKind(operation);
    ExpressionInfo result;
    result.type = resultType;
    return fixture.analyzer.evaluateConstantBinary(node, left, right, result);
}

void testMaskForWidthObservability(TestContext& context)
{
    SemaFixture fixture;
    const ConstantValue byte = fixture.analyzer.convertConstant(
        ConstantValue::unsignedInteger(std::numeric_limits<std::uint64_t>::max()),
        fixture.types.builtin(TypeKind::NUInt), fixture.types.builtin(TypeKind::Byte));
    const ConstantValue integer = fixture.analyzer.convertConstant(
        ConstantValue::unsignedInteger(std::numeric_limits<std::uint64_t>::max()),
        fixture.types.builtin(TypeKind::NUInt), fixture.types.builtin(TypeKind::UInt));
    const ConstantValue native = fixture.analyzer.convertConstant(
        ConstantValue::unsignedInteger(std::numeric_limits<std::uint64_t>::max()),
        fixture.types.builtin(TypeKind::NUInt), fixture.types.builtin(TypeKind::NUInt));
    context.expect(byte.unsignedInteger() == 0xffU && integer.unsignedInteger() == 0xffffffffULL &&
            native.unsignedInteger() == std::numeric_limits<std::uint64_t>::max(),
        "TS-SEMA-281 observable 8/32/64-bit masks must truncate exact low bits through convertConstant");
    context.expect(false,
        "TS-SEMA-281 TEST-INFRASTRUCTURE: maskForWidth has anonymous linkage, so widths 0/1/63 need an isolated product-source seam");
}

void testSignedBitsRoundTrip(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (const auto [kind, minimum, maximum] : std::array{
             std::tuple{TypeKind::Byte, std::int64_t{0}, std::int64_t{255}},
             std::tuple{TypeKind::Short, std::int64_t{-32768}, std::int64_t{32767}},
             std::tuple{TypeKind::Int, std::int64_t{-2147483647} - 1, std::int64_t{2147483647}},
             std::tuple{TypeKind::Long, std::numeric_limits<std::int64_t>::min(),
                 std::numeric_limits<std::int64_t>::max()}})
    {
        Type* destination = fixture.types.builtin(kind);
        const ConstantValue low = fixture.analyzer.convertConstant(
            ConstantValue::signedInteger(minimum), fixture.types.builtin(TypeKind::Long), destination);
        const ConstantValue high = fixture.analyzer.convertConstant(
            ConstantValue::signedInteger(maximum), fixture.types.builtin(TypeKind::Long), destination);
        const std::uint32_t width = fixture.analyzer.integerWidth(destination);
        if (fixture.analyzer.isSignedInteger(destination))
        {
            exact = exact && low.signedInteger() == minimum && high.signedInteger() == maximum;
        }
        else
        {
            const std::uint64_t mask = width == 64 ? std::numeric_limits<std::uint64_t>::max()
                : (std::uint64_t{1} << width) - 1;
            exact = exact && low.unsignedInteger() == (static_cast<std::uint64_t>(minimum) & mask) &&
                high.unsignedInteger() == (static_cast<std::uint64_t>(maximum) & mask);
        }
    }
    context.expect(exact, "TS-SEMA-282 signed/bits conversions must round-trip exact boundary patterns at every width");
}

void testConstantAsSigned(TestContext& context)
{
    SemaFixture fixture;
    ExpressionInfo signedInfo = signedConstant(fixture.types.builtin(TypeKind::Int), -3);
    ExpressionInfo unsignedInfo = unsignedConstant(fixture.types.builtin(TypeKind::UInt), 7);
    ExpressionInfo tooLarge = unsignedConstant(
        fixture.types.builtin(TypeKind::ULong), std::numeric_limits<std::uint64_t>::max());
    ExpressionInfo boolean = booleanConstant(fixture.types.builtin(TypeKind::Bool), true);
    context.expect(fixture.analyzer.constantAsSigned(signedInfo) == std::optional<std::int64_t>(-3) &&
            fixture.analyzer.constantAsSigned(unsignedInfo) == std::optional<std::int64_t>(7) &&
            !fixture.analyzer.constantAsSigned(tooLarge).has_value() &&
            !fixture.analyzer.constantAsSigned(boolean).has_value(),
        "TS-SEMA-283 constantAsSigned must accept signed/fitting unsigned and reject bool/overflow/other payloads");
}

void testConstantAsUnsigned(TestContext& context)
{
    SemaFixture fixture;
    ExpressionInfo positive = signedConstant(fixture.types.builtin(TypeKind::Int), 9);
    ExpressionInfo negative = signedConstant(fixture.types.builtin(TypeKind::Int), -1);
    ExpressionInfo unsignedInfo = unsignedConstant(fixture.types.builtin(TypeKind::UInt), 12);
    ExpressionInfo boolean = booleanConstant(fixture.types.builtin(TypeKind::Bool), false);
    context.expect(fixture.analyzer.constantAsUnsigned(positive) == std::optional<std::uint64_t>(9) &&
            !fixture.analyzer.constantAsUnsigned(negative).has_value() &&
            fixture.analyzer.constantAsUnsigned(unsignedInfo) == std::optional<std::uint64_t>(12) &&
            !fixture.analyzer.constantAsUnsigned(boolean).has_value(),
        "TS-SEMA-284 constantAsUnsigned must accept unsigned/nonnegative signed and reject negative/bool payloads");
}

void testConvertIntegerWidths(TestContext& context)
{
    SemaFixture fixture;
    Type* longType = fixture.types.builtin(TypeKind::Long);
    const ConstantValue byte = fixture.analyzer.convertConstant(
        ConstantValue::signedInteger(-1), longType, fixture.types.builtin(TypeKind::Byte));
    const ConstantValue shortValue = fixture.analyzer.convertConstant(
        ConstantValue::unsignedInteger(0xffffU), fixture.types.builtin(TypeKind::UInt),
        fixture.types.builtin(TypeKind::Short));
    const ConstantValue integer = fixture.analyzer.convertConstant(
        ConstantValue::unsignedInteger(0x1ffffffffULL), fixture.types.builtin(TypeKind::ULong),
        fixture.types.builtin(TypeKind::UInt));
    const ConstantValue extended = fixture.analyzer.convertConstant(
        ConstantValue::signedInteger(-2), fixture.types.builtin(TypeKind::Int), longType);
    context.expect(byte.kind() == ConstantKind::UnsignedInteger && byte.unsignedInteger() == 0xffU &&
            shortValue.kind() == ConstantKind::SignedInteger && shortValue.signedInteger() == -1 &&
            integer.unsignedInteger() == 0xffffffffULL && extended.signedInteger() == -2,
        "TS-SEMA-285 integer conversion must apply exact modular truncation/sign interpretation at destination width");
}

void testConvertOtherConstantKinds(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* floating = fixture.types.builtin(TypeKind::Float);
    Type* enumeration = fixture.types.createEnum("E", nullptr);
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    Type* pointer = fixture.types.pointerTo(integer);
    const ConstantValue toFloat = fixture.analyzer.convertConstant(ConstantValue::signedInteger(3), integer, floating);
    const ConstantValue enumValue = fixture.analyzer.convertConstant(
        ConstantValue::signedInteger(5), integer, enumeration);
    const ConstantValue boolValue = fixture.analyzer.convertConstant(ConstantValue::boolean(true), boolean, integer);
    const ConstantValue pointerValue = fixture.analyzer.convertConstant(ConstantValue::nullValue(), pointer, pointer);
    context.expect(toFloat.kind() == ConstantKind::Floating && toFloat.floating() == 3.0 &&
            enumValue.kind() == ConstantKind::SignedInteger && enumValue.signedInteger() == 5 &&
            boolValue.kind() == ConstantKind::Boolean && boolValue.boolean() &&
            pointerValue.kind() == ConstantKind::Null,
        "TS-SEMA-286 float/enum conversions and current bool/pointer pass-through payloads must remain exact");
}

void testUnaryLogicalNot(TestContext& context)
{
    SemaFixture fixture;
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    const ExpressionInfo yes = evaluateUnary(fixture, TokenKind::Bang, booleanConstant(boolean, true), boolean);
    const ExpressionInfo no = evaluateUnary(fixture, TokenKind::Bang, booleanConstant(boolean, false), boolean);
    const ExpressionInfo integer = evaluateUnary(
        fixture, TokenKind::Bang, signedConstant(fixture.types.builtin(TypeKind::Int), 0), boolean);
    context.expect(yes.isConstant && !yes.constantValue.boolean() && no.isConstant && no.constantValue.boolean() &&
            !integer.isConstant,
        "TS-SEMA-287 unary ! must invert boolean constants and leave integer constants unevaluated in this helper");
}

void testUnaryMinusBoundaries(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* unsignedType = fixture.types.builtin(TypeKind::UInt);
    Type* doubleType = fixture.types.builtin(TypeKind::Double);
    const ExpressionInfo signedResult = evaluateUnary(
        fixture, TokenKind::Minus, signedConstant(integer, std::numeric_limits<std::int32_t>::min()), integer);
    const ExpressionInfo unsignedResult = evaluateUnary(
        fixture, TokenKind::Minus, unsignedConstant(unsignedType, 1), unsignedType);
    const ExpressionInfo floatingResult = evaluateUnary(
        fixture, TokenKind::Minus, floatingConstant(doubleType, 1.5), doubleType);
    context.expect(signedResult.isConstant && signedResult.constantValue.signedInteger() ==
            std::numeric_limits<std::int32_t>::min() && unsignedResult.isConstant &&
            unsignedResult.constantValue.unsignedInteger() == 0xffffffffULL && floatingResult.isConstant &&
            floatingResult.constantValue.floating() == -1.5,
        "TS-SEMA-288 unary minus must preserve modular signed/unsigned boundaries and exact floating negation");
}

void testUnaryBitwiseAndPlus(TestContext& context)
{
    SemaFixture fixture;
    Type* byteType = fixture.types.builtin(TypeKind::Byte);
    Type* intType = fixture.types.builtin(TypeKind::Int);
    const ExpressionInfo complement = evaluateUnary(
        fixture, TokenKind::Tilde, unsignedConstant(byteType, 0x0f), byteType);
    const ExpressionInfo identity = evaluateUnary(
        fixture, TokenKind::Plus, signedConstant(intType, -17), intType);
    context.expect(complement.isConstant && complement.constantValue.unsignedInteger() == 0xf0 &&
            identity.isConstant && identity.constantValue.signedInteger() == -17,
        "TS-SEMA-289 unary ~ must mask to width and unary + must preserve the exact constant");
}

void testBooleanTruthTableAndShortCircuit(TestContext& context)
{
    SemaFixture fixture;
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    bool truthTable = true;
    for (bool left : {false, true})
    {
        for (bool right : {false, true})
        {
            const ExpressionInfo andResult = evaluateBinary(
                fixture, TokenKind::AmpAmp, booleanConstant(boolean, left), booleanConstant(boolean, right), boolean);
            const ExpressionInfo orResult = evaluateBinary(
                fixture, TokenKind::PipePipe, booleanConstant(boolean, left), booleanConstant(boolean, right), boolean);
            truthTable = truthTable && andResult.isConstant && andResult.constantValue.boolean() == (left && right) &&
                orResult.isConstant && orResult.constantValue.boolean() == (left || right);
        }
    }
    AstNode* trapped = fixture.binary(TokenKind::Slash, fixture.integer(1), fixture.integer(0));
    AstNode* comparison = fixture.binary(TokenKind::EqualEqual, trapped, fixture.integer(0));
    AstNode* shortCircuit = fixture.binary(TokenKind::AmpAmp, fixture.boolean(false), comparison);
    const ExpressionInfo result = fixture.analyzer.analyzeBinary(shortCircuit);
    context.expect(truthTable && result.isConstant && !result.constantValue.boolean() &&
            !hasDiagnostic(fixture.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-290 logical truth tables and short-circuit trap suppression must remain exact");
}

void testSignedComparisons(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    const ExpressionInfo minus = signedConstant(integer, -2);
    const ExpressionInfo plus = signedConstant(integer, 3);
    bool exact = true;
    for (const auto [operation, expected] : std::array{
             std::pair{TokenKind::EqualEqual, false}, std::pair{TokenKind::BangEqual, true},
             std::pair{TokenKind::Less, true}, std::pair{TokenKind::LessEqual, true},
             std::pair{TokenKind::Greater, false}, std::pair{TokenKind::GreaterEqual, false}})
    {
        const ExpressionInfo result = evaluateBinary(fixture, operation, minus, plus, boolean);
        exact = exact && result.isConstant && result.constantValue.boolean() == expected;
    }
    context.expect(exact, "TS-SEMA-291 signed equality and relational operators must match the complete comparison matrix");
}

void testUnsignedComparisons(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::UInt);
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    const ExpressionInfo low = unsignedConstant(integer, 1);
    const ExpressionInfo high = unsignedConstant(integer, 0xffffffffULL);
    bool exact = true;
    for (const auto [operation, expected] : std::array{
             std::pair{TokenKind::EqualEqual, false}, std::pair{TokenKind::BangEqual, true},
             std::pair{TokenKind::Less, true}, std::pair{TokenKind::LessEqual, true},
             std::pair{TokenKind::Greater, false}, std::pair{TokenKind::GreaterEqual, false}})
    {
        const ExpressionInfo result = evaluateBinary(fixture, operation, low, high, boolean);
        exact = exact && result.isConstant && result.constantValue.boolean() == expected;
    }
    context.expect(exact, "TS-SEMA-292 unsigned equality and relational operators must use unsigned ordering exactly");
}

void testWrappingArithmetic(TestContext& context)
{
    SemaFixture fixture;
    Type* byteType = fixture.types.builtin(TypeKind::Byte);
    Type* uintType = fixture.types.builtin(TypeKind::UInt);
    const ExpressionInfo add = evaluateBinary(
        fixture, TokenKind::Plus, unsignedConstant(byteType, 255), unsignedConstant(byteType, 1), byteType);
    const ExpressionInfo subtract = evaluateBinary(
        fixture, TokenKind::Minus, unsignedConstant(byteType, 0), unsignedConstant(byteType, 1), byteType);
    const ExpressionInfo multiply = evaluateBinary(
        fixture, TokenKind::Star, unsignedConstant(uintType, 0x80000000ULL),
        unsignedConstant(uintType, 2), uintType);
    context.expect(add.isConstant && add.constantValue.unsignedInteger() == 0 && subtract.isConstant &&
            subtract.constantValue.unsignedInteger() == 255 && multiply.isConstant &&
            multiply.constantValue.unsignedInteger() == 0,
        "TS-SEMA-293 add/sub/mul must wrap bit-exactly at each destination width");
}

void testDivisionAndRemainder(TestContext& context)
{
    SemaFixture fixture;
    Type* signedType = fixture.types.builtin(TypeKind::Int);
    Type* unsignedType = fixture.types.builtin(TypeKind::UInt);
    const ExpressionInfo signedDivision = evaluateBinary(
        fixture, TokenKind::Slash, signedConstant(signedType, -17), signedConstant(signedType, 5), signedType);
    const ExpressionInfo signedRemainder = evaluateBinary(
        fixture, TokenKind::Percent, signedConstant(signedType, -17), signedConstant(signedType, 5), signedType);
    const ExpressionInfo unsignedDivision = evaluateBinary(
        fixture, TokenKind::Slash, unsignedConstant(unsignedType, 17), unsignedConstant(unsignedType, 5), unsignedType);
    const ExpressionInfo unsignedRemainder = evaluateBinary(
        fixture, TokenKind::Percent, unsignedConstant(unsignedType, 17), unsignedConstant(unsignedType, 5), unsignedType);
    context.expect(signedDivision.constantValue.signedInteger() == -3 &&
            signedRemainder.constantValue.signedInteger() == -2 &&
            unsignedDivision.constantValue.unsignedInteger() == 3 &&
            unsignedRemainder.constantValue.unsignedInteger() == 2,
        "TS-SEMA-294 signed/unsigned division and remainder must produce exact quotient and remainder");
}

void testDivisionTraps(TestContext& context)
{
    SemaFixture zero;
    Type* integer = zero.types.builtin(TypeKind::Int);
    const ExpressionInfo divideZero = evaluateBinary(
        zero, TokenKind::Slash, signedConstant(integer, 1), signedConstant(integer, 0), integer);
    SemaFixture overflow;
    Type* longType = overflow.types.builtin(TypeKind::Long);
    const ExpressionInfo minimum = evaluateBinary(overflow, TokenKind::Slash,
        signedConstant(longType, std::numeric_limits<std::int64_t>::min()), signedConstant(longType, -1), longType);
    context.expect(!divideZero.isConstant && !minimum.isConstant &&
            hasDiagnostic(zero.diagnostics, DiagnosticId::ConstantEvaluationTrap) &&
            hasDiagnostic(overflow.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-295 divide/mod zero and signed minimum/-1 must emit exact constant-evaluation traps");
}

void testBitwiseOperations(TestContext& context)
{
    SemaFixture fixture;
    Type* byteType = fixture.types.builtin(TypeKind::Byte);
    const ExpressionInfo left = unsignedConstant(byteType, 0xcc);
    const ExpressionInfo right = unsignedConstant(byteType, 0xaa);
    const ExpressionInfo andResult = evaluateBinary(fixture, TokenKind::Amp, left, right, byteType);
    const ExpressionInfo orResult = evaluateBinary(fixture, TokenKind::Pipe, left, right, byteType);
    const ExpressionInfo xorResult = evaluateBinary(fixture, TokenKind::Caret, left, right, byteType);
    context.expect(andResult.constantValue.unsignedInteger() == 0x88 &&
            orResult.constantValue.unsignedInteger() == 0xee &&
            xorResult.constantValue.unsignedInteger() == 0x66,
        "TS-SEMA-296 bitwise and/or/xor must preserve exact low-width bit patterns");
}

void testShiftBoundaries(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    const ExpressionInfo value = signedConstant(integer, 1);
    const ExpressionInfo byZero = evaluateBinary(
        fixture, TokenKind::LessLess, value, signedConstant(integer, 0), integer);
    const ExpressionInfo byLast = evaluateBinary(
        fixture, TokenKind::LessLess, value, signedConstant(integer, 31), integer);
    const ExpressionInfo byWidth = evaluateBinary(
        fixture, TokenKind::LessLess, value, signedConstant(integer, 32), integer);
    const ExpressionInfo negative = evaluateBinary(
        fixture, TokenKind::GreaterGreater, value, signedConstant(integer, -1), integer);
    context.expect(byZero.isConstant && byZero.constantValue.signedInteger() == 1 && byLast.isConstant &&
            byLast.constantValue.signedInteger() == std::numeric_limits<std::int32_t>::min() &&
            !byWidth.isConstant && !negative.isConstant && fixture.diagnostics.errorCount() == 2 &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-297 shifts 0/width-1 must evaluate and width/negative counts must trap exactly");
}

void testTrapSuppressionScopes(TestContext& context)
{
    SemaFixture fixture;
    AstNode* firstDivision = fixture.binary(TokenKind::Slash, fixture.integer(1), fixture.integer(0));
    AstNode* comparison = fixture.binary(TokenKind::EqualEqual, firstDivision, fixture.integer(0));
    fixture.analyzer.analyzeBinary(fixture.binary(TokenKind::AmpAmp, fixture.boolean(false), comparison));

    AstNode* secondDivision = fixture.binary(TokenKind::Slash, fixture.integer(1), fixture.integer(0));
    AstNode* size = fixture.node(AstNodeKind::SizeofExpression);
    size->setFirst(secondDivision);
    fixture.analyzer.analyzeSizeof(size, false);

    AstNode* thirdDivision = fixture.binary(TokenKind::Slash, fixture.integer(1), fixture.integer(0));
    AstNode* align = fixture.node(AstNodeKind::AlignofExpression);
    align->setFirst(thirdDivision);
    fixture.analyzer.analyzeAlignof(align, false);

    AstNode* ordinaryDivision = fixture.binary(TokenKind::Slash, fixture.integer(1), fixture.integer(0));
    fixture.analyzer.analyzeBinary(ordinaryDivision);
    context.expect(fixture.diagnostics.errorCount() == 1 &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-298 trap diagnostics must be suppressed only in short-circuit/sizeof/alignof scopes");
}
}

void registerSemaConstantEvaluation281To298Module(TestRegistry& registry)
{
    addSemaCase(registry, "TS-SEMA-281", "CS-SEMA-027", "unit", testMaskForWidthObservability,
        "PR", TestDisposition::KnownProductDefect, {"infrastructure-special", "anonymous-helper"});
    addSemaCase(registry, "TS-SEMA-282", "CS-SEMA-027", "unit", testSignedBitsRoundTrip);
    addSemaCase(registry, "TS-SEMA-283", "CS-SEMA-027", "unit", testConstantAsSigned);
    addSemaCase(registry, "TS-SEMA-284", "CS-SEMA-027", "unit", testConstantAsUnsigned);
    addSemaCase(registry, "TS-SEMA-285", "CS-SEMA-027", "unit", testConvertIntegerWidths);
    addSemaCase(registry, "TS-SEMA-286", "CS-SEMA-027", "unit", testConvertOtherConstantKinds);
    addSemaCase(registry, "TS-SEMA-287", "CS-SEMA-027", "unit", testUnaryLogicalNot);
    addSemaCase(registry, "TS-SEMA-288", "CS-SEMA-027", "unit", testUnaryMinusBoundaries);
    addSemaCase(registry, "TS-SEMA-289", "CS-SEMA-027", "unit", testUnaryBitwiseAndPlus);
    addSemaCase(registry, "TS-SEMA-290", "CS-SEMA-027", "unit", testBooleanTruthTableAndShortCircuit);
    addSemaCase(registry, "TS-SEMA-291", "CS-SEMA-027", "unit", testSignedComparisons);
    addSemaCase(registry, "TS-SEMA-292", "CS-SEMA-027", "unit", testUnsignedComparisons);
    addSemaCase(registry, "TS-SEMA-293", "CS-SEMA-027", "unit", testWrappingArithmetic);
    addSemaCase(registry, "TS-SEMA-294", "CS-SEMA-027", "unit", testDivisionAndRemainder);
    addSemaCase(registry, "TS-SEMA-295", "CS-SEMA-027", "unit", testDivisionTraps);
    addSemaCase(registry, "TS-SEMA-296", "CS-SEMA-027", "unit", testBitwiseOperations);
    addSemaCase(registry, "TS-SEMA-297", "CS-SEMA-027", "unit", testShiftBoundaries);
    addSemaCase(registry, "TS-SEMA-298", "CS-SEMA-027", "unit", testTrapSuppressionScopes);
}
}
