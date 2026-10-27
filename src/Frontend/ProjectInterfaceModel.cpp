#include "nativesharp/Frontend/ProjectInterfaceModel.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ReadonlyReferenceReturn.h"
#include "Internal/ProjectInterfaceImplementingType.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::frontend
{
namespace
{
std::vector<const ast::TypeSyntax*> declarationSpecifications(const ast::AstNode* syntax)
{
    if (syntax == nullptr)
    {
        return {};
    }
    if (!syntax->typeSyntaxes().empty())
    {
        return {syntax->typeSyntaxes().begin(), syntax->typeSyntaxes().end()};
    }
    if (syntax->typeSyntax() != nullptr)
    {
        return {syntax->typeSyntax()};
    }
    return {};
}

void appendUnique(std::vector<std::size_t>& destination, std::size_t value)
{
    if (std::find(destination.begin(), destination.end(), value) == destination.end())
    {
        destination.push_back(value);
    }
}

bool declarationIsTrait(const NamedTypeDeclaration& type)
{
    return std::ranges::any_of(type.parts, [](const NamedTypeDeclarationPart& part)
    {
        if (part.syntax == nullptr) return false;
        const std::string_view metadata = part.syntax->auxiliaryText();
        return metadata == "ns3-trait" ||
            metadata.find("nsc-meta:ns3-trait") != std::string_view::npos;
    });
}

std::string lifetimeContractKey(const ClassMemberSymbol& method)
{
    std::unordered_map<std::string, std::string> canonical;
    for (std::size_t index = 0; index < method.lifetimeParameters.size(); ++index)
        canonical.emplace(method.lifetimeParameters[index], "L" + std::to_string(index));
    const auto name = [&](std::string_view value)
    {
        if (value.empty() || value == "this" || value == "static" || value.starts_with("$p"))
            return std::string(value);
        const auto found = canonical.find(std::string(value));
        return found == canonical.end() ? "?" + std::string(value) : found->second;
    };
    std::string result = "ret=" + name(method.returnLifetimeName) + ";params=";
    for (const MethodParameterSymbol& parameter : method.parameters)
        result += name(parameter.lifetimeName) + ',';
    std::vector<std::string> constraints;
    for (const auto& [longer, shorter] : method.lifetimeOutlivesConstraints)
        constraints.push_back(name(longer) + ':' + name(shorter));
    std::ranges::sort(constraints);
    result += ";where=";
    for (const std::string& constraint : constraints) result += constraint + ',';
    return result;
}

bool implementingTypeDepthLess(
    const ProjectInterfaceImplementingType& left,
    const ProjectInterfaceImplementingType& right)
{
    return left.depth < right.depth;
}
}

bool ProjectInterfaceModel::build(
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
    interfaceInfoByType_.resize(nameLookup.namedTypes().size());
    interfacesByClass_.resize(nameLookup.namedTypes().size());
    operatorWitnessesByType_.resize(nameLookup.namedTypes().size());
    tablePlanOrdinalsByClass_.resize(nameLookup.namedTypes().size());
    tablePlanOrdinalsByClassIndexBuilt_ = true;

    for (std::size_t ordinal = 0; ordinal < nameLookup.namedTypes().size(); ++ordinal)
    {
        if (nameLookup.namedTypes()[ordinal].kind != NamedTypeDeclarationKind::Interface)
        {
            continue;
        }
        interfaceInfoByType_[ordinal] = interfaces_.size();
        InterfaceTypeInfo info;
        info.typeOrdinal = ordinal;
        info.isTrait = declarationIsTrait(nameLookup.namedTypes()[ordinal]);
        interfaces_.push_back(std::move(info));
    }

    std::vector<VisitState> states(nameLookup.namedTypes().size(), VisitState::Unvisited);
    // EN: Interfaces and implementing types of restored API units with snapshot entries (plan 3.7).
    // ES: Interfaces y tipos implementadores de unidades de API restauradas con entradas en el
    //     snapshot (plan 3.7).
    installSnapshotInterfaces(states);
    bool ok = collectInterfaceBases(diagnostics);
    for (const InterfaceTypeInfo& info : interfaces_)
    {
        if (!visitInterface(info.typeOrdinal, states, diagnostics))
        {
            ok = false;
        }
    }
    if (!buildClassPlans(diagnostics))
    {
        ok = false;
    }
    if (!buildTraitObjectContracts(diagnostics))
    {
        ok = false;
    }

    built_ = true;
    return ok && !diagnostics.hasErrors();
}

std::span<const InterfaceTypeInfo> ProjectInterfaceModel::interfaces() const noexcept
{
    return interfaces_;
}

const InterfaceTypeInfo* ProjectInterfaceModel::interfaceInfo(std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= interfaceInfoByType_.size() || !interfaceInfoByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *interfaceInfoByType_[typeOrdinal];
    return index < interfaces_.size() ? &interfaces_[index] : nullptr;
}

bool ProjectInterfaceModel::isInterface(std::size_t typeOrdinal) const noexcept
{
    return interfaceInfo(typeOrdinal) != nullptr;
}

bool ProjectInterfaceModel::interfaceDerivesFrom(
    std::size_t derivedInterfaceOrdinal,
    std::size_t baseInterfaceOrdinal,
    bool includeIdentity) const noexcept
{
    if (includeIdentity && derivedInterfaceOrdinal == baseInterfaceOrdinal)
    {
        return isInterface(derivedInterfaceOrdinal);
    }
    const InterfaceTypeInfo* info = interfaceInfo(derivedInterfaceOrdinal);
    return info != nullptr && std::find(
        info->transitiveBaseInterfaceOrdinals.begin(),
        info->transitiveBaseInterfaceOrdinals.end(),
        baseInterfaceOrdinal) != info->transitiveBaseInterfaceOrdinals.end();
}

bool ProjectInterfaceModel::classImplementsInterface(
    std::size_t classTypeOrdinal,
    std::size_t interfaceTypeOrdinal) const noexcept
{
    if (classTypeOrdinal >= interfacesByClass_.size())
    {
        return false;
    }
    const auto& values = interfacesByClass_[classTypeOrdinal];
    return std::find(values.begin(), values.end(), interfaceTypeOrdinal) != values.end();
}

std::span<const std::size_t> ProjectInterfaceModel::interfacesOfClass(
    std::size_t classTypeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return classTypeOrdinal < interfacesByClass_.size()
        ? std::span<const std::size_t>(interfacesByClass_[classTypeOrdinal])
        : std::span<const std::size_t>(empty);
}

std::span<const TraitOperatorWitness> ProjectInterfaceModel::operatorWitnessesOfType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<TraitOperatorWitness> empty;
    return typeOrdinal < operatorWitnessesByType_.size()
        ? std::span<const TraitOperatorWitness>(operatorWitnessesByType_[typeOrdinal])
        : std::span<const TraitOperatorWitness>(empty);
}

