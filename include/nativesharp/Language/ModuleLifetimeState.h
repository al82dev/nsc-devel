#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct ModuleLifetimeState final
{
    std::string contractFingerprint;
    std::size_t liveObjects = 0;
    bool unloading = false;
};
}
