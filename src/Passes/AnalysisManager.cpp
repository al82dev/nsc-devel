#include "nativesharp/Passes/AnalysisManager.h"

#include "nativesharp/CFG/CfgBuilder.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Dominance/DominanceBuilder.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/Passes/OptimizationArena.h"
#include "nativesharp/Passes/OptimizationModule.h"

#include <utility>

namespace nativesharp::passes
{
AnalysisManager::AnalysisManager(
    OptimizationModule& module,
    OptimizationArena& arena)
    : module_(module),
      arena_(arena)
{
}

const cfg::CfgModule* AnalysisManager::ensureCfg()
{
    if (cfg_ != nullptr)
    {
        return cfg_;
    }

    cfg::CfgBuilder builder(arena_.cfgArena());
    cfg_ = builder.build(module_.module());
    if (cfg_ == nullptr)
    {
        fail("CFG rebuild failed after optimization: " + builder.error());
        return nullptr;
    }

    cfg::CfgVerifier verifier;
    if (!verifier.verify(cfg_))
    {
        cfg_ = nullptr;
        fail("CFG verification failed after optimization: " + verifier.error());
        return nullptr;
    }

    ++cfgBuildCount_;
    module_.setCfgModule(cfg_);
    return cfg_;
}

const dominance::DominanceModule* AnalysisManager::ensureDominance()
{
    if (dominance_ != nullptr)
    {
        return dominance_;
    }

    const cfg::CfgModule* cfgModule = ensureCfg();
    if (cfgModule == nullptr)
    {
        return nullptr;
    }

    dominance::DominanceBuilder builder(arena_.dominanceArena());
    dominance_ = builder.build(cfgModule);
    if (dominance_ == nullptr)
    {
        fail("dominance rebuild failed after optimization: " + builder.error());
        return nullptr;
    }

    dominance::DominanceVerifier verifier;
    if (!verifier.verify(dominance_))
    {
        dominance_ = nullptr;
        fail("dominance verification failed after optimization: " + verifier.error());
        return nullptr;
    }

    ++dominanceBuildCount_;
    module_.setDominanceModule(dominance_);
    return dominance_;
}

void AnalysisManager::invalidate(PreservedAnalyses preserved) noexcept
{
    if (!preserved.preserves(AnalysisKind::Cfg))
    {
        cfg_ = nullptr;
        dominance_ = nullptr;
        module_.setCfgModule(nullptr);
        module_.setDominanceModule(nullptr);
        return;
    }

    if (!preserved.preserves(AnalysisKind::Dominance))
    {
        dominance_ = nullptr;
        module_.setDominanceModule(nullptr);
    }
}

std::size_t AnalysisManager::cfgBuildCount() const noexcept { return cfgBuildCount_; }

std::size_t AnalysisManager::dominanceBuildCount() const noexcept
{
    return dominanceBuildCount_;
}

const std::string& AnalysisManager::error() const noexcept { return error_; }

bool AnalysisManager::fail(std::string message)
{
    error_ = std::move(message);
    return false;
}
}
