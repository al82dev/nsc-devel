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
    return {std::move(label), std::move(source), false, true, {}, {"F"}};
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

// A global return cannot be parsed as a compilation-unit declaration. Exercise the semantic
// context check directly so the test remains a Sema contract instead of a Parser oracle.
void testReturnOutsideFunction(TestContext& context)
{
    SemaFixture fixture;
    auto* statement = fixture.node(nativesharp::ast::AstNodeKind::ReturnStatement);
    statement->setFirst(fixture.integer(1));
    fixture.analyzer.analyzeReturn(statement);
    context.expect(fixture.diagnostics.diagnostics().size() == 1U &&
            hasDiagnostic(fixture.diagnostics, DiagnosticId::InvalidReturn),
        "TS-SEMA-152 return without current function must emit exactly InvalidReturn");
}

// Observes the restored analyzer state after failing bodies. The old second source appended a
// global return, which Parser rejected before the restoration invariant could be inspected.
void testContextRestoration(TestContext& context)
{
    const Scenario loop = badMany("loop-restore", "int F(){while(1){}break;return 0;}",
        {DiagnosticId::InvalidCondition, DiagnosticId::BreakOutsideLoop});
    sema_flow_support::FrontendCompilation loopCompilation;
    context.expect(!loopCompilation.compileBuffer("TS-SEMA-161-loop.ns", loop.source, false),
        "TS-SEMA-161 invalid loop fixture must fail semantic analysis");
    sema_flow_support::expectDiagnostics(context, "TS-SEMA-161", loop, loopCompilation);
    context.expect(loopCompilation.semanticAnalyzer() != nullptr &&
            loopCompilation.semanticAnalyzer()->loopDepth_ == 0,
        "TS-SEMA-161 loop depth must be restored after the failing loop");

    const Scenario function = bad("function-restore", "int F(){return \"x\";}",
        DiagnosticId::InvalidConversion);
    sema_flow_support::FrontendCompilation functionCompilation;
    context.expect(!functionCompilation.compileBuffer("TS-SEMA-161-function.ns", function.source, false),
        "TS-SEMA-161 invalid function fixture must fail semantic analysis");
    sema_flow_support::expectDiagnostics(context, "TS-SEMA-161", function, functionCompilation);
    context.expect(functionCompilation.semanticAnalyzer() != nullptr &&
            functionCompilation.semanticAnalyzer()->currentFunction_ == nullptr,
        "TS-SEMA-161 current function must be restored after the failing body");
}
}

// Registers CS-SEMA-017 statement, control-flow, and context-restoration contracts.
void registerSemaStatementModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-146", "CS-SEMA-017", {
        ok("nested", "int F(){int x=1;{int x=2;}return x;}"),
        bad("escaped", "int F(){{int x=2;}return x;}", DiagnosticId::UnknownIdentifier),
    });
    addCase(registry, "TS-SEMA-147", "CS-SEMA-017", {
        ok("bool", "int F(bool b){if(b){return 1;}return 0;}"),
        bad("integer", "int F(){if(1){return 1;}return 0;}", DiagnosticId::InvalidCondition),
        bad("string", "int F(){if(\"x\"){return 1;}return 0;}", DiagnosticId::InvalidCondition),
    });
    addCase(registry, "TS-SEMA-148", "CS-SEMA-017", {
        ok("valid", "int F(){while(true){return 1;}}"),
        bad("invalid", "int F(){while(1){return 1;}return 0;}", DiagnosticId::InvalidCondition),
    });
    addCase(registry, "TS-SEMA-149", "CS-SEMA-017", {
        ok("order", "int F(){int sum=0;for(int i=0;i<3;i=i+1){sum=sum+i;}return sum;}"),
        bad("scope", "int F(){for(int i=0;i<1;i=i+1){}return i;}", DiagnosticId::UnknownIdentifier),
    });
    addCase(registry, "TS-SEMA-150", "CS-SEMA-017", {
        ok("inside", "int F(){while(true){break;}for(;;){continue;}}"),
        bad("break-outside", "int F(){break;return 0;}", DiagnosticId::BreakOutsideLoop),
        bad("continue-outside", "int F(){continue;return 0;}", DiagnosticId::ContinueOutsideLoop),
    });
    addCase(registry, "TS-SEMA-151", "CS-SEMA-017", {
        ok("nested", "int F(){while(true){for(;;){break;}continue;}}"),
    });
    addSemaCase(registry, "TS-SEMA-152", "CS-SEMA-017", "component", testReturnOutsideFunction);
    addCase(registry, "TS-SEMA-153", "CS-SEMA-017", {
        ok("empty", "void F(){return;}"),
        bad("value", "void F(){return 1;}", DiagnosticId::InvalidReturn),
    });
    addCase(registry, "TS-SEMA-154", "CS-SEMA-017", {
        bad("empty", "int F(){return;}", DiagnosticId::InvalidReturn),
        bad("incompatible", "int F(){return \"x\";}", DiagnosticId::InvalidConversion),
        ok("convertible", "long F(){return 1;}"),
    });
    addCase(registry, "TS-SEMA-155", "CS-SEMA-017", {
        ok("true", "int F(){static_assert(true);return 0;}"),
        bad("false", "int F(){static_assert(false);return 0;}", DiagnosticId::StaticAssertionFailed),
    });
    addCase(registry, "TS-SEMA-156", "CS-SEMA-017", {
        bad("not-bool", "int F(){static_assert(1);return 0;}", DiagnosticId::InvalidCondition),
        bad("runtime", "int F(int x){static_assert(x==1);return 0;}", DiagnosticId::ConstRequiresConstant),
    });
    addCase(registry, "TS-SEMA-157", "CS-SEMA-017", {
        bad("unavailable", "int F(){asm { nop } return 0;}", DiagnosticId::InlineAssemblyNotAvailable),
        badMany("constraint", "int F(){int x=0;asm { nop } in(x : \"i\");return 0;}",
            {DiagnosticId::InvalidInlineAssemblyConstraint, DiagnosticId::InlineAssemblyNotAvailable}),
    });
    addCase(registry, "TS-SEMA-158", "CS-SEMA-017", {
        bad("modifiable", "int F(){int x=0;asm { nop } out(x : \"=r\");return x;}",
            DiagnosticId::InlineAssemblyNotAvailable),
        badMany("constant", "int F(){asm { nop } out(1 : \"=r\");return 0;}",
            {DiagnosticId::InvalidAssignmentTarget, DiagnosticId::InlineAssemblyNotAvailable}),
    });
    addCase(registry, "TS-SEMA-159", "CS-SEMA-017", {
        ok("expression", "int F(){int x=1;x+2;return x;}"),
    });
    addCase(registry, "TS-SEMA-160", "CS-SEMA-017", {
        ok("switch-current", "int F(){switch(0){}return 0;}"),
    });
    addSemaCase(registry, "TS-SEMA-161", "CS-SEMA-017", "component", testContextRestoration);
}
}
