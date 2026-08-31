#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <array>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using Pair = std::pair<std::string_view, std::string_view>;

constexpr std::array<std::string_view, 12> kNumericTypes = {
    "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
    "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    "builtin:nint", "builtin:nuint", "builtin:float", "builtin:double"};

constexpr std::array<std::string_view, 10> kIntegralTypes = {
    "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
    "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    "builtin:nint", "builtin:nuint"};

constexpr std::array<Pair, 53> kNumericWidenings = {{
    {"builtin:byte", "builtin:short"}, {"builtin:byte", "builtin:ushort"},
    {"builtin:byte", "builtin:int"}, {"builtin:byte", "builtin:uint"},
    {"builtin:byte", "builtin:long"}, {"builtin:byte", "builtin:ulong"},
    {"builtin:byte", "builtin:nint"}, {"builtin:byte", "builtin:nuint"},
    {"builtin:byte", "builtin:float"}, {"builtin:byte", "builtin:double"},
    {"builtin:char", "builtin:ushort"}, {"builtin:char", "builtin:int"},
    {"builtin:char", "builtin:uint"}, {"builtin:char", "builtin:long"},
    {"builtin:char", "builtin:ulong"}, {"builtin:char", "builtin:nint"},
    {"builtin:char", "builtin:nuint"}, {"builtin:char", "builtin:float"},
    {"builtin:char", "builtin:double"},
    {"builtin:short", "builtin:int"}, {"builtin:short", "builtin:long"},
    {"builtin:short", "builtin:nint"}, {"builtin:short", "builtin:float"},
    {"builtin:short", "builtin:double"},
    {"builtin:ushort", "builtin:int"}, {"builtin:ushort", "builtin:uint"},
    {"builtin:ushort", "builtin:long"}, {"builtin:ushort", "builtin:ulong"},
    {"builtin:ushort", "builtin:nint"}, {"builtin:ushort", "builtin:nuint"},
    {"builtin:ushort", "builtin:float"}, {"builtin:ushort", "builtin:double"},
    {"builtin:int", "builtin:long"}, {"builtin:int", "builtin:nint"},
    {"builtin:int", "builtin:float"}, {"builtin:int", "builtin:double"},
    {"builtin:uint", "builtin:long"}, {"builtin:uint", "builtin:ulong"},
    {"builtin:uint", "builtin:nuint"}, {"builtin:uint", "builtin:float"},
    {"builtin:uint", "builtin:double"},
    {"builtin:long", "builtin:float"}, {"builtin:long", "builtin:double"},
    {"builtin:ulong", "builtin:float"}, {"builtin:ulong", "builtin:double"},
    {"builtin:nint", "builtin:long"}, {"builtin:nint", "builtin:float"},
    {"builtin:nint", "builtin:double"},
    {"builtin:nuint", "builtin:ulong"}, {"builtin:nuint", "builtin:float"},
    {"builtin:nuint", "builtin:double"},
    {"builtin:float", "builtin:double"}
}};

bool contains(std::span<const std::string_view> values, std::string_view value) noexcept
{
    for (const std::string_view candidate : values)
    {
        if (candidate == value)
        {
            return true;
        }
    }
    return false;
}

bool isIntegralPromotion(std::string_view source, std::string_view target) noexcept
{
    return target == "builtin:int" &&
        (source == "builtin:byte" || source == "builtin:char" ||
         source == "builtin:short" || source == "builtin:ushort");
}

bool isNumericWidening(std::string_view source, std::string_view target) noexcept
{
    for (const Pair& pair : kNumericWidenings)
    {
        if (pair.first == source && pair.second == target)
        {
            return true;
        }
    }
    return false;
}

bool constantFits(std::uint64_t value, bool sourceUnsigned, std::string_view target) noexcept
{
    if (target == "builtin:byte")
    {
        return value <= std::numeric_limits<std::uint8_t>::max();
    }
    // CC1-100 "char es unsigned de 8 bits". `builtin:char` estaba agrupado aqui con
    // `builtin:ushort` y aceptaba hasta 65535, mientras TODAS las demas capas del compilador lo
    // tratan como un byte: `llvmType` lo mapea a i8, la ABI lo pasa como i8, el parser lee un
    // literal de caracter de un byte y el modelo de layout le da tamano 1.
    //
    // La diferencia no era cosmetica. Un `switch` sobre un `char` aceptaba `case 65:` y `case 321:`
    // como etiquetas distintas -- 321 <= 65535, asi que la conversion constante decia que si -- y
    // el backend materializaba las dos como `i8 65`, entregandole a LLVM dos valores de caso
    // identicos en un mismo switch, que es comportamiento indefinido. El chequeo de duplicados
    // existe precisamente para impedir eso, y esta linea era el agujero debajo.
    //
    // El `char` de NativeSharp mide un byte hoy. Cuando pase a UTF-16, esta linea se mueve CON el
    // layout, no antes.
    if (target == "builtin:char")
    {
        return value <= std::numeric_limits<std::uint8_t>::max();
    }
    if (target == "builtin:ushort")
    {
        return value <= std::numeric_limits<std::uint16_t>::max();
    }
    if (target == "builtin:short")
    {
        return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int16_t>::max());
    }
    if (target == "builtin:int")
    {
        return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    }
    if (target == "builtin:uint")
    {
        return value <= std::numeric_limits<std::uint32_t>::max();
    }
    if (target == "builtin:long" || target == "builtin:nint")
    {
        return sourceUnsigned
            ? value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            : value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    }
    if (target == "builtin:ulong" || target == "builtin:nuint")
    {
        return true;
    }
    return false;
}

ProjectImplicitConversion makeConversion(
    ProjectConversionKind kind,
    int rank,
    std::string_view source,
    std::string_view target)
{
    return ProjectImplicitConversion{kind, rank, std::string(source), std::string(target), std::nullopt};
}
}

