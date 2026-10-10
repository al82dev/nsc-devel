#include "ParserFinalTestSupport.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"

#include <string>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;

AstNode* parseExpression(ParserFixture& fixture)
{
    return fixture.parser().parseExpression();
}

void testSimpleRuntimeHole(TestContext& context)
{
    ParserFixture fixture("$\"hello {value}\"");
    AstNode* root = parseExpression(fixture);
    context.expect(root != nullptr && root->kind() == AstNodeKind::BinaryExpression &&
            root->tokenKind() == TokenKind::Plus && firstText(root, "value") != nullptr &&
            countAuxiliary(root, "runtime-interpolation") == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-501 one runtime hole must be re-lexed once and inserted into the lowered AST");
}

void testMultipleRuntimeHoles(TestContext& context)
{
    ParserFixture fixture("$\"A{first}B{second}C\"");
    AstNode* root = parseExpression(fixture);
    const auto nodes = flatten(root);
    std::size_t calls = 0;
    std::size_t literalSegments = 0;
    for (const AstNode* node : nodes)
    {
        calls += node->auxiliaryText() == "runtime-interpolation" ? 1U : 0U;
        literalSegments += node->kind() == AstNodeKind::StringLiteralExpression &&
            (node->text() == "A" || node->text() == "B" || node->text() == "C") ? 1U : 0U;
    }
    context.expect(root != nullptr && calls == 2 && literalSegments == 3 &&
            firstText(root, "first") != nullptr && firstText(root, "second") != nullptr,
        "TP-PAR-502 multiple runtime holes and literal segments must all survive in source order");
}

void testEscapedBraces(TestContext& context)
{
    ParserFixture fixture("$\"{{left}} and {{right}}\"");
    AstNode* root = parseExpression(fixture);
    context.expect(root != nullptr && root->kind() == AstNodeKind::InterpolatedStringExpression &&
            root->integerValue() == 1 && countAuxiliary(root, "runtime-interpolation") == 0 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-503 escaped braces must remain literal content and create no runtime hole");
}

void testCurrentFormatMetadata(TestContext& context)
{
    ParserFixture decimal("$\"{value:D4}\"");
    ParserFixture hexadecimal("$\"{value:X8}\"");
    AstNode* decimalRoot = parseExpression(decimal);
    AstNode* hexRoot = parseExpression(hexadecimal);
    context.expect(firstText(decimalRoot, "ZFill") != nullptr &&
            firstText(decimalRoot, "ToString") != nullptr &&
            firstText(hexRoot, "ToHexString") != nullptr &&
            decimal.diagnostics().diagnostics().empty() && hexadecimal.diagnostics().diagnostics().empty(),
        "TP-PAR-504 the current D-width and X-width formats must select their exact lowering helpers");
}

void testNestedHoleExpression(TestContext& context)
{
    ParserFixture fixture("$\"{choose(flag ? left : right)}\"");
    AstNode* root = parseExpression(fixture);
    const AstNode* conditional = firstKind(root, AstNodeKind::ConditionalExpression);
    context.expect(conditional != nullptr && firstText(conditional, "flag") != nullptr &&
            firstText(conditional, "left") != nullptr && firstText(conditional, "right") != nullptr &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-505 nested delimiters in a hole must produce the exact nested conditional AST");
}

void testLeftoverHoleToken(TestContext& context)
{
    ParserFixture fixture("$\"{value extra}\"");
    AstNode* root = parseExpression(fixture);
    context.expect(root != nullptr &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedExpression) == 1,
        "TP-PAR-506 a hole with leftover tokens must report incomplete expression consumption");
}

void testLexicallyInvalidHole(TestContext& context)
{
    const std::string source = "$\"{@}\"";
    ParserFixture fixture(source);
    AstNode* root = parseExpression(fixture);
    bool translated = false;
    for (const auto& diagnostic : fixture.diagnostics().diagnostics())
    {
        if (diagnostic.range().has_value() &&
            diagnostic.range()->begin().offset() <= source.size() &&
            diagnostic.range()->end().offset() <= source.size()) translated = true;
    }
    context.expect(root != nullptr && fixture.diagnostics().hasErrors() && translated,
        "TP-PAR-507 a lexically invalid hole must retain a diagnostic range in the outer source");
}

void testInvalidInterpolationString(TestContext& context)
{
    ParserFixture fixture("$\"{value\"");
    AstNode* root = parseExpression(fixture);
    context.expect(root != nullptr &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidStringLiteral) >= 1 &&
            !fixture.diagnostics().diagnostics().empty() &&
            fixture.diagnostics().diagnostics().front().range().has_value(),
        "TP-PAR-508 malformed interpolation must report InvalidStringLiteral with a source range");
}

void testUnexpectedPrimaryToken(TestContext& context)
{
    ParserFixture fixture(";");
    AstNode* root = fixture.parser().parsePrimaryExpression();
    context.expect(root != nullptr && root->kind() == AstNodeKind::IdentifierExpression &&
            root->text() == "<error>" &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedExpression) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-509 an unexpected primary token must report ExpectedExpression and return an error node");
}

void testEofPrimary(TestContext& context)
{
    ParserFixture fixture("");
    const std::size_t before = fixture.parser().index_;
    AstNode* root = fixture.parser().parsePrimaryExpression();
    context.expect(root != nullptr && root->text() == "<error>" &&
            fixture.parser().index_ == before && fixture.parser().current().kind() == TokenKind::EndOfFile &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedExpression) == 1,
        "TP-PAR-510 EOF as a primary must report once without advancing beyond the token buffer");
}

