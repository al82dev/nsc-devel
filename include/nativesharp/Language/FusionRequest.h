#pragma once

#include "nativesharp/Language/EvaluationContract.h"

namespace nativesharp::language
{
struct FusionRequest final
{
    EvaluationContract source;
    EvaluationContract adapter;
    // The optimizer states what the fused loop will actually preserve. These are proof facts,
    // not optimization wishes; a missing fact rejects fusion conservatively.
    bool fusedIsLazy = true;
    bool fusedPreservesLeftToRightEffects = true;
    bool fusedPreservesShortCircuit = true;
    bool fusedEvaluatesEachSourceAtMostOnce = true;
};
}
