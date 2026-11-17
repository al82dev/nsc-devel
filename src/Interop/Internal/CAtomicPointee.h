#pragma once

#include "nativesharp/Interop/CppSemanticInventory.h"

#include <memory>
#include <optional>

namespace nativesharp::interop
{
// EN: T19-002. A C pointer to `_Atomic T` with T a scalar, a pointer, an enum or a record: the same pointer to T,
//     whose pointee the caller marks atomic (`cppPointerPointeeAtomic`), so every access through it stays atomic.
//     The qualifiers of the atomic type pass to T. No value for any other type.
//     T19-038: shared by functions, globals and record fields.
// ES: T19-002. Un puntero C a `_Atomic T` con T escalar, puntero, enum o record: el mismo puntero a T, cuyo
//     apuntado marca atomico quien llama (`cppPointerPointeeAtomic`), asi que todo acceso por el sigue siendo
//     atomico. Los calificadores del tipo atomico pasan a T. Sin valor para cualquier otro tipo.
//     T19-038: lo comparten funciones, globales y campos de record.
inline std::optional<CppType> cAtomicPointeeCarrier(const CppType& type)
{
    if (type.kind != CppTypeKind::Pointer || !type.element || type.element->kind != CppTypeKind::Atomic ||
        !type.element->element) return std::nullopt;
    const auto kind = type.element->element->kind;
    if (kind != CppTypeKind::Boolean && kind != CppTypeKind::SignedInteger && kind != CppTypeKind::UnsignedInteger &&
        kind != CppTypeKind::Floating && kind != CppTypeKind::Pointer && kind != CppTypeKind::Enumeration &&
        kind != CppTypeKind::Record)
        return std::nullopt;
    CppType mapped = type;
    auto element = std::make_shared<CppType>(*type.element->element);
    element->isConst = element->isConst || type.element->isConst;
    element->isVolatile = element->isVolatile || type.element->isVolatile;
    mapped.element = std::move(element);
    return mapped;
}
}
