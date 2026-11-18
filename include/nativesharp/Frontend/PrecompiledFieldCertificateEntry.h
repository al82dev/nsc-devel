#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::frontend
{
struct PrecompiledFieldCertificateEntry final
{
    std::string name;
    std::string typeKey;
    std::uint32_t flags = 0;
    bool isPublic = false;
    bool isProtected = false;
    bool accessKnown = false;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
};
}
