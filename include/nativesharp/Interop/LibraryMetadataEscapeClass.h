#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataEscapeClass : std::uint8_t
{
    NoEscape = 0,
    Return = 1,
    CallerOwned = 2,
    Global = 3,
    Unknown = 4,
    RefSlotNoEscape = 5,
    ObjectPointerGlobal = 6,
    // EN: The address reaches only imported C functions (language::EscapeClass::ForeignImport).
    // ES: La dirección sólo llega a funciones C importadas (language::EscapeClass::ForeignImport).
    ForeignImport = 7
};
}
