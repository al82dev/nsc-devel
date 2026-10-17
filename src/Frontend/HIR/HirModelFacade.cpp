#include "nativesharp/Frontend/ProjectOopHirModel.h"

// Public model facade extracted by H01B1. The build implementation lives in
// HirBuildCoordinator.cpp; lowering components are split by the remaining H01 slices.

namespace nativesharp::frontend
{
bool ProjectOopHirModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectConstructionModel& constructionModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::string& error,
    const ProjectGenericConstraintModel* genericConstraintModel,
    std::size_t workerCount,
    const ProjectNamespaceFunctionModel* namespaceFunctionModel,
    const ProjectMetaprogrammingModel* metaprogrammingModel)
{
    return buildImpl(
        nameLookup,
        memberModel,
        inheritanceModel,
        interfaceModel,
        methodBodyModel,
        constructionModel,
        objectLayoutModel,
        error,
        genericConstraintModel,
        workerCount,
        namespaceFunctionModel,
        metaprogrammingModel,
        std::nullopt,
        nullptr);
}

std::span<const ProjectOopHirFunction> ProjectOopHirModel::functions() const noexcept
{
    return functions_;
}

std::span<const ProjectOopHirFieldInitializer> ProjectOopHirModel::fieldInitializers() const noexcept
{
    return fieldInitializers_;
}

const ProjectOopHirFunction* ProjectOopHirModel::functionForMember(std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= functionByMember_.size())
    {
        return functionForNamespaceFunction(memberOrdinal - functionByMember_.size());
    }
    if (!functionByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByMember_[memberOrdinal]];
}

const ProjectOopHirFunction* ProjectOopHirModel::functionForNamespaceFunction(
    std::size_t functionOrdinal) const noexcept
{
    if (functionOrdinal >= functionByNamespaceFunction_.size() ||
        !functionByNamespaceFunction_[functionOrdinal].has_value())
    {
        return nullptr;
    }
    return &functions_[*functionByNamespaceFunction_[functionOrdinal]];
}

bool ProjectOopHirModel::isBuilt() const noexcept
{
    return built_;
}
}
