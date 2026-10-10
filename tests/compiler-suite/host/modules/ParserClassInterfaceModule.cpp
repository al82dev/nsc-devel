#include "ParserDeclarationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace parser_declaration_support;

void testMinimalClass(TestContext& context)
{
    DeclarationFixture fixture("class Empty { }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ClassDeclaration &&
            node->text() == "Empty" && node->children().empty(),
        "TP-PAR-183 minimal class must produce one exact ClassDeclaration");
}

void testClassFlags(TestContext& context)
{
    DeclarationFixture fixture(
        "partial class P { } abstract class A { } sealed class S { } static class T { }");
    const auto& nodes = fixture.root()->children();
    context.expect(nodes.size() == 4 && nodes[0]->flag() &&
            nodes[1]->hasModifier(TokenKind::KwAbstract) &&
            nodes[2]->hasModifier(TokenKind::KwSealed) &&
            nodes[3]->hasModifier(TokenKind::KwStatic) && !nodes[1]->flag() && !nodes[2]->flag(),
        "TP-PAR-184 partial/abstract/sealed/static class flags must remain orthogonal");
}

void testClassBases(TestContext& context)
{
    DeclarationFixture fixture(
        "class Derived : Base, IFirst, ISecond { }", {"Base", "IFirst", "ISecond"});
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->typeSyntaxes().size() == 3 &&
            node->typeSyntax() == node->typeSyntaxes()[0] &&
            node->typeSyntaxes()[0]->baseName() == "Base" &&
            node->typeSyntaxes()[1]->baseName() == "IFirst" &&
            node->typeSyntaxes()[2]->baseName() == "ISecond",
        "TP-PAR-185 base class and interfaces must preserve source order");
}

void testGenericClassConstraints(TestContext& context)
{
    DeclarationFixture fixture("class Box<T> where T : class, IValue { T Value; }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->genericParameters() == std::vector<std::string>{"T"} &&
            node->genericConstraints().size() == 1 && node->genericConstraints()[0].requiresClass &&
            node->genericConstraints()[0].typeConstraints.size() == 1 &&
            node->genericConstraints()[0].typeConstraints[0]->baseName() == "IValue" &&
            childNamed(node, "Value") != nullptr,
        "TP-PAR-186 class generic scope and constraints must remain exact");
}

void testMinimalAndPartialInterface(TestContext& context)
{
    DeclarationFixture fixture("interface IEmpty { } partial interface IPart { }");
    AstNode* const minimal = declaration(fixture);
    AstNode* const partial = declaration(fixture, 1);
    context.expect(minimal != nullptr && partial != nullptr &&
            minimal->kind() == AstNodeKind::InterfaceDeclaration && !minimal->flag() &&
            partial->kind() == AstNodeKind::InterfaceDeclaration && partial->flag(),
        "TP-PAR-187 interface and partial-interface flags must be exact");
}

void testTraitDeclaration(TestContext& context)
{
    DeclarationFixture fixture("trait IValue { int Get(); }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::InterfaceDeclaration &&
            hasMarker(node, "ns3-trait") && node->children().size() == 1 &&
            node->children()[0]->kind() == AstNodeKind::MethodDeclaration &&
            node->children()[0]->text() == "Get",
        "TP-PAR-188 trait marker and members must be retained on the interface node");
}

void testTraitImplementation(TestContext& context)
{
    DeclarationFixture fixture(
        "trait IValue { int Get(); } class Value { } "
        "impl IValue for Value { int Get() { return 1; } }");
    AstNode* const node = declaration(fixture, 2);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ExtensionDeclaration &&
            node->text() == "Value" && node->typeSyntax() != nullptr &&
            node->typeSyntax()->baseName() == "IValue" && node->second() != nullptr &&
            node->second()->typeSyntax() != nullptr && node->second()->typeSyntax()->baseName() == "Value",
        "TP-PAR-189 impl Trait for Type must use ExtensionDeclaration receiver/base representation");
}

void testAssociatedTypeDeclaration(TestContext& context)
{
    DeclarationFixture fixture("trait IValue { type Item; }");
    AstNode* const trait = declaration(fixture);
    AstNode* const item = childNamed(trait, "Item");
    context.expect(item != nullptr && item->kind() == AstNodeKind::AssociatedTypeDeclaration &&
            item->typeSyntax() == nullptr,
        "TP-PAR-190 associated type declaration must preserve kind, name, and open definition");
}

void testAssociatedConstantDeclaration(TestContext& context)
{
    DeclarationFixture fixture("trait IValue { const int Value = 42; }");
    AstNode* const item = childNamed(declaration(fixture), "Value");
    context.expect(item != nullptr && item->kind() == AstNodeKind::AssociatedConstantDeclaration &&
            item->typeSyntax() != nullptr && item->typeSyntax()->baseName() == "int" &&
            item->first() != nullptr && item->first()->kind() == AstNodeKind::IntegerLiteralExpression &&
            item->first()->integerValue() == 42,
        "TP-PAR-191 associated constant must preserve type, name, and default expression");
}

