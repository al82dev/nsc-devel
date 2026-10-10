#include "SemaTestSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Sema/Symbol.h"

#include <array>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::AnalyzerFixture;
using sema_support::addSemaCase;
using sema_support::diagnosticCount;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::diagnostics::DiagnosticId;

Symbol* findGlobal(SemanticAnalyzer& analyzer, std::string_view name)
{
    for (Symbol* symbol : analyzer.globalSymbols())
        if (symbol->name() == name) return symbol;
    return nullptr;
}

std::vector<AstNode*> flatten(AstNode* root)
{
    std::vector<AstNode*> result;
    std::vector<AstNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        AstNode* node = pending.back();
        pending.pop_back();
        result.push_back(node);
        if (node->first() != nullptr) pending.push_back(node->first());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->third() != nullptr) pending.push_back(node->third());
        for (AstNode* child : node->children()) if (child != nullptr) pending.push_back(child);
    }
    return result;
}

void c087(TestContext& c)
{
    TypeContext types;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    const auto layout = nativesharp::target::TargetDataLayout::WindowsX64();
    SemanticAnalyzer analyzer(types, layout, diagnostics);
    AstNode nonRoot(AstNodeKind::IntegerLiteralExpression, {});
    c.expect(!analyzer.analyze(nullptr) && !analyzer.analyze(&nonRoot) && diagnostics.diagnostics().empty(),
        "TS-SEMA-087 null and non-CompilationUnit roots are rejected without an invented diagnostic");
}

void c088(TestContext& c)
{
    AnalyzerFixture fixture("");
    c.expect(fixture.analyze() && fixture.analyzer().globalSymbols().empty() &&
            fixture.diagnostics().diagnostics().empty(),
        "TS-SEMA-088 an empty CompilationUnit yields an empty global scope and success");
}

void c089(TestContext& c)
{
    AnalyzerFixture fixture("struct S { int x; } enum E { A } extern C int F(); int G(); int value;");
    const bool analyzed = fixture.analyze();
    c.expect(analyzed && findGlobal(fixture.analyzer(), "S") != nullptr &&
            findGlobal(fixture.analyzer(), "E") != nullptr && findGlobal(fixture.analyzer(), "F") != nullptr &&
            findGlobal(fixture.analyzer(), "G") != nullptr && findGlobal(fixture.analyzer(), "value") != nullptr,
        "TS-SEMA-089 five implemented global declaration kinds dispatch to their owning routes");
}

void c090(TestContext& c)
{
    AnalyzerFixture fixture("class C { }");
    c.expect(!fixture.analyze() && diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidType) == 1,
        "TS-SEMA-090 ClassDeclaration is rejected with the current InvalidType diagnostic");
}

void c091(TestContext& c)
{
    TypeContext types;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    const auto layout = nativesharp::target::TargetDataLayout::WindowsX64();
    SemanticAnalyzer analyzer(types, layout, diagnostics);
    AstNode root(AstNodeKind::CompilationUnit, {});
    AstNode invalid(AstNodeKind::ExpressionStatement, {});
    root.addChild(&invalid);
    c.expect(!analyzer.analyze(&root) && diagnosticCount(diagnostics, DiagnosticId::ExpectedDeclaration) == 1,
        "TS-SEMA-091 a non-declaration global node reports ExpectedDeclaration");
}

void c092(TestContext& c)
{
    AnalyzerFixture fixture("int first; struct Second { int x; } enum Third { A } int Fourth();");
    const bool analyzed = fixture.analyze();
    const auto globals = fixture.analyzer().globalSymbols();
    c.expect(analyzed && globals.size() == 4 && globals[0]->name() == "first" &&
            globals[1]->name() == "Second" && globals[2]->name() == "Third" &&
            globals[3]->name() == "Fourth",
        "TS-SEMA-092 globalSymbols preserves exact source order");
}

void c093(TestContext& c)
{
    AnalyzerFixture absent("");
    absent.analyze();
    const bool absentRejected = !absent.analyzer().validateExecutableEntryPoint(absent.root()) &&
        diagnosticCount(absent.diagnostics(), DiagnosticId::MissingMain) == 1;
    AnalyzerFixture variable("int Main;");
    variable.analyze();
    const bool variableRejected = !variable.analyzer().validateExecutableEntryPoint(variable.root()) &&
        diagnosticCount(variable.diagnostics(), DiagnosticId::MissingMain) == 1;
    AnalyzerFixture declaration("int Main();");
    declaration.analyze();
    const bool declarationRejected = !declaration.analyzer().validateExecutableEntryPoint(declaration.root()) &&
        diagnosticCount(declaration.diagnostics(), DiagnosticId::MissingMain) == 1;
    c.expect(absentRejected && variableRejected && declarationRejected,
        "TS-SEMA-093 absent, non-function, and undefined Main each report MissingMain");
}

void c094(TestContext& c)
{
    {
        AnalyzerFixture fixture("struct S { int x; } int F(int x) { int y = x; return y; }");
        c.expect(fixture.analyze(),
            "TS-SEMA-094 analyzer must own a valid symbol/scope graph before destruction");
    }
    c.expect(false,
        "TS-SEMA-094 TEST-INFRASTRUCTURE: leak and double-delete certification requires the sanitizer profile reserved for the later coverage phase");
}

