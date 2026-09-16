#pragma once

#include <string>
#include <string_view>

namespace nativesharp::cfg { class CfgModule; }
namespace nativesharp::dominance { class DominanceModule; }
namespace nativesharp::hir { class HirNode; }
namespace nativesharp::mir { class MirModule; }
namespace nativesharp::passes { class OptimizationModule; }
namespace nativesharp::ssa { class SsaModule; }

namespace nativesharp::frontend
{
enum class StageBoundary
{
    HirToMir,
    MirToCfg,
    CfgToDominance,
    DominanceToSsa,
    SsaToOptimization,
    OptimizationToBackend
};

std::string_view stageBoundaryName(StageBoundary boundary) noexcept;

// Applies the verifier owned by each producer exactly where its output crosses into the next
// stage. Diagnostics deliberately identify the boundary as well as the local invariant.
class StageBoundaryVerifier final
{
public:
    bool verify(StageBoundary boundary, const hir::HirNode* value);
    bool verify(StageBoundary boundary, const mir::MirModule* value);
    bool verify(StageBoundary boundary, const cfg::CfgModule* value);
    bool verify(StageBoundary boundary, const dominance::DominanceModule* value);
    bool verify(StageBoundary boundary, const ssa::SsaModule* value);
    bool verify(StageBoundary boundary, const passes::OptimizationModule* value);

    StageBoundary boundary() const noexcept;
    const std::string& error() const noexcept;

private:
    bool accept(StageBoundary boundary);
    bool reject(StageBoundary boundary, std::string detail);

    StageBoundary boundary_ = StageBoundary::HirToMir;
    std::string error_;
};
}
