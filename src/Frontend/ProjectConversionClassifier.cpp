#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <string>
#include <unordered_set>

namespace nativesharp::frontend
{
namespace
{
ProjectImplicitConversion noConversion(std::string_view source, std::string_view target)
{
    return ProjectImplicitConversion{
        ProjectConversionKind::None, 1000, std::string(source), std::string(target), std::nullopt};
}
}

ProjectConversionClassifier::ProjectConversionClassifier(
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel* interfaceModel,
    const ProjectMemberModel* memberModel) noexcept
    : inheritanceModel_(inheritanceModel),
      interfaceModel_(interfaceModel),
      memberModel_(memberModel)
{
}

ProjectImplicitConversion ProjectConversionClassifier::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant) const
{
    ProjectImplicitConversion primitive = ProjectTypeConversion::classifyImplicit(
        sourceTypeKey,
        targetTypeKey,
        integerConstant,
        isReferenceType(targetTypeKey));
    if (primitive.exists())
    {
        return primitive;
    }

    const std::optional<std::size_t> sourceType = inheritanceModel_.typeOrdinalForTypeKey(sourceTypeKey);
    const std::optional<std::size_t> targetType = inheritanceModel_.typeOrdinalForTypeKey(targetTypeKey);
    if (sourceType.has_value() && targetType.has_value() &&
        inheritanceModel_.isClass(*sourceType) && inheritanceModel_.isClass(*targetType) &&
        inheritanceModel_.isDerivedFrom(*sourceType, *targetType))
    {
        const std::optional<std::size_t> distance = inheritanceModel_.inheritanceDistance(
            *sourceType, *targetType);
        return ProjectImplicitConversion{
            ProjectConversionKind::ReferenceUpcast,
            4 + static_cast<int>(distance.value_or(1)),
            std::string(sourceTypeKey),
            std::string(targetTypeKey),
            std::nullopt};
    }

    if (interfaceModel_ != nullptr && sourceType.has_value() && targetType.has_value())
    {
        if (inheritanceModel_.isClass(*sourceType) && interfaceModel_->isInterface(*targetType) &&
            interfaceModel_->classImplementsInterface(*sourceType, *targetType))
        {
            return ProjectImplicitConversion{
                ProjectConversionKind::ReferenceInterface,
                5,
                std::string(sourceTypeKey),
                std::string(targetTypeKey),
                std::nullopt};
        }
        if (interfaceModel_->isInterface(*sourceType) && interfaceModel_->isInterface(*targetType) &&
            interfaceModel_->interfaceDerivesFrom(*sourceType, *targetType))
        {
            return ProjectImplicitConversion{
                ProjectConversionKind::ReferenceInterface,
                5,
                std::string(sourceTypeKey),
                std::string(targetTypeKey),
                std::nullopt};
        }
    }

    const ProjectImplicitConversion userDefined = findUserDefined(
        sourceTypeKey, targetTypeKey, true);
    if (userDefined.exists())
    {
        return userDefined;
    }

    return primitive;
}

ProjectImplicitConversion ProjectConversionClassifier::classifyExplicitUserDefined(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey) const
{
    const ProjectImplicitConversion implicit = classifyImplicit(sourceTypeKey, targetTypeKey);
    if (implicit.kind == ProjectConversionKind::UserDefinedImplicit)
    {
        return implicit;
    }
    return findUserDefined(sourceTypeKey, targetTypeKey, false);
}

ProjectImplicitConversion ProjectConversionClassifier::findUserDefined(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    bool implicitOnly) const
{
    if (memberModel_ == nullptr)
    {
        return noConversion(sourceTypeKey, targetTypeKey);
    }

    std::unordered_set<std::size_t> declaringTypes;
    const std::optional<std::size_t> sourceType = inheritanceModel_.typeOrdinalForTypeKey(sourceTypeKey);
    const std::optional<std::size_t> targetType = inheritanceModel_.typeOrdinalForTypeKey(targetTypeKey);
    if (sourceType.has_value()) declaringTypes.insert(*sourceType);
    if (targetType.has_value()) declaringTypes.insert(*targetType);

    std::optional<std::size_t> selected;
    ProjectConversionKind selectedKind = ProjectConversionKind::None;
    for (const std::size_t typeOrdinal : declaringTypes)
    {
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
            if (!member.isUserConversion || !member.isStatic || member.parameters.size() != 1)
            {
                continue;
            }
            if (implicitOnly && !member.isImplicitUserConversion)
            {
                continue;
            }
            if (member.parameters[0].typeKey != sourceTypeKey || member.declaredTypeKey != targetTypeKey)
            {
                continue;
            }
            if (selected.has_value())
            {
                // Ambiguous user conversion: report as absent here. Sema will reject the context.
                return noConversion(sourceTypeKey, targetTypeKey);
            }
            selected = memberOrdinal;
            selectedKind = member.isImplicitUserConversion
                ? ProjectConversionKind::UserDefinedImplicit
                : ProjectConversionKind::UserDefinedExplicit;
        }
    }

    if (!selected.has_value())
    {
        return noConversion(sourceTypeKey, targetTypeKey);
    }
    return ProjectImplicitConversion{
        selectedKind,
        20,
        std::string(sourceTypeKey),
        std::string(targetTypeKey),
        selected};
}

bool ProjectConversionClassifier::isReferenceType(std::string_view typeKey) const noexcept
{
    if (ProjectTypeConversion::isNativeCoreReference(typeKey)) return true;
    const std::optional<std::size_t> type = inheritanceModel_.typeOrdinalForTypeKey(typeKey);
    return type.has_value() &&
        (inheritanceModel_.isClass(*type) ||
         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*type)));
}

bool ProjectConversionClassifier::isClassReferenceType(std::string_view typeKey) const noexcept
{
    const std::optional<std::size_t> type = inheritanceModel_.typeOrdinalForTypeKey(typeKey);
    return type.has_value() && inheritanceModel_.isClass(*type);
}
}