std::span<const ClassInterfaceTablePlan> ProjectInterfaceModel::tablePlans() const noexcept
{
    return tablePlans_;
}

std::span<const std::size_t> ProjectInterfaceModel::tablePlanOrdinalsForClass(
    std::size_t classTypeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return tablePlanOrdinalsByClassIndexBuilt_ &&
            classTypeOrdinal < tablePlanOrdinalsByClass_.size()
        ? std::span<const std::size_t>(tablePlanOrdinalsByClass_[classTypeOrdinal])
        : std::span<const std::size_t>(empty);
}

bool ProjectInterfaceModel::hasTablePlanOrdinalsByClassIndex() const noexcept
{
    return tablePlanOrdinalsByClassIndexBuilt_;
}

const ClassInterfaceTablePlan* ProjectInterfaceModel::tablePlan(
    std::size_t classTypeOrdinal,
    std::size_t interfaceTypeOrdinal) const noexcept
{
    for (const ClassInterfaceTablePlan& plan : tablePlans_)
    {
        if (plan.classTypeOrdinal == classTypeOrdinal && plan.interfaceTypeOrdinal == interfaceTypeOrdinal)
        {
            return &plan;
        }
    }
    return nullptr;
}

const TraitObjectContract* ProjectInterfaceModel::traitObjectContract(
    std::size_t traitTypeOrdinal) const noexcept
{
    const auto found = std::ranges::find(
        traitObjectContracts_, traitTypeOrdinal, &TraitObjectContract::traitTypeOrdinal);
    return found == traitObjectContracts_.end() ? nullptr : &*found;
}

bool ProjectInterfaceModel::buildTraitObjectContracts(
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    traitObjectContracts_.clear();
    traitObjectContracts_.reserve(interfaces_.size());

    for (const InterfaceTypeInfo& info : interfaces_)
    {
        TraitObjectContract contract;
        contract.traitTypeOrdinal = info.typeOrdinal;
        contract.objectSafe = true;

        for (const std::size_t memberOrdinal : info.effectiveMethodMemberOrdinals)
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
            if (member.syntax != nullptr && !member.syntax->genericParameters().empty())
            {
                contract.objectSafe = false;
                break;
            }
        }

        // Associated requirements have no single runtime identity unless the trait fixes a
        // default. Implementations may still override a default for static dispatch, but a bare
        // requirement cannot be named through one erased `dyn Trait` surface.
        const auto declarationHasUnboundAssociatedItem = [&](std::size_t ordinal) -> bool
        {
            if (ordinal >= nameLookup_->namedTypes().size()) return true;
            const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[ordinal];
            for (const NamedTypeDeclarationPart& part : declaration.parts)
            {
                if (part.syntax == nullptr) continue;
                for (const ast::AstNode* child : part.syntax->children())
                {
                    if (child == nullptr) continue;
                    if (child->kind() == ast::AstNodeKind::AssociatedTypeDeclaration &&
                        child->typeSyntax() == nullptr)
                        return true;
                    if (child->kind() == ast::AstNodeKind::AssociatedConstantDeclaration &&
                        child->first() == nullptr)
                        return true;
                }
            }
            return false;
        };
        if (contract.objectSafe)
        {
            contract.objectSafe = !declarationHasUnboundAssociatedItem(info.typeOrdinal);
            for (const std::size_t base : info.transitiveBaseInterfaceOrdinals)
                if (declarationHasUnboundAssociatedItem(base))
                {
                    contract.objectSafe = false;
                    break;
                }
        }
        traitObjectContracts_.push_back(contract);
    }

    for (const TypeNameBinding& binding : nameLookup_->bindings())
    {
        if (binding.syntax == nullptr || !binding.syntax->isDynamicTrait() ||
            !binding.targetTypeOrdinal.has_value())
            continue;
        const std::size_t target = *binding.targetTypeOrdinal;
        const TraitObjectContract* contract = traitObjectContract(target);
        if (contract == nullptr) continue; // Name lookup already diagnoses non-interface targets.
        if (!binding.syntax->modifiers().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                binding.syntax->range(),
                "'dyn Trait' is already a reference representation and cannot carry pointer or array modifiers");
            ok = false;
        }
        if (!contract->objectSafe)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                binding.syntax->range(),
                "trait or interface '" + nameLookup_->namedTypes()[target].identity.key() +
                    "' is not object-safe: dyn dispatch requires non-generic instance slots and "
                    "fully defaulted associated items");
            ok = false;
        }
    }
    return ok;
}

std::optional<std::size_t> ProjectInterfaceModel::typeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:"))
    {
        return std::nullopt;
    }
    return nameLookup_->typeOrdinalForIdentity(typeKey.substr(5));
}

