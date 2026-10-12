#include "nativesharp/Language/AggregateSemantics.h"

#include "CheckedLayoutArithmetic.h"

#include <algorithm>
#include <unordered_set>

namespace nativesharp::language
{
std::string tupleIdentity(std::span<const TupleElement> elements)
{
    std::string result = "tuple:(";
    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        if (index != 0) result.push_back(',');
        result += std::to_string(elements[index].typeKey.size());
        result.push_back(':');
        result += elements[index].typeKey;
    }
    result.push_back(')');
    return result;
}

bool tupleNamesAffectIdentity() noexcept { return false; }

bool tupleConversionExists(
    std::span<const std::string> sourceTypes,
    std::span<const std::string> targetTypes) noexcept
{
    if (sourceTypes.size() != targetTypes.size()) return false;
    for (std::size_t index = 0; index < sourceTypes.size(); ++index)
    {
        if (sourceTypes[index] == targetTypes[index]) continue;
        const bool numeric = sourceTypes[index].starts_with("builtin:") &&
            targetTypes[index].starts_with("builtin:") &&
            sourceTypes[index] != "builtin:string" && targetTypes[index] != "builtin:string";
        if (!numeric) return false;
    }
    return true;
}

std::optional<AggregateLayout> computeWindowsX64AggregateLayout(
    std::span<const AggregateFieldLayout> fields) noexcept
{
    AggregateLayout result;
    result.offsets.reserve(fields.size());
    for (const AggregateFieldLayout field : fields)
    {
        if (field.size == 0 || field.alignment == 0 || field.alignment > 16 ||
            (field.alignment & (field.alignment - 1U)) != 0) return std::nullopt;
        result.alignment = std::max(result.alignment, field.alignment);
        const std::optional<std::size_t> alignedSize =
            detail::checkedLayoutAlignUp(result.size, field.alignment);
        if (!alignedSize.has_value()) return std::nullopt;
        result.size = *alignedSize;
        result.offsets.push_back(result.size);
        const std::optional<std::size_t> fieldEnd =
            detail::checkedLayoutAdd(result.size, field.size);
        if (!fieldEnd.has_value()) return std::nullopt;
        result.size = *fieldEnd;
    }
    const std::optional<std::size_t> finalSize =
        detail::checkedLayoutAlignUp(result.size, result.alignment);
    if (!finalSize.has_value()) return std::nullopt;
    result.size = *finalSize;
    return result;
}

WindowsX64AggregateReturn classifyWindowsX64AggregateReturn(std::size_t size) noexcept
{
    return size == 1 || size == 2 || size == 4 || size == 8
        ? WindowsX64AggregateReturn::DirectInteger
        : WindowsX64AggregateReturn::IndirectSRet;
}

bool validateDeconstructionBindings(
    std::span<const DeconstructionBinding> bindings,
    std::string& error)
{
    error.clear();
    std::unordered_set<std::string> names;
    for (const DeconstructionBinding& binding : bindings)
    {
        if (binding.mode == DeconstructionBindingMode::Discard || binding.name == "_") continue;
        if (binding.name.empty())
        {
            error = "deconstruction binding requires a name or discard";
            return false;
        }
        if (!names.insert(binding.name).second)
        {
            error = "duplicate deconstruction binding '" + binding.name + "'";
            return false;
        }
    }
    return true;
}

bool validatePromotedFields(std::span<const PromotedField> fields, std::string& error)
{
    error.clear();
    std::unordered_set<std::string> names;
    for (const PromotedField& field : fields)
    {
        if (field.name.empty() || field.typeKey.empty())
        {
            error = "promoted field requires an explicit name and type";
            return false;
        }
        if (!names.insert(field.name).second)
        {
            error = "duplicate promoted field '" + field.name + "'";
            return false;
        }
    }
    return true;
}

bool validateAggregateSpread(
    std::size_t sourceCount,
    std::span<const std::string> explicitFields,
    std::string& error)
{
    error.clear();
    if (sourceCount > 1)
    {
        error = "an aggregate spread accepts exactly one source";
        return false;
    }
    std::unordered_set<std::string> names;
    for (const std::string& name : explicitFields)
    {
        if (!names.insert(name).second)
        {
            error = "duplicate explicit aggregate field '" + name + "'";
            return false;
        }
    }
    return true;
}
}
