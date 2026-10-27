#pragma once

#include "CoffRelocation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
// ES: Una seccion de un objeto COFF: su nombre, donde estan sus bytes en el archivo y sus relocaciones,
// ordenadas por desplazamiento.
// EN: One section of a COFF object: its name, where its bytes are in the file and its relocations, sorted
// by offset.
struct CoffSection final
{
    std::string name;
    std::uint32_t dataOffset = 0;
    std::uint32_t dataSize = 0;
    std::vector<CoffRelocation> relocations;
};
}