std::string ProjectInterfaceModel::typeKeyForOrdinal(std::size_t typeOrdinal) const
{
    if (nameLookup_ == nullptr || typeOrdinal >= nameLookup_->namedTypes().size())
    {
        return "<error>";
    }
    return "type:" + nameLookup_->namedTypes()[typeOrdinal].identity.key();
}

bool ProjectInterfaceModel::isBuilt() const noexcept
{
    return built_;
}

bool ProjectInterfaceModel::collectInterfaceBases(diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    for (InterfaceTypeInfo& info : interfaces_)
    {
        if (info.typeOrdinal < snapshotInstalledInterfaces_.size() && snapshotInstalledInterfaces_[info.typeOrdinal])
            continue;
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[info.typeOrdinal];
        for (const NamedTypeDeclarationPart& part : type.parts)
        {
            for (const ast::TypeSyntax* specification : declarationSpecifications(part.syntax))
            {
                if (specification == nullptr)
                {
                    continue;
                }
                if (!specification->modifiers().empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidBaseType,
                        diagnostics::DiagnosticSeverity::Error,
                        specification->range(),
                        "interface inheritance requires a direct interface type; pointer/array modifiers are invalid");
                    ok = false;
                    continue;
                }
                const std::optional<std::size_t> base = nameLookup_->resolvedTypeOrdinal(specification);
                if (!base.has_value() || *base >= nameLookup_->namedTypes().size())
                {
                    ok = false;
                    continue;
                }
                if (nameLookup_->namedTypes()[*base].kind != NamedTypeDeclarationKind::Interface)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidBaseType,
                        diagnostics::DiagnosticSeverity::Error,
                        specification->range(),
                        "interface '" + type.identity.key() + "' may inherit only from interfaces");
                    ok = false;
                    continue;
                }
                if (*base == info.typeOrdinal)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InheritanceCycle,
                        diagnostics::DiagnosticSeverity::Error,
                        specification->range(),
                        "interface '" + type.identity.key() + "' cannot inherit directly from itself");
                    ok = false;
                    continue;
                }
                appendUnique(info.directBaseInterfaceOrdinals, *base);
            }
        }
    }
    return ok;
}

bool ProjectInterfaceModel::inheritInterfaceMethod(
    std::size_t typeOrdinal,
    InterfaceTypeInfo& info,
    std::size_t memberOrdinal,
    std::unordered_map<std::string, std::size_t>& signatureOwner,
    std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
    if (method.syntax != nullptr && method.syntax->first() != nullptr)
    {
        auto& defaults = inheritedDefaults[method.signatureKey];
        if (std::find(defaults.begin(), defaults.end(), memberOrdinal) == defaults.end())
            defaults.push_back(memberOrdinal);
    }
    const auto found = signatureOwner.find(method.signatureKey);
    if (found == signatureOwner.end())
    {
        signatureOwner.emplace(method.signatureKey, memberOrdinal);
        info.effectiveMethodMemberOrdinals.push_back(memberOrdinal);
        return true;
    }

    bool ok = true;
    const ClassMemberSymbol& previous = memberModel_->members()[found->second];
    if (previous.declaredTypeKey != method.declaredTypeKey ||
        previous.declaredErrorTypeKey != method.declaredErrorTypeKey ||
        previous.isReferenceReturn != method.isReferenceReturn ||
        isReadonlyReferenceReturn(previous) != isReadonlyReferenceReturn(method) ||
        lifetimeContractKey(previous) != lifetimeContractKey(method))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            nameLookup_->namedTypes()[typeOrdinal].range,
            "interface inherits incompatible return types for method '" + method.signatureKey + "'");
        ok = false;
    }
    const bool previousDefault = previous.syntax != nullptr && previous.syntax->first() != nullptr;
    const bool methodDefault = method.syntax != nullptr && method.syntax->first() != nullptr;
    if ((methodDefault && (!previousDefault || interfaceDerivesFrom(
        method.declaringTypeOrdinal, previous.declaringTypeOrdinal))) ||
        (!previous.isNoHeap && method.isNoHeap))
    {
        std::ranges::replace(info.effectiveMethodMemberOrdinals, found->second, memberOrdinal);
        found->second = memberOrdinal;
    }
    return ok;
}

bool ProjectInterfaceModel::inheritStaticRequirement(
    std::size_t typeOrdinal,
    InterfaceTypeInfo& info,
    std::size_t memberOrdinal,
    std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& requirement = memberModel_->members()[memberOrdinal];
    const auto found = staticSignatureOwner.find(requirement.signatureKey);
    if (found == staticSignatureOwner.end())
    {
        staticSignatureOwner.emplace(requirement.signatureKey, memberOrdinal);
        info.requiredStaticMemberOrdinals.push_back(memberOrdinal);
        return true;
    }
    const ClassMemberSymbol& previous = memberModel_->members()[found->second];
    if (previous.declaredTypeKey == requirement.declaredTypeKey &&
        previous.declaredErrorTypeKey == requirement.declaredErrorTypeKey)
    {
        return true;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOverride,
        diagnostics::DiagnosticSeverity::Error,
        nameLookup_->namedTypes()[typeOrdinal].range,
        "trait inherits incompatible static requirements for '" + requirement.signatureKey + "'");
    return false;
}

