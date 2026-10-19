#pragma once

#include "nativesharp/Target/StructFieldLayout.h"

#include <cstdint>
#include <vector>

namespace nativesharp::target
{
struct StructLayout
{
    std::uint64_t size;
    std::uint64_t alignment;
    std::vector<StructFieldLayout> fields;
};
}
