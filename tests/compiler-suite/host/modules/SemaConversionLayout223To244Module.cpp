#include "SemaConversionEvaluationTestSupport.h"

#include <cmath>

namespace nsc::suite
{
namespace
{
using namespace sema_conversion_evaluation_support;

void testExplicitIdentity(TestContext& context)
{
    SemaFixture fixture;
    Type* type = fixture.types.builtin(TypeKind::Int);
    AstNode* owner = fixture.node(AstNodeKind::CastExpression);
    AstNode* operand = fixture.integer(1);
    fixture.analyzer.recordExplicitConversion(owner, operand, type, type);
    const auto conversions = fixture.analyzer.model().conversions(owner);
    context.expect(fixture.analyzer.classifyExplicitConversion(type, type) == ConversionKind::Identity &&
            conversions.size() == 1 && conversions[0].kind == ConversionKind::Identity,
        "TS-SEMA-223 explicit identity must remain classified and recorded by the explicit helper");
}

void testExplicitNumericMatrix(TestContext& context)
{
    SemaFixture fixture;
    Type* intType = fixture.types.builtin(TypeKind::Int);
    Type* uintType = fixture.types.builtin(TypeKind::UInt);
    Type* floatType = fixture.types.builtin(TypeKind::Float);
    Type* doubleType = fixture.types.builtin(TypeKind::Double);
    const ConstantValue toFloat = fixture.analyzer.convertConstant(ConstantValue::signedInteger(-7), intType, floatType);
    const ConstantValue toInt = fixture.analyzer.convertConstant(ConstantValue::floating(8.75), doubleType, intType);
    context.expect(fixture.analyzer.canExplicitlyConvert(intType, doubleType) &&
            fixture.analyzer.canExplicitlyConvert(floatType, uintType) &&
            fixture.analyzer.classifyExplicitConversion(intType, doubleType) == ConversionKind::ExplicitNumeric &&
            toFloat.kind() == ConstantKind::Floating && toFloat.floating() == -7.0 &&
            toInt.kind() == ConstantKind::SignedInteger && toInt.signedInteger() == 8,
        "TS-SEMA-224 integral/floating explicit conversions must retain exact kind and constant payload");
}

void testExplicitEnumMatrix(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* first = fixture.types.createEnum("First", nullptr);
    Type* second = fixture.types.createEnum("Second", nullptr);
    context.expect(fixture.analyzer.canExplicitlyConvert(first, integer) &&
            fixture.analyzer.canExplicitlyConvert(integer, first) &&
            fixture.analyzer.classifyExplicitConversion(first, integer) == ConversionKind::ExplicitEnum &&
            fixture.analyzer.classifyExplicitConversion(integer, first) == ConversionKind::ExplicitEnum &&
            fixture.analyzer.canExplicitlyConvert(first, first) &&
            !fixture.analyzer.canExplicitlyConvert(first, second),
        "TS-SEMA-225 enum/numeric conversion is explicit while distinct enum identities remain incompatible");
}

void testExplicitPointer(TestContext& context)
{
    SemaFixture fixture;
    Type* left = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int));
    Type* right = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Byte));
    context.expect(fixture.analyzer.canExplicitlyConvert(left, right) &&
            fixture.analyzer.classifyExplicitConversion(left, right) == ConversionKind::ExplicitPointer,
        "TS-SEMA-226 pointer-to-pointer conversion must classify as ExplicitPointer");
}

void testExplicitPointerInteger(TestContext& context)
{
    SemaFixture fixture;
    Type* pointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int));
    Type* nintType = fixture.types.builtin(TypeKind::NInt);
    Type* nuintType = fixture.types.builtin(TypeKind::NUInt);
    context.expect(fixture.analyzer.canExplicitlyConvert(pointer, nintType) &&
            fixture.analyzer.canExplicitlyConvert(nuintType, pointer) &&
            fixture.analyzer.classifyExplicitConversion(pointer, nintType) == ConversionKind::ExplicitPointerInteger &&
            fixture.analyzer.classifyExplicitConversion(nuintType, pointer) == ConversionKind::ExplicitPointerInteger,
        "TS-SEMA-227 pointer/native-integer conversions must classify as ExplicitPointerInteger in both directions");
}

