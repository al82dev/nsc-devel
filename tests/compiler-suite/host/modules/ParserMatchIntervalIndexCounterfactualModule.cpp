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

constexpr std::size_t kIntervalCount = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-550-indexed-32768",
    "TP-PAR-550-intra-arm",
    "TP-PAR-550-adjacency-guards",
    "TP-PAR-550-mixed-boundaries"};

std::size_t overlapDiagnosticCount(ParserFixture& fixture)
{
    std::size_t count = 0;
    for (const auto& diagnostic : fixture.diagnostics().diagnostics())
        if (diagnostic.id() == DiagnosticId::InvalidOperand &&
            diagnostic.message().find("overlaps an earlier alternative") != std::string::npos)
            ++count;
    return count;
}

bool testIndexedIntervals(std::string& detail)
{
    std::string source = "match (value) { ";
    source.reserve(kIntervalCount * 18U);
    for (std::size_t index = 0; index < kIntervalCount; ++index)
        source += std::to_string(index * 2U) + " => {}, ";
    source += "32768 => {} }";
    ParserFixture fixture(source);
    AstNode* const node = fixture.parser().parseStatement();
    const std::size_t overlaps = overlapDiagnosticCount(fixture);
    if (node != nullptr && overlaps == 1U &&
        fixture.diagnostics().diagnostics().size() == 1U) return true;
    detail = "overlaps=" + std::to_string(overlaps) + ", diagnostics=" +
        std::to_string(fixture.diagnostics().diagnostics().size());
    return false;
}

bool testIntraArmOverlap(std::string& detail)
{
    ParserFixture fixture("match (value) { 1..4 | 3..7 => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    const std::size_t overlaps = overlapDiagnosticCount(fixture);
    if (node != nullptr && overlaps == 1U &&
        fixture.diagnostics().diagnostics().size() == 1U) return true;
    detail = "overlaps=" + std::to_string(overlaps);
    return false;
}

bool testAdjacencyAndGuards(std::string& detail)
{
    ParserFixture fixture(
        "match (value) { 1..4 => {}, 4..7 => {}, 8..10 when ready => {}, "
        "8..10 => {}, 9..11 when ready => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    const std::size_t overlaps = overlapDiagnosticCount(fixture);
    if (node != nullptr && overlaps == 1U &&
        fixture.diagnostics().diagnostics().size() == 1U) return true;
    detail = "overlaps=" + std::to_string(overlaps) + ", diagnostics=" +
        std::to_string(fixture.diagnostics().diagnostics().size());
    return false;
}

bool testMixedAndBoundaryDomains(std::string& detail)
{
    ParserFixture fixture(
        "match (value) { 1 | Some(_) => {}, 1 => {}, "
        "9223372036854775807 => {}, 9223372036854775807 => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    if (node != nullptr && fixture.diagnostics().diagnostics().empty()) return true;
    detail = "diagnostics=" + std::to_string(fixture.diagnostics().diagnostics().size());
    return false;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testIndexedIntervals(detail);
    if (probe == kProbes[1]) return testIntraArmOverlap(detail);
    if (probe == kProbes[2]) return testAdjacencyAndGuards(detail);
    if (probe == kProbes[3]) return testMixedAndBoundaryDomains(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testMatchIntervalIndex(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-550 ordered match interval index must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-550", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-550 interval index probe must pass " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserMatchIntervalIndexCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-550",
        "CP-PAR-042",
        testMatchIntervalIndex,
        "NIGHTLY");
}
}