bool ProjectInterfaceModel::inheritBaseInterface(
    std::size_t typeOrdinal,
    InterfaceTypeInfo& info,
    std::size_t baseOrdinal,
    std::vector<VisitState>& states,
    std::unordered_map<std::string, std::size_t>& signatureOwner,
    std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
    std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!visitInterface(baseOrdinal, states, diagnostics))
    {
        return false;
    }
    appendUnique(info.transitiveBaseInterfaceOrdinals, baseOrdinal);
    const InterfaceTypeInfo* base = interfaceInfo(baseOrdinal);
    if (base == nullptr)
    {
        return false;
    }
    for (const std::size_t transitive : base->transitiveBaseInterfaceOrdinals)
    {
        appendUnique(info.transitiveBaseInterfaceOrdinals, transitive);
    }

    bool ok = true;
    for (const std::size_t memberOrdinal : base->effectiveMethodMemberOrdinals)
    {
        if (!inheritInterfaceMethod(
            typeOrdinal, info, memberOrdinal, signatureOwner, inheritedDefaults, diagnostics))
            ok = false;
    }
    for (const std::size_t memberOrdinal : base->requiredStaticMemberOrdinals)
    {
        if (!inheritStaticRequirement(
            typeOrdinal, info, memberOrdinal, staticSignatureOwner, diagnostics))
            ok = false;
    }
    return ok;
}

bool ProjectInterfaceModel::collectInheritedInterfaceMembers(
    std::size_t typeOrdinal,
    InterfaceTypeInfo& info,
    std::vector<VisitState>& states,
    std::unordered_map<std::string, std::size_t>& signatureOwner,
    std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
    std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    for (const std::size_t baseOrdinal : info.directBaseInterfaceOrdinals)
    {
        if (!inheritBaseInterface(
            typeOrdinal,
            info,
            baseOrdinal,
            states,
            signatureOwner,
            staticSignatureOwner,
            inheritedDefaults,
            diagnostics))
        {
            ok = false;
        }
    }
    return ok;
}

bool ProjectInterfaceModel::validateDeclaredInterfaceMethod(
    const InterfaceTypeInfo& info,
    const ClassMemberSymbol& method,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (method.kind != ClassMemberKind::Method)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "Stage 36 interface declaration spaces contain methods only");
        return false;
    }
    const bool hasBody = method.syntax != nullptr && method.syntax->first() != nullptr;
    if (!info.isTrait && (method.isStatic ||
        method.accessibility != ClassMemberAccessibility::Public ||
        method.syntax == nullptr || method.isAbstract == hasBody))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "interface members are public instance requirements without bodies or default "
            "members with bodies");
        return false;
    }
    if (info.isTrait && (method.accessibility != ClassMemberAccessibility::Public ||
        method.syntax == nullptr || method.isAbstract == hasBody))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "trait members are public requirements without bodies or default methods with bodies");
        return false;
    }
    return true;
}

bool ProjectInterfaceModel::mergeTraitStaticRequirement(
    InterfaceTypeInfo& info,
    std::size_t memberOrdinal,
    std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
    const auto found = staticSignatureOwner.find(method.signatureKey);
    if (found == staticSignatureOwner.end())
    {
        staticSignatureOwner.emplace(method.signatureKey, memberOrdinal);
        info.requiredStaticMemberOrdinals.push_back(memberOrdinal);
        return true;
    }
    const ClassMemberSymbol& previous = memberModel_->members()[found->second];
    if (previous.declaredTypeKey != method.declaredTypeKey)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "trait static requirement '" + method.signatureKey +
                "' conflicts with its inherited return type");
        return false;
    }
    std::ranges::replace(info.requiredStaticMemberOrdinals, found->second, memberOrdinal);
    found->second = memberOrdinal;
    return true;
}

bool ProjectInterfaceModel::mergeDeclaredInterfaceMethod(
    InterfaceTypeInfo& info,
    std::size_t memberOrdinal,
    std::unordered_map<std::string, std::size_t>& signatureOwner,
    std::unordered_set<std::string>& redeclaredSignatures,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
    const auto found = signatureOwner.find(method.signatureKey);
    if (found == signatureOwner.end())
    {
        signatureOwner.emplace(method.signatureKey, memberOrdinal);
        info.effectiveMethodMemberOrdinals.push_back(memberOrdinal);
        return true;
    }
    const ClassMemberSymbol& previous = memberModel_->members()[found->second];
    if (previous.declaredTypeKey != method.declaredTypeKey ||
        previous.declaredErrorTypeKey != method.declaredErrorTypeKey ||
        previous.isReferenceReturn != method.isReferenceReturn ||
        isReadonlyReferenceReturn(previous) != isReadonlyReferenceReturn(method) ||
        lifetimeContractKey(previous) != lifetimeContractKey(method))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "interface method '" + method.signatureKey + "' conflicts with an inherited return type");
        ok = false;
    }
    if (previous.isNoHeap && !method.isNoHeap)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            method.range,
            "NS3030: interface method '" + method.signatureKey +
                "' cannot drop the inherited noheap contract");
        ok = false;
    }
    std::ranges::replace(info.effectiveMethodMemberOrdinals, found->second, memberOrdinal);
    found->second = memberOrdinal;
    redeclaredSignatures.insert(method.signatureKey);
    // Redeclaring an inherited contract keeps its slot but replaces its contract/default.
    return ok;
}

bool ProjectInterfaceModel::collectDeclaredInterfaceMembers(
    std::size_t typeOrdinal,
    InterfaceTypeInfo& info,
    std::unordered_map<std::string, std::size_t>& signatureOwner,
    std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
    std::unordered_set<std::string>& redeclaredSignatures,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
        if (!validateDeclaredInterfaceMethod(info, method, diagnostics))
            ok = false;
        if (method.kind != ClassMemberKind::Method)
        {
            continue;
        }
        if (method.isStatic)
        {
            if (info.isTrait && !mergeTraitStaticRequirement(
                info, memberOrdinal, staticSignatureOwner, diagnostics))
                ok = false;
            continue;
        }
        if (!mergeDeclaredInterfaceMethod(
            info, memberOrdinal, signatureOwner, redeclaredSignatures, diagnostics))
            ok = false;
    }
    return ok;
}

