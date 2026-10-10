#include "SemaFlowTestSupport.h"

namespace nsc::suite
{
namespace
{
using sema_flow_support::DiagnosticId;
using sema_flow_support::Scenario;
using sema_flow_support::addCase;

Scenario ok(std::string label, std::string source)
{
    return {std::move(label), std::move(source), false, true, {}, {"F"}};
}

Scenario bad(std::string label, std::string source, DiagnosticId id)
{
    return {std::move(label), std::move(source), false, false, {{id, {}}}, {}};
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
    addCase(registry, "TS-SEMA-152", "CS-SEMA-017", {
        bad("global-return", "return 1;", DiagnosticId::InvalidReturn),
    });
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
        bad("unavailable", "int F(){asm { \"nop\" } return 0;}", DiagnosticId::InlineAssemblyNotAvailable),
        bad("constraint", "int F(){int x=0;asm(\"nop\" : \"?\"(x));return 0;}",
            DiagnosticId::InvalidInlineAssemblyConstraint),
    });
    addCase(registry, "TS-SEMA-158", "CS-SEMA-017", {
        bad("modifiable", "int F(){int x=0;asm(\"nop\" : \"=r\"(x));return x;}",
            DiagnosticId::InlineAssemblyNotAvailable),
        bad("constant", "int F(){asm(\"nop\" : \"=r\"(1));return 0;}",
            DiagnosticId::InvalidAssignmentTarget),
    });
    addCase(registry, "TS-SEMA-159", "CS-SEMA-017", {
        ok("expression", "int F(){int x=1;x+2;return x;}"),
    });
    addCase(registry, "TS-SEMA-160", "CS-SEMA-017", {
        ok("switch-current", "int F(){switch(0){}return 0;}"),
    });
    addCase(registry, "TS-SEMA-161", "CS-SEMA-017", {
        bad("loop-restore", "int F(){while(1){}break;return 0;}", DiagnosticId::InvalidCondition),
        bad("function-restore", "int F(){return \"x\";} return 1;", DiagnosticId::InvalidConversion),
    });
}
}
