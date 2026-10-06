#pragma once

#include "nativesharp/Language/LanguageEdition.h"

#include <cstddef>

namespace nativesharp::frontend
{
class ProjectGenericConstraintModel;
class ProjectGenericModel;
class ProjectInheritanceModel;
class ProjectInterfaceModel;
class ProjectMemberModel;
class ProjectNameLookup;
class ProjectNamespaceFunctionModel;

class ProjectCallableAnalysisContext final
{
public:
    ProjectCallableAnalysisContext(
        std::size_t callableOrdinal,
        std::size_t waveOrdinal,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectGenericModel* genericModel,
        const ProjectGenericConstraintModel* genericConstraintModel,
        language::LanguageEdition languageEdition) noexcept;

    [[nodiscard]] std::size_t callableOrdinal() const noexcept;
    [[nodiscard]] std::size_t waveOrdinal() const noexcept;
    [[nodiscard]] const ProjectNameLookup& nameLookup() const noexcept;
    [[nodiscard]] const ProjectMemberModel& memberModel() const noexcept;
    [[nodiscard]] const ProjectNamespaceFunctionModel& namespaceFunctionModel() const noexcept;
    [[nodiscard]] const ProjectInheritanceModel& inheritanceModel() const noexcept;
    [[nodiscard]] const ProjectInterfaceModel& interfaceModel() const noexcept;
    [[nodiscard]] const ProjectGenericModel* genericModel() const noexcept;
    [[nodiscard]] const ProjectGenericConstraintModel* genericConstraintModel() const noexcept;
    [[nodiscard]] language::LanguageEdition languageEdition() const noexcept;

private:
    std::size_t callableOrdinal_ = 0;
    std::size_t waveOrdinal_ = 0;
    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectNamespaceFunctionModel* namespaceFunctionModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectGenericModel* genericModel_ = nullptr;
    const ProjectGenericConstraintModel* genericConstraintModel_ = nullptr;
    language::LanguageEdition languageEdition_ = language::LanguageEdition::V2;
};
}
