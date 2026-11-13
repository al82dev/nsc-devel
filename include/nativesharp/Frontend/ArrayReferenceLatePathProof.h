#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::diagnostics { class DiagnosticEngine; }

namespace nativesharp::frontend
{
class ProjectCompilation;
struct ArrayReferenceReplayObservation;
struct ArrayReferenceForObservation;

// Evaluates a replay observation without modifying it. Unknown includes every
// operation that the late proof cannot establish.
enum class ArrayReferenceLatePathResult { Unknown, ProvenDeadPath };

[[nodiscard]] ArrayReferenceLatePathResult evaluateArrayReferenceLatePath(
    const ProjectCompilation& project, const ArrayReferenceReplayObservation& observation);

// Called after the construction model is built; reports only proven paths.
// A request that ends at method-body analysis does not run this later check.
void reportArrayReferenceLatePaths(const ProjectCompilation& project,
    std::span<const ArrayReferenceReplayObservation> observations,
    diagnostics::DiagnosticEngine& diagnostics);

enum class ExperimentalForFirstCondition { Unknown, False, True };
struct ExperimentalForFacts final
{
    ExperimentalForFirstCondition firstCondition = ExperimentalForFirstCondition::Unknown;
    std::size_t boundBreakCount = 0;
    bool terminationProven = false;
};

// Observation only: no reachability or termination conclusion is inferred
// from a first condition or the existence of a break.
[[nodiscard]] ExperimentalForFacts inspectExperimentalForFirstEvaluation(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation);

struct ExperimentalForBreakPath final
{
    // Conditional on entering the loop; never a claim that entry is reachable.
    bool reachesAfterLoopFromEntry = false;
    // Normal prefix from callable entry only; never a claim about Main or callers.
    bool preLoopReachabilityProven = false;
    // Declaration identities are rebound through the complete model; this
    // proves only the path from loop entry, not how entry was reached.
    std::vector<const ast::AstNode*> destroyedOwners;
};
[[nodiscard]] ExperimentalForBreakPath inspectExperimentalForBreakPath(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation);

struct ExperimentalForReadPath final
{
    // Conditional on callable entry and normal allocation; not an entrypoint
    // reachability claim. This is not consumed by diagnostic emission.
    bool provenFromCallableEntry = false;
    const ast::AstNode* receiver = nullptr;
};
[[nodiscard]] ExperimentalForReadPath inspectExperimentalForReadPath(
    const ProjectCompilation& project, const ArrayReferenceForObservation& observation);
}
