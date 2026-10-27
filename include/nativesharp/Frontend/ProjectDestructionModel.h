#pragma once

#include "nativesharp/Frontend/ClassDestructionPlan.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class PrecompiledSnapshotReader;

class ProjectDestructionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const ClassDestructionPlan> plans() const noexcept;
    const ClassDestructionPlan* planForType(std::size_t typeOrdinal) const noexcept;
    bool isBuilt() const noexcept;

    // EN: Plan item 3.10. Accepted snapshots, set before build(). A class whose parts are all in a
    //     snapshot unit takes its destruction plan from the `destruction` section, after its base
    //     plan is built exactly as today.
    // ES: Punto 3.10 del plan. Snapshots aceptados, asignados antes de build(). Una clase con todas
    //     sus partes en una unidad con snapshot toma su plan de destrucción de la sección
    //     `destruction`, después de construir el plan de su base igual que hoy.
    void setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept;
    std::size_t installedSnapshotClassCount() const noexcept;
    // EN: Producer side: writes the plans of the classes whose parts are all in `fileId`.
    // ES: Lado productor: escribe los planes de las clases con todas sus partes en `fileId`.
    void setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept;
    bool writePrecompiledSnapshotDestruction(std::string& payload, std::string& error) const;

private:
    bool buildClass(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void readSnapshotDestruction();
    bool readSnapshotPlan(PrecompiledSnapshotReader& reader);
    bool installSnapshotPlan(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        bool& built);

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    std::vector<ClassDestructionPlan> plans_;
    std::vector<std::optional<std::size_t>> planByType_;
    std::vector<bool> buildingType_;
    std::span<const PrecompiledSnapshotUnit> precompiledSnapshots_;
    std::unordered_map<std::size_t, ClassDestructionPlan> snapshotPlans_;
    std::size_t installedSnapshotClasses_ = 0;
    std::optional<FileId> snapshotCaptureFile_;
    bool built_ = false;
};
}
