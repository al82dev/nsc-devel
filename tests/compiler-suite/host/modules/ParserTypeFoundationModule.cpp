#include "ParserFoundationTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;

void testEveryBuiltinType(TestContext& context)
{
    constexpr std::array<std::string_view, 18> spellings = {
        "void", "bool", "sbyte", "byte", "char", "short", "ushort", "int", "uint",
        "long", "ulong", "nint", "nuint", "i128", "u128", "float", "double", "string",
    };
    bool allExact = true;
    for (const std::string_view spelling : spellings)
    {
        ParserFixture fixture{std::string(spelling)};
        const TokenKind expectedKind = fixture.tokens()[0].kind();
        TypeSyntax* const type = fixture.parser().parseType();
        allExact = allExact && type != nullptr && type->baseTokenKind() == expectedKind &&
            type->baseName() == spelling && fixture.parser().current().kind() == TokenKind::EndOfFile;
    }
    context.expect(allExact,
        "TP-PAR-036 every builtin must preserve exact base token kind and canonical name");
}

void testKnownQualifiedNominalType(TestContext& context)
{
    ParserFixture fixture("A.B.C", {"A.B.C"});
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->baseTokenKind() == TokenKind::Identifier &&
            type->baseName() == "A.B.C" && type->range().begin().offset() == 0 &&
            type->range().end().offset() == 5,
        "TP-PAR-037 qualified known type must retain complete nominal name and range");
}

void testOwnershipQualifiedTypes(TestContext& context)
{
    ParserFixture owned("owned T", {"T"});
    ParserFixture borrowed("borrowed T", {"T"});
    ParserFixture lifetime("borrowed[a] T", {"T"});
    TypeSyntax* const ownedType = owned.parser().parseType();
    TypeSyntax* const borrowedType = borrowed.parser().parseType();
    TypeSyntax* const lifetimeType = lifetime.parser().parseType();
    context.expect(ownedType != nullptr &&
            ownedType->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned &&
            borrowedType != nullptr &&
            borrowedType->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed &&
            lifetimeType != nullptr &&
            lifetimeType->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed &&
            lifetimeType->lifetimeName() == "a",
        "TP-PAR-038 ownership qualifier and borrowed lifetime must be exact");
}

void testDynamicTraitType(TestContext& context)
{
    ParserFixture fixture("dyn Trait", {"Trait"});
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isDynamicTrait() && type->baseName() == "Trait",
        "TP-PAR-039 dyn Trait must retain dynamic marker and nominal base");
}

void testOpaqueResultType(TestContext& context)
{
    ParserFixture fixture("some Trait", {"Trait"});
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isOpaqueResult() && type->baseName() == "Trait" &&
            type->opaqueResultTraitName() == "Trait",
        "TP-PAR-040 some Trait must retain opaque-result marker and trait name");
}

void testTupleArities(TestContext& context)
{
    ParserFixture empty("()");
    ParserFixture unary("(int,)");
    ParserFixture multiple("(int,string)");
    TypeSyntax* const emptyType = empty.parser().parseType();
    TypeSyntax* const unaryType = unary.parser().parseType();
    TypeSyntax* const multipleType = multiple.parser().parseType();
    context.expect(emptyType != nullptr && emptyType->isTuple() &&
            emptyType->typeArguments().empty() && unaryType != nullptr && unaryType->isTuple() &&
            unaryType->typeArguments().size() == 1 && multipleType != nullptr &&
            multipleType->isTuple() && multipleType->typeArguments().size() == 2,
        "TP-PAR-041 tuple flag and element arity must be exact for empty, unary, and multiple");
    context.expect(emptyType != nullptr && emptyType->range().begin().offset() == 0 &&
            emptyType->range().end().offset() == 2 && multipleType != nullptr &&
            multipleType->range().end().offset() == 12,
        "TP-PAR-041 tuple ranges must span their complete delimiters");
}

