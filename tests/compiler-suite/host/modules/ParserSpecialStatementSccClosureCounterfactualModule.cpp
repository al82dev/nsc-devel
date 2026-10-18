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
constexpr std::array<std::string_view, 5> kProbes{
    "TP-PAR-564-depth",
    "TP-PAR-564-scopes",
    "TP-PAR-564-asm",
    "TP-PAR-564-yield",
    "TP-PAR-564-recovery"};

std::size_t countKind(const AstNode* root, AstNodeKind kind)
{
    std::size_t count = 0;
    for (const AstNode* node : flatten(root))
        count += node != nullptr && node->kind() == kind ? 1U : 0U;
    return count;
}

bool hasDiagnostic(ParserFixture& fixture, DiagnosticId id, std::string_view text)
{
    for (const auto& diagnostic : fixture.diagnostics().diagnostics())
        if (diagnostic.id() == id && diagnostic.message().find(text) != std::string::npos)
            return true;
    return false;
}

std::string makeDeepSpecialSource()
{
    std::string source = "fp(strict) { ";
    source.reserve(kDepth * 22U);
    for (std::size_t depth = 1; depth < kDepth; ++depth)
        source += "unsafe(\"depth\") { ";
    source += "asm { nop } yield return value; ";
    for (std::size_t depth = 1; depth < kDepth; ++depth) source += " }";
    source += " }";
    return source;
}

bool testDepth(std::string& detail)
{
    ParserFixture fixture(makeDeepSpecialSource());
    fixture.parser().comprehensionContexts_.push_back(
        Parser::ComprehensionContext{"deep-builder", "deep-loop"});
    const AstNode* root = fixture.parser().parseStatement();
    fixture.parser().comprehensionContexts_.clear();
    const bool passed = root != nullptr && countAuxiliary(root, "unsafe-block") == kDepth - 1U &&
        countKind(root, AstNodeKind::InlineAssemblyStatement) == 1U &&
        countAuxiliary(root, "comprehension-builder-add") == 1U &&
        root->floatingPointMode() == FloatingPointModeSyntaxKind::Strict &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = "deep mixed scopes/asm/yield shape, cursor, diagnostics, or FP mode changed";
    return passed;
}

bool testScopes(std::string& detail)
{
    ParserFixture fp("fp(strict) { fp(fast) { inner; } outer; }");
    const AstNode* root = fp.parser().parseStatement();
    const AstNode* nested = root != nullptr && !root->children().empty()
        ? root->children()[0] : nullptr;
    const AstNode* outer = root != nullptr && root->children().size() == 2U
        ? root->children()[1] : nullptr;
    ParserFixture unsafe(
        "unsafe { a;b;c;d;e;f;g;h;i; } unsafe(\"reviewed\") { a;b;c;d;e;f;g;h;i; }");
    const AstNode* broad = unsafe.parser().parseStatement();
    const AstNode* reasoned = unsafe.parser().parseStatement();
    const bool passed = root != nullptr && nested != nullptr && outer != nullptr &&
        root->floatingPointMode() == FloatingPointModeSyntaxKind::Strict &&
        nested->floatingPointMode() == FloatingPointModeSyntaxKind::Fast &&
        outer->floatingPointMode() == FloatingPointModeSyntaxKind::Strict &&
        broad != nullptr && reasoned != nullptr && reasoned->text() == "\"reviewed\"" &&
        diagnosticCount(unsafe.diagnostics(), DiagnosticId::NS3030MissingEffectContext) == 1U &&
        fp.diagnostics().diagnostics().empty();
    if (!passed)
    {
        detail = "root=" + std::string(root == nullptr ? "null" : "set") +
            ", children=" + std::to_string(root == nullptr ? 0U : root->children().size()) +
            ", nested=" + std::string(nested == nullptr ? "null" : "set") +
            ", outer=" + std::string(outer == nullptr ? "null" : "set") +
            ", root-mode=" + std::to_string(root == nullptr ? -1 :
                static_cast<int>(root->floatingPointMode())) +
            ", nested-mode=" + std::to_string(nested == nullptr ? -1 :
                static_cast<int>(nested->floatingPointMode())) +
            ", outer-mode=" + std::to_string(outer == nullptr ? -1 :
                static_cast<int>(outer->floatingPointMode())) +
            ", unsafe-reason='" + std::string(reasoned == nullptr ? "<null>" : reasoned->text()) +
            "', warnings=" + std::to_string(diagnosticCount(
                unsafe.diagnostics(), DiagnosticId::NS3030MissingEffectContext)) +
            ", fp-diagnostics=" + std::to_string(fp.diagnostics().diagnostics().size());
    }
    return passed;
}

