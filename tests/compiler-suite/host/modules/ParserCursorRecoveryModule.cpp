#include "ParserFoundationTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNodeKind;

void testEmptyCompilationUnit(TestContext& context)
{
    ParserFixture fixture("");
    AstNode* const root = fixture.parse();
    context.expect(root != nullptr && root->kind() == AstNodeKind::CompilationUnit &&
            root->children().empty(),
        "TP-PAR-001 empty source must produce an empty CompilationUnit");
    context.expect(fixture.parser().index_ == 0 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            root->range().begin() == fixture.tokens().back().range().begin() &&
            root->range().end() == fixture.tokens().back().range().end(),
        "TP-PAR-001 cursor and range must remain at stable EOF");
}

void testSingleDeclarationAndEofRange(TestContext& context)
{
    ParserFixture fixture("int value;");
    AstNode* const root = fixture.parse();
    context.expect(root->children().size() == 1 &&
            root->children()[0]->kind() == AstNodeKind::GlobalVariableDeclaration,
        "TP-PAR-002 one declaration must produce exactly one child");
    context.expect(root->range().begin() == fixture.tokens().front().range().begin() &&
            root->range().end() == fixture.tokens().back().range().end(),
        "TP-PAR-002 unit range must span first token through EOF");
}

void testTwoDeclarationOrderAndRanges(TestContext& context)
{
    ParserFixture fixture("int first; int second;");
    AstNode* const root = fixture.parse();
    context.expect(root->children().size() == 2 && root->children()[0]->text() == "first" &&
            root->children()[1]->text() == "second",
        "TP-PAR-003 two declarations must retain source order");
    context.expect(root->children().size() == 2 &&
            root->children()[0]->range().end().offset() <= root->children()[1]->range().begin().offset(),
        "TP-PAR-003 declaration ranges must retain source order");
}

void testPeekSaturatesAtEof(TestContext& context)
{
    ParserFixture fixture("int value;");
    fixture.parser().index_ = fixture.tokens().size() - 1;
    const Token& eof = fixture.parser().current();
    context.expect(&fixture.parser().peek(1) == &eof && &fixture.parser().peek(1000) == &eof,
        "TP-PAR-004 peek beyond EOF must reference the final token without advancing");
    context.expect(fixture.parser().index_ == fixture.tokens().size() - 1,
        "TP-PAR-004 peek must not change cursor state");
}

void testConsumeSaturatesAtEof(TestContext& context)
{
    ParserFixture fixture("");
    const std::size_t eofIndex = fixture.parser().index_;
    const Token& first = fixture.parser().consume();
    const Token& second = fixture.parser().consume();
    context.expect(first.kind() == TokenKind::EndOfFile && &first == &second &&
            fixture.parser().index_ == eofIndex,
        "TP-PAR-005 repeated consume at EOF must not pass the final token");
}

