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

Scenario lexicalError(std::string label, std::string source, DiagnosticId id)
{
    return {std::move(label), std::move(source), false, false, {{id, {}}}, {}, false};
}
}

// Registers CS-SEMA-018 identifier binding and literal-value contracts.
void registerSemaIdentifierLiteralModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-162", "CS-SEMA-018", {
        ok("variable", "int g=1;int F(int p){int l=g+p;return l;}"),
        ok("field", "struct S{int X;} int F(){S s;s.X=1;return s.X;}"),
    });
    addCase(registry, "TS-SEMA-163", "CS-SEMA-018", {
        ok("callee", "int G(){return 1;}int F(){return G();}"),
        bad("value", "int G(){return 1;}int F(){int x=G;return x;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-164", "CS-SEMA-018", {
        bad("struct", "struct S{} int F(){S;return 0;}", DiagnosticId::InvalidOperand),
        bad("enum", "enum E{A} int F(){E;return 0;}", DiagnosticId::InvalidOperand),
    });
    addCase(registry, "TS-SEMA-165", "CS-SEMA-018", {
        bad("unknown", "int F(){return missing;}", DiagnosticId::UnknownIdentifier),
    });
    addCase(registry, "TS-SEMA-166", "CS-SEMA-018", {
        ok("bases", "int F(){int a=42;int b=0x2A;int c=0b101010;int d=052;return a+b+c+d;}"),
        ok("suffix", "long F(){uint a=1u;long b=2L;ulong c=3uL;return b+(long)a+(long)c;}"),
    });
    addCase(registry, "TS-SEMA-167", "CS-SEMA-018", {
        lexicalError("malformed", "int F(){return 0xGG;}", DiagnosticId::InvalidNumericLiteral),
        lexicalError("overflow", "int F(){return 999999999999999999999999999999;}",
            DiagnosticId::InvalidNumericLiteral),
    });
    addCase(registry, "TS-SEMA-168", "CS-SEMA-018", {
        ok("finite", "double F(){float a=1.5f;double b=2.25;return a+b;}"),
        lexicalError("malformed", "double F(){return 1e+;}", DiagnosticId::InvalidNumericLiteral),
    });
    addCase(registry, "TS-SEMA-169", "CS-SEMA-018", {
        ok("character", "int F(){char c='A';return (int)c;}"),
        ok("string-nul", "int F(){char c=\"a\\0b\"[1];return (int)c;}"),
    });
    addCase(registry, "TS-SEMA-170", "CS-SEMA-018", {
        ok("booleans", "int F(){bool a=true;bool b=false;if(a&&!b){return 1;}return 0;}"),
        ok("null", "int F(){int* p=null;if(p==null){return 1;}return 0;}"),
    });
    addCase(registry, "TS-SEMA-171", "CS-SEMA-018", {
        bad("this", "int F(){this;return 0;}", DiagnosticId::InvalidMemberAccess),
    });
    addCase(registry, "TS-SEMA-172", "CS-SEMA-018", {
        bad("placement", "int F(){int x;int* p=&x;new(p) int(1);return x;}",
            DiagnosticId::PlacementConstructionNotAvailable),
    });
}
}
