#include "nativesharp/Passes/PreservedAnalyses.h"

namespace nativesharp::passes
{
PreservedAnalyses::PreservedAnalyses(bool cfg, bool dominance) noexcept
    : cfg_(cfg),
      dominance_(dominance)
{
}

PreservedAnalyses PreservedAnalyses::all() noexcept
{
    return PreservedAnalyses(true, true);
}

PreservedAnalyses PreservedAnalyses::none() noexcept
{
    return PreservedAnalyses(false, false);
}

PreservedAnalyses PreservedAnalyses::cfgAndDominance() noexcept
{
    return PreservedAnalyses(true, true);
}

bool PreservedAnalyses::preserves(AnalysisKind kind) const noexcept
{
    switch (kind)
    {
        case AnalysisKind::Cfg:
            return cfg_;
        case AnalysisKind::Dominance:
            return dominance_;
    }
    return false;
}
}
