#include "nativesharp/Language/ReleaseSemantics.h"

#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    Win64AbiFreezeManifest manifest{"windows-x64", "mangle-v3", "layout-v3", "calls-v3",
        "variants-v3", "traits-v3", "errors-v3", "coroutines-v3", "nsmeta-v3",
        true, true, true, true};
    std::string error;
    check(validateWin64AbiFreeze(manifest, error), "complete Win64 ABI freeze evidence passes");
    auto changed = manifest; changed.target = "linux-x64";
    check(!validateWin64AbiFreeze(changed, error), "non-Windows target fails");
    changed = manifest; changed.manglingRevision.clear();
    check(!validateWin64AbiFreeze(changed, error), "missing mangling revision fails");
    changed = manifest; changed.layoutRevision.clear();
    check(!validateWin64AbiFreeze(changed, error), "missing layout revision fails");
    changed = manifest; changed.traitObjectRevision.clear();
    check(!validateWin64AbiFreeze(changed, error), "missing trait-object revision fails");
    changed = manifest; changed.publicCoroutineRevision.clear();
    check(!validateWin64AbiFreeze(changed, error), "missing public-coroutine revision fails");
    changed = manifest; changed.preservedDllFixturesPassed = false;
    check(!validateWin64AbiFreeze(changed, error), "DLL fixture regression blocks freeze");
    changed = manifest; changed.crossVersionConsumerPassed = false;
    check(!validateWin64AbiFreeze(changed, error), "cross-version failure blocks freeze");
    changed = manifest; changed.symbolSnapshotsPassed = false;
    check(!validateWin64AbiFreeze(changed, error), "symbol mismatch blocks freeze");
    changed = manifest; changed.pdbContractPassed = false;
    check(!validateWin64AbiFreeze(changed, error), "PDB mismatch blocks freeze");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " Win64 ABI freeze checks\n";
    return 0;
}
