#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 5> kProbes{
    "TP-PAR-563-if-depth",
    "TP-PAR-563-while-depth",
    "TP-PAR-563-let-else-depth",
    "TP-PAR-563-lowering",
    "TP-PAR-563-recovery"};

struct TreeStats final
{
    std::size_t temporaries = 0;
    std::size_t matched = 0;
    std::size_t whileLoops = 0;
    std::size_t breaks = 0;
    std::size_t inlineLetElse = 0;
    std::size_t terminal = 0;
};

TreeStats summarize(const AstNode* root)
{
    TreeStats result;
    for (const AstNode* node : flatten(root))
    {
        if (node == nullptr) continue;
        const bool declaration = node->kind() == AstNodeKind::VariableDeclarationStatement;
        const bool hidden = node->text().starts_with("$__nsc_conditional_pattern_");
        result.matched += declaration && hidden && node->text().ends_with("_matched") ? 1U : 0U;
        result.temporaries += declaration && hidden && !node->text().ends_with("_matched") ? 1U : 0U;
        result.whileLoops += node->kind() == AstNodeKind::WhileStatement ? 1U : 0U;
        result.breaks += node->kind() == AstNodeKind::BreakStatement ? 1U : 0U;
        result.inlineLetElse += node->auxiliaryText() == "conditional-pattern-inline" ? 1U : 0U;
        result.terminal += node->text() == "terminal" ? 1U : 0U;
    }
    return result;
}

