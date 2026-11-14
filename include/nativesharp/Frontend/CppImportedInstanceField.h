#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
    std::optional<bool> microsoftVolatileSemantics;
    std::optional<std::string> clangUsr;
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
    bool isAnonymousCarrier = false;
    // The field is a pointer whose pointee is `volatile` in C. / El campo es un puntero a un apuntado `volatile` en C.
    bool pointeeVolatile = false;
    // Physical element stride for each dimension of a non-contiguous C++ vector array.
    std::vector<std::uint64_t> cppArrayElementStrides;
    // Clang type size and natural alignment for each array-view dimension.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> cppArrayPhysicalLayouts;
};
}
