#include "SemaConversionEvaluationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace sema_conversion_evaluation_support;

constexpr std::array<TypeKind, 10> kIntegerKinds = {
    TypeKind::Byte, TypeKind::Char, TypeKind::Short, TypeKind::UShort, TypeKind::Int,
    TypeKind::UInt, TypeKind::Long, TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt};

constexpr std::array<TypeKind, 12> kNumericKinds = {
    TypeKind::Byte, TypeKind::Char, TypeKind::Short, TypeKind::UShort, TypeKind::Int, TypeKind::UInt,
    TypeKind::Long, TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt, TypeKind::Float, TypeKind::Double};

bool expectedStandardImplicit(TypeKind source, TypeKind destination)
{
    if (source == destination) return true;
    switch (source)
    {
        case TypeKind::Byte:
        case TypeKind::Char:
            return destination == TypeKind::Short || destination == TypeKind::UShort ||
                destination == TypeKind::Int || destination == TypeKind::UInt || destination == TypeKind::Long ||
                destination == TypeKind::ULong || destination == TypeKind::NInt || destination == TypeKind::NUInt ||
                destination == TypeKind::Float || destination == TypeKind::Double;
        case TypeKind::Short:
            return destination == TypeKind::Int || destination == TypeKind::Long ||
                destination == TypeKind::NInt || destination == TypeKind::Float || destination == TypeKind::Double;
        case TypeKind::UShort:
            return destination == TypeKind::Int || destination == TypeKind::UInt || destination == TypeKind::Long ||
                destination == TypeKind::ULong || destination == TypeKind::NInt || destination == TypeKind::NUInt ||
                destination == TypeKind::Float || destination == TypeKind::Double;
        case TypeKind::Int:
            return destination == TypeKind::Long || destination == TypeKind::NInt ||
                destination == TypeKind::Float || destination == TypeKind::Double;
        case TypeKind::UInt:
            return destination == TypeKind::Long || destination == TypeKind::ULong ||
                destination == TypeKind::NUInt || destination == TypeKind::Float || destination == TypeKind::Double;
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
            return destination == TypeKind::Float || destination == TypeKind::Double;
        case TypeKind::Float:
            return destination == TypeKind::Double;
        default:
            return false;
    }
}

TypeKind promoted(TypeKind kind)
{
    return kind == TypeKind::Byte || kind == TypeKind::Char || kind == TypeKind::Short || kind == TypeKind::UShort
        ? TypeKind::Int
        : kind;
}

std::optional<TypeKind> expectedCommonIntegral(TypeKind left, TypeKind right)
{
    left = promoted(left);
    right = promoted(right);
    if (left == right) return left;
    for (TypeKind candidate : {TypeKind::Int, TypeKind::UInt, TypeKind::Long,
             TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt})
    {
        if (expectedStandardImplicit(left, candidate) && expectedStandardImplicit(right, candidate)) return candidate;
    }
    return std::nullopt;
}

void testTypeClassificationMatrices(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (Type* type : allRepresentativeTypes(fixture))
    {
        const TypeKind kind = type->kind();
        const bool integer = std::find(kIntegerKinds.begin(), kIntegerKinds.end(), kind) != kIntegerKinds.end();
        const bool signedInteger = kind == TypeKind::Short || kind == TypeKind::Int ||
            kind == TypeKind::Long || kind == TypeKind::NInt;
        const bool floating = kind == TypeKind::Float || kind == TypeKind::Double;
        exact = exact && fixture.analyzer.isInteger(type) == integer;
        exact = exact && fixture.analyzer.isSignedInteger(type) == signedInteger;
        exact = exact && fixture.analyzer.isUnsignedInteger(type) == (integer && !signedInteger);
        exact = exact && fixture.analyzer.isFloating(type) == floating;
        exact = exact && fixture.analyzer.isNumeric(type) == (integer || floating);
    }
    context.expect(exact, "TS-SEMA-267 integer/signed/unsigned/floating/numeric classifiers must match all 22 kinds");
}

void testPointerIncrementSizedMatrices(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (Type* type : allRepresentativeTypes(fixture))
    {
        const bool pointer = type->kind() == TypeKind::Pointer;
        const bool numeric = std::find(kNumericKinds.begin(), kNumericKinds.end(), type->kind()) != kNumericKinds.end();
        const bool increment = numeric || (pointer && type->elementType()->kind() != TypeKind::Void &&
            type->elementType()->isComplete());
        exact = exact && fixture.analyzer.isPointer(type) == pointer;
        exact = exact && fixture.analyzer.isScalarIncrementType(type) == increment;
        exact = exact && fixture.analyzer.isSized(type) == fixture.analyzer.layoutOfType(type).has_value();
    }
    context.expect(exact, "TS-SEMA-268 pointer/scalar-increment/sized classifiers must match all 22 representative kinds");
}

