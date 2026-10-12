#include "ParserFoundationTestSupport.h"
#include "HIRContractSupport.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(TestRegistry& registry, std::string id, TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-028", "Host.Parser", "component", "PR",
        {"parser", "statement", "declaration"}, std::move(function), disposition});
}

AstNode* parseStatement(ParserFixture& fixture)
{
    return fixture.parser().parseStatement();
}

void testExpressionStatement(TestContext& context)
{
    ParserFixture fixture("value + 1;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ExpressionStatement &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::BinaryExpression &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-301 expression followed by semicolon must become one complete expression statement");
}

void testMissingExpressionTerminator(TestContext& context)
{
    ParserFixture fixture("value + 1");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ExpressionStatement &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-302 missing expression terminator must diagnose once and stop at EOF");
}

void testExplicitVariableInitializer(TestContext& context)
{
    ParserFixture fixture("int answer = 42;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->text() == "answer" && node->typeSyntax() != nullptr &&
            node->typeSyntax()->baseName() == "int" && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::IntegerLiteralExpression &&
            node->first()->integerValue() == 42 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-303 explicit local must retain its name, type, and initializer");
}

void testVariableWithoutInitializer(TestContext& context)
{
    ParserFixture fixture("int value;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->first() == nullptr && node->range().isValid() &&
            node->range().begin().offset() == 0 && node->range().end().offset() == 10 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-304 an uninitialized explicit local must retain a null initializer and full range");
}

void testInferredConstAndRaiiFlags(TestContext& context)
{
    ParserFixture inferred("var inferred = 1;");
    ParserFixture constant("const int constant = 2;");
    ParserFixture raii("raii Resource owner = source;", {"Resource"});
    AstNode* const inferredNode = parseStatement(inferred);
    AstNode* const constantNode = parseStatement(constant);
    AstNode* const raiiNode = parseStatement(raii);
    context.expect(inferredNode != nullptr && inferredNode->typeSyntax() != nullptr &&
            inferredNode->typeSyntax()->baseName() == "var" && !inferredNode->flag() &&
            constantNode != nullptr && constantNode->flag() &&
            !constantNode->hasModifier(TokenKind::KwRaii) && raiiNode != nullptr &&
            !raiiNode->flag() && raiiNode->hasModifier(TokenKind::KwRaii) &&
            inferred.diagnostics().diagnostics().empty() &&
            constant.diagnostics().diagnostics().empty() && raii.diagnostics().diagnostics().empty(),
        "TP-PAR-305 inferred, const, and RAII locals must retain independent syntax state");
}

void testMultipleDeclarators(TestContext& context)
{
    ParserFixture fixture("int first = 1, second = 2;");
    AstNode* const declarationList = parseStatement(fixture);
    AstNode* const first = declarationList != nullptr && declarationList->children().size() == 2
        ? declarationList->children()[0] : nullptr;
    AstNode* const second = declarationList != nullptr && declarationList->children().size() == 2
        ? declarationList->children()[1] : nullptr;
    context.expect(declarationList != nullptr && declarationList->kind() == AstNodeKind::BlockStatement &&
            declarationList->auxiliaryText() == "declaration-list" && first != nullptr &&
            first->kind() == AstNodeKind::VariableDeclarationStatement &&
            first->text() == "first" && first->typeSyntax() != nullptr &&
            first->typeSyntax()->baseName() == "int" && first->first() != nullptr &&
            first->first()->kind() == AstNodeKind::IntegerLiteralExpression &&
            first->first()->integerValue() == 1 && second != nullptr &&
            second->kind() == AstNodeKind::VariableDeclarationStatement &&
            second->text() == "second" && second->typeSyntax() != nullptr &&
            second->typeSyntax()->baseName() == "int" && second->first() != nullptr &&
            second->first()->kind() == AstNodeKind::IntegerLiteralExpression &&
            second->first()->integerValue() == 2 &&
            first->range().begin().offset() < second->range().begin().offset() &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-306 one declaration must preserve multiple declarators in source order");

    auto compilation = hir_contract_support::compileHir(
        context,
        "TP-PAR-306",
        "int F(){int first=1, second=2;return first+second;}");
    context.expect(
        hir_contract_support::countKind(
            compilation->hirRoot(),
            nativesharp::hir::HirNodeKind::LocalVariableDeclaration) == 2,
        "TP-PAR-306 both declarators must reach HIR as independent local declarations");
}

void testTupleDeconstruction(TestContext& context)
{
    ParserFixture fixture("var (left, right) = pair;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::DeconstructionDeclarationStatement &&
            node->children().size() == 2 && node->children()[0]->text() == "left" &&
            node->children()[1]->text() == "right" && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::IdentifierExpression &&
            node->first()->text() == "pair" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-307 tuple deconstruction must retain its two bindings and source expression");
}

void testNestedDiscardDeconstruction(TestContext& context)
{
    ParserFixture fixture("var (head, (_, tail)) = value;");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const nested = node != nullptr && node->children().size() == 2
        ? node->children()[1] : nullptr;
    context.expect(node != nullptr && node->kind() == AstNodeKind::DeconstructionDeclarationStatement &&
            node->children().size() == 2 && node->children()[0]->text() == "head" &&
            nested != nullptr && nested->kind() == AstNodeKind::TupleExpression &&
            nested->children().size() == 2 && nested->children()[0]->text() == "_" &&
            nested->children()[1]->text() == "tail" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-308 nested deconstruction must retain nesting and the explicit discard spelling");
}

void testDeconstructionBindingModes(TestContext& context)
{
    ParserFixture fixture("var (ref left, move right, value) = source;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::DeconstructionDeclarationStatement &&
            node->children().size() == 3 && node->children()[0]->auxiliaryText() == "ref" &&
            node->children()[1]->auxiliaryText() == "move" &&
            node->children()[2]->auxiliaryText() == "value" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-309 deconstruction must preserve ref, move, and value binding modes");
}

void testMalformedDeconstructionProgress(TestContext& context)
{
    ParserFixture fixture("{ var (, item) = ; next(); }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const following = block != nullptr && !block->children().empty()
        ? block->children().back() : nullptr;
    const AstNode* const call = following != nullptr ? following->first() : nullptr;
    context.expect(block != nullptr && block->kind() == AstNodeKind::BlockStatement &&
            fixture.diagnostics().hasErrors() && following != nullptr &&
            following->kind() == AstNodeKind::ExpressionStatement && call != nullptr &&
            call->kind() == AstNodeKind::CallExpression && call->first() != nullptr &&
            call->first()->text() == "next" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-310 malformed deconstruction must diagnose and preserve progress to the next statement");
}

void testPositionalAggregate(TestContext& context)
{
    ParserFixture fixture("Vector value = {1, 2, 3};", {"Vector"});
    AstNode* const node = parseStatement(fixture);
    const AstNode* const aggregate = node != nullptr ? node->first() : nullptr;
    context.expect(aggregate != nullptr && aggregate->kind() == AstNodeKind::AggregateInitializerExpression &&
            aggregate->children().size() == 3 && aggregate->children()[0]->integerValue() == 1 &&
            aggregate->children()[1]->integerValue() == 2 &&
            aggregate->children()[2]->integerValue() == 3 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-311 positional aggregate elements must remain in source order");
}

void testDesignatedAggregate(TestContext& context)
{
    ParserFixture fixture("Vector value = {x = 1, y = 2};", {"Vector"});
    AstNode* const node = parseStatement(fixture);
    const AstNode* const aggregate = node != nullptr ? node->first() : nullptr;
    context.expect(aggregate != nullptr && aggregate->children().size() == 2 &&
            aggregate->children()[0]->argumentName() == "x" &&
            aggregate->children()[1]->argumentName() == "y" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-312 designated aggregate elements must retain exact member names");
}

void testAggregateSpread(TestContext& context)
{
    ParserFixture fixture("Vector value = {1, ..other};", {"Vector"});
    AstNode* const node = parseStatement(fixture);
    const AstNode* const aggregate = node != nullptr ? node->first() : nullptr;
    const AstNode* const spread = aggregate != nullptr && aggregate->children().size() == 2
        ? aggregate->children()[1] : nullptr;
    context.expect(spread != nullptr && spread->kind() == AstNodeKind::SpreadElementExpression &&
            spread->tokenKind() == TokenKind::DotDot && spread->first() != nullptr &&
            spread->first()->kind() == AstNodeKind::IdentifierExpression &&
            spread->first()->text() == "other" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-313 aggregate spread must retain its marker and source expression");
}

void testRecoveryPreservesAdjacentStatements(TestContext& context)
{
    ParserFixture fixture("{ first(); @; second(); }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const firstCall = block != nullptr && !block->children().empty()
        ? block->children().front()->first() : nullptr;
    const AstNode* const secondCall = block != nullptr && !block->children().empty()
        ? block->children().back()->first() : nullptr;
    context.expect(block != nullptr && block->kind() == AstNodeKind::BlockStatement &&
            block->children().size() >= 2 && block->children().front()->kind() == AstNodeKind::ExpressionStatement &&
            block->children().back()->kind() == AstNodeKind::ExpressionStatement &&
            firstCall != nullptr && firstCall->kind() == AstNodeKind::CallExpression &&
            firstCall->first() != nullptr && firstCall->first()->text() == "first" &&
            secondCall != nullptr && secondCall->kind() == AstNodeKind::CallExpression &&
            secondCall->first() != nullptr && secondCall->first()->text() == "second" &&
            fixture.diagnostics().hasErrors(),
        "TP-PAR-314 a malformed middle statement must not erase either valid neighbor");
}
}

void registerParserStatementContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-301", testExpressionStatement);
    addCase(registry, "TP-PAR-302", testMissingExpressionTerminator);
    addCase(registry, "TP-PAR-303", testExplicitVariableInitializer);
    addCase(registry, "TP-PAR-304", testVariableWithoutInitializer);
    addCase(registry, "TP-PAR-305", testInferredConstAndRaiiFlags);
    addCase(registry, "TP-PAR-306", testMultipleDeclarators);
    addCase(registry, "TP-PAR-307", testTupleDeconstruction);
    addCase(registry, "TP-PAR-308", testNestedDiscardDeconstruction);
    addCase(registry, "TP-PAR-309", testDeconstructionBindingModes);
    addCase(registry, "TP-PAR-310", testMalformedDeconstructionProgress);
    addCase(registry, "TP-PAR-311", testPositionalAggregate);
    addCase(registry, "TP-PAR-312", testDesignatedAggregate);
    addCase(registry, "TP-PAR-313", testAggregateSpread);
    addCase(registry, "TP-PAR-314", testRecoveryPreservesAdjacentStatements);
}
}
