#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Language/CrossModuleLifetime.h"

#include <cctype>
#include <string>

namespace nativesharp::frontend
{
namespace
{
// NSC-LIMIT-009. Solo se llama sobre claves de identidad. Ver symbolFragmentForIdentityKey: un
// anidado y el tipo homonimo de namespace compartian entrada de destruccion.
std::string sanitizeSymbol(std::string_view value)
{
    return compilation::symbolFragmentForIdentityKey(value);
}
}

bool ProjectDestructionModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_ || nameLookup_ != nullptr || memberModel_ != nullptr || inheritanceModel_ != nullptr)
    {
        return false;
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt())
    {
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    inheritanceModel_ = &inheritanceModel;
    planByType_.resize(nameLookup.namedTypes().size());
    buildingType_.resize(nameLookup.namedTypes().size(), false);

    bool ok = true;
    for (const ClassInheritanceInfo& info : inheritanceModel.classes())
    {
        if (!buildClass(info.typeOrdinal, diagnostics))
        {
            ok = false;
        }
    }

    built_ = true;
    return ok && !diagnostics.hasErrors();
}

std::span<const ClassDestructionPlan> ProjectDestructionModel::plans() const noexcept
{
    return plans_;
}

const ClassDestructionPlan* ProjectDestructionModel::planForType(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= planByType_.size() || !planByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *planByType_[typeOrdinal];
    return index < plans_.size() ? &plans_[index] : nullptr;
}

bool ProjectDestructionModel::isBuilt() const noexcept
{
    return built_;
}

bool ProjectDestructionModel::buildClass(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        typeOrdinal >= planByType_.size())
    {
        return false;
    }
    if (planByType_[typeOrdinal].has_value())
    {
        return true;
    }
    if (buildingType_[typeOrdinal])
    {
        return false;
    }
    buildingType_[typeOrdinal] = true;

    ClassDestructionPlan plan;
    plan.typeOrdinal = typeOrdinal;
    plan.destructionEntryName = "__nsc_destroy_" +
        sanitizeSymbol(nameLookup_->namedTypes()[typeOrdinal].identity.key());
    plan.objectDestructionEntryName = "__nsc_destroy_object_" +
        sanitizeSymbol(nameLookup_->namedTypes()[typeOrdinal].identity.key());
    const compilation::TypeIdentity& identity = nameLookup_->namedTypes()[typeOrdinal].identity;
    const language::CrossModuleLifetimeContract crossModule =
        language::makeCrossModuleLifetimeContract(
            identity.moduleName(), identity.key(), "nativesharp.default",
            language::ModuleUnloadPolicy::RejectWhileObjectsLive);
    plan.crossModuleCreateEntryName = crossModule.createThunk;
    plan.crossModuleDestroyEntryName = crossModule.destroyThunk;
    plan.crossModuleDeallocateEntryName = crossModule.deallocateThunk;
    plan.crossModuleLifetimeAbiMajor = crossModule.abiMajor;

    std::size_t destructorCount = 0;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Destructor)
        {
            continue;
        }
        ++destructorCount;
        if (!plan.explicitDestructorMemberOrdinal.has_value())
        {
            plan.explicitDestructorMemberOrdinal = memberOrdinal;
        }
    }

    if (destructorCount > 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            nameLookup_->namedTypes()[typeOrdinal].range,
            "class '" + nameLookup_->namedTypes()[typeOrdinal].identity.key() +
                "' declares more than one destructor");
        buildingType_[typeOrdinal] = false;
        return false;
    }

    if (plan.explicitDestructorMemberOrdinal.has_value())
    {
        plan.destructorMemberOrdinals.push_back(*plan.explicitDestructorMemberOrdinal);
    }

    const std::optional<std::size_t> baseTypeOrdinal = inheritanceModel_->baseTypeOrdinal(typeOrdinal);
    if (baseTypeOrdinal.has_value())
    {
        if (!buildClass(*baseTypeOrdinal, diagnostics))
        {
            buildingType_[typeOrdinal] = false;
            return false;
        }
        const ClassDestructionPlan* basePlan = planForType(*baseTypeOrdinal);
        if (basePlan == nullptr)
        {
            buildingType_[typeOrdinal] = false;
            return false;
        }
        plan.destructorMemberOrdinals.insert(
            plan.destructorMemberOrdinals.end(),
            basePlan->destructorMemberOrdinals.begin(),
            basePlan->destructorMemberOrdinals.end());
    }

    planByType_[typeOrdinal] = plans_.size();
    plans_.push_back(std::move(plan));
    buildingType_[typeOrdinal] = false;
    return true;
}
}
