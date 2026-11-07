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

inline bool cppCEnumMixedUintInt(const std::optional<std::string>& leftEnum,
                                 std::string_view leftType,
                                 const std::optional<std::string>& rightEnum,
                                 std::string_view rightType)
{
    return (leftEnum == "builtin:uint" && rightType == "builtin:int") ||
           (rightEnum == "builtin:uint" && leftType == "builtin:int");
}
}
