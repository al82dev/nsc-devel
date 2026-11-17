#pragma once

#include "nativesharp/Frontend/ClassMemberAccessibility.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// Experimental static C++ method declaration; no invocation support is implied.
struct CppImportedStaticMethod final
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string externalSymbol;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
    bool dllImport = false;
    bool returnPointeeReadonly = false;
    std::vector<bool> parameterPointeeReadonly;
    std::vector<std::uint8_t> objectSizeModes;
    bool sharedExternalSymbolVerified = false;
    // C++ access of the method; NativeSharp checks it at each use.
    ClassMemberAccessibility accessibility = ClassMemberAccessibility::Public;
    // EN: T19-006. The C++ spelling of each parameter, which orders overloads as Clang when their NativeSharp types
    //     are equal (`int` and `long`), and a suffix that keeps such overloads, and default-argument adapters,
    //     distinct in the member signature.
    // ES: T19-006. La escritura C++ de cada parametro, que ordena las sobrecargas como Clang cuando sus tipos
    //     NativeSharp son iguales (`int` y `long`), y un sufijo que mantiene distintas en la firma esas sobrecargas
    //     y los adaptadores de argumentos predeterminados.
    std::vector<std::string> cppParameterSpellings;
    std::string cppSignatureSuffix;
    // T19-006: the C++ declaration, for the alternatives of a failed call. / Para las alternativas de una llamada.
    std::string cppDeclarationSpelling;
};
}
