#include "SemaFlowTestSupport.h"

namespace nsc::suite
{
namespace
{
using sema_flow_support::DiagnosticId;
using sema_flow_support::Scenario;
using sema_flow_support::addCase;

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
    addCase(registry, "TS-SEMA-123", "CS-SEMA-015", {
        bad("void-param", "void F(void x);", DiagnosticId::InvalidType),
        bad("array-param", "void F(int[2] x);", DiagnosticId::InvalidType),
        bad("incomplete-param", "struct S; void F(S x);", DiagnosticId::IncompleteType),
    });
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
    addCase(registry, "TS-SEMA-128", "CS-SEMA-015", {
        bad("extern-body", "extern C { int F(){return 0;} }", DiagnosticId::InvalidType,
            "extern C functions cannot have a body in NativeSharp 2.0"),
    });
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
        bad("declared", "int Main();", DiagnosticId::MissingFunctionDefinition, {}, true),
        ok("defined", "int Main(){return 0;}", {"Main"}, true),
    });
    addCase(registry, "TS-SEMA-133", "CS-SEMA-015", {
        bad("fallthrough", "int F(bool b){if(b){return 1;}}", DiagnosticId::NotAllPathsReturn),
        ok("complete", "int F(bool b){if(b){return 1;}return 0;}", {"F"}),
    });
    addCase(registry, "TS-SEMA-134", "CS-SEMA-015", {
        ok("scope", "int F(int x){ { int y=x; } return x; } int G(){int x=2;return x;}", {"F", "G"}),
    });
    addCase(registry, "TS-SEMA-135", "CS-SEMA-015", {
        bad("extern-text", "extern C { int F(){return 0;} }", DiagnosticId::InvalidType,
            "NativeSharp 2.0"),
        bad("main-text", "int Main(int x){return x;}", DiagnosticId::InvalidMainSignature,
            "NativeSharp 2.0 Main"),
    });
}
}
