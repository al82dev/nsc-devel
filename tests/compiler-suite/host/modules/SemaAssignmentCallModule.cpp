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
}

// Registers CS-SEMA-020 assignment, compound operation, and call contracts.
void registerSemaAssignmentCallModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-195", "CS-SEMA-020", {
        ok("simple", "int F(){long x=0;x=42;return (int)x;}"),
    });
    addCase(registry, "TS-SEMA-196", "CS-SEMA-020", {
        bad("constant", "int F(){const int x=1;x=2;return x;}", DiagnosticId::InvalidAssignmentTarget),
        bad("rvalue", "int F(){(1+2)=3;return 0;}", DiagnosticId::InvalidAssignmentTarget),
    });
    addCase(registry, "TS-SEMA-197", "CS-SEMA-020", {
        bad("rhs", "int F(){int x=0;x=\"bad\";return x;}", DiagnosticId::InvalidConversion),
    });
    addCase(registry, "TS-SEMA-198", "CS-SEMA-020", {
        ok("tokens", "int F(){int x=8;x+=1;x-=1;x*=2;x/=2;x%=3;x&=1;x|=2;x^=3;x<<=1;x>>=1;return x;}"),
    });
    addCase(registry, "TS-SEMA-199", "CS-SEMA-020", {
        ok("arithmetic", "int F(){int x=2;x+=3;x*=4;return x;}"),
        ok("bitwise", "int F(){int x=3;x&=1;x|=2;x^=1;return x;}"),
        ok("shift", "int F(){int x=1;x<<=3;x>>=1;return x;}"),
    });
    addCase(registry, "TS-SEMA-200", "CS-SEMA-020", {
        bad("floating-bitwise", "int F(){float x=1.0f;x&=1;return 0;}", DiagnosticId::InvalidOperand),
        bad("pointer-multiply", "int F(){int* p=null;p*=2;return 0;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-201", "CS-SEMA-020", {
        ok("narrow-result", "int F(){byte x=42;x+=1;return x;}"),
    });
    addCase(registry, "TS-SEMA-202", "CS-SEMA-020", {
        ok("zero", "int A(){return 1;}int F(){return A();}"),
        ok("one", "long A(long x){return x;}int F(){return (int)A(1);}"),
        ok("many", "long A(long x,int y,byte z){return x+y+z;}int F(){return (int)A(1,2,3);}"),
    });
    addCase(registry, "TS-SEMA-203", "CS-SEMA-020", {
        bad("variable", "int F(){int x=1;return x();}", DiagnosticId::InvalidCall),
    });
    addCase(registry, "TS-SEMA-204", "CS-SEMA-020", {
        bad("few", "int A(int x){return x;}int F(){return A();}", DiagnosticId::InvalidArgumentCount),
        badMany("many", "int A(){return 0;}int F(){return A(unknown);}",
            {DiagnosticId::InvalidArgumentCount, DiagnosticId::UnknownIdentifier}),
    });
    addCase(registry, "TS-SEMA-205", "CS-SEMA-020", {
        bad("first", "int A(int x,float y){return x;}int F(){return A(\"x\",1.0f);}",
            DiagnosticId::InvalidConversion),
        bad("second", "int A(int x,float y){return x;}int F(){return A(1,\"y\");}",
            DiagnosticId::InvalidConversion),
    });
    addCase(registry, "TS-SEMA-206", "CS-SEMA-020", {
        ok("void", "void A(){}int F(){A();return 0;}"),
        ok("value", "long A(){return 1;}long F(){return A();}"),
    });
}
}
