#pragma once

#include "nativesharp/Passes/AnalysisKind.h"

namespace nativesharp::passes
{
class PreservedAnalyses final
{
public:
    static PreservedAnalyses all() noexcept;
    static PreservedAnalyses none() noexcept;
    static PreservedAnalyses cfgAndDominance() noexcept;

    bool preserves(AnalysisKind kind) const noexcept;

private:
    PreservedAnalyses(bool cfg, bool dominance) noexcept;

    bool cfg_ = false;
    bool dominance_ = false;
};
}
