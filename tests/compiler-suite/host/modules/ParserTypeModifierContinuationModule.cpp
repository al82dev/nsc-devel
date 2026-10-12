#include "ParserFoundationTestSupport.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-007", "Host.Parser", "component", "PR",
        {"parser", "type", "modifier"}, std::move(function), disposition});
}

void testPointerChainsAndPointeeConst(TestContext& context)
{
    ParserFixture plain("T*");
    ParserFixture pointeeConst("T const*");
    ParserFixture chain("T***");
    TypeSyntax* const plainType = plain.parser().parseType();
    TypeSyntax* const constType = pointeeConst.parser().parseType();
    TypeSyntax* const chainType = chain.parser().parseType();
    context.expect(plainType != nullptr && plainType->modifiers().size() == 1 &&
            plainType->modifiers()[0].kind == TypeModifierSyntaxKind::Pointer &&
            !plainType->isPointeeConst() && constType != nullptr && constType->isPointeeConst() &&
            constType->modifiers().size() == 1 && chainType != nullptr &&
            chainType->modifiers().size() == 3 &&
            chainType->modifiers()[0].kind == TypeModifierSyntaxKind::Pointer &&
            chainType->modifiers()[1].kind == TypeModifierSyntaxKind::Pointer &&
            chainType->modifiers()[2].kind == TypeModifierSyntaxKind::Pointer,
        "TP-PAR-060 pointer chains and postfix pointee const must preserve exact modifier order");
}

