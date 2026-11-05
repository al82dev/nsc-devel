#pragma once

#include "nativesharp/Interop/CppTypeAliasDeclaration.h"

#include <cstddef>
#include <filesystem>
#include <string>

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
};
}
