#include "SemaFlowTestSupport.h"

namespace nsc::suite
{
namespace
{
using sema_flow_support::DiagnosticId;
using sema_flow_support::Scenario;
using sema_flow_support::addCase;

Scenario ok(std::string label, std::string source, std::vector<std::string> globals = {})
{
    return {std::move(label), std::move(source), false, true, {}, std::move(globals)};
}

Scenario bad(std::string label, std::string source, DiagnosticId id)
{
    return {std::move(label), std::move(source), false, false, {{id, {}}}, {}};
}
}

// Registers CS-SEMA-016 global and local variable contracts.
void registerSemaVariableModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-136", "CS-SEMA-016", {
        ok("global", "int Count;", {"Count"}),
    });
    addCase(registry, "TS-SEMA-137", "CS-SEMA-016", {
        ok("constant", "long Count = 42;", {"Count"}),
    });
    addCase(registry, "TS-SEMA-138", "CS-SEMA-016", {
        bad("incompatible", "int Count = \"bad\";", DiagnosticId::InvalidConversion),
    });
    addCase(registry, "TS-SEMA-139", "CS-SEMA-016", {
        bad("runtime", "int F(){return 1;} int Count=F();", DiagnosticId::GlobalRequiresConstant),
    });
    addCase(registry, "TS-SEMA-140", "CS-SEMA-016", {
        bad("const-runtime", "int F(){return 1;} const int Count=F();", DiagnosticId::ConstRequiresConstant),
    });
    addCase(registry, "TS-SEMA-141", "CS-SEMA-016", {
        ok("local", "int F(){int value; return 0;}", {"F"}),
    });
    addCase(registry, "TS-SEMA-142", "CS-SEMA-016", {
        ok("constant", "int F(){long value=42;return (int)value;}", {"F"}),
        ok("runtime", "int F(int x){long value=x;return (int)value;}", {"F"}),
    });
    addCase(registry, "TS-SEMA-143", "CS-SEMA-016", {
        bad("local-const", "int F(int x){const int value=x;return value;}",
            DiagnosticId::ConstRequiresConstant),
    });
    addCase(registry, "TS-SEMA-144", "CS-SEMA-016", {
        ok("local-array", "int F(){int[2] values;return values[0];}", {"F"}),
        bad("global-array-init", "int[2] values = 1;", DiagnosticId::InvalidConversion),
    });
    addCase(registry, "TS-SEMA-145", "CS-SEMA-016", {
        bad("duplicate", "int F(){int x;int x;return 0;}", DiagnosticId::DuplicateSymbol),
        ok("shadow", "int x=1;int F(){int x=2;{int x=3;}return x;}", {"x", "F"}),
    });
}
}
