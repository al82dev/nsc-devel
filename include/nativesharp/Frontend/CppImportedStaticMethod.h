#pragma once

#include "nativesharp/Frontend/ClassMemberAccessibility.h"
#include "nativesharp/Frontend/ParameterPassKind.h"

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
    std::vector<std::string> cppParameterSpellings{};
    std::string cppSignatureSuffix{};
    // T19-006: the C++ declaration, for the alternatives of a failed call. / Para las alternativas de una llamada.
    std::string cppDeclarationSpelling{};
    // EN: T19-007. `T&` is a ref and `const T&` an in parameter; a reference result is a ref return, bound to its
    //     only ref or in parameter or else to static storage (T10-003).
    // ES: T19-007. `T&` es un parametro ref y `const T&` uno in; un resultado por referencia es un retorno ref,
    //     ligado a su unico parametro ref o in o, si no, a almacenamiento estatico (T10-003).
    std::vector<ParameterPassKind> parameterPassKinds = {};
    bool returnsReference = false;
    bool returnsReadonlyReference = false;
    bool constructedResult = false; // X2-05: a non-trivial record by value. / Un record no trivial por valor.
    // T19-007: which parameters are C++ `T&&` (ref parameters that also bind a value). / Cuales son `T&&`.
    std::vector<bool> parameterRvalueReferences = {};
};
}
