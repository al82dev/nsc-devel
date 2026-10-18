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
    "TP-PAR-559-using-depth",
    "TP-PAR-559-resource-order",
    "TP-PAR-559-lambda-initializer",
    "TP-PAR-559-recovery"};

std::string makeDeepUsingSource()
{
    std::string source;
    source.reserve(kDepth * 48U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "using (Resource resource = open) { ";
    source += "terminal;";
    for (std::size_t index = 0; index < kDepth; ++index) source += " }";
    return source;
}

bool validUsing(const AstNode* node)
{
    return node != nullptr && node->kind() == AstNodeKind::UsingStatement &&
        node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "Resource" &&
        node->first() != nullptr && node->second() != nullptr && node->range().isValid();
}

bool testDeepUsing(std::string& detail)
{
    ParserFixture fixture(makeDeepUsingSource(), {"Resource"});
    const AstNode* node = fixture.parser().parseStatement();
    std::size_t depth = 0;
    while (validUsing(node))
    {
        if (node->text() != "resource" || node->second()->kind() != AstNodeKind::BlockStatement ||
            node->second()->children().size() != 1U)
        {
            detail = "using AST changed at depth " + std::to_string(depth);
            return false;
        }
        ++depth;
        node = node->second()->children()[0];
    }
    if (depth != kDepth)
    {
        detail = "expected " + std::to_string(kDepth) + " using nodes, got " +
            std::to_string(depth);
        return false;
    }
    if (node == nullptr || node->kind() != AstNodeKind::ExpressionStatement ||
        !fixture.diagnostics().diagnostics().empty())
    {
        detail = "deep terminal or diagnostics changed";
        return false;
    }
    return fixture.parser().current().kind() == TokenKind::EndOfFile;
}

bool testResourceOrder(std::string& detail)
{
    ParserFixture fixture(
        "using (Resource outer = openOuter) { "
        "using (Resource inner = openInner) { work; } afterInner; } trailing;",
        {"Resource"});
    const AstNode* outer = fixture.parser().parseStatement();
    const AstNode* trailing = fixture.parser().parseStatement();
    const AstNode* outerBody = outer != nullptr ? outer->second() : nullptr;
    const AstNode* inner = outerBody != nullptr && outerBody->children().size() == 2U
        ? outerBody->children()[0] : nullptr;
    const bool ok = validUsing(outer) && outer->text() == "outer" && validUsing(inner) &&
        inner->text() == "inner" && outerBody->children()[1]->kind() == AstNodeKind::ExpressionStatement &&
        trailing != nullptr && trailing->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok) detail = "resource nesting, source order, cursor, or diagnostics changed";
    return ok;
}

bool testLambdaInitializer(std::string& detail)
{
    ParserFixture fixture(
        "using (Resource resource = handler => { return open; }) { work; } trailing;",
        {"Resource"});
    const AstNode* node = fixture.parser().parseStatement();
    const AstNode* trailing = fixture.parser().parseStatement();
    const bool ok = validUsing(node) && node->first()->kind() == AstNodeKind::LambdaExpression &&
        node->first()->flag() && node->first()->first() != nullptr &&
        node->first()->first()->kind() == AstNodeKind::BlockStatement &&
        trailing != nullptr && trailing->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok) detail = "expression-to-block initializer boundary or following cursor changed";
    return ok;
}

bool testRecovery(std::string& detail)
{
    ParserFixture missingEqual(
        "{ using (Resource resource open) {} return 7; }", {"Resource"});
    const AstNode* firstBlock = missingEqual.parser().parseStatement();
    const AstNode* firstFollowing = firstBlock != nullptr && !firstBlock->children().empty()
        ? firstBlock->children().back() : nullptr;
    ParserFixture missingBody(
        "using (Resource resource = open) return 9;", {"Resource"});
    const AstNode* statement = missingBody.parser().parseStatement();
    const AstNode* secondFollowing = missingBody.parser().parseStatement();
    const bool ok = firstFollowing != nullptr &&
        firstFollowing->kind() == AstNodeKind::ReturnStatement &&
        missingEqual.diagnostics().hasErrors() && statement != nullptr &&
        statement->kind() == AstNodeKind::UsingStatement && statement->second() == nullptr &&
        secondFollowing != nullptr && secondFollowing->kind() == AstNodeKind::ReturnStatement &&
        missingBody.diagnostics().hasErrors();
    if (!ok) detail = "resource/body recovery or following-statement progress changed";
    return ok;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepUsing(detail);
    if (probe == kProbes[1]) return testResourceOrder(detail);
    if (probe == kProbes[2]) return testLambdaInitializer(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testUsingCoordinator(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-559 using continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-559", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-559 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserUsingCoordinatorCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-559",
        "CP-PAR-035",
        testUsingCoordinator,
        "NIGHTLY");
}
}
