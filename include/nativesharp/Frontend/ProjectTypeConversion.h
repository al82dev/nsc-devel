#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
enum class ProjectConversionKind
{
    None,
    Identity,
    IntegralPromotion,
    ConstantInteger,
    Numeric,
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
    static bool isNativeCoreReference(std::string_view typeKey) noexcept;
    static std::optional<std::string> nativeArrayElementType(std::string_view typeKey);
};

const char* projectConversionKindName(ProjectConversionKind kind) noexcept;
}
