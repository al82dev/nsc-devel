#include "nativesharp/Passes/OptimizationModule.h"

#include <utility>

namespace nativesharp::passes
{
OptimizationModule::OptimizationModule(
    const ssa::SsaModule* sourceSsa,
    mir::MirModule* module,
    mir::MirArena* arena) noexcept
    : sourceSsa_(sourceSsa),
      module_(module),
      arena_(arena)
{
}

const ssa::SsaModule* OptimizationModule::sourceSsa() const noexcept { return sourceSsa_; }
mir::MirModule* OptimizationModule::module() const noexcept { return module_; }
mir::MirArena* OptimizationModule::arena() const noexcept { return arena_; }

void OptimizationModule::setCfgModule(const cfg::CfgModule* module) noexcept { cfgModule_ = module; }
const cfg::CfgModule* OptimizationModule::cfgModule() const noexcept { return cfgModule_; }

void OptimizationModule::setDominanceModule(
    const dominance::DominanceModule* module) noexcept
{
    dominanceModule_ = module;
}

const dominance::DominanceModule* OptimizationModule::dominanceModule() const noexcept
{
    return dominanceModule_;
}

void OptimizationModule::addRecord(PassRecord record)
{
    records_.push_back(std::move(record));
}

const std::vector<PassRecord>& OptimizationModule::records() const noexcept { return records_; }

void OptimizationModule::setIterations(std::size_t value) noexcept { iterations_ = value; }
std::size_t OptimizationModule::iterations() const noexcept { return iterations_; }

void OptimizationModule::setCfgBuildCount(std::size_t value) noexcept { cfgBuildCount_ = value; }
std::size_t OptimizationModule::cfgBuildCount() const noexcept { return cfgBuildCount_; }

void OptimizationModule::setDominanceBuildCount(std::size_t value) noexcept
{
    dominanceBuildCount_ = value;
}

std::size_t OptimizationModule::dominanceBuildCount() const noexcept
{
    return dominanceBuildCount_;
}
}
