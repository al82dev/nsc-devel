#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace nativesharp::interop
{
// EN: R1 (item 8.0, decision D6). A boundary contract that a project declares next to its C/C++ reference for what
//     the header does not say: a parameter's lifetime or nullability, or an owned result and its release function.
//     NativeSharp never infers borrowing or transfer from a `T*` type; without a contract nothing is added.
// ES: R1 (item 8.0, decision D6). Un contrato de frontera que un proyecto declara junto a su referencia C/C++ para lo
//     que el encabezado no dice: la vida o la nulidad de un parametro, o un resultado con dueno y su funcion de
//     liberacion. NativeSharp nunca deduce prestamo ni transferencia de un tipo `T*`; sin contrato no se agrega nada.
enum class CppContractKind
{
    Borrowed,
    Retained,
    Transfer,
    Out,
    NonNull,
    Nullable,
    OwnedResult
};

struct CppContract
{
    std::string function;
    std::optional<std::size_t> parameter;
    CppContractKind kind = CppContractKind::Borrowed;
    // `Static`, `Receiver` or `Call:<function>` for Retained. / Para Retained.
    std::string until;
    bool optional = false;
    // The release function of an owned result. / La funcion de liberacion de un resultado con dueno.
    std::string release;
};
}
