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

    for (std::size_t ordinal = 0; ordinal < nameLookup.namedTypes().size(); ++ordinal)
    {
        if (nameLookup.namedTypes()[ordinal].kind != NamedTypeDeclarationKind::Interface)
        {
            continue;
        }
        interfaceInfoByType_[ordinal] = interfaces_.size();
        InterfaceTypeInfo info;
        info.typeOrdinal = ordinal;
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
                    previous.isReferenceReturn != method.isReferenceReturn)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOverride,
                        diagnostics::DiagnosticSeverity::Error,
                        nameLookup_->namedTypes()[typeOrdinal].range,
                        "interface inherits incompatible return types for method '" + method.signatureKey + "'");
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
        if (method.isStatic || method.accessibility != ClassMemberAccessibility::Public ||
            !method.isAbstract || method.syntax == nullptr || method.syntax->first() != nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                method.range,
                "Stage 36 interface methods are public abstract instance signatures without bodies");
            ok = false;
        }

        const auto found = signatureOwner.find(method.signatureKey);
        if (found != signatureOwner.end())
        {
            const ClassMemberSymbol& previous = memberModel_->members()[found->second];
            if (previous.declaredTypeKey != method.declaredTypeKey ||
                previous.isReferenceReturn != method.isReferenceReturn)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOverride,
                    diagnostics::DiagnosticSeverity::Error,
                    method.range,
                    "interface method '" + method.signatureKey + "' conflicts with an inherited return type");
                ok = false;
            }
            // Redeclaring an identical inherited contract does not allocate another interface slot.
            continue;
        }
        signatureOwner.emplace(method.signatureKey, memberOrdinal);
        info->effectiveMethodMemberOrdinals.push_back(memberOrdinal);
    }

    states[typeOrdinal] = ok ? VisitState::Complete : VisitState::Failed;
    return ok;
}

bool ProjectInterfaceModel::buildClassPlans(diagnostics::DiagnosticEngine& diagnostics)
{
    bool ok = true;
    std::vector<const ClassInheritanceInfo*> orderedClasses;
    orderedClasses.reserve(inheritanceModel_->classes().size());
    for (const ClassInheritanceInfo& classInfo : inheritanceModel_->classes())
    {
        orderedClasses.push_back(&classInfo);
    }
    std::stable_sort(orderedClasses.begin(), orderedClasses.end(),
        [](const ClassInheritanceInfo* left, const ClassInheritanceInfo* right)
        {
            return left->depth < right->depth;
        });

    // Base classes must publish their inherited interface set before derived classes copy it.
    // Sorting by the already-validated inheritance depth makes this independent of source order.
    for (const ClassInheritanceInfo* classInfoPointer : orderedClasses)
    {
        const ClassInheritanceInfo& classInfo = *classInfoPointer;
        const std::size_t classOrdinal = classInfo.typeOrdinal;
        std::vector<std::size_t>& implemented = interfacesByClass_[classOrdinal];

        if (classInfo.baseTypeOrdinal.has_value() && *classInfo.baseTypeOrdinal < interfacesByClass_.size())
        {
            implemented = interfacesByClass_[*classInfo.baseTypeOrdinal];
        }

        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[classOrdinal];
        bool sawInterface = false;
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
                    sawInterface = true;
                    appendUnique(implemented, *candidate);
                    const InterfaceTypeInfo* info = interfaceInfo(*candidate);
                    if (info != nullptr)
                    {
                        for (const std::size_t baseInterface : info->transitiveBaseInterfaceOrdinals)
                        {
                            appendUnique(implemented, baseInterface);
                        }
                    }
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
                const std::optional<std::size_t> implementation = findImplementation(classOrdinal, interfaceMethod);
                plan.implementationMemberOrdinals.push_back(implementation);
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
            tablePlans_.push_back(std::move(plan));
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
                candidate.isReferenceReturn != interfaceMethod.isReferenceReturn || candidate.isAbstract)
            {
                return std::nullopt;
            }
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
