#pragma once

#include "nativesharp/Passes/PassRecord.h"

#include <cstddef>
#include <vector>

namespace nativesharp::cfg
{
class CfgModule;
}

namespace nativesharp::dominance
{
class DominanceModule;
}

namespace nativesharp::mir
{
class MirArena;
class MirModule;
}

namespace nativesharp::ssa
{
class SsaModule;
}

namespace nativesharp::passes
{
class OptimizationModule final
{
public:
    OptimizationModule(
        const ssa::SsaModule* sourceSsa,
        mir::MirModule* module,
        mir::MirArena* arena) noexcept;

    const ssa::SsaModule* sourceSsa() const noexcept;
    mir::MirModule* module() const noexcept;
    mir::MirArena* arena() const noexcept;

    void setCfgModule(const cfg::CfgModule* module) noexcept;
    const cfg::CfgModule* cfgModule() const noexcept;

    void setDominanceModule(const dominance::DominanceModule* module) noexcept;
    const dominance::DominanceModule* dominanceModule() const noexcept;

    void addRecord(PassRecord record);
    const std::vector<PassRecord>& records() const noexcept;

    void setIterations(std::size_t value) noexcept;
    std::size_t iterations() const noexcept;

    void setCfgBuildCount(std::size_t value) noexcept;
    std::size_t cfgBuildCount() const noexcept;

    void setDominanceBuildCount(std::size_t value) noexcept;
    std::size_t dominanceBuildCount() const noexcept;

private:
    const ssa::SsaModule* sourceSsa_;
    mir::MirModule* module_;
    mir::MirArena* arena_;
    const cfg::CfgModule* cfgModule_ = nullptr;
    const dominance::DominanceModule* dominanceModule_ = nullptr;
    std::vector<PassRecord> records_;
    std::size_t iterations_ = 0;
    std::size_t cfgBuildCount_ = 0;
    std::size_t dominanceBuildCount_ = 0;
};
}