void testIntegerWidths(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (Type* type : allRepresentativeTypes(fixture))
    {
        std::uint32_t expected = 0;
        if (type->kind() == TypeKind::Byte || type->kind() == TypeKind::Char) expected = 8;
        if (type->kind() == TypeKind::Short || type->kind() == TypeKind::UShort) expected = 16;
        if (type->kind() == TypeKind::Int || type->kind() == TypeKind::UInt) expected = 32;
        if (type->kind() == TypeKind::Long || type->kind() == TypeKind::ULong ||
            type->kind() == TypeKind::NInt || type->kind() == TypeKind::NUInt) expected = 64;
        exact = exact && fixture.analyzer.integerWidth(type) == expected;
    }
    context.expect(exact, "TS-SEMA-269 integerWidth must return exact 8/16/32/64 widths and zero for non-integers");
}

void testIntegerPromotions(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (Type* type : allRepresentativeTypes(fixture))
    {
        Type* expected = promoted(type->kind()) == TypeKind::Int
            ? fixture.types.builtin(TypeKind::Int)
            : type;
        exact = exact && fixture.analyzer.promoteInteger(type) == expected;
    }
    context.expect(exact, "TS-SEMA-270 promoteInteger must map the four narrow kinds to Int and preserve every other kind");
}

void testCommonIntegralCartesianProduct(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (TypeKind left : kIntegerKinds)
    {
        for (TypeKind right : kIntegerKinds)
        {
            const std::optional<TypeKind> expected = expectedCommonIntegral(left, right);
            Type* actual = fixture.analyzer.commonIntegralType(
                fixture.types.builtin(left), fixture.types.builtin(right));
            exact = exact && ((expected.has_value() && actual == fixture.types.builtin(*expected)) ||
                (!expected.has_value() && actual == nullptr));
        }
    }
    context.expect(exact, "TS-SEMA-271 commonIntegralType must match the complete ordered integer Cartesian product");
}

void testCommonNumericCartesianProduct(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (TypeKind left : kNumericKinds)
    {
        for (TypeKind right : kNumericKinds)
        {
            std::optional<TypeKind> expected;
            if (left == TypeKind::Double || right == TypeKind::Double)
            {
                if (expectedStandardImplicit(left, TypeKind::Double) &&
                    expectedStandardImplicit(right, TypeKind::Double)) expected = TypeKind::Double;
            }
            else if (left == TypeKind::Float || right == TypeKind::Float)
            {
                if (expectedStandardImplicit(left, TypeKind::Float) &&
                    expectedStandardImplicit(right, TypeKind::Float)) expected = TypeKind::Float;
            }
            else
            {
                expected = expectedCommonIntegral(left, right);
            }
            Type* actual = fixture.analyzer.commonNumericType(
                fixture.types.builtin(left), fixture.types.builtin(right));
            exact = exact && ((expected.has_value() && actual == fixture.types.builtin(*expected)) ||
                (!expected.has_value() && actual == nullptr));
        }
    }
    context.expect(exact, "TS-SEMA-272 commonNumericType must match Double/Float/integral/Error across the full product");
}

void testConstantFitsBoundaries(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (TypeKind destinationKind : kIntegerKinds)
    {
        Type* destination = fixture.types.builtin(destinationKind);
        const std::uint32_t width = fixture.analyzer.integerWidth(destination);
        if (fixture.analyzer.isSignedInteger(destination))
        {
            const std::int64_t minimum = width == 64 ? std::numeric_limits<std::int64_t>::min()
                : -(std::int64_t{1} << (width - 1));
            const std::int64_t maximum = width == 64 ? std::numeric_limits<std::int64_t>::max()
                : (std::int64_t{1} << (width - 1)) - 1;
            exact = exact && fixture.analyzer.constantFits(ConstantValue::signedInteger(minimum), destination);
            exact = exact && fixture.analyzer.constantFits(ConstantValue::signedInteger(maximum), destination);
            if (width != 64)
            {
                exact = exact && !fixture.analyzer.constantFits(ConstantValue::signedInteger(minimum - 1), destination);
                exact = exact && !fixture.analyzer.constantFits(ConstantValue::signedInteger(maximum + 1), destination);
            }
        }
        else
        {
            const std::uint64_t maximum = width == 64 ? std::numeric_limits<std::uint64_t>::max()
                : (std::uint64_t{1} << width) - 1;
            exact = exact && fixture.analyzer.constantFits(ConstantValue::unsignedInteger(0), destination);
            exact = exact && fixture.analyzer.constantFits(ConstantValue::unsignedInteger(maximum), destination);
            exact = exact && !fixture.analyzer.constantFits(ConstantValue::signedInteger(-1), destination);
            if (width != 64)
            {
                exact = exact && !fixture.analyzer.constantFits(ConstantValue::unsignedInteger(maximum + 1), destination);
            }
        }
    }
    context.expect(exact, "TS-SEMA-273 constantFits must accept/reject signed and unsigned boundary values exactly");
}

