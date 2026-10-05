#include "nativesharp/Language/ReleaseSemantics.h"

#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    SemanticFreezeManifest manifest{"overload-v3", "conversion-v3", "inference-v3",
        "ownership-drop-v3", "errors-v3", "async-v3", "memory-v3", {"3.1-traits"}, true, true};
    std::string error;
    check(validateSemanticFreeze(manifest, error), "complete semantic freeze evidence passes");
    auto changed = manifest; changed.overloadRevision.clear();
    check(!validateSemanticFreeze(changed, error), "missing overload revision fails");
    changed = manifest; changed.conversionRevision.clear();
    check(!validateSemanticFreeze(changed, error), "missing conversion revision fails");
    changed = manifest; changed.inferenceRevision.clear();
    check(!validateSemanticFreeze(changed, error), "missing inference revision fails");
    changed = manifest; changed.ownershipAndDropRevision.clear();
    check(!validateSemanticFreeze(changed, error), "missing ownership/drop revision fails");
    changed = manifest; changed.asyncRevision.clear();
    check(!validateSemanticFreeze(changed, error), "missing async revision fails");
    changed = manifest; changed.memoryModelRevision.clear();
    check(!validateSemanticFreeze(changed, error), "missing memory-model revision fails");
    changed = manifest; changed.deferredIssueMilestones = {"3.0-late"};
    check(!validateSemanticFreeze(changed, error), "3.0 exception cannot bypass freeze");
    changed = manifest; changed.normativeSuitePassed = false;
    check(!validateSemanticFreeze(changed, error), "missing normative evidence fails");
    changed = manifest; changed.differentialConfigurationsDeterministic = false;
    check(!validateSemanticFreeze(changed, error), "nondeterministic configuration fails");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " semantic freeze checks\n";
    return 0;
}
