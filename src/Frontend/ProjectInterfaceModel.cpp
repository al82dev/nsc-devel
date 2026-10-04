#include "nativesharp/Frontend/ProjectInterfaceModel.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <algorithm>
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

    bool ok = collectInterfaceBases(diagnostics);
    std::vector<VisitState> states(nameLookup.namedTypes().size(), VisitState::Unvisited);
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
    const std::string_view identity = typeKey.substr(5);
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        if (nameLookup_->namedTypes()[ordinal].identity.key() == identity)
        {
            return ordinal;
        }
    }
    return std::nullopt;
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

    bool ok = true;
    std::unordered_map<std::string, std::size_t> signatureOwner;
    std::unordered_map<std::string, std::size_t> staticSignatureOwner;
    std::unordered_map<std::string, std::vector<std::size_t>> inheritedDefaults;
    std::unordered_set<std::string> redeclaredSignatures;
    for (const std::size_t baseOrdinal : info->directBaseInterfaceOrdinals)
    {
        if (!visitInterface(baseOrdinal, states, diagnostics))
        {
            ok = false;
            continue;
        }
        appendUnique(info->transitiveBaseInterfaceOrdinals, baseOrdinal);
        const InterfaceTypeInfo* base = interfaceInfo(baseOrdinal);
        if (base == nullptr)
        {
            ok = false;
            continue;
        }
        for (const std::size_t transitive : base->transitiveBaseInterfaceOrdinals)
        {
            appendUnique(info->transitiveBaseInterfaceOrdinals, transitive);
        }
        for (const std::size_t memberOrdinal : base->effectiveMethodMemberOrdinals)
        {
            if (memberOrdinal >= memberModel_->members().size())
            {
                continue;
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
                info->effectiveMethodMemberOrdinals.push_back(memberOrdinal);
            }
            else
            {
                const ClassMemberSymbol& previous = memberModel_->members()[found->second];
                if (previous.declaredTypeKey != method.declaredTypeKey ||
                    previous.declaredErrorTypeKey != method.declaredErrorTypeKey ||
                    previous.isReferenceReturn != method.isReferenceReturn ||
                    lifetimeContractKey(previous) != lifetimeContractKey(method))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOverride,
                        diagnostics::DiagnosticSeverity::Error,
                        nameLookup_->namedTypes()[typeOrdinal].range,
                        "interface inherits incompatible return types for method '" + method.signatureKey + "'");
                    ok = false;
                }
                const bool previousDefault = previous.syntax != nullptr &&
                    previous.syntax->first() != nullptr;
                const bool methodDefault = method.syntax != nullptr &&
                    method.syntax->first() != nullptr;
                if (info->isTrait && methodDefault &&
                    (!previousDefault || interfaceDerivesFrom(
                        method.declaringTypeOrdinal,
                        previous.declaringTypeOrdinal)))
                {
                    std::ranges::replace(
                        info->effectiveMethodMemberOrdinals, found->second, memberOrdinal);
                    found->second = memberOrdinal;
                }
                else if (!previous.isNoHeap && method.isNoHeap)
                {
                    std::ranges::replace(
                        info->effectiveMethodMemberOrdinals, found->second, memberOrdinal);
                    found->second = memberOrdinal;
                }
            }
        }
        for (const std::size_t memberOrdinal : base->requiredStaticMemberOrdinals)
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& requirement = memberModel_->members()[memberOrdinal];
            const auto found = staticSignatureOwner.find(requirement.signatureKey);
            if (found == staticSignatureOwner.end())
            {
                staticSignatureOwner.emplace(requirement.signatureKey, memberOrdinal);
                info->requiredStaticMemberOrdinals.push_back(memberOrdinal);
            }
            else
            {
                const ClassMemberSymbol& previous = memberModel_->members()[found->second];
                if (previous.declaredTypeKey != requirement.declaredTypeKey ||
                    previous.declaredErrorTypeKey != requirement.declaredErrorTypeKey)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOverride,
                        diagnostics::DiagnosticSeverity::Error,
                        nameLookup_->namedTypes()[typeOrdinal].range,
                        "trait inherits incompatible static requirements for '" +
                            requirement.signatureKey + "'");
                    ok = false;
                }
            }
        }
    }

    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& method = memberModel_->members()[memberOrdinal];
        if (method.kind != ClassMemberKind::Method)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                method.range,
                "Stage 36 interface declaration spaces contain methods only");
            ok = false;
            continue;
        }
        const bool hasBody = method.syntax != nullptr && method.syntax->first() != nullptr;
        if (!info->isTrait && (method.isStatic ||
            method.accessibility != ClassMemberAccessibility::Public ||
            !method.isAbstract || method.syntax == nullptr || hasBody))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                method.range,
                "Stage 36 interface methods are public abstract instance signatures without bodies");
            ok = false;
        }

        if (info->isTrait && (method.accessibility != ClassMemberAccessibility::Public ||
            method.syntax == nullptr || method.isAbstract == hasBody))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                method.range,
                "trait members are public requirements without bodies or default methods with bodies");
            ok = false;
        }

        if (method.isStatic)
        {
            if (info->isTrait)
            {
                const auto found = staticSignatureOwner.find(method.signatureKey);
                if (found == staticSignatureOwner.end())
                {
                    staticSignatureOwner.emplace(method.signatureKey, memberOrdinal);
                    info->requiredStaticMemberOrdinals.push_back(memberOrdinal);
                }
                else
                {
                    const ClassMemberSymbol& previous = memberModel_->members()[found->second];
                    if (previous.declaredTypeKey != method.declaredTypeKey)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOverride,
                            diagnostics::DiagnosticSeverity::Error,
                            method.range,
                            "trait static requirement '" + method.signatureKey +
                                "' conflicts with its inherited return type");
                        ok = false;
                    }
                    else
                    {
                        std::ranges::replace(
                            info->requiredStaticMemberOrdinals, found->second, memberOrdinal);
                        found->second = memberOrdinal;
                    }
                }
            }
            continue;
        }

        const auto found = signatureOwner.find(method.signatureKey);
        if (found != signatureOwner.end())
        {
            const ClassMemberSymbol& previous = memberModel_->members()[found->second];
            if (previous.declaredTypeKey != method.declaredTypeKey ||
                previous.declaredErrorTypeKey != method.declaredErrorTypeKey ||
                previous.isReferenceReturn != method.isReferenceReturn ||
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
            else if (!previous.isNoHeap && method.isNoHeap)
            {
                std::ranges::replace(
                    info->effectiveMethodMemberOrdinals, found->second, memberOrdinal);
                found->second = memberOrdinal;
            }
            else if (info->isTrait)
            {
                std::ranges::replace(
                    info->effectiveMethodMemberOrdinals, found->second, memberOrdinal);
                found->second = memberOrdinal;
            }
            redeclaredSignatures.insert(method.signatureKey);
            // Redeclaring an inherited contract keeps its slot but replaces its contract/default.
            continue;
        }
        signatureOwner.emplace(method.signatureKey, memberOrdinal);
        info->effectiveMethodMemberOrdinals.push_back(memberOrdinal);
    }

    if (info->isTrait)
    {
        for (const auto& [signature, defaults] : inheritedDefaults)
        {
            std::size_t undominatedDefaults = 0;
            for (const std::size_t candidateOrdinal : defaults)
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                const bool dominated = std::ranges::any_of(
                    defaults,
                    [&](const std::size_t otherOrdinal)
                    {
                        if (otherOrdinal == candidateOrdinal ||
                            otherOrdinal >= memberModel_->members().size())
                            return false;
                        const ClassMemberSymbol& other = memberModel_->members()[otherOrdinal];
                        return interfaceDerivesFrom(
                            other.declaringTypeOrdinal,
                            candidate.declaringTypeOrdinal);
                    });
                if (!dominated) ++undominatedDefaults;
            }
            if (undominatedDefaults > 1U && !redeclaredSignatures.contains(signature))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    nameLookup_->namedTypes()[typeOrdinal].range,
                    "trait inherits competing default implementations for '" + signature +
                        "'; redeclare the method to select or replace the default");
                ok = false;
            }
        }
    }

    states[typeOrdinal] = ok ? VisitState::Complete : VisitState::Failed;
    return ok;
}

