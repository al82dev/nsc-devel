#pragma once

#include "nativesharp/Frontend/ClassMemberAccessibility.h"
#include "nativesharp/Frontend/ParameterPassKind.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// Experimental declaration for isolated member indexing; this is not a call ABI contract.
struct CppImportedInstanceMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string externalSymbol;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
    bool receiverConst;
    bool dllImport = false;
    bool returnPointeeReadonly = false;
    std::vector<bool> parameterPointeeReadonly;
    std::vector<std::uint8_t> objectSizeModes;
    bool sharedExternalSymbolVerified = false;
    // T11-005: `T&` is a ref and `const T&` an in parameter; a `T&` result is a ref return bound to this.
    std::vector<ParameterPassKind> parameterPassKinds{};
    bool returnsReference = false;
    // T19-007: the reference result is `const T&`. / El resultado por referencia es `const T&`.
    bool returnsReadonlyReference = false;
    bool constructedResult = false; // X2-05: a non-trivial record by value. / Un record no trivial por valor.
    // EN: T19-012. C++ access of the method; NativeSharp checks it at each call.
    // ES: T19-012. Acceso C++ del metodo; NativeSharp lo comprueba en cada llamada.
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
    // T19-007: which parameters are C++ `T&&` (ref parameters that also bind a value). / Cuales son `T&&`.
    std::vector<bool> parameterRvalueReferences = {};
    bool isDeleted = false; // X1-03: a `= delete` method. / Un metodo `= delete`.
    // X1-05: an `&&` receiver: called on a temporary only. / Un receptor `&&`: solo sobre un temporal.
    bool receiverRvalue = false;
};

enum class CppImportedLifetimeKind { Constructor, Destructor };
struct CppImportedLifetimeMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    CppImportedLifetimeKind kind;
    std::string externalSymbol;
    std::vector<std::string> parameterTypeKeys;
    bool dllImport = false;
    bool isNonThrowing = false;
    bool isImplicit = false;
    bool isTrivial = false;
    std::vector<bool> parameterPointeeReadonly;
    // T11-006: `T&` is a ref and `const T&` an in parameter; T11-007: the declared access.
    std::vector<ParameterPassKind> parameterPassKinds = {};
    ClassMemberAccessibility accessibility = ClassMemberAccessibility::Public;
    // T19-006: as for CppImportedInstanceMethod. / Como en CppImportedInstanceMethod.
    std::vector<std::string> cppParameterSpellings = {};
    std::string cppSignatureSuffix = {};
    std::string cppDeclarationSpelling = {};
    // T19-007: which parameters are C++ `T&&` (ref parameters that also bind a value). / Cuales son `T&&`.
    std::vector<bool> parameterRvalueReferences = {};
    // X2-05: a factory function that constructs the owner in place (`T f(...)`). / Una funcion fabrica.
    bool factory = false;
    // X2-05: a hidden move constructor, only for C++'s implicit move. / Solo para el movimiento implicito.
    bool implicitMoveOnly = false;
};
}
