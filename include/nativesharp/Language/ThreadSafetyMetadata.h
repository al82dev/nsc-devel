#pragma once

#include "nativesharp/Language/ConcurrencyContractVersion.h"

#include <cstdint>
#include <string>

namespace nativesharp::language
{
struct ThreadSafetyMetadata final
{
    std::uint16_t contractMajor = kConcurrencyContractMajor;
    std::uint16_t contractMinor = kConcurrencyContractMinor;
    std::string typeName;
    bool send = false;
    bool sync = false;
    bool unsafeSend = false;
    bool unsafeSync = false;
};
}
