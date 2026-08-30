#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ClassInheritanceInfo final
{
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::string baseTypeKey;
    std::size_t depth = 0;
    std::vector<std::size_t> directlyDerivedTypeOrdinals;
};

class ProjectInheritanceModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const ClassInheritanceInfo> classes() const noexcept;
    const ClassInheritanceInfo* classInfo(std::size_t typeOrdinal) const noexcept;
    std::optional<std::size_t> baseTypeOrdinal(std::size_t typeOrdinal) const noexcept;
    bool isClass(std::size_t typeOrdinal) const noexcept;
    bool isDerivedFrom(
        std::size_t derivedTypeOrdinal,
        std::size_t targetBaseTypeOrdinal,
        bool includeIdentity = false) const noexcept;
    std::optional<std::size_t> inheritanceDistance(
        std::size_t derivedTypeOrdinal,
        std::size_t targetBaseTypeOrdinal) const noexcept;
    std::optional<std::size_t> typeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    std::string typeKeyForOrdinal(std::size_t typeOrdinal) const;
    bool isBuilt() const noexcept;

private:
    enum class VisitState
    {
        Unvisited,
        Visiting,
        Complete,
        Failed
    };

    bool resolveBaseSpecifications(diagnostics::DiagnosticEngine& diagnostics);
    bool visitClass(
        std::size_t typeOrdinal,
        std::vector<VisitState>& states,
        diagnostics::DiagnosticEngine& diagnostics);

    const ProjectNameLookup* nameLookup_ = nullptr;
    std::vector<ClassInheritanceInfo> classes_;
    std::vector<std::optional<std::size_t>> classInfoByType_;
    bool built_ = false;
};
}
