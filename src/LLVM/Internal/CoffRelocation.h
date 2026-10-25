#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
// ES: Una relocacion de una seccion COFF: el campo en `offset` (relativo a la seccion) se resuelve con el
// simbolo `symbolIndex` segun `type`.
// EN: One relocation of a COFF section: the field at `offset` (section-relative) is resolved with symbol
// `symbolIndex` according to `type`.
struct CoffRelocation final
{
    std::uint32_t offset = 0;
    std::uint32_t symbolIndex = 0;
    std::uint16_t type = 0;
};
}
