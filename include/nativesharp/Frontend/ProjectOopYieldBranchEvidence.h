#pragma once

namespace nativesharp::frontend
{
// Syntactic and semantic observations only; neither variant proves lifetime or authorizes cleanup.
enum class ProjectOopYieldBranchEvidence
{
    Unknown,
    SemanticFreshOwnedTransfer,
    StaticFieldNonOwning
};
}
