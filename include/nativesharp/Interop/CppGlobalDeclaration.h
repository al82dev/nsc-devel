#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <optional>
#include <cstdint>
#include <string>

namespace nativesharp::interop
{
// EN: T13-022. The value Clang's constant evaluator gives a constant C++ variable of scalar type.
// ES: T13-022. El valor que da el evaluador de constantes de Clang a una variable C++ constante escalar.
struct CppConstantValue
{
    enum class Kind { Integer, Floating, Boolean };
    Kind kind = Kind::Integer;
    // Two's complement bits, sign-extended for a signed type. / Bits en complemento a dos, con signo extendido.
    std::uint64_t integerBits = 0;
    double floating = 0.0;
    bool boolean = false;
};

struct CppGlobalDeclaration
{
    std::string name;
    std::string qualifiedName;
    CppType type;
    bool externalLinkage = false;
    bool threadLocal = false;
    // A C++ thread_local that Clang reaches through __tls_guard and __dyn_tls_on_demand_init, because its
    // declaration is neither constinit nor a visible constant-initialized definition.
    bool cppTlsOnDemand = false;
    bool dllImport = false;
    // A name supplied by Clang does not prove that a linked library defines it.
    std::optional<std::string> externalSymbol;
    std::optional<CppSourcePoint> location;
    // Effective alignment of the declaration in bits, absent for incomplete types.
    std::optional<std::uint64_t> declarationAlignmentBits;
    std::optional<std::string> clangUsr;
    // T13-022: present for a constant C++ variable with internal linkage, which has no symbol.
    // T13-022: presente para una variable C++ constante con enlace interno, que no tiene simbolo.
    std::optional<CppConstantValue> constantValue;
    // T14-002: an object-like macro of the header whose expansion Clang evaluates to a constant.
    // T14-002: una macro de objeto del encabezado cuya expansion Clang evalua a una constante.
    bool macro = false;
};
}
