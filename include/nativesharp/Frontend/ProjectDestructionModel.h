#pragma once

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ClassDestructionPlan final
{
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> explicitDestructorMemberOrdinal;
    std::vector<std::size_t> destructorMemberOrdinals;
    std::string destructionEntryName;
    std::string objectDestructionEntryName;
};

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

private:
    bool buildClass(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    std::vector<ClassDestructionPlan> plans_;
    std::vector<std::optional<std::size_t>> planByType_;
    std::vector<bool> buildingType_;
    bool built_ = false;
};
}
