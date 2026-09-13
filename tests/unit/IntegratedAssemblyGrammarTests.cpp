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
    asm {
        mov rax, rbx
        add rax, 2
    }
    return 42;
}
)NS");
    tests.expect(!valid.diagnostics.hasErrors(), "asm block grammar parses without diagnostics");
    const AstNode* mainFunction = findFunction(valid.root, "Main");
    const AstNode* body = mainFunction != nullptr ? mainFunction->first() : nullptr;
    const AstNode* assembly = body != nullptr && !body->children().empty()
        ? body->children().front()
        : nullptr;
    tests.expect(
        assembly != nullptr && assembly->kind() == AstNodeKind::InlineAssemblyStatement,
        "asm block has an explicit InlineAssemblyStatement AST node");
    tests.expect(
        assembly != nullptr &&
            assembly->text() == "\n        mov rax, rbx\n        add rax, 2\n    ",
        "assembly body retains its exact source bytes including physical newlines");
    tests.expect(
        assembly != nullptr && assembly->range().isValid(),
        "asm node covers a valid whole-statement source range");
    tests.expect(
        body != nullptr && body->children().size() == 2 &&
            body->children()[1]->kind() == AstNodeKind::ReturnStatement,
        "parsing resumes at the statement following the asm block without a semicolon");
    tests.expect(
        assembly != nullptr && assembly->children().size() == 2 &&
            assembly->children()[0]->kind() == AstNodeKind::InlineAssemblyLine &&
            assembly->children()[0]->text() == "mov rax, rbx" &&
            assembly->children()[1]->text() == "add rax, 2",
        "non-blank physical assembly lines have explicit exact-spelling AST records");
    if (assembly != nullptr && assembly->children().size() == 2)
    {
        const source::SourcePosition firstPosition =
            valid.sources.getPosition(assembly->children()[0]->range().begin());
        const source::SourcePosition secondPosition =
            valid.sources.getPosition(assembly->children()[1]->range().begin());
        tests.expect(
            firstPosition.line == 5 && firstPosition.column == 9 &&
                secondPosition.line == 6 && secondPosition.column == 9,
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

    return tests.result();
}