void c095(TestContext& c)
{
    AnalyzerFixture fixture("int global;");
    const bool analyzed = fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "global");
    c.expect(analyzed && symbol != nullptr && symbol->kind() == SymbolKind::GlobalVariable &&
            symbol->declaration() != nullptr && symbol->type()->kind() == TypeKind::Int,
        "TS-SEMA-095 global symbol creation preserves ownership, order, lookup, and identity");
}

void c096(TestContext& c)
{
    AnalyzerFixture fixture("int duplicate; int duplicate;");
    c.expect(!fixture.analyze() && diagnosticCount(fixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::PreviousDeclaration) == 1,
        "TS-SEMA-096 same-scope duplicates report the duplicate and previous declaration");
}

void c097(TestContext& c)
{
    AnalyzerFixture fixture("int Main() { int x = 1; { int x = 2; } return x; }");
    c.expect(fixture.analyze() && fixture.diagnostics().diagnostics().empty(),
        "TS-SEMA-097 a nested variable may shadow a parent variable");
}

void c098(TestContext& c)
{
    AnalyzerFixture structFixture("struct Name { int x; } int Main() { int Name = 1; return Name; }");
    AnalyzerFixture enumFixture("enum Kind { A } int Main() { int Kind = 1; return Kind; }");
    c.expect(!structFixture.analyze() && !enumFixture.analyze() &&
            diagnosticCount(structFixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1 &&
            diagnosticCount(enumFixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1,
        "TS-SEMA-098 values cannot shadow a visible struct or enum");
}

void c099(TestContext& c)
{
    AnalyzerFixture fixture(
        "int Main() { { int blockValue = 1; } for (int i = 0; i < 1; i = i + 1) { int inner = i; } int i = 2; return i; }");
    c.expect(fixture.analyze() && fixture.diagnostics().diagnostics().empty(),
        "TS-SEMA-099 block and for scopes restore the containing current scope on exit");
}

void c100(TestContext& c)
{
    AnalyzerFixture fixture("int global; int Main() { int local = global; return local; }");
    const bool analyzed = fixture.analyze();
    bool sawGlobal = false;
    bool sawLocal = false;
    for (AstNode* node : flatten(fixture.root()))
    {
        if (node->kind() != AstNodeKind::IdentifierExpression) continue;
        Symbol* symbol = fixture.analyzer().model().symbol(node);
        if (node->text() == "global") sawGlobal = symbol != nullptr && symbol->name() == "global";
        if (node->text() == "local") sawLocal = symbol != nullptr && symbol->name() == "local";
    }
    AstNode unbound(AstNodeKind::IdentifierExpression, {});
    unbound.setText("missing");
    c.expect(analyzed && sawGlobal && sawLocal && fixture.analyzer().model().symbol(&unbound) == nullptr,
        "TS-SEMA-100 analyzed bindings preserve local-before-parent priority and stable misses");
}

void c101(TestContext& c)
{
    AnalyzerFixture withPrevious("int value; int value;");
    withPrevious.analyze();
    TypeContext types;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    const auto layout = nativesharp::target::TargetDataLayout::WindowsX64();
    SemanticAnalyzer analyzer(types, layout, diagnostics);
    AstNode root(AstNodeKind::CompilationUnit, {});
    AstNode first(AstNodeKind::GlobalVariableDeclaration, {});
    AstNode second(AstNodeKind::GlobalVariableDeclaration, {});
    first.setText("value"); second.setText("value");
    root.addChild(&first); root.addChild(&second);
    analyzer.analyze(&root);
    c.expect(diagnosticCount(withPrevious.diagnostics(), DiagnosticId::PreviousDeclaration) == 1 &&
            diagnosticCount(diagnostics, DiagnosticId::PreviousDeclaration) == 1,
        "TS-SEMA-101 previous-declaration notes are emitted only when a declaration exists");
    c.expect(false,
        "TS-SEMA-101 TEST-INFRASTRUCTURE: the null-declaration branch of private reportPreviousDeclaration needs a product test seam");
}
}

void registerSemaAnalyzerLifecycleModule(TestRegistry& registry)
{
    const std::array<TestFunction, 15> tests = {c087, c088, c089, c090, c091, c092, c093, c094,
        c095, c096, c097, c098, c099, c100, c101};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 87;
        const std::string id = "TS-SEMA-" + std::to_string(ordinal);
        const std::string contract = ordinal <= 94 ? "CS-SEMA-011" : "CS-SEMA-012";
        const TestDisposition disposition = ordinal == 94 || ordinal == 101
            ? TestDisposition::InfrastructurePending : TestDisposition::Required;
        std::vector<std::string> tags;
        if (ordinal == 94 || ordinal == 101) tags = {"infrastructure-special"};
        addSemaCase(registry, id, contract, "component", tests[index],
            ordinal == 94 ? "NIGHTLY" : "PR", disposition, std::move(tags));
    }
}
}
