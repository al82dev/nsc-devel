#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
enum class ProjectConversionKind
{
    None,
    Identity,
    Never,
    IntegralPromotion,
    ConstantInteger,
    Numeric,
    Tuple,
    Function,
    NullPointer,
    NullReference,
    ReferenceUpcast,
    ReferenceInterface,
    PointerToVoid,
    StringLiteralToCharPointer,
    // L-003: a runtime `string` passed to a C `const char*` parameter of a direct call: a NUL-terminated copy on
    // the stack that lives for the call. / Un `string` en tiempo de ejecucion pasado a un `const char*` C.
    StringToCharPointer,
    StringLiteralToNativeString,
    // NS304-T11-F01: a mutable view (Span<T>) becomes an immutable view (ReadOnlySpan<T>). Identity
    // element, one-way (immutable never gains mutability). Structural (pointer const + length).
    View,
    UserDefinedImplicit,
    UserDefinedExplicit,
    CFlexibleArrayToPointer,
    // T12-003: a pointer to a C++ record becomes a pointer to an unambiguous public non-virtual base, adjusted by
    // Clang's base offset; a null pointer stays null.
    // T12-003: un puntero a un record C++ pasa a puntero a una base publica no virtual no ambigua, ajustado con
    // el offset de la base de Clang; un puntero nulo sigue nulo.
    CppPointerUpcast,
    // EN: T19-039. A real value or a `_Complex` carrier becomes another `_Complex` carrier through the C conversion
    //     Clang compiles (`namespaceFunctionOrdinal`), as C converts it.
    // ES: T19-039. Un valor real o un portador `_Complex` pasa a otro portador `_Complex` con la conversion C que
    //     compila Clang (`namespaceFunctionOrdinal`), como lo convierte C.
    CComplexValue,
    // Arithmetic conversion of the direct result of an imported call.
    ForeignNumeric
};
}
