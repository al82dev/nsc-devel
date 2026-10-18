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
    "TP-PAR-558-try-depth",
    "TP-PAR-558-clause-order",
    "TP-PAR-558-standalone-catch",
    "TP-PAR-558-recovery"};

std::string makeDeepTrySource()
{
    std::string source;
    source.reserve(kDepth * 80U);
    for (std::size_t index = 0; index < kDepth; ++index) source += "try { ";
    source += "terminal;";
    for (std::size_t index = 0; index < kDepth; ++index)
        source += " } catch (Error error) when (ready) { recover; } finally { cleanup; }";
    return source;
}

bool validCatch(const AstNode* clause)
{
    return clause != nullptr && clause->kind() == AstNodeKind::CatchClause &&
        clause->first() != nullptr && clause->second() != nullptr &&
        clause->range().isValid();
}

bool testDeepTry(std::string& detail)
{
    ParserFixture fixture(makeDeepTrySource());
    const AstNode* node = fixture.parser().parseStatement();
    std::size_t depth = 0;
    while (node != nullptr && node->kind() == AstNodeKind::TryStatement)
    {
        const AstNode* protectedBlock = node->first();
        const AstNode* clause = node->children().size() == 1U ? node->children()[0] : nullptr;
        if (!node->range().isValid() || protectedBlock == nullptr ||
            node->second() == nullptr || !validCatch(clause))
        {
            detail = "try/catch/finally shape or range changed at depth " +
                std::to_string(depth);
            return false;
        }
        ++depth;
        node = protectedBlock->children().size() == 1U
            ? protectedBlock->children()[0] : nullptr;
    }
    if (depth != kDepth)
    {
        detail = "expected " + std::to_string(kDepth) + " try nodes, got " +
            std::to_string(depth);
        return false;
    }
    if (!fixture.diagnostics().diagnostics().empty())
    {
        detail = "valid deep try chain emitted diagnostics";
        return false;
    }
    return fixture.parser().current().kind() == TokenKind::EndOfFile;
}

bool testClauseOrder(std::string& detail)
{
    ParserFixture fixture(
        "try { protectedWork; } catch (First first) when (guard) { recoverFirst; } "
        "catch { recoverAll; } finally { cleanup; }", {"First"});
    const AstNode* node = fixture.parser().parseStatement();
    const AstNode* first = node != nullptr && node->children().size() == 2U
        ? node->children()[0] : nullptr;
    const AstNode* second = node != nullptr && node->children().size() == 2U
        ? node->children()[1] : nullptr;
    const bool ok = node != nullptr && node->kind() == AstNodeKind::TryStatement &&
        node->first() != nullptr && node->second() != nullptr && validCatch(first) &&
        second != nullptr && second->kind() == AstNodeKind::CatchClause && second->flag() &&
        second->first() != nullptr && second->second() == nullptr &&
        first->text() == "first" && first->typeSyntax() != nullptr &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok)
    {
        detail = "try=" + std::string(node != nullptr ? "yes" : "no") +
            ", kind=" + std::to_string(node != nullptr ? static_cast<int>(node->kind()) : -1) +
            ", children=" + std::to_string(node != nullptr ? node->children().size() : 0U) +
            ", protected=" + std::string(node != nullptr && node->first() != nullptr ? "yes" : "no") +
            ", finally=" + std::string(node != nullptr && node->second() != nullptr ? "yes" : "no") +
            ", first-valid=" + std::string(validCatch(first) ? "yes" : "no") +
            ", first-name=" + std::string(first != nullptr ? first->text() : "<null>") +
            ", first-type=" + std::string(first != nullptr && first->typeSyntax() != nullptr ? "yes" : "no") +
            ", second=" + std::string(second != nullptr ? "yes" : "no") +
            ", second-flag=" + std::string(second != nullptr && second->flag() ? "yes" : "no") +
            ", second-body=" + std::string(second != nullptr && second->first() != nullptr ? "yes" : "no") +
            ", second-filter=" + std::string(second != nullptr && second->second() != nullptr ? "yes" : "no") +
            ", current=" + std::to_string(static_cast<int>(fixture.parser().current().kind())) +
            ", diagnostics=" + std::to_string(fixture.diagnostics().diagnostics().size());
    }
    return ok;
}

bool testStandaloneCatch(std::string& detail)
{
    ParserFixture fixture(
        "catch (Failure error) when (ready) { recover; } trailing;", {"Failure"});
    const AstNode* clause = fixture.parser().parseCatchClause();
    const AstNode* trailing = fixture.parser().parseStatement();
    const bool ok = validCatch(clause) && clause->text() == "error" &&
        clause->typeSyntax() != nullptr && trailing != nullptr &&
        trailing->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok)
    {
        detail = "clause=" + std::string(clause != nullptr ? "yes" : "no") +
            ", valid=" + std::string(validCatch(clause) ? "yes" : "no") +
            ", name=" + std::string(clause != nullptr ? clause->text() : "<null>") +
            ", type=" + std::string(clause != nullptr && clause->typeSyntax() != nullptr ? "yes" : "no") +
            ", trailing=" + std::string(trailing != nullptr ? "yes" : "no") +
            ", trailing-kind=" + std::to_string(trailing != nullptr ? static_cast<int>(trailing->kind()) : -1) +
            ", current=" + std::to_string(static_cast<int>(fixture.parser().current().kind())) +
            ", diagnostics=" + std::to_string(fixture.diagnostics().diagnostics().size());
    }
    return ok;
}

bool testRecovery(std::string& detail)
{
    ParserFixture missingClause("try { body; } following;");
    const AstNode* firstTry = missingClause.parser().parseStatement();
    const AstNode* firstFollowing = missingClause.parser().parseStatement();
    ParserFixture missingBody("try { body; } catch (Error error) return 1;");
    const AstNode* secondTry = missingBody.parser().parseStatement();
    const AstNode* secondFollowing = missingBody.parser().parseStatement();
    const bool ok = firstTry != nullptr && firstTry->kind() == AstNodeKind::TryStatement &&
        firstFollowing != nullptr && secondTry != nullptr &&
        secondTry->kind() == AstNodeKind::TryStatement && secondFollowing != nullptr &&
        secondFollowing->kind() == AstNodeKind::ReturnStatement &&
        missingClause.diagnostics().hasErrors() && missingBody.diagnostics().hasErrors();
    if (!ok) detail = "missing clause/body recovery or following-statement progress changed";
    return ok;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepTry(detail);
    if (probe == kProbes[1]) return testClauseOrder(detail);
    if (probe == kProbes[2]) return testStandaloneCatch(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testTryContinuation(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-558 try continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-558", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-558 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserTryContinuationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-558",
        "CP-PAR-035",
        testTryContinuation,
        "NIGHTLY");
}
}
