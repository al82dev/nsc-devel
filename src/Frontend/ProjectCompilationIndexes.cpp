#include "nativesharp/Frontend/ProjectCompilationIndexes.h"

namespace nativesharp::frontend
{
void ProjectCompilationIndexes::clear() noexcept
{
    typesByOrdinal_.clear();
    typeOrdinalByIdentity_.clear();
    typeOrdinalsByName_.clear();
}

void ProjectCompilationIndexes::rebuildTypes(std::span<const NamedTypeDeclaration> types)
{
    clear();
    typesByOrdinal_.reserve(types.size());
    typeOrdinalByIdentity_.reserve(types.size());
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = types[ordinal];
        typesByOrdinal_.push_back(&type);
        typeOrdinalByIdentity_.try_emplace(type.identity.key(), ordinal);
        typeOrdinalsByName_[type.identity.name()].push_back(ordinal);
        const std::string qualifiedName = type.identity.fullyQualifiedName();
        if (qualifiedName != type.identity.name())
        {
            typeOrdinalsByName_[qualifiedName].push_back(ordinal);
        }
    }
}

const NamedTypeDeclaration* ProjectCompilationIndexes::typeForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < typesByOrdinal_.size() ? typesByOrdinal_[ordinal] : nullptr;
}

std::optional<std::size_t> ProjectCompilationIndexes::typeOrdinalForIdentity(
    std::string_view identity) const
{
    const auto iterator = typeOrdinalByIdentity_.find(std::string(identity));
    return iterator == typeOrdinalByIdentity_.end()
        ? std::nullopt : std::optional<std::size_t>(iterator->second);
}

std::span<const std::size_t> ProjectCompilationIndexes::typeOrdinalsForName(
    std::string_view name) const
{
    const auto iterator = typeOrdinalsByName_.find(std::string(name));
    return iterator == typeOrdinalsByName_.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}
}
