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
    "TP-PAR-556-mixed-control-depth",
    "TP-PAR-556-if-contracts",
    "TP-PAR-556-loop-contracts",
    "TP-PAR-556-recovery"};

// Builds one mixed D1 chain without using recursive fixture construction.
std::string makeDeepControlSource()
{
    std::string source;
    source.reserve(kDepth * 30U);
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        switch (index % 4U)
        {
        case 0U: source += "if (condition) "; break;
        case 1U: source += "while (condition) { "; break;
        case 2U: source += "do { "; break;
        default: source += "for (;;) { "; break;
        }
    }
    source += "terminal;";
    for (std::size_t index = kDepth; index-- > 0;)
    {
        switch (index % 4U)
        {
        case 0U: break;
        case 1U: source += " }"; break;
        case 2U: source += " } while (condition);"; break;
        default: source += " }"; break;
        }
    }
    return source;
}

// Counts only source control nodes while checking that every produced range remains valid.
bool validateDeepControl(const AstNode* root, std::string& detail)
{
    std::size_t controls = 0;
    for (const AstNode* node : flatten(root))
    {
        if (!node->range().isValid())
        {
            detail = "invalid source range in mixed control tree";
            return false;
        }
        controls += node->kind() == AstNodeKind::IfStatement ||
            node->kind() == AstNodeKind::WhileStatement ||
            node->kind() == AstNodeKind::DoWhileStatement ||
            node->kind() == AstNodeKind::ForStatement ? 1U : 0U;
    }
    if (controls == kDepth) return true;
    detail = "expected " + std::to_string(kDepth) + " controls, got " +
        std::to_string(controls);
    return false;
}

// Parses the 32,768-layer mixed chain and requires zero diagnostics.
bool testDeepMixedControl(std::string& detail)
{
    ParserFixture fixture(makeDeepControlSource());
    const AstNode* const root = fixture.parser().parseStatement();
    if (!validateDeepControl(root, detail)) return false;
    if (fixture.diagnostics().diagnostics().empty()) return true;
    detail = "valid mixed control chain emitted diagnostics";
    return false;
}

// Verifies condition declarations and nearest-if else binding without changing their AST shape.
bool testIfContracts(std::string& detail)
{
    ParserFixture declaration(
        "if (bool ready = source) value; else if (other) alternate; else fallback;");
    const AstNode* const declared = declaration.parser().parseStatement();
    ParserFixture dangling("if (first) if (second) inner; else nearest;");
    const AstNode* const outer = dangling.parser().parseStatement();
    const AstNode* const outerBody = outer != nullptr ? outer->second() : nullptr;
    const AstNode* const inner = outerBody != nullptr && outerBody->children().size() == 1U
        ? outerBody->children()[0] : nullptr;
    const bool declarationOk = declared != nullptr &&
        countAuxiliary(declared, "condition-declaration") == 1U &&
        firstKind(declared, AstNodeKind::VariableDeclarationStatement) != nullptr &&
        declaration.diagnostics().diagnostics().empty();
    const bool danglingOk = outer != nullptr && outer->kind() == AstNodeKind::IfStatement &&
        outer->third() == nullptr && inner != nullptr &&
        inner->kind() == AstNodeKind::IfStatement && inner->third() != nullptr &&
        dangling.diagnostics().diagnostics().empty();
    if (declarationOk && danglingOk) return true;
    detail = declarationOk ? "dangling else binding changed" : "if declaration contract changed";
    return false;
}

// Verifies while declarations, do trailing conditions, and every for header slot.
bool testLoopContracts(std::string& detail)
{
    ParserFixture whileFixture("while (bool ready = source) { value; }");
    const AstNode* const whileNode = whileFixture.parser().parseStatement();
    ParserFixture doFixture("do { body; } while (condition);");
    const AstNode* const doNode = doFixture.parser().parseStatement();
    ParserFixture forFixture(
        "for (int i = 0; condition; i = i + 1, j = j + 1) { body; }");
    const AstNode* const forNode = forFixture.parser().parseStatement();
    const bool whileOk = whileNode != nullptr &&
        whileNode->kind() == AstNodeKind::WhileStatement &&
        countAuxiliary(whileNode, "condition-declaration-iteration") == 1U &&
        whileFixture.diagnostics().diagnostics().empty();
    const bool doOk = doNode != nullptr && doNode->kind() == AstNodeKind::DoWhileStatement &&
        doNode->first() != nullptr && doNode->second() != nullptr &&
        doFixture.diagnostics().diagnostics().empty();
    const bool forOk = forNode != nullptr && forNode->kind() == AstNodeKind::ForStatement &&
        forNode->first() != nullptr && forNode->second() != nullptr &&
        forNode->third() != nullptr && forNode->third()->children().size() == 2U &&
        forFixture.diagnostics().diagnostics().empty();
    if (whileOk && doOk && forOk) return true;
    detail = !whileOk ? "while declaration contract changed" :
        (!doOk ? "do-while trailing condition changed" : "for header contract changed");
    return false;
}

// Requires bounded recovery for each malformed D1 family without suppressing diagnostics.
bool testRecovery(std::string& detail)
{
    ParserFixture whileFixture("while (condition) value;");
    const AstNode* const whileNode = whileFixture.parser().parseStatement();
    ParserFixture doFixture("do { body; } condition;");
    const AstNode* const doNode = doFixture.parser().parseStatement();
    ParserFixture forFixture("for (item; condition item) { body; }");
    const AstNode* const forNode = forFixture.parser().parseStatement();
    const bool recovered = whileNode != nullptr && doNode != nullptr && forNode != nullptr &&
        whileNode->range().isValid() && doNode->range().isValid() && forNode->range().isValid() &&
        !whileFixture.diagnostics().diagnostics().empty() &&
        !doFixture.diagnostics().diagnostics().empty() &&
        !forFixture.diagnostics().diagnostics().empty();
    if (recovered) return true;
    detail = "D1 recovery, diagnostic, range, or progress changed";
    return false;
}

// Maps one isolated probe name to exactly one bounded D1 counterfactual.
bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepMixedControl(detail);
    if (probe == kProbes[1]) return testIfContracts(detail);
    if (probe == kProbes[2]) return testLoopContracts(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

// Runs deep work out of process so a native stack regression cannot terminate Host.Parser.
void testStructuredControlContinuation(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-556 structured control must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-556", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-556 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

// Registers TP-PAR-556 in the canonical Host.Parser catalog.
void registerParserStructuredControlContinuationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-556",
        "CP-PAR-028",
        testStructuredControlContinuation,
        "NIGHTLY");
}
}
