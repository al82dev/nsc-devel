#include "ParserCallableTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace parser_callable_support;

void testEmptyConstructor(TestContext& context)
{
    DeclarationFixture fixture("class C { C() { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ConstructorDeclaration &&
            node->text() == "C" && node->children().empty() && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::BlockStatement,
        "TP-PAR-257 empty constructor must retain identity and empty block body");
}

void testConstructorParameters(TestContext& context)
{
    DeclarationFixture fixture("class C { C(int first, long second = 2) { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->children().size() == 2 &&
            node->children()[0]->text() == "first" && node->children()[0]->first() == nullptr &&
            node->children()[1]->text() == "second" && node->children()[1]->first() != nullptr &&
            node->children()[1]->first()->kind() == AstNodeKind::IntegerLiteralExpression,
        "TP-PAR-258 constructor parameters and defaults must remain exact children");
}

void testBaseInitializer(TestContext& context)
{
    DeclarationFixture fixture("class C { C(int value) : base(value, 2) { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->second() != nullptr &&
            node->second()->kind() == AstNodeKind::BaseConstructorInitializer &&
            node->second()->children().size() == 2,
        "TP-PAR-259 base initializer must retain kind and source-order arguments");
}

void testThisInitializer(TestContext& context)
{
    DeclarationFixture fixture("class C { C() : this(1) { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->second() != nullptr &&
            node->second()->kind() == AstNodeKind::ThisConstructorInitializer &&
            node->second()->children().size() == 1,
        "TP-PAR-260 this initializer must retain kind and argument");
}

void testConstructorNoHeapThrows(TestContext& context)
{
    // NS304-T09-F10: a constructor declares a typed error like any callable. The error TypeSyntax
    // is preserved beside the noheap modifier; semantic validation of the error type happens later.
    DeclarationFixture fixture("class C { noheap C() throws Error { } }", {"Error"});
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->hasModifier(TokenKind::KwNoHeap) &&
            node->throwsTypeSyntax() != nullptr &&
            node->throwsTypeSyntax()->baseName() == "Error" &&
            fixture.diagnostics().empty(),
        "TP-PAR-261 noheap constructor must retain its flag and preserve the typed throws error");
}

void testDifferentConstructorName(TestContext& context)
{
    DeclarationFixture fixture("class C { D() { } int kept; }", {"D"});
    AstNode* const type = declaration(fixture);
    bool constructor = false;
    if (type != nullptr)
    {
        for (const AstNode* const child : type->children())
            constructor = constructor || (child != nullptr && child->kind() == AstNodeKind::ConstructorDeclaration);
    }
    context.expect(!constructor && !fixture.diagnostics().empty(),
        "TP-PAR-262 a name different from the containing class must not become a constructor");
}

void testDestructor(TestContext& context)
{
    DeclarationFixture fixture("class C { ~C() { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::DestructorDeclaration &&
            node->text() == "~C" && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::BlockStatement,
        "TP-PAR-263 valid destructor must retain exact name and body");
}

void testInvalidDestructorShapes(TestContext& context)
{
    DeclarationFixture parameters("class C { ~C(int value) { } }");
    DeclarationFixture returnType("class C { int ~C() { } int kept; }");
    context.expect(diagnosticContains(parameters, "destructors cannot declare parameters") &&
            !returnType.diagnostics().empty(),
        "TP-PAR-264 destructor parameters and explicit return type must diagnose");
}

void testBodylessConstructor(TestContext& context)
{
    DeclarationFixture fixture("class C { C(); }");
    context.expect(diagnosticContains(fixture, "constructor declarations require a body"),
        "TP-PAR-265 bodyless constructor must retain the current explicit diagnostic");
}

void testConstructorRecovery(TestContext& context)
{
    DeclarationFixture fixture("class C { C(int value { } int kept; }");
    AstNode* const type = declaration(fixture);
    context.expect(type != nullptr && childNamed(type, "kept") != nullptr && !fixture.diagnostics().empty(),
        "TP-PAR-266 missing constructor delimiter must diagnose and recover to the following member");
}
}

void registerParserConstructorDestructorModule(TestRegistry& registry)
{
    addCallableCase(registry, "TP-PAR-257", "CP-PAR-025", testEmptyConstructor);
    addCallableCase(registry, "TP-PAR-258", "CP-PAR-025", testConstructorParameters);
    addCallableCase(registry, "TP-PAR-259", "CP-PAR-025", testBaseInitializer);
    addCallableCase(registry, "TP-PAR-260", "CP-PAR-025", testThisInitializer);
    addCallableCase(registry, "TP-PAR-261", "CP-PAR-025", testConstructorNoHeapThrows);
    addCallableCase(registry, "TP-PAR-262", "CP-PAR-025", testDifferentConstructorName);
    addCallableCase(registry, "TP-PAR-263", "CP-PAR-025", testDestructor);
    addCallableCase(registry, "TP-PAR-264", "CP-PAR-025", testInvalidDestructorShapes);
    addCallableCase(registry, "TP-PAR-265", "CP-PAR-025", testBodylessConstructor);
    addCallableCase(registry, "TP-PAR-266", "CP-PAR-025", testConstructorRecovery);
}
}
