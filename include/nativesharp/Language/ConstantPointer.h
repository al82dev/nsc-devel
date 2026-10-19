#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::language
{
struct ConstantPointer final
{
    std::uint64_t allocation = 0;
    std::size_t offset = 0;
};
}