bool ProjectInterfaceModel::inheritedDefaultIsDominated(
    const std::vector<std::size_t>& defaults,
    std::size_t candidateOrdinal) const
{
    const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
    for (const std::size_t otherOrdinal : defaults)
    {
        if (otherOrdinal == candidateOrdinal || otherOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& other = memberModel_->members()[otherOrdinal];
        if (interfaceDerivesFrom(other.declaringTypeOrdinal, candidate.declaringTypeOrdinal))
        {
            return true;
        }
    }
    return false;
}

bool ProjectInterfaceModel::validateInheritedDefaults(
    std::size_t typeOrdinal,
    const std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
    const std::unordered_set<std::string>& redeclaredSignatures,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    bool ok = true;
    for (const auto& [signature, defaults] : inheritedDefaults)
    {
        std::size_t undominatedDefaults = 0;
        for (const std::size_t candidateOrdinal : defaults)
        {
            if (candidateOrdinal >= memberModel_->members().size())
            {
                continue;
            }
            if (!inheritedDefaultIsDominated(defaults, candidateOrdinal))
                ++undominatedDefaults;
        }
        if (undominatedDefaults > 1U && !redeclaredSignatures.contains(signature))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOverride,
                diagnostics::DiagnosticSeverity::Error,
                nameLookup_->namedTypes()[typeOrdinal].range,
                "trait or interface inherits competing default implementations for '" + signature +
                    "'; redeclare the method to select or replace the default");
            ok = false;
        }
    }
    return ok;
}

bool ProjectInterfaceModel::visitInterface(
    std::size_t typeOrdinal,
    std::vector<VisitState>& states,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (typeOrdinal >= states.size())
    {
        return false;
    }
    if (states[typeOrdinal] == VisitState::Complete)
    {
        return true;
    }
    if (states[typeOrdinal] == VisitState::Failed)
    {
        return false;
    }
    if (states[typeOrdinal] == VisitState::Visiting)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InheritanceCycle,
            diagnostics::DiagnosticSeverity::Error,
            nameLookup_->namedTypes()[typeOrdinal].range,
            "interface inheritance cycle reaches '" + nameLookup_->namedTypes()[typeOrdinal].identity.key() + "'");
        states[typeOrdinal] = VisitState::Failed;
        return false;
    }

    InterfaceTypeInfo* info = const_cast<InterfaceTypeInfo*>(interfaceInfo(typeOrdinal));
    if (info == nullptr)
    {
        return false;
    }
    states[typeOrdinal] = VisitState::Visiting;
    std::unordered_map<std::string, std::size_t> signatureOwner;
    std::unordered_map<std::string, std::size_t> staticSignatureOwner;
    std::unordered_map<std::string, std::vector<std::size_t>> inheritedDefaults;
    std::unordered_set<std::string> redeclaredSignatures;

    bool ok = collectInheritedInterfaceMembers(
        typeOrdinal, *info, states, signatureOwner, staticSignatureOwner, inheritedDefaults, diagnostics);
    if (!collectDeclaredInterfaceMembers(
        typeOrdinal, *info, signatureOwner, staticSignatureOwner, redeclaredSignatures, diagnostics))
        ok = false;
    if (!validateInheritedDefaults(typeOrdinal, inheritedDefaults, redeclaredSignatures, diagnostics))
        ok = false;
    states[typeOrdinal] = ok ? VisitState::Complete : VisitState::Failed;
    return ok;
}

bool ProjectInterfaceModel::validateNonInterfaceSpecification(
    std::size_t candidateOrdinal,
    NamedTypeDeclarationKind kind,
    bool valueType,
    bool sawInterface,
    const ast::TypeSyntax& specification,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (valueType)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            specification.range(),
            "a struct cannot inherit class '" +
                nameLookup_->namedTypes()[candidateOrdinal].identity.key() +
                "'; only NativeSharp 3 traits are valid after ':'");
        return false;
    }
    if (kind == NamedTypeDeclarationKind::Class && sawInterface)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            specification.range(),
            "the single class base must precede interface specifications");
        return false;
    }
    return true;
}

bool ProjectInterfaceModel::validateDirectInterfaceTarget(
    std::size_t candidateOrdinal,
    bool valueType,
    bool explicitImpl,
    const InterfaceTypeInfo* info,
    const ast::TypeSyntax& specification,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (valueType && (info == nullptr || !info->isTrait))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            specification.range(),
            "a NativeSharp 3 struct may implement traits only; ordinary "
            "interfaces require class reference semantics");
        return false;
    }
    if (explicitImpl && (info == nullptr || !info->isTrait))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            specification.range(),
            "an impl block requires a NativeSharp 3 trait; '" +
                nameLookup_->namedTypes()[candidateOrdinal].identity.key() +
                "' is an ordinary interface");
        return false;
    }
    return true;
}

bool ProjectInterfaceModel::validateDirectInterfaceDuplicate(
    std::size_t classOrdinal,
    std::size_t candidateOrdinal,
    bool explicitImpl,
    const ast::TypeSyntax& specification,
    const std::unordered_map<std::size_t, ProjectInterfaceDirectSource>& directInterfaces,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const auto duplicate = directInterfaces.find(candidateOrdinal);
    if (duplicate == directInterfaces.end() ||
        (!explicitImpl && !duplicate->second.explicitImpl))
    {
        return true;
    }
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[classOrdinal];
    diagnostics.report(
        diagnostics::DiagnosticId::DuplicateSymbol,
        diagnostics::DiagnosticSeverity::Error,
        specification.range(),
        "duplicate trait impl for '" + nameLookup_->namedTypes()[candidateOrdinal].identity.key() +
            "' on class '" + type.identity.key() + "'");
    if (duplicate->second.syntax != nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::PreviousDeclaration,
            diagnostics::DiagnosticSeverity::Note,
            duplicate->second.syntax->range(),
            "previous trait implementation is here");
    }
    return false;
}

