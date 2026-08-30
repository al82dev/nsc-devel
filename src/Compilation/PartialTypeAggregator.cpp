#include "nativesharp/Compilation/PartialTypeAggregator.h"

#include <algorithm>
#include <utility>

namespace nativesharp::compilation
{
bool PartialTypeAggregator::addPart(TypePartDescriptor part, std::string& error)
{
    if (finalized_)
    {
        error = "cannot add a partial type declaration after aggregation was finalized";
        return false;
    }
    if (!part.identity.isValid())
    {
        error = "invalid type identity: " + part.identity.validationError();
        return false;
    }
    if (part.sourcePath.empty())
    {
        error = "type declaration part must preserve its source path";
        return false;
    }

    const auto duplicate = std::find_if(
        parts_.begin(),
        parts_.end(),
        [&part](const TypePartDescriptor& existing)
        {
            return existing.identity == part.identity &&
                existing.sourceOrdinal == part.sourceOrdinal &&
                existing.sourceOffset == part.sourceOffset;
        });
    if (duplicate != parts_.end())
    {
        error = "duplicate type declaration source location: '" + part.sourcePath + "'";
        return false;
    }

    parts_.push_back(std::move(part));
    return true;
}

bool PartialTypeAggregator::finalize(std::string& error)
{
    if (finalized_)
    {
        return true;
    }

    std::sort(
        parts_.begin(),
        parts_.end(),
        [](const TypePartDescriptor& left, const TypePartDescriptor& right)
        {
            if (left.identity.key() != right.identity.key())
            {
                return left.identity.key() < right.identity.key();
            }
            if (left.sourceOrdinal != right.sourceOrdinal)
            {
                return left.sourceOrdinal < right.sourceOrdinal;
            }
            return left.sourceOffset < right.sourceOffset;
        });

    for (const TypePartDescriptor& part : parts_)
    {
        if (types_.empty() || !(types_.back().identity == part.identity))
        {
            types_.push_back(MergedTypeDescriptor{
                part.identity,
                TypeDeclarationKind::Class,
                DeclaredAccessibility::Internal,
                false,
                false,
                false,
                std::nullopt,
                {}});
        }
        types_.back().parts.push_back(part);
    }

    for (MergedTypeDescriptor& merged : types_)
    {
        if (!mergeGroup(merged, error))
        {
            types_.clear();
            return false;
        }
    }

    finalized_ = true;
    return true;
}

std::span<const MergedTypeDescriptor> PartialTypeAggregator::types() const noexcept
{
    return types_;
}

bool PartialTypeAggregator::isFinalized() const noexcept
{
    return finalized_;
}

bool PartialTypeAggregator::mergeGroup(MergedTypeDescriptor& merged, std::string& error)
{
    if (merged.parts.empty())
    {
        error = "internal error: partial type group is empty";
        return false;
    }

    const std::string typeName = merged.identity.fullyQualifiedName();
    merged.kind = merged.parts.front().kind;
    std::optional<DeclaredAccessibility> explicitAccessibility;

    for (const TypePartDescriptor& part : merged.parts)
    {
        if (part.kind != merged.kind)
        {
            error = "declarations of type '" + typeName + "' have different type kinds";
            return false;
        }
    }

    if (merged.parts.size() > 1)
    {
        const bool anyPartial = std::any_of(
            merged.parts.begin(),
            merged.parts.end(),
            [](const TypePartDescriptor& part)
            {
                return part.isPartial;
            });
        if (!anyPartial)
        {
            error = "duplicate non-partial type declaration '" + typeName + "'";
            return false;
        }
        for (const TypePartDescriptor& part : merged.parts)
        {
            if (!part.isPartial)
            {
                error = "type '" + typeName +
                    "' has multiple declarations but not every declaration is partial";
                return false;
            }
        }
    }

    for (const TypePartDescriptor& part : merged.parts)
    {
        if (part.accessibility != DeclaredAccessibility::Unspecified)
        {
            if (explicitAccessibility.has_value() && *explicitAccessibility != part.accessibility)
            {
                error = "partial declarations of type '" + typeName + "' have conflicting accessibility";
                return false;
            }
            explicitAccessibility = part.accessibility;
        }
        if (part.baseTypeKey.has_value())
        {
            if (merged.baseTypeKey.has_value() && *merged.baseTypeKey != *part.baseTypeKey)
            {
                error = "partial declarations of type '" + typeName + "' specify different base classes";
                return false;
            }
            merged.baseTypeKey = part.baseTypeKey;
        }

        merged.isAbstract = merged.isAbstract || part.isAbstract;
        merged.isSealed = merged.isSealed || part.isSealed;
        merged.isStatic = merged.isStatic || part.isStatic;
    }

    merged.accessibility = explicitAccessibility.value_or(DeclaredAccessibility::Internal);
    if (merged.isAbstract && merged.isSealed && !merged.isStatic)
    {
        error = "type '" + typeName + "' cannot be both abstract and sealed";
        return false;
    }
    if (merged.isStatic && (merged.isAbstract || merged.isSealed))
    {
        error = "static type '" + typeName + "' cannot explicitly combine static with abstract or sealed";
        return false;
    }
    if (merged.isStatic && merged.baseTypeKey.has_value())
    {
        error = "static type '" + typeName + "' cannot specify a base class";
        return false;
    }
    return true;
}
}