void testImplicitIdentityAndPromotion(TestContext& context)
{
    SemaFixture fixture;
    Type* intType = fixture.types.builtin(TypeKind::Int);
    bool exact = fixture.analyzer.classifyImplicitConversion(signedConstant(intType, 1), intType) ==
        ConversionKind::Identity;
    for (TypeKind kind : {TypeKind::Byte, TypeKind::Char, TypeKind::Short, TypeKind::UShort})
    {
        Type* source = fixture.types.builtin(kind);
        exact = exact && fixture.analyzer.classifyImplicitConversion(unsignedConstant(source, 1), intType) ==
            ConversionKind::IntegralPromotion;
    }
    context.expect(exact, "TS-SEMA-274 implicit identity and all narrow-to-Int promotions must classify exactly");
}

void testImplicitConstantInteger(TestContext& context)
{
    SemaFixture fixture;
    Type* sourceType = fixture.types.builtin(TypeKind::Int);
    Type* byteType = fixture.types.builtin(TypeKind::Byte);
    ExpressionInfo fitting = signedConstant(sourceType, 255);
    ExpressionInfo overflow = signedConstant(sourceType, 256);
    ExpressionInfo floating = floatingConstant(fixture.types.builtin(TypeKind::Double), 1.0);
    context.expect(fixture.analyzer.canImplicitlyConvert(fitting, byteType) &&
            fixture.analyzer.classifyImplicitConversion(fitting, byteType) ==
                ConversionKind::ImplicitConstantInteger &&
            !fixture.analyzer.canImplicitlyConvert(overflow, byteType) &&
            !fixture.analyzer.canImplicitlyConvert(floating, byteType),
        "TS-SEMA-275 implicit constant-integer conversion must require fitting integer source and destination");
}

void testImplicitNumericMatrix(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (TypeKind source : kNumericKinds)
    {
        for (TypeKind destination : kNumericKinds)
        {
            ExpressionInfo info;
            info.type = fixture.types.builtin(source);
            const bool actual = fixture.analyzer.canImplicitlyConvert(info, fixture.types.builtin(destination));
            exact = exact && actual == expectedStandardImplicit(source, destination);
            if (actual && source != destination)
            {
                const ConversionKind kind = fixture.analyzer.classifyImplicitConversion(
                    info, fixture.types.builtin(destination));
                const bool promotion = destination == TypeKind::Int && promoted(source) == TypeKind::Int;
                exact = exact && kind == (promotion
                    ? ConversionKind::IntegralPromotion
                    : ConversionKind::ImplicitNumeric);
            }
        }
    }
    context.expect(exact, "TS-SEMA-276 implicit numeric acceptance and classification must match the full matrix");
}

void testSpecialImplicitConversions(TestContext& context)
{
    SemaFixture fixture;
    Type* intPointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int));
    Type* voidPointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Void));
    Type* charPointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Char));
    ExpressionInfo nullInfo;
    nullInfo.type = fixture.types.builtin(TypeKind::NullLiteral);
    ExpressionInfo pointerInfo;
    pointerInfo.type = intPointer;
    ExpressionInfo stringInfo;
    stringInfo.type = fixture.types.stringLiteral(4);
    context.expect(fixture.analyzer.canImplicitlyConvert(nullInfo, intPointer) &&
            fixture.analyzer.classifyImplicitConversion(nullInfo, intPointer) ==
                ConversionKind::ImplicitNullPointer &&
            fixture.analyzer.canImplicitlyConvert(pointerInfo, voidPointer) &&
            fixture.analyzer.classifyImplicitConversion(pointerInfo, voidPointer) ==
                ConversionKind::ImplicitPointerToVoid &&
            fixture.analyzer.canImplicitlyConvert(stringInfo, charPointer) &&
            fixture.analyzer.classifyImplicitConversion(stringInfo, charPointer) ==
                ConversionKind::ImplicitStringLiteralToCharPointer,
        "TS-SEMA-277 null/pointer-to-void/string-to-char-pointer implicit conversions must retain exact kinds");
}

void testForbiddenImplicitConversions(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    Type* intPointer = fixture.types.pointerTo(integer);
    Type* bytePointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Byte));
    ExpressionInfo intInfo;
    intInfo.type = integer;
    ExpressionInfo boolInfo;
    boolInfo.type = boolean;
    ExpressionInfo pointerInfo;
    pointerInfo.type = intPointer;
    context.expect(!fixture.analyzer.canImplicitlyConvert(pointerInfo, bytePointer) &&
            !fixture.analyzer.canImplicitlyConvert(pointerInfo, integer) &&
            !fixture.analyzer.canImplicitlyConvert(intInfo, intPointer) &&
            !fixture.analyzer.canImplicitlyConvert(boolInfo, integer) &&
            !fixture.analyzer.canImplicitlyConvert(intInfo, boolean),
        "TS-SEMA-278 unrelated pointers, pointer/numeric and bool/numeric implicit conversions must remain forbidden");
}

