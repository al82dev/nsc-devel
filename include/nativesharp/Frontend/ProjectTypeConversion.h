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
    NullPointer,
    NullReference,
    ReferenceUpcast,
    ReferenceInterface,
    PointerToVoid,
    StringLiteralToCharPointer,
    StringLiteralToNativeString,
    UserDefinedImplicit,
    UserDefinedExplicit
};

struct ProjectIntegerConstant final
{
    std::uint64_t value = 0;
    bool isUnsigned = false;
    // ID 119. `value` es una MAGNITUD, no un patron de bits en complemento a dos: `constantFits`
    // solo compara contra cotas superiores. Un literal negativo se representa entonces con la
    // magnitud y esta bandera, no con `value` negado -- negarlo daria 0xFFFF...FD y ninguna cota
    // superior lo aceptaria.
    bool isNegative = false;
};

struct ProjectImplicitConversion final
{
    ProjectConversionKind kind = ProjectConversionKind::None;
    int rank = 1000;
    std::string sourceTypeKey;
    std::string targetTypeKey;
    std::optional<std::size_t> userDefinedMemberOrdinal;

    bool exists() const noexcept;
};

class ProjectTypeConversion final
{
public:
    static ProjectImplicitConversion classifyImplicit(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        std::optional<ProjectIntegerConstant> integerConstant = std::nullopt,
        bool targetIsClassReference = false);

    static std::optional<std::string> commonNumericType(
        std::string_view leftTypeKey,
        std::string_view rightTypeKey,
        std::optional<ProjectIntegerConstant> leftIntegerConstant = std::nullopt,
        std::optional<ProjectIntegerConstant> rightIntegerConstant = std::nullopt);

    static bool isNumeric(std::string_view typeKey) noexcept;
    static bool isIntegral(std::string_view typeKey) noexcept;
    static bool isPointer(std::string_view typeKey) noexcept;
    static bool isNativeString(std::string_view typeKey) noexcept;
    static bool isNativeArray(std::string_view typeKey) noexcept;
    static bool isNativeRangeArray(std::string_view typeKey) noexcept;
    static std::size_t nativeArrayRank(std::string_view typeKey) noexcept;
    static bool isNativeCoreReference(std::string_view typeKey) noexcept;
    static std::optional<std::string> nativeArrayElementType(std::string_view typeKey);
    static bool isTuple(std::string_view typeKey) noexcept;
    static std::string tupleTypeKey(std::span<const std::string> elementTypeKeys);
    static std::optional<std::vector<std::string>> tupleElementTypeKeys(std::string_view typeKey);
};

const char* projectConversionKindName(ProjectConversionKind kind) noexcept;
}
