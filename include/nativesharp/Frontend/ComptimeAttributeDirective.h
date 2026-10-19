#pragma once

#include "nativesharp/Frontend/ComptimeAttributeRetention.h"
#include "nativesharp/Frontend/ComptimeAttributeTarget.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ComptimeAttributeDirective final
{
    std::size_t offset = 0;
    std::size_t length = 0;
    std::string identity;
    bool isDefinition = false;
    std::vector<ComptimeAttributeTarget> targets;
    bool repeatable = false;
    ComptimeAttributeRetention retention = ComptimeAttributeRetention::CompileTime;
    std::vector<std::string> constantArguments;
};
}
