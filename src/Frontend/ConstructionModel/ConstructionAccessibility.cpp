#include "nativesharp/Frontend/ProjectConstructionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
bool sameProject(
    std::size_t leftTypeOrdinal,
    std::size_t rightTypeOrdinal,
    const ProjectNameLookup& nameLookup) noexcept
{
    if (leftTypeOrdinal >= nameLookup.namedTypes().size() ||
        rightTypeOrdinal >= nameLookup.namedTypes().size())
    {
        return false;
    }
    return nameLookup.namedTypes()[leftTypeOrdinal].projectOrdinal ==
        nameLookup.namedTypes()[rightTypeOrdinal].projectOrdinal;
}
}

bool ProjectConstructionModel::constructorAccessible(
    const ClassMemberSymbol& constructor,
    std::size_t callerMemberOrdinal,
    bool baseInitializer) const noexcept
{
    if (memberModel_ == nullptr)
    {
        return false;
    }
    if (callerMemberOrdinal >= memberModel_->members().size())
    {
        if (namespaceFunctionModel_ == nullptr) return false;
        const std::size_t functionOrdinal = callerMemberOrdinal - memberModel_->members().size();
        if (functionOrdinal >= namespaceFunctionModel_->functions().size()) return false;
        return constructorAccessibleFromProject(
            constructor,
            namespaceFunctionModel_->functions()[functionOrdinal].projectOrdinal);
    }
    return constructorAccessibleFromType(
        constructor,
        memberModel_->members()[callerMemberOrdinal].declaringTypeOrdinal,
        baseInitializer);
}

bool ProjectConstructionModel::constructorAccessibleFromProject(
    const ClassMemberSymbol& constructor,
    std::size_t callerProjectOrdinal) const noexcept
{
    if (nameLookup_ == nullptr ||
        constructor.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    switch (constructor.accessibility)
    {
        case ClassMemberAccessibility::Public:
            return true;
        case ClassMemberAccessibility::Internal:
            return nameLookup_->namedTypes()[constructor.declaringTypeOrdinal].projectOrdinal ==
                callerProjectOrdinal;
        case ClassMemberAccessibility::Private:
        case ClassMemberAccessibility::Protected:
            return false;
    }
    return false;
}

bool ProjectConstructionModel::constructorAccessibleFromType(
    const ClassMemberSymbol& constructor,
    std::size_t callerTypeOrdinal,
    bool baseInitializer) const noexcept
{
    if (nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        callerTypeOrdinal >= nameLookup_->namedTypes().size() ||
        constructor.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    if (callerTypeOrdinal == constructor.declaringTypeOrdinal)
    {
        return true;
    }

    switch (constructor.accessibility)
    {
        case ClassMemberAccessibility::Public:
            return true;
        case ClassMemberAccessibility::Internal:
            return sameProject(callerTypeOrdinal, constructor.declaringTypeOrdinal, *nameLookup_);
        case ClassMemberAccessibility::Private:
            return false;
        case ClassMemberAccessibility::Protected:
            return baseInitializer &&
                inheritanceModel_->isDerivedFrom(callerTypeOrdinal, constructor.declaringTypeOrdinal);
    }
    return false;
}

bool ProjectConstructionModel::implicitDefaultAccessible(
    std::size_t targetTypeOrdinal,
    std::size_t callerMemberOrdinal,
    bool baseInitializer) const noexcept
{
    if (memberModel_ == nullptr)
    {
        return false;
    }
    if (callerMemberOrdinal >= memberModel_->members().size())
    {
        if (namespaceFunctionModel_ == nullptr) return false;
        const std::size_t functionOrdinal = callerMemberOrdinal - memberModel_->members().size();
        if (functionOrdinal >= namespaceFunctionModel_->functions().size()) return false;
        return implicitDefaultAccessibleFromProject(
            targetTypeOrdinal,
            namespaceFunctionModel_->functions()[functionOrdinal].projectOrdinal);
    }
    return implicitDefaultAccessibleFromType(
        targetTypeOrdinal,
        memberModel_->members()[callerMemberOrdinal].declaringTypeOrdinal,
        baseInitializer);
}

bool ProjectConstructionModel::implicitDefaultAccessibleFromProject(
    std::size_t targetTypeOrdinal,
    std::size_t callerProjectOrdinal) const noexcept
{
    if (nameLookup_ == nullptr || targetTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    const NamedTypeDeclaration& target = nameLookup_->namedTypes()[targetTypeOrdinal];
    return target.accessibility == NamedTypeAccessibility::Public ||
        target.projectOrdinal == callerProjectOrdinal;
}

bool ProjectConstructionModel::implicitDefaultAccessibleFromType(
    std::size_t targetTypeOrdinal,
    std::size_t callerTypeOrdinal,
    bool baseInitializer) const noexcept
{
    if (nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        callerTypeOrdinal >= nameLookup_->namedTypes().size() ||
        targetTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    if (callerTypeOrdinal == targetTypeOrdinal)
    {
        return true;
    }
    const NamedTypeDeclaration& target = nameLookup_->namedTypes()[targetTypeOrdinal];
    if (baseInitializer && inheritanceModel_->isDerivedFrom(callerTypeOrdinal, targetTypeOrdinal))
    {
        return target.accessibility == NamedTypeAccessibility::Public ||
            sameProject(callerTypeOrdinal, targetTypeOrdinal, *nameLookup_);
    }
    if (target.accessibility == NamedTypeAccessibility::Public)
    {
        return true;
    }
    return sameProject(callerTypeOrdinal, targetTypeOrdinal, *nameLookup_);
}

}
