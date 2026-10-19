#pragma once

#include "MirFunctionKey.h"

#include <string>

namespace nativesharp::frontend::internal
{
struct MirCacheRecord final
{
    MirFunctionKey key;
    std::string interfaceFingerprint;
    std::string implementationFingerprint;
};
}
