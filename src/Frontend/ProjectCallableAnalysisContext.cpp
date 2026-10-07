#include "nativesharp/Frontend/ProjectCallableAnalysisContext.h"

namespace nativesharp::frontend
{
ProjectCallableAnalysisContext::ProjectCallableAnalysisContext(
    std::size_t callableOrdinal,
    std::size_t waveOrdinal,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectGenericModel* genericModel,
    const ProjectGenericConstraintModel* genericConstraintModel) noexcept
    : callableOrdinal_(callableOrdinal),
      waveOrdinal_(waveOrdinal),
      nameLookup_(&nameLookup),
      memberModel_(&memberModel),
      namespaceFunctionModel_(&namespaceFunctionModel),
      inheritanceModel_(&inheritanceModel),
      interfaceModel_(&interfaceModel),
      genericModel_(genericModel),
      genericConstraintModel_(genericConstraintModel)
{
}

std::size_t ProjectCallableAnalysisContext::callableOrdinal() const noexcept
{
    return callableOrdinal_;
}

std::size_t ProjectCallableAnalysisContext::waveOrdinal() const noexcept
{
    return waveOrdinal_;
}

const ProjectNameLookup& ProjectCallableAnalysisContext::nameLookup() const noexcept
{
    return *nameLookup_;
}

const ProjectMemberModel& ProjectCallableAnalysisContext::memberModel() const noexcept
{
    return *memberModel_;
}

const ProjectNamespaceFunctionModel&
ProjectCallableAnalysisContext::namespaceFunctionModel() const noexcept
{
    return *namespaceFunctionModel_;
}

const ProjectInheritanceModel& ProjectCallableAnalysisContext::inheritanceModel() const noexcept
{
    return *inheritanceModel_;
}

const ProjectInterfaceModel& ProjectCallableAnalysisContext::interfaceModel() const noexcept
{
    return *interfaceModel_;
}

const ProjectGenericModel* ProjectCallableAnalysisContext::genericModel() const noexcept
{
    return genericModel_;
}

const ProjectGenericConstraintModel*
ProjectCallableAnalysisContext::genericConstraintModel() const noexcept
{
    return genericConstraintModel_;
}

}
