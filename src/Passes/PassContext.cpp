#include "nativesharp/Passes/PassContext.h"

namespace nativesharp::passes
{
PassContext::PassContext(
    mir::MirModule* module,
    mir::MirArena& arena,
    AnalysisManager& analyses) noexcept
    : module_(module),
      arena_(&arena),
      analyses_(&analyses)
{
}

mir::MirModule* PassContext::module() const noexcept { return module_; }
mir::MirArena& PassContext::arena() const noexcept { return *arena_; }
AnalysisManager& PassContext::analyses() const noexcept { return *analyses_; }
}
