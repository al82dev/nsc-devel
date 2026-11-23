#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::interop
{
// EN: T12-004. A member reached through inheritance or virtual dispatch is called through an adapter that
//     Clang generates: an extern "C" function with the physical ABI of an instance method (the object first,
//     a class result in memory after it) whose body is the C++ call on the derived type. Clang then applies
//     the this adjustment, the vtable of the Microsoft ABI and virtual bases. The adapter is named after the
//     Clang identities of the record and the method, so every session that emits it emits the same symbol.
// ES: T12-004. Un miembro alcanzado por herencia o por despacho virtual se llama mediante un adaptador que
//     genera Clang: una funcion extern "C" con el ABI fisico de un metodo de instancia (el objeto primero, un
//     resultado de clase en memoria despues) cuyo cuerpo es la llamada C++ sobre el tipo derivado. Asi Clang
//     aplica el ajuste de this, la vtable del ABI de Microsoft y las bases virtuales. El adaptador se nombra
//     con las identidades de Clang del record y del metodo, asi que toda sesion que lo emite emite el mismo
//     simbolo.
inline constexpr std::string_view cppMemberThunkPrefix = "__nsc_cpp_call_";

inline std::string cppMemberThunkSymbol(std::string_view ownerUsr, std::string_view methodUsr)
{
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&](std::string_view text) {
        for (const unsigned char c : text) { hash ^= c; hash *= 1099511628211ULL; }
    };
    mix(ownerUsr);
    mix("#");
    mix(methodUsr);
    static constexpr char digits[] = "0123456789abcdef";
    std::string symbol(cppMemberThunkPrefix);
    for (int shift = 60; shift >= 0; shift -= 4) symbol.push_back(digits[(hash >> shift) & 0xF]);
    return symbol;
}

// EN: T12-004. The adapter of a member operator called as a free function: its first parameter is the object.
// ES: T12-004. El adaptador de un operador miembro llamado como funcion libre: su primer parametro es el objeto.
inline std::string cppMemberOperatorSymbol(std::string_view ownerUsr, std::string_view methodUsr)
{
    return cppMemberThunkSymbol(std::string(ownerUsr) + "#operator", methodUsr);
}

// EN: T19-006. The adapter that calls a C++ function or method with its first `arity` arguments, so Clang supplies
//     the remaining default arguments at each call, whatever they are (an enumerator, a record, a global, a
//     call). A free function has the owner `#function`.
// ES: T19-006. El adaptador que llama a una funcion o metodo C++ con sus primeros `arity` argumentos, para que Clang
//     ponga en cada llamada los argumentos predeterminados restantes, sean cuales sean (un enumerador, un record,
//     una global, una llamada). Una funcion libre tiene el propietario `#function`.
inline std::string cppDefaultArgumentThunkSymbol(std::string_view ownerUsr, std::string_view usr, std::size_t arity)
{
    return cppMemberThunkSymbol(std::string(ownerUsr) + "#default", std::string(usr) + "#" + std::to_string(arity));
}

// EN: X2-05. The adapter that takes a C++ function's non-trivial record parameters by `const T&` and calls it with
//     them, so Clang copy-constructs each by-value argument and the callee destroys it, as the Microsoft ABI requires.
// ES: X2-05. El adaptador que toma por `const T&` los parametros record no triviales de una funcion C++ y la llama con
//     ellos, asi Clang construye por copia cada argumento por valor y el llamado lo destruye, como exige el ABI de
//     Microsoft.
inline std::string cppByValueThunkSymbol(std::string_view usr)
{
    return cppMemberThunkSymbol("#function#byvalue", usr);
}

// T19-006: the identity of that adapter as a declaration. / La identidad de ese adaptador como declaracion.
inline std::string cppDefaultArgumentThunkUsr(std::string_view usr, std::size_t arity)
{
    return std::string(usr) + "@nsc-default:" + std::to_string(arity);
}
}
