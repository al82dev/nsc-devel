#include "nativesharp/Language/ReleaseSemantics.h"

#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    SyntaxFreezeManifest manifest{"grammar-3.0-rc", "sha-grammar", "sha-precedence",
        "sha-keywords", "sha-recovery", true, true, true, true, true};
    std::string error;
    check(validateSyntaxFreeze(manifest, error), "complete syntax freeze evidence passes");
    auto changed = manifest; changed.canonicalGrammarHash.clear();
    check(!validateSyntaxFreeze(changed, error), "missing canonical grammar hash fails");
    changed = manifest; changed.precedenceHash.clear();
    check(!validateSyntaxFreeze(changed, error), "missing precedence hash fails");
    changed = manifest; changed.reservedWordsHash.clear();
    check(!validateSyntaxFreeze(changed, error), "missing reserved-word hash fails");
    changed = manifest; changed.recoveryRulesHash.clear();
    check(!validateSyntaxFreeze(changed, error), "missing recovery hash fails");
    changed = manifest; changed.generatedFromCanonicalSource = false;
    check(!validateSyntaxFreeze(changed, error), "hand-maintained reference grammar fails");
    changed = manifest; changed.ambiguityCheckPassed = false;
    check(!validateSyntaxFreeze(changed, error), "ambiguity failure blocks freeze");
    changed = manifest; changed.formatterRoundTripPassed = false;
    check(!validateSyntaxFreeze(changed, error), "formatter mismatch blocks freeze");
    changed = manifest; changed.specificationSnippetsPassed = false;
    check(!validateSyntaxFreeze(changed, error), "broken specification snippet blocks freeze");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " syntax freeze checks\n";
    return 0;
}
