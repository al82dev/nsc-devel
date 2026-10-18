#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 3> kProbes{
    "TP-PAR-552-question-index-depth",
    "TP-PAR-552-question-index-crossed-depth",
    "TP-PAR-552-question-index-contracts"};

// Reads every question token through the product's O(1) query without changing its cached role.
// The fixture owns all token and parser storage for the duration of the returned snapshot.
std::vector<std::uint8_t> collectQuestionRoles(ParserFixture& fixture)
{
    std::vector<std::uint8_t> roles;
    for (std::size_t index = 0; index < fixture.tokens().size(); ++index)
    {
        if (fixture.tokens()[index].kind() != TokenKind::Question) continue;
        fixture.parser().index_ = index;
        roles.push_back(static_cast<std::uint8_t>(fixture.parser().questionBeginsConditional()));
    }
    return roles;
}

// Proves indexed preclassification at the depth that made repeated suffix scans O(N^2). The
// optional crossed delimiters exercise exact malformed-input recovery at the same depth.
bool testQuestionIndexDepth(std::string_view middle, std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 4U + middle.size());
    for (std::size_t index = 0; index < kDepth; ++index) source += "v?";
    source += middle;
    for (std::size_t index = 0; index < kDepth; ++index) source += ":v";
    ParserFixture fixture(std::move(source));
    const std::vector<std::uint8_t> roles = collectQuestionRoles(fixture);
    if (roles.size() != kDepth)
    {
        detail = "question count mismatch";
        return false;
    }
    for (std::size_t index = 0; index < roles.size(); ++index)
    {
        if (roles[index] != 0U) continue;
        detail = "conditional role lost at depth " + std::to_string(index);
        return false;
    }
    return fixture.parser().conditionalQuestionIndex_.size() == fixture.tokens().size();
}

// Preserves nesting, postfix propagation, expression delimiters and malformed-closer recovery.
// Expected roles are recorded in lexical order so classification order is also observable.
bool testQuestionIndexContracts(std::string& detail)
{
    ParserFixture valid("a? + b?; c ? d : e; f ? (g ? h : i) : j; k ? [l ? m : n] : o");
    constexpr std::array<std::uint8_t, 7> expected{0U, 0U, 1U, 1U, 1U, 1U, 1U};
    const std::vector<std::uint8_t> validRoles = collectQuestionRoles(valid);
    if (!std::equal(validRoles.begin(), validRoles.end(), expected.begin(), expected.end()))
    {
        detail = "valid nesting or propagation roles changed";
        return false;
    }
    ParserFixture recovery("a ? b; c?; d ? (e?] : f; q ? value ) : z");
    constexpr std::array<std::uint8_t, 5> recoveryExpected{0U, 0U, 1U, 0U, 0U};
    const std::vector<std::uint8_t> recoveryRoles = collectQuestionRoles(recovery);
    if (!std::equal(
            recoveryRoles.begin(), recoveryRoles.end(),
            recoveryExpected.begin(), recoveryExpected.end()))
    {
        detail = "delimiter recovery roles changed";
        return false;
    }
    return true;
}

// Dispatches an isolated probe so a complexity or stack regression cannot abort the host aggregate.
bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testQuestionIndexDepth("leaf", detail);
    if (probe == kProbes[1]) return testQuestionIndexDepth("([)]leaf", detail);
    if (probe == kProbes[2]) return testQuestionIndexContracts(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

// Executes all three counterfactuals in fresh child processes and preserves the canonical JSON oracle.
void testConditionalQuestionIndex(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-552 conditional question index must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-552", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-552 question-index probe must pass " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

// Publishes TP-PAR-552 through the canonical Host.Parser registry and NIGHTLY profile.
void registerParserConditionalQuestionIndexCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-552",
        "CP-PAR-036",
        testConditionalQuestionIndex,
        "NIGHTLY");
}
}
