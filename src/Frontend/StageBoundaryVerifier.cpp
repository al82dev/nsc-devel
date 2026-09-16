#include "nativesharp/Frontend/StageBoundaryVerifier.h"

#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/SSA/SsaVerifier.h"

namespace nativesharp::frontend
{
std::string_view stageBoundaryName(StageBoundary boundary) noexcept
{
    switch (boundary)
    {
        case StageBoundary::HirToMir: return "hir->mir";
        case StageBoundary::MirToCfg: return "mir->cfg";
        case StageBoundary::CfgToDominance: return "cfg->dominance";
        case StageBoundary::DominanceToSsa: return "dominance->ssa";
        case StageBoundary::SsaToOptimization: return "ssa->optimization";
        case StageBoundary::OptimizationToBackend: return "optimization->backend";
    }
    return "unknown";
}

bool StageBoundaryVerifier::verify(StageBoundary boundary, const hir::HirNode* value)
{
    hir::HirVerifier verifier;
    return verifier.verify(value) ? accept(boundary) : reject(boundary, verifier.error());
}

bool StageBoundaryVerifier::verify(StageBoundary boundary, const mir::MirModule* value)
{
    mir::MirVerifier verifier;
    return verifier.verify(value) ? accept(boundary) : reject(boundary, verifier.error());
}

bool StageBoundaryVerifier::verify(StageBoundary boundary, const cfg::CfgModule* value)
{
    cfg::CfgVerifier verifier;
    return verifier.verify(value) ? accept(boundary) : reject(boundary, verifier.error());
}

bool StageBoundaryVerifier::verify(
    StageBoundary boundary, const dominance::DominanceModule* value)
{
    dominance::DominanceVerifier verifier;
    return verifier.verify(value) ? accept(boundary) : reject(boundary, verifier.error());
}

bool StageBoundaryVerifier::verify(StageBoundary boundary, const ssa::SsaModule* value)
{
    ssa::SsaVerifier verifier;
    return verifier.verify(value) ? accept(boundary) : reject(boundary, verifier.error());
}

bool StageBoundaryVerifier::verify(
    StageBoundary boundary, const passes::OptimizationModule* value)
{
    passes::OptimizationVerifier verifier;
    return verifier.verify(value) ? accept(boundary) : reject(boundary, verifier.error());
}

StageBoundary StageBoundaryVerifier::boundary() const noexcept { return boundary_; }
const std::string& StageBoundaryVerifier::error() const noexcept { return error_; }

bool StageBoundaryVerifier::accept(StageBoundary boundary)
{
    boundary_ = boundary;
    error_.clear();
    return true;
}

bool StageBoundaryVerifier::reject(StageBoundary boundary, std::string detail)
{
    boundary_ = boundary;
    error_ = "stage boundary '" + std::string(stageBoundaryName(boundary)) + "' rejected value";
    if (!detail.empty()) error_ += ": " + detail;
    return false;
}
}
