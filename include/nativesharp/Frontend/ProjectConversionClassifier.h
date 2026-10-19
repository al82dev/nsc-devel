#pragma once

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
class ProjectConversionClassifier final
{
public:
    explicit ProjectConversionClassifier(
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel* interfaceModel = nullptr,
        const ProjectMemberModel* memberModel = nullptr) noexcept;

    ProjectImplicitConversion classifyImplicit(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        std::optional<ProjectIntegerConstant> integerConstant = std::nullopt) const;

    ProjectImplicitConversion classifyExplicitUserDefined(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey) const;

    bool isClassReferenceType(std::string_view typeKey) const noexcept;
    bool isReferenceType(std::string_view typeKey) const noexcept;

private:
    std::optional<ProjectImplicitConversion> classifyTupleConversion(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey) const;
    std::optional<ProjectImplicitConversion> classifyReferenceConversion(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        const std::optional<std::size_t>& sourceType,
        const std::optional<std::size_t>& targetType) const;
    static std::string_view terminalTypeKey(std::string_view key);
    static std::string_view specializationElementSpelling(
        std::string_view terminal,
        std::string_view prefix);
    static bool specializedViewElementMatches(
        std::string_view targetTerminal,
        std::string_view prefix,
        std::string_view sourceSpelling);
    static std::optional<ProjectImplicitConversion> classifySpanViewConversion(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey);
    static std::optional<ProjectImplicitConversion> classifyNativeArrayViewConversion(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey);
    static std::optional<ProjectImplicitConversion> classifyStringViewConversion(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey);
    ProjectImplicitConversion findUserDefined(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        bool implicitOnly) const;

    const ProjectInheritanceModel& inheritanceModel_;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
};
}
