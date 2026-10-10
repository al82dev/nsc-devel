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
    registry.add(TestCase{std::move(id), "CP-PAR-037", "Host.Parser", "component", "PR",
        {"parser", "expression", "lambda"}, std::move(function), disposition});
}

AstNode* parseExpression(ParserFixture& fixture)
{
    return fixture.parser().parseExpression();
}

void testExpressionLambda(TestContext& context)
{
    ParserFixture fixture("() => 42");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::LambdaExpression &&
            node->children().empty() && !node->flag() && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::IntegerLiteralExpression &&
            node->first()->integerValue() == 42 && fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-445 expression lambda must retain parameter list, expression body, and expression-body flag");
}

void testBlockLambda(TestContext& context)
{
    ParserFixture fixture("() => { return 42; }");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::LambdaExpression && node->flag() &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::BlockStatement &&
            node->first()->children().size() == 1 &&
            node->first()->children()[0]->kind() == AstNodeKind::ReturnStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-446 block lambda must retain an explicit BlockStatement body");
}

void testAutomaticAndTypedParameters(TestContext& context)
{
    ParserFixture fixture("(left, int right, auto inferred) => left + right + inferred");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::LambdaExpression &&
            node->children().size() == 3 && node->children()[0]->text() == "left" &&
            node->children()[0]->typeSyntax() == nullptr && node->children()[1]->text() == "right" &&
            node->children()[1]->typeSyntax() != nullptr &&
            node->children()[1]->typeSyntax()->baseName() == "int" &&
            node->children()[2]->text() == "inferred" && node->children()[2]->typeSyntax() != nullptr &&
            node->children()[2]->typeSyntax()->baseName() == "__lambda_auto_0" &&
            node->genericParameters().size() == 1 &&
            node->genericParameters()[0] == "__lambda_auto_0" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-447 contextual, typed, and auto lambda parameters must retain exact declarations");
}

void testCaptureModes(TestContext& context)
{
    ParserFixture empty("[]() => 0");
    ParserFixture modes("[copy value, ref alias, ref mut mutable, move owner]() => value");
    AstNode* const emptyNode = parseExpression(empty);
    AstNode* const modeNode = parseExpression(modes);
    context.expect(emptyNode != nullptr && emptyNode->auxiliaryText() == "captures:" &&
            modeNode != nullptr &&
            modeNode->auxiliaryText() == "captures:copy=value;ref=alias;mutref=mutable;move=owner" &&
            empty.diagnostics().diagnostics().empty() && modes.diagnostics().diagnostics().empty(),
        "TP-PAR-448 empty, copy, ref, mutable-ref, and move capture modes must retain exact contract text");
}

void testGenericLambdaConstraints(TestContext& context)
{
    ParserFixture fixture("<T>(T value) where T : struct, copy => value");
    AstNode* const node = parseExpression(fixture);
    const GenericConstraintClause* clause = node != nullptr && node->genericConstraints().size() == 1
        ? &node->genericConstraints()[0] : nullptr;
    context.expect(node != nullptr && node->kind() == AstNodeKind::LambdaExpression &&
            node->genericParameters().size() == 1 && node->genericParameters()[0] == "T" &&
            node->children().size() == 1 && node->children()[0]->typeSyntax() != nullptr &&
            node->children()[0]->typeSyntax()->baseName() == "T" && clause != nullptr &&
            clause->parameterName == "T" && clause->requiresValueType && clause->requiresCopy &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-449 generic lambda must retain parameters and exact where constraints");
}

void testExplicitLambdaReturnType(TestContext& context)
{
    ParserFixture fixture("(int value) -> int => value");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::LambdaExpression &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "int" &&
            node->first() != nullptr && node->first()->text() == "value" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-450 explicit lambda return type must retain its TypeSyntax independently from the body");
}
}

void registerParserLambdaContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-445", testExpressionLambda);
    addCase(registry, "TP-PAR-446", testBlockLambda);
    addCase(registry, "TP-PAR-447", testAutomaticAndTypedParameters);
    addCase(registry, "TP-PAR-448", testCaptureModes);
    addCase(registry, "TP-PAR-449", testGenericLambdaConstraints);
    addCase(registry, "TP-PAR-450", testExplicitLambdaReturnType, TestDisposition::KnownProductDefect);
}
}