bool ProjectImplicitConversion::exists() const noexcept
{
    return kind != ProjectConversionKind::None;
}

ProjectImplicitConversion ProjectTypeConversion::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant,
    bool targetIsClassReference)
{
    if (sourceTypeKey.empty() || targetTypeKey.empty() ||
        sourceTypeKey == "<error>" || targetTypeKey == "<error>")
    {
        return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == targetTypeKey)
    {
        return makeConversion(ProjectConversionKind::Identity, 0, sourceTypeKey, targetTypeKey);
    }

    if (isIntegralPromotion(sourceTypeKey, targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::IntegralPromotion, 1, sourceTypeKey, targetTypeKey);
    }

    if (isNumericWidening(sourceTypeKey, targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::Numeric, 3, sourceTypeKey, targetTypeKey);
    }

    if (integerConstant.has_value() && isIntegral(targetTypeKey) &&
        constantFits(integerConstant->value, integerConstant->isUnsigned, targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::ConstantInteger, 2, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "builtin:null" && isPointer(targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::NullPointer, 4, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "builtin:null" &&
        (targetIsClassReference || isNativeCoreReference(targetTypeKey)))
    {
        return makeConversion(ProjectConversionKind::NullReference, 4, sourceTypeKey, targetTypeKey);
    }

    if (isPointer(sourceTypeKey) && targetTypeKey == "builtin:void*")
    {
        return makeConversion(ProjectConversionKind::PointerToVoid, 4, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "string-literal" && targetTypeKey == "builtin:char*")
    {
        return makeConversion(
            ProjectConversionKind::StringLiteralToCharPointer,
            4,
            sourceTypeKey,
            targetTypeKey);
    }

    if (sourceTypeKey == "string-literal" && targetTypeKey == "builtin:string")
    {
        return makeConversion(
            ProjectConversionKind::StringLiteralToNativeString,
            4,
            sourceTypeKey,
            targetTypeKey);
    }

    return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
}

std::optional<std::string> ProjectTypeConversion::commonNumericType(
    std::string_view leftTypeKey,
    std::string_view rightTypeKey,
    std::optional<ProjectIntegerConstant> leftIntegerConstant,
    std::optional<ProjectIntegerConstant> rightIntegerConstant)
{
    if (!isNumeric(leftTypeKey) || !isNumeric(rightTypeKey) ||
        leftTypeKey == "builtin:bool" || rightTypeKey == "builtin:bool")
    {
        return std::nullopt;
    }
    if (leftTypeKey == rightTypeKey)
    {
        return std::string(leftTypeKey);
    }

    const ProjectImplicitConversion leftToRight = classifyImplicit(
        leftTypeKey, rightTypeKey, leftIntegerConstant);
    const ProjectImplicitConversion rightToLeft = classifyImplicit(
        rightTypeKey, leftTypeKey, rightIntegerConstant);
    if (leftToRight.exists() && !rightToLeft.exists())
    {
        return std::string(rightTypeKey);
    }
    if (rightToLeft.exists() && !leftToRight.exists())
    {
        return std::string(leftTypeKey);
    }

    constexpr std::array<std::string_view, 12> preference = {
        "builtin:double", "builtin:float", "builtin:ulong", "builtin:long",
        "builtin:nuint", "builtin:nint", "builtin:uint", "builtin:int",
        "builtin:ushort", "builtin:short", "builtin:char", "builtin:byte"};
    for (const std::string_view candidate : preference)
    {
        if (classifyImplicit(leftTypeKey, candidate, leftIntegerConstant).exists() &&
            classifyImplicit(rightTypeKey, candidate, rightIntegerConstant).exists())
        {
            return std::string(candidate);
        }
    }
    return std::nullopt;
}

bool ProjectTypeConversion::isNumeric(std::string_view typeKey) noexcept
{
    return contains(kNumericTypes, typeKey);
}

bool ProjectTypeConversion::isIntegral(std::string_view typeKey) noexcept
{
    return contains(kIntegralTypes, typeKey);
}

bool ProjectTypeConversion::isPointer(std::string_view typeKey) noexcept
{
    return !typeKey.empty() && typeKey.back() == '*';
}

bool ProjectTypeConversion::isNativeString(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:string";
}

bool ProjectTypeConversion::isNativeArray(std::string_view typeKey) noexcept
{
    return typeKey.size() >= 2 && typeKey.ends_with("[]");
}

bool ProjectTypeConversion::isNativeCoreReference(std::string_view typeKey) noexcept
{
    return isNativeString(typeKey) || isNativeArray(typeKey);
}

std::optional<std::string> ProjectTypeConversion::nativeArrayElementType(std::string_view typeKey)
{
    if (!isNativeArray(typeKey)) return std::nullopt;
    return std::string(typeKey.substr(0, typeKey.size() - 2));
}

const char* projectConversionKindName(ProjectConversionKind kind) noexcept
{
    switch (kind)
    {
        case ProjectConversionKind::None: return "none";
        case ProjectConversionKind::Identity: return "identity";
        case ProjectConversionKind::IntegralPromotion: return "integral-promotion";
        case ProjectConversionKind::ConstantInteger: return "constant-integer";
        case ProjectConversionKind::Numeric: return "numeric";
        case ProjectConversionKind::NullPointer: return "null-pointer";
        case ProjectConversionKind::NullReference: return "null-reference";
        case ProjectConversionKind::ReferenceUpcast: return "reference-upcast";
        case ProjectConversionKind::ReferenceInterface: return "reference-interface";
        case ProjectConversionKind::PointerToVoid: return "pointer-to-void";
        case ProjectConversionKind::StringLiteralToCharPointer: return "string-literal-to-char-pointer";
        case ProjectConversionKind::StringLiteralToNativeString: return "string-literal-to-native-string";
        case ProjectConversionKind::UserDefinedImplicit: return "user-defined-implicit";
        case ProjectConversionKind::UserDefinedExplicit: return "user-defined-explicit";
    }
    return "none";
}
}
