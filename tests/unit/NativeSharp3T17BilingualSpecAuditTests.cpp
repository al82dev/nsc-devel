#include "nativesharp/Language/ReleaseSemantics.h"

#include <array>
#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    const std::array clauses = {
        BilingualClause{"NS3-SYN-001", "requisito", "requirement", 2, 2, 1, 1, true, true, true, true},
        BilingualClause{"NS3-ABI-001", "requisito", "requirement", 1, 1, 0, 0, true, true, true, true}};
    std::string error;
    check(validateBilingualSpecificationAudit(clauses, error), "complete bilingual audit passes");
    auto changed = clauses; changed[0].identity = changed[1].identity;
    check(!validateBilingualSpecificationAudit(changed, error), "duplicate clause ID fails");
    changed = clauses; changed[0].englishRequirement.clear();
    check(!validateBilingualSpecificationAudit(changed, error), "missing English requirement fails");
    changed = clauses; changed[0].englishExamples = 1;
    check(!validateBilingualSpecificationAudit(changed, error), "example parity mismatch fails");
    changed = clauses; changed[0].englishCompatibilityNotes = 0;
    check(!validateBilingualSpecificationAudit(changed, error), "compatibility-note mismatch fails");
    changed = clauses; changed[0].glossaryConsistent = false;
    check(!validateBilingualSpecificationAudit(changed, error), "glossary inconsistency fails");
    changed = clauses; changed[0].linksValid = false;
    check(!validateBilingualSpecificationAudit(changed, error), "broken link fails");
    changed = clauses; changed[0].codeSamplesCompile = false;
    check(!validateBilingualSpecificationAudit(changed, error), "broken sample fails");
    changed = clauses; changed[0].presentInFinalIndexes = false;
    check(!validateBilingualSpecificationAudit(changed, error), "orphan clause fails");
    const std::array<BilingualClause, 0> empty{};
    check(!validateBilingualSpecificationAudit(empty, error), "empty specification fails");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " bilingual specification checks\n";
    return 0;
}
