#include "nativesharp/Frontend/ProjectConstructionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{

void ProjectConstructionModel::bindImplicitDefaultConstructors(
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr)
    {
        return;
    }

    std::vector<std::pair<std::size_t, std::size_t>> orderedTypes;
    orderedTypes.reserve(inheritanceModel_->classes().size());
    for (const ClassInheritanceInfo& info : inheritanceModel_->classes())
    {
        orderedTypes.emplace_back(info.depth, info.typeOrdinal);
    }
    std::sort(orderedTypes.begin(), orderedTypes.end());

    for (const auto& [depth, typeOrdinal] : orderedTypes)
    {
        (void)depth;
        if (typeOrdinal < nameLookup_->namedTypes().size() && nameLookup_->namedTypes()[typeOrdinal].isPrecompiled)
        {
            // A fieldless precompiled class with no published constructor has the same
            // semantic zero-work default construction as its producer: allocate, install the
            // descriptor, and follow any fieldless implicit base chain. Keep skipping classes
            // with instance storage because NSMETA does not transport their source initializer
            // expressions; inventing those in the consumer would be incorrect.
            const bool hasInstanceStorage = std::ranges::any_of(
                memberModel_->membersOfType(typeOrdinal), [&](const std::size_t memberOrdinal) {
                    return memberOrdinal < memberModel_->members().size() &&
                           memberModel_->members()[memberOrdinal].kind == ClassMemberKind::Field &&
                           !memberModel_->members()[memberOrdinal].isStatic;
                });
            if (hasInstanceStorage) continue;
        }
        if (memberModel_->constructorsOfType(typeOrdinal).empty())
        {
            bindImplicitDefaultConstructor(typeOrdinal, diagnostics);
        }
    }
}

void ProjectConstructionModel::bindExplicitBaseDefaultConstructor(
    std::size_t typeOrdinal,
    std::span<const std::size_t> baseConstructors,
    ImplicitDefaultConstructorBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::optional<std::size_t> parameterless;
    for (const std::size_t memberOrdinal : baseConstructors)
    {
        const ClassMemberSymbol& constructor = memberModel_->members()[memberOrdinal];
        if (constructor.parameters.empty())
        {
            parameterless = memberOrdinal;
            break;
        }
    }
    if (!parameterless.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            nameLookup_->namedTypes()[typeOrdinal].range,
            "implicit constructor for class '" +
                nameLookup_->namedTypes()[typeOrdinal].identity.fullyQualifiedName() +
                "' cannot chain to its base class because the base declares no parameterless constructor");
        binding.resolution = ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor;
    }
    else if (!constructorAccessibleFromType(
                 memberModel_->members()[*parameterless], typeOrdinal, true))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InaccessibleMember,
            diagnostics::DiagnosticSeverity::Error,
            nameLookup_->namedTypes()[typeOrdinal].range,
            "implicit constructor for class '" +
                nameLookup_->namedTypes()[typeOrdinal].identity.fullyQualifiedName() +
                "' cannot access the parameterless constructor of its base class");
        binding.resolution = ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor;
    }
    else
    {
        binding.resolution = ImplicitDefaultConstructorResolutionKind::ResolvedExplicitBaseEntry;
        binding.directBaseConstructorMemberOrdinal = parameterless;
        binding.entryConstructorMemberOrdinal = parameterless;
    }
}

void ProjectConstructionModel::bindImplicitBaseDefaultConstructor(
    std::size_t typeOrdinal,
    std::size_t baseTypeOrdinal,
    ImplicitDefaultConstructorBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // The direct base also owns a semantic-only default constructor. Since Stage 34 has no
    // instance-field initializers yet, an intermediate implicit constructor has no executable
    // body of its own. We therefore validate every inheritance edge but may physically elide
    // zero-work implicit constructors and call the nearest explicit ancestor entry point.
    if (!implicitDefaultAccessibleFromType(baseTypeOrdinal, typeOrdinal, true))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InaccessibleMember,
            diagnostics::DiagnosticSeverity::Error,
            nameLookup_->namedTypes()[typeOrdinal].range,
            "implicit constructor for class '" +
                nameLookup_->namedTypes()[typeOrdinal].identity.fullyQualifiedName() +
                "' cannot access the implicit constructor of its base class");
        binding.resolution = ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor;
        return;
    }

    const ImplicitDefaultConstructorBinding* baseBinding =
        implicitDefaultConstructorBindingForType(baseTypeOrdinal);
    if (baseBinding == nullptr)
    {
        binding.resolution = ImplicitDefaultConstructorResolutionKind::Invalid;
    }
    else if (baseBinding->resolution ==
                 ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor ||
             baseBinding->resolution ==
                 ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor ||
             baseBinding->resolution == ImplicitDefaultConstructorResolutionKind::Invalid)
    {
        // The base declaration already emitted the primary diagnostic. Mark the derived
        // chain invalid without multiplying cascade diagnostics.
        binding.resolution = ImplicitDefaultConstructorResolutionKind::Invalid;
    }
    else
    {
        binding.resolution = ImplicitDefaultConstructorResolutionKind::ResolvedImplicitBaseChain;
        binding.entryConstructorMemberOrdinal = baseBinding->entryConstructorMemberOrdinal;
    }
}

void ProjectConstructionModel::bindImplicitDefaultConstructor(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        typeOrdinal >= nameLookup_->namedTypes().size() ||
        typeOrdinal >= implicitDefaultBindingByType_.size())
    {
        return;
    }
    if (implicitDefaultBindingByType_[typeOrdinal].has_value())
    {
        return;
    }
    // EN: A class of a restored API unit with a snapshot entry takes its binding from it (plan 3.11).
    // ES: Una clase de una unidad de API restaurada con entrada en el snapshot toma de ella su binding
    //     (plan 3.11).
    if (installSnapshotImplicitDefault(typeOrdinal)) return;

    ImplicitDefaultConstructorBinding binding;
    binding.typeOrdinal = typeOrdinal;
    binding.baseTypeOrdinal = inheritanceModel_->baseTypeOrdinal(typeOrdinal);

    if (!binding.baseTypeOrdinal.has_value())
    {
        binding.resolution = ImplicitDefaultConstructorResolutionKind::NoBaseRequired;
        implicitDefaultBindingByType_[typeOrdinal] = implicitDefaultConstructorBindings_.size();
        implicitDefaultConstructorBindings_.push_back(std::move(binding));
        return;
    }

    const std::size_t baseTypeOrdinal = *binding.baseTypeOrdinal;
    const std::span<const std::size_t> baseConstructors =
        memberModel_->constructorsOfType(baseTypeOrdinal);

    if (!baseConstructors.empty())
    {
        bindExplicitBaseDefaultConstructor(
            typeOrdinal, baseConstructors, binding, diagnostics);
    }
    else
    {
        bindImplicitBaseDefaultConstructor(
            typeOrdinal, baseTypeOrdinal, binding, diagnostics);
    }

    implicitDefaultBindingByType_[typeOrdinal] = implicitDefaultConstructorBindings_.size();
    implicitDefaultConstructorBindings_.push_back(std::move(binding));
}

}
