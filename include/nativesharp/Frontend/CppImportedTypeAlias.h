#pragma once

#include "nativesharp/Interop/CppTypeAliasDeclaration.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
// Owns the Clang snapshot for the lifetime of a compilation. A name can have
// multiple observations; consumers must resolve visibility and compatibility.
struct CppImportedTypeAlias
{
    std::size_t projectOrdinal;
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::string targetTriple;
    interop::CppTypeAliasDeclaration declaration;
    // Pairs established from complete precompiled C record closures in this compilation.
    std::vector<std::pair<std::size_t, std::size_t>> equivalentRecordFunctionAliases;
    // EN: T08-D10: restored from a precompiled library's C dependency catalogue (internal, not NSMETA). A template
    //     transported from that producer resolves its C typedefs in this catalogue.
    // ES: T08-D10: restaurado del catalogo de dependencias C de una biblioteca precompilada (interno, no NSMETA).
    //     Una plantilla transportada de ese productor resuelve sus typedef C en este catalogo.
    bool fromPrecompiledCatalog = false;
};
}
