#pragma once

#include "nativesharp/Runtime/RuntimeCapabilityStatus.h"

#include <string>
#include <vector>

namespace nativesharp::runtime
{
struct RuntimeCapability final
{
    std::string id;
    std::string name;
    RuntimeCapabilityStatus status = RuntimeCapabilityStatus::Missing;
    std::vector<std::string> evidence;
    std::vector<std::string> missing;
};
}
