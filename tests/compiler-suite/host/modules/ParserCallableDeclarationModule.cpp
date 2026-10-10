#include "ParserCallableTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace parser_callable_support;

void testMinimalMethod(TestContext& context)
{
    DeclarationFixture fixture("class C { int Read(int value) { return value; } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::MethodDeclaration &&
            node->text() == "Read" && node->typeSyntax() != nullptr &&
            node->typeSyntax()->baseName() == "int" && node->children().size() == 1 &&
            node->children()[0]->text() == "value" && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::BlockStatement,
        "TP-PAR-267 minimal method must preserve return type, name, parameters, and block");
}

void testGenericMethod(TestContext& context)
{
    DeclarationFixture fixture("class C { T Identity<T>(T value) where T : class { return value; } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->genericParameters() == std::vector<std::string>{"T"} &&
            node->children().size() == 1 && node->children()[0]->typeSyntax() != nullptr &&
            node->children()[0]->typeSyntax()->baseName() == "T" &&
            node->genericConstraints().size() == 1 &&
            node->genericConstraints()[0].parameterName == "T" &&
            node->genericConstraints()[0].requiresClass,
        "TP-PAR-268 generic method must preserve parameters and where constraints");
}

void testMethodModifiers(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { static int S() { return 0; } virtual int V() { return 0; } "
        "override int O() { return 0; } abstract int A(); sealed override int Z() { return 0; } "
        "new int N() { return 0; } }");
    AstNode* const type = declaration(fixture);
    context.expect(type != nullptr && type->children().size() == 6 &&
            type->children()[0]->hasModifier(TokenKind::KwStatic) &&
            type->children()[1]->hasModifier(TokenKind::KwVirtual) &&
            type->children()[2]->hasModifier(TokenKind::KwOverride) &&
            type->children()[3]->hasModifier(TokenKind::KwAbstract) &&
            type->children()[4]->hasModifier(TokenKind::KwSealed) &&
            type->children()[4]->hasModifier(TokenKind::KwOverride) &&
            type->children()[5]->hasModifier(TokenKind::KwNew),
        "TP-PAR-269 method modifiers must remain independent exact flags");
}

void testInliningPolicies(TestContext& context)
{
    DeclarationFixture valid(
        "class C { inline int I() { return 1; } forceinline int F() { return 2; } "
        "noinline int N() { return 3; } }");
    DeclarationFixture invalid("class C { inline noinline int Bad() { return 0; } }");
    AstNode* const type = declaration(valid);
    context.expect(type != nullptr && type->children().size() == 3 &&
            type->children()[0]->hasModifier(TokenKind::KwInline) &&
            type->children()[1]->hasModifier(TokenKind::KwForceInline) &&
            type->children()[2]->hasModifier(TokenKind::KwNoInline),
        "TP-PAR-270 each inlining spelling must retain its exact metadata");
    context.expect(diagnosticContains(invalid, "contradictory inlining policies"),
        "TP-PAR-270 mutually exclusive inlining policies must diagnose");
}

void testExportExternC(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { export extern C static int Entry(int value) { return value; } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->hasModifier(TokenKind::KwExport) &&
            node->hasModifier(TokenKind::KwExtern) && node->hasModifier(TokenKind::KwStatic),
        "TP-PAR-271 export extern C method must preserve ABI modifiers independently");
}

void testUnsafeNoHeap(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { unsafe int U() { return 0; } noheap int N() { return 0; } "
        "noheap unsafe int B() { return 0; } }");
    AstNode* const type = declaration(fixture);
    context.expect(type != nullptr && type->children().size() == 3 &&
            type->children()[0]->isUnsafe() && !type->children()[0]->hasModifier(TokenKind::KwNoHeap) &&
            !type->children()[1]->isUnsafe() && type->children()[1]->hasModifier(TokenKind::KwNoHeap) &&
            type->children()[2]->isUnsafe() && type->children()[2]->hasModifier(TokenKind::KwNoHeap),
        "TP-PAR-272 unsafe and noheap must remain orthogonal callable flags");
}

void testMethodBodyForms(TestContext& context)
{
    DeclarationFixture block("class C { int Block() { return 1; } }");
    DeclarationFixture expression("class C { int Expression() => 1; }");
    AstNode* const blockNode = member(block);
    AstNode* const expressionNode = member(expression);
    context.expect(blockNode != nullptr && blockNode->first() != nullptr &&
            blockNode->first()->kind() == AstNodeKind::BlockStatement,
        "TP-PAR-273 block-bodied method must retain a BlockStatement body");
    context.expect(expressionNode != nullptr && expressionNode->first() == nullptr &&
            !expression.diagnostics().empty(),
        "TP-PAR-273 expression-bodied method must retain the current explicit rejection");
}

void testBodylessAllowedContexts(TestContext& context)
{
    DeclarationFixture interfaceFixture("interface I { int Read(); }");
    DeclarationFixture externFixture("extern C { int read(int value); }");
    AstNode* const interfaceMethod = member(interfaceFixture);
    AstNode* const externBlock = declaration(externFixture);
    AstNode* const externFunction = externBlock != nullptr && !externBlock->children().empty()
        ? externBlock->children()[0] : nullptr;
    context.expect(interfaceMethod != nullptr && interfaceMethod->first() == nullptr &&
            interfaceFixture.diagnostics().empty() && externFunction != nullptr &&
            externFunction->first() == nullptr && externFunction->flag() &&
            externFixture.diagnostics().empty(),
        "TP-PAR-274 interface and extern callables must accept null bodies without diagnostics");
}

