#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nativesharp::frontend
{
// Isolated experimental metadata; field access and object lifetime are not enabled by this type.
struct CppImportedInstanceField final
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string typeKey;
    std::uint64_t byteOffset;
    bool isConst;
    bool isVolatile;
    bool pointeeReadonly = false;
    std::uint64_t byteSize = 0;
    std::uint64_t byteAlignment = 0;
};
}
