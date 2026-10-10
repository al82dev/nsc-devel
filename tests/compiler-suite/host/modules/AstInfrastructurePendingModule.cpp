#include "AstInfrastructureTestSupport.h"

namespace nsc::suite
{
void registerAstInfrastructurePendingModule(TestRegistry& registry)
{
    using ast_infrastructure_support::addInfrastructurePendingCase;

    addInfrastructurePendingCase(registry, "TA-AST-014", "CA-AST-002", "unit",
        "deterministic registration failure after object construction",
        "the current allocation leak is characterized without reporting a false PASS",
        "deterministic allocation or registration failure injection");
    addInfrastructurePendingCase(registry, "TA-AST-016", "CA-AST-002", "unit",
        "destructor throws while the arena destroys its entries",
        "the child process terminates and remaining-entry behavior is characterized",
        "isolated death-test subprocess");
    addInfrastructurePendingCase(registry, "TA-AST-017", "CA-AST-002", "unit",
        "unsynchronized concurrent arena calls",
        "the absence of a synchronization guarantee is diagnosed as a real data race",
        "ThreadSanitizer-instrumented target");
    addInfrastructurePendingCase(registry, "TA-AST-215", "CA-AST-019", "component",
        "AstDumper receives a null type argument",
        "the violated precondition and resulting failure are characterized in isolation",
        "isolated death-test subprocess");
    addInfrastructurePendingCase(registry, "TA-AST-223", "CA-AST-020", "robustness",
        "AstDumper traverses a self-cycle through a slot",
        "the current non-termination is characterized by an external deterministic limit",
        "isolated subprocess with watchdog");
    addInfrastructurePendingCase(registry, "TA-AST-224", "CA-AST-020", "robustness",
        "AstDumper traverses a two-node children cycle",
        "the current non-termination is characterized by an external deterministic limit",
        "isolated subprocess with watchdog");
}
}
