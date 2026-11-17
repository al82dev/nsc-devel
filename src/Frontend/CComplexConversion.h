#pragma once

#include "nativesharp/Frontend/ProjectConversionKind.h"
#include "nativesharp/Frontend/ProjectImplicitConversion.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// EN: T19-039. The element type key of a C `_Complex` carrier (`struct __complex_T { T real; T imag; }`), or none.
// ES: T19-039. La clave del tipo elemento de un portador C `_Complex` (`struct __complex_T { T real; T imag; }`), o
//     ninguna.
inline std::optional<std::string> cComplexCarrierElement(
    const ProjectInheritanceModel& inheritance, const ProjectMemberModel& members, std::string_view typeKey)
{
    if (!typeKey.starts_with("type:") || typeKey.find("__complex_") == std::string_view::npos) return std::nullopt;
    const auto ordinal = inheritance.typeOrdinalForTypeKey(typeKey);
    if (!ordinal) return std::nullopt;
    std::optional<std::string> real, imag;
    for (const auto member : members.membersOfType(*ordinal))
    {
        if (member >= members.members().size()) continue;
        const auto& field = members.members()[member];
        if (field.kind != ClassMemberKind::Field || !field.isCppExternal) continue;
        if (field.name == "real") real = field.declaredTypeKey;
        else if (field.name == "imag") imag = field.declaredTypeKey;
    }
    if (!real || real != imag) return std::nullopt;
    return real;
}

// EN: T19-039. An implicit conversion of C to a `_Complex` carrier: from a real value, or from another carrier, as C
//     converts it (the real part, and zero or the imaginary part). NativeSharp admits it when it admits implicitly the
//     conversion of the element types, as for real values; the value goes through the C function Clang compiles.
// ES: T19-039. Una conversion implicita de C a un portador `_Complex`: desde un valor real, o desde otro portador,
//     como la hace C (la parte real, y cero o la parte imaginaria). NativeSharp la admite cuando admite
//     implicitamente la conversion de los tipos elemento, como con los valores reales; el valor pasa por la funcion
//     C que compila Clang.
inline std::optional<ProjectImplicitConversion> classifyCComplexConversion(
    const ProjectInheritanceModel* inheritance, const ProjectMemberModel* members,
    const ProjectNamespaceFunctionModel* functions, std::string_view sourceTypeKey, std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant)
{
    if (!inheritance || !members || !functions || sourceTypeKey == targetTypeKey) return std::nullopt;
    const auto targetElement = cComplexCarrierElement(*inheritance, *members, targetTypeKey);
    if (!targetElement) return std::nullopt;
    const auto sourceElement = cComplexCarrierElement(*inheritance, *members, sourceTypeKey);
    const std::string element = sourceElement ? *sourceElement : std::string(sourceTypeKey);
    if (!ProjectTypeConversion::isNumeric(element) && element != "builtin:bool") return std::nullopt;
    if (element != *targetElement &&
        !ProjectTypeConversion::classifyImplicit(element, *targetElement,
                                                 sourceElement ? std::nullopt : integerConstant, false).exists())
        return std::nullopt;
    const auto& all = functions->functions();
    for (std::size_t ordinal = 0; ordinal < all.size(); ++ordinal)
    {
        const auto& function = all[ordinal];
        if (function.name != "__nsc_complex_cv" || function.parameters.size() != 1 ||
            function.declaredTypeKey != targetTypeKey) continue;
        const auto& parameter = function.parameters.front().typeKey;
        // EN: A real source binds the conversion from its own type, or from `int` for a constant, as C promotes it.
        // ES: Un origen real enlaza la conversion desde su propio tipo, o desde `int` para una constante, como la
        //     promueve C.
        if (parameter != sourceTypeKey) continue;
        ProjectImplicitConversion conversion{ProjectConversionKind::CComplexValue, 20, std::string(sourceTypeKey),
                                             std::string(targetTypeKey), std::nullopt};
        conversion.namespaceFunctionOrdinal = ordinal;
        return conversion;
    }
    return std::nullopt;
}
}
