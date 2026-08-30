#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"

#include <string>

namespace nativesharp::frontend
{
bool ProjectVirtualDispatchModel::build(
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
    virtualInfoByMember_.resize(memberModel.members().size());
    tableByType_.resize(nameLookup.namedTypes().size());
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

std::span<const VirtualMethodInfo> ProjectVirtualDispatchModel::virtualMethods() const noexcept
{
    return virtualMethods_;
}

std::span<const ClassVirtualTable> ProjectVirtualDispatchModel::classTables() const noexcept
{
    return classTables_;
}

const VirtualMethodInfo* ProjectVirtualDispatchModel::virtualMethod(std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= virtualInfoByMember_.size() || !virtualInfoByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *virtualInfoByMember_[memberOrdinal];
    return index < virtualMethods_.size() ? &virtualMethods_[index] : nullptr;
}

const ClassVirtualTable* ProjectVirtualDispatchModel::classTable(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= tableByType_.size() || !tableByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *tableByType_[typeOrdinal];
    return index < classTables_.size() ? &classTables_[index] : nullptr;
}

bool ProjectVirtualDispatchModel::isVirtualMethod(std::size_t memberOrdinal) const noexcept
{
    return virtualMethod(memberOrdinal) != nullptr;
}

std::optional<std::size_t> ProjectVirtualDispatchModel::slotForMember(std::size_t memberOrdinal) const noexcept
{
    const VirtualMethodInfo* info = virtualMethod(memberOrdinal);
    return info != nullptr ? std::optional<std::size_t>(info->slotOrdinal) : std::nullopt;
}

bool ProjectVirtualDispatchModel::isBuilt() const noexcept
{
    return built_;
}

bool ProjectVirtualDispatchModel::buildClass(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        typeOrdinal >= tableByType_.size())
    {
        return false;
    }
    if (tableByType_[typeOrdinal].has_value())
    {
        return true;
    }
    if (buildingType_[typeOrdinal])
    {
        // The inheritance model already diagnoses cycles. Keep this guard defensive so this
        // physical dispatch pass never recurses forever if called with a malformed model.
        return false;
    }
    buildingType_[typeOrdinal] = true;

    ClassVirtualTable table;
    table.typeOrdinal = typeOrdinal;
    table.baseTypeOrdinal = inheritanceModel_->baseTypeOrdinal(typeOrdinal);
    table.isAbstract = nameLookup_->namedTypes()[typeOrdinal].isAbstract;
    table.isSealed = nameLookup_->namedTypes()[typeOrdinal].isSealed;
    if (table.baseTypeOrdinal.has_value())
    {
        if (!buildClass(*table.baseTypeOrdinal, diagnostics))
        {
            buildingType_[typeOrdinal] = false;
            return false;
        }
        const ClassVirtualTable* baseTable = classTable(*table.baseTypeOrdinal);
        if (baseTable == nullptr)
        {
            buildingType_[typeOrdinal] = false;
            return false;
        }
        table.slotMemberOrdinals = baseTable->slotMemberOrdinals;
        table.slotAbstract = baseTable->slotAbstract;
    }

    bool ok = true;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
        if (method.kind != ClassMemberKind::Method)
        {
            continue;
        }

        if (method.isAbstract)
        {
            if (!table.isAbstract)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "abstract method '" + method.signatureKey +
                        "' requires the declaring class to be abstract");
                ok = false;
            }
            if (method.accessibility == ClassMemberAccessibility::Private)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "an abstract method cannot be private because a derived class must implement it");
                ok = false;
            }
        }

        const std::optional<std::size_t> inherited = inheritedMethodWithSignature(typeOrdinal, method);
        if (method.isOverride)
        {
            if (!inherited.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "override method '" + method.signatureKey + "' has no inherited method to override");
                ok = false;
                continue;
            }
            const VirtualMethodInfo* inheritedVirtual = virtualMethod(*inherited);
            if (inheritedVirtual == nullptr)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "override method '" + method.signatureKey + "' targets a non-virtual inherited method");
                ok = false;
                continue;
            }
            const ClassMemberSymbol& baseMethod = memberModel_->members()[*inherited];
            if (baseMethod.isSealed)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "override method '" + method.signatureKey +
                        "' targets a sealed override and cannot override it again");
                ok = false;
                continue;
            }
            if (!ProjectMemberAccessibility::isAccessible(
                    baseMethod,
                    typeOrdinal,
                    typeOrdinal,
                    *nameLookup_,
                    *inheritanceModel_))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "override method '" + method.signatureKey +
                        "' targets an inherited method that is not accessible from the derived class");
                ok = false;
                continue;
            }
            if (baseMethod.declaredTypeKey != method.declaredTypeKey)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "Macro A requires an override to preserve the exact return type of the base virtual method");
                ok = false;
                continue;
            }
            if (baseMethod.accessibility != method.accessibility)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "Macro A requires an override to preserve the accessibility of the base virtual method");
                ok = false;
                continue;
            }

            const std::size_t slot = inheritedVirtual->slotOrdinal;
            if (slot >= table.slotMemberOrdinals.size())
            {
                ok = false;
                continue;
            }
            table.slotMemberOrdinals[slot] = memberOrdinal;
            if (slot >= table.slotAbstract.size())
            {
                table.slotAbstract.resize(table.slotMemberOrdinals.size(), false);
            }
            table.slotAbstract[slot] = method.isAbstract;
            VirtualMethodInfo info;
            info.memberOrdinal = memberOrdinal;
            info.slotOrdinal = slot;
            info.rootVirtualMemberOrdinal = inheritedVirtual->rootVirtualMemberOrdinal;
            info.overriddenMemberOrdinal = inherited;
            virtualInfoByMember_[memberOrdinal] = virtualMethods_.size();
            virtualMethods_.push_back(info);
            continue;
        }

        if (method.isVirtual)
        {
            if (method.accessibility == ClassMemberAccessibility::Private)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "a virtual method cannot be private because derived classes must be able to override it");
                ok = false;
                continue;
            }
            if (inherited.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    virtualMethod(*inherited) != nullptr
                        ? "method hides an inherited virtual signature; use override in the Macro A language subset"
                        : "virtual method hides an inherited non-virtual signature; explicit new-slot hiding is outside Macro A");
                ok = false;
                continue;
            }

            VirtualMethodInfo info;
            info.memberOrdinal = memberOrdinal;
            info.slotOrdinal = table.slotMemberOrdinals.size();
            info.rootVirtualMemberOrdinal = memberOrdinal;
            table.slotMemberOrdinals.push_back(memberOrdinal);
            table.slotAbstract.push_back(method.isAbstract);
            virtualInfoByMember_[memberOrdinal] = virtualMethods_.size();
            virtualMethods_.push_back(info);
            continue;
        }

        if (inherited.has_value() && virtualMethod(*inherited) != nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOverride,
                diagnostics::DiagnosticSeverity::Error,
                method.range,
                "method hides an inherited virtual signature; use override in the Macro A language subset");
            ok = false;
        }
    }

    if (!table.isAbstract)
    {
        for (std::size_t slot = 0; slot < table.slotAbstract.size(); ++slot)
        {
            if (!table.slotAbstract[slot])
            {
                continue;
            }
            const std::size_t memberOrdinal = slot < table.slotMemberOrdinals.size()
                ? table.slotMemberOrdinals[slot]
                : memberModel_->members().size();
            const std::string signature = memberOrdinal < memberModel_->members().size()
                ? memberModel_->members()[memberOrdinal].signatureKey
                : std::string("<unknown>");
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOverride,
                diagnostics::DiagnosticSeverity::Error,
                nameLookup_->namedTypes()[typeOrdinal].range,
                "concrete class '" + nameLookup_->namedTypes()[typeOrdinal].identity.key() +
                    "' does not implement abstract virtual slot '" + signature + "'");
            ok = false;
        }
    }

    tableByType_[typeOrdinal] = classTables_.size();
    classTables_.push_back(std::move(table));
    buildingType_[typeOrdinal] = false;
    return ok;
}

std::optional<std::size_t> ProjectVirtualDispatchModel::inheritedMethodWithSignature(
    std::size_t typeOrdinal,
    const ClassMemberSymbol& method) const noexcept
{
    if (memberModel_ == nullptr || inheritanceModel_ == nullptr)
    {
        return std::nullopt;
    }
    std::optional<std::size_t> current = inheritanceModel_->baseTypeOrdinal(typeOrdinal);
    while (current.has_value())
    {
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(*current))
        {
            if (memberOrdinal >= memberModel_->members().size())
            {
                continue;
            }
            const ClassMemberSymbol& candidate = memberModel_->members()[memberOrdinal];
            if (candidate.kind == ClassMemberKind::Method && candidate.signatureKey == method.signatureKey)
            {
                return memberOrdinal;
            }
        }
        current = inheritanceModel_->baseTypeOrdinal(*current);
    }
    return std::nullopt;
}
}