bool ProjectInterfaceModel::collectDirectInterface(
    std::size_t classOrdinal,
    bool valueType,
    const ast::TypeSyntax& specification,
    bool& sawInterface,
    std::unordered_map<std::size_t, ProjectInterfaceDirectSource>& directInterfaces,
    std::vector<std::size_t>& implemented,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::size_t> candidate = nameLookup_->resolvedTypeOrdinal(&specification);
    if (!candidate.has_value() || *candidate >= nameLookup_->namedTypes().size())
    {
        return true;
    }
    const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*candidate].kind;
    if (kind != NamedTypeDeclarationKind::Interface)
    {
        return validateNonInterfaceSpecification(
            *candidate, kind, valueType, sawInterface, specification, diagnostics);
    }
    const bool explicitImpl = specification.isExplicitTraitImplementation();
    const InterfaceTypeInfo* info = interfaceInfo(*candidate);
    if (!validateDirectInterfaceTarget(
        *candidate, valueType, explicitImpl, info, specification, diagnostics))
    {
        return false;
    }
    // The historical unsafe marker remains readable, but it is not a
    // caller- or implementation-side permission boundary in a native language.
    // Trait compatibility is determined by its actual method contracts.
    if (!validateDirectInterfaceDuplicate(
        classOrdinal, *candidate, explicitImpl, specification, directInterfaces, diagnostics))
    {
        return false;
    }
    directInterfaces.emplace(
        *candidate, ProjectInterfaceDirectSource{&specification, explicitImpl});
    sawInterface = true;
    appendUnique(implemented, *candidate);
    if (info != nullptr)
    {
        for (const std::size_t baseInterface : info->transitiveBaseInterfaceOrdinals)
        {
            appendUnique(implemented, baseInterface);
        }
    }
    return true;
}

bool ProjectInterfaceModel::collectDirectInterfaces(
    std::size_t classOrdinal,
    bool valueType,
    std::vector<std::size_t>& implemented,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    bool sawInterface = false;
    std::unordered_map<std::size_t, ProjectInterfaceDirectSource> directInterfaces;
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[classOrdinal];
    for (const NamedTypeDeclarationPart& part : type.parts)
    {
        for (const ast::TypeSyntax* specification : declarationSpecifications(part.syntax))
        {
            if (specification == nullptr)
            {
                continue;
            }
            if (!collectDirectInterface(
                classOrdinal,
                valueType,
                *specification,
                sawInterface,
                directInterfaces,
                implemented,
                diagnostics))
            {
                ok = false;
            }
        }
    }
    return ok;
}

bool ProjectInterfaceModel::implementationThrowsCompatible(
    const ClassMemberSymbol& implementation,
    const ClassMemberSymbol& requirement) const
{
    if (implementation.declaredErrorTypeKey.empty())
    {
        return true;
    }
    if (requirement.declaredErrorTypeKey.empty())
    {
        return false;
    }
    const std::optional<std::size_t> implementationError =
        inheritanceModel_->typeOrdinalForTypeKey(implementation.declaredErrorTypeKey);
    const std::optional<std::size_t> contractError =
        inheritanceModel_->typeOrdinalForTypeKey(requirement.declaredErrorTypeKey);
    return implementationError.has_value() && contractError.has_value() &&
        inheritanceModel_->isDerivedFrom(*implementationError, *contractError, true);
}

bool ProjectInterfaceModel::appendInterfaceMethodImplementation(
    std::size_t classOrdinal,
    std::size_t interfaceOrdinal,
    std::size_t interfaceMethodOrdinal,
    const NamedTypeDeclaration& type,
    const ClassMemberSymbol& interfaceMethod,
    ClassInterfaceTablePlan& plan,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::optional<std::size_t> implementation = findImplementation(classOrdinal, interfaceMethod);
    if (!implementation.has_value() &&
        interfaceMethod.syntax != nullptr && interfaceMethod.syntax->first() != nullptr)
    {
        implementation = interfaceMethodOrdinal;
    }
    plan.implementationMemberOrdinals.push_back(implementation);
    bool ok = true;
    if (implementation.has_value() && interfaceMethod.isNoHeap &&
        !memberModel_->members()[*implementation].isNoHeap)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            memberModel_->members()[*implementation].range,
            "NS3030: method '" + memberModel_->members()[*implementation].signatureKey +
                "' cannot implement noheap interface contract '" +
                interfaceMethod.signatureKey + "' without noheap");
        ok = false;
    }
    if (implementation.has_value())
    {
        const ClassMemberSymbol& concrete = memberModel_->members()[*implementation];
        if (!implementationThrowsCompatible(concrete, interfaceMethod))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOverride,
                diagnostics::DiagnosticSeverity::Error,
                concrete.range,
                "method '" + concrete.signatureKey + "' declares throws '" +
                    concrete.declaredErrorTypeKey + "' outside interface contract '" +
                    (interfaceMethod.declaredErrorTypeKey.empty()
                        ? std::string{"<non-throwing>"}
                        : interfaceMethod.declaredErrorTypeKey) + "'");
            ok = false;
        }
    }
    if (!implementation.has_value() && !type.isAbstract &&
        reportReplacedAllocatorSignature(classOrdinal, interfaceOrdinal, interfaceMethod, diagnostics))
        return false;
    if (!implementation.has_value() && !type.isAbstract)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "concrete class '" + type.identity.key() + "' does not implement interface method '" +
                interfaceMethod.signatureKey + "' from '" +
                nameLookup_->namedTypes()[interfaceOrdinal].identity.key() + "'");
        ok = false;
    }
    return ok;
}