void testExplicitConversionFullMatrix(TestContext& context)
{
    SemaFixture fixture;
    const std::vector<Type*> types = allRepresentativeTypes(fixture);
    bool exact = true;
    for (Type* source : types)
    {
        for (Type* destination : types)
        {
            const bool same = source == destination;
            const bool toVoid = destination->kind() == TypeKind::Void;
            const bool boolExcluded = source->kind() == TypeKind::Bool || destination->kind() == TypeKind::Bool;
            const bool numeric = fixture.analyzer.isNumeric(source) && fixture.analyzer.isNumeric(destination);
            const bool enumNumeric = (source->kind() == TypeKind::Enum && fixture.analyzer.isInteger(destination)) ||
                (fixture.analyzer.isInteger(source) && destination->kind() == TypeKind::Enum);
            const bool pointerPair = fixture.analyzer.isPointer(source) && fixture.analyzer.isPointer(destination);
            const bool pointerInteger =
                (fixture.analyzer.isPointer(source) &&
                    (destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt)) ||
                (fixture.analyzer.isPointer(destination) &&
                    (source->kind() == TypeKind::NInt || source->kind() == TypeKind::NUInt));
            const bool nullPointer = source->kind() == TypeKind::NullLiteral && fixture.analyzer.isPointer(destination);
            const bool expected = same || toVoid || (!boolExcluded &&
                (numeric || enumNumeric || pointerPair || pointerInteger || nullPointer));
            exact = exact && fixture.analyzer.canExplicitlyConvert(source, destination) == expected;
        }
    }
    context.expect(exact, "TS-SEMA-279 explicit conversion acceptance must match the complete type-kind matrix");
}

void testErrorConversionCascade(TestContext& context)
{
    SemaFixture fixture;
    Type* error = fixture.types.builtin(TypeKind::Error);
    Type* integer = fixture.types.builtin(TypeKind::Int);
    ExpressionInfo errorInfo;
    errorInfo.type = error;
    ExpressionInfo intInfo;
    intInfo.type = integer;
    context.expect(fixture.analyzer.canImplicitlyConvert(errorInfo, integer) &&
            fixture.analyzer.canImplicitlyConvert(intInfo, error) &&
            fixture.analyzer.canImplicitlyConvert(errorInfo, error) &&
            fixture.analyzer.classifyImplicitConversion(errorInfo, error) == ConversionKind::Identity &&
            !fixture.analyzer.canExplicitlyConvert(error, integer),
        "TS-SEMA-280 Error must cascade through implicit checks, preserve identity, and not invent explicit casts");
}
}

void registerSemaNumericConversion267To280Module(TestRegistry& registry)
{
    addSemaCase(registry, "TS-SEMA-267", "CS-SEMA-026", "unit", testTypeClassificationMatrices);
    addSemaCase(registry, "TS-SEMA-268", "CS-SEMA-026", "unit", testPointerIncrementSizedMatrices);
    addSemaCase(registry, "TS-SEMA-269", "CS-SEMA-026", "unit", testIntegerWidths);
    addSemaCase(registry, "TS-SEMA-270", "CS-SEMA-026", "unit", testIntegerPromotions);
    addSemaCase(registry, "TS-SEMA-271", "CS-SEMA-026", "unit", testCommonIntegralCartesianProduct);
    addSemaCase(registry, "TS-SEMA-272", "CS-SEMA-026", "unit", testCommonNumericCartesianProduct);
    addSemaCase(registry, "TS-SEMA-273", "CS-SEMA-026", "unit", testConstantFitsBoundaries);
    addSemaCase(registry, "TS-SEMA-274", "CS-SEMA-026", "unit", testImplicitIdentityAndPromotion);
    addSemaCase(registry, "TS-SEMA-275", "CS-SEMA-026", "unit", testImplicitConstantInteger);
    addSemaCase(registry, "TS-SEMA-276", "CS-SEMA-026", "unit", testImplicitNumericMatrix);
    addSemaCase(registry, "TS-SEMA-277", "CS-SEMA-026", "unit", testSpecialImplicitConversions);
    addSemaCase(registry, "TS-SEMA-278", "CS-SEMA-026", "unit", testForbiddenImplicitConversions);
    addSemaCase(registry, "TS-SEMA-279", "CS-SEMA-026", "unit", testExplicitConversionFullMatrix);
    addSemaCase(registry, "TS-SEMA-280", "CS-SEMA-026", "unit", testErrorConversionCascade);
}
}
