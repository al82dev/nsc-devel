#pragma once

#include "nativesharp/Frontend/VirtualMethodInfo.h"
#include "nativesharp/Frontend/ClassVirtualTable.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"
#include "Internal/ClassVirtualTableSnapshotEntry.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class PrecompiledSnapshotReader;

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

    // EN: Plan item 3.9. Accepted snapshots, set before build(). A class whose parts are all in a
    //     snapshot unit takes its virtual slots and the virtual methods of its own members from the
    //     `dispatch` section, after its base table is built exactly as today.
    // ES: Punto 3.9 del plan. Snapshots aceptados, asignados antes de build(). Una clase con todas
    //     sus partes en una unidad con snapshot toma sus slots virtuales y los métodos virtuales de
    //     sus propios miembros de la sección `dispatch`, después de construir su tabla base igual que
    //     hoy.
    void setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept;
    std::size_t installedSnapshotClassCount() const noexcept;
    // EN: Producer side: writes the tables of the classes whose parts are all in `fileId`.
    // ES: Lado productor: escribe las tablas de las clases con todas sus partes en `fileId`.
    void setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept;
    bool writePrecompiledSnapshotDispatch(std::string& payload, std::string& error) const;

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
    void readSnapshotDispatch();
    bool readSnapshotClassTable(PrecompiledSnapshotReader& reader);
    bool installSnapshotClassTable(ClassVirtualTable& table);
    void captureSnapshotClassTable(std::size_t typeOrdinal, std::size_t methodsBegin);
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
    std::span<const PrecompiledSnapshotUnit> precompiledSnapshots_;
    std::unordered_map<std::size_t, ClassVirtualTableSnapshotEntry> snapshotTables_;
    std::size_t installedSnapshotClasses_ = 0;
    std::optional<FileId> snapshotCaptureFile_;
    std::unordered_map<std::size_t, std::pair<std::size_t, std::size_t>> capturedMethodRanges_;
    bool built_ = false;
};
}
