#pragma once

#include <cstdint>

namespace nativesharp::language
{
enum class SmartPointerThreadModel : std::uint8_t
{
    UniqueTransfer,
    LocalOnly,
    AtomicShared
};
}