void testNullPointerAndToVoid(TestContext& context)
{
    SemaFixture fixture;
    Type* nullType = fixture.types.builtin(TypeKind::NullLiteral);
    Type* pointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int));
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* voidType = fixture.types.builtin(TypeKind::Void);
    context.expect(fixture.analyzer.canExplicitlyConvert(nullType, pointer) &&
            fixture.analyzer.classifyExplicitConversion(nullType, pointer) == ConversionKind::ExplicitNullPointer &&
            fixture.analyzer.canExplicitlyConvert(integer, voidType) &&
            fixture.analyzer.classifyExplicitConversion(integer, voidType) == ConversionKind::ToVoid,
        "TS-SEMA-228 null-to-pointer and any-to-void must retain their exact explicit conversion kinds");
}

void testBoolConversionExclusions(TestContext& context)
{
    SemaFixture fixture;
    Type* boolean = fixture.types.builtin(TypeKind::Bool);
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* pointer = fixture.types.pointerTo(integer);
    Type* voidType = fixture.types.builtin(TypeKind::Void);
    context.expect(fixture.analyzer.canExplicitlyConvert(boolean, boolean) &&
            !fixture.analyzer.canExplicitlyConvert(boolean, integer) &&
            !fixture.analyzer.canExplicitlyConvert(integer, boolean) &&
            !fixture.analyzer.canExplicitlyConvert(boolean, pointer) &&
            fixture.analyzer.canExplicitlyConvert(boolean, voidType),
        "TS-SEMA-229 bool must support identity/to-void only and remain excluded from numeric/pointer casts");
}

