#include "ParserCallableTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace parser_callable_support;

AstNode* functionBody(const DeclarationFixture& fixture) noexcept
{
    AstNode* const function = declaration(fixture);
    return function == nullptr ? nullptr : function->first();
}

void testBlocks(TestContext& context)
{
    DeclarationFixture fixture("void Empty() { } void Filled() { 1; 2; 3; }");
    AstNode* const empty = functionBody(fixture);
    AstNode* const filledFunction = declaration(fixture, 1);
    AstNode* const filled = filledFunction == nullptr ? nullptr : filledFunction->first();
    context.expect(empty != nullptr && empty->kind() == AstNodeKind::BlockStatement &&
            empty->children().empty() && empty->range().begin().offset() < empty->range().end().offset() &&
            filled != nullptr && filled->kind() == AstNodeKind::BlockStatement &&
            filled->children().size() == 3 &&
            filled->children()[0]->kind() == AstNodeKind::ExpressionStatement &&
            filled->children()[2]->kind() == AstNodeKind::ExpressionStatement,
        "TP-PAR-297 empty and populated blocks must preserve children and finite source ranges");
}

void testNestedBlocks(TestContext& context)
{
    DeclarationFixture fixture("void F() { { { 1; } 2; } 3; }");
    AstNode* const outer = functionBody(fixture);
    AstNode* const middle = outer != nullptr && !outer->children().empty() ? outer->children()[0] : nullptr;
    AstNode* const inner = middle != nullptr && !middle->children().empty() ? middle->children()[0] : nullptr;
    context.expect(outer != nullptr && outer->children().size() == 2 &&
            middle != nullptr && middle->kind() == AstNodeKind::BlockStatement &&
            middle->children().size() == 2 && inner != nullptr &&
            inner->kind() == AstNodeKind::BlockStatement && inner->children().size() == 1,
        "TP-PAR-298 nested blocks must preserve exact parent/child hierarchy");
}

void testMissingBlockBrace(TestContext& context)
{
    DeclarationFixture fixture("void F() { 1;");
    AstNode* const body = functionBody(fixture);
    context.expect(body != nullptr && body->kind() == AstNodeKind::BlockStatement &&
            body->children().size() == 1 &&
            fixture.diagnosticCount(DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-299 missing block brace must report ExpectedToken and terminate at EOF");
}

struct StatementExpectation final
{
    const char* source;
    AstNodeKind kind;
};

constexpr std::array<StatementExpectation, 24> kStatements{{
    {"if (true) { }", AstNodeKind::IfStatement},
    {"while (true) { }", AstNodeKind::WhileStatement},
    {"do { } while (true);", AstNodeKind::DoWhileStatement},
    {"for (;;) { }", AstNodeKind::ForStatement},
    {"foreach (int item in values) { }", AstNodeKind::ForeachStatement},
    {"switch (0) { default: break; }", AstNodeKind::SwitchStatement},
    {"break;", AstNodeKind::BreakStatement},
    {"continue;", AstNodeKind::ContinueStatement},
    {"return;", AstNodeKind::ReturnStatement},
    {"try { } finally { }", AstNodeKind::TryStatement},
    {"throw 1;", AstNodeKind::ThrowStatement},
    {"using (int resource = 0) { }", AstNodeKind::UsingStatement},
    {"static_assert(true);", AstNodeKind::StaticAssertStatement},
    {"asm { nop }", AstNodeKind::InlineAssemblyStatement},
    {"int value = 0;", AstNodeKind::VariableDeclarationStatement},
    {"value = 1;", AstNodeKind::ExpressionStatement},
    {"fp(strict) { 1; }", AstNodeKind::BlockStatement},
    {"unsafe(\"reason\") { 1; }", AstNodeKind::BlockStatement},
    {"static int value = 0;", AstNodeKind::VariableDeclarationStatement},
    {"var (left, right) = pair;", AstNodeKind::DeconstructionDeclarationStatement},
    {"must_tail return 0;", AstNodeKind::ReturnStatement},
    {"defer [] { }", AstNodeKind::TryStatement},
    {"match (0) { _ => { } }", AstNodeKind::BlockStatement},
    {"yield return 1;", AstNodeKind::BlockStatement},
}};

void testStatementKeywordDispatch(TestContext& context)
{
    for (const StatementExpectation& expectation : kStatements)
    {
        DeclarationFixture fixture(std::string("void F() { ") + expectation.source + " }");
        AstNode* const body = functionBody(fixture);
        AstNode* const statement = body != nullptr && !body->children().empty() ? body->children()[0] : nullptr;
        context.expect(statement != nullptr && statement->kind() == expectation.kind,
            std::string("TP-PAR-300 statement dispatch mismatch for: ") + expectation.source);
    }
}
}

void registerParserBlockStatementModule(TestRegistry& registry)
{
    addCallableCase(registry, "TP-PAR-297", "CP-PAR-028", testBlocks);
    addCallableCase(registry, "TP-PAR-298", "CP-PAR-028", testNestedBlocks);
    addCallableCase(registry, "TP-PAR-299", "CP-PAR-028", testMissingBlockBrace);
    addCallableCase(registry, "TP-PAR-300", "CP-PAR-028", testStatementKeywordDispatch);
}
}
