#include "ParserExpressionTestSupport.h"
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
using namespace expression_support;
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-568-depth", "TP-PAR-568-contracts", "TP-PAR-568-interpolation",
    "TP-PAR-568-recovery"};

std::string makeDeepPrimarySource()
{
    std::string source;
    std::vector<std::string_view> closes;
    closes.reserve(kDepth);
    for (std::size_t depth = 0; depth < kDepth; ++depth)
    {
        switch (depth % 6U)
        {
        case 0U: source += '('; closes.push_back(")"); break;
        case 1U: source += '('; closes.push_back(", 0)"); break;
        case 2U: source += '['; closes.push_back("]"); break;
        case 3U: source += "[.."; closes.push_back("]"); break;
        case 4U: source += "nameof("; closes.push_back(")"); break;
        default: source += "checked("; closes.push_back(")"); break;
        }
    }
    source += "leaf";
    for (auto close = closes.rbegin(); close != closes.rend(); ++close) source += *close;
    return source;
}

AstNode* unwrapPrimary(AstNode* node, std::size_t kind)
{
    if (kind == 0U) return node;
    if (kind == 1U && node != nullptr && node->kind() == AstNodeKind::TupleExpression &&
        node->children().size() == 2U) return node->children()[0];
    if (kind == 2U && node != nullptr && node->kind() == AstNodeKind::CollectionExpression &&
        node->children().size() == 1U) return node->children()[0];
    if (kind == 3U && node != nullptr && node->kind() == AstNodeKind::CollectionExpression &&
        node->children().size() == 1U &&
        node->children()[0]->kind() == AstNodeKind::SpreadElementExpression)
        return node->children()[0]->first();
    if (kind == 4U && node != nullptr && node->kind() == AstNodeKind::NameOfExpression)
        return node->first();
    if (kind == 5U && node != nullptr && node->kind() == AstNodeKind::OverflowPolicyExpression &&
        node->auxiliaryText() == "checked") return node->first();
    return nullptr;
}

bool testDeepPrimary(std::string& detail)
{
    ParserFixture fixture(makeDeepPrimarySource());
    AstNode* node = parse(fixture);
    for (std::size_t depth = 0; depth < kDepth && node != nullptr; ++depth)
        node = unwrapPrimary(node, depth % 6U);
    const bool passed = node != nullptr && node->kind() == AstNodeKind::IdentifierExpression &&
        node->text() == "leaf" && fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "32768 mixed primary frames changed AST, cursor, or diagnostics";
    return passed;
}

bool testPrimaryContracts(std::string& detail)
{
    ParserFixture empty("()"); AstNode* emptyRoot = parse(empty);
    ParserFixture grouping("(value)"); AstNode* grouped = parse(grouping);
    ParserFixture tuple("(named: first, ..rest)"); AstNode* tupleRoot = parse(tuple);
    ParserFixture collection("[first, ..rest]"); AstNode* collectionRoot = parse(collection);
    ParserFixture generic("nameof(Box<int>)"); AstNode* genericRoot = parse(generic);
    ParserFixture atom("module::Type"); AstNode* atomRoot = parse(atom);
    const bool passed = emptyRoot != nullptr && emptyRoot->kind() == AstNodeKind::TupleExpression &&
        emptyRoot->children().empty() && grouped != nullptr && grouped->text() == "value" &&
        tupleRoot != nullptr && tupleRoot->kind() == AstNodeKind::TupleExpression &&
        tupleRoot->children().size() == 2U && tupleRoot->children()[0]->argumentName() == "named" &&
        tupleRoot->children()[1]->kind() == AstNodeKind::SpreadElementExpression &&
        collectionRoot != nullptr && collectionRoot->children().size() == 2U &&
        collectionRoot->children()[1]->kind() == AstNodeKind::SpreadElementExpression &&
        genericRoot != nullptr && genericRoot->typeSyntax() != nullptr &&
        genericRoot->typeSyntaxes().size() == 1U && atomRoot != nullptr &&
        atomRoot->text() == "module::Type";
    if (!passed) detail = "grouping/tuple/collection/nameof/token primary contract changed";
    return passed;
}

bool testInterpolation(std::string& detail)
{
    std::string source = "$\"{";
    source.append(kDepth, '(');
    source += "value";
    source.append(kDepth, ')');
    source += "}\"";
    ParserFixture deep(source); AstNode* deepRoot = parse(deep);
    ParserFixture runtime("$\"A{first}B{second:X4}C\""); AstNode* runtimeRoot = parse(runtime);
    ParserFixture nested("$\"{$\"{value}\"}\""); AstNode* nestedRoot = parse(nested);
    const bool passed = deepRoot != nullptr && firstText(deepRoot, "value") != nullptr &&
        deep.parser().at(TokenKind::EndOfFile) && deep.diagnostics().diagnostics().empty() &&
        runtimeRoot != nullptr && firstText(runtimeRoot, "first") != nullptr &&
        firstText(runtimeRoot, "ToHexString") != nullptr && runtime.diagnostics().diagnostics().empty() &&
        nestedRoot != nullptr && firstText(nestedRoot, "value") != nullptr &&
        nested.diagnostics().diagnostics().empty();
    if (!passed) detail = "interpolation holes did not remain on the primary worklist";
    return passed;
}

bool testRecovery(std::string& detail)
{
    ParserFixture tuple("(first, second"); static_cast<void>(parse(tuple));
    ParserFixture collection("[first, second"); static_cast<void>(parse(collection));
    ParserFixture nameOf("nameof(value"); static_cast<void>(parse(nameOf));
    ParserFixture overflow("checked(value"); static_cast<void>(parse(overflow));
    ParserFixture unexpected(";"); AstNode* error = unexpected.parser().parsePrimaryExpression();
    const bool passed = diagnosticCount(tuple.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(collection.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(nameOf.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(overflow.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        error != nullptr && diagnosticCount(
            unexpected.diagnostics(), DiagnosticId::ExpectedExpression) == 1U;
    if (!passed) detail = "primary recovery lost its exact diagnostic family";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepPrimary(detail);
    if (probe == kProbes[1]) return testPrimaryContracts(detail);
    if (probe == kProbes[2]) return testInterpolation(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testPrimaryPrattCounterfactual(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail), "TP-PAR-568 primary Pratt contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-568", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-568 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserPrimaryPrattCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-568", "CP-PAR-040", testPrimaryPrattCounterfactual, "NIGHTLY");
}
}
