#pragma once

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <optional>
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
    ProjectImplicitConversion findUserDefined(
        std::string_view sourceTypeKey,
        std::string_view targetTypeKey,
        bool implicitOnly) const;

    const ProjectInheritanceModel& inheritanceModel_;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
};
}