void testPointerAlignment(TestContext& context)
{
    ParserFixture valid("T* align(16)");
    ParserFixture invalid("T* align(3)");
    TypeSyntax* const validType = valid.parser().parseType();
    TypeSyntax* const invalidType = invalid.parser().parseType();
    context.expect(validType != nullptr && validType->modifiers().size() == 1 &&
            validType->modifiers()[0].pointerKnownAlignment == 16 &&
            valid.diagnostics().diagnostics().empty(),
        "TP-PAR-061 valid pointer alignment must be retained exactly");
    context.expect(invalidType != nullptr && invalidType->modifiers().size() == 1 &&
            invalidType->modifiers()[0].pointerKnownAlignment == 1 &&
            diagnosticCount(invalid.diagnostics(), DiagnosticId::InvalidType) == 1 &&
            invalid.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-061 invalid pointer alignment must diagnose and recover after the qualifier");
}

void testPointerAddressSpace(TestContext& context)
{
    ParserFixture valid("T* addrspace(7)");
    ParserFixture invalid("T* addrspace(256)");
    TypeSyntax* const validType = valid.parser().parseType();
    TypeSyntax* const invalidType = invalid.parser().parseType();
    context.expect(validType != nullptr && validType->modifiers().size() == 1 &&
            validType->modifiers()[0].pointerAddressSpace == 7 &&
            valid.diagnostics().diagnostics().empty(),
        "TP-PAR-062 valid pointer address space must be retained exactly");
    context.expect(invalidType != nullptr && invalidType->modifiers().size() == 1 &&
            invalidType->modifiers()[0].pointerAddressSpace == 0 &&
            diagnosticCount(invalid.diagnostics(), DiagnosticId::InvalidType) == 1 &&
            invalid.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-062 out-of-range address space must diagnose and recover finitely");
}

void testPointerProvenance(TestContext& context)
{
    ParserFixture none("T* provenance(none)");
    ParserFixture tracked("T* provenance(tracked)");
    ParserFixture invalid("T* provenance(unknown)");
    TypeSyntax* const noneType = none.parser().parseType();
    TypeSyntax* const trackedType = tracked.parser().parseType();
    TypeSyntax* const invalidType = invalid.parser().parseType();
    context.expect(noneType != nullptr && noneType->modifiers().size() == 1 &&
            !noneType->modifiers()[0].pointerHasProvenance && trackedType != nullptr &&
            trackedType->modifiers().size() == 1 && trackedType->modifiers()[0].pointerHasProvenance,
        "TP-PAR-063 none/tracked provenance modes must map to the exact pointer flag");
    context.expect(invalidType != nullptr &&
            diagnosticCount(invalid.diagnostics(), DiagnosticId::ExpectedToken) == 1 &&
            invalid.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-063 invalid provenance text must diagnose and recover through the closing delimiter");
}

void testFixedArrayBound(TestContext& context)
{
    ParserFixture fixture("int[4]");
    TypeSyntax* const type = fixture.parser().parseType();
    const TypeModifierSyntax* const modifier = type != nullptr && type->modifiers().size() == 1
        ? &type->modifiers()[0] : nullptr;
    context.expect(modifier != nullptr && modifier->kind == TypeModifierSyntaxKind::Array &&
            modifier->arrayCount != nullptr &&
            modifier->arrayCount->kind() == AstNodeKind::IntegerLiteralExpression &&
            modifier->arrayCount->integerValue() == 4 && modifier->range.begin().offset() == 3 &&
            modifier->range.end().offset() == 6 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-064 fixed array must retain bound expression, modifier kind, and source range");
}

void testDynamicAndRankArrays(TestContext& context)
{
    ParserFixture dynamic("int[]");
    ParserFixture rankTwo("int[,]");
    ParserFixture rankThree("int[,,]");
    TypeSyntax* const dynamicType = dynamic.parser().parseType();
    TypeSyntax* const rankTwoType = rankTwo.parser().parseType();
    TypeSyntax* const rankThreeType = rankThree.parser().parseType();
    context.expect(dynamicType != nullptr && dynamicType->modifiers().size() == 1 &&
            dynamicType->modifiers()[0].kind == TypeModifierSyntaxKind::DynamicArray &&
            rankTwoType != nullptr && rankTwoType->modifiers().size() == 1 &&
            rankTwoType->modifiers()[0].kind == TypeModifierSyntaxKind::RangeArray &&
            rankTwoType->modifiers()[0].arrayRank == 2 && rankThreeType != nullptr &&
            rankThreeType->modifiers().size() == 1 &&
            rankThreeType->modifiers()[0].kind == TypeModifierSyntaxKind::RangeArray &&
            rankThreeType->modifiers()[0].arrayRank == 3,
        "TP-PAR-065 dynamic and ranked arrays must preserve exact kind and rank");
}

void testFlexibleArray(TestContext& context)
{
    ParserFixture fixture("byte[*]");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->modifiers().size() == 1 &&
            type->modifiers()[0].kind == TypeModifierSyntaxKind::FlexibleArray &&
            type->modifiers()[0].range.begin().offset() == 4 &&
            type->modifiers()[0].range.end().offset() == 7 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-066 flexible array modifier must preserve exact kind and source position");
}

void testMixedPointerArrayOrder(TestContext& context)
{
    ParserFixture fixture("int*[4][]");
    TypeSyntax* const type = fixture.parser().parseType();
    context.expect(type != nullptr && type->modifiers().size() == 3 &&
            type->modifiers()[0].kind == TypeModifierSyntaxKind::Pointer &&
            type->modifiers()[1].kind == TypeModifierSyntaxKind::Array &&
            type->modifiers()[2].kind == TypeModifierSyntaxKind::DynamicArray &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-067 mixed pointer/array modifiers must remain in source order");
}

void testAllocationElementStopsBeforeInitializer(TestContext& context)
{
    ParserFixture fixture("Widget<int>* = value");
    TypeSyntax* const type = fixture.parser().parseAllocationElementType();
    context.expect(type != nullptr && type->baseName() == "Widget" &&
            type->typeArguments().size() == 1 && type->typeArguments()[0]->baseName() == "int" &&
            type->modifiers().size() == 1 &&
            type->modifiers()[0].kind == TypeModifierSyntaxKind::Pointer &&
            fixture.parser().current().kind() == TokenKind::Equal &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-068 allocation element parser must stop before the initializer token");
}

void testInvalidAndVoidAllocationElements(TestContext& context)
{
    ParserFixture invalid(";");
    TypeSyntax* const invalidType = invalid.parser().parseAllocationElementType();
    context.expect(invalidType == nullptr && invalid.parser().index_ == 0 &&
            diagnosticCount(invalid.diagnostics(), DiagnosticId::ExpectedType) == 1,
        "TP-PAR-069 invalid allocation element start must diagnose without losing the token");

    ParserFixture voidObject("void next");
    TypeSyntax* const voidType = voidObject.parser().parseAllocationElementType();
    context.expect(voidType != nullptr && voidObject.parser().current().kind() == TokenKind::Identifier &&
            diagnosticCount(voidObject.diagnostics(), DiagnosticId::InvalidType) == 1,
        "TP-PAR-069 void allocation object must diagnose InvalidType after consuming its type");
}
}

void registerParserTypeModifierContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-060", testPointerChainsAndPointeeConst);
    addCase(registry, "TP-PAR-061", testPointerAlignment);
    addCase(registry, "TP-PAR-062", testPointerAddressSpace);
    addCase(registry, "TP-PAR-063", testPointerProvenance);
    addCase(registry, "TP-PAR-064", testFixedArrayBound);
    addCase(registry, "TP-PAR-065", testDynamicAndRankArrays);
    addCase(registry, "TP-PAR-066", testFlexibleArray);
    addCase(registry, "TP-PAR-067", testMixedPointerArrayOrder);
    addCase(registry, "TP-PAR-068", testAllocationElementStopsBeforeInitializer);
    addCase(registry, "TP-PAR-069", testInvalidAndVoidAllocationElements);
}
}
