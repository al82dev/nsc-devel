#pragma once

#include "nativesharp/Passes/MirPass.h"

namespace nativesharp::passes
{
class SimplifyControlFlowPass final : public MirPass
{
public:
    std::string_view name() const noexcept override;
    PassRequirements requirements() const noexcept override;
    PreservedAnalyses preservedAnalyses() const noexcept override;
    PassRunResult run(PassContext& context, std::string& error) override;
};
}
