#include "nativesharp/Passes/PassRequirements.h"

namespace nativesharp::passes
{
PassRequirements::PassRequirements(bool cfg, bool dominance) noexcept
    : cfg_(cfg),
      dominance_(dominance)
{
}

PassRequirements PassRequirements::none() noexcept
{
    return PassRequirements(false, false);
}

PassRequirements PassRequirements::cfg() noexcept
{
    return PassRequirements(true, false);
}

PassRequirements PassRequirements::dominance() noexcept
{
    return PassRequirements(true, true);
}

bool PassRequirements::needsCfg() const noexcept { return cfg_; }
bool PassRequirements::needsDominance() const noexcept { return dominance_; }
}
