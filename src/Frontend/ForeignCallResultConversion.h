#pragma once
#include "nativesharp/Frontend/ProjectImplicitConversion.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Frontend/CppImportedEnumNumeric.h"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
inline ProjectImplicitConversion classifyForeignCallNumericResult(
    const ProjectNameLookup* lookup, std::string_view source, std::string_view target)
{
    static constexpr std::array<std::string_view, 16> arithmetic = {
        "builtin:sbyte", "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
        "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
        "builtin:nint", "builtin:nuint", "builtin:i128", "builtin:u128",
        "builtin:float", "builtin:double", "builtin:bool"};
    const auto numeric = [&](std::string_view value) {
        return std::find(arithmetic.begin(), arithmetic.end(), value) != arithmetic.end();
    };
    // Scoped C++ enums do not implicitly convert to arithmetic types.
    const auto enumBase = cppImportedCEnumUnderlying(lookup, source);
    const bool sourceNumeric = numeric(source) || (enumBase.has_value() && numeric(*enumBase));
    const bool pointerToBool = (source.ends_with('*') || ProjectTypeConversion::isFunctionPointer(source)) &&
        target == "builtin:bool";
    return {(sourceNumeric && numeric(target)) || pointerToBool ? ProjectConversionKind::ForeignNumeric
                                                                 : ProjectConversionKind::None,
            5, std::string(source), std::string(target), std::nullopt};
}
}
