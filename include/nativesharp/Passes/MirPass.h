#pragma once

#include "nativesharp/Passes/PassRequirements.h"
#include "nativesharp/Passes/PassRunResult.h"
#include "nativesharp/Passes/PreservedAnalyses.h"

#include <string>
#include <string_view>

namespace nativesharp::passes
{
class PassContext;

class MirPass
{
public:
    MirPass() = default;
    MirPass(const MirPass&) = delete;
    MirPass& operator=(const MirPass&) = delete;
    virtual ~MirPass() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual PassRequirements requirements() const noexcept = 0;
    virtual PreservedAnalyses preservedAnalyses() const noexcept = 0;
    virtual PassRunResult run(PassContext& context, std::string& error) = 0;
};
}