void testFloatingIntegerRange(TestContext& context)
{
    SemaFixture inside;
    AstNode* insideCast = inside.node(AstNodeKind::CastExpression);
    insideCast->setTypeSyntax(inside.typeSyntax(TokenKind::KwInt, "int"));
    insideCast->setFirst(inside.floating(42.75));
    const ExpressionInfo insideResult = inside.analyzer.analyzeCast(insideCast);

    SemaFixture outside;
    AstNode* outsideCast = outside.node(AstNodeKind::CastExpression);
    outsideCast->setTypeSyntax(outside.typeSyntax(TokenKind::KwInt, "int"));
    outsideCast->setFirst(outside.floating(std::numeric_limits<double>::infinity(), "inf"));
    const ExpressionInfo outsideResult = outside.analyzer.analyzeCast(outsideCast);
    context.expect(insideResult.isConstant && insideResult.constantValue.signedInteger() == 42 &&
            inside.diagnostics.diagnostics().empty() && outsideResult.type->kind() == TypeKind::Error &&
            hasDiagnostic(outside.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-230 in-range float constants convert exactly and non-finite/out-of-range constants trap");
}

void testRecordHelperOmissions(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* error = fixture.types.builtin(TypeKind::Error);
    AstNode* owner = fixture.node(AstNodeKind::BinaryExpression);
    AstNode* operand = fixture.integer(1);
    fixture.analyzer.recordImplicitConversion(owner, operand, signedConstant(integer, 1), integer);
    fixture.analyzer.recordImplicitConversion(owner, operand, signedConstant(error, 1), integer);
    fixture.analyzer.recordImplicitConversion(owner, operand, signedConstant(integer, 1), error);
    context.expect(fixture.analyzer.model().conversions(owner).empty(),
        "TS-SEMA-231 implicit record helper must omit identity and error source/destination conversions");
}

void testConversionRoles(TestContext& context)
{
    SemaFixture fixture;
    Type* byteType = fixture.types.builtin(TypeKind::Byte);
    Type* intType = fixture.types.builtin(TypeKind::Int);
    AstNode* owner = fixture.node(AstNodeKind::BinaryExpression);
    AstNode* first = fixture.integer(1);
    AstNode* second = fixture.integer(2);
    fixture.analyzer.recordImplicitConversion(owner, first, unsignedConstant(byteType, 1), intType);
    fixture.analyzer.recordIntegralPromotion(owner, second, byteType, intType, 1);
    fixture.analyzer.model_.addConversion(owner,
        ConversionInfo{ConversionKind::ImplicitNumeric, ConversionRole::Result, owner, intType, intType, -1});
    const auto conversions = fixture.analyzer.model().conversions(owner);
    context.expect(conversions.size() == 3 && conversions[0].role == ConversionRole::Contextual &&
            conversions[0].operandIndex == -1 && conversions[1].role == ConversionRole::Operand &&
            conversions[1].operandIndex == 1 && conversions[2].role == ConversionRole::Result &&
            conversions[2].operandIndex == -1,
        "TS-SEMA-232 contextual, operand and result conversion records must preserve roles and indices");
}

void testSizeofSupportedKinds(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (const auto [kind, layoutKind] : std::array{
             std::pair{TypeKind::Bool, BuiltinLayoutKind::Bool},
             std::pair{TypeKind::Byte, BuiltinLayoutKind::Byte},
             std::pair{TypeKind::Char, BuiltinLayoutKind::Char},
             std::pair{TypeKind::Short, BuiltinLayoutKind::Short},
             std::pair{TypeKind::UShort, BuiltinLayoutKind::UShort},
             std::pair{TypeKind::Int, BuiltinLayoutKind::Int},
             std::pair{TypeKind::UInt, BuiltinLayoutKind::UInt},
             std::pair{TypeKind::Long, BuiltinLayoutKind::Long},
             std::pair{TypeKind::ULong, BuiltinLayoutKind::ULong},
             std::pair{TypeKind::NInt, BuiltinLayoutKind::NInt},
             std::pair{TypeKind::NUInt, BuiltinLayoutKind::NUInt},
             std::pair{TypeKind::Float, BuiltinLayoutKind::Float},
             std::pair{TypeKind::Double, BuiltinLayoutKind::Double}})
    {
        const auto actual = fixture.analyzer.sizeOf(fixture.types.builtin(kind));
        exact = exact && actual == std::optional<std::uint64_t>(fixture.layout.layoutOf(layoutKind).size);
    }
    Type* pointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int));
    Type* enumeration = fixture.types.createEnum("E", nullptr);
    exact = exact && fixture.analyzer.sizeOf(pointer) == fixture.layout.layoutOf(BuiltinLayoutKind::Pointer).size;
    exact = exact && fixture.analyzer.sizeOf(enumeration) == fixture.layout.layoutOf(BuiltinLayoutKind::Enum).size;
    context.expect(exact, "TS-SEMA-233 sizeof must use exact target sizes for every supported scalar/pointer/enum kind");
}

void testAlignofSupportedKinds(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (const auto [kind, layoutKind] : std::array{
             std::pair{TypeKind::Bool, BuiltinLayoutKind::Bool},
             std::pair{TypeKind::Byte, BuiltinLayoutKind::Byte},
             std::pair{TypeKind::Short, BuiltinLayoutKind::Short},
             std::pair{TypeKind::Int, BuiltinLayoutKind::Int},
             std::pair{TypeKind::Long, BuiltinLayoutKind::Long},
             std::pair{TypeKind::Float, BuiltinLayoutKind::Float},
             std::pair{TypeKind::Double, BuiltinLayoutKind::Double}})
    {
        const auto actual = fixture.analyzer.layoutOfType(fixture.types.builtin(kind));
        exact = exact && actual.has_value() && actual->alignment == fixture.layout.layoutOf(layoutKind).alignment;
    }
    context.expect(exact, "TS-SEMA-234 alignof must use exact target alignments for every supported kind");
}