// EN: Freeze 3.0 P6 (DEC-09). IAllocator keeps only the signature frozen by C-08. A class that still
//     implements one of the replaced members -- Allocate(int), Resize(byte*, int, int),
//     Deallocate(byte*) -- gets a diagnostic on that member naming the new signature, instead of the
//     generic "does not implement" on the class.
// ES: Congelamiento 3.0 P6 (DEC-09). IAllocator conserva solo la firma congelada por C-08. Una clase
//     que aun implementa uno de los miembros reemplazados -- Allocate(int), Resize(byte*, int, int),
//     Deallocate(byte*) -- recibe en ese miembro un diagnostico con la firma nueva, en lugar del
//     generico "does not implement" sobre la clase.
bool ProjectInterfaceModel::reportReplacedAllocatorSignature(
    std::size_t classOrdinal,
    std::size_t interfaceOrdinal,
    const ClassMemberSymbol& interfaceMethod,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    struct ReplacedSignature
    {
        std::string_view name;
        std::vector<std::string_view> oldParameters;
        std::string_view current;
    };
    static const std::array<ReplacedSignature, 3> replaced = {{
        {"Allocate", {"builtin:int"}, "byte* Allocate(long bytes, int alignment)"},
        {"Resize", {"builtin:byte*", "builtin:int", "builtin:int"},
            "byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment)"},
        {"Deallocate", {"builtin:byte*"}, "void Deallocate(byte* ptr, long bytes, int alignment)"}}};
    if (nameLookup_->namedTypes()[interfaceOrdinal].identity.key() !=
        "NativeSharpCore::NativeSharp.Core.IAllocator") return false;
    const auto entry = std::ranges::find(replaced, interfaceMethod.name, &ReplacedSignature::name);
    if (entry == replaced.end()) return false;
    for (const std::size_t ordinal : memberModel_->findMembers(classOrdinal, interfaceMethod.name))
    {
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.isStatic || member.parameters.size() != entry->oldParameters.size() ||
            !std::ranges::equal(member.parameters, entry->oldParameters, {},
                &MethodParameterSymbol::typeKey)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            member.range,
            "'" + member.signatureKey + "' implements the IAllocator signature replaced in 3.0 (C-08); "
                "implement '" + std::string(entry->current) + "'");
        return true;
    }
    return false;
}

void ProjectInterfaceModel::appendOperatorWitness(
    std::size_t classOrdinal,
    std::size_t interfaceOrdinal,
    std::size_t requirementOrdinal,
    std::size_t implementationOrdinal,
    const ClassMemberSymbol& requirement)
{
    auto& witnesses = operatorWitnessesByType_[classOrdinal];
    for (const TraitOperatorWitness& witness : witnesses)
    {
        if (witness.traitTypeOrdinal == interfaceOrdinal &&
            witness.requirementMemberOrdinal == requirementOrdinal &&
            witness.implementationMemberOrdinal == implementationOrdinal)
        {
            return;
        }
    }
    witnesses.push_back(TraitOperatorWitness{
        classOrdinal,
        interfaceOrdinal,
        requirementOrdinal,
        implementationOrdinal,
        requirement.operatorTraitKey});
}

bool ProjectInterfaceModel::validateStaticRequirementImplementation(
    std::size_t classOrdinal,
    std::size_t interfaceOrdinal,
    const NamedTypeDeclaration& type,
    std::size_t requirementOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (requirementOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& requirement = memberModel_->members()[requirementOrdinal];
    const std::optional<std::size_t> implementation =
        findStaticImplementation(classOrdinal, requirement);
    bool ok = true;
    if (!implementation.has_value() && !type.isAbstract)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOverride,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "concrete class '" + type.identity.key() +
                "' does not implement static trait requirement '" + requirement.signatureKey +
                "' from '" + nameLookup_->namedTypes()[interfaceOrdinal].identity.key() + "'");
        ok = false;
    }
    if (implementation.has_value())
    {
        const ClassMemberSymbol& concrete = memberModel_->members()[*implementation];
        if (!implementationThrowsCompatible(concrete, requirement))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOverride,
                diagnostics::DiagnosticSeverity::Error,
                concrete.range,
                "static method '" + concrete.signatureKey + "' declares throws '" +
                    concrete.declaredErrorTypeKey + "' outside trait contract '" +
                    (requirement.declaredErrorTypeKey.empty()
                        ? std::string{"<non-throwing>"}
                        : requirement.declaredErrorTypeKey) + "'");
            ok = false;
        }
    }
    if (implementation.has_value() && requirement.isUserOperator &&
        !requirement.operatorTraitKey.empty())
    {
        appendOperatorWitness(
            classOrdinal, interfaceOrdinal, requirementOrdinal, *implementation, requirement);
    }
    return ok;
}

bool ProjectInterfaceModel::buildInterfacePlan(
    std::size_t classOrdinal,
    bool valueType,
    std::size_t interfaceOrdinal,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const InterfaceTypeInfo* interface = interfaceInfo(interfaceOrdinal);
    if (interface == nullptr)
    {
        return true;
    }
    ClassInterfaceTablePlan plan;
    plan.classTypeOrdinal = classOrdinal;
    plan.interfaceTypeOrdinal = interfaceOrdinal;
    plan.stableInterfaceTypeId = stableTypeId(typeKeyForOrdinal(interfaceOrdinal));
    plan.interfaceMethodMemberOrdinals = interface->effectiveMethodMemberOrdinals;

    bool ok = true;
    for (const std::size_t methodOrdinal : interface->effectiveMethodMemberOrdinals)
    {
        const ClassMemberSymbol& interfaceMethod = memberModel_->members()[methodOrdinal];
        if (!appendInterfaceMethodImplementation(
            classOrdinal,
            interfaceOrdinal,
            methodOrdinal,
            type,
            interfaceMethod,
            plan,
            diagnostics))
            ok = false;
    }
    for (const std::size_t requirementOrdinal : interface->requiredStaticMemberOrdinals)
    {
        if (!validateStaticRequirementImplementation(
            classOrdinal, interfaceOrdinal, type, requirementOrdinal, diagnostics))
            ok = false;
    }
    if (!valueType)
    {
        const std::size_t planOrdinal = tablePlans_.size();
        tablePlans_.push_back(std::move(plan));
        if (classOrdinal < tablePlanOrdinalsByClass_.size())
            tablePlanOrdinalsByClass_[classOrdinal].push_back(planOrdinal);
    }
    return ok;
}