void testPartiallyNamedTuple(TestContext& context)
{
    ParserFixture fixture("(int x,string,long z)");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isTuple() && type->typeArguments().size() == 3 &&
            type->tupleElementNames() == std::vector<std::string>({"x", "", "z"}),
        "TP-PAR-042 tuple element names must remain positionally aligned");
}

void testUnexpectedTypeToken(TestContext& context)
{
    ParserFixture fixture(";");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type == nullptr && fixture.parser().index_ == 0 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedType) == 1,
        "TP-PAR-043 unexpected type token must diagnose ExpectedType without false consumption");
}

void testVoidObjectArrayAndPointer(TestContext& context)
{
    ParserFixture object("void");
    ParserFixture array("void[]");
    ParserFixture pointer("void*");
    TypeSyntax* const objectType = object.parser().parseType();
    TypeSyntax* const arrayType = array.parser().parseType();
    TypeSyntax* const pointerType = pointer.parser().parseType();
    context.expect(objectType != nullptr && objectType->modifiers().empty() &&
            arrayType != nullptr && arrayType->modifiers().size() == 1 &&
            arrayType->modifiers()[0].kind == TypeModifierSyntaxKind::DynamicArray &&
            pointerType != nullptr && pointerType->modifiers().size() == 1 &&
            pointerType->modifiers()[0].kind == TypeModifierSyntaxKind::Pointer,
        "TP-PAR-044 parseType must distinguish void object, array, and pointer shapes");
    ParserFixture fields("struct S { void object; void[] array; void* pointer; }");
    fields.parse();
    context.expect(diagnosticCount(fields.diagnostics(), DiagnosticId::InvalidType) >= 2,
        "TP-PAR-044 object and array void fields must be rejected while pointer remains representable");
}

void testQuestionNullabilityContexts(TestContext& context)
{
    ParserFixture allowed("T*? tail", {"T"});
    ParserFixture notPointer("T? tail", {"T"});
    TypeSyntax* const allowedType = allowed.parser().parseType();
    TypeSyntax* const rejectedType = notPointer.parser().parseType();
    context.expect(allowedType != nullptr && allowedType->modifiers().size() == 1 &&
            allowedType->modifiers()[0].pointerNullability == PointerNullabilitySyntaxKind::Nullable &&
            rejectedType != nullptr && rejectedType->modifiers().empty() &&
            notPointer.parser().current().kind() == TokenKind::Question,
        "TP-PAR-045 question marker must bind only to a pointer modifier");
}

void testExplicitTraitImplementationType(TestContext& context)
{
    ParserFixture fixture("impl Trait for Target {}", {"Trait", "Target"});
    AstNode* const root = fixture.parse();
    AstNode* const implementation = root->children().empty() ? nullptr : root->children()[0];
    context.expect(implementation != nullptr &&
            implementation->kind() == AstNodeKind::ClassDeclaration &&
            implementation->typeSyntax() != nullptr &&
            implementation->typeSyntax()->isExplicitTraitImplementation() &&
            implementation->typeSyntax()->baseName() == "Trait",
        "TP-PAR-046 impl trait type must preserve explicit-implementation flag and name");
}

void testDeepValidTypeCombination(TestContext& context)
{
    ParserFixture fixture(
        "borrowed[a] Map<string,fn&[b](int)->Result*>*? align(16)[]",
        {"Map", "Result"});
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr &&
            type->ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed &&
            type->lifetimeName() == "a" && type->baseName() == "Map" &&
            type->typeArguments().size() == 2 && type->modifiers().size() == 2,
        "TP-PAR-047 deep valid type must preserve outer ownership, generics, pointer, and array");
    context.expect(type != nullptr && type->typeArguments().size() == 2 &&
            type->typeArguments()[1]->isFunction() &&
            type->typeArguments()[1]->functionCarrier() == FunctionTypeCarrierSyntaxKind::Reference &&
            type->typeArguments()[1]->functionLifetimeName() == "b",
        "TP-PAR-047 deep valid type must preserve nested function hierarchy");
}

