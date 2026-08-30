#pragma once

#include "nativesharp/Passes/PreservedAnalyses.h"

#include <cstddef>
#include <string>

namespace nativesharp::cfg
{
class CfgModule;
}

namespace nativesharp::dominance
{
class DominanceModule;
}

namespace nativesharp::passes
{
class OptimizationArena;
class OptimizationModule;

class AnalysisManager final
{
public:
    AnalysisManager(
        OptimizationModule& module,
        OptimizationArena& arena);

    const cfg::CfgModule* ensureCfg();
    const dominance::DominanceModule* ensureDominance();

    void invalidate(PreservedAnalyses preserved) noexcept;

    std::size_t cfgBuildCount() const noexcept;
    std::size_t dominanceBuildCount() const noexcept;
    const std::string& error() const noexcept;

private:
    bool fail(std::string message);

    OptimizationModule& module_;
    OptimizationArena& arena_;
    const cfg::CfgModule* cfg_ = nullptr;
    const dominance::DominanceModule* dominance_ = nullptr;
    std::size_t cfgBuildCount_ = 0;
    std::size_t dominanceBuildCount_ = 0;
    std::string error_;
};
}
