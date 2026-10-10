#include "LexerTestSupport.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite {

namespace {

using lexer_test::LexerFixture;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticSeverity;
using nativesharp::lexer::TokenKind;

void addCase(TestRegistry& registry,
             std::string id,
             std::string_view contractId,
             std::string_view,
             TestFunction function) {
    registry.add(TestCase{std::move(id), std::string(contractId), "Host.Lexer", "component", "PR",
        {"lexer", "diagnostic", "recovery"}, std::move(function)});
}

void expectToken(TestContext& context,
                 const LexerFixture& fixture,
                 std::size_t index,
                 TokenKind kind,
                 std::uint32_t begin,
                 std::uint32_t end,
                 std::string_view lexeme,
                 std::string_view caseId) {
    lexer_test::expectToken(context, fixture.tokens()[index], kind, begin, end, lexeme, caseId);
}

void expectDiagnostic(TestContext& context,
                      const LexerFixture& fixture,
                      std::size_t index,
                      DiagnosticId id,
                      DiagnosticSeverity,
                      std::uint32_t begin,
                      std::uint32_t end,
                      std::string_view message,
                      std::string_view caseId) {
    lexer_test::expectDiagnostic(context, fixture.diagnostics().diagnostics()[index], id,
        message, begin, true, begin, end, caseId);
}

} // namespace

void registerLexerDiagnosticRecoveryModule(TestRegistry& registry) {
    addCase(registry, "TL-LEX-098",
                     "CL-LEX-014",
                     "an unexpected byte reports its exact source range",
                     [](TestContext& context) {
                         LexerFixture fixture("@");
                         context.expect(fixture.tokens().size() == 2, "TL-LEX-098 token count must match");
                         if (fixture.tokens().size() == 2) {
                             expectToken(context,
                                         fixture,
                                         0,
                                         TokenKind::Invalid,
                                         0,
                                         1,
                                         "@",
                                         "TL-LEX-098");
                             expectToken(context,
                                         fixture,
                                         1,
                                         TokenKind::EndOfFile,
                                         1,
                                         1,
                                         "",
                                         "TL-LEX-098");
                         }
                         expectDiagnostic(context,
                                          fixture,
                                          0,
                                          DiagnosticId::UnexpectedCharacter,
                                          DiagnosticSeverity::Error,
                                          0,
                                          1,
                                          "unexpected character in source",
                                          "TL-LEX-098");
                     });

    addCase(registry, "TL-LEX-099",
                     "CL-LEX-014",
                     "lexing recovers after every unexpected byte",
                     [](TestContext& context) {
                         LexerFixture fixture("@ # $");
                         context.expect(fixture.tokens().size() == 4, "TL-LEX-099 token count must match");
                         if (fixture.tokens().size() == 4) {
                             expectToken(context,
                                         fixture,
                                         0,
                                         TokenKind::Invalid,
                                         0,
                                         1,
                                         "@",
                                         "TL-LEX-099");
                             expectToken(context,
                                         fixture,
                                         1,
                                         TokenKind::Invalid,
                                         2,
                                         3,
                                         "#",
                                         "TL-LEX-099");
                             expectToken(context,
                                         fixture,
                                         2,
                                         TokenKind::Invalid,
                                         4,
                                         5,
                                         "$",
                                         "TL-LEX-099");
                             expectToken(context,
                                         fixture,
                                         3,
                                         TokenKind::EndOfFile,
                                         5,
                                         5,
                                         "",
                                         "TL-LEX-099");
                         }
                         expectDiagnostic(context,
                                          fixture,
                                          0,
                                          DiagnosticId::UnexpectedCharacter,
                                          DiagnosticSeverity::Error,
                                          0,
                                          1,
                                          "unexpected character in source",
                                          "TL-LEX-099");
                         expectDiagnostic(context,
                                          fixture,
                                          1,
                                          DiagnosticId::UnexpectedCharacter,
                                          DiagnosticSeverity::Error,
                                          2,
                                          3,
                                          "unexpected character in source",
                                          "TL-LEX-099");
                         expectDiagnostic(context,
                                          fixture,
                                          2,
                                          DiagnosticId::UnexpectedCharacter,
                                          DiagnosticSeverity::Error,
                                          4,
                                          5,
                                          "unexpected character in source",
                                          "TL-LEX-099");
                     });

    addCase(registry, "TL-LEX-100",
                     "CL-LEX-014",
                     "diagnostic error limit suppresses only later reports",
                     [](TestContext& context) {
                         LexerFixture fixture("@#$", 1);
                         context.expect(fixture.tokens().size() == 4, "TL-LEX-100 token count must match");
                         if (fixture.tokens().size() == 4) {
                             expectToken(context,
                                         fixture,
                                         0,
                                         TokenKind::Invalid,
                                         0,
                                         1,
                                         "@",
                                         "TL-LEX-100");
                             expectToken(context,
                                         fixture,
                                         1,
                                         TokenKind::Invalid,
                                         1,
                                         2,
                                         "#",
                                         "TL-LEX-100");
                             expectToken(context,
                                         fixture,
                                         2,
                                         TokenKind::Invalid,
                                         2,
                                         3,
                                         "$",
                                         "TL-LEX-100");
                             expectToken(context,
                                         fixture,
                                         3,
                                         TokenKind::EndOfFile,
                                         3,
                                         3,
                                         "",
                                         "TL-LEX-100");
                         }
                         expectDiagnostic(context,
                                          fixture,
                                          0,
                                          DiagnosticId::UnexpectedCharacter,
                                          DiagnosticSeverity::Error,
                                          0,
                                          1,
                                          "unexpected character in source",
                                          "TL-LEX-100");
                         context.expect(fixture.diagnostics().errorCount() == 1,
                                        "TL-LEX-100 error count must stop at the limit");
                         context.expect(fixture.diagnostics().errorLimitReached(),
                                        "TL-LEX-100 must expose the reached limit");
                         context.expect(fixture.diagnostics().suppressedDiagnosticCount() == 2,
                                        "TL-LEX-100 must count two suppressed diagnostics");
                     });
}

} // namespace nsc::suite
