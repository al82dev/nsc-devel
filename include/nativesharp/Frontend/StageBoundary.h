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
}
