#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
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
    const std::optional<std::vector<std::string>> sourceTuple =
        ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
    const std::optional<std::vector<std::string>> targetTuple =
        ProjectTypeConversion::tupleElementTypeKeys(targetTypeKey);
    if (sourceTuple.has_value() || targetTuple.has_value())
    {
        if (!sourceTuple.has_value() || !targetTuple.has_value() ||
            sourceTuple->size() != targetTuple->size())
            return noConversion(sourceTypeKey, targetTypeKey);
        if (sourceTypeKey == targetTypeKey)
            return ProjectTypeConversion::classifyImplicit(sourceTypeKey, targetTypeKey);
        int rank = 0;
        for (std::size_t index = 0; index < sourceTuple->size(); ++index)
        {
            const ProjectImplicitConversion element = classifyImplicit(
                (*sourceTuple)[index], (*targetTuple)[index]);
            // User-defined conversions require a call per element, which the structural LLVM
            // converter intentionally does not synthesize. Keep tuple conversion structural:
            // builtins, pointer/reference identity/upcasts, and nested tuples only.
            if (!element.exists() ||
                element.kind == ProjectConversionKind::UserDefinedImplicit ||
                element.kind == ProjectConversionKind::UserDefinedExplicit)
                return noConversion(sourceTypeKey, targetTypeKey);
            rank = std::max(rank, element.rank);
        }
        return ProjectImplicitConversion{
            ProjectConversionKind::Tuple,
            rank + 1,
            std::string(sourceTypeKey),
            std::string(targetTypeKey),
            std::nullopt};
    }

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

    // NS304-T11-F01: Span<T> -> ReadOnlySpan<T> is a one-way structural view conversion. Mutable
    // storage becomes an immutable view over the same element type; the reverse is rejected because
    // it would invent mutability. It ranks after reference conversions but before user-defined.
    {
        const auto terminal = [](std::string_view key) -> std::string_view
        {
            const std::size_t separator = key.find_last_of(".:+");
            return separator == std::string_view::npos ? key : key.substr(separator + 1U);
        };
        const std::string_view sourceTerminal = terminal(sourceTypeKey);
        const std::string_view targetTerminal = terminal(targetTypeKey);
        constexpr std::string_view spanPrefix = "Span__g1__";
        constexpr std::string_view readOnlySpanPrefix = "ReadOnlySpan__g1__";
        if (!sourceTerminal.starts_with(spanPrefix) ||
            !targetTerminal.starts_with(readOnlySpanPrefix))
        {
            // Fall through to user-defined conversion.
        }
        else
        {
            const auto elementSpelling = [](std::string_view terminal, std::string_view prefix)
                -> std::string_view
            {
                const std::string_view rest = terminal.substr(prefix.size());
                const std::size_t hash = rest.rfind("__h");
                return hash == std::string_view::npos ? std::string_view{} : rest.substr(0, hash);
            };
            if (elementSpelling(sourceTerminal, spanPrefix) ==
                elementSpelling(targetTerminal, readOnlySpanPrefix))
            {
                return ProjectImplicitConversion{
                    ProjectConversionKind::View,
                    6,
                    std::string(sourceTypeKey),
                    std::string(targetTypeKey),
                    std::nullopt};
            }
        }
    }

    // NS304-T11-F02: a native array becomes a span/readonly-span view without copying. The element
    // matches by sanitized spelling (`builtin:int` -> `int`, `type:F06::T` -> `T`).
    if (const std::optional<std::string> sourceElement =
            ProjectTypeConversion::nativeArrayElementType(sourceTypeKey))
    {
        const auto terminal = [](std::string_view key) -> std::string_view
        {
            const std::size_t separator = key.find_last_of(".:+");
            return separator == std::string_view::npos ? key : key.substr(separator + 1U);
        };
        std::string sourceSpelling;
        if (sourceElement->starts_with("builtin:"))
            sourceSpelling = sourceElement->substr(8);
        else
            sourceSpelling = std::string(terminal(*sourceElement));
        if (!sourceSpelling.empty())
        {
            const std::string targetTerminal = std::string(terminal(targetTypeKey));
            const auto targetElementMatches = [&](std::string_view prefix) -> bool
            {
                if (!std::string_view(targetTerminal).starts_with(prefix)) return false;
                const std::string_view rest = std::string_view(targetTerminal).substr(prefix.size());
                const std::size_t hash = rest.rfind("__h");
                return hash != std::string_view::npos && rest.substr(0, hash) == sourceSpelling;
            };
            if (targetElementMatches("Span__g1__") || targetElementMatches("ReadOnlySpan__g1__"))
            {
                return ProjectImplicitConversion{
                    ProjectConversionKind::View,
                    7,
                    std::string(sourceTypeKey),
                    std::string(targetTypeKey),
                    std::nullopt};
            }
        }
    }

    // NS304-T11-F02: a native string becomes a StringView view without copying (UTF-8 base+length).
    if (sourceTypeKey == "builtin:string")
    {
        const auto terminal = [](std::string_view key) -> std::string_view
        {
            const std::size_t separator = key.find_last_of(".:+");
            return separator == std::string_view::npos ? key : key.substr(separator + 1U);
        };
        if (terminal(targetTypeKey) == "StringView")
        {
            return ProjectImplicitConversion{
                ProjectConversionKind::View,
                7,
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
