#include "SemaConversionEvaluationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace sema_conversion_evaluation_support;

TypeModifierSyntax arrayModifier(AstNode* count)
{
    TypeModifierSyntax modifier;
    modifier.kind = TypeModifierSyntaxKind::Array;
    modifier.arrayCount = count;
    return modifier;
}

TypeModifierSyntax pointerModifier()
{
    TypeModifierSyntax modifier;
    modifier.kind = TypeModifierSyntaxKind::Pointer;
    return modifier;
}

ExpressionInfo analyzeNewArray(SemaFixture& fixture, AstNode* count, TokenKind keyword = TokenKind::KwInt)
{
    AstNode* node = fixture.node(AstNodeKind::NewExpression);
    node->setTypeSyntax(fixture.typeSyntax(keyword, std::string(nativesharp::lexer::tokenKindName(keyword))));
    node->setFlag(true);
    node->setFirst(count);
    return fixture.analyzer.analyzeNew(node);
}

ExpressionInfo analyzeStackArray(SemaFixture& fixture, AstNode* count, TokenKind keyword = TokenKind::KwInt)
{
    AstNode* node = fixture.node(AstNodeKind::StackallocExpression);
    node->setTypeSyntax(fixture.typeSyntax(keyword, std::string(nativesharp::lexer::tokenKindName(keyword))));
    node->setFirst(count);
    return fixture.analyzer.analyzeStackalloc(node);
}

void testNewObject(TestContext& context)
{
    SemaFixture fixture;
    AstNode* node = fixture.node(AstNodeKind::NewExpression);
    node->setTypeSyntax(fixture.typeSyntax(TokenKind::KwInt, "int"));
    const ExpressionInfo result = fixture.analyzer.analyzeNew(node);
    context.expect(result.type != nullptr && result.type->kind() == TypeKind::Pointer &&
            result.type->elementType() == fixture.types.builtin(TypeKind::Int) &&
            fixture.diagnostics.diagnostics().empty(),
        "TS-SEMA-245 new over a valid object type must return a pointer to that exact type");
}

void testNewInvalidObjectTypes(TestContext& context)
{
    SemaFixture fixture;
    AstNode* voidNode = fixture.node(AstNodeKind::NewExpression);
    voidNode->setTypeSyntax(fixture.typeSyntax(TokenKind::KwVoid, "void"));
    const ExpressionInfo result = fixture.analyzer.analyzeNew(voidNode);
    Type* function = fixture.types.functionType(fixture.types.builtin(TypeKind::Void), {});
    Type* incomplete = fixture.types.createStruct("Forward", nullptr);
    incomplete->setComplete(false);
    fixture.diagnostics.clear();
    const bool functionValid = fixture.analyzer.validateObjectType(function, SourceRange{}, false);
    const bool incompleteValid = fixture.analyzer.validateObjectType(incomplete, SourceRange{}, false);
    context.expect(result.type->kind() == TypeKind::Error &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::IncompleteType) &&
            !functionValid && !incompleteValid,
        "TS-SEMA-246 new/object validation must reject void, function and incomplete object types");
}

void testNewArrayCounts(TestContext& context)
{
    SemaFixture zero;
    SemaFixture one;
    SemaFixture many;
    const ExpressionInfo a = analyzeNewArray(zero, zero.integer(0));
    const ExpressionInfo b = analyzeNewArray(one, one.integer(1));
    const ExpressionInfo c = analyzeNewArray(many, many.integer(37));
    context.expect(a.type->kind() == TypeKind::Pointer && b.type->kind() == TypeKind::Pointer &&
            c.type->kind() == TypeKind::Pointer && zero.diagnostics.diagnostics().empty() &&
            one.diagnostics.diagnostics().empty() && many.diagnostics.diagnostics().empty(),
        "TS-SEMA-247 new[] must analyze integer counts 0, 1 and N while preserving the element pointer result");
}

void testNewArrayNonIntegerCount(TestContext& context)
{
    SemaFixture boolean;
    SemaFixture floating;
    const ExpressionInfo a = analyzeNewArray(boolean, boolean.boolean(true));
    const ExpressionInfo b = analyzeNewArray(floating, floating.floating(2.5));
    context.expect(a.type->kind() == TypeKind::Error && b.type->kind() == TypeKind::Error &&
            hasDiagnostic(boolean.diagnostics, DiagnosticId::InvalidAllocationType) &&
            hasDiagnostic(floating.diagnostics, DiagnosticId::InvalidAllocationType),
        "TS-SEMA-248 new[] must reject boolean and floating counts with InvalidAllocationType");
}

