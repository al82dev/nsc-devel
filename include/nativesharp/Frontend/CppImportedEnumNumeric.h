#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"
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