bool ProjectInterfaceModel::buildImplementedInterfacePlans(
    std::size_t classOrdinal,
    bool valueType,
    const std::vector<std::size_t>& implemented,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[classOrdinal];
    for (const std::size_t interfaceOrdinal : implemented)
    {
        if (!buildInterfacePlan(classOrdinal, valueType, interfaceOrdinal, type, diagnostics))
        {
            ok = false;
        }
    }
    return ok;
}

bool ProjectInterfaceModel::buildImplementingTypePlans(
    std::size_t classOrdinal,
    std::optional<std::size_t> baseOrdinal,
    bool valueType,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (installSnapshotImplementingType(classOrdinal)) return true;
    std::vector<std::size_t>& implemented = interfacesByClass_[classOrdinal];
    if (baseOrdinal.has_value() && *baseOrdinal < interfacesByClass_.size())
    {
        implemented = interfacesByClass_[*baseOrdinal];
    }
    bool ok = collectDirectInterfaces(classOrdinal, valueType, implemented, diagnostics);
    if (!buildImplementedInterfacePlans(classOrdinal, valueType, implemented, diagnostics))
    {
        ok = false;
    }
    return ok;
}

bool ProjectInterfaceModel::buildClassPlans(diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<ProjectInterfaceImplementingType> orderedTypes;
    orderedTypes.reserve(
        inheritanceModel_->classes().size() + nameLookup_->namedTypes().size());
    for (const ClassInheritanceInfo& classInfo : inheritanceModel_->classes())
    {
        orderedTypes.push_back(ProjectInterfaceImplementingType{
            classInfo.typeOrdinal, classInfo.baseTypeOrdinal, classInfo.depth, false});
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        if (nameLookup_->namedTypes()[ordinal].kind == NamedTypeDeclarationKind::Struct)
        {
            orderedTypes.push_back(ProjectInterfaceImplementingType{
                ordinal, std::nullopt, 0U, true});
        }
    }
    std::stable_sort(orderedTypes.begin(), orderedTypes.end(), implementingTypeDepthLess);

    // Base classes must publish their inherited interface set before derived classes copy it.
    // Sorting by the already-validated inheritance depth makes this independent of source order.
    // NativeSharp 3 structs participate in nominal trait constraints too, but remain value types:
    // they have no class base and receive no dynamic interface table from this phase.
    bool ok = true;
    for (const ProjectInterfaceImplementingType& implementingType : orderedTypes)
    {
        if (!buildImplementingTypePlans(
            implementingType.ordinal,
            implementingType.baseOrdinal,
            implementingType.valueType,
            diagnostics))
        {
            ok = false;
        }
    }
    return ok;
}

std::optional<std::size_t> ProjectInterfaceModel::findImplementation(
    std::size_t classTypeOrdinal,
    const ClassMemberSymbol& interfaceMethod) const noexcept
{
    std::optional<std::size_t> current = classTypeOrdinal;
    while (current.has_value())
    {
        for (const std::size_t memberOrdinal :
             memberModel_->findMembersBySignature(*current, interfaceMethod.signatureKey))
        {
            if (memberOrdinal >= memberModel_->members().size())
            {
                continue;
            }
            const ClassMemberSymbol& candidate = memberModel_->members()[memberOrdinal];
            if (candidate.kind != ClassMemberKind::Method)
            {
                continue;
            }
            if (candidate.isStatic || candidate.accessibility != ClassMemberAccessibility::Public ||
                candidate.declaredTypeKey != interfaceMethod.declaredTypeKey ||
                candidate.isReferenceReturn != interfaceMethod.isReferenceReturn ||
                isReadonlyReferenceReturn(candidate) != isReadonlyReferenceReturn(interfaceMethod) ||
                lifetimeContractKey(candidate) != lifetimeContractKey(interfaceMethod) ||
                candidate.isAbstract)
            {
                return std::nullopt;
            }
            return memberOrdinal;
        }
        current = inheritanceModel_->baseTypeOrdinal(*current);
    }
    return std::nullopt;
}

std::optional<std::size_t> ProjectInterfaceModel::findStaticImplementation(
    std::size_t classTypeOrdinal,
    const ClassMemberSymbol& requirement) const noexcept
{
    std::optional<std::size_t> current = classTypeOrdinal;
    while (current.has_value())
    {
        for (const std::size_t memberOrdinal :
             memberModel_->findMembersBySignature(*current, requirement.signatureKey))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[memberOrdinal];
            if (candidate.kind != ClassMemberKind::Method)
                continue;
            if (!candidate.isStatic ||
                candidate.accessibility != ClassMemberAccessibility::Public ||
                candidate.declaredTypeKey != requirement.declaredTypeKey ||
                candidate.isReferenceReturn != requirement.isReferenceReturn ||
                isReadonlyReferenceReturn(candidate) != isReadonlyReferenceReturn(requirement) ||
                lifetimeContractKey(candidate) != lifetimeContractKey(requirement) ||
                candidate.isAbstract)
                return std::nullopt;
            return memberOrdinal;
        }
        current = inheritanceModel_->baseTypeOrdinal(*current);
    }
    return std::nullopt;
}

std::uint64_t ProjectInterfaceModel::stableTypeId(std::string_view value) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= 1099511628211ULL;
    }
    return hash;
}
}
