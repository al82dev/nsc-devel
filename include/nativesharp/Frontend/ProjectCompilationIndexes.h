#pragma once

#include "nativesharp/Frontend/NamedTypeDeclaration.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
class ProjectCompilationIndexes final
{
public:
    void clear() noexcept;
    void rebuildTypes(std::span<const NamedTypeDeclaration> types);
    const NamedTypeDeclaration* typeForOrdinal(std::size_t ordinal) const noexcept;
    std::optional<std::size_t> typeOrdinalForIdentity(std::string_view identity) const;
    std::span<const std::size_t> typeOrdinalsForName(std::string_view name) const;

private:
    std::vector<const NamedTypeDeclaration*> typesByOrdinal_;
    std::unordered_map<std::string, std::size_t> typeOrdinalByIdentity_;
    std::unordered_map<std::string, std::vector<std::size_t>> typeOrdinalsByName_;
};
}