void testAssociatedImplementations(TestContext& context)
{
    DeclarationFixture fixture(
        "trait IValue { type Item; const int Value; } class Number { } "
        "impl IValue for Number { type Item = int; const int Value = 42; }");
    AstNode* const implementation = declaration(fixture, 2);
    AstNode* const type = childNamed(implementation, "Item");
    AstNode* const constant = childNamed(implementation, "Value");
    context.expect(type != nullptr && type->typeSyntax() != nullptr && type->typeSyntax()->baseName() == "int" &&
            constant != nullptr && constant->typeSyntax() != nullptr && constant->first() != nullptr,
        "TP-PAR-192 associated implementations must provide value/type according to item kind");
}

void testInvalidInterfaceMemberRecovery(TestContext& context)
{
    DeclarationFixture fixture("interface IRecovered { int Field; int Get(); }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && !fixture.diagnostics().empty() && childNamed(node, "Get") != nullptr,
        "TP-PAR-193 invalid interface member must diagnose and recover to the following member");
}

void testMissingBaseAndBrace(TestContext& context)
{
    DeclarationFixture missingBase("class Bad : { } class Tail { }");
    DeclarationFixture missingBrace("class Open { int Value;");
    context.expect(!missingBase.diagnostics().empty() && declaration(missingBase, 1) != nullptr &&
            !missingBrace.diagnostics().empty() && declaration(missingBrace) != nullptr,
        "TP-PAR-194 missing base/brace must diagnose and preserve global progress");
}

void testClassGenericScopeDoesNotLeak(TestContext& context)
{
    DeclarationFixture fixture("class Box<T> { T Value; } int Main() { T * value; return 0; }");
    AstNode* const function = declaration(fixture, 1);
    AstNode* const body = function == nullptr ? nullptr : function->first();
    context.expect(body != nullptr && !body->children().empty() &&
            body->children()[0]->kind() == AstNodeKind::ExpressionStatement,
        "TP-PAR-195 class generic parameter must leave known type scope after the declaration");
}

void testMixedClassMembers(TestContext& context)
{
    DeclarationFixture fixture(
        "class Mixed { int Field; Mixed() { } int Property { get; } int Method() { return Field; } "
        "class Nested { } }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->children().size() == 5 &&
            node->children()[0]->kind() == AstNodeKind::FieldDeclaration &&
            node->children()[1]->kind() == AstNodeKind::ConstructorDeclaration &&
            node->children()[2]->kind() == AstNodeKind::PropertyDeclaration &&
            node->children()[3]->kind() == AstNodeKind::MethodDeclaration &&
            node->children()[4]->kind() == AstNodeKind::ClassDeclaration &&
            node->range().begin().offset() < node->range().end().offset(),
        "TP-PAR-196 mixed valid class members must preserve order and complete ranges");
}
}

void registerParserClassInterfaceModule(TestRegistry& registry)
{
    addDeclarationCase(registry, "TP-PAR-183", "CP-PAR-019", "PR", testMinimalClass);
    addDeclarationCase(registry, "TP-PAR-184", "CP-PAR-019", "PR", testClassFlags);
    addDeclarationCase(registry, "TP-PAR-185", "CP-PAR-019", "PR", testClassBases);
    addDeclarationCase(registry, "TP-PAR-186", "CP-PAR-019", "PR", testGenericClassConstraints);
    addDeclarationCase(registry, "TP-PAR-187", "CP-PAR-019", "PR", testMinimalAndPartialInterface);
    addDeclarationCase(registry, "TP-PAR-188", "CP-PAR-019", "PR", testTraitDeclaration);
    addDeclarationCase(registry, "TP-PAR-189", "CP-PAR-019", "PR", testTraitImplementation);
    addDeclarationCase(registry, "TP-PAR-190", "CP-PAR-019", "PR", testAssociatedTypeDeclaration);
    addDeclarationCase(registry, "TP-PAR-191", "CP-PAR-019", "PR", testAssociatedConstantDeclaration);
    addDeclarationCase(registry, "TP-PAR-192", "CP-PAR-019", "PR", testAssociatedImplementations);
    addDeclarationCase(registry, "TP-PAR-193", "CP-PAR-019", "PR", testInvalidInterfaceMemberRecovery);
    addDeclarationCase(registry, "TP-PAR-194", "CP-PAR-019", "PR", testMissingBaseAndBrace);
    addDeclarationCase(registry, "TP-PAR-195", "CP-PAR-019", "PR", testClassGenericScopeDoesNotLeak);
    addDeclarationCase(registry, "TP-PAR-196", "CP-PAR-019", "NIGHTLY", testMixedClassMembers);
}
}