void testNewArrayTrapPoststate(TestContext& context)
{
    SemaFixture negative;
    AstNode* minusOne = negative.unary(TokenKind::Minus, negative.integer(1));
    const ExpressionInfo first = analyzeNewArray(negative, minusOne);
    SemaFixture overflow;
    const ExpressionInfo second = analyzeNewArray(
        overflow, overflow.integer(std::numeric_limits<std::uint64_t>::max(), "18446744073709551615UL"),
        TokenKind::KwDouble);
    context.expect(first.type->kind() == TypeKind::Pointer && second.type->kind() == TypeKind::Pointer &&
            hasDiagnostic(negative.diagnostics, DiagnosticId::ConstantEvaluationTrap) &&
            hasDiagnostic(overflow.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-249 negative and byte-size-overflow new[] counts must diagnose traps but retain pointer poststate");
}

void testDeletePointerAndNull(TestContext& context)
{
    SemaFixture pointerFixture;
    pointerFixture.initializeGlobalScope();
    Type* pointerType = pointerFixture.types.pointerTo(pointerFixture.types.builtin(TypeKind::Int));
    Symbol pointer(SymbolKind::Variable, "value", pointerType, nullptr);
    pointerFixture.analyzer.currentScope_->declare(&pointer);
    AstNode* identifier = pointerFixture.node(AstNodeKind::IdentifierExpression);
    identifier->setText("value");
    AstNode* deletePointer = pointerFixture.node(AstNodeKind::DeleteExpression);
    deletePointer->setFirst(identifier);
    const ExpressionInfo first = pointerFixture.analyzer.analyzeDelete(deletePointer);

    SemaFixture nullFixture;
    AstNode* deleteNull = nullFixture.node(AstNodeKind::DeleteExpression);
    deleteNull->setFirst(nullFixture.nullLiteral());
    const ExpressionInfo second = nullFixture.analyzer.analyzeDelete(deleteNull);
    context.expect(first.type->kind() == TypeKind::Void && second.type->kind() == TypeKind::Void &&
            first.category == ValueCategory::Value && second.category == ValueCategory::Value &&
            pointerFixture.diagnostics.diagnostics().empty() && nullFixture.diagnostics.diagnostics().empty(),
        "TS-SEMA-250 delete must accept pointer and null operands and produce void rvalues");
}

void testDeleteInvalidOperands(TestContext& context)
{
    SemaFixture scalar;
    AstNode* deleteScalar = scalar.node(AstNodeKind::DeleteExpression);
    deleteScalar->setFirst(scalar.integer(9));
    const ExpressionInfo first = scalar.analyzer.analyzeDelete(deleteScalar);

    SemaFixture aggregate;
    aggregate.initializeGlobalScope();
    Type* record = aggregate.types.createStruct("Record", nullptr);
    Symbol value(SymbolKind::Variable, "record", record, nullptr);
    aggregate.analyzer.currentScope_->declare(&value);
    AstNode* identifier = aggregate.node(AstNodeKind::IdentifierExpression);
    identifier->setText("record");
    AstNode* deleteAggregate = aggregate.node(AstNodeKind::DeleteExpression);
    deleteAggregate->setFirst(identifier);
    const ExpressionInfo second = aggregate.analyzer.analyzeDelete(deleteAggregate);
    context.expect(first.type->kind() == TypeKind::Error && second.type->kind() == TypeKind::Error &&
            hasDiagnostic(scalar.diagnostics, DiagnosticId::InvalidDeleteOperand) &&
            hasDiagnostic(aggregate.diagnostics, DiagnosticId::InvalidDeleteOperand),
        "TS-SEMA-251 delete must reject scalar and aggregate non-pointer operands");
}

void testStackallocValid(TestContext& context)
{
    SemaFixture zero;
    SemaFixture many;
    const ExpressionInfo first = analyzeStackArray(zero, zero.integer(0));
    const ExpressionInfo second = analyzeStackArray(many, many.integer(16));
    context.expect(first.type->kind() == TypeKind::Pointer && second.type->kind() == TypeKind::Pointer &&
            first.type->elementType()->kind() == TypeKind::Int &&
            zero.diagnostics.diagnostics().empty() && many.diagnostics.diagnostics().empty(),
        "TS-SEMA-252 stackalloc with valid object type and integer count must return the exact element pointer");
}

void testStackallocInvalid(TestContext& context)
{
    SemaFixture badType;
    const ExpressionInfo first = analyzeStackArray(badType, badType.integer(1), TokenKind::KwVoid);
    SemaFixture badCount;
    const ExpressionInfo second = analyzeStackArray(badCount, badCount.boolean(false));
    context.expect(first.type->kind() == TypeKind::Error && second.type->kind() == TypeKind::Error &&
            hasDiagnostic(badType.diagnostics, DiagnosticId::InvalidStackalloc) &&
            hasDiagnostic(badCount.diagnostics, DiagnosticId::InvalidStackalloc),
        "TS-SEMA-253 stackalloc must reject invalid element types and non-integer counts with InvalidStackalloc");
}

void testStackallocTrapPoststate(TestContext& context)
{
    SemaFixture negative;
    const ExpressionInfo first = analyzeStackArray(
        negative, negative.unary(TokenKind::Minus, negative.integer(1)));
    SemaFixture overflow;
    const ExpressionInfo second = analyzeStackArray(
        overflow, overflow.integer(std::numeric_limits<std::uint64_t>::max(), "18446744073709551615UL"),
        TokenKind::KwDouble);
    context.expect(first.type->kind() == TypeKind::Pointer && second.type->kind() == TypeKind::Pointer &&
            hasDiagnostic(negative.diagnostics, DiagnosticId::ConstantEvaluationTrap) &&
            hasDiagnostic(overflow.diagnostics, DiagnosticId::ConstantEvaluationTrap),
        "TS-SEMA-254 negative/overflow stackalloc counts must diagnose traps and retain pointer/error state");
}

void testResolveBuiltinKeywords(TestContext& context)
{
    SemaFixture fixture;
    const std::array mappings = {
        std::pair{TokenKind::KwVoid, TypeKind::Void}, std::pair{TokenKind::KwBool, TypeKind::Bool},
        std::pair{TokenKind::KwByte, TypeKind::Byte}, std::pair{TokenKind::KwChar, TypeKind::Char},
        std::pair{TokenKind::KwShort, TypeKind::Short}, std::pair{TokenKind::KwUShort, TypeKind::UShort},
        std::pair{TokenKind::KwInt, TypeKind::Int}, std::pair{TokenKind::KwUInt, TypeKind::UInt},
        std::pair{TokenKind::KwLong, TypeKind::Long}, std::pair{TokenKind::KwULong, TypeKind::ULong},
        std::pair{TokenKind::KwNInt, TypeKind::NInt}, std::pair{TokenKind::KwNUInt, TypeKind::NUInt},
        std::pair{TokenKind::KwFloat, TypeKind::Float}, std::pair{TokenKind::KwDouble, TypeKind::Double}};
    bool exact = true;
    for (const auto& [token, kind] : mappings)
    {
        TypeSyntax* syntax = fixture.typeSyntax(token, std::string(nativesharp::lexer::tokenKindName(token)));
        exact = exact && fixture.analyzer.resolveBaseType(syntax) == fixture.types.builtin(kind);
    }
    context.expect(exact, "TS-SEMA-255 all 14 builtin type keywords must resolve to their exact canonical builtin");
}

void testResolveNominalTypes(TestContext& context)
{
    SemaFixture fixture;
    fixture.initializeGlobalScope();
    Type* structure = fixture.types.createStruct("Record", nullptr);
    Type* enumeration = fixture.types.createEnum("Choice", nullptr);
    Symbol structSymbol(SymbolKind::Struct, "Record", structure, nullptr);
    Symbol enumSymbol(SymbolKind::Enum, "Choice", enumeration, nullptr);
    fixture.analyzer.globalScope_->declare(&structSymbol);
    fixture.analyzer.globalScope_->declare(&enumSymbol);
    context.expect(fixture.analyzer.resolveBaseType(fixture.typeSyntax(TokenKind::Identifier, "Record")) == structure &&
            fixture.analyzer.resolveBaseType(fixture.typeSyntax(TokenKind::Identifier, "Choice")) == enumeration,
        "TS-SEMA-256 global struct and enum names must resolve to their exact nominal identities");
}

void testUnknownAndInvalidTypeSyntax(TestContext& context)
{
    SemaFixture fixture;
    fixture.initializeGlobalScope();
    Type* unknown = fixture.analyzer.resolveBaseType(fixture.typeSyntax(TokenKind::Identifier, "Missing"));
    Type* invalid = fixture.analyzer.resolveBaseType(fixture.typeSyntax(TokenKind::Invalid, "?"));
    context.expect(unknown->kind() == TypeKind::Error && invalid->kind() == TypeKind::Error &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::UnknownTypeName) &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::ExpectedType),
        "TS-SEMA-257 unknown names and invalid base syntax must diagnose UnknownTypeName and ExpectedType");
}

