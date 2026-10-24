#pragma once

#include <cstdint>

namespace nativesharp::frontend
{
// EN: Tag written before every reference of a semantic snapshot, so a reader that expects one
//     kind of reference rejects any other instead of misreading it.
// ES: Marca escrita antes de cada referencia de un snapshot semántico, para que un lector que
//     espera una clase de referencia rechace cualquier otra en lugar de leerla mal.
enum class PrecompiledSnapshotReferenceKind : std::uint8_t
{
    Null = 0,
    SyntaxNode = 1,
    TypeSyntax = 2,
    TypeKey = 3,
    MemberSignature = 4,
    NamespaceSymbol = 5,
    Specialization = 6,
    UnitOrdinal = 7
};
}