bool testAssembly(std::string& detail)
{
    ParserFixture fixture(
        "asm(target=x64,dialect=intel) { mov $out0, $0 } "
        "out((*result) : \"=m\") in(source = value, () => { return 1; } : \"r\") "
        "clobber(\"cc\", \"memory\"); trailing;");
    const AstNode* assembly = fixture.parser().parseStatement();
    const AstNode* trailing = fixture.parser().parseStatement();
    const std::size_t outputs = countKind(assembly, AstNodeKind::InlineAssemblyOutputOperand);
    const std::size_t inputs = countKind(assembly, AstNodeKind::InlineAssemblyInputOperand);
    const std::size_t clobbers = countKind(assembly, AstNodeKind::InlineAssemblyClobber);
    const bool passed = assembly != nullptr &&
        assembly->auxiliaryText() == "target:x86_64-pc-windows-msvc;dialect:intel" &&
        outputs == 1U && inputs == 2U && clobbers == 2U &&
        trailing != nullptr && trailing->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
    {
        detail = "assembly=" + std::string(assembly == nullptr ? "null" : "set") +
            ", metadata='" + std::string(assembly == nullptr ? "<null>" :
                assembly->auxiliaryText()) + "', outputs=" + std::to_string(outputs) +
            ", inputs=" + std::to_string(inputs) + ", clobbers=" +
            std::to_string(clobbers) + ", trailing-kind=" +
            std::to_string(trailing == nullptr ? -1 : static_cast<int>(trailing->kind())) +
            ", current=" + std::to_string(static_cast<int>(fixture.parser().current().kind())) +
            ", diagnostics=" + std::to_string(fixture.diagnostics().diagnostics().size());
    }
    return passed;
}

bool testComprehensionYield(std::string& detail)
{
    ParserFixture fixture("yield break; yield value; yield return next;");
    fixture.parser().comprehensionContexts_.push_back(
        Parser::ComprehensionContext{"builder", "loop"});
    const AstNode* stop = fixture.parser().parseStatement();
    const AstNode* value = fixture.parser().parseStatement();
    const AstNode* returned = fixture.parser().parseStatement();
    fixture.parser().comprehensionContexts_.clear();
    const bool passed = stop != nullptr && stop->kind() == AstNodeKind::BreakStatement &&
        stop->text() == "loop" && stop->first() != nullptr &&
        stop->first()->auxiliaryText() == "comprehension-result" &&
        countAuxiliary(value, "comprehension-builder-add") == 1U &&
        countAuxiliary(returned, "comprehension-builder-add") == 1U &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "yield break/value/return builder or loop shape changed";
    return passed;
}

bool testRecovery(std::string& detail)
{
    ParserFixture fp("fp(other) { body; } trailing;");
    static_cast<void>(fp.parser().parseStatement());
    const AstNode* fpTrailing = fp.parser().parseStatement();
    ParserFixture unsafe("unsafe(123) { body; } trailing;");
    static_cast<void>(unsafe.parser().parseStatement());
    const AstNode* unsafeTrailing = unsafe.parser().parseStatement();
    ParserFixture assembly("asm { nop } in(value) return 0;");
    static_cast<void>(assembly.parser().parseStatement());
    const AstNode* asmTrailing = assembly.parser().parseStatement();
    const bool passed = hasDiagnostic(fp, DiagnosticId::ExpectedToken,
            "floating-point scope mode must be strict, precise, or fast") &&
        hasDiagnostic(unsafe, DiagnosticId::ExpectedToken, "unsafe block reason string") &&
        hasDiagnostic(assembly, DiagnosticId::ExpectedToken,
            "';' after asm operand clauses") && fpTrailing != nullptr &&
        unsafeTrailing != nullptr && asmTrailing != nullptr &&
        asmTrailing->kind() == AstNodeKind::ReturnStatement;
    if (!passed) detail = "special-statement recovery or following cursor changed";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDepth(detail);
    if (probe == kProbes[1]) return testScopes(detail);
    if (probe == kProbes[2]) return testAssembly(detail);
    if (probe == kProbes[3]) return testComprehensionYield(detail);
    if (probe == kProbes[4]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testSpecialStatementClosure(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-564 special-statement closure must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-564", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-564 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserSpecialStatementSccClosureCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-564",
        "CP-PAR-036",
        testSpecialStatementClosure,
        "NIGHTLY");
}
}
