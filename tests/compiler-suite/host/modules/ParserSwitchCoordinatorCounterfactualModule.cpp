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
    "TP-PAR-561-switch-depth",
    "TP-PAR-561-shape",
    "TP-PAR-561-declarations",
    "TP-PAR-561-recovery"};

std::string makeDeepSwitchSource()
{
    std::string source;
    source.reserve(kDepth * 30U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "switch (value) { default: ";
    source += "terminal;";
    for (std::size_t index = 0; index < kDepth; ++index) source += " }";
    return source;
}

const AstNode* onlySection(const AstNode* node)
{
    return node != nullptr && node->kind() == AstNodeKind::SwitchStatement &&
        node->children().size() == 1U ? node->children()[0] : nullptr;
}

bool testDeepSwitch(std::string& detail)
{
    const std::string source = makeDeepSwitchSource();
    ParserFixture fixture(source);
    const AstNode* node = fixture.parser().parseStatement();
    std::size_t depth = 0;
    while (node != nullptr && node->kind() == AstNodeKind::SwitchStatement)
    {
        const AstNode* section = onlySection(node);
        if (section == nullptr || section->integerValue() != 1U ||
            section->children().size() != 2U || !section->range().isValid() ||
            !node->range().isValid())
        {
            detail = "switch section contract changed at depth " + std::to_string(depth);
            return false;
        }
        const AstNode* label = section->children()[0];
        if (label == nullptr || label->kind() != AstNodeKind::SwitchLabel ||
            !label->flag() || label->first() != nullptr)
        {
            detail = "default label shape changed at depth " + std::to_string(depth);
            return false;
        }
        node = section->children()[1];
        ++depth;
    }
    const bool ok = depth == kDepth && node != nullptr &&
        node->kind() == AstNodeKind::ExpressionStatement &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty();
    if (!ok) detail = "expected switch depth " + std::to_string(kDepth) +
        ", got " + std::to_string(depth);
    return ok;
}

bool groupedShapeIsCanonical(const AstNode* node)
{
    if (node == nullptr || node->kind() != AstNodeKind::SwitchStatement ||
        node->children().size() != 2U || node->first() == nullptr) return false;
    const AstNode* first = node->children()[0];
    const AstNode* second = node->children()[1];
    if (first == nullptr || second == nullptr || first->integerValue() != 2U ||
        second->integerValue() != 1U || first->children().size() != 4U ||
        second->children().size() != 2U) return false;
    return first->children()[0]->kind() == AstNodeKind::SwitchLabel &&
        !first->children()[0]->flag() && first->children()[0]->first() != nullptr &&
        first->children()[1]->kind() == AstNodeKind::SwitchLabel &&
        !first->children()[1]->flag() && first->children()[1]->first() != nullptr &&
        second->children()[0]->kind() == AstNodeKind::SwitchLabel &&
        second->children()[0]->flag() && second->children()[0]->first() == nullptr;
}

bool testShape(std::string& detail)
{
    const std::string source =
        "switch (value) { case 1: case choose(): action; break; "
        "default: default(int); }";
    ParserFixture fixture(source);
    const AstNode* node = fixture.parser().parseStatement();

    ParserFixture flatten(
        "switch (value) { case 0: let var item = source else { break; }; "
        "default: break; }");
    const AstNode* flattened = flatten.parser().parseStatement();
    const AstNode* section = flattened != nullptr && !flattened->children().empty()
        ? flattened->children()[0] : nullptr;
    bool leakedInlineWrapper = false;
    if (section != nullptr)
        for (const AstNode* child : section->children())
            leakedInlineWrapper = leakedInlineWrapper ||
                (child != nullptr && child->auxiliaryText() == "conditional-pattern-inline");

    const bool ok = groupedShapeIsCanonical(node) && node->range().isValid() &&
        node->range().begin().offset() == 0U &&
        node->range().end().offset() == source.size() &&
        fixture.parser().current().kind() == TokenKind::EndOfFile &&
        fixture.diagnostics().diagnostics().empty() && section != nullptr &&
        section->integerValue() == 1U && section->children().size() > 2U &&
        !leakedInlineWrapper && flatten.diagnostics().diagnostics().empty();
    if (!ok) detail = "labels-first shape, ranges, default expression, or inline flatten changed";
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

bool testDeclarations(std::string& detail)
{
    ParserFixture simple(
        "switch (int choice = source) { case 1: use(choice); default: break; }");
    const AstNode* wrapper = simple.parser().parseStatement();
    const AstNode* node = wrapper != nullptr && wrapper->children().size() == 2U
        ? wrapper->children()[1] : nullptr;
    const bool simpleOk = wrapper != nullptr &&
        wrapper->kind() == AstNodeKind::BlockStatement &&
        wrapper->auxiliaryText() == "condition-declaration" &&
        wrapper->children()[0]->kind() == AstNodeKind::VariableDeclarationStatement &&
        node != nullptr && node->kind() == AstNodeKind::SwitchStatement &&
        node->first() != nullptr && node->first()->text() == "choice" &&
        simple.parser().current().kind() == TokenKind::EndOfFile &&
        simple.diagnostics().diagnostics().empty();

    ParserFixture multiple(
        "switch (int first = one, second = two) { default: break; }");
    const AstNode* multipleWrapper = multiple.parser().parseStatement();
    const AstNode* multipleSwitch = multipleWrapper != nullptr &&
        multipleWrapper->children().size() == 2U ? multipleWrapper->children()[1] : nullptr;
    const bool multipleOk = multipleSwitch != nullptr && multipleSwitch->first() != nullptr &&
        multipleSwitch->first()->text() == "first" && hasExactDiagnostic(
            multiple, DiagnosticId::InvalidCondition,
            "a condition declaration must declare exactly one local");
    if (!simpleOk || !multipleOk) detail = "condition declaration wrapper or binding changed";
    return simpleOk && multipleOk;
}

bool testRecovery(std::string& detail)
{
    ParserFixture labels("switch (value) { stray; default: break; }");
    const AstNode* labelsNode = labels.parser().parseStatement();
    ParserFixture colon("switch (value) { case 1 break; }");
    colon.parser().parseStatement();
    ParserFixture close("switch (value) { default: break;");
    close.parser().parseStatement();
    ParserFixture list("switch (value) { case [1]: break; }");
    list.parser().parseStatement();

    const AstNode* recovered = onlySection(labelsNode);
    const bool shapeOk = recovered != nullptr && recovered->integerValue() == 1U;
    const bool labelDiagnostic = hasExactDiagnostic(
        labels, DiagnosticId::ExpectedToken,
        "expected 'case' or 'default' at the start of a switch section");
    const bool colonDiagnostic = hasExactDiagnostic(
        colon, DiagnosticId::ExpectedToken,
        "expected ':' after the case value, found 'break'");
    const bool closeDiagnostic = hasExactDiagnostic(
        close, DiagnosticId::ExpectedToken,
        "expected '}' to close the switch, found ''");
    // EN: Freeze 3.0 C-06: a list pattern in a case label is a deliberate rejection (list patterns are
    //     implemented in match and is), reported as UnexpectedToken with this exact message.
    // ES: Congelamiento 3.0 C-06: un list pattern en una etiqueta case es un rechazo deliberado (los
    //     list patterns estan implementados en match e is), reportado como UnexpectedToken con este texto.
    const bool listDiagnostic = !list.diagnostics().diagnostics().empty() &&
        list.diagnostics().diagnostics().front().id() == DiagnosticId::UnexpectedToken &&
        list.diagnostics().diagnostics().front().message() ==
            "list patterns are supported in 'match' and 'is', not in 'case' labels; "
            "use a match expression over the value";
    const bool cursorOk = labels.parser().current().kind() == TokenKind::EndOfFile &&
        close.parser().current().kind() == TokenKind::EndOfFile;
    const bool ok = shapeOk && labelDiagnostic && colonDiagnostic && closeDiagnostic &&
        listDiagnostic && cursorOk;
    if (!ok) detail = "recovery flags shape/label/colon/close/list/cursor=" +
        std::to_string(shapeOk) + "/" + std::to_string(labelDiagnostic) + "/" +
        std::to_string(colonDiagnostic) + "/" + std::to_string(closeDiagnostic) + "/" +
        std::to_string(listDiagnostic) + "/" + std::to_string(cursorOk);
    return ok;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepSwitch(detail);
    if (probe == kProbes[1]) return testShape(detail);
    if (probe == kProbes[2]) return testDeclarations(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testSwitchCoordinator(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-561 switch continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-561", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-561 probe must pass " + std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") + ", exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserSwitchCoordinatorCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-561",
        "CP-PAR-035",
        testSwitchCoordinator,
        "NIGHTLY");
}
}
