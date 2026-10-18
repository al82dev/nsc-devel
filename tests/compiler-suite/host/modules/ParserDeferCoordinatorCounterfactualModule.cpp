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
    "TP-PAR-560-defer-depth",
    "TP-PAR-560-captures-lifo",
    "TP-PAR-560-statement-boundary",
    "TP-PAR-560-recovery"};

const AstNode* loweredDefer(const AstNode* registration)
{
    if (registration == nullptr) return nullptr;
    for (const AstNode* child : registration->children())
        if (child != nullptr && child->kind() == AstNodeKind::TryStatement) return child;
    return nullptr;
}

std::string makeDeepDeferSource()
{
    std::string source;
    source.reserve(kDepth * 14U);
    for (std::size_t index = 0; index < kDepth; ++index) source += "defer [] { ";
    source += "terminal;";
    for (std::size_t index = 0; index < kDepth; ++index) source += " }";
    return source;
}

bool testDeepDefer(std::string& detail)
{
    ParserFixture fixture(makeDeepDeferSource());
    const AstNode* registration = fixture.parser().parseStatement();
    const AstNode* lowered = loweredDefer(registration);
    std::size_t depth = 0;
    while (lowered != nullptr && lowered->kind() == AstNodeKind::TryStatement)
    {
        const AstNode* cleanup = lowered->second();
        const AstNode* body = cleanup != nullptr && cleanup->children().size() == 1U
            ? cleanup->children()[0] : nullptr;
        if (!lowered->range().isValid() || lowered->auxiliaryText() != "defer-lowered" ||
            lowered->first() == nullptr || body == nullptr ||
            body->auxiliaryText() != "defer-capture-boundary")
        {
            detail = "defer lowering or capture boundary changed at depth " +
                std::to_string(depth);
            return false;
        }
        ++depth;
        lowered = body->children().size() == 1U ? body->children()[0] : nullptr;
    }
    const bool ok = depth == kDepth && lowered != nullptr &&
        lowered->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().nextDeferTemporary_ == kDepth &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok)
        detail = "expected depth/ordinal " + std::to_string(kDepth) + ", got " +
            std::to_string(depth) + "/" +
            std::to_string(fixture.parser().nextDeferTemporary_);
    return ok;
}

bool validCaptureLowering(
    const AstNode* registration,
    std::string_view hidden,
    std::string_view allowedNames)
{
    const AstNode* lowered = loweredDefer(registration);
    const AstNode* cleanup = lowered != nullptr ? lowered->second() : nullptr;
    const AstNode* boundary = cleanup != nullptr && !cleanup->children().empty()
        ? cleanup->children().back() : nullptr;
    return registration != nullptr && registration->auxiliaryText() == "defer-registration" &&
        !registration->children().empty() && registration->children()[0]->text() == hidden &&
        registration->children()[0]->auxiliaryText() == "local:defer-value-capture" &&
        cleanup != nullptr && cleanup->children().size() == 3U &&
        cleanup->children()[0]->text() == "first" &&
        cleanup->children()[0]->auxiliaryText() == "local:defer-value-capture" &&
        cleanup->children()[1]->text() == "second" &&
        cleanup->children()[1]->auxiliaryText() == "local:ref" && boundary != nullptr &&
        boundary->text() == allowedNames && boundary->auxiliaryText() == "defer-capture-boundary";
}

