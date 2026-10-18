#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-562-match-depth",
    "TP-PAR-562-lowering",
    "TP-PAR-562-diagnostics",
    "TP-PAR-562-recovery"};

std::string makeDeepMatchSource()
{
    std::string source;
    source.reserve(kDepth * 28U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "match (value) { _ => { ";
    source += "terminal;";
    for (std::size_t index = 0; index < kDepth; ++index) source += " } }";
    return source;
}

bool testDeepMatch(std::string& detail)
{
    ParserFixture fixture(makeDeepMatchSource());
    const AstNode* root = fixture.parser().parseStatement();
    std::size_t matches = 0;
    std::size_t terminals = 0;
    for (const AstNode* node : flatten(root))
    {
        matches += node != nullptr &&
            node->auxiliaryText().starts_with("match-coverage:v1:") ? 1U : 0U;
        terminals += node != nullptr && node->text() == "terminal" ? 1U : 0U;
    }
    const bool passed = root != nullptr && matches == kDepth && terminals == 1U &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "matches=" + std::to_string(matches) +
            ", terminals=" + std::to_string(terminals) +
            ", diagnostics=" +
            std::to_string(fixture.diagnostics().diagnostics().size());
    return passed;
}

std::size_t countText(const AstNode* root, std::string_view text)
{
    std::size_t result = 0;
    for (const AstNode* node : flatten(root))
        result += node != nullptr && node->text() == text ? 1U : 0U;
    return result;
}

bool testLowering(std::string& detail)
{
    ParserFixture fixture(
        "match (next()) { Some(var payload) => { consume(payload); }, "
        "[var head, .. var tail] when ready => {}, ref mut alias => {} }");
    const AstNode* first = fixture.parser().parseStatement();
    ParserFixture sequence(
        "match (first) { _ => {} } match (second) { _ => {} }");
    const AstNode* sequenceFirst = sequence.parser().parseStatement();
    const AstNode* sequenceSecond = sequence.parser().parseStatement();
    const bool passed = first != nullptr && sequenceFirst != nullptr && sequenceSecond != nullptr &&
        first->text() == "$__nsc_match_0" &&
        sequenceFirst->text() == "$__nsc_match_0" &&
        sequenceSecond->text() == "$__nsc_match_1" &&
        first->auxiliaryText() == "match-coverage:v1:Some;~#1+;*" &&
        first->children().size() == 5U &&
        first->children()[0]->auxiliaryText() == "local:ref" &&
        countText(first, "next") == 1U && countText(first, "payload") >= 2U &&
        countText(sequenceSecond, "second") == 1U &&
        fixture.diagnostics().diagnostics().empty() &&
        sequence.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "first=" + (first != nullptr ? std::string(first->text()) : "<null>") +
            ", sequence=" +
            (sequenceFirst != nullptr ? std::string(sequenceFirst->text()) : "<null>") + "/" +
            (sequenceSecond != nullptr ? std::string(sequenceSecond->text()) : "<null>") +
            ", coverage=" +
            (first != nullptr ? std::string(first->auxiliaryText()) : "<null>") +
            ", children=" +
            std::to_string(first != nullptr ? first->children().size() : 0U) +
            ", ownership=" + (first != nullptr && !first->children().empty()
                ? std::string(first->children()[0]->auxiliaryText()) : "<missing>") +
            ", next=" + std::to_string(countText(first, "next")) +
            ", payload=" + std::to_string(countText(first, "payload")) +
            ", second-count=" + std::to_string(countText(sequenceSecond, "second")) +
            ", diagnostics=" + std::to_string(fixture.diagnostics().diagnostics().size());
    return passed;
}

bool diagnosticContains(
    const nativesharp::diagnostics::Diagnostic& diagnostic,
    DiagnosticId id,
    std::string_view text)
{
    return diagnostic.id() == id && diagnostic.message().find(text) != std::string::npos;
}

bool testDiagnostics(std::string& detail)
{
    ParserFixture fixture(
        "match (value) { (ref alias, _) | (var other, _) when false => {}, "
        "_ => {}, 1 | 1 => {} }");
    static_cast<void>(fixture.parser().parseStatement());
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    const bool passed = diagnostics.size() == 5U &&
        diagnosticContains(diagnostics[0], DiagnosticId::InvalidOperand,
            "ref pattern binding cannot cross") &&
        diagnosticContains(diagnostics[1], DiagnosticId::InvalidOperand,
            "OR-pattern alternatives") &&
        diagnosticContains(diagnostics[2], DiagnosticId::InvalidOperand,
            "guard is the constant false") &&
        diagnosticContains(diagnostics[3], DiagnosticId::InvalidOperand,
            "unreachable after a catch-all") &&
        diagnosticContains(diagnostics[4], DiagnosticId::InvalidOperand,
            "overlaps an earlier alternative");
    if (!passed) detail = "diagnostic count/order=" + std::to_string(diagnostics.size());
    return passed;
}

bool hasDiagnostic(ParserFixture& fixture, DiagnosticId id, std::string_view text)
{
    for (const auto& diagnostic : fixture.diagnostics().diagnostics())
        if (diagnosticContains(diagnostic, id, text)) return true;
    return false;
}

bool testRecovery(std::string& detail)
{
    ParserFixture empty("match (value) {}");
    const AstNode* emptyNode = empty.parser().parseStatement();
    ParserFixture body("match (value) { A => next; after; }");
    const AstNode* bodyNode = body.parser().parseStatement();
    ParserFixture guard("match (value) { _ when flag = other => {} }");
    const AstNode* guardNode = guard.parser().parseStatement();
    const bool passed = emptyNode != nullptr && emptyNode->children().size() == 2U &&
        hasDiagnostic(empty, DiagnosticId::InvalidOperand,
            "a match statement requires at least one arm") && bodyNode != nullptr &&
        hasDiagnostic(body, DiagnosticId::ExpectedToken,
            "a match arm requires a block body") && guardNode != nullptr &&
        hasDiagnostic(guard, DiagnosticId::ExpectedToken,
            "expected '=>' after match pattern, found '='") &&
        empty.parser().current().kind() == TokenKind::EndOfFile;
    if (!passed) detail = "empty/body/conditional-guard recovery contract changed";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepMatch(detail);
    if (probe == kProbes[1]) return testLowering(detail);
    if (probe == kProbes[2]) return testDiagnostics(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testMatchCoordinator(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-562 match continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-562", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-562 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserMatchCoordinatorCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-562",
        "CP-PAR-035",
        testMatchCoordinator,
        "NIGHTLY");
}
}
