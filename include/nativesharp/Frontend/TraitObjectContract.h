#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::frontend
{
// NativeSharp 3.0 T05-F05. A `dyn Trait` is deliberately not a boxed value and not a
// Rust-style two-word fat pointer. On Windows x64 it is the existing one-word object reference;
// the object's class descriptor owns the stable interface map that resolves the trait table.
// Keeping this as an explicit frontend contract prevents documentation/metadata/backend code
// from inventing different representations for the same source type.
struct TraitObjectContract final
{
    std::size_t traitTypeOrdinal = 0;
    std::uint64_t size = 8;
    std::uint64_t alignment = 8;
    bool objectSafe = false;
    bool descriptorInterfaceMap = true;
    bool nullIsZero = true;
    bool conversionAllocates = false;
};
}
