#pragma once

#include "nativesharp/Interop/LibraryMetadataMember.h"

#include <string>
#include <vector>

namespace nativesharp::interop
{
// NSMETA 1.40 (NS304-T08-F05). Una conformance `impl Trait for Type` cuyo target es un tipo
// externo (el modulo posee el trait, no el tipo). El consumidor reconstruye el impl para que el
// dispatch estatico sobre el tipo externo resuelva los metodos sin las fuentes del productor.
struct LibraryMetadataTraitImpl final
{
    std::string targetTypeKey;
    std::string traitTypeKey;
    std::vector<LibraryMetadataMember> methods;
};
}
