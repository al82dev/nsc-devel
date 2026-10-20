#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::driver
{
struct MigrationTextEdit final
{
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    std::string replacement;
    std::string equivalenceProof;
};
} // namespace nativesharp::driver