bool ProjectInterfaceModel::buildClassPlans(diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    struct ImplementingType final
    {
        std::size_t ordinal = 0;
        std::optional<std::size_t> baseOrdinal;
        std::size_t depth = 0;
        bool valueType = false;
    };
    std::vector<ImplementingType> orderedTypes;
    orderedTypes.reserve(
        inheritanceModel_->classes().size() + nameLookup_->namedTypes().size());
    for (const ClassInheritanceInfo& classInfo : inheritanceModel_->classes())
    {
        orderedTypes.push_back(ImplementingType{
            classInfo.typeOrdinal, classInfo.baseTypeOrdinal, classInfo.depth, false});
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        if (nameLookup_->namedTypes()[ordinal].kind == NamedTypeDeclarationKind::Struct)
            orderedTypes.push_back(ImplementingType{ordinal, std::nullopt, 0U, true});
    }
    std::stable_sort(orderedTypes.begin(), orderedTypes.end(),
        [](const ImplementingType& left, const ImplementingType& right)
        {
            return left.depth < right.depth;
        });

    // Base classes must publish their inherited interface set before derived classes copy it.
    // Sorting by the already-validated inheritance depth makes this independent of source order.
    // NativeSharp 3 structs participate in nominal trait constraints too, but remain value types:
    // they have no class base and receive no dynamic interface table from this phase.
    for (const ImplementingType& implementingType : orderedTypes)
    {
        const std::size_t classOrdinal = implementingType.ordinal;
        std::vector<std::size_t>& implemented = interfacesByClass_[classOrdinal];

        if (implementingType.baseOrdinal.has_value() &&
            *implementingType.baseOrdinal < interfacesByClass_.size())
        {
            implemented = interfacesByClass_[*implementingType.baseOrdinal];
        }

        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[classOrdinal];
        bool sawInterface = false;
        struct DirectInterfaceSource final
        {
            const ast::TypeSyntax* syntax = nullptr;
            bool explicitImpl = false;
        };
        std::unordered_map<std::size_t, DirectInterfaceSource> directInterfaces;
        for (const NamedTypeDeclarationPart& part : type.parts)
        {
            for (const ast::TypeSyntax* specification : declarationSpecifications(part.syntax))
            {
                if (specification == nullptr)
                {
                    continue;
                }
                const std::optional<std::size_t> candidate = nameLookup_->resolvedTypeOrdinal(specification);
                if (!candidate.has_value() || *candidate >= nameLookup_->namedTypes().size())
                {
                    continue;
                }
                const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*candidate].kind;
                if (kind == NamedTypeDeclarationKind::Interface)
                {
                    const bool explicitImpl = specification->isExplicitTraitImplementation();
                    const InterfaceTypeInfo* info = interfaceInfo(*candidate);
                    if (implementingType.valueType && (info == nullptr || !info->isTrait))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidBaseType,
                            diagnostics::DiagnosticSeverity::Error,
                            specification->range(),
                            "a NativeSharp 3 struct may implement traits only; ordinary "
                            "interfaces require class reference semantics");
                        ok = false;
                        continue;
                    }
                    if (explicitImpl && (info == nullptr || !info->isTrait))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidBaseType,
                            diagnostics::DiagnosticSeverity::Error,
                            specification->range(),
                            "an impl block requires a NativeSharp 3 trait; '" +
                                nameLookup_->namedTypes()[*candidate].identity.key() +
                                "' is an ordinary interface");
                        ok = false;
                        continue;
                    }
                    if (explicitImpl && info != nullptr && info->isTrait)
                    {
                        const ast::AstNode* traitSyntax =
                            nameLookup_->namedTypes()[*candidate].syntax;
                        const bool traitIsUnsafe =
                            traitSyntax != nullptr && traitSyntax->isUnsafe();
                        const bool implIsUnsafe =
                            part.syntax != nullptr && part.syntax->isUnsafe();
                        if (traitIsUnsafe != implIsUnsafe)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::NS3030MissingEffectContext,
                                diagnostics::DiagnosticSeverity::Error,
                                specification->range(),
                                traitIsUnsafe
                                    ? "implementing an unsafe trait requires 'unsafe impl'"
                                    : "'unsafe impl' is valid only for a trait declared unsafe");
                            ok = false;
                            continue;
                        }
                    }
                    if (const auto duplicate = directInterfaces.find(*candidate);
                        duplicate != directInterfaces.end() &&
                        (explicitImpl || duplicate->second.explicitImpl))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::DuplicateSymbol,
                            diagnostics::DiagnosticSeverity::Error,
                            specification->range(),
                            "duplicate trait impl for '" +
                                nameLookup_->namedTypes()[*candidate].identity.key() +
                                "' on class '" + type.identity.key() + "'");
                        if (duplicate->second.syntax != nullptr)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::PreviousDeclaration,
                                diagnostics::DiagnosticSeverity::Note,
                                duplicate->second.syntax->range(),
                                "previous trait implementation is here");
                        }
                        ok = false;
                        continue;
                    }
                    directInterfaces.emplace(
                        *candidate, DirectInterfaceSource{specification, explicitImpl});
                    sawInterface = true;
                    appendUnique(implemented, *candidate);
                    if (info != nullptr)
                    {
                        for (const std::size_t baseInterface : info->transitiveBaseInterfaceOrdinals)
                        {
                            appendUnique(implemented, baseInterface);
                        }
                    }
                }
                else if (implementingType.valueType)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidBaseType,
                        diagnostics::DiagnosticSeverity::Error,
                        specification->range(),
                        "a struct cannot inherit class '" +
                            nameLookup_->namedTypes()[*candidate].identity.key() +
                            "'; only NativeSharp 3 traits are valid after ':'");
                    ok = false;
                }
                else if (kind == NamedTypeDeclarationKind::Class && sawInterface)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidBaseType,
                        diagnostics::DiagnosticSeverity::Error,
                        specification->range(),
                        "the single class base must precede interface specifications");
                    ok = false;
                }
            }
        }

        for (const std::size_t interfaceOrdinal : implemented)
        {
            const InterfaceTypeInfo* interface = interfaceInfo(interfaceOrdinal);
            if (interface == nullptr)
            {
                continue;
            }
            ClassInterfaceTablePlan plan;
            plan.classTypeOrdinal = classOrdinal;
            plan.interfaceTypeOrdinal = interfaceOrdinal;
            plan.stableInterfaceTypeId = stableTypeId(typeKeyForOrdinal(interfaceOrdinal));
            plan.interfaceMethodMemberOrdinals = interface->effectiveMethodMemberOrdinals;

            for (const std::size_t interfaceMethodOrdinal : interface->effectiveMethodMemberOrdinals)
            {
                const ClassMemberSymbol& interfaceMethod = memberModel_->members()[interfaceMethodOrdinal];
                std::optional<std::size_t> implementation =
                    findImplementation(classOrdinal, interfaceMethod);
                if (!implementation.has_value() && interface->isTrait &&
                    interfaceMethod.syntax != nullptr && interfaceMethod.syntax->first() != nullptr)
                {
                    implementation = interfaceMethodOrdinal;
                }
                plan.implementationMemberOrdinals.push_back(implementation);
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
                    const ClassMemberSymbol& concrete =
                        memberModel_->members()[*implementation];
                    bool compatibleThrows = concrete.declaredErrorTypeKey.empty();
                    if (!compatibleThrows && !interfaceMethod.declaredErrorTypeKey.empty())
                    {
                        const std::optional<std::size_t> concreteError =
                            inheritanceModel_->typeOrdinalForTypeKey(
                                concrete.declaredErrorTypeKey);
                        const std::optional<std::size_t> contractError =
                            inheritanceModel_->typeOrdinalForTypeKey(
                                interfaceMethod.declaredErrorTypeKey);
                        compatibleThrows = concreteError.has_value() &&
                            contractError.has_value() &&
                            inheritanceModel_->isDerivedFrom(
                                *concreteError, *contractError, true);
                    }
                    if (!compatibleThrows)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOverride,
                            diagnostics::DiagnosticSeverity::Error,
                            concrete.range,
                            "method '" + concrete.signatureKey + "' declares throws '" +
                                concrete.declaredErrorTypeKey +
                                "' outside interface contract '" +
                                (interfaceMethod.declaredErrorTypeKey.empty()
                                    ? std::string{"<non-throwing>"}
                                    : interfaceMethod.declaredErrorTypeKey) + "'");
                        ok = false;
                    }
                }
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
            }
            for (const std::size_t requirementOrdinal : interface->requiredStaticMemberOrdinals)
            {
                if (requirementOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& requirement = memberModel_->members()[requirementOrdinal];
                const std::optional<std::size_t> implementation =
                    findStaticImplementation(classOrdinal, requirement);
                if (!implementation.has_value() &&
                    !type.isAbstract)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOverride,
                        diagnostics::DiagnosticSeverity::Error,
                        type.range,
                        "concrete class '" + type.identity.key() +
                            "' does not implement static trait requirement '" +
                            requirement.signatureKey + "' from '" +
                            nameLookup_->namedTypes()[interfaceOrdinal].identity.key() + "'");
                    ok = false;
                }
                if (implementation.has_value())
                {
                    const ClassMemberSymbol& concrete =
                        memberModel_->members()[*implementation];
                    bool compatibleThrows = concrete.declaredErrorTypeKey.empty();
                    if (!compatibleThrows && !requirement.declaredErrorTypeKey.empty())
                    {
                        const std::optional<std::size_t> concreteError =
                            inheritanceModel_->typeOrdinalForTypeKey(
                                concrete.declaredErrorTypeKey);
                        const std::optional<std::size_t> contractError =
                            inheritanceModel_->typeOrdinalForTypeKey(
                                requirement.declaredErrorTypeKey);
                        compatibleThrows = concreteError.has_value() &&
                            contractError.has_value() &&
                            inheritanceModel_->isDerivedFrom(
                                *concreteError, *contractError, true);
                    }
                    if (!compatibleThrows)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOverride,
                            diagnostics::DiagnosticSeverity::Error,
                            concrete.range,
                            "static method '" + concrete.signatureKey +
                                "' declares throws '" + concrete.declaredErrorTypeKey +
                                "' outside trait contract '" +
                                (requirement.declaredErrorTypeKey.empty()
                                    ? std::string{"<non-throwing>"}
                                    : requirement.declaredErrorTypeKey) + "'");
                        ok = false;
                    }
                }
                if (implementation.has_value() && requirement.isUserOperator &&
                    !requirement.operatorTraitKey.empty())
                {
                    auto& witnesses = operatorWitnessesByType_[classOrdinal];
                    const bool duplicate = std::ranges::any_of(
                        witnesses,
                        [&](const TraitOperatorWitness& witness)
                        {
                            return witness.traitTypeOrdinal == interfaceOrdinal &&
                                witness.requirementMemberOrdinal == requirementOrdinal &&
                                witness.implementationMemberOrdinal == *implementation;
                        });
                    if (!duplicate)
                    {
                        witnesses.push_back(TraitOperatorWitness{
                            classOrdinal,
                            interfaceOrdinal,
                            requirementOrdinal,
                            *implementation,
                            requirement.operatorTraitKey});
                    }
                }
            }
            if (!implementingType.valueType) tablePlans_.push_back(std::move(plan));
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
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(*current))
        {
            if (memberOrdinal >= memberModel_->members().size())
            {
                continue;
            }
            const ClassMemberSymbol& candidate = memberModel_->members()[memberOrdinal];
            if (candidate.kind != ClassMemberKind::Method || candidate.signatureKey != interfaceMethod.signatureKey)
            {
                continue;
            }
            if (candidate.isStatic || candidate.accessibility != ClassMemberAccessibility::Public ||
                candidate.declaredTypeKey != interfaceMethod.declaredTypeKey ||
                candidate.isReferenceReturn != interfaceMethod.isReferenceReturn ||
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
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(*current))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[memberOrdinal];
            if (candidate.kind != ClassMemberKind::Method ||
                candidate.signatureKey != requirement.signatureKey)
                continue;
            if (!candidate.isStatic ||
                candidate.accessibility != ClassMemberAccessibility::Public ||
                candidate.declaredTypeKey != requirement.declaredTypeKey ||
                candidate.isReferenceReturn != requirement.isReferenceReturn ||
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
