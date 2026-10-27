#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
// ES: Un simbolo de la tabla COFF. `present` es falso para las entradas auxiliares, que ocupan un indice pero
// no son simbolos. `sectionNumber` empieza en 1; 0 o negativo significa externo, absoluto o de depuracion.
// EN: One COFF symbol table entry. `present` is false for auxiliary entries, which take an index but are
// not symbols. `sectionNumber` is 1-based; 0 or negative means external, absolute or debug.
struct CoffSymbol final
{
    std::uint32_t value = 0;
    std::int32_t sectionNumber = 0;
    bool present = false;
};
}
