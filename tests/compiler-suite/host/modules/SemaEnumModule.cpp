#include "SemaFlowTestSupport.h"

namespace nsc::suite
{
namespace
{
using sema_flow_support::DiagnosticId;
using sema_flow_support::ExpectedDiagnostic;
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

// Registers CS-SEMA-014 enum declaration and initializer contracts.
void registerSemaEnumModule(TestRegistry& registry)
{
    addCase(registry, "TS-SEMA-112", "CS-SEMA-014", {
        ok("empty", "enum Empty { }", {"Empty"}),
    });
    addCase(registry, "TS-SEMA-113", "CS-SEMA-014", {
        ok("automatic", "enum E { A, B, C }", {"E"}),
    });
    addCase(registry, "TS-SEMA-114", "CS-SEMA-014", {
        ok("signed", "enum E { Minus = -1, Plus = 2u }", {"E"}),
    });
    addCase(registry, "TS-SEMA-115", "CS-SEMA-014", {
        ok("previous", "enum E { A = 4, B = A + 3 }", {"E"}),
    });
    addCase(registry, "TS-SEMA-116", "CS-SEMA-014", {
        bad("forward", "enum E { A = B, B = 1 }", DiagnosticId::UnknownIdentifier),
        bad("unknown", "enum E { A = Missing }", DiagnosticId::UnknownIdentifier),
    });
    addCase(registry, "TS-SEMA-117", "CS-SEMA-014", {
        bad("floating", "enum E { A = 1.5 }", DiagnosticId::InvalidEnumInitializer),
        bad("runtime", "int F(); enum E { A = F() }", DiagnosticId::InvalidEnumInitializer),
    });
    addCase(registry, "TS-SEMA-118", "CS-SEMA-014", {
        bad("duplicate", "enum E { A, A }", DiagnosticId::DuplicateSymbol),
    });
    addCase(registry, "TS-SEMA-119", "CS-SEMA-014", {
        bad("overflow", "enum E { Max = 2147483647, Overflow }", DiagnosticId::InvalidEnumInitializer),
    });
    addCase(registry, "TS-SEMA-120", "CS-SEMA-014", {
        bad("underlying", "enum E : byte { A }", DiagnosticId::InvalidEnumInitializer),
    });
}
}
