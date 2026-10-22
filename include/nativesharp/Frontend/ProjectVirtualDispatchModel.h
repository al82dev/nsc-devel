#pragma once

#include "nativesharp/Frontend/VirtualMethodInfo.h"
#include "nativesharp/Frontend/ClassVirtualTable.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{


class ProjectVirtualDispatchModel final
{
public:
    ProjectVirtualDispatchModel() noexcept;
    explicit ProjectVirtualDispatchModel(ProjectCompilationIndexes& indexes) noexcept;

    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const VirtualMethodInfo> virtualMethods() const noexcept;
    std::span<const ClassVirtualTable> classTables() const noexcept;
    const VirtualMethodInfo* virtualMethod(std::size_t memberOrdinal) const noexcept;
    const ClassVirtualTable* classTable(std::size_t typeOrdinal) const noexcept;
    bool isVirtualMethod(std::size_t memberOrdinal) const noexcept;
    std::optional<std::size_t> slotForMember(std::size_t memberOrdinal) const noexcept;
    bool isBuilt() const noexcept;

private:
    bool inheritBaseVirtualTable(
        ClassVirtualTable& table,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAbstractMethod(
        const ClassVirtualTable& table,
        const ClassMemberSymbol& method,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool overrideThrowsCompatible(
        const ClassMemberSymbol& method,
        const ClassMemberSymbol& baseMethod) const;
    bool validateOverrideEffects(
        const ClassMemberSymbol& method,
        const ClassMemberSymbol& baseMethod,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateOverrideSignature(
        std::size_t typeOrdinal,
        const ClassMemberSymbol& method,
        const ClassMemberSymbol& baseMethod,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool registerOverride(
        std::size_t memberOrdinal,
        std::size_t inheritedMemberOrdinal,
        const ClassMemberSymbol& method,
        const VirtualMethodInfo& inheritedVirtual,
        ClassVirtualTable& table);
    bool processOverrideMethod(
        std::size_t typeOrdinal,
        std::size_t memberOrdinal,
        const ClassMemberSymbol& method,
        std::optional<std::size_t> inheritedMemberOrdinal,
        ClassVirtualTable& table,
        diagnostics::DiagnosticEngine& diagnostics);
    bool processNewVirtualMethod(
        std::size_t memberOrdinal,
        const ClassMemberSymbol& method,
        std::optional<std::size_t> inheritedMemberOrdinal,
        ClassVirtualTable& table,
        diagnostics::DiagnosticEngine& diagnostics);
    bool processClassMethod(
        std::size_t typeOrdinal,
        std::size_t memberOrdinal,
        ClassVirtualTable& table,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateConcreteVirtualSlots(
        std::size_t typeOrdinal,
        const ClassVirtualTable& table,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool buildClass(std::size_t typeOrdinal, diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> inheritedMethodWithSignature(
        std::size_t typeOrdinal,
        const ClassMemberSymbol& method) const noexcept;

    ProjectCompilationIndexes ownedIndexes_;
    ProjectCompilationIndexes* indexes_ = nullptr;
    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    std::vector<VirtualMethodInfo> virtualMethods_;
    std::vector<std::optional<std::size_t>> virtualInfoByMember_;
    std::vector<ClassVirtualTable> classTables_;
    std::vector<std::optional<std::size_t>> tableByType_;
    std::vector<bool> buildingType_;
    bool built_ = false;
};
}
