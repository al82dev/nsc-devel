#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <string>
#include <vector>

namespace
{
struct ParsedSyntax
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    nativesharp::ast::AstArena arena;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(ParsedSyntax& syntax, std::string source)
{
    const nativesharp::FileId fileId = syntax.sources.addBuffer("assembly_grammar.ns", std::move(source));
    const nativesharp::source::SourceFile* file = syntax.sources.getFile(fileId);
    nativesharp::lexer::Lexer lexer(*file, syntax.diagnostics);
    syntax.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(syntax.tokens, syntax.arena, syntax.diagnostics);
    syntax.root = parser.parseCompilationUnit();
}

const nativesharp::ast::AstNode* findFunction(
    const nativesharp::ast::AstNode* root,
    std::string_view name)
{
    if (root == nullptr) return nullptr;
    for (const nativesharp::ast::AstNode* child : root->children())
    {
        if (child->kind() == nativesharp::ast::AstNodeKind::FunctionDeclaration &&
            child->text() == name)
        {
            return child;
        }
    }
    return nullptr;
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::ast;
    using nativesharp::tests::TestContext;

    TestContext tests;

    ParsedSyntax valid;
    parse(
        valid,
        R"NS(
int Main()
{
    int left = 40;
    int right = 2;
    asm {
        mov rax, $0
        add rax, $1
    } in(left, right);
    return 42;
}
)NS");
    tests.expect(!valid.diagnostics.hasErrors(), "asm block grammar parses without diagnostics");
    const AstNode* mainFunction = findFunction(valid.root, "Main");
    const AstNode* body = mainFunction != nullptr ? mainFunction->first() : nullptr;
    const AstNode* assembly = body != nullptr && body->children().size() > 2
        ? body->children()[2]
        : nullptr;
    tests.expect(
        assembly != nullptr && assembly->kind() == AstNodeKind::InlineAssemblyStatement,
        "asm block has an explicit InlineAssemblyStatement AST node");
    tests.expect(
        assembly != nullptr &&
            assembly->text() == "\n        mov rax, $0\n        add rax, $1\n    ",
        "assembly body retains its exact source bytes including physical newlines");
    tests.expect(
        assembly != nullptr && assembly->range().isValid(),
        "asm node covers a valid whole-statement source range");
    tests.expect(
        body != nullptr && body->children().size() == 4 &&
            body->children()[3]->kind() == AstNodeKind::ReturnStatement,
        "parsing resumes at the statement following the terminated asm operand clauses");
    tests.expect(
        assembly != nullptr && assembly->children().size() == 4 &&
            assembly->children()[0]->kind() == AstNodeKind::InlineAssemblyLine &&
            assembly->children()[0]->text() == "mov rax, $0" &&
            assembly->children()[1]->text() == "add rax, $1",
        "non-blank physical assembly lines have explicit exact-spelling AST records");
    if (assembly != nullptr && assembly->children().size() >= 2)
    {
        const source::SourcePosition firstPosition =
            valid.sources.getPosition(assembly->children()[0]->range().begin());
        const source::SourcePosition secondPosition =
            valid.sources.getPosition(assembly->children()[1]->range().begin());
        tests.expect(
            firstPosition.line == 7 && firstPosition.column == 9 &&
                secondPosition.line == 8 && secondPosition.column == 9,
            "assembly line records begin at their exact one-based instruction locations");
    }
    else
    {
        tests.expect(false, "assembly line source locations are available");
    }

    bool sawRawText = false;
    bool sawRawNewLine = false;
    for (const lexer::Token& token : valid.tokens)
    {
        sawRawText = sawRawText || token.kind() == lexer::TokenKind::AssemblyText;
        sawRawNewLine = sawRawNewLine || token.kind() == lexer::TokenKind::AssemblyNewLine;
    }
    tests.expect(
        sawRawText && sawRawNewLine,
        "lexer switches to exact assembly text and newline tokens inside asm braces");
    if (assembly != nullptr && assembly->children().size() == 4)
    {
        const AstNode* firstInput = assembly->children()[2];
        const AstNode* secondInput = assembly->children()[3];
        tests.expect(
            firstInput->kind() == AstNodeKind::InlineAssemblyInputOperand &&
                firstInput->integerValue() == 0 && firstInput->first() != nullptr &&
                firstInput->first()->kind() == AstNodeKind::IdentifierExpression &&
                firstInput->first()->text() == "left" &&
                secondInput->kind() == AstNodeKind::InlineAssemblyInputOperand &&
                secondInput->integerValue() == 1 && secondInput->first() != nullptr &&
                secondInput->first()->text() == "right",
            "asm in clause records expressions in stable zero-based positional order");
    }
    else
    {
        tests.expect(false, "asm input operands are present in the AST");
    }

    ParsedSyntax balanced;
    parse(balanced, "int Main() { asm { outer { inner } } return 0; }");
    tests.expect(
        !balanced.diagnostics.hasErrors(),
        "balanced braces inside an asm block preserve the outer grammar boundary");

    ParsedSyntax contextualIdentifier;
    parse(contextualIdentifier, "int Main() { int asm = 40; asm = asm + 2; return asm; }");
    tests.expect(
        !contextualIdentifier.diagnostics.hasErrors(),
        "asm remains an ordinary identifier when it is not followed by an assembly block");

    ParsedSyntax assemblyCharacters;
    parse(
        assemblyCharacters,
        "int Main() { asm { mov rax, [rbx + rcx*4] ; $ @ # stay raw } return 0; }");
    tests.expect(
        !assemblyCharacters.diagnostics.hasErrors(),
        "assembly lexical mode preserves punctuation that is invalid in ordinary NativeSharp");

    ParsedSyntax emptyInputs;
    parse(emptyInputs, "int Main() { asm { nop } in(); return 0; }");
    bool sawRequiredInput = false;
    for (const diagnostics::Diagnostic& diagnostic : emptyInputs.diagnostics.diagnostics())
    {
        sawRequiredInput = sawRequiredInput ||
            diagnostic.message().find("requires at least one input expression") != std::string::npos;
    }
    tests.expect(
        emptyInputs.diagnostics.hasErrors() && sawRequiredInput,
        "empty asm in clause is rejected at its closing parenthesis");

    ParsedSyntax outputs;
    parse(
        outputs,
        "int Main() { int result; int source = 42; "
        "asm { mov $out0, $0 } out(result) in(source); return result; }");
    const AstNode* outputMain = findFunction(outputs.root, "Main");
    const AstNode* outputBody = outputMain != nullptr ? outputMain->first() : nullptr;
    const AstNode* outputAssembly = outputBody != nullptr && outputBody->children().size() > 2
        ? outputBody->children()[2]
        : nullptr;
    const AstNode* outputOperand = outputAssembly != nullptr && outputAssembly->children().size() > 1
        ? outputAssembly->children()[1]
        : nullptr;
    tests.expect(
        !outputs.diagnostics.hasErrors() && outputOperand != nullptr &&
            outputOperand->kind() == AstNodeKind::InlineAssemblyOutputOperand &&
            outputOperand->integerValue() == 0 && outputOperand->first() != nullptr &&
            outputOperand->first()->text() == "result" &&
            outputOperand->auxiliaryText() == "=r" && !outputOperand->flag() &&
            outputAssembly->children().size() == 3 &&
            outputAssembly->children()[2]->kind() == AstNodeKind::InlineAssemblyInputOperand &&
            outputAssembly->children()[2]->auxiliaryText() == "r" &&
            !outputAssembly->children()[2]->flag(),
        "asm operands retain stable ordinals and normalized default register constraints");

    ParsedSyntax explicitConstraints;
    parse(
        explicitConstraints,
        "int Main() { int result; int source = 42; "
        "asm { mov $out0, $0 } out(result : \"={rax}\") in(source : \"{rcx}\"); "
        "return result; }");
    const AstNode* constrainedMain = findFunction(explicitConstraints.root, "Main");
    const AstNode* constrainedBody = constrainedMain != nullptr ? constrainedMain->first() : nullptr;
    const AstNode* constrainedAssembly =
        constrainedBody != nullptr && constrainedBody->children().size() > 2
            ? constrainedBody->children()[2]
            : nullptr;
    tests.expect(
        !explicitConstraints.diagnostics.hasErrors() && constrainedAssembly != nullptr &&
            constrainedAssembly->children().size() == 3 &&
            constrainedAssembly->children()[1]->auxiliaryText() == "={rax}" &&
            constrainedAssembly->children()[1]->flag() &&
            constrainedAssembly->children()[2]->auxiliaryText() == "{rcx}" &&
            constrainedAssembly->children()[2]->flag(),
        "explicit fixed-register constraints are decoded and attached to their operands");

    ParsedSyntax readWriteOutput;
    parse(
        readWriteOutput,
        "int Main() { int value = 1; asm { inc $out0 } out(value : \"+r\"); return value; }");
    tests.expect(
        !readWriteOutput.diagnostics.hasErrors(),
        "read-write general-register output constraint is accepted");

    ParsedSyntax invalidInputConstraint;
    parse(
        invalidInputConstraint,
        "int Main() { int value = 1; asm { nop } in(value : \"=r\"); return 0; }");
    bool sawInvalidInputConstraint = false;
    for (const diagnostics::Diagnostic& diagnostic :
        invalidInputConstraint.diagnostics.diagnostics())
    {
        sawInvalidInputConstraint = sawInvalidInputConstraint ||
            diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint;
    }
    tests.expect(
        invalidInputConstraint.diagnostics.hasErrors() && sawInvalidInputConstraint,
        "input operands reject output-only register constraints");

    ParsedSyntax invalidOutputConstraint;
    parse(
        invalidOutputConstraint,
        "int Main() { int value; asm { nop } out(value : \"r\"); return 0; }");
    bool sawInvalidOutputConstraint = false;
    for (const diagnostics::Diagnostic& diagnostic :
        invalidOutputConstraint.diagnostics.diagnostics())
    {
        sawInvalidOutputConstraint = sawInvalidOutputConstraint ||
            diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint;
    }
    tests.expect(
        invalidOutputConstraint.diagnostics.hasErrors() && sawInvalidOutputConstraint,
        "output operands require an explicit write or read-write marker");

    ParsedSyntax malformedRegisterConstraint;
    parse(
        malformedRegisterConstraint,
        "int Main() { int value = 1; asm { nop } in(value : \"{r ax}\"); return 0; }");
    tests.expect(
        malformedRegisterConstraint.diagnostics.hasErrors(),
        "fixed-register constraints reject whitespace and malformed register atoms");

    ParsedSyntax nonLiteralConstraint;
    parse(
        nonLiteralConstraint,
        "int Main() { int value = 1; asm { nop } in(value : value); return 0; }");
    bool sawNonLiteralConstraint = false;
    for (const diagnostics::Diagnostic& diagnostic : nonLiteralConstraint.diagnostics.diagnostics())
    {
        sawNonLiteralConstraint = sawNonLiteralConstraint ||
            diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint;
    }
    tests.expect(
        nonLiteralConstraint.diagnostics.hasErrors() && sawNonLiteralConstraint,
        "asm register constraints require compile-time string syntax");

    ParsedSyntax immediateAndMemoryConstraints;
    parse(
        immediateAndMemoryConstraints,
        "int Main() { int output; int input = 40; "
        "asm { add $out0, $0 } out(output : \"=m\") in(2 : \"i\", input : \"m\"); "
        "return output; }");
    const AstNode* storageMain = findFunction(immediateAndMemoryConstraints.root, "Main");
    const AstNode* storageBody = storageMain != nullptr ? storageMain->first() : nullptr;
    const AstNode* storageAssembly = storageBody != nullptr && storageBody->children().size() > 2
        ? storageBody->children()[2]
        : nullptr;
    tests.expect(
        !immediateAndMemoryConstraints.diagnostics.hasErrors() && storageAssembly != nullptr &&
            storageAssembly->children().size() == 4 &&
            storageAssembly->children()[1]->auxiliaryText() == "=m" &&
            storageAssembly->children()[2]->auxiliaryText() == "i" &&
            storageAssembly->children()[3]->auxiliaryText() == "m",
        "immediate and memory constraints preserve their positional AST contracts");

    frontend::FrontendCompilation nonConstantImmediate;
    const bool nonConstantImmediateOk = nonConstantImmediate.compileBuffer(
        "assembly_nonconstant_immediate.ns",
        "int Main() { int value = 2; asm { mov rax, $0 } in(value : \"i\"); return 0; }");
    bool sawNonConstantImmediate = false;
    for (const diagnostics::Diagnostic& diagnostic :
        nonConstantImmediate.diagnostics().diagnostics())
    {
        sawNonConstantImmediate = sawNonConstantImmediate ||
            (diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint &&
             diagnostic.message().find("compile-time integral constant") != std::string::npos);
    }
    tests.expect(
        !nonConstantImmediateOk && sawNonConstantImmediate,
        "asm immediate inputs reject runtime values");

    frontend::FrontendCompilation floatingImmediate;
    const bool floatingImmediateOk = floatingImmediate.compileBuffer(
        "assembly_floating_immediate.ns",
        "int Main() { asm { mov rax, $0 } in(2.0 : \"i\"); return 0; }");
    bool sawFloatingImmediate = false;
    for (const diagnostics::Diagnostic& diagnostic : floatingImmediate.diagnostics().diagnostics())
    {
        sawFloatingImmediate = sawFloatingImmediate ||
            diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint;
    }
    tests.expect(
        !floatingImmediateOk && sawFloatingImmediate,
        "asm immediate inputs reject non-integral constants");

    frontend::FrontendCompilation temporaryMemory;
    const bool temporaryMemoryOk = temporaryMemory.compileBuffer(
        "assembly_temporary_memory.ns",
        "int Main() { asm { mov rax, $0 } in(40 + 2 : \"m\"); return 0; }");
    bool sawTemporaryMemory = false;
    for (const diagnostics::Diagnostic& diagnostic : temporaryMemory.diagnostics().diagnostics())
    {
        sawTemporaryMemory = sawTemporaryMemory ||
            (diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint &&
             diagnostic.message().find("addressable storage") != std::string::npos);
    }
    tests.expect(
        !temporaryMemoryOk && sawTemporaryMemory,
        "asm memory inputs reject rvalue temporaries");

    frontend::FrontendCompilation addressableMemory;
    const bool addressableMemoryOk = addressableMemory.compileBuffer(
        "assembly_addressable_memory.ns",
        "int Main() { int value = 42; asm { mov rax, $0 } in(value : \"m\"); return 0; }");
    bool sawAddressableMemoryError = false;
    for (const diagnostics::Diagnostic& diagnostic : addressableMemory.diagnostics().diagnostics())
    {
        sawAddressableMemoryError = sawAddressableMemoryError ||
            diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyConstraint;
    }
    tests.expect(
        !addressableMemoryOk && !sawAddressableMemoryError,
        "an addressable local satisfies the memory contract before lowering is enabled");

    ParsedSyntax registerClobbers;
    parse(
        registerClobbers,
        "int Main() { int value = 42; "
        "asm { add rax, $0 } in(value) clobber(\"RAX\", \"r11\"); return 0; }");
    const AstNode* clobberMain = findFunction(registerClobbers.root, "Main");
    const AstNode* clobberBody = clobberMain != nullptr ? clobberMain->first() : nullptr;
    const AstNode* clobberAssembly = clobberBody != nullptr && clobberBody->children().size() > 1
        ? clobberBody->children()[1]
        : nullptr;
    tests.expect(
        !registerClobbers.diagnostics.hasErrors() && clobberAssembly != nullptr &&
            clobberAssembly->children().size() == 4 &&
            clobberAssembly->children()[2]->kind() == AstNodeKind::InlineAssemblyClobber &&
            clobberAssembly->children()[2]->text() == "rax" &&
            clobberAssembly->children()[2]->integerValue() == 0 &&
            clobberAssembly->children()[3]->text() == "r11" &&
            clobberAssembly->children()[3]->integerValue() == 1,
        "register clobbers are canonicalized and retain stable source order");

    ParsedSyntax duplicateClobber;
    parse(
        duplicateClobber,
        "int Main() { asm { nop } clobber(\"rax\", \"RAX\"); return 0; }");
    bool sawDuplicateClobber = false;
    for (const diagnostics::Diagnostic& diagnostic : duplicateClobber.diagnostics.diagnostics())
    {
        sawDuplicateClobber = sawDuplicateClobber ||
            (diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyClobber &&
             diagnostic.message().find("duplicate") != std::string::npos);
    }
    tests.expect(
        duplicateClobber.diagnostics.hasErrors() && sawDuplicateClobber,
        "register clobbers reject duplicates after case normalization");

    ParsedSyntax fixedRegisterClobber;
    parse(
        fixedRegisterClobber,
        "int Main() { int value = 1; "
        "asm { nop } in(value : \"{rax}\") clobber(\"rax\"); return 0; }");
    bool sawFixedRegisterConflict = false;
    for (const diagnostics::Diagnostic& diagnostic :
        fixedRegisterClobber.diagnostics.diagnostics())
    {
        sawFixedRegisterConflict = sawFixedRegisterConflict ||
            (diagnostic.id() == diagnostics::DiagnosticId::InvalidInlineAssemblyClobber &&
             diagnostic.message().find("fixed operand") != std::string::npos);
    }
    tests.expect(
        fixedRegisterClobber.diagnostics.hasErrors() && sawFixedRegisterConflict,
        "a fixed operand register cannot also be declared clobbered");

    ParsedSyntax effectClobbers;
    parse(
        effectClobbers,
        "int Main() { asm { nop } clobber(\"cc\", \"memory\"); return 0; }");
    const AstNode* effectMain = findFunction(effectClobbers.root, "Main");
    const AstNode* effectBody = effectMain != nullptr ? effectMain->first() : nullptr;
    const AstNode* effectAssembly = effectBody != nullptr && !effectBody->children().empty()
        ? effectBody->children()[0]
        : nullptr;
    tests.expect(
        !effectClobbers.diagnostics.hasErrors() && effectAssembly != nullptr &&
            effectAssembly->children().size() == 3 &&
            effectAssembly->children()[1]->text() == "cc" &&
            effectAssembly->children()[1]->auxiliaryText() == "flags" &&
            effectAssembly->children()[2]->text() == "memory" &&
            effectAssembly->children()[2]->auxiliaryText() == "memory",
        "condition-code and memory clobbers carry distinct non-register categories");

    tests.expect(
        clobberAssembly != nullptr && clobberAssembly->children().size() == 4 &&
            clobberAssembly->children()[2]->auxiliaryText() == "register" &&
            clobberAssembly->children()[3]->auxiliaryText() == "register",
        "physical register clobbers remain explicitly classified as registers");

    ParsedSyntax emptyClobber;
    parse(emptyClobber, "int Main() { asm { nop } clobber(); return 0; }");
    tests.expect(
        emptyClobber.diagnostics.hasErrors(),
        "empty register clobber clauses are rejected");

    ParsedSyntax emptyOutputs;
    parse(emptyOutputs, "int Main() { asm { nop } out(); return 0; }");
    bool sawRequiredOutput = false;
    for (const diagnostics::Diagnostic& diagnostic : emptyOutputs.diagnostics.diagnostics())
    {
        sawRequiredOutput = sawRequiredOutput ||
            diagnostic.message().find("requires at least one output target") != std::string::npos;
    }
    tests.expect(
        emptyOutputs.diagnostics.hasErrors() && sawRequiredOutput,
        "empty asm out clause is rejected at its closing parenthesis");

    frontend::FrontendCompilation invalidOutput;
    const bool invalidOutputOk = invalidOutput.compileBuffer(
        "assembly_output_lvalue.ns",
        "int Main() { asm { mov $out0, 1 } out(42); return 0; }");
    bool sawInvalidOutput = false;
    for (const diagnostics::Diagnostic& diagnostic : invalidOutput.diagnostics().diagnostics())
    {
        sawInvalidOutput = sawInvalidOutput ||
            (diagnostic.id() == diagnostics::DiagnosticId::InvalidAssignmentTarget &&
             diagnostic.message().find("asm output") != std::string::npos);
    }
    tests.expect(
        !invalidOutputOk && sawInvalidOutput,
        "asm out rejects a value that is not a modifiable lvalue");

    ParsedSyntax missingClauseTerminator;
    parse(missingClauseTerminator, "int Main() { int x = 1; asm { nop } in(x) return 0; }");
    bool sawClauseSemicolon = false;
    for (const diagnostics::Diagnostic& diagnostic : missingClauseTerminator.diagnostics.diagnostics())
    {
        sawClauseSemicolon = sawClauseSemicolon ||
            diagnostic.message().find("after asm operand clauses") != std::string::npos;
    }
    tests.expect(
        missingClauseTerminator.diagnostics.hasErrors() && sawClauseSemicolon,
        "asm input clauses require a final semicolon");

    ParsedSyntax namespaceScope;
    parse(namespaceScope, "asm { nop } int Main() { return 0; }");
    bool sawScopeDiagnostic = false;
    for (const diagnostics::Diagnostic& diagnostic : namespaceScope.diagnostics.diagnostics())
    {
        sawScopeDiagnostic = sawScopeDiagnostic ||
            diagnostic.message().find("only valid inside function or method bodies") != std::string::npos;
    }
    tests.expect(
        namespaceScope.diagnostics.hasErrors() && sawScopeDiagnostic,
        "asm block at namespace scope is rejected with a precise diagnostic");
    tests.expect(
        findFunction(namespaceScope.root, "Main") != nullptr,
        "namespace-scope asm recovery preserves the following declaration");

    ParsedSyntax unterminated;
    parse(unterminated, "int Main() { asm { mov rax, rbx");
    bool sawClosingBraceDiagnostic = false;
    for (const diagnostics::Diagnostic& diagnostic : unterminated.diagnostics.diagnostics())
    {
        sawClosingBraceDiagnostic = sawClosingBraceDiagnostic ||
            diagnostic.message().find("close asm block") != std::string::npos;
    }
    tests.expect(
        unterminated.diagnostics.hasErrors() && sawClosingBraceDiagnostic,
        "unterminated asm block identifies its missing closing brace");

    frontend::FrontendCompilation semanticBoundary;
    const bool executable = semanticBoundary.compileBuffer(
        "assembly_not_lowered.ns",
        "int Main() { asm { nop } return 0; }");
    bool sawSemanticBoundary = false;
    for (const diagnostics::Diagnostic& diagnostic : semanticBoundary.diagnostics().diagnostics())
    {
        sawSemanticBoundary = sawSemanticBoundary ||
            diagnostic.id() == diagnostics::DiagnosticId::InlineAssemblyNotAvailable;
    }
    tests.expect(
        !executable && sawSemanticBoundary && semanticBoundary.hirError().empty(),
        "pre-lowering asm is rejected as a source diagnostic instead of an internal HIR failure");

    frontend::FrontendCompilation boundInput;
    const bool boundInputOk = boundInput.compileBuffer(
        "assembly_input_binding.ns",
        "int Main() { asm { mov rax, $0 } in(missing); return 0; }");
    bool sawUnknownInput = false;
    for (const diagnostics::Diagnostic& diagnostic : boundInput.diagnostics().diagnostics())
    {
        sawUnknownInput = sawUnknownInput ||
            diagnostic.id() == diagnostics::DiagnosticId::UnknownIdentifier;
    }
    tests.expect(
        !boundInputOk && sawUnknownInput,
        "asm input expressions participate in ordinary semantic name binding");

    return tests.result();
}
