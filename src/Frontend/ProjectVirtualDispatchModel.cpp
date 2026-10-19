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

bool ProjectVirtualDispatchModel::inheritBaseVirtualTable(
    ClassVirtualTable& table,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!table.baseTypeOrdinal.has_value())
    {
        return true;
    }
    if (!buildClass(*table.baseTypeOrdinal, diagnostics))
    {
        return false;
    }
    const ClassVirtualTable* baseTable = classTable(*table.baseTypeOrdinal);
    if (baseTable == nullptr)
    {
        return false;
    }
    table.slotMemberOrdinals = baseTable->slotMemberOrdinals;
    table.slotAbstract = baseTable->slotAbstract;
    return true;
}

bool ProjectVirtualDispatchModel::validateAbstractMethod(
    const ClassVirtualTable& table,
    const ClassMemberSymbol& method,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!method.isAbstract)
    {
        return true;
    }
    bool ok = true;
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
    return ok;
}

bool ProjectVirtualDispatchModel::overrideThrowsCompatible(
    const ClassMemberSymbol& method,
    const ClassMemberSymbol& baseMethod) const
{
    if (method.declaredErrorTypeKey.empty())
    {
        return true;
    }
    if (baseMethod.declaredErrorTypeKey.empty())
    {
        return false;
    }
    const std::optional<std::size_t> derivedError =
        inheritanceModel_->typeOrdinalForTypeKey(method.declaredErrorTypeKey);
    const std::optional<std::size_t> baseError =
        inheritanceModel_->typeOrdinalForTypeKey(baseMethod.declaredErrorTypeKey);
    return derivedError.has_value() && baseError.has_value() &&
        inheritanceModel_->isDerivedFrom(*derivedError, *baseError, true);
}

bool ProjectVirtualDispatchModel::validateOverrideEffects(
    const ClassMemberSymbol& method,
    const ClassMemberSymbol& baseMethod,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!overrideThrowsCompatible(method, baseMethod))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "override method '" + method.signatureKey + "' declares throws '" +
                method.declaredErrorTypeKey + "' outside inherited contract '" +
                (baseMethod.declaredErrorTypeKey.empty()
                    ? std::string{"<non-throwing>"}
                    : baseMethod.declaredErrorTypeKey) + "'");
        return false;
    }
    if (baseMethod.isNoHeap && !method.isNoHeap)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "NS3030: override method '" + method.signatureKey +
                "' cannot drop the inherited noheap contract");
        return false;
    }
    if (baseMethod.isSealed)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "override method '" + method.signatureKey +
                "' targets a sealed override and cannot override it again");
        return false;
    }
    return true;
}

bool ProjectVirtualDispatchModel::validateOverrideSignature(
    std::size_t typeOrdinal,
    const ClassMemberSymbol& method,
    const ClassMemberSymbol& baseMethod,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!ProjectMemberAccessibility::isAccessible(
        baseMethod, typeOrdinal, typeOrdinal, *nameLookup_, *inheritanceModel_))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "override method '" + method.signatureKey +
                "' targets an inherited method that is not accessible from the derived class");
        return false;
    }
    if (baseMethod.declaredTypeKey != method.declaredTypeKey ||
        baseMethod.isReferenceReturn != method.isReferenceReturn)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "Macro A requires an override to preserve the exact return type and ref-return mode "
            "of the base virtual method");
        return false;
    }
    if (baseMethod.accessibility != method.accessibility)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "Macro A requires an override to preserve the accessibility of the base virtual method");
        return false;
    }
    return true;
}

