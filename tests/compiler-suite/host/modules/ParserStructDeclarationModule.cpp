#include "ParserDeclarationTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using namespace parser_declaration_support;

void testEmptyStruct(TestContext& context)
{
    DeclarationFixture fixture("struct Empty { }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::StructDeclaration &&
            node->text() == "Empty" && node->children().empty(),
        "TP-PAR-151 empty struct must produce one empty StructDeclaration");
}

void testEmptyUnion(TestContext& context)
{
    DeclarationFixture fixture("union Storage { }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::StructDeclaration &&
            node->text() == "Storage" && hasMarker(node, "union") && node->children().empty(),
        "TP-PAR-152 union must retain the exact union layout marker");
}

void testIndependentStructFlags(TestContext& context)
{
    DeclarationFixture fixture(
        "record struct R { } copy struct C { } borrowed struct V { } readonly struct Q { }");
    const auto& nodes = fixture.root()->children();
    context.expect(nodes.size() == 4 && hasMarker(nodes[0], "record") &&
            !hasMarker(nodes[0], "copy") && hasMarker(nodes[1], "copy") &&
            !hasMarker(nodes[1], "record") && hasMarker(nodes[2], "borrowedview") &&
            nodes[3]->hasModifier(TokenKind::KwReadonly),
        "TP-PAR-153 record/copy/borrowed/readonly flags must remain independent");
}

void testPrimaryParameters(TestContext& context)
{
    DeclarationFixture fixture("struct Pair(public int First, readonly long Second) { }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->children().size() == 3 &&
            node->children()[0]->kind() == AstNodeKind::FieldDeclaration &&
            node->children()[0]->text() == "First" &&
            node->children()[1]->kind() == AstNodeKind::FieldDeclaration &&
            node->children()[1]->text() == "Second" &&
            node->children()[1]->hasModifier(TokenKind::KwReadonly) &&
            node->children()[2]->kind() == AstNodeKind::ConstructorDeclaration,
        "TP-PAR-154 promoted fields and constructor must preserve source order");
}

void testPrimaryConstructor(TestContext& context)
{
    DeclarationFixture fixture("struct Pair(int X, int Y) { }");
    AstNode* const node = declaration(fixture);
    AstNode* const constructor = node == nullptr ? nullptr : childNamed(node, "Pair");
    AstNode* const body = constructor == nullptr ? nullptr : constructor->first();
    context.expect(constructor != nullptr && hasMarker(constructor, "promoted-primary-constructor") &&
            constructor->children().size() == 2 && constructor->children()[0]->text() == "X" &&
            constructor->children()[1]->text() == "Y" && body != nullptr &&
            body->kind() == AstNodeKind::BlockStatement && body->children().size() == 2 &&
            constructor->range().begin().offset() < constructor->range().end().offset(),
        "TP-PAR-155 synthesized primary constructor must align parameters, assignments, and range");
}

void testStructBasesAndWhere(TestContext& context)
{
    DeclarationFixture fixture("struct Box<T> : Trait, Other where T : class { }", {"Trait", "Other"});
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->genericParameters() == std::vector<std::string>{"T"} &&
            node->typeSyntaxes().size() == 2 && node->typeSyntaxes()[0]->baseName() == "Trait" &&
            node->typeSyntaxes()[1]->baseName() == "Other" && node->genericConstraints().size() == 1 &&
            node->genericConstraints()[0].parameterName == "T" &&
            node->genericConstraints()[0].requiresClass,
        "TP-PAR-156 struct base specifications and where clauses must remain exact");
}

void testStructMemberOrder(TestContext& context)
{
    DeclarationFixture fixture(
        "struct Mixed { int Field; int Get() { return Field; } struct Nested { } }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->children().size() == 3 &&
            node->children()[0]->kind() == AstNodeKind::FieldDeclaration &&
            node->children()[0]->text() == "Field" &&
            node->children()[1]->kind() == AstNodeKind::MethodDeclaration &&
            node->children()[1]->text() == "Get" &&
            node->children()[2]->kind() == AstNodeKind::StructDeclaration &&
            node->children()[2]->text() == "Nested",
        "TP-PAR-157 fields, methods, and nested types must preserve source order");
}

