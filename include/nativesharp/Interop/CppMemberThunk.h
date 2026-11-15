#pragma once

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
}