bool testCapturesAndLifo(std::string& detail)
{
    ParserFixture captures(
        "defer [move first, ref second, ref this] { work; } "
        "defer [move first, ref second, ref this] { work; }");
    const AstNode* first = captures.parser().parseStatement();
    const AstNode* second = captures.parser().parseStatement();
    const bool captureOk = validCaptureLowering(
        first, "__nsc_defer_capture_0_first", "first,second,this") &&
        validCaptureLowering(
            second, "__nsc_defer_capture_1_first", "first,second,this") &&
        captures.parser().nextDeferTemporary_ == 2U &&
        captures.parser().current().kind() == TokenKind::EndOfFile &&
        captures.diagnostics().diagnostics().empty();

    ParserFixture lifo("{ defer [] { first; } defer [] { second; } work; }");
    const AstNode* block = lifo.parser().parseStatement();
    const AstNode* outer = block != nullptr && block->children().size() == 1U
        ? block->children()[0] : nullptr;
    const AstNode* protectedBlock = outer != nullptr ? outer->first() : nullptr;
    const AstNode* inner = protectedBlock != nullptr && protectedBlock->children().size() == 1U
        ? protectedBlock->children()[0] : nullptr;
    const bool lifoOk = outer != nullptr && outer->kind() == AstNodeKind::TryStatement &&
        inner != nullptr && inner->kind() == AstNodeKind::TryStatement &&
        inner->first() != nullptr && inner->first()->children().size() == 1U &&
        inner->first()->children()[0]->kind() == AstNodeKind::ExpressionStatement &&
        lifo.diagnostics().diagnostics().empty();
    if (!captureOk || !lifoOk) detail = "capture shape, monotonic ordinal, or LIFO nesting changed";
    return captureOk && lifoOk;
}

bool testStatementBoundary(std::string& detail)
{
    ParserFixture fixture(
        "defer [] { try { work; } finally { cleanup; } } trailing;");
    const AstNode* registration = fixture.parser().parseStatement();
    const AstNode* lowered = loweredDefer(registration);
    const AstNode* cleanup = lowered != nullptr ? lowered->second() : nullptr;
    const AstNode* body = cleanup != nullptr && cleanup->children().size() == 1U
        ? cleanup->children()[0] : nullptr;
    const AstNode* nestedTry = body != nullptr && body->children().size() == 1U
        ? body->children()[0] : nullptr;
    const AstNode* trailing = fixture.parser().parseStatement();
    const bool ok = nestedTry != nullptr && nestedTry->kind() == AstNodeKind::TryStatement &&
        nestedTry->first() != nullptr && nestedTry->second() != nullptr &&
        trailing != nullptr && trailing->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok) detail = "statement-to-block continuation or following cursor changed";
    return ok;
}

bool hasExactDiagnostic(
    ParserFixture& fixture,
    DiagnosticId id,
    std::string_view message)
{
    for (const auto& diagnostic : fixture.diagnostics().diagnostics())
        if (diagnostic.id() == id && diagnostic.message() == message) return true;
    return false;
}

bool testRecovery(std::string& detail)
{
    ParserFixture modifier("defer [item] { cleanup; }");
    modifier.parser().parseStatement();
    ParserFixture receiver("defer [move this] { cleanup; }");
    receiver.parser().parseStatement();
    ParserFixture duplicate("defer [ref item, move item] { cleanup; }");
    duplicate.parser().parseStatement();
    ParserFixture missingName("defer [ref] { cleanup; }");
    missingName.parser().parseStatement();
    ParserFixture missingBody("defer [] return 7;");
    const AstNode* registration = missingBody.parser().parseStatement();
    const AstNode* following = missingBody.parser().parseStatement();
    const bool ok = hasExactDiagnostic(
            modifier, DiagnosticId::ExpectedToken,
            "defer capture requires explicit 'ref' or 'move'") &&
        hasExactDiagnostic(
            receiver, DiagnosticId::InvalidOperand,
            "defer can capture this only by reference") &&
        hasExactDiagnostic(
            duplicate, DiagnosticId::DuplicateSymbol,
            "duplicate defer capture 'item'") && missingName.diagnostics().hasErrors() &&
        registration != nullptr && registration->auxiliaryText() == "defer-registration" &&
        following != nullptr && following->kind() == AstNodeKind::ReturnStatement &&
        missingBody.diagnostics().hasErrors() &&
        missingBody.parser().current().kind() == TokenKind::EndOfFile;
    if (!ok) detail = "capture/body recovery or exact diagnostics changed";
    return ok;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepDefer(detail);
    if (probe == kProbes[1]) return testCapturesAndLifo(detail);
    if (probe == kProbes[2]) return testStatementBoundary(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testDeferCoordinator(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-560 defer continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-560", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-560 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserDeferCoordinatorCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-560",
        "CP-PAR-035",
        testDeferCoordinator,
        "NIGHTLY");
}
}
