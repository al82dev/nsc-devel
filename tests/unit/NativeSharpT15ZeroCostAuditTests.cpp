#include "nativesharp/Language/IntegrationQuality.h"

#include <array>
#include <iostream>
#include <string>

using namespace nativesharp::language;

namespace
{
int checks = 0;
int failures = 0;
void check(bool value, const char* message)
{
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

CostAuditRecord record(GeneratedCost kind, const char* construct, CostOptimization optimization)
{
    return {{kind, construct, true}, optimization, true, 1, 1, {}};
}
}

int main()
{
    std::array records = {
        record(GeneratedCost::Allocation, "owned-array", CostOptimization::Off),
        record(GeneratedCost::Copy, "copy-value", CostOptimization::Off),
        record(GeneratedCost::Drop, "owned-scope", CostOptimization::Off),
        record(GeneratedCost::DynamicDispatch, "dyn-trait-call", CostOptimization::On),
        record(GeneratedCost::UnwindEdge, "typed-throws", CostOptimization::On),
        record(GeneratedCost::AtomicFence, "atomic-release", CostOptimization::On)};
    std::string error;
    check(validateZeroCostAudit(records, error), "complete MIR/assembly cost audit passes");

    std::array<CostEvent, 2> explicitCosts = {{{GeneratedCost::Copy, "copy", true},
                                               {GeneratedCost::Drop, "drop", true}}};
    check(hiddenCostAuditPasses(explicitCosts, error), "explicit costs pass the legacy gate");
    auto hidden = records;
    hidden[0].event.explicitlyRequested = false;
    check(!validateZeroCostAudit(hidden, error), "unapproved hidden allocation is rejected");
    hidden[0].approvedException = "ABI-mandated ownership box";
    check(validateZeroCostAudit(hidden, error), "documented exception is accepted");
    auto missingMir = records;
    missingMir[1].mirOccurrences = 0;
    check(!validateZeroCostAudit(missingMir, error), "missing MIR evidence is rejected");
    auto missingAsm = records;
    missingAsm[2].assemblyOccurrences = 0;
    check(!validateZeroCostAudit(missingAsm, error), "missing assembly evidence is rejected");
    auto linkedFixture = records;
    linkedFixture[3].libraryFree = false;
    check(!validateZeroCostAudit(linkedFixture, error), "library-dependent fixture is rejected");
    auto duplicate = records;
    duplicate[5].event.sourceConstruct = duplicate[4].event.sourceConstruct;
    check(!validateZeroCostAudit(duplicate, error), "duplicate construct profile is rejected");
    auto oneMode = records;
    for (auto& item : oneMode) item.optimization = CostOptimization::Off;
    check(!validateZeroCostAudit(oneMode, error), "single optimization mode is rejected");
    auto mislabeled = records;
    mislabeled[0].approvedException = "not needed";
    check(!validateZeroCostAudit(mislabeled, error), "explicit cost cannot be an exception");

    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " zero-cost audit checks\n";
    return 0;
}