void testDeepNestedInterpolation(TestContext& context)
{
    std::string source = "$\"{";
    source.append(32, '(');
    source += "value";
    source.append(32, ')');
    source += "}\"";
    ParserFixture fixture(source);
    AstNode* root = parseExpression(fixture);
    context.expect(root != nullptr && firstText(root, "value") != nullptr &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-511 balanced interpolation at the last directed safe depth must preserve offsets and finish at EOF");
    context.expect(false,
        "TP-PAR-511 PRODUCT-DEFECT: depth 128 terminates with Windows stack overflow (0xC00000FD); process isolation is required to keep the suite alive");
}

void testPrimaryYieldsToPostfix(TestContext& context)
{
    ParserFixture fixture("$\"plain\".Length");
    AstNode* root = fixture.parser().parsePrimaryExpression();
    context.expect(root != nullptr && root->kind() == AstNodeKind::InterpolatedStringExpression &&
            fixture.parser().current().kind() == TokenKind::Dot && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-512 primary parsing must leave a following postfix operator unconsumed");
}
}

void registerParserInterpolationFinalModule(TestRegistry& registry)
{
    addFinalParserCase(registry, "TP-PAR-501", "CP-PAR-040", testSimpleRuntimeHole);
    addFinalParserCase(registry, "TP-PAR-502", "CP-PAR-040", testMultipleRuntimeHoles);
    addFinalParserCase(registry, "TP-PAR-503", "CP-PAR-040", testEscapedBraces);
    addFinalParserCase(registry, "TP-PAR-504", "CP-PAR-040", testCurrentFormatMetadata);
    addFinalParserCase(registry, "TP-PAR-505", "CP-PAR-040", testNestedHoleExpression);
    addFinalParserCase(registry, "TP-PAR-506", "CP-PAR-040", testLeftoverHoleToken);
    addFinalParserCase(registry, "TP-PAR-507", "CP-PAR-040", testLexicallyInvalidHole);
    addFinalParserCase(registry, "TP-PAR-508", "CP-PAR-040", testInvalidInterpolationString);
    addFinalParserCase(registry, "TP-PAR-509", "CP-PAR-040", testUnexpectedPrimaryToken);
    addFinalParserCase(registry, "TP-PAR-510", "CP-PAR-040", testEofPrimary);
    addFinalParserCase(registry, "TP-PAR-511", "CP-PAR-040", testDeepNestedInterpolation,
        "NIGHTLY", TestDisposition::KnownProductDefect, {"known-defect", "stack-overflow"});
    addFinalParserCase(registry, "TP-PAR-512", "CP-PAR-040", testPrimaryYieldsToPostfix);
}
}
