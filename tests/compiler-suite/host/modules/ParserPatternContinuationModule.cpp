#include "ParserFoundationTestSupport.h"

#include <array>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-031", "Host.Parser", "component", "PR",
        {"parser", "pattern", "literal"}, std::move(function)});
}

void testWildcardPattern(TestContext& context)
{
    ParserFixture fixture("_");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(pattern.kind == Parser::PatternKind::Wildcard && pattern.name.empty() &&
            pattern.firstValue == nullptr && pattern.children.empty() && pattern.range.isValid() &&
            pattern.range.begin().offset() == 0 && pattern.range.end().offset() == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-349 underscore must produce one exact wildcard pattern without bindings or values");
}

void testLiteralPatterns(TestContext& context)
{
    ParserFixture booleanTrue("true");
    ParserFixture booleanFalse("false");
    ParserFixture integer("42");
    ParserFixture character("'A'");
    ParserFixture string("\"text\"");
    ParserFixture nullValue("null");
    const Parser::Pattern truePattern = booleanTrue.parser().parsePattern(false);
    const Parser::Pattern falsePattern = booleanFalse.parser().parsePattern(false);
    const Parser::Pattern integerPattern = integer.parser().parsePattern(false);
    const Parser::Pattern characterPattern = character.parser().parsePattern(false);
    const Parser::Pattern stringPattern = string.parser().parsePattern(false);
    const Parser::Pattern nullPattern = nullValue.parser().parsePattern(false);
    context.expect(truePattern.kind == Parser::PatternKind::Literal &&
            truePattern.firstValue != nullptr &&
            truePattern.firstValue->kind() == AstNodeKind::BooleanLiteralExpression &&
            truePattern.firstValue->tokenKind() == TokenKind::KwTrue &&
            falsePattern.kind == Parser::PatternKind::Literal &&
            falsePattern.firstValue != nullptr &&
            falsePattern.firstValue->kind() == AstNodeKind::BooleanLiteralExpression &&
            falsePattern.firstValue->tokenKind() == TokenKind::KwFalse,
        "TP-PAR-350 boolean literal patterns must preserve their distinct token values");
    context.expect(integerPattern.kind == Parser::PatternKind::Literal &&
            integerPattern.firstValue != nullptr &&
            integerPattern.firstValue->kind() == AstNodeKind::IntegerLiteralExpression &&
            integerPattern.firstValue->integerValue() == 42 &&
            integerPattern.firstInteger.has_value() && *integerPattern.firstInteger == 42,
        "TP-PAR-350 integer literal pattern must preserve both AST and signed comparison value");
    context.expect(characterPattern.kind == Parser::PatternKind::Literal &&
            characterPattern.firstValue != nullptr &&
            characterPattern.firstValue->kind() == AstNodeKind::CharacterLiteralExpression &&
            characterPattern.firstValue->integerValue() == static_cast<unsigned>('A') &&
            stringPattern.kind == Parser::PatternKind::Literal &&
            stringPattern.firstValue != nullptr &&
            stringPattern.firstValue->kind() == AstNodeKind::StringLiteralExpression &&
            stringPattern.firstValue->text() == "text",
        "TP-PAR-350 character and string literal patterns must retain decoded values");
    context.expect(nullPattern.kind == Parser::PatternKind::Literal &&
            nullPattern.firstValue != nullptr &&
            nullPattern.firstValue->kind() == AstNodeKind::NullLiteralExpression &&
            nullPattern.firstValue->tokenKind() == TokenKind::KwNull &&
            truePattern.range.begin().offset() == 0 && truePattern.range.end().offset() == 4 &&
            integerPattern.range.begin().offset() == 0 && integerPattern.range.end().offset() == 2 &&
            characterPattern.range.end().offset() == 3 && stringPattern.range.end().offset() == 6 &&
            nullPattern.range.end().offset() == 4,
        "TP-PAR-350 every literal pattern must retain its exact source range and node kind");
}
}

void registerParserPatternContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-349", testWildcardPattern);
    addCase(registry, "TP-PAR-350", testLiteralPatterns);
}
}
