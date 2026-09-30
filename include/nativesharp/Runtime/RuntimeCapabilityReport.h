#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::runtime
{
enum class RuntimeCapabilityStatus
{
    Ready,
    Partial,
    Missing
};

struct RuntimeCapability final
{
    std::string id;
    std::string name;
    RuntimeCapabilityStatus status = RuntimeCapabilityStatus::Missing;
    std::vector<std::string> evidence;
    std::vector<std::string> missing;
};

std::vector<RuntimeCapability> runtimeCapabilities();
std::string runtimeCapabilityReport();
const char* runtimeCapabilityStatusName(RuntimeCapabilityStatus status) noexcept;
bool runtimeCapabilityReady(std::string_view id, std::string& detail);
}
