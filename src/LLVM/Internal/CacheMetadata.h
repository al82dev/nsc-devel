#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::llvm_backend
{
struct CacheMetadata final
{
    std::string key;
    std::string digest;
    std::uintmax_t size = 0;
};
}
