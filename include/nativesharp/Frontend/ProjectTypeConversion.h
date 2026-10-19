#pragma once

#include "nativesharp/Frontend/ProjectConversionKind.h"
#include "nativesharp/Frontend/ProjectIntegerConstant.h"
#include "nativesharp/Frontend/ProjectImplicitConversion.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace nativesharp::frontend
{



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
    static bool isFunctionPointer(std::string_view typeKey) noexcept;
    static bool isFunctionReference(std::string_view typeKey) noexcept;
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
