#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ErrorSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/IntegrationQuality.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"
#include "nativesharp/Language/ModuleSemantics.h"
#include "nativesharp/Language/NumericSemantics.h"
#include "nativesharp/Language/OwnershipSemantics.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    const std::array<InteractionCase, 6> matrix = {{
        {"variant-move-drop", {FeatureArea::Variants, FeatureArea::Ownership}, 101},
        {"generic-trait-associated", {FeatureArea::Generics, FeatureArea::Traits}, 102},
        {"borrow-across-await", {FeatureArea::Ownership, FeatureArea::Coroutines}, 103},
        {"result-async-cancel", {FeatureArea::Errors, FeatureArea::Coroutines}, 104},
        {"module-ffi-contract", {FeatureArea::Modules, FeatureArea::Ffi}, 105},
        {"derive-module-fingerprint", {FeatureArea::Metaprogramming, FeatureArea::Modules}, 106}}};
    tests.expect(validateInteractionMatrix(matrix, error),
        "integration matrix covers every prioritized high-risk feature pair with fixed seeds");

    const std::array<WholeProgramWitness, 6> corpus = {{{"parser", true, true, false},
        {"arena", true, true, false}, {"job-graph", true, true, false},
        {"serializer", true, true, false}, {"dll-host-plugin", true, true, false},
        {"async-server", true, true, false}}};
    tests.expect(validateWholeProgramCorpus(corpus, error),
        "whole-program witness catalog is deterministic, incremental, and SDK-independent");
    tests.expect(withinPerformanceBudget({20, 1024, 100, 8, 2048}, {25, 2048, 120, 10, 4096}) &&
            !withinPerformanceBudget({30, 1024, 100, 8, 2048}, {25, 2048, 120, 10, 4096}),
        "frontend, memory, MIR, monomorphization, and code-size budgets are independently enforced");
    tests.expect(diagnosticPassesQualityGate(
            {"NS3015", "borrow crosses await", "prestamo cruza await", true, true, 1}, error),
        "diagnostic gate requires bilingual stable diagnostics and semantics-preserving fix-its");
    tests.expect(semanticDebugName("Demo.Work", "async state", 3) == "Demo.Work$async_state$3" &&
            semanticToolingSchemaVersion() == "nsc-semantic-tooling-v1",
        "tooling sees stable semantic names and an explicit schema version");
    tests.expect(hiddenCostAuditPasses(
            std::array<CostEvent, 2>{{{GeneratedCost::Drop, "move closure", true},
                {GeneratedCost::AtomicFence, "atomic release", true}}}, error) &&
            !hiddenCostAuditPasses(
                std::array<CostEvent, 1>{{{GeneratedCost::Allocation, "params", false}}}, error),
        "cost audit accepts explicit costs and rejects hidden allocation/copy/drop/dispatch/unwind/fence");

    MoveState moved{false, false, {true}};
    tests.expect(moveWholeValue(moved, error) && classifyValueState(moved) == ValueState::Moved &&
            !borrowMayCrossAwait(false, true),
        "ownership and coroutine contracts compose without permitting an escaped moved borrow");
    ConstraintSet constraints;
    tests.expect(normalizeConstraints(std::array<std::string, 1>{"trait:Hash"}, constraints, error) &&
            dispatchMetadata(DispatchKind::Static, "Hash") == "static:Hash",
        "generic constraints preserve explicit static trait dispatch");
    const std::array<VariantCase, 2> cases = {{{"Ok", 8, 8}, {"Error", 8, 8}}};
    tests.expect(computeWindowsX64VariantLayout(cases).has_value() &&
            validateThrowContract({"Error", ThrowAbi::ResultCarrier, false, false}, error),
        "variant result layout composes with typed error ABI");
    tests.expect(validateRepresentation({Representation::C, 8, 8, 2}, error) &&
            publicApiFingerprintInput(std::array<PublicApiSymbol, 1>{{
                {"Demo.Api", "fn(C*)", "nothrow", "repr(C)", true}}}).find("repr(C)") != std::string::npos,
        "FFI representation participates in the public API fingerprint");
    tests.expect(comptimeInputIdentity(std::array<ComptimeInput, 1>{{{"schema", "abc"}}}).find("abc") !=
            std::string::npos && constevalMatchesRuntime(127, 1, {8, true}, OverflowPolicy::Wrapping),
        "metaprogram inputs and numeric consteval remain reproducible in the integrated contract");
    return tests.result();
}
