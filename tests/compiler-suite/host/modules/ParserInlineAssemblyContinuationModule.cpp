#include "ParserFoundationTestSupport.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(TestRegistry& registry, std::string id, TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-029", "Host.Parser", "component", "PR",
        {"parser", "statement", "inline-assembly"}, std::move(function), disposition});
}

AstNode* parseAsm(ParserFixture& fixture)
{
    return fixture.parser().parseStatement();
}

std::vector<const AstNode*> childrenOfKind(const AstNode* node, AstNodeKind kind)
{
    std::vector<const AstNode*> result;
    if (node == nullptr) return result;
    for (const AstNode* child : node->children())
        if (child != nullptr && child->kind() == kind) result.push_back(child);
    return result;
}

void testEmptyAssembly(TestContext& context)
{
    ParserFixture fixture("asm {}");
    AstNode* const node = parseAsm(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::InlineAssemblyStatement &&
            node->children().empty() && node->text().empty() && node->range().isValid() &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-315 empty contextual asm must produce one empty assembly statement");
}

void testAssemblyTargetAndDialectMetadata(TestContext& context)
{
    ParserFixture fixture("asm(target = x64, dialect = intel) { nop }");
    AstNode* const node = parseAsm(fixture);
    const auto lines = childrenOfKind(node, AstNodeKind::InlineAssemblyLine);
    context.expect(node != nullptr && node->kind() == AstNodeKind::InlineAssemblyStatement &&
            node->auxiliaryText() == "target:x86_64-pc-windows-msvc;dialect:intel" &&
            node->text() == " nop " && lines.size() == 1 && lines[0]->text() == "nop" &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-316 explicit valid target and dialect must retain canonical metadata");
}

void testInvalidAssemblyTarget(TestContext& context)
{
    ParserFixture fixture("asm { vaddps xmm0, xmm1, xmm2 }");
    AstNode* const node = parseAsm(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::InlineAssemblyStatement &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidInlineAssemblyTarget) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-317 an instruction outside the configured CPU baseline must report its target error");
}

void testAssemblyRawLines(TestContext& context)
{
    ParserFixture fixture("asm {\n  mov rax, rbx\n\tadd rax, 2\n}");
    AstNode* const node = parseAsm(fixture);
    const auto lines = childrenOfKind(node, AstNodeKind::InlineAssemblyLine);
    context.expect(node != nullptr && node->text() == "\n  mov rax, rbx\n\tadd rax, 2\n" &&
            lines.size() == 2 && lines[0]->text() == "mov rax, rbx" &&
            lines[1]->text() == "add rax, 2" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-318 assembly statement must preserve exact body bytes and trimmed physical-line nodes");
}

void testOutputOperands(TestContext& context)
{
    ParserFixture fixture("asm { mov $out0, 1 } out(result, exact : \"={rax}\");");
    AstNode* const node = parseAsm(fixture);
    const auto outputs = childrenOfKind(node, AstNodeKind::InlineAssemblyOutputOperand);
    context.expect(outputs.size() == 2 && outputs[0]->integerValue() == 0 &&
            outputs[0]->auxiliaryText() == "=r" && !outputs[0]->flag() &&
            outputs[0]->first() != nullptr && outputs[0]->first()->text() == "result" &&
            outputs[1]->integerValue() == 1 && outputs[1]->auxiliaryText() == "={rax}" &&
            outputs[1]->flag() && outputs[1]->first() != nullptr &&
            outputs[1]->first()->text() == "exact" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-319 implicit and explicit outputs must retain ordinal, constraint, and target expression");
}

void testInputOperands(TestContext& context)
{
    ParserFixture fixture("asm { add rax, $0 } in(source, 2 : \"i\");");
    AstNode* const node = parseAsm(fixture);
    const auto inputs = childrenOfKind(node, AstNodeKind::InlineAssemblyInputOperand);
    context.expect(inputs.size() == 2 && inputs[0]->integerValue() == 0 &&
            inputs[0]->auxiliaryText() == "r" && !inputs[0]->flag() &&
            inputs[0]->first() != nullptr && inputs[0]->first()->text() == "source" &&
            inputs[1]->integerValue() == 1 && inputs[1]->auxiliaryText() == "i" &&
            inputs[1]->flag() && inputs[1]->first() != nullptr &&
            inputs[1]->first()->integerValue() == 2 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-320 implicit and explicit inputs must retain ordinal, constraint, and expression");
}

void testValidClobbers(TestContext& context)
{
    ParserFixture fixture("asm { nop } clobber(\"RAX\", \"cc\", \"memory\");");
    AstNode* const node = parseAsm(fixture);
    const auto clobbers = childrenOfKind(node, AstNodeKind::InlineAssemblyClobber);
    context.expect(clobbers.size() == 3 && clobbers[0]->text() == "rax" &&
            clobbers[0]->auxiliaryText() == "register" && clobbers[0]->integerValue() == 0 &&
            clobbers[1]->text() == "cc" && clobbers[1]->auxiliaryText() == "flags" &&
            clobbers[2]->text() == "memory" && clobbers[2]->auxiliaryText() == "memory" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-321 clobbers must be canonicalized and retain exact source order and category");
}

void testInvalidAndDuplicateClobbers(TestContext& context)
{
    ParserFixture invalid("asm { nop } clobber(\"r ax\");");
    ParserFixture duplicate("asm { nop } clobber(\"rax\", \"RAX\");");
    parseAsm(invalid);
    parseAsm(duplicate);
    context.expect(diagnosticCount(invalid.diagnostics(), DiagnosticId::InvalidInlineAssemblyClobber) == 1 &&
            diagnosticCount(duplicate.diagnostics(), DiagnosticId::InvalidInlineAssemblyClobber) == 1,
        "TP-PAR-322 malformed and duplicate clobbers must report the clobber-specific diagnostic");
}

void testAllowedOutputConstraints(TestContext& context)
{
    constexpr std::array<std::string_view, 6> constraints = {
        "=r", "+r", "=m", "+m", "={rax}", "+{r11}"};
    for (const std::string_view constraint : constraints)
    {
        ParserFixture fixture("asm { nop } out(value : \"" + std::string(constraint) + "\");");
        AstNode* const node = parseAsm(fixture);
        const auto outputs = childrenOfKind(node, AstNodeKind::InlineAssemblyOutputOperand);
        context.expect(outputs.size() == 1 && outputs[0]->auxiliaryText() == constraint &&
                outputs[0]->flag() && fixture.diagnostics().diagnostics().empty(),
            "TP-PAR-323 every allowed output constraint must be accepted and retained verbatim");
    }
}

void testInvalidOutputConstraint(TestContext& context)
{
    ParserFixture fixture("asm { nop } out(value : \"r\");");
    AstNode* const node = parseAsm(fixture);
    const auto outputs = childrenOfKind(node, AstNodeKind::InlineAssemblyOutputOperand);
    context.expect(outputs.size() == 1 && outputs[0]->auxiliaryText() == "=r" &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidInlineAssemblyConstraint) == 1,
        "TP-PAR-324 invalid output constraint must diagnose and recover to the default output constraint");
}

void testAllowedInputConstraints(TestContext& context)
{
    constexpr std::array<std::string_view, 4> constraints = {"r", "i", "m", "{rcx}"};
    for (const std::string_view constraint : constraints)
    {
        ParserFixture fixture("asm { nop } in(value : \"" + std::string(constraint) + "\");");
        AstNode* const node = parseAsm(fixture);
        const auto inputs = childrenOfKind(node, AstNodeKind::InlineAssemblyInputOperand);
        context.expect(inputs.size() == 1 && inputs[0]->auxiliaryText() == constraint &&
                inputs[0]->flag() && fixture.diagnostics().diagnostics().empty(),
            "TP-PAR-325 every allowed input constraint must be accepted and retained verbatim");
    }
}

void testInvalidInputConstraint(TestContext& context)
{
    ParserFixture fixture("asm { nop } in(value : \"=r\");");
    AstNode* const node = parseAsm(fixture);
    const auto inputs = childrenOfKind(node, AstNodeKind::InlineAssemblyInputOperand);
    context.expect(inputs.size() == 1 && inputs[0]->auxiliaryText() == "r" &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidInlineAssemblyConstraint) == 1,
        "TP-PAR-326 invalid input constraint must diagnose and recover to the default input constraint");
}

void testIndexAndNamedPlaceholders(TestContext& context)
{
    ParserFixture fixture("asm { add $out0, $source } out(result) in(source);");
    ParserFixture missing("asm { add $out0, $missing } out(result) in(source);");
    AstNode* const node = parseAsm(fixture);
    parseAsm(missing);
    context.expect(node != nullptr && node->kind() == AstNodeKind::InlineAssemblyStatement &&
            fixture.diagnostics().diagnostics().empty() &&
            diagnosticCount(missing.diagnostics(), DiagnosticId::InvalidInlineAssemblyPlaceholder) == 1,
        "TP-PAR-327 indexed and named placeholders must resolve only to their declared operands");
}

void testMissingPlaceholder(TestContext& context)
{
    ParserFixture fixture("asm { mov eax, $1 } in(value);");
    parseAsm(fixture);
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidInlineAssemblyPlaceholder) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-328 missing placeholder must report its dedicated diagnostic and finish parsing");
}

void testAllowedLocalControlFlow(TestContext& context)
{
    ParserFixture fixture("asm {\n jmp Done\n mov rax, 0\nDone:\n}");
    AstNode* const node = parseAsm(fixture);
    const auto lines = childrenOfKind(node, AstNodeKind::InlineAssemblyLine);
    context.expect(lines.size() == 3 && lines[0]->auxiliaryText() == "branch:jmp:done" &&
            lines[2]->auxiliaryText() == "label:done" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-329 a local branch must resolve only to a unique label in the same asm block");
}

void testForbiddenAssemblyControlFlow(TestContext& context)
{
    ParserFixture external("asm { jmp Outside }");
    ParserFixture call("asm { call target }");
    ParserFixture duplicate("asm {\nSame:\nSame:\n}");
    parseAsm(external);
    parseAsm(call);
    parseAsm(duplicate);
    context.expect(diagnosticCount(external.diagnostics(), DiagnosticId::InvalidInlineAssemblyControlFlow) == 1 &&
            diagnosticCount(call.diagnostics(), DiagnosticId::InvalidInlineAssemblyControlFlow) == 1 &&
            diagnosticCount(duplicate.diagnostics(), DiagnosticId::InvalidInlineAssemblyControlFlow) == 1,
        "TP-PAR-330 external branches, calls, and duplicate labels must be rejected as control-flow escapes");
}

void testMissingClauseDelimiter(TestContext& context)
{
    ParserFixture fixture("asm { nop } in(value) return 0;");
    AstNode* const assembly = parseAsm(fixture);
    AstNode* const following = parseAsm(fixture);
    context.expect(assembly != nullptr && assembly->kind() == AstNodeKind::InlineAssemblyStatement &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            following != nullptr && following->kind() == AstNodeKind::ReturnStatement &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-331 missing asm clause terminator must diagnose without consuming the following statement");
}

void testAssemblyStatementBoundary(TestContext& context)
{
    ParserFixture fixture("asm { nop } return 42;");
    AstNode* const assembly = parseAsm(fixture);
    AstNode* const following = parseAsm(fixture);
    context.expect(assembly != nullptr && assembly->kind() == AstNodeKind::InlineAssemblyStatement &&
            following != nullptr && following->kind() == AstNodeKind::ReturnStatement &&
            following->first() != nullptr && following->first()->integerValue() == 42 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-332 asm lexical mode must end exactly at its brace and preserve the following statement");
}
}

void registerParserInlineAssemblyContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-315", testEmptyAssembly);
    addCase(registry, "TP-PAR-316", testAssemblyTargetAndDialectMetadata);
    addCase(registry, "TP-PAR-317", testInvalidAssemblyTarget);
    addCase(registry, "TP-PAR-318", testAssemblyRawLines);
    addCase(registry, "TP-PAR-319", testOutputOperands);
    addCase(registry, "TP-PAR-320", testInputOperands);
    addCase(registry, "TP-PAR-321", testValidClobbers);
    addCase(registry, "TP-PAR-322", testInvalidAndDuplicateClobbers);
    addCase(registry, "TP-PAR-323", testAllowedOutputConstraints);
    addCase(registry, "TP-PAR-324", testInvalidOutputConstraint);
    addCase(registry, "TP-PAR-325", testAllowedInputConstraints);
    addCase(registry, "TP-PAR-326", testInvalidInputConstraint);
    addCase(registry, "TP-PAR-327", testIndexAndNamedPlaceholders);
    addCase(registry, "TP-PAR-328", testMissingPlaceholder);
    addCase(registry, "TP-PAR-329", testAllowedLocalControlFlow);
    addCase(registry, "TP-PAR-330", testForbiddenAssemblyControlFlow);
    addCase(registry, "TP-PAR-331", testMissingClauseDelimiter);
    addCase(registry, "TP-PAR-332", testAssemblyStatementBoundary);
}
}
