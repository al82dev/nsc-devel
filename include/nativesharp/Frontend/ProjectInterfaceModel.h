#pragma once

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct InterfaceTypeInfo final
{
    std::size_t typeOrdinal = 0;
    bool isTrait = false;
    std::vector<std::size_t> directBaseInterfaceOrdinals;
    std::vector<std::size_t> transitiveBaseInterfaceOrdinals;
    std::vector<std::size_t> effectiveMethodMemberOrdinals;
    std::vector<std::size_t> requiredStaticMemberOrdinals;
};

struct ClassInterfaceTablePlan final
{
    std::size_t classTypeOrdinal = 0;
    std::size_t interfaceTypeOrdinal = 0;
    std::uint64_t stableInterfaceTypeId = 0;
    std::vector<std::size_t> interfaceMethodMemberOrdinals;
    std::vector<std::optional<std::size_t>> implementationMemberOrdinals;
};

class ProjectInterfaceModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const InterfaceTypeInfo> interfaces() const noexcept;
    const InterfaceTypeInfo* interfaceInfo(std::size_t typeOrdinal) const noexcept;
    bool isInterface(std::size_t typeOrdinal) const noexcept;
    bool interfaceDerivesFrom(
        std::size_t derivedInterfaceOrdinal,
        std::size_t baseInterfaceOrdinal,
        bool includeIdentity = false) const noexcept;
    bool classImplementsInterface(
        std::size_t classTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
    std::span<const std::size_t> interfacesOfClass(std::size_t classTypeOrdinal) const noexcept;
    std::span<const ClassInterfaceTablePlan> tablePlans() const noexcept;
    const ClassInterfaceTablePlan* tablePlan(
        std::size_t classTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
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

    bool collectInterfaceBases(diagnostics::DiagnosticEngine& diagnostics);
    bool visitInterface(
        std::size_t typeOrdinal,
        std::vector<VisitState>& states,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildClassPlans(diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> findImplementation(
        std::size_t classTypeOrdinal,
        const ClassMemberSymbol& interfaceMethod) const noexcept;
    std::optional<std::size_t> findStaticImplementation(
        std::size_t classTypeOrdinal,
        const ClassMemberSymbol& requirement) const noexcept;
    static std::uint64_t stableTypeId(std::string_view value) noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    std::vector<InterfaceTypeInfo> interfaces_;
    std::vector<std::optional<std::size_t>> interfaceInfoByType_;
    std::vector<std::vector<std::size_t>> interfacesByClass_;
    std::vector<ClassInterfaceTablePlan> tablePlans_;
    bool built_ = false;
};
}
