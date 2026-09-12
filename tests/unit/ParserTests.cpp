#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Lexer/TokenKind.h"

#include "../support/TestContext.h"

#include <string>

namespace
{
const nativesharp::ast::AstNode* findGlobal(
    const nativesharp::ast::AstNode* root,
    nativesharp::ast::AstNodeKind kind,
    std::string_view name)
{
    if (root == nullptr)
    {
        return nullptr;
    }

    for (const nativesharp::ast::AstNode* child : root->children())
    {
        if (child->kind() == kind && child->text() == name)
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
    frontend::FrontendCompilation compilation;
    const std::string source = R"NS(
struct Node
{
    int Value;
    Node* Next;
}

enum Color
{
    Red,
    Green = Red + 1,
    Blue,
}

extern C
{
    int puts(char* text);
}

int Add(int a, int b);

int Add(int a, int b)
{
    return a + b * 2;
}

int Main()
{
    int x = 1;
    int* p = &x;
    int[4] values;
    values[0] = (int)sizeof(Node);
    Node* nodes = new Node[2];
    Node* local = stackalloc Node[1];
    if (x < 4)
    {
        x++;
    }
    else if (x == 4)
    {
        --x;
    }
    while (x < 10)
    {
        x += 1;
        if (x == 8)
        {
            continue;
        }
    }
    for (int i = 0; i < 2; i++)
    {
        nodes[i].Value = i;
    }
    delete[] nodes;
    local->Value = *p;
    puts("ok");
    return Add(x, values[0]);
}
)NS";

    const bool ok = compilation.compileBuffer("parser.ns", source);
    tests.expect(ok, "full grammar sample parses and type-checks");
    tests.expect(!compilation.diagnostics().hasErrors(), "full grammar sample has no diagnostics");

    const AstNode* root = compilation.syntaxTree();
    tests.expect(root != nullptr && root->kind() == AstNodeKind::CompilationUnit, "root is CompilationUnit");
    tests.expect(findGlobal(root, AstNodeKind::StructDeclaration, "Node") != nullptr, "struct declaration is in AST");
    tests.expect(findGlobal(root, AstNodeKind::EnumDeclaration, "Color") != nullptr, "enum declaration is in AST");
    tests.expect(
        findGlobal(root, AstNodeKind::FunctionDeclaration, "Add") != nullptr,
        "function declaration is in AST");
    tests.expect(findGlobal(root, AstNodeKind::FunctionDeclaration, "Main") != nullptr, "Main is in AST");

    const AstNode* mainFunction = findGlobal(root, AstNodeKind::FunctionDeclaration, "Main");
    tests.expect(mainFunction != nullptr && mainFunction->first() != nullptr, "Main has a parsed block body");
    tests.expect(mainFunction != nullptr && mainFunction->first()->children().size() >= 10,
        "Main body contains parsed statements");

    frontend::FrontendCompilation invalidBraces;
    const bool bracesOk = invalidBraces.compileBuffer("bad_braces.ns", "int Main() { if (true) return 0; }");
    tests.expect(!bracesOk, "if body without braces is rejected by parser");

    frontend::FrontendCompilation declarationAmbiguity;
    const bool ambiguityOk = declarationAmbiguity.compileBuffer(
        "known_types.ns",
        "struct P { int X; } int Main() { P* p = null; int x = 2; x * x; return 0; }");
    tests.expect(ambiguityOk, "KnownTypeNames distinguishes declarations from multiplication expressions");

    frontend::FrontendCompilation castAmbiguity;
    const bool castOk = castAmbiguity.compileBuffer(
        "cast.ns",
        "struct P { int X; } int Main() { int x = 2; int y = (x) * 3; P* p = null; p = (P*)p; return y; }");
    tests.expect(castOk, "KnownTypeNames distinguishes grouping from cast syntax");

    frontend::FrontendCompilation sizeofAmbiguity;
    const bool sizeofOk = sizeofAmbiguity.compileBuffer(
        "sizeof_enum_member.ns",
        "enum E { A } int Main() { const nuint s = sizeof(E.A); return (int)s; }");
    tests.expect(sizeofOk, "sizeof(E.A) is parsed as sizeof(expression), not sizeof(type)");

    frontend::FrontendCompilation typeNamedC;
    const bool typeNamedCOk = typeNamedC.compileBuffer(
        "type_c.ns",
        "struct C { public int Value; } "
        "int Main() { C value; value.Value = 42; return value.Value; }");
    frontend::FrontendCompilation contextualC;
    const bool contextualCOk = contextualC.compileBuffer(
        "contextual_c.ns",
        "struct Box { public int C; } enum Keys { C = 67 } "
        "int Main() { int C = 1; Box value; value.C = C; return (int)Keys.C - 25; }");
    tests.expect(typeNamedCOk && !typeNamedC.diagnostics().hasErrors() &&
            contextualCOk && !contextualC.diagnostics().hasErrors(),
        "C is an ordinary type, local, field, and enum-member identifier outside extern C");

    frontend::FrontendCompilation invalidExternLanguage;
    const bool invalidExternLanguageOk = invalidExternLanguage.compileBuffer(
        "extern_language.ns", "extern X { int puts(char* text); }");
    bool hasExpectedC = false;
    for (const diagnostics::Diagnostic& diagnostic :
         invalidExternLanguage.diagnostics().diagnostics())
    {
        hasExpectedC = hasExpectedC ||
            diagnostic.message().find("expected 'C'") != std::string::npos;
    }
    tests.expect(!invalidExternLanguageOk && hasExpectedC,
        "extern followed by another identifier keeps the specific expected-C diagnostic");

    frontend::FrontendCompilation ternary;
    const bool ternaryOk = ternary.compileBuffer(
        "no_ternary.ns",
        "int Main() { return true ? 1 : 0; }");
    tests.expect(!ternaryOk, "invalid ternary conditional syntax is rejected by NativeSharp 1.0.1");

    frontend::FrontendCompilation singleStatementLoop;
    const bool loopOk = singleStatementLoop.compileBuffer(
        "braced_loop.ns",
        "int Main() { while (false) return 1; return 0; }");
    tests.expect(!loopOk, "while body without braces is rejected by parser");

    frontend::FrontendCompilation expressionForms;
    const bool expressionFormsOk = expressionForms.compileBuffer(
        "expression_forms.ns",
        "struct P { int X; } int Main() { int x = 1; int* p = new int; *p = x; "
        "nuint a = sizeof(x++); nuint b = sizeof(int); int y = (int)1.0f; ++x; x--; "
        "P value; P* pp = &value; pp->X = y; delete p; return (int)(a + b); }");
    tests.expect(expressionFormsOk,
        "new/delete, sizeof(type/expression), casts, prefix/postfix, dereference, and pointer-member forms parse");

    frontend::FrontendCompilation recovery;
    const bool recoveryOk = recovery.compileBuffer(
        "recovery.ns",
        "int F() { int x = ; return 0; } int Main() { int y = ; return 0; }");
    tests.expect(!recoveryOk, "parser rejects a source containing multiple malformed statements");
    tests.expect(recovery.diagnostics().errorCount() >= 2,
        "parser recovery continues far enough to report more than one independent syntax error");

    return tests.result();
}
