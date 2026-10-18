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

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-557-foreach-depth",
    "TP-PAR-557-collection-probe",
    "TP-PAR-557-binding-ordinals",
    "TP-PAR-557-recovery"};

const AstNode* arrayUserBody(const AstNode* foreachNode)
{
    const AstNode* candidate = foreachNode != nullptr ? foreachNode->second() : nullptr;
    const AstNode* loop = candidate != nullptr && candidate->children().size() == 1U
        ? candidate->children()[0] : nullptr;
    const AstNode* body = loop != nullptr && loop->children().size() == 1U
        ? loop->children()[0] : nullptr;
    return body != nullptr && body->children().size() >= 2U ? body->children()[1] : nullptr;
}

std::string makeDeepForeachSource()
{
    std::string source;
    source.reserve(kDepth * 35U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "foreach (var item in values) { ";
    source += "terminal;";
    for (std::size_t index = 0; index < kDepth; ++index) source += " }";
    return source;
}

bool testDeepForeach(std::string& detail)
{
    ParserFixture fixture(makeDeepForeachSource());
    const AstNode* node = fixture.parser().parseStatement();
    std::size_t depth = 0;
    while (node != nullptr && node->kind() == AstNodeKind::ForeachStatement)
    {
        if (!node->range().isValid() || node->first() == nullptr)
        {
            detail = "invalid foreach range or source declaration";
            return false;
        }
        ++depth;
        const AstNode* body = arrayUserBody(node);
        node = body != nullptr && body->children().size() == 1U
            ? body->children()[0] : nullptr;
    }
    if (depth != kDepth)
    {
        detail = "expected " + std::to_string(kDepth) + " foreach nodes, got " +
            std::to_string(depth);
        return false;
    }
    if (!fixture.diagnostics().diagnostics().empty())
    {
        detail = "valid deep foreach chain emitted diagnostics";
        return false;
    }
    return true;
}

bool testCollectionProbe(std::string& detail)
{
    ParserFixture fixture(
        "foreach (var item in Build(() => { return source; })) { consume(item); }");
    const AstNode* node = fixture.parser().parseStatement();
    const AstNode* declaration = node != nullptr ? node->first() : nullptr;
    const AstNode* collection = declaration != nullptr ? declaration->first() : nullptr;
    const AstNode* probe = node != nullptr && !node->children().empty()
        ? node->children()[0] : nullptr;
    const bool ok = node != nullptr && node->kind() == AstNodeKind::ForeachStatement &&
        collection != nullptr && collection->kind() == AstNodeKind::CallExpression &&
        probe != nullptr && probe->kind() == AstNodeKind::CallExpression &&
        probe != collection && collection->range().isValid() && probe->range().isValid() &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok) detail = "collection/probe slots, lambda-block parsing, or cursor restoration changed";
    return ok;
}

bool testBindingOrdinals(std::string& detail)
{
    ParserFixture fixture(
        "{ foreach (int value in first) {} foreach (ref var readonlyItem in second) {} "
        "foreach (ref mut var mutableItem in third) {} foreach (raii var resource in fourth) {} "
        "foreach (var (left, right) in fifth) {} }");
    const AstNode* block = fixture.parser().parseStatement();
    if (block == nullptr || block->children().size() != 5U)
    {
        detail = "foreach binding fixture preserved " +
            std::to_string(block != nullptr ? block->children().size() : 0U) +
            " statements and emitted " +
            std::to_string(fixture.diagnostics().diagnostics().size()) + " diagnostics";
        if (!fixture.diagnostics().diagnostics().empty())
            detail += ": " + fixture.diagnostics().diagnostics().front().message();
        return false;
    }
    constexpr std::array<std::string_view, 4> modes{
        "foreach:value", "foreach:ref", "foreach:ref-mut", "foreach:value"};
    for (std::size_t index = 0; index < block->children().size(); ++index)
    {
        const AstNode* node = block->children()[index];
        const std::string expected = "$__foreach_" + std::to_string(index) + "_source";
        if (node == nullptr || node->kind() != AstNodeKind::ForeachStatement ||
            node->first() == nullptr || node->first()->text() != expected ||
            (index < modes.size() && node->auxiliaryText() != modes[index]))
        {
            detail = "binding mode or monotonic temporary ordinal changed";
            return false;
        }
    }
    if (!fixture.diagnostics().diagnostics().empty())
    {
        detail = "valid binding forms emitted diagnostics";
        return false;
    }
    return true;
}

bool testRecovery(std::string& detail)
{
    ParserFixture fixture("{ foreach (var item in ) {} return 1; }");
    const AstNode* block = fixture.parser().parseStatement();
    const AstNode* following = block != nullptr && !block->children().empty()
        ? block->children().back() : nullptr;
    const bool ok = block != nullptr && following != nullptr &&
        following->kind() == AstNodeKind::ReturnStatement && fixture.diagnostics().hasErrors() &&
        fixture.parser().current().kind() == TokenKind::EndOfFile;
    if (!ok) detail = "malformed collection did not diagnose and preserve following progress";
    return ok;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepForeach(detail);
    if (probe == kProbes[1]) return testCollectionProbe(detail);
    if (probe == kProbes[2]) return testBindingOrdinals(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testForeachCoordinator(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-557 foreach coordinator must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-557", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-557 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserForeachCoordinatorCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-557",
        "CP-PAR-034",
        testForeachCoordinator,
        "NIGHTLY");
}
}