void testExpressionLayoutSuppressesTraps(TestContext& context)
{
    SemaFixture fixture;
    AstNode* division = fixture.binary(TokenKind::Slash, fixture.integer(1), fixture.integer(0));
    AstNode* size = fixture.node(AstNodeKind::SizeofExpression);
    size->setFirst(division);
    const ExpressionInfo result = fixture.analyzer.analyzeSizeof(size, false);
    context.expect(result.isConstant && result.constantValue.unsignedInteger() ==
            fixture.layout.layoutOf(BuiltinLayoutKind::Int).size &&
            !hasDiagnostic(fixture.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-235 sizeof(expression) must analyze its operand while suppressing constant traps");
}

void testUnsizedKinds(TestContext& context)
{
    SemaFixture fixture;
    Type* function = fixture.types.functionType(fixture.types.builtin(TypeKind::Void), {});
    const bool absent = !fixture.analyzer.layoutOfType(fixture.types.builtin(TypeKind::Void)).has_value() &&
        !fixture.analyzer.layoutOfType(function).has_value() &&
        !fixture.analyzer.layoutOfType(fixture.types.builtin(TypeKind::NullLiteral)).has_value() &&
        !fixture.analyzer.layoutOfType(fixture.types.builtin(TypeKind::Error)).has_value();
    AstNode* expression = fixture.node(AstNodeKind::SizeofTypeExpression);
    expression->setTypeSyntax(fixture.typeSyntax(TokenKind::KwVoid, "void"));
    const ExpressionInfo result = fixture.analyzer.analyzeSizeof(expression, true);
    context.expect(absent && result.type->kind() == TypeKind::Error &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidSizeof),
        "TS-SEMA-236 void/function/null/error layouts must be absent and sizeof must diagnose InvalidSizeof");
}

void testArrayLayouts(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    const auto zero = fixture.analyzer.layoutOfType(fixture.types.arrayOf(integer, 0));
    const auto one = fixture.analyzer.layoutOfType(fixture.types.arrayOf(integer, 1));
    const auto many = fixture.analyzer.layoutOfType(fixture.types.arrayOf(integer, 7));
    const auto overflow = fixture.analyzer.layoutOfType(
        fixture.types.arrayOf(integer, std::numeric_limits<std::uint64_t>::max()));
    context.expect(zero.has_value() && zero->size == 0 && one.has_value() && one->size == 4 &&
            many.has_value() && many->size == 28 && !overflow.has_value(),
        "TS-SEMA-237 array count 0/1/N must retain exact layout and overflowing counts must return nullopt");
}

void testStructPadding(TestContext& context)
{
    SemaFixture fixture;
    Type* structure = fixture.types.createStruct("Padded", nullptr);
    fixture.analyzer.structFieldTypes_[structure] = {
        fixture.types.builtin(TypeKind::Byte), fixture.types.builtin(TypeKind::Int),
        fixture.types.builtin(TypeKind::Short)};
    const auto layout = fixture.analyzer.layoutOfType(structure);
    context.expect(layout.has_value() && layout->size == 12 && layout->alignment == 4,
        "TS-SEMA-238 struct layout must include Win64 field padding and final aggregate alignment");
}

void testIncompleteStructLayout(TestContext& context)
{
    SemaFixture fixture;
    Type* structure = fixture.types.createStruct("Forward", nullptr);
    structure->setComplete(false);
    context.expect(!fixture.analyzer.layoutOfType(structure).has_value(),
        "TS-SEMA-239 incomplete structs must not expose a physical layout");
}

void testOffsetofValidFields(TestContext& context)
{
    SemaFixture fixture;
    fixture.initializeGlobalScope();
    Type* structure = fixture.types.createStruct("Pair", nullptr);
    Symbol typeSymbol(SymbolKind::Struct, "Pair", structure, nullptr);
    Symbol first(SymbolKind::Field, "A", fixture.types.builtin(TypeKind::Byte), nullptr);
    Symbol second(SymbolKind::Field, "B", fixture.types.builtin(TypeKind::Int), nullptr);
    first.setFieldOffset(0);
    second.setFieldOffset(4);
    typeSymbol.addMember(&first);
    typeSymbol.addMember(&second);
    fixture.analyzer.globalScope_->declare(&typeSymbol);
    auto evaluate = [&](std::string name) {
        AstNode* node = fixture.node(AstNodeKind::OffsetofExpression);
        node->setTypeSyntax(fixture.typeSyntax(TokenKind::Identifier, "Pair"));
        node->setText(std::move(name));
        return fixture.analyzer.analyzeOffsetof(node);
    };
    const ExpressionInfo firstResult = evaluate("A");
    const ExpressionInfo secondResult = evaluate("B");
    context.expect(firstResult.isConstant && firstResult.constantValue.unsignedInteger() == 0 &&
            secondResult.isConstant && secondResult.constantValue.unsignedInteger() == 4 &&
            firstResult.type->kind() == TypeKind::NUInt,
        "TS-SEMA-240 offsetof must produce exact nuint constants for zero and nonzero resolved field offsets");
}

void testOffsetofInvalidType(TestContext& context)
{
    SemaFixture nonStruct;
    nonStruct.initializeGlobalScope();
    AstNode* integer = nonStruct.node(AstNodeKind::OffsetofExpression);
    integer->setTypeSyntax(nonStruct.typeSyntax(TokenKind::KwInt, "int"));
    integer->setText("Value");
    nonStruct.analyzer.analyzeOffsetof(integer);

    SemaFixture incomplete;
    incomplete.initializeGlobalScope();
    Type* structure = incomplete.types.createStruct("Forward", nullptr);
    structure->setComplete(false);
    Symbol symbol(SymbolKind::Struct, "Forward", structure, nullptr);
    incomplete.analyzer.globalScope_->declare(&symbol);
    AstNode* forward = incomplete.node(AstNodeKind::OffsetofExpression);
    forward->setTypeSyntax(incomplete.typeSyntax(TokenKind::Identifier, "Forward"));
    forward->setText("Value");
    incomplete.analyzer.analyzeOffsetof(forward);
    context.expect(hasDiagnostic(nonStruct.diagnostics, DiagnosticId::InvalidOffsetof) &&
            hasDiagnostic(incomplete.diagnostics, DiagnosticId::InvalidOffsetof),
        "TS-SEMA-241 offsetof must reject non-struct and incomplete struct types with InvalidOffsetof");
}

void testOffsetofMissingOrUnresolvedField(TestContext& context)
{
    SemaFixture fixture;
    fixture.initializeGlobalScope();
    Type* structure = fixture.types.createStruct("Record", nullptr);
    Symbol typeSymbol(SymbolKind::Struct, "Record", structure, nullptr);
    Symbol unresolved(SymbolKind::Field, "Pending", fixture.types.builtin(TypeKind::Int), nullptr);
    typeSymbol.addMember(&unresolved);
    fixture.analyzer.globalScope_->declare(&typeSymbol);
    auto evaluate = [&](std::string name) {
        fixture.diagnostics.clear();
        AstNode* node = fixture.node(AstNodeKind::OffsetofExpression);
        node->setTypeSyntax(fixture.typeSyntax(TokenKind::Identifier, "Record"));
        node->setText(std::move(name));
        fixture.analyzer.analyzeOffsetof(node);
        return hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidOffsetof);
    };
    context.expect(evaluate("Missing") && evaluate("Pending"),
        "TS-SEMA-242 offsetof must reject both unknown fields and fields without materialized offsets");
}

