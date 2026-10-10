#include "ParserFoundationTestSupport.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;

void addCase(TestRegistry& registry, std::string id, TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-034", "Host.Parser", "component", "PR",
        {"parser", "statement", "foreach"}, std::move(function), disposition});
}

AstNode* parseStatement(ParserFixture& fixture)
{
    return fixture.parser().parseStatement();
}

const AstNode* arrayElement(const AstNode* foreachNode)
{
    const AstNode* candidate = foreachNode != nullptr ? foreachNode->second() : nullptr;
    const AstNode* loop = candidate != nullptr && candidate->children().size() == 1
        ? candidate->children()[0] : nullptr;
    const AstNode* body = loop != nullptr && loop->children().size() == 1
        ? loop->children()[0] : nullptr;
    return body != nullptr && !body->children().empty() ? body->children()[0] : nullptr;
}

const AstNode* arrayUserBody(const AstNode* foreachNode)
{
    const AstNode* candidate = foreachNode != nullptr ? foreachNode->second() : nullptr;
    const AstNode* loop = candidate != nullptr && candidate->children().size() == 1
        ? candidate->children()[0] : nullptr;
    const AstNode* body = loop != nullptr && loop->children().size() == 1
        ? loop->children()[0] : nullptr;
    return body != nullptr && body->children().size() >= 2 ? body->children()[1] : nullptr;
}

