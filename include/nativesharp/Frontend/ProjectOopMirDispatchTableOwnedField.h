#pragma once

#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"

namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable::OwnedField final
{
    std::size_t memberOrdinal = 0;
    std::uint64_t offset = 0;
    // A native string owns a flat allocation released directly; a class instance is released
    // through its runtime class descriptor's destruction entry.
    bool isNativeString = false;
    // EN: Freeze 3.0 (owned arrays): an owned native array is released with its elements; its
    //     array type key says what the elements are.
    // ES: Congelamiento 3.0 (arreglos owned): un arreglo nativo owned se libera con sus elementos;
    //     su type key de arreglo dice que son los elementos.
    bool isNativeArray = false;
    std::string arrayTypeKey;
};
}