void testPointerModifierOrder(TestContext& context)
{
    SemaFixture fixture;
    TypeSyntax* one = fixture.typeSyntax(TokenKind::KwInt, "int");
    one->addModifier(pointerModifier());
    TypeSyntax* three = fixture.typeSyntax(TokenKind::KwInt, "int");
    three->addModifier(pointerModifier());
    three->addModifier(pointerModifier());
    three->addModifier(pointerModifier());
    Type* oneType = fixture.analyzer.resolveType(one);
    Type* threeType = fixture.analyzer.resolveType(three);
    context.expect(oneType->kind() == TypeKind::Pointer && oneType->elementType()->kind() == TypeKind::Int &&
            threeType->kind() == TypeKind::Pointer && threeType->elementType()->kind() == TypeKind::Pointer &&
            threeType->elementType()->elementType()->kind() == TypeKind::Pointer &&
            threeType->elementType()->elementType()->elementType()->kind() == TypeKind::Int,
        "TS-SEMA-258 pointer modifiers must preserve exact one/N nesting order");
}

void testFixedArrayPositiveCounts(TestContext& context)
{
    SemaFixture fixture;
    TypeSyntax* one = fixture.typeSyntax(TokenKind::KwInt, "int");
    one->addModifier(arrayModifier(fixture.integer(1)));
    TypeSyntax* many = fixture.typeSyntax(TokenKind::KwByte, "byte");
    many->addModifier(arrayModifier(fixture.integer(12)));
    Type* oneType = fixture.analyzer.resolveType(one);
    Type* manyType = fixture.analyzer.resolveType(many);
    context.expect(oneType->kind() == TypeKind::Array && oneType->count() == 1 &&
            oneType->elementType()->kind() == TypeKind::Int && manyType->kind() == TypeKind::Array &&
            manyType->count() == 12 && manyType->elementType()->kind() == TypeKind::Byte,
        "TS-SEMA-259 positive constant fixed-array counts must preserve exact type and count");
}