void testConcreteBodylessMethod(TestContext& context)
{
    DeclarationFixture fixture("class C { int Read(); }");
    context.expect(diagnosticContains(fixture, "non-abstract class methods require a body"),
        "TP-PAR-275 concrete bodyless method must diagnose");
}

void testGlobalFunction(TestContext& context)
{
    DeclarationFixture fixture("int Read(int value) { return value; }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::FunctionDeclaration &&
            node->text() == "Read" && node->typeSyntax() != nullptr &&
            node->typeSyntax()->baseName() == "int" && node->children().size() == 1 &&
            node->first() != nullptr,
        "TP-PAR-276 global function must retain exact declaration shape");
}

void testGlobalVariables(TestContext& context)
{
    DeclarationFixture fixture("int plain; int initialized = 42;");
    AstNode* const plain = declaration(fixture);
    AstNode* const initialized = declaration(fixture, 1);
    context.expect(plain != nullptr && initialized != nullptr &&
            plain->kind() == AstNodeKind::GlobalVariableDeclaration && plain->first() == nullptr &&
            initialized->kind() == AstNodeKind::GlobalVariableDeclaration &&
            initialized->first() != nullptr && initialized->first()->integerValue() == 42,
        "TP-PAR-277 global variables must distinguish absent and present initializers");
}

void testExternBlockOrder(TestContext& context)
{
    DeclarationFixture fixture("extern C { int first(int value); void second(); }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ExternCDeclaration &&
            node->children().size() == 2 && node->children()[0]->text() == "first" &&
            node->children()[1]->text() == "second" && node->children()[0]->flag() &&
            node->children()[1]->flag(),
        "TP-PAR-278 extern C block must preserve bodyless functions and source order");
}

void testMalformedExternRecovery(TestContext& context)
{
    DeclarationFixture fixture("extern C { int broken() { return 0; } int kept(); } int after;");
    AstNode* const block = declaration(fixture);
    AstNode* const after = declaration(fixture, 1);
    context.expect(block != nullptr && block->children().size() == 2 &&
            block->children()[1] != nullptr && block->children()[1]->text() == "kept" &&
            after != nullptr && after->text() == "after" && !fixture.diagnostics().empty(),
        "TP-PAR-279 malformed extern member must diagnose and recover to following declarations");
}

void testFunctionGlobalAmbiguity(TestContext& context)
{
    DeclarationFixture fixture("int callable() { return 1; } int value = 2;");
    AstNode* const callable = declaration(fixture);
    AstNode* const value = declaration(fixture, 1);
    context.expect(callable != nullptr && callable->kind() == AstNodeKind::FunctionDeclaration &&
            value != nullptr && value->kind() == AstNodeKind::GlobalVariableDeclaration &&
            callable->text() == "callable" && value->text() == "value",
        "TP-PAR-280 shared type/name prefix must dispatch without cursor loss");
}

void testIncompatibleModifiers(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { abstract sealed int A(); inline forceinline int B() { return 0; } }");
    context.expect(diagnosticContains(fixture, "cannot be both abstract and sealed") &&
            diagnosticContains(fixture, "contradictory inlining policies"),
        "TP-PAR-281 incompatible method modifiers must produce their exact diagnostics");
}

void testCallableThenGlobalRanges(TestContext& context)
{
    DeclarationFixture fixture("int callable() { return 1; } int value;");
    AstNode* const callable = declaration(fixture);
    AstNode* const value = declaration(fixture, 1);
    context.expect(callable != nullptr && value != nullptr &&
            callable->range().end().offset() <= value->range().begin().offset() &&
            callable->kind() == AstNodeKind::FunctionDeclaration &&
            value->kind() == AstNodeKind::GlobalVariableDeclaration,
        "TP-PAR-282 callable and following global must have separated ordered ranges");
}
}

void registerParserCallableDeclarationModule(TestRegistry& registry)
{
    addCallableCase(registry, "TP-PAR-267", "CP-PAR-026", testMinimalMethod);
    addCallableCase(registry, "TP-PAR-268", "CP-PAR-026", testGenericMethod);
    addCallableCase(registry, "TP-PAR-269", "CP-PAR-026", testMethodModifiers);
    addCallableCase(registry, "TP-PAR-270", "CP-PAR-026", testInliningPolicies);
    addCallableCase(registry, "TP-PAR-271", "CP-PAR-026", testExportExternC);
    addCallableCase(registry, "TP-PAR-272", "CP-PAR-026", testUnsafeNoHeap);
    addCallableCase(registry, "TP-PAR-273", "CP-PAR-026", testMethodBodyForms);
    addCallableCase(registry, "TP-PAR-274", "CP-PAR-026", testBodylessAllowedContexts);
    addCallableCase(registry, "TP-PAR-275", "CP-PAR-026", testConcreteBodylessMethod);
    addCallableCase(registry, "TP-PAR-276", "CP-PAR-026", testGlobalFunction);
    addCallableCase(registry, "TP-PAR-277", "CP-PAR-026", testGlobalVariables);
    addCallableCase(registry, "TP-PAR-278", "CP-PAR-026", testExternBlockOrder);
    addCallableCase(registry, "TP-PAR-279", "CP-PAR-026", testMalformedExternRecovery);
    addCallableCase(registry, "TP-PAR-280", "CP-PAR-026", testFunctionGlobalAmbiguity);
    addCallableCase(registry, "TP-PAR-281", "CP-PAR-026", testIncompatibleModifiers);
    addCallableCase(registry, "TP-PAR-282", "CP-PAR-026", testCallableThenGlobalRanges);
}
}
