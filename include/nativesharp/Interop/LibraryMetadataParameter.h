#pragma once

#include "nativesharp/Interop/LibraryMetadataDefaultValueKind.h"
#include "nativesharp/Interop/LibraryMetadataOwnershipKind.h"
#include "nativesharp/Interop/LibraryMetadataParameterPassKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataParameter final
{
    std::string name;
    std::string typeKey;
    LibraryMetadataOwnershipKind ownership = LibraryMetadataOwnershipKind::None;
    LibraryMetadataParameterPassKind passKind = LibraryMetadataParameterPassKind::Value;
    // NSMETA 1.24. The final parameter keeps its `params ReadOnlySpan<T>` call-site contract
    // across a LibraryReference boundary; the physical type and ABI remain unchanged.
    bool isParameterPack = false;
    // NSMETA 1.31. Source-level dynamic dispatch marker; physical typeKey is unchanged.
    bool isDynamicTrait = false;
    std::string lifetimeName;
    // NSMETA 1.3. `defaultValueBits` stores the integer/character magnitude, a boolean bit, or
    // the IEEE-754 bits of the parsed double literal. Strings use `defaultValueText`; null has no
    // payload. Bit 0 of `defaultValueFlags` preserves a unary minus for integer literals.
    LibraryMetadataDefaultValueKind defaultValueKind = LibraryMetadataDefaultValueKind::None;
    std::uint8_t defaultValueFlags = 0;
    std::uint64_t defaultValueBits = 0;
    std::string defaultValueText;
    // NSMETA 1.38. UINT32_MAX means no length link. Callback lifetime: 0 none, 1 scoped,
    // 2 escaping. These contracts are semantic and do not alter the physical parameter ABI.
    std::uint32_t ffiLengthParameter = UINT32_MAX;
    std::string ffiLengthParameterName;
    std::uint8_t ffiCallbackLifetime = 0;
};
}
