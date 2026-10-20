#pragma once

#include "nativesharp/Runtime/RuntimeCapability.h"
#include "nativesharp/Runtime/RuntimeCapabilityStatus.h"

#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::runtime
{
std::vector<RuntimeCapability> runtimeCapabilities();
std::string runtimeCapabilityReport();
const char* runtimeCapabilityStatusName(RuntimeCapabilityStatus status) noexcept;
bool runtimeCapabilityReady(std::string_view id, std::string& detail);
}