std::string makeDeepIfSource()
{
    std::string source;
    source.reserve(kDepth * 31U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "if (let var item = source) ";
    source += "terminal;";
    return source;
}

std::string makeDeepWhileSource()
{
    std::string source;
    source.reserve(kDepth * 34U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "while (let var item = source) ";
    source += "terminal;";
    return source;
}

std::string makeDeepLetElseSource()
{
    std::string source;
    source.reserve(kDepth * 42U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "let var item = source else { ";
    source += "return;";
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        source += " };";
        if (index + 1U != kDepth) source += " return;";
    }
    return source;
}

bool testDeepIf(std::string& detail)
{
    ParserFixture fixture(makeDeepIfSource());
    const AstNode* root = fixture.parser().parseStatement();
    const TreeStats stats = summarize(root);
    const bool passed = root != nullptr && stats.temporaries == kDepth &&
        stats.matched == kDepth && stats.terminal == 1U &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "if temporaries/matched/terminal/diagnostics=" +
            std::to_string(stats.temporaries) + "/" + std::to_string(stats.matched) + "/" +
            std::to_string(stats.terminal) + "/" +
            std::to_string(fixture.diagnostics().diagnostics().size());
    return passed;
}

bool testDeepWhile(std::string& detail)
{
    ParserFixture fixture(makeDeepWhileSource());
    const AstNode* root = fixture.parser().parseStatement();
    const TreeStats stats = summarize(root);
    const bool passed = root != nullptr && stats.temporaries == kDepth &&
        stats.matched == kDepth && stats.whileLoops == kDepth && stats.breaks == kDepth &&
        stats.terminal == 1U && fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "while temporaries/matched/loops/breaks/diagnostics=" +
            std::to_string(stats.temporaries) + "/" + std::to_string(stats.matched) + "/" +
            std::to_string(stats.whileLoops) + "/" + std::to_string(stats.breaks) + "/" +
            std::to_string(fixture.diagnostics().diagnostics().size());
    return passed;
}

bool testDeepLetElse(std::string& detail)
{
    ParserFixture fixture(makeDeepLetElseSource());
    const AstNode* root = fixture.parser().parseStatement();
    const TreeStats stats = summarize(root);
    const bool passed = root != nullptr && stats.temporaries == kDepth &&
        stats.matched == kDepth && stats.inlineLetElse == 1U &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "let temporaries/matched/inline/diagnostics=" +
            std::to_string(stats.temporaries) + "/" + std::to_string(stats.matched) + "/" +
            std::to_string(stats.inlineLetElse) + "/" +
            std::to_string(fixture.diagnostics().diagnostics().size());
    return passed;
}

std::size_t countText(const AstNode* root, std::string_view text)
{
    std::size_t count = 0;
    for (const AstNode* node : flatten(root))
        count += node != nullptr && node->text() == text ? 1U : 0U;
    return count;
}

const AstNode* sourceDeclaration(const AstNode* root)
{
    for (const AstNode* node : flatten(root))
        if (node != nullptr && node->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->text().starts_with("$__nsc_conditional_pattern_") &&
            !node->text().ends_with("_matched")) return node;
    return nullptr;
}

bool testLowering(std::string& detail)
{
    ParserFixture fixture(
        "match (seed) { _ => {} } "
        "if (let ref alias = next()) { consume(alias); } else { wait(); } "
        "while (let move current = again()) { consume(current); } "
        "let var value = final() else { return; };");
    const AstNode* match = fixture.parser().parseStatement();
    const AstNode* selection = fixture.parser().parseStatement();
    const AstNode* loop = fixture.parser().parseStatement();
    const AstNode* letElse = fixture.parser().parseStatement();
    const AstNode* selectionSource = sourceDeclaration(selection);
    const AstNode* loopSource = sourceDeclaration(loop);
    const AstNode* letSource = sourceDeclaration(letElse);
    const bool passed = match != nullptr && selectionSource != nullptr && loopSource != nullptr &&
        letSource != nullptr && selectionSource->text() == "$__nsc_conditional_pattern_1" &&
        loopSource->text() == "$__nsc_conditional_pattern_2" &&
        letSource->text() == "$__nsc_conditional_pattern_3" &&
        selectionSource->auxiliaryText() == "local:ref" && !selectionSource->flag() &&
        !loopSource->flag() && letSource->flag() && countText(selection, "next") == 1U &&
        countText(loop, "again") == 1U && countText(letElse, "final") == 1U &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "temporary sequence, ownership, source evaluation, or lowering order changed";
    return passed;
}

bool hasDiagnostic(ParserFixture& fixture, DiagnosticId id, std::string_view text)
{
    for (const auto& diagnostic : fixture.diagnostics().diagnostics())
        if (diagnostic.id() == id && diagnostic.message().find(text) != std::string::npos)
            return true;
    return false;
}

bool testRecovery(std::string& detail)
{
    ParserFixture pattern(
        "if (let (ref alias, _) | (var other, _) = source) action; else fallback;");
    static_cast<void>(pattern.parser().parseStatement());
    ParserFixture divergence("let var value = source else { action; }; after;");
    static_cast<void>(divergence.parser().parseStatement());
    const AstNode* afterDivergence = divergence.parser().parseStatement();
    ParserFixture equal("if (let var value source) action; after;");
    static_cast<void>(equal.parser().parseStatement());
    const AstNode* afterEqual = equal.parser().parseStatement();
    ParserFixture semicolon("let var value = source else { return; } after;");
    static_cast<void>(semicolon.parser().parseStatement());
    const AstNode* afterSemicolon = semicolon.parser().parseStatement();
    const bool passed = hasDiagnostic(pattern, DiagnosticId::InvalidOperand,
            "ref/move pattern binding cannot cross") &&
        hasDiagnostic(pattern, DiagnosticId::InvalidOperand, "OR-pattern alternatives") &&
        hasDiagnostic(divergence, DiagnosticId::InvalidOperand,
            "the else body of a let-else statement must diverge") &&
        hasDiagnostic(equal, DiagnosticId::ExpectedToken,
            "expected '=' between conditional pattern and source expression") &&
        hasDiagnostic(semicolon, DiagnosticId::ExpectedToken,
            "expected ';' after let-else statement") && afterDivergence != nullptr &&
        afterEqual != nullptr && afterSemicolon != nullptr;
    if (!passed) detail = "diagnostic order/text or following-statement recovery changed";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepIf(detail);
    if (probe == kProbes[1]) return testDeepWhile(detail);
    if (probe == kProbes[2]) return testDeepLetElse(detail);
    if (probe == kProbes[3]) return testLowering(detail);
    if (probe == kProbes[4]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testConditionalPatternCoordinator(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-563 conditional-pattern continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-563", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-563 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserConditionalPatternCoordinatorCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-563",
        "CP-PAR-035",
        testConditionalPatternCoordinator,
        "NIGHTLY");
}
}
