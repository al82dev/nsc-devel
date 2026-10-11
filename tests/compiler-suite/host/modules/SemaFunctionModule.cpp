#include "SemaConversionEvaluationTestSupport.h"
#include "SemaFlowTestSupport.h"

namespace nsc::suite
{
namespace
{
using sema_flow_support::DiagnosticId;
using sema_flow_support::Scenario;
using sema_flow_support::addCase;
using sema_conversion_evaluation_support::SemaFixture;
using sema_conversion_evaluation_support::addSemaCase;
using sema_conversion_evaluation_support::hasDiagnostic;

Scenario ok(std::string label, std::string source, std::vector<std::string> globals = {}, bool main = false)
{
    return {std::move(label), std::move(source), main, true, {}, std::move(globals)};
}

Scenario bad(std::string label, std::string source, DiagnosticId id, std::string text = {}, bool main = false)
{
    return {std::move(label), std::move(source), main, false, {{id, std::move(text)}}, {}};
}

Scenario badWithPrevious(std::string label, std::string source, DiagnosticId id)
{
    return {std::move(label), std::move(source), false, false,
        {{id, {}}, {DiagnosticId::PreviousDeclaration, {}}}, {}};
}

// The parser intentionally rejects extern-C bodies, so construct the AST directly to keep
// these contracts at SemanticAnalyzer::analyzeFunction instead of testing parser recovery.
void analyzeExternCBody(SemaFixture& fixture)
{
    fixture.initializeGlobalScope();
    auto* function = fixture.node(nativesharp::ast::AstNodeKind::FunctionDeclaration);
    function->setText("F");
    function->setTypeSyntax(fixture.typeSyntax(nativesharp::lexer::TokenKind::KwInt, "int"));
    function->setFirst(fixture.node(nativesharp::ast::AstNodeKind::BlockStatement));
    fixture.analyzer.analyzeFunction(function, true);
}

void testExternCBodyDiagnostic(TestContext& context)
{
    SemaFixture fixture;
    analyzeExternCBody(fixture);
    const auto& diagnostics = fixture.diagnostics.diagnostics();
    context.expect(diagnostics.size() == 1U &&
            diagnostics.front().id() == DiagnosticId::InvalidType,
        "TS-SEMA-128 extern-C body must emit exactly InvalidType at the semantic boundary");
    if (!diagnostics.empty())
    {
        context.expect(diagnostics.front().message() ==
                "extern C functions cannot have a body in NativeSharp 2.0",
            "TS-SEMA-128 extern-C body diagnostic text must remain exact");
    }
}

void testFunctionDiagnosticText(TestContext& context)
{
    SemaFixture fixture;
    analyzeExternCBody(fixture);
    const auto& diagnostics = fixture.diagnostics.diagnostics();
    context.expect(diagnostics.size() == 1U &&
            diagnostics.front().message().find("NativeSharp 2.0") != std::string::npos,
        "TS-SEMA-135 extern-C body diagnostic must identify the NativeSharp 2.0 contract");
    sema_flow_support::runScenario(context, "TS-SEMA-135",
        bad("main-text", "int Main(int x){return x;}", DiagnosticId::InvalidMainSignature,
            "NativeSharp 2.0 Main"));
}

// Keeps the parser-facing void/array cases while constructing the incomplete nominal directly.
// A forward `struct S;` declaration is not part of the current grammar, so using that spelling
// would stop in Parser and would never exercise SemanticAnalyzer::validateObjectType.
void testInvalidParameterTypes(TestContext& context)
{
    for (const Scenario& scenario : {
             bad("void-param", "void F(void x);", DiagnosticId::InvalidType),
             bad("array-param", "void F(int[2] x);", DiagnosticId::InvalidType),
         })
    {
        sema_flow_support::runScenario(context, "TS-SEMA-123", scenario);
    }

    SemaFixture fixture;
    fixture.initializeGlobalScope();
    auto* incomplete = fixture.types.createStruct("S", nullptr);
    nativesharp::sema::Symbol symbol(
        nativesharp::sema::SymbolKind::Struct, "S", incomplete, nullptr);
    context.expect(fixture.analyzer.globalScope_->declare(&symbol),
        "TS-SEMA-123 incomplete fixture must install nominal S");
    auto* syntax = fixture.typeSyntax(nativesharp::lexer::TokenKind::Identifier, "S");
    auto* resolved = fixture.analyzer.resolveType(syntax);
    context.expect(resolved == incomplete,
        "TS-SEMA-123 incomplete parameter type must resolve to the installed nominal");
    context.expect(!fixture.analyzer.validateObjectType(resolved, {}, false),
        "TS-SEMA-123 incomplete parameter object type must be rejected");
    context.expect(fixture.diagnostics.diagnostics().size() == 1U &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::IncompleteType),
        "TS-SEMA-123 incomplete parameter must emit exactly IncompleteType");
}
}