bool ProjectVirtualDispatchModel::registerOverride(
    std::size_t memberOrdinal,
    std::size_t inheritedMemberOrdinal,
    const ClassMemberSymbol& method,
    const VirtualMethodInfo& inheritedVirtual,
    ClassVirtualTable& table)
{
    const std::size_t slot = inheritedVirtual.slotOrdinal;
    if (slot >= table.slotMemberOrdinals.size())
    {
        return false;
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
    info.rootVirtualMemberOrdinal = inheritedVirtual.rootVirtualMemberOrdinal;
    info.overriddenMemberOrdinal = inheritedMemberOrdinal;
    virtualInfoByMember_[memberOrdinal] = virtualMethods_.size();
    virtualMethods_.push_back(info);
    return true;
}

bool ProjectVirtualDispatchModel::processOverrideMethod(
    std::size_t typeOrdinal,
    std::size_t memberOrdinal,
    const ClassMemberSymbol& method,
    std::optional<std::size_t> inheritedMemberOrdinal,
    ClassVirtualTable& table,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!inheritedMemberOrdinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "override method '" + method.signatureKey + "' has no inherited method to override");
        return false;
    }
    const VirtualMethodInfo* inheritedVirtual = virtualMethod(*inheritedMemberOrdinal);
    if (inheritedVirtual == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "override method '" + method.signatureKey + "' targets a non-virtual inherited method");
        return false;
    }
    const ClassMemberSymbol& baseMethod = memberModel_->members()[*inheritedMemberOrdinal];
    if (!validateOverrideEffects(method, baseMethod, diagnostics))
    {
        return false;
    }
    if (!validateOverrideSignature(typeOrdinal, method, baseMethod, diagnostics))
    {
        return false;
    }
    return registerOverride(
        memberOrdinal, *inheritedMemberOrdinal, method, *inheritedVirtual, table);
}

bool ProjectVirtualDispatchModel::processNewVirtualMethod(
    std::size_t memberOrdinal,
    const ClassMemberSymbol& method,
    std::optional<std::size_t> inheritedMemberOrdinal,
    ClassVirtualTable& table,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (method.accessibility == ClassMemberAccessibility::Private)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "a virtual method cannot be private because derived classes must be able to override it");
        return false;
    }
    if (inheritedMemberOrdinal.has_value() && !method.isNewSlot)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            virtualMethod(*inheritedMemberOrdinal) != nullptr
                ? "method hides an inherited virtual signature; use override in the Macro A language subset"
                : "virtual method hides an inherited non-virtual signature; explicit new-slot "
                    "hiding is outside Macro A");
        return false;
    }
    VirtualMethodInfo info;
    info.memberOrdinal = memberOrdinal;
    info.slotOrdinal = table.slotMemberOrdinals.size();
    info.rootVirtualMemberOrdinal = memberOrdinal;
    table.slotMemberOrdinals.push_back(memberOrdinal);
    table.slotAbstract.push_back(method.isAbstract);
    virtualInfoByMember_[memberOrdinal] = virtualMethods_.size();
    virtualMethods_.push_back(info);
    return true;
}

bool ProjectVirtualDispatchModel::processClassMethod(
    std::size_t typeOrdinal,
    std::size_t memberOrdinal,
    ClassVirtualTable& table,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
    if (method.kind != ClassMemberKind::Method)
    {
        return true;
    }
    bool ok = validateAbstractMethod(table, method, diagnostics);
    const std::optional<std::size_t> inherited = inheritedMethodWithSignature(typeOrdinal, method);
    if (method.isNewSlot && method.isOverride)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "method '" + method.signatureKey + "' cannot be both new and override");
        return false;
    }
    if (method.isOverride)
    {
        const bool overrideOk = processOverrideMethod(
            typeOrdinal, memberOrdinal, method, inherited, table, diagnostics);
        return ok && overrideOk;
    }
    if (method.isVirtual)
    {
        const bool virtualOk = processNewVirtualMethod(
            memberOrdinal, method, inherited, table, diagnostics);
        return ok && virtualOk;
    }
    if (inherited.has_value() && virtualMethod(*inherited) != nullptr && !method.isNewSlot)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "method hides an inherited virtual signature; use override in the Macro A language subset");
        return false;
    }
    return ok;
}

bool ProjectVirtualDispatchModel::validateConcreteVirtualSlots(
    std::size_t typeOrdinal,
    const ClassVirtualTable& table,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (table.isAbstract)
    {
        return true;
    }
    bool ok = true;
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
    return ok;
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
    if (!inheritBaseVirtualTable(table, diagnostics))
    {
        buildingType_[typeOrdinal] = false;
        return false;
    }

    bool ok = true;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (!processClassMethod(typeOrdinal, memberOrdinal, table, diagnostics))
        {
            ok = false;
        }
    }
    if (!validateConcreteVirtualSlots(typeOrdinal, table, diagnostics))
    {
        ok = false;
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