void testFixedArrayInvalidCounts(TestContext& context)
{
    SemaFixture fixture;
    fixture.initializeGlobalScope();
    Symbol count(SymbolKind::Variable, "count", fixture.types.builtin(TypeKind::Int), nullptr);
    fixture.analyzer.currentScope_->declare(&count);
    AstNode* identifier = fixture.node(AstNodeKind::IdentifierExpression);
    identifier->setText("count");
    std::array<AstNode*, 3> counts = {
        fixture.integer(0), fixture.unary(TokenKind::Minus, fixture.integer(1)), identifier};
    bool allError = true;
    for (AstNode* countNode : counts)
    {
        TypeSyntax* syntax = fixture.typeSyntax(TokenKind::KwInt, "int");
        syntax->addModifier(arrayModifier(countNode));
        allError = allError && fixture.analyzer.resolveType(syntax)->kind() == TypeKind::Error;
    }
    context.expect(allError && fixture.diagnostics.errorCount() == 3 &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidArrayCount),
        "TS-SEMA-260 zero, negative and nonconstant fixed-array counts must all diagnose InvalidArrayCount");
}

void testFixedArrayTargetOverflow(TestContext& context)
{
    SemaFixture fixture;
    TypeSyntax* syntax = fixture.typeSyntax(TokenKind::KwDouble, "double");
    syntax->addModifier(arrayModifier(
        fixture.integer(std::numeric_limits<std::uint64_t>::max(), "18446744073709551615UL")));
    Type* result = fixture.analyzer.resolveType(syntax);
    context.expect(result->kind() == TypeKind::Error &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidArrayCount),
        "TS-SEMA-261 a representable count whose target byte size overflows must diagnose InvalidArrayCount");
}

