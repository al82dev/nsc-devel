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

Scenario ok(std::string label, std::string source)
{
    return {std::move(label), std::move(source), false, true, {}, {}};
}

Scenario bad(std::string label, std::string source, DiagnosticId id)
{
    return {std::move(label), std::move(source), false, false, {{id, {}}}, {}};
}

Scenario badMany(std::string label, std::string source, std::initializer_list<DiagnosticId> ids)
{
    Scenario scenario{std::move(label), std::move(source), false, false, {}, {}};
    for (DiagnosticId id : ids) scenario.diagnostics.push_back({id, {}});
    return scenario;
}

// The grammar has no forward `struct S;` declaration. Install an incomplete nominal directly
// and keep the observable under test at SemanticAnalyzer::analyzeIndex.
void testInvalidPointerIndexBases(TestContext& context)
{
    sema_flow_support::runScenario(context, "TS-SEMA-214",
        bad("void", "int F(){void* p=null;p[0];return 0;}", DiagnosticId::InvalidIndex));

    SemaFixture fixture;
    fixture.initializeGlobalScope();
    auto* incomplete = fixture.types.createStruct("S", nullptr);
    auto* pointer = fixture.types.pointerTo(incomplete);
    nativesharp::sema::Symbol variable(
        nativesharp::sema::SymbolKind::Variable, "p", pointer, nullptr);
    context.expect(fixture.analyzer.currentScope_->declare(&variable),
        "TS-SEMA-214 incomplete fixture must declare pointer p");

    auto* base = fixture.node(nativesharp::ast::AstNodeKind::IdentifierExpression);
    base->setText("p");
    auto* index = fixture.node(nativesharp::ast::AstNodeKind::IndexExpression);
    index->setFirst(base);
    index->setSecond(fixture.integer(0));
    const auto result = fixture.analyzer.analyzeIndex(index);
    context.expect(result.type != nullptr &&
            result.type->kind() == nativesharp::sema::TypeKind::Error,
        "TS-SEMA-214 incomplete pointer indexing must return the error type");
    context.expect(fixture.diagnostics.diagnostics().size() == 1U &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidIndex),
        "TS-SEMA-214 incomplete pointer indexing must emit exactly InvalidIndex");
}
}

// Registers CS-SEMA-021 indexing, bounds, and member-binding contracts.
void registerSemaIndexMemberModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-207", "CS-SEMA-021", {
        ok("constant", "int F(){int[4] a;a[3]=7;return a[3];}"),
        ok("dynamic", "int F(int i){int[4] a;a[i]=7;return a[i];}"),
    });
    addCase(registry, "TS-SEMA-208", "CS-SEMA-021", {
        ok("rank-two", "int F(){int[2,3] a;a[1,2]=7;return a[1,2];}"),
        ok("nested", "int F(){int[2][3] a;a[1][2]=7;return a[1][2];}"),
    });
    addCase(registry, "TS-SEMA-209", "CS-SEMA-021", {
        bad("first", "int F(){int[2,3] a;return a[1.0,0];}", DiagnosticId::InvalidIndex),
        bad("second", "int F(){int[2,3] a;return a[0,1.0];}", DiagnosticId::InvalidIndex),
    });
    addCase(registry, "TS-SEMA-210", "CS-SEMA-021", {
        bad("negative", "int F(){int[4] a;return a[-1];}", DiagnosticId::ConstantIndexOutOfBounds),
        bad("count", "int F(){int[4] a;return a[4];}", DiagnosticId::ConstantIndexOutOfBounds),
        bad("beyond", "int F(){int[4] a;return a[5];}", DiagnosticId::ConstantIndexOutOfBounds),
    });
    addCase(registry, "TS-SEMA-211", "CS-SEMA-021", {
        ok("valid", "int F(){char c=\"abc\"[1];return (int)c;}"),
        ok("terminator", "int F(){char c=\"abc\"[3];return (int)c;}"),
        bad("beyond", "int F(){return (int)\"abc\"[4];}", DiagnosticId::ConstantIndexOutOfBounds),
    });
    addCase(registry, "TS-SEMA-212", "CS-SEMA-021", {
        ok("pointer", "int F(int* p,int i){p[i]=3;return p[i];}"),
    });
    addCase(registry, "TS-SEMA-213", "CS-SEMA-021", {
        bad("scalar", "int F(){int x=1;return x[0];}", DiagnosticId::InvalidIndex),
    });
    addSemaCase(registry, "TS-SEMA-214", "CS-SEMA-021", "component", testInvalidPointerIndexBases);
    addCase(registry, "TS-SEMA-215", "CS-SEMA-021", {
        ok("hit", "struct S{int X;}int F(){S s;s.X=1;return s.X;}"),
        bad("miss", "struct S{int X;}int F(){S s;return s.Y;}", DiagnosticId::UnknownMember),
    });
    addCase(registry, "TS-SEMA-216", "CS-SEMA-021", {
        ok("hit", "enum E{A=3}int F(){return (int)E.A;}"),
        bad("miss", "enum E{A}int F(){return (int)E.B;}", DiagnosticId::UnknownMember),
    });
    addCase(registry, "TS-SEMA-217", "CS-SEMA-021", {
        bad("scalar", "int F(){int x=1;return x.Value;}", DiagnosticId::InvalidMemberAccess),
    });
    addCase(registry, "TS-SEMA-218", "CS-SEMA-021", {
        ok("pointer-hit", "struct S{int X;}int F(){S s;S* p=&s;p->X=1;return p->X;}"),
    });
    addCase(registry, "TS-SEMA-219", "CS-SEMA-021", {
        bad("struct-arrow", "struct S{int X;}int F(){S s;return s->X;}", DiagnosticId::InvalidMemberAccess),
        bad("scalar-arrow", "int F(){int x=1;return x->Value;}", DiagnosticId::InvalidMemberAccess),
    });
    addCase(registry, "TS-SEMA-220", "CS-SEMA-021", {
        ok("mutable", "struct S{int X;}int F(){S s;s.X=1;return s.X;}"),
        badMany("const", "struct S{int X;}int F(){const S s;s.X=1;return s.X;}",
            {DiagnosticId::ConstRequiresConstant, DiagnosticId::InvalidAssignmentTarget}),
    });
    addCase(registry, "TS-SEMA-221", "CS-SEMA-021", {
        ok("binding", "struct S{int X;}int F(){S s;return s.X;}"),
    });
    addCase(registry, "TS-SEMA-222", "CS-SEMA-021", {
        bad("base-error", "int F(){return missing[0];}", DiagnosticId::UnknownIdentifier),
        bad("index-error", "int F(){int[2] a;return a[missing];}", DiagnosticId::UnknownIdentifier),
    });
}
}
