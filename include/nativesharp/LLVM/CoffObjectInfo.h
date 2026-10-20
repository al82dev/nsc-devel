#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
struct CoffObjectInfo final
{
    std::uint16_t machine = 0;
    std::uint16_t sectionCount = 0;
    std::uint16_t optionalHeaderSize = 0;
};
}