void testOwnershipResolution(TestContext& context)
{
    SemaFixture fixture;
    TypeSyntax* plain = fixture.typeSyntax(TokenKind::KwInt, "int");
    TypeSyntax* owned = fixture.typeSyntax(TokenKind::KwInt, "int");
    TypeSyntax* borrowed = fixture.typeSyntax(TokenKind::KwInt, "int");
    owned->setOwnershipQualifier(OwnershipQualifierSyntaxKind::Owned);
    borrowed->setOwnershipQualifier(OwnershipQualifierSyntaxKind::Borrowed);
    Type* plainType = fixture.analyzer.resolveType(plain);
    Type* ownedType = fixture.analyzer.resolveType(owned);
    Type* borrowedType = fixture.analyzer.resolveType(borrowed);
    context.expect(plainType == fixture.types.builtin(TypeKind::Int) &&
            ownedType->ownership() == OwnershipKind::Owned && borrowedType->ownership() == OwnershipKind::Borrowed &&
            ownedType->unqualifiedType() == plainType && borrowedType->unqualifiedType() == plainType,
        "TS-SEMA-262 none/owned/borrowed syntax must produce the exact final canonical wrapper");
}

void testValidateObjectTypeMatrix(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* array = fixture.types.arrayOf(integer, 2);
    Type* incomplete = fixture.types.createStruct("Forward", nullptr);
    incomplete->setComplete(false);
    const bool scalarOk = fixture.analyzer.validateObjectType(integer, SourceRange{}, false);
    const bool arrayRejected = !fixture.analyzer.validateObjectType(array, SourceRange{}, false);
    const bool arrayAllowed = fixture.analyzer.validateObjectType(array, SourceRange{}, true);
    const bool voidRejected = !fixture.analyzer.validateObjectType(
        fixture.types.builtin(TypeKind::Void), SourceRange{}, true);
    const bool incompleteRejected = !fixture.analyzer.validateObjectType(incomplete, SourceRange{}, true);
    context.expect(scalarOk && arrayRejected && arrayAllowed && voidRejected && incompleteRejected &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidType) &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::IncompleteType),
        "TS-SEMA-263 validateObjectType must preserve the exact kind-by-allowArray acceptance matrix");
}

void testCAbiDirectTypeMatrix(TestContext& context)
{
    SemaFixture fixture;
    bool exact = true;
    for (TypeKind kind : {TypeKind::Bool, TypeKind::Byte, TypeKind::Char, TypeKind::Short, TypeKind::UShort,
             TypeKind::Int, TypeKind::UInt, TypeKind::Long, TypeKind::ULong, TypeKind::NInt, TypeKind::NUInt,
             TypeKind::Float, TypeKind::Double})
    {
        exact = exact && fixture.analyzer.isCAbiCompatibleDirectType(fixture.types.builtin(kind), false);
    }
    exact = exact && fixture.analyzer.isCAbiCompatibleDirectType(
        fixture.types.pointerTo(fixture.types.builtin(TypeKind::Int)), false);
    exact = exact && fixture.analyzer.isCAbiCompatibleDirectType(fixture.types.createEnum("E", nullptr), false);
    exact = exact && fixture.analyzer.isCAbiCompatibleDirectType(fixture.types.builtin(TypeKind::Void), true);
    exact = exact && !fixture.analyzer.isCAbiCompatibleDirectType(fixture.types.builtin(TypeKind::Void), false);
    exact = exact && fixture.analyzer.isCAbiCompatibleDirectType(fixture.types.builtin(TypeKind::Error), false);
    context.expect(exact, "TS-SEMA-264 C ABI direct-type compatibility must match the complete scalar/pointer/enum matrix");
}

void testCAbiStructTerminationBoundary(TestContext& context)
{
    SemaFixture fixture;
    Type* empty = fixture.types.createStruct("Empty", nullptr);
    Type* incomplete = fixture.types.createStruct("Forward", nullptr);
    incomplete->setComplete(false);
    context.expect(!fixture.analyzer.isCAbiCompatibleStruct(empty) &&
            !fixture.analyzer.isCAbiCompatibleStruct(incomplete),
        "TS-SEMA-265 empty and incomplete structs must be rejected before recursive field traversal");

    // HostExecutor launches every manifest case in its own process. A missing
    // recursion guard therefore fails only this case instead of the full suite.
    Type* recursive = fixture.types.createStruct("Recursive", nullptr);
    recursive->setComplete(true);
    fixture.analyzer.structFieldTypes_[recursive] = {recursive};
    context.expect(!fixture.analyzer.isCAbiCompatibleStruct(recursive),
        "TS-SEMA-265 a self-recursive C ABI struct must terminate and reject");
}