void testTargetLayoutDelegation(TestContext& context)
{
    SemaFixture fixture;
    Type* pointer = fixture.types.pointerTo(fixture.types.builtin(TypeKind::Byte));
    const auto scalar = fixture.analyzer.layoutOfType(fixture.types.builtin(TypeKind::Double));
    const auto pointerLayout = fixture.analyzer.layoutOfType(pointer);
    context.expect(scalar.has_value() && pointerLayout.has_value() &&
            scalar->size == fixture.layout.layoutOf(BuiltinLayoutKind::Double).size &&
            scalar->alignment == fixture.layout.layoutOf(BuiltinLayoutKind::Double).alignment &&
            pointerLayout->size == fixture.layout.layoutOf(BuiltinLayoutKind::Pointer).size &&
            pointerLayout->alignment == fixture.layout.layoutOf(BuiltinLayoutKind::Pointer).alignment,
        "TS-SEMA-243 scalar and pointer layout queries must delegate exactly to TargetDataLayout");
}

void testNestedLayoutOverflow(TestContext& context)
{
    SemaFixture fixture;
    Type* overflowing = fixture.types.arrayOf(
        fixture.types.builtin(TypeKind::Double), std::numeric_limits<std::uint64_t>::max());
    Type* nested = fixture.types.createStruct("NestedOverflow", nullptr);
    fixture.analyzer.structFieldTypes_[nested] = {fixture.types.builtin(TypeKind::Byte), overflowing};
    context.expect(!fixture.analyzer.layoutOfType(overflowing).has_value() &&
            !fixture.analyzer.layoutOfType(nested).has_value(),
        "TS-SEMA-244 nested array/struct layout overflow must return nullopt without wrapping");
}
}

