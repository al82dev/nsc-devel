#pragma once

#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// Only imported C enums with a validated representation participate here.
inline std::optional<std::string> cppImportedCEnumUnderlying(
    const ProjectNameLookup* lookup, std::string_view typeKey)
{
    if (!lookup || !typeKey.starts_with("type:")) return std::nullopt;
    const auto ordinal = lookup->typeOrdinalForIdentity(typeKey.substr(5));
    const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
    if (!imported || imported->origin != CppImportedTypeOrigin::CEnum || !imported->enumBase ||
        !imported->objectLayout || imported->isIncomplete || !imported->deferredImportError.empty())
        return std::nullopt;
    return "builtin:" + imported->enumBase->name;
}

// EN: T09-003. Clang passes a C enum by value as its underlying integer, so the gates of the non-cdecl
//     C conventions treat a complete imported C enum whose layout matches one of the eight fixed-width
//     integers like that builtin key. Callers holding Clang's type still reconcile the enum identity.
// ES: T09-003. Clang pasa un enum C por valor como su entero subyacente; las compuertas de las
//     convenciones C no cdecl tratan un enum C importado y completo cuyo layout coincide con uno de los
//     ocho enteros de ancho fijo como esa clave incorporada. Quien tenga el tipo de Clang concilia la identidad.
inline std::optional<std::string> cppCEnumAbiScalarKey(const ProjectNameLookup* lookup, std::string_view typeKey)
{
    if (typeKey.ends_with('*')) return std::nullopt;
    auto underlying = cppImportedCEnumUnderlying(lookup, typeKey);
    if (!underlying) return std::nullopt;
    static constexpr std::string_view allowed[] = {"builtin:sbyte", "builtin:byte", "builtin:short", "builtin:ushort",
                                                   "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong"};
    const auto* info = builtins::findIntegerType(*underlying);
    if (!info || std::find(std::begin(allowed), std::end(allowed), info->key) == std::end(allowed)) return std::nullopt;
    const auto* imported = lookup->cppImportedType(*lookup->typeOrdinalForIdentity(typeKey.substr(5)));
    if (imported->objectLayout->sizeBytes * 8 != info->width ||
        imported->objectLayout->alignmentBytes * 8 != info->width) return std::nullopt;
    return underlying;
}

// A C enum of 32-bit base mixed with the other 32-bit integer of opposite signedness follows C's usual
// arithmetic conversions: both operands become `unsigned int`.
// Un enum C de base de 32 bits mezclado con el otro entero de 32 bits de signo opuesto sigue las
// conversiones aritméticas usuales de C: ambos operandos pasan a `unsigned int`.
inline bool cppCEnumMixedUintInt(const std::optional<std::string>& leftEnum,
                                 std::string_view leftType,
                                 const std::optional<std::string>& rightEnum,
                                 std::string_view rightType)
{
    return (leftEnum == "builtin:uint" && rightType == "builtin:int") ||
           (rightEnum == "builtin:uint" && leftType == "builtin:int") ||
           (leftEnum == "builtin:int" && rightType == "builtin:uint") ||
           (rightEnum == "builtin:int" && leftType == "builtin:uint");
}
}
