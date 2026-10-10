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
}

// Registers CS-SEMA-019 unary, binary, conversion, and operator-record contracts.
void registerSemaOperatorModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-173", "CS-SEMA-019", {
        ok("mutable", "int F(){int x=1;int* p=&x;return *p;}"),
        ok("const", "int F(){const int x=1;const int* p=&x;return *p;}"),
        bad("rvalue", "int F(){int* p=&(1+2);return 0;}", DiagnosticId::InvalidAddressOf),
    });
    addCase(registry, "TS-SEMA-174", "CS-SEMA-019", {
        ok("valid", "int F(){int x=1;int* p=&x;return *p;}"),
        bad("void", "int F(){void* p=null;*p;return 0;}", DiagnosticId::InvalidDereference),
        bad("scalar", "int F(){int x=1;return *x;}", DiagnosticId::InvalidDereference),
    });
    addCase(registry, "TS-SEMA-175", "CS-SEMA-019", {
        ok("integer", "int F(){int x=1;++x;x++;--x;x--;return x;}"),
        ok("pointer", "int F(){int[2] a;int* p=&a[0];++p;p--;return 0;}"),
    });
    addCase(registry, "TS-SEMA-176", "CS-SEMA-019", {
        bad("constant", "int F(){const int x=1;++x;return x;}", DiagnosticId::InvalidAssignmentTarget),
        bad("rvalue", "int F(){++(1+2);return 0;}", DiagnosticId::InvalidAssignmentTarget),
    });
    addCase(registry, "TS-SEMA-177", "CS-SEMA-019", {
        ok("bool", "int F(bool b){return !b ? 1 : 0;}"),
        bad("integer", "int F(){return !1;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-178", "CS-SEMA-019", {
        ok("promotion", "int F(){byte b=1;return ~b;}"),
        ok("constant", "int F(){return ~0;}"),
    });
    addCase(registry, "TS-SEMA-179", "CS-SEMA-019", {
        ok("numeric", "double F(){int x=-1;double y=+2.0;return x+y;}"),
        bad("pointer", "int F(){int* p=null;+p;return 0;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-180", "CS-SEMA-019", {
        ok("truth-table", "int F(){bool a=false&&true;bool b=true||false;return a?0:(b?1:0);}"),
        ok("short-circuit", "int F(){bool a=false&&(1/0==0);return a?1:0;}"),
    });
    addCase(registry, "TS-SEMA-181", "CS-SEMA-019", {
        bad("left", "int F(){bool b=1&&true;return 0;}", DiagnosticId::InvalidOperand),
        bad("right", "int F(){bool b=true||1;return 0;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-182", "CS-SEMA-019", {
        ok("mixed", "int F(){byte a=1;int b=2;bool c=a==b;double d=2.0;return c&&(b!=d)?1:0;}"),
    });
    addCase(registry, "TS-SEMA-183", "CS-SEMA-019", {
        ok("null", "int F(){int* p=null;return p==null?1:0;}"),
        ok("void", "int F(){int* p=null;void* q=p;return p==q?1:0;}"),
        bad("incompatible", "int F(){int* p=null;float* q=null;return p==q;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-184", "CS-SEMA-019", {
        ok("numeric", "int F(){return 1<2?1:0;}"),
        bad("pointer", "int F(){int* a=null;int* b=null;return a<b;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-185", "CS-SEMA-019", {
        ok("bitwise", "int F(){byte a=3;short b=5;return (a&b)|(a^b);}"),
    });
    addCase(registry, "TS-SEMA-186", "CS-SEMA-019", {
        ok("valid", "int F(){uint x=1u;return (int)(x<<3);}"),
        bad("width", "int F(){return 1<<32;}", DiagnosticId::ConstantEvaluationTrap),
        bad("negative", "int F(){return 1<<-1;}", DiagnosticId::ConstantEvaluationTrap),
    });
    addCase(registry, "TS-SEMA-187", "CS-SEMA-019", {
        ok("pointer-left", "int F(){int[2] a;int* p=&a[0];p=p+1;return 0;}"),
        ok("integer-left", "int F(){int[2] a;int* p=&a[0];p=1+p;return 0;}"),
    });
    addCase(registry, "TS-SEMA-188", "CS-SEMA-019", {
        ok("subtract", "int F(){int[2] a;int* p=&a[1];p=p-1;return 0;}"),
    });
    addCase(registry, "TS-SEMA-189", "CS-SEMA-019", {
        ok("compatible", "int F(){int[2] a;int* p=&a[0];int* q=&a[1];nint n=q-p;return (int)n;}"),
        bad("incompatible", "int F(){int* p=null;float* q=null;nint n=p-q;return (int)n;}",
            DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-190", "CS-SEMA-019", {
        ok("integer", "int F(){return 7%3;}"),
        bad("floating", "int F(){float a=1.0f;float b=2.0f;a%b;return 0;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-191", "CS-SEMA-019", {
        ok("matrix", "double F(){byte a=1;int b=2;float c=3.0f;return (a+b)*c/2.0-c;}"),
    });
    addCase(registry, "TS-SEMA-192", "CS-SEMA-019", {
        bad("cascade", "int F(){return missing+1;}", DiagnosticId::UnknownIdentifier),
    });
    addCase(registry, "TS-SEMA-193", "CS-SEMA-019", {
        bad("unknown-unary", "int F(){return @1;}", DiagnosticId::UnexpectedCharacter),
        bad("unknown-binary", "int F(){return 1 @ 2;}", DiagnosticId::UnexpectedCharacter),
    });
    addCase(registry, "TS-SEMA-194", "CS-SEMA-019", {
        ok("records", "int F(int a,int b){int c=-a;c=c+b;c=c*b;c=c/b;c=c%b;c=c<<1;return c==b?~c:c;}"),
    });
}
}