void registerSemaConversionLayout223To244Module(TestRegistry& registry)
{
    addSemaCase(registry, "TS-SEMA-223", "CS-SEMA-022", "component", testExplicitIdentity);
    addSemaCase(registry, "TS-SEMA-224", "CS-SEMA-022", "component", testExplicitNumericMatrix);
    addSemaCase(registry, "TS-SEMA-225", "CS-SEMA-022", "component", testExplicitEnumMatrix);
    addSemaCase(registry, "TS-SEMA-226", "CS-SEMA-022", "component", testExplicitPointer);
    addSemaCase(registry, "TS-SEMA-227", "CS-SEMA-022", "component", testExplicitPointerInteger);
    addSemaCase(registry, "TS-SEMA-228", "CS-SEMA-022", "component", testNullPointerAndToVoid);
    addSemaCase(registry, "TS-SEMA-229", "CS-SEMA-022", "component", testBoolConversionExclusions);
    addSemaCase(registry, "TS-SEMA-230", "CS-SEMA-022", "component", testFloatingIntegerRange);
    addSemaCase(registry, "TS-SEMA-231", "CS-SEMA-022", "component", testRecordHelperOmissions);
    addSemaCase(registry, "TS-SEMA-232", "CS-SEMA-022", "component", testConversionRoles);
    addSemaCase(registry, "TS-SEMA-233", "CS-SEMA-023", "component", testSizeofSupportedKinds);
    addSemaCase(registry, "TS-SEMA-234", "CS-SEMA-023", "component", testAlignofSupportedKinds);
    addSemaCase(registry, "TS-SEMA-235", "CS-SEMA-023", "component", testExpressionLayoutSuppressesTraps);
    addSemaCase(registry, "TS-SEMA-236", "CS-SEMA-023", "component", testUnsizedKinds);
    addSemaCase(registry, "TS-SEMA-237", "CS-SEMA-023", "component", testArrayLayouts);
    addSemaCase(registry, "TS-SEMA-238", "CS-SEMA-023", "component", testStructPadding);
    addSemaCase(registry, "TS-SEMA-239", "CS-SEMA-023", "component", testIncompleteStructLayout);
    addSemaCase(registry, "TS-SEMA-240", "CS-SEMA-023", "component", testOffsetofValidFields);
    addSemaCase(registry, "TS-SEMA-241", "CS-SEMA-023", "component", testOffsetofInvalidType);
    addSemaCase(registry, "TS-SEMA-242", "CS-SEMA-023", "component", testOffsetofMissingOrUnresolvedField);
    addSemaCase(registry, "TS-SEMA-243", "CS-SEMA-023", "component", testTargetLayoutDelegation);
    addSemaCase(registry, "TS-SEMA-244", "CS-SEMA-023", "component", testNestedLayoutOverflow);
}
}
