#pragma once

namespace nativesharp::passes
{
class PassRequirements final
{
public:
    static PassRequirements none() noexcept;
    static PassRequirements cfg() noexcept;
    static PassRequirements dominance() noexcept;

    bool needsCfg() const noexcept;
    bool needsDominance() const noexcept;

private:
    PassRequirements(bool cfg, bool dominance) noexcept;

    bool cfg_ = false;
    bool dominance_ = false;
};
}