void testCompilationUnitProgressGuard(TestContext& context)
{
    ParserFixture fixture("}");
    AstNode* const root = fixture.parse();
    context.expect(root->children().empty() &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-006 no-progress declaration path must discard one token and terminate");
}

void testKnownTypeSeedAndBuiltins(TestContext& context)
{
    ParserFixture fixture("", {"External", "External", ""});
    constexpr std::array<std::string_view, 19> seededTypeNames = {
        "void", "bool", "byte", "char", "short", "ushort", "int", "uint",
        "long", "ulong", "nint", "nuint", "float", "double", "string", "object",
        "Future", "Iterator", "AsyncIterator",
    };
    bool containsAll = true;
    for (const std::string_view typeName : seededTypeNames)
    {
        containsAll = containsAll && fixture.parser().knownTypeNames_.contains(std::string(typeName));
    }
    context.expect(containsAll && fixture.parser().knownTypeNames_.contains("External") &&
            !fixture.parser().knownTypeNames_.contains("") &&
            fixture.parser().knownTypeNames_.size() == seededTypeNames.size() + 1,
        "TP-PAR-007 seed must add one unique nonempty name beside all nineteen default type names");
}

void testSuccessfulExpectConsumes(TestContext& context)
{
    ParserFixture fixture(";");
    const bool result = fixture.parser().expect(TokenKind::Semicolon, "';'");
    context.expect(result && fixture.parser().index_ == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-008 successful expect must consume one token without diagnostics");
}

void testFailedExpectPreservesCursor(TestContext& context)
{
    ParserFixture fixture(";");
    const auto range = fixture.parser().current().range();
    const bool result = fixture.parser().expect(TokenKind::RightBrace, "'}'");
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    context.expect(!result && fixture.parser().index_ == 0 &&
            fixture.parser().current().kind() == TokenKind::Semicolon,
        "TP-PAR-009 failed expect must preserve the current token");
    context.expect(diagnostics.size() == 1 && diagnostics[0].id() == DiagnosticId::ExpectedToken &&
            diagnostics[0].range().has_value() &&
            diagnostics[0].range()->begin() == range.begin() &&
            diagnostics[0].range()->end() == range.end(),
        "TP-PAR-009 failed expect must report ExpectedToken at current range");
}

void testContextualKeywordExactLexeme(TestContext& context)
{
    ParserFixture success("where other");
    const bool matched = success.parser().expectContextualKeyword("where", "'where'");
    ParserFixture failure("Where other");
    const bool rejected = failure.parser().expectContextualKeyword("where", "'where'");
    context.expect(matched && success.parser().index_ == 1 &&
            !rejected && failure.parser().index_ == 0 &&
            diagnosticCount(failure.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-010 contextual keyword matching must use exact Identifier lexeme");
}

void testPrimaryExpressionRecovery(TestContext& context)
{
    ParserFixture fixture(";");
    AstNode* const error = fixture.parser().parsePrimaryExpression();
    context.expect(error != nullptr && error->kind() == AstNodeKind::IdentifierExpression &&
            error->text() == "<error>" && fixture.parser().index_ == 1,
        "TP-PAR-011 invalid primary must produce ErrorExpression form and advance");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedExpression) == 1,
        "TP-PAR-011 invalid primary must report ExpectedExpression exactly once");
}

void testGlobalSynchronizationAfterSemicolon(TestContext& context)
{
    ParserFixture fixture("junk more ; int value;");
    fixture.parser().synchronizeGlobal();
    context.expect(fixture.parser().current().kind() == TokenKind::KwInt &&
            fixture.parser().index_ == 3,
        "TP-PAR-012 global recovery must stop immediately after consuming semicolon");
}

void testGlobalSynchronizationPreservesRightBrace(TestContext& context)
{
    ParserFixture fixture("junk more }");
    fixture.parser().synchronizeGlobal();
    context.expect(fixture.parser().current().kind() == TokenKind::RightBrace &&
            fixture.parser().index_ == 2,
        "TP-PAR-013 global recovery must preserve right brace for its caller");
}

void testGlobalSynchronizationPreservesDeclarationStart(TestContext& context)
{
    ParserFixture fixture("junk int value;");
    fixture.parser().synchronizeGlobal();
    context.expect(fixture.parser().current().kind() == TokenKind::KwInt &&
            fixture.parser().index_ == 1,
        "TP-PAR-014 global recovery must preserve a recoverable declaration start");
}

void testStatementSynchronizationConsumesSemicolon(TestContext& context)
{
    ParserFixture fixture("junk more ; }");
    fixture.parser().synchronizeStatement();
    context.expect(fixture.parser().current().kind() == TokenKind::RightBrace &&
            fixture.parser().index_ == 3,
        "TP-PAR-015 statement recovery must consume its terminator and return");
}

void testStatementSynchronizationPreservesRightBrace(TestContext& context)
{
    ParserFixture fixture("junk more }");
    fixture.parser().synchronizeStatement();
    context.expect(fixture.parser().current().kind() == TokenKind::RightBrace &&
            fixture.parser().index_ == 2,
        "TP-PAR-016 statement recovery must preserve closing brace");
}

void testConsecutiveErrorsAlwaysProgress(TestContext& context)
{
    ParserFixture fixture("} } }");
    AstNode* const root = fixture.parse();
    context.expect(root->children().empty() &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.parser().index_ == fixture.tokens().size() - 1,
        "TP-PAR-017 consecutive errors must make finite progress to EOF");
    context.expect(fixture.diagnostics().diagnostics().size() <= 3,
        "TP-PAR-017 diagnostics must remain finite and bounded by bad tokens");
}
}

void registerParserCursorRecoveryModule(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-001", "CP-PAR-001", "component", "PR", testEmptyCompilationUnit);
    addParserCase(registry, "TP-PAR-002", "CP-PAR-001", "component", "PR", testSingleDeclarationAndEofRange);
    addParserCase(registry, "TP-PAR-003", "CP-PAR-001", "component", "PR", testTwoDeclarationOrderAndRanges);
    addParserCase(registry, "TP-PAR-004", "CP-PAR-001", "component", "PR", testPeekSaturatesAtEof);
    addParserCase(registry, "TP-PAR-005", "CP-PAR-001", "component", "PR", testConsumeSaturatesAtEof);
    addParserCase(registry, "TP-PAR-006", "CP-PAR-001", "component", "PR", testCompilationUnitProgressGuard);
    addParserCase(registry, "TP-PAR-007", "CP-PAR-001", "component", "PR", testKnownTypeSeedAndBuiltins);
    addParserCase(registry, "TP-PAR-008", "CP-PAR-002", "component", "PR", testSuccessfulExpectConsumes);
    addParserCase(registry, "TP-PAR-009", "CP-PAR-002", "component", "PR", testFailedExpectPreservesCursor);
    addParserCase(registry, "TP-PAR-010", "CP-PAR-002", "component", "PR", testContextualKeywordExactLexeme);
    addParserCase(registry, "TP-PAR-011", "CP-PAR-002", "component", "PR", testPrimaryExpressionRecovery);
    addParserCase(registry, "TP-PAR-012", "CP-PAR-002", "component", "PR",
        testGlobalSynchronizationAfterSemicolon);
    addParserCase(registry, "TP-PAR-013", "CP-PAR-002", "component", "PR",
        testGlobalSynchronizationPreservesRightBrace);
    addParserCase(registry, "TP-PAR-014", "CP-PAR-002", "component", "PR",
        testGlobalSynchronizationPreservesDeclarationStart);
    addParserCase(registry, "TP-PAR-015", "CP-PAR-002", "component", "PR",
        testStatementSynchronizationConsumesSemicolon);
    addParserCase(registry, "TP-PAR-016", "CP-PAR-002", "component", "PR",
        testStatementSynchronizationPreservesRightBrace);
    addParserCase(registry, "TP-PAR-017", "CP-PAR-002", "component", "PR", testConsecutiveErrorsAlwaysProgress);
}
}
