#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend::method_body_detail
{
// EN: T08-D11/D12. NativeSharp keeps its own integer rules in every expression, also when an operand is read from
//     C. These helpers only recognize the operations whose NativeSharp result differs from the C one, so the
//     body analysis can report it with the explicit correction; they never change a type or a value.
// ES: T08-D11/D12. NativeSharp conserva sus reglas enteras en toda expresion, tambien cuando un operando se lee de
//     C. Estas funciones solo reconocen las operaciones cuyo resultado NativeSharp difiere del de C, para que el
//     analisis de cuerpos lo informe con la correccion explicita; nunca cambian un tipo ni un valor.

// EN: Source spelling of a builtin type key (`builtin:byte` -> `byte`).
// ES: Escritura en fuente de una clave de tipo predefinido (`builtin:byte` -> `byte`).
inline std::string cDivergenceTypeName(std::string_view typeKey)
{
    constexpr std::string_view prefix = "builtin:";
    if (typeKey.starts_with(prefix)) typeKey.remove_prefix(prefix.size());
    return std::string(typeKey);
}

// EN: A narrow integer that C promotes to `int` before `-` and `~`, while NativeSharp keeps its type.
// ES: Un entero estrecho que C promueve a `int` antes de `-` y `~`, mientras NativeSharp conserva su tipo.
inline bool cPromotedNarrowInteger(std::string_view typeKey)
{
    return typeKey == "builtin:byte" || typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
        typeKey == "builtin:ushort";
}

// EN: Message for `-` or `~` on a narrow C operand.
// ES: Mensaje para `-` o `~` sobre un operando C estrecho.
inline std::string cUnaryDivergenceMessage(char operation, std::string_view typeKey)
{
    const std::string type = cDivergenceTypeName(typeKey);
    return std::string("unary '") + operation + "' on a C value of type '" + type +
        "' keeps the NativeSharp type '" + type + "'; C promotes the operand to 'int', so the result can differ "
        "from C: write '" + operation + "(int)' before the operand to get the C result";
}

// EN: `int` with `unsigned int` (either order) that NativeSharp widens to `long`; C converts the `int` operand to
//     `unsigned int`. A constant that NativeSharp already adapts to `uint` gives no `long` and no divergence.
// ES: `int` con `unsigned int` (en cualquier orden) que NativeSharp amplia a `long`; C convierte el operando `int`
//     a `unsigned int`. Una constante que NativeSharp ya adapta a `uint` no da `long` ni divergencia.
inline bool cIntUintWidensToLong(
    std::string_view left, std::string_view right, const std::optional<std::string>& common)
{
    const bool mixed = (left == "builtin:int" && right == "builtin:uint") ||
        (left == "builtin:uint" && right == "builtin:int");
    return mixed && common.has_value() && *common == "builtin:long";
}

// EN: Message for an `int`/`unsigned int` operation with a C operand.
// ES: Mensaje para una operacion `int`/`unsigned int` con un operando C.
inline std::string cIntUintDivergenceMessage(bool comparison, bool leftIsInt)
{
    return std::string(comparison ? "comparison" : "operation") +
        " of 'int' with 'uint' and a C operand follows NativeSharp: both operands widen to 'long'; C converts the "
        "'int' operand to 'unsigned int', so the result can differ from C: write '(uint)' before the " +
        (leftIsInt ? "left" : "right") + " operand to get the C result";
}
}