void testBitFieldsAndAnonymousAggregates(TestContext& context)
{
    DeclarationFixture fixture("struct Bits { uint Low : 3; struct { int X; }; }");
    AstNode* const node = declaration(fixture);
    AstNode* const low = childNamed(node, "Low");
    context.expect(node != nullptr && low != nullptr && low->kind() == AstNodeKind::FieldDeclaration &&
            hasMarker(low, "bitfield") && low->second() != nullptr &&
            low->second()->kind() == AstNodeKind::IntegerLiteralExpression &&
            low->second()->integerValue() == 3 && node->children().size() == 2 &&
            node->children()[1]->kind() == AstNodeKind::StructDeclaration &&
            hasMarker(node->children()[1], "anonymous-struct") &&
            hasMarker(node->children()[1], "promote"),
        "TP-PAR-158 bitfields and anonymous aggregates must keep their exact AST representation");
}

void testAggregateLayoutAttributes(TestContext& context)
{
    DeclarationFixture fixture("pack(1) align(8) size(16) struct Packed { }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && hasMarker(node, "pack=1") && hasMarker(node, "align=8") &&
            hasMarker(node, "size=16"),
        "TP-PAR-159 pack/align/size values must be preserved exactly");
}

void testDuplicatePrimaryName(TestContext& context)
{
    DeclarationFixture fixture("struct Duplicate(int Value, int Value) { }");
    context.expect(fixture.diagnosticCount(DiagnosticId::DuplicateSymbol) == 1,
        "TP-PAR-160 duplicate promoted names must report DuplicateSymbol");
}

void testInvalidMemberRecovery(TestContext& context)
{
    DeclarationFixture fixture("struct Recovered { @ ; int Value; }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && childNamed(node, "Value") != nullptr && !fixture.diagnostics().empty(),
        "TP-PAR-161 invalid struct member must recover to the following valid member");
}

void testMissingStructBrace(TestContext& context)
{
    DeclarationFixture fixture("struct Open { int Value;");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && !fixture.diagnostics().empty() &&
            node->range().begin().offset() <= node->range().end().offset(),
        "TP-PAR-162 missing close brace must diagnose and retain a finite declaration range");
}

void testUnionModifierRejection(TestContext& context)
{
    DeclarationFixture fixture("readonly union Bad { int Value; }");
    context.expect(!fixture.diagnostics().empty(),
        "TP-PAR-163 incompatible union modifiers must retain the current rejection");
}

void testNestedGenericScope(TestContext& context)
{
    DeclarationFixture fixture(
        "struct Outer<T> { struct Inner<U> { U Value; } T Item; } "
        "int Main() { T * value; return 0; }");
    AstNode* const outer = declaration(fixture);
    AstNode* const function = declaration(fixture, 1);
    AstNode* const body = function == nullptr ? nullptr : function->first();
    context.expect(outer != nullptr && outer->genericParameters() == std::vector<std::string>{"T"} &&
            childNamed(outer, "Inner") != nullptr && body != nullptr && !body->children().empty() &&
            body->children()[0]->kind() == AstNodeKind::ExpressionStatement,
        "TP-PAR-164 nested generic scopes must not leak T into the following function");
}
}

void registerParserStructDeclarationModule(TestRegistry& registry)
{
    addDeclarationCase(registry, "TP-PAR-151", "CP-PAR-016", "PR", testEmptyStruct);
    addDeclarationCase(registry, "TP-PAR-152", "CP-PAR-016", "PR", testEmptyUnion);
    addDeclarationCase(registry, "TP-PAR-153", "CP-PAR-016", "PR", testIndependentStructFlags);
    addDeclarationCase(registry, "TP-PAR-154", "CP-PAR-016", "PR", testPrimaryParameters);
    addDeclarationCase(registry, "TP-PAR-155", "CP-PAR-016", "PR", testPrimaryConstructor);
    addDeclarationCase(registry, "TP-PAR-156", "CP-PAR-016", "PR", testStructBasesAndWhere);
    addDeclarationCase(registry, "TP-PAR-157", "CP-PAR-016", "PR", testStructMemberOrder);
    addDeclarationCase(registry, "TP-PAR-158", "CP-PAR-016", "PR", testBitFieldsAndAnonymousAggregates);
    addDeclarationCase(registry, "TP-PAR-159", "CP-PAR-016", "PR", testAggregateLayoutAttributes);
    addDeclarationCase(registry, "TP-PAR-160", "CP-PAR-016", "PR", testDuplicatePrimaryName);
    addDeclarationCase(registry, "TP-PAR-161", "CP-PAR-016", "PR", testInvalidMemberRecovery);
    addDeclarationCase(registry, "TP-PAR-162", "CP-PAR-016", "PR", testMissingStructBrace);
    addDeclarationCase(registry, "TP-PAR-163", "CP-PAR-016", "PR", testUnionModifierRejection);
    addDeclarationCase(registry, "TP-PAR-164", "CP-PAR-016", "PR", testNestedGenericScope);
}
}
