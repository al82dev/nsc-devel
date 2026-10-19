#pragma once

#include "nativesharp/Frontend/ComptimeAttributeRetention.h"
#include "nativesharp/Frontend/ComptimeAttributeTarget.h"

#include <vector>

namespace nativesharp::frontend::internal
{
struct ComptimeAttributeDefinition final
{
    std::vector<ComptimeAttributeTarget> targets;
    bool repeatable = false;
    ComptimeAttributeRetention retention = ComptimeAttributeRetention::CompileTime;
};
}