void testCAbiStructFields(TestContext& context)
{
    SemaFixture fixture;
    Type* integer = fixture.types.builtin(TypeKind::Int);
    Type* nested = fixture.types.createStruct("Nested", nullptr);
    nested->setComplete(true);
    fixture.analyzer.structFieldTypes_[nested] = {integer};
    Type* outer = fixture.types.createStruct("Outer", nullptr);
    outer->setComplete(true);
    fixture.analyzer.structFieldTypes_[outer] = {fixture.types.arrayOf(integer, 3), nested};
    Type* errorOuter = fixture.types.createStruct("ErrorOuter", nullptr);
    errorOuter->setComplete(true);
    fixture.analyzer.structFieldTypes_[errorOuter] = {fixture.types.builtin(TypeKind::Error)};
    context.expect(fixture.analyzer.isCAbiCompatibleStruct(outer) &&
            !fixture.analyzer.isCAbiCompatibleStruct(errorOuter),
        "TS-SEMA-266 C ABI compatibility must recurse through array/nested-struct fields and reject error fields");
}
}

void registerSemaAllocationType245To266Module(TestRegistry& registry)
{
    addSemaCase(registry, "TS-SEMA-245", "CS-SEMA-024", "component", testNewObject);
    addSemaCase(registry, "TS-SEMA-246", "CS-SEMA-024", "component", testNewInvalidObjectTypes,
        "PR", TestDisposition::KnownProductDefect, {"product-defect", "allocation", "function-type"});
    addSemaCase(registry, "TS-SEMA-247", "CS-SEMA-024", "component", testNewArrayCounts);
    addSemaCase(registry, "TS-SEMA-248", "CS-SEMA-024", "component", testNewArrayNonIntegerCount);
    addSemaCase(registry, "TS-SEMA-249", "CS-SEMA-024", "component", testNewArrayTrapPoststate);
    addSemaCase(registry, "TS-SEMA-250", "CS-SEMA-024", "component", testDeletePointerAndNull);
    addSemaCase(registry, "TS-SEMA-251", "CS-SEMA-024", "component", testDeleteInvalidOperands);
    addSemaCase(registry, "TS-SEMA-252", "CS-SEMA-024", "component", testStackallocValid);
    addSemaCase(registry, "TS-SEMA-253", "CS-SEMA-024", "component", testStackallocInvalid);
    addSemaCase(registry, "TS-SEMA-254", "CS-SEMA-024", "component", testStackallocTrapPoststate);
    addSemaCase(registry, "TS-SEMA-255", "CS-SEMA-025", "component", testResolveBuiltinKeywords);
    addSemaCase(registry, "TS-SEMA-256", "CS-SEMA-025", "component", testResolveNominalTypes);
    addSemaCase(registry, "TS-SEMA-257", "CS-SEMA-025", "component", testUnknownAndInvalidTypeSyntax);
    addSemaCase(registry, "TS-SEMA-258", "CS-SEMA-025", "component", testPointerModifierOrder);
    addSemaCase(registry, "TS-SEMA-259", "CS-SEMA-025", "component", testFixedArrayPositiveCounts);
    addSemaCase(registry, "TS-SEMA-260", "CS-SEMA-025", "component", testFixedArrayInvalidCounts);
    addSemaCase(registry, "TS-SEMA-261", "CS-SEMA-025", "component", testFixedArrayTargetOverflow);
    addSemaCase(registry, "TS-SEMA-262", "CS-SEMA-025", "component", testOwnershipResolution);
    addSemaCase(registry, "TS-SEMA-263", "CS-SEMA-025", "component", testValidateObjectTypeMatrix);
    addSemaCase(registry, "TS-SEMA-264", "CS-SEMA-025", "component", testCAbiDirectTypeMatrix);
    addSemaCase(registry, "TS-SEMA-265", "CS-SEMA-025", "component", testCAbiStructTerminationBoundary,
        "PR", TestDisposition::KnownProductDefect,
        {"known-product-defect", "process-isolation", "known-defect", "unsafe-boundary"});
    addSemaCase(registry, "TS-SEMA-266", "CS-SEMA-025", "component", testCAbiStructFields);
}
}