void testCollectionEvaluatedOnce(TestContext& context)
{
    ParserFixture fixture("foreach (var item in MakeCollection()) { consume(item); }");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const sourceDeclaration = node != nullptr ? node->first() : nullptr;
    const AstNode* const collection = sourceDeclaration != nullptr ? sourceDeclaration->first() : nullptr;
    const AstNode* const probe = node != nullptr && !node->children().empty() ? node->children()[0] : nullptr;
    context.expect(node != nullptr && node->kind() == AstNodeKind::ForeachStatement &&
            sourceDeclaration != nullptr && sourceDeclaration->text() == "$__foreach_source" &&
            collection != nullptr && collection->kind() == AstNodeKind::CallExpression &&
            collection->first() != nullptr && collection->first()->text() == "MakeCollection" &&
            probe != nullptr && probe->kind() == AstNodeKind::CallExpression && probe != collection &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-401 executable collection must be owned only by the source declaration; its distinct probe is non-lowered");
}

void testEmptyAndNestedBodyPreserved(TestContext& context)
{
    ParserFixture empty("foreach (var item in values) {}");
    ParserFixture nested("foreach (var item in values) { if (item) { continue; } }");
    AstNode* const emptyNode = parseStatement(empty);
    AstNode* const nestedNode = parseStatement(nested);
    const AstNode* const emptyBody = arrayUserBody(emptyNode);
    const AstNode* const nestedBody = arrayUserBody(nestedNode);
    context.expect(emptyBody != nullptr && emptyBody->kind() == AstNodeKind::BlockStatement &&
            emptyBody->children().empty() && nestedBody != nullptr &&
            nestedBody->kind() == AstNodeKind::BlockStatement && nestedBody->children().size() == 1 &&
            nestedBody->children()[0]->kind() == AstNodeKind::IfStatement &&
            empty.diagnostics().diagnostics().empty() && nested.diagnostics().diagnostics().empty(),
        "TP-PAR-402 foreach lowering must preserve empty and nested-control bodies exactly");
}

void testExplicitAndInferredElementTypes(TestContext& context)
{
    ParserFixture explicitType("foreach (int item in values) {}");
    ParserFixture inferredType("foreach (var item in values) {}");
    const AstNode* const explicitElement = arrayElement(parseStatement(explicitType));
    const AstNode* const inferredElement = arrayElement(parseStatement(inferredType));
    context.expect(explicitElement != nullptr && explicitElement->text() == "item" &&
            explicitElement->typeSyntax() != nullptr && explicitElement->typeSyntax()->baseName() == "int" &&
            explicitElement->flag() && inferredElement != nullptr && inferredElement->text() == "item" &&
            inferredElement->typeSyntax() != nullptr && inferredElement->typeSyntax()->baseName() == "var" &&
            inferredElement->flag() && explicitType.diagnostics().diagnostics().empty() &&
            inferredType.diagnostics().diagnostics().empty(),
        "TP-PAR-403 explicit and inferred foreach bindings must retain their exact declaration forms");
}

void testInvalidSourceRecovery(TestContext& context)
{
    ParserFixture fixture("{ foreach (var item in ) {} return 1; }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const following = block != nullptr && !block->children().empty()
        ? block->children().back() : nullptr;
    context.expect(block != nullptr && block->kind() == AstNodeKind::BlockStatement &&
            fixture.diagnostics().hasErrors() && following != nullptr &&
            following->kind() == AstNodeKind::ReturnStatement &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-404 invalid foreach source must diagnose and preserve the following statement");
}

void testSequentialForeachTemporaries(TestContext& context)
{
    ParserFixture fixture("{ foreach (var left in first) {} foreach (var right in second) {} }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const first = block != nullptr && block->children().size() == 2 ? block->children()[0] : nullptr;
    const AstNode* const second = block != nullptr && block->children().size() == 2 ? block->children()[1] : nullptr;
    context.expect(first != nullptr && second != nullptr && first->first() != nullptr &&
            second->first() != nullptr && first->first()->text() != second->first()->text() &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-405 sequential foreach statements must receive distinct synthetic temporary names");
}

void testNestedForeachTemporaries(TestContext& context)
{
    ParserFixture fixture("foreach (var outer in first) { foreach (var inner in second) {} }");
    AstNode* const outer = parseStatement(fixture);
    const AstNode* const outerBody = arrayUserBody(outer);
    const AstNode* const inner = outerBody != nullptr && outerBody->children().size() == 1
        ? outerBody->children()[0] : nullptr;
    context.expect(outer != nullptr && inner != nullptr && inner->kind() == AstNodeKind::ForeachStatement &&
            outer->first() != nullptr && inner->first() != nullptr &&
            outer->first()->text() != inner->first()->text() && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-406 nested foreach statements must not collide in synthetic temporary identity");
}

void testForeachHelperShapes(TestContext& context)
{
    ParserFixture fixture("foreach (var item in values) {}");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const arrayCandidate = node != nullptr ? node->second() : nullptr;
    const AstNode* const loop = arrayCandidate != nullptr && arrayCandidate->children().size() == 1
        ? arrayCandidate->children()[0] : nullptr;
    const AstNode* const element = arrayElement(node);
    context.expect(loop != nullptr && loop->kind() == AstNodeKind::ForStatement &&
            loop->second() != nullptr && loop->second()->kind() == AstNodeKind::BinaryExpression &&
            loop->second()->tokenKind() == TokenKind::Less && loop->third() != nullptr &&
            loop->third()->kind() == AstNodeKind::AssignmentExpression && element != nullptr &&
            element->first() != nullptr && element->first()->kind() == AstNodeKind::IndexExpression &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-407 foreach helpers must use canonical member, index, binary, and assignment forms");
}

void testDescendingRangeStep(TestContext& context)
{
    ParserFixture fixture("foreach (int value in 10..0 step -2) {}");
    AstNode* const lowered = parseStatement(fixture);
    const AstNode* const loop = lowered != nullptr && lowered->children().size() == 5
        ? lowered->children()[4] : nullptr;
    context.expect(lowered != nullptr && lowered->kind() == AstNodeKind::BlockStatement &&
            lowered->auxiliaryText() == "foreach:integer-range-exclusive" &&
            lowered->children().size() == 5 && loop != nullptr && loop->kind() == AstNodeKind::ForStatement &&
            lowered->children()[2]->first() != nullptr &&
            lowered->children()[2]->first()->kind() == AstNodeKind::UnaryExpression &&
            lowered->children()[2]->first()->tokenKind() == TokenKind::Minus &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-408 descending stepped range must preserve negative step and range-loop contract");
}

void testProtocolCandidateOrder(TestContext& context)
{
    ParserFixture fixture("foreach (var item in values) {}");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const arrayCandidate = node != nullptr ? node->second() : nullptr;
    const AstNode* const protocolCandidate = node != nullptr ? node->third() : nullptr;
    const AstNode* const spanCandidate = node != nullptr && node->children().size() == 2
        ? node->children()[1] : nullptr;
    context.expect(node != nullptr && arrayCandidate != nullptr && protocolCandidate != nullptr &&
            spanCandidate != nullptr && arrayCandidate->children().size() == 1 &&
            arrayCandidate->children()[0]->kind() == AstNodeKind::ForStatement &&
            protocolCandidate->children().size() == 2 &&
            protocolCandidate->children()[0]->auxiliaryText() == "local:foreach-enumerator" &&
            protocolCandidate->children()[1]->kind() == AstNodeKind::WhileStatement &&
            spanCandidate->children().size() == 1 &&
            spanCandidate->children()[0]->kind() == AstNodeKind::ForStatement,
        "TP-PAR-409 foreach must expose array, enumerator, and span candidates in contractual slots");
}

void testUnknownIterablePreserved(TestContext& context)
{
    ParserFixture fixture("foreach (var item in mystery) {}");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const source = node != nullptr && node->first() != nullptr ? node->first()->first() : nullptr;
    context.expect(node != nullptr && node->kind() == AstNodeKind::ForeachStatement &&
            source != nullptr && source->kind() == AstNodeKind::IdentifierExpression &&
            source->text() == "mystery" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-410 syntactically unknown iterable must remain intact for semantic protocol selection");
}
}

void registerParserForeachContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-401", testCollectionEvaluatedOnce);
    addCase(registry, "TP-PAR-402", testEmptyAndNestedBodyPreserved);
    addCase(registry, "TP-PAR-403", testExplicitAndInferredElementTypes);
    addCase(registry, "TP-PAR-404", testInvalidSourceRecovery);
    addCase(registry, "TP-PAR-405", testSequentialForeachTemporaries, TestDisposition::KnownProductDefect);
    addCase(registry, "TP-PAR-406", testNestedForeachTemporaries, TestDisposition::KnownProductDefect);
    addCase(registry, "TP-PAR-407", testForeachHelperShapes);
    addCase(registry, "TP-PAR-408", testDescendingRangeStep);
    addCase(registry, "TP-PAR-409", testProtocolCandidateOrder);
    addCase(registry, "TP-PAR-410", testUnknownIterablePreserved);
}
}
