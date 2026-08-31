#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "../support/TestContext.h"

#include <optional>
#include <string>

int main()
{
    using nativesharp::frontend::ProjectConversionKind;
    using nativesharp::frontend::ProjectIntegerConstant;
    using nativesharp::frontend::ProjectTypeConversion;
    using nativesharp::tests::TestContext;

    TestContext tests;

    tests.expect(
        ProjectTypeConversion::classifyImplicit("builtin:int", "builtin:int").kind ==
            ProjectConversionKind::Identity,
        "identity conversion is classified explicitly");
    tests.expect(
        ProjectTypeConversion::classifyImplicit("builtin:char", "builtin:int").kind ==
            ProjectConversionKind::IntegralPromotion,
        "char to int is an integral promotion");
    tests.expect(
        ProjectTypeConversion::classifyImplicit("builtin:int", "builtin:long").kind ==
            ProjectConversionKind::Numeric,
        "int to long is a standard numeric conversion");
    tests.expect(
        !ProjectTypeConversion::classifyImplicit("builtin:double", "builtin:float").exists(),
        "double does not implicitly narrow to float");
    tests.expect(
        ProjectTypeConversion::classifyImplicit(
            "builtin:int",
            "builtin:byte",
            ProjectIntegerConstant{1, false}).kind == ProjectConversionKind::ConstantInteger,
        "representable integer literal may convert to byte");
    tests.expect(
        !ProjectTypeConversion::classifyImplicit(
            "builtin:int",
            "builtin:byte",
            ProjectIntegerConstant{300, false}).exists(),
        "out-of-range integer literal cannot convert to byte");
    tests.expect(
        ProjectTypeConversion::classifyImplicit("builtin:null", "builtin:int*").kind ==
            ProjectConversionKind::NullPointer,
        "null converts to pointer type");
    tests.expect(
        ProjectTypeConversion::classifyImplicit(
            "builtin:null", "type:Core::Game.Payload", std::nullopt, true).kind ==
            ProjectConversionKind::NullReference,
        "null converts to a named type only when semantic context marks it as a class reference");
    tests.expect(
        !ProjectTypeConversion::classifyImplicit(
            "builtin:null", "type:Core::Game.Payload", std::nullopt, false).exists(),
        "null is not blindly accepted for every named type key such as structs or enums");
    tests.expect(
        ProjectTypeConversion::classifyImplicit("builtin:int*", "builtin:void*").kind ==
            ProjectConversionKind::PointerToVoid,
        "typed pointer converts to void pointer");
    tests.expect(
        ProjectTypeConversion::classifyImplicit("string-literal", "builtin:char*").kind ==
            ProjectConversionKind::StringLiteralToCharPointer,
        "string literal converts to char pointer");
    tests.expect(
        ProjectTypeConversion::classifyImplicit(
            "type:Core::Game.Payload",
            "type:Core::Game.Payload").kind == ProjectConversionKind::Identity,
        "named type identity is an identity conversion");
    tests.expect(
        !ProjectTypeConversion::classifyImplicit(
            "type:Core::Game.Payload",
            "type:Core::Game.Other").exists(),
        "unrelated named types do not receive invented implicit conversions");
    tests.expect(
        ProjectTypeConversion::classifyImplicit("builtin:char", "builtin:int").rank <
            ProjectTypeConversion::classifyImplicit("builtin:char", "builtin:long").rank,
        "integral promotion ranks above generic numeric conversion");
    tests.expect(
        ProjectTypeConversion::classifyImplicit(
            "builtin:int",
            "builtin:byte",
            ProjectIntegerConstant{1, false}).rank <
            ProjectTypeConversion::classifyImplicit("builtin:int", "builtin:long").rank,
        "constant integer conversion ranks above generic numeric widening");
    const std::optional<std::string> commonIntLong =
        ProjectTypeConversion::commonNumericType("builtin:int", "builtin:long");
    tests.expect(
        commonIntLong.has_value() && *commonIntLong == "builtin:long",
        "common numeric type of int and long is long");
    const std::optional<std::string> commonFloatDouble =
        ProjectTypeConversion::commonNumericType("builtin:float", "builtin:double");
    tests.expect(
        commonFloatDouble.has_value() && *commonFloatDouble == "builtin:double",
        "common numeric type of float and double is double");
    tests.expect(
        !ProjectTypeConversion::commonNumericType("builtin:bool", "builtin:int").has_value(),
        "bool does not participate in numeric common-type selection");

    // CC1-101 "regresiones de rango de char". `builtin:char` es un byte en layout, ABI, LLVM y
    // parser; aqui aceptaba hasta 65535 porque compartia rama con `ushort`. Un `switch` sobre
    // `char` con `case 65:` y `case 321:` pasaba el frontend y le entregaba a LLVM dos casos
    // `i8 65` identicos -- comportamiento indefinido, justo debajo del chequeo de duplicados que
    // existe para impedirlo. Estas cuatro lineas son lo que impide que vuelva.
    tests.expect(
        ProjectTypeConversion::classifyImplicit(
            "builtin:int", "builtin:char", ProjectIntegerConstant{255, false}).exists(),
        "255 fits in char");
    tests.expect(
        !ProjectTypeConversion::classifyImplicit(
            "builtin:int", "builtin:char", ProjectIntegerConstant{256, false}).exists(),
        "256 does not fit in char -- char is one byte");
    tests.expect(
        !ProjectTypeConversion::classifyImplicit(
            "builtin:int", "builtin:char", ProjectIntegerConstant{321, false}).exists(),
        "321 does not fit in char -- this is the value that used to collide with 65 as i8");
    tests.expect(
        ProjectTypeConversion::classifyImplicit(
            "builtin:int", "builtin:ushort", ProjectIntegerConstant{65535, false}).exists(),
        "65535 still fits in ushort -- splitting char off did not narrow ushort");

    return tests.result();
}