void testEmptyFunctionType(TestContext& context)
{
    ParserFixture fixture("fn*()->void");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isFunction() &&
            type->functionCarrier() == FunctionTypeCarrierSyntaxKind::Pointer &&
            type->typeArguments().empty() && type->functionReturnType() != nullptr &&
            type->functionReturnType()->baseName() == "void",
        "TP-PAR-048 empty function type must preserve carrier and void return");
}

void testFunctionTypeParameters(TestContext& context)
{
    ParserFixture fixture("fn*(int,string,bool)->void");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->isFunction() && type->typeArguments().size() == 3 &&
            type->typeArguments()[0]->baseName() == "int" &&
            type->typeArguments()[1]->baseName() == "string" &&
            type->typeArguments()[2]->baseName() == "bool",
        "TP-PAR-049 function type parameters must retain exact source order");
}

void testSupportedCallingConventions(TestContext& context)
{
    using Convention = FunctionCallingConventionSyntaxKind;
    constexpr std::array<std::string_view, 7> spellings = {
        "native", "C", "cdecl", "stdcall", "fastcall", "vectorcall", "win64",
    };
    constexpr std::array expected = {
        Convention::NativeSharp, Convention::C, Convention::C, Convention::StdCall,
        Convention::FastCall, Convention::VectorCall, Convention::Win64,
    };
    bool allExact = true;
    for (std::size_t index = 0; index < spellings.size(); ++index)
    {
        ParserFixture fixture("fn*" + std::string(spellings[index]) + "()->void");
        TypeSyntax* const type = fixture.parser().parseType();
        allExact = allExact && type != nullptr && type->isFunction() &&
            type->functionCallingConvention() == expected[index];
    }
    context.expect(allExact,
        "TP-PAR-050 every supported calling convention must map to its contractual enum");
}
}

void registerParserTypeFoundationModule(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-036", "CP-PAR-005", "component", "PR", testEveryBuiltinType);
    addParserCase(registry, "TP-PAR-037", "CP-PAR-005", "component", "PR", testKnownQualifiedNominalType);
    addParserCase(registry, "TP-PAR-038", "CP-PAR-005", "component", "PR", testOwnershipQualifiedTypes);
    addParserCase(registry, "TP-PAR-039", "CP-PAR-005", "component", "PR", testDynamicTraitType);
    addParserCase(registry, "TP-PAR-040", "CP-PAR-005", "component", "PR", testOpaqueResultType);
    addParserCase(registry, "TP-PAR-041", "CP-PAR-005", "component", "PR", testTupleArities);
    addParserCase(registry, "TP-PAR-042", "CP-PAR-005", "component", "PR", testPartiallyNamedTuple);
    addParserCase(registry, "TP-PAR-043", "CP-PAR-005", "component", "PR", testUnexpectedTypeToken);
    addParserCase(registry, "TP-PAR-044", "CP-PAR-005", "component", "PR", testVoidObjectArrayAndPointer);
    addParserCase(registry, "TP-PAR-045", "CP-PAR-005", "component", "PR", testQuestionNullabilityContexts);
    addParserCase(registry, "TP-PAR-046", "CP-PAR-005", "component", "PR", testExplicitTraitImplementationType);
    addParserCase(registry, "TP-PAR-047", "CP-PAR-005", "component", "NIGHTLY", testDeepValidTypeCombination);
    addParserCase(registry, "TP-PAR-048", "CP-PAR-006", "component", "PR", testEmptyFunctionType);
    addParserCase(registry, "TP-PAR-049", "CP-PAR-006", "component", "PR", testFunctionTypeParameters);
    addParserCase(registry, "TP-PAR-050", "CP-PAR-006", "component", "PR", testSupportedCallingConventions);
}
}