// Registers CS-SEMA-015 function, linkage, parameter, and entry-point contracts.
void registerSemaFunctionModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-121", "CS-SEMA-015", {
        ok("void-zero", "void A() { }", {"A"}),
        ok("value-one", "int B(int x) { return x; }", {"B"}),
        ok("value-many", "long C(int a, byte b, long c) { return c; }", {"C"}),
    });
    addCase(registry, "TS-SEMA-122", "CS-SEMA-015", {
        bad("array-return", "int[2] F();", DiagnosticId::InvalidType),
    });
    addSemaCase(registry, "TS-SEMA-123", "CS-SEMA-015", "component", testInvalidParameterTypes);
    addCase(registry, "TS-SEMA-124", "CS-SEMA-015", {
        ok("repeat", "int F(int x); int F(int x);", {"F"}),
    });
    addCase(registry, "TS-SEMA-125", "CS-SEMA-015", {
        badWithPrevious("type", "int F(int x); int F(float x);", DiagnosticId::ConflictingFunctionDeclaration),
        badWithPrevious("linkage", "extern C int G(int x); int G(int x);", DiagnosticId::ConflictingFunctionDeclaration),
    });
    addCase(registry, "TS-SEMA-126", "CS-SEMA-015", {
        ok("declare-define", "int F(); int F() { return 1; }", {"F"}),
    });
    addCase(registry, "TS-SEMA-127", "CS-SEMA-015", {
        badWithPrevious("double-definition", "int F(){return 1;} int F(){return 2;}",
            DiagnosticId::DuplicateFunctionDefinition),
    });
    addSemaCase(registry, "TS-SEMA-128", "CS-SEMA-015", "component", testExternCBodyDiagnostic);
    addCase(registry, "TS-SEMA-129", "CS-SEMA-015", {
        ok("scalars", "enum E { A } extern C int F(byte x, int* p, E e);", {"E", "F"}),
    });
    addCase(registry, "TS-SEMA-130", "CS-SEMA-015", {
        ok("valid-struct", "struct P { int X; int[2] Y; } extern C P F(P p);", {"P", "F"}),
        bad("empty", "struct E { } extern C E F();", DiagnosticId::InvalidExternCSignature),
        bad("nested-empty", "struct E{} struct P{E Value;} extern C void F(P p);",
            DiagnosticId::InvalidExternCSignature),
    });
    addCase(registry, "TS-SEMA-131", "CS-SEMA-015", {
        ok("valid", "int Main(){return 0;}", {"Main"}),
        bad("parameter", "int Main(int argc){return argc;}", DiagnosticId::InvalidMainSignature),
        bad("return", "void Main(){}", DiagnosticId::InvalidMainSignature),
    });
    addCase(registry, "TS-SEMA-132", "CS-SEMA-015", {
        bad("absent", "int F(){return 0;}", DiagnosticId::MissingMain, {}, true),
        bad("declared", "int Main();", DiagnosticId::MissingMain, {}, true),
        ok("defined", "int Main(){return 0;}", {"Main"}, true),
    });
    addCase(registry, "TS-SEMA-133", "CS-SEMA-015", {
        bad("fallthrough", "int F(bool b){if(b){return 1;}}", DiagnosticId::NotAllPathsReturn),
        ok("complete", "int F(bool b){if(b){return 1;}return 0;}", {"F"}),
    });
    addCase(registry, "TS-SEMA-134", "CS-SEMA-015", {
        ok("scope", "int F(int x){ { int y=x; } return x; } int G(){int x=2;return x;}", {"F", "G"}),
    });
    addSemaCase(registry, "TS-SEMA-135", "CS-SEMA-015", "component", testFunctionDiagnosticText);
}
}
