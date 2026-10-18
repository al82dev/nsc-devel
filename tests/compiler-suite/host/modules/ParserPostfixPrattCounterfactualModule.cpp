#include "ParserExpressionTestSupport.h"
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
using namespace expression_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-567-call-binary", "TP-PAR-567-mixed", "TP-PAR-567-metadata",
    "TP-PAR-567-recovery"};

bool checkCallBinaryDepth(AstNode* node)
{
    for (std::size_t depth = 0; depth < kDepth; ++depth)
    {
        if (node == nullptr || node->kind() != AstNodeKind::CallExpression ||
            node->children().size() != 1U) return false;
        AstNode* binary = node->children()[0];
        if (binary == nullptr || binary->kind() != AstNodeKind::BinaryExpression ||
            binary->tokenKind() != TokenKind::Plus) return false;
        node = binary->second();
    }
    return node != nullptr && node->kind() == AstNodeKind::IdentifierExpression &&
        node->text() == "leaf";
}

bool testCallBinaryDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 8U + 4U);
    for (std::size_t depth = 0; depth < kDepth; ++depth) source += "f(1 + ";
    source += "leaf";
    for (std::size_t depth = 0; depth < kDepth; ++depth) source += ')';
    ParserFixture fixture(source);
    AstNode* root = parse(fixture);
    const bool passed = checkCallBinaryDepth(root) && fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "32768 call/binary continuations changed AST, cursor, or diagnostics";
    return passed;
}

std::string makeMixedSource()
{
    std::string source;
    source.reserve(kDepth * 24U + 4U);
    for (std::size_t depth = 0; depth < kDepth; ++depth)
    {
        if (depth % 4U == 0U) source += "call(";
        else if (depth % 4U == 1U) source += "box[0..";
        else if (depth % 4U == 2U) source += "seed with { Value = ";
        else source += "box?[";
    }
    source += "leaf";
    for (std::size_t depth = kDepth; depth != 0; --depth)
    {
        const std::size_t kind = (depth - 1U) % 4U;
        source += kind == 2U ? " }" : "]";
        if (kind == 0U) source.back() = ')';
    }
    return source;
}

AstNode* mixedChild(AstNode* node, std::size_t kind)
{
    if (node == nullptr) return nullptr;
    if (kind == 0U && node->kind() == AstNodeKind::CallExpression && node->children().size() == 1U)
        return node->children()[0];
    if (kind == 1U && node->kind() == AstNodeKind::IndexExpression && node->second() != nullptr &&
        node->second()->kind() == AstNodeKind::RangeExpression) return node->second()->second();
    if (kind == 2U && node->kind() == AstNodeKind::WithExpression && node->children().size() == 1U &&
        node->children()[0]->argumentName() == "Value") return node->children()[0];
    if (kind == 3U && node->kind() == AstNodeKind::NullConditionalIndexExpression &&
        node->children().size() == 1U) return node->children()[0];
    return nullptr;
}

bool testMixedDepth(std::string& detail)
{
    ParserFixture fixture(makeMixedSource());
    AstNode* node = parse(fixture);
    for (std::size_t depth = 0; depth < kDepth && node != nullptr; ++depth)
        node = mixedChild(node, depth % 4U);
    const bool passed = node != nullptr && node->text() == "leaf" &&
        fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "32768 call/index/range/with continuations changed order or shape";
    return passed;
}

bool testMetadata(std::string& detail)
{
    ParserFixture call("Box<int>.Make(named: value, ref target, out output, in input)");
    AstNode* root = parse(call);
    ParserFixture flat("source.member?[0](arg)?.tail++?");
    AstNode* chain = parse(flat);
    ParserFixture builtin("assert(value)");
    AstNode* intrinsic = parse(builtin);
    const bool passed = root != nullptr && root->kind() == AstNodeKind::CallExpression &&
        root->children().size() == 4U && root->children()[0]->argumentName() == "named" &&
        root->children()[1]->auxiliaryText() == "pass:ref" &&
        root->children()[2]->auxiliaryText() == "pass:out" &&
        root->children()[3]->auxiliaryText() == "pass:in" && chain != nullptr &&
        chain->kind() == AstNodeKind::ResultPropagationExpression && intrinsic != nullptr &&
        intrinsic->auxiliaryText() == "builtin-assert" && call.diagnostics().diagnostics().empty() &&
        flat.diagnostics().diagnostics().empty() && builtin.diagnostics().diagnostics().empty();
    if (!passed) detail = "generic/named/pass-mode/builtin or mixed suffix metadata changed";
    return passed;
}

bool testRecovery(std::string& detail)
{
    ParserFixture call("run(value");
    ParserFixture index("items[begin..");
    ParserFixture member("source?.");
    ParserFixture with("source with { Value 1 }");
    static_cast<void>(parse(call));
    static_cast<void>(parse(index));
    static_cast<void>(parse(member));
    static_cast<void>(parse(with));
    const bool passed = diagnosticCount(call.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(index.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(member.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(with.diagnostics(), DiagnosticId::ExpectedToken) != 0U;
    if (!passed) detail = "postfix recovery lost an expected-token diagnostic";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testCallBinaryDepth(detail);
    if (probe == kProbes[1]) return testMixedDepth(detail);
    if (probe == kProbes[2]) return testMetadata(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testPostfixPrattCounterfactual(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail), "TP-PAR-567 postfix Pratt contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-567", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-567 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserPostfixPrattCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-567", "CP-PAR-039", testPostfixPrattCounterfactual, "NIGHTLY");
}
}
