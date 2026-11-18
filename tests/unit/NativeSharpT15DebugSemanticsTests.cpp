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

SemanticDebugEntry entry(std::string role, DebugEntityKind kind, DebugOptimization optimization)
{
    SemanticDebugEntry result;
    result.stableName = semanticDebugName("Main", role, static_cast<std::size_t>(kind));
    result.typeName = kind == DebugEntityKind::VariantPayload ? "Result::Ok(i32)" : "i32";
    result.kind = kind;
    result.optimization = optimization;
    result.scopeBeginLine = 10;
    result.scopeEndLine = 20;
    result.sequencePointLines = {10, 15, 20};
    result.sourceGenerated = kind == DebugEntityKind::CoroutineState || kind == DebugEntityKind::Temporary;
    result.hasInlineSite = kind == DebugEntityKind::InlineSite;
    return result;
}
}

int main()
{
    std::array evidence = {
        entry("local", DebugEntityKind::Local, DebugOptimization::O0),
        entry("inline", DebugEntityKind::InlineSite, DebugOptimization::O0),
        entry("capture", DebugEntityKind::ClosureCapture, DebugOptimization::O0),
        entry("variant", DebugEntityKind::VariantPayload, DebugOptimization::O0),
        entry("owner", DebugEntityKind::OwnershipState, DebugOptimization::O0),
        entry("exception", DebugEntityKind::ExceptionValue, DebugOptimization::O0),
        entry("coroutine", DebugEntityKind::CoroutineState, DebugOptimization::O0),
        entry("temporary", DebugEntityKind::Temporary, DebugOptimization::O2)};
    std::string error;
    check(validateSemanticDebugEvidence(evidence, error), "complete O0/O2 evidence passes");
    check(semanticToolingSchemaVersion() == "nsc-semantic-tooling-v1", "tooling schema remains stable");
    check(semanticDebugName("Owner", "async state", 2) == "Owner$async_state$2", "stable generated name");

    auto invented = evidence;
    invented[7].valueInvented = true;
    check(!validateSemanticDebugEvidence(invented, error), "invented O2 value is rejected");
    auto missingPoints = evidence;
    missingPoints[0].sequencePointLines.clear();
    check(!validateSemanticDebugEvidence(missingPoints, error), "missing O0 sequence points are rejected");
    auto badLifetime = evidence;
    badLifetime[1].scopeEndLine = 9;
    check(!validateSemanticDebugEvidence(badLifetime, error), "invalid lifetime is rejected");
    auto outsideScope = evidence;
    outsideScope[2].sequencePointLines = {21};
    check(!validateSemanticDebugEvidence(outsideScope, error), "sequence point outside scope is rejected");
    auto missingInline = evidence;
    missingInline[1].hasInlineSite = false;
    check(!validateSemanticDebugEvidence(missingInline, error), "missing inline-site record is rejected");
    auto fakeLocal = evidence;
    fakeLocal[0].sourceGenerated = true;
    check(!validateSemanticDebugEvidence(fakeLocal, error), "generated state cannot masquerade as local");
    auto optimizedOutO0 = evidence;
    optimizedOutO0[3].availability = DebugAvailability::OptimizedOut;
    check(!validateSemanticDebugEvidence(optimizedOutO0, error), "optimized-out O0 value is rejected");

    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " semantic debug checks\n";
    return 0;
}
