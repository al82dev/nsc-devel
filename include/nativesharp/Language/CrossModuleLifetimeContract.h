#pragma once

#include "nativesharp/Language/ModuleUnloadPolicy.h"

#include <cstdint>
#include <string>

namespace nativesharp::language
{
inline constexpr std::uint16_t kCrossModuleLifetimeAbiMajor = 1;
inline constexpr std::uint16_t kCrossModuleLifetimeAbiMinor = 0;

struct CrossModuleLifetimeContract final
{
    std::uint16_t abiMajor = kCrossModuleLifetimeAbiMajor;
    std::uint16_t abiMinor = kCrossModuleLifetimeAbiMinor;
    std::string producerModule;
    std::string typeIdentity;
    std::string allocatorIdentity;
    std::string createThunk;
    std::string destroyThunk;
    std::string deallocateThunk;
    ModuleUnloadPolicy unloadPolicy = ModuleUnloadPolicy::RejectWhileObjectsLive;
};
}
