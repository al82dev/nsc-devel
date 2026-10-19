#include "nativesharp/Frontend/ProjectConstructionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{

void ProjectConstructionModel::initializeConstructionAllocator(
    const ProjectConstructionSite& site,
    ObjectConstructionBinding& binding) const
{
    binding.allocatorSelection =
        site.kind == ProjectConstructionSiteKind::Allocator;
    binding.allocatorTypeKey = site.allocatorTypeKey;
    binding.allocatorAllocateMemberOrdinal = site.allocatorAllocateMemberOrdinal;
    binding.allocatorDeallocateMemberOrdinal = site.allocatorDeallocateMemberOrdinal;
    binding.allocatorFailureProfile = site.allocatorFailureProfile;
    binding.allocatorResultOkFactoryMemberOrdinal = site.allocatorResultOkFactoryMemberOrdinal;
    binding.allocatorResultErrorFactoryMemberOrdinal = site.allocatorResultErrorFactoryMemberOrdinal;
    binding.allocatorResultCarrierTypeKey = site.allocatorResultCarrierTypeKey;
}

bool ProjectConstructionModel::initializeConstructionBinding(
    const ProjectConstructionSite& site,
    ObjectConstructionBinding& binding,
    std::optional<std::size_t>& targetOrdinal,
    bool& targetIsStruct,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* expression = site.syntax;

    // NS304-T09-F07: `new()` target-typed llega sin TypeSyntax; el tipo ya está resuelto en
    // site.targetTypeOrdinal. Se permite avanzar solo en ese caso.
    const bool targetTyped = expression != nullptr && expression->typeSyntax() == nullptr &&
        expression->auxiliaryText() == "target-typed";
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        methodBodyModel_ == nullptr || expression == nullptr ||
        (expression->typeSyntax() == nullptr && !targetTyped))
    {
        return false;
    }

    if (!targetTyped && !expression->typeSyntax()->modifiers().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->typeSyntax()->range(),
            "object construction requires an unmodified class type; pointer construction is not object creation");
        return false;
    }

    // C6 "struct members": `new T(args)` now also accepts a STRUCT. On a class it means
    // "allocate on the heap and run the constructor on it"; on a struct it means "initialize
    // storage in place and run the constructor on it" -- no allocation, no descriptor, nothing
    // to free. The two share this entire routine because everything here is about WHICH
    // constructor runs (candidates, overload resolution, argument conversions), and none of that
    // depends on where the object lives.
    targetOrdinal = site.targetTypeOrdinal;
    const bool targetIsClass = targetOrdinal.has_value() &&
        *targetOrdinal < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*targetOrdinal].kind == NamedTypeDeclarationKind::Class;
    targetIsStruct = targetOrdinal.has_value() &&
        *targetOrdinal < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*targetOrdinal].kind == NamedTypeDeclarationKind::Struct;
    if (!targetIsClass && !targetIsStruct)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "new T(args) requires T to resolve to a class or struct type");
        return false;
    }

    binding.targetIsStruct = targetIsStruct;
    initializeConstructionAllocator(site, binding);
    binding.targetTypeOrdinal = *targetOrdinal;
    binding.targetTypeKey = site.targetTypeKey;
    if (nameLookup_->namedTypes()[*targetOrdinal].isAbstract)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot instantiate abstract class '" +
                nameLookup_->namedTypes()[*targetOrdinal].identity.key() + "'");
        binding.resolution = ConstructionResolutionKind::InvalidTarget;
        return false;
    }
    if (nameLookup_->namedTypes()[*targetOrdinal].isStatic)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot instantiate static class '" +
                nameLookup_->namedTypes()[*targetOrdinal].identity.key() + "'");
        binding.resolution = ConstructionResolutionKind::InvalidTarget;
        return false;
    }
    return true;
}

void ProjectConstructionModel::bindImplicitConstruction(
    ObjectConstructionBinding& binding,
    std::size_t targetOrdinal,
    std::size_t callerMemberOrdinal,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    bool targetIsStruct,
    const ast::AstNode* expression,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    ConstructorCandidateBinding candidate;
    candidate.implicitDefault = true;
    candidate.accessible = implicitDefaultAccessible(targetOrdinal, callerMemberOrdinal, false);
    candidate.viable = candidate.accessible && argumentTypes.empty();
    binding.evaluatedCandidates.push_back(candidate);

    if (!candidate.accessible)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "implicit parameterless constructor is not accessible from this module");
        binding.resolution = ConstructionResolutionKind::Inaccessible;
    }
    else if (!candidate.viable)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::string(targetIsStruct ? "struct" : "class") +
                " has only an implicit parameterless constructor");
        binding.resolution = ConstructionResolutionKind::NoViableCandidate;
    }
    else if (targetIsStruct)
    {
        // C6 "struct members": `new Vec2()` on a struct with no declared constructor. The
        // class path below has to walk the implicit base-constructor CHAIN to decide whether a
        // parameterless construction is even possible; a struct has no base, so there is no
        // chain and nothing that could make it impossible. Its default is the one C guarantees
        // for a zeroed aggregate: every field at its zero value, which the backend produces
        // with a memset before any field initializer runs.
        binding.resolution = ConstructionResolutionKind::ResolvedImplicitDefault;
        binding.selectedImplicitDefault = true;
    }
    else
    {
        const ImplicitDefaultConstructorBinding* implicit =
            implicitDefaultConstructorBindingForType(targetOrdinal);
        const bool implicitChainValid = implicit != nullptr &&
            implicit->resolution != ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor &&
            implicit->resolution != ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor &&
            implicit->resolution != ImplicitDefaultConstructorResolutionKind::Invalid;
        if (!implicitChainValid)
        {
            binding.resolution = ConstructionResolutionKind::NoViableCandidate;
        }
        else
        {
            binding.resolution = ConstructionResolutionKind::ResolvedImplicitDefault;
            binding.selectedImplicitDefault = true;
            binding.implicitDefaultBaseConstructorMemberOrdinal =
                implicit->entryConstructorMemberOrdinal;
        }
    }
}

void ProjectConstructionModel::evaluateConstructionCandidates(
    ObjectConstructionBinding& binding,
    std::size_t callerMemberOrdinal,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    std::size_t constructorArgumentCount,
    bool& anyAccessible,
    bool& anyMatchingArity,
    bool& accessibleMatchingArity) const
{
    const ProjectConversionClassifier conversions(*inheritanceModel_, interfaceModel_, memberModel_);
    for (const std::size_t constructorOrdinal : binding.explicitConstructorMemberOrdinals)
    {
        const ClassMemberSymbol& constructor = memberModel_->members()[constructorOrdinal];
        const bool accessible = constructorAccessible(
            constructor, callerMemberOrdinal, false);
        bool matchingArity = false;
        // NSC-LIMIT-010. PRIMERO LA FISICA, DESPUES EL CONTRATO, y en ese orden.
        //
        // Un candidato fisicamente imposible no llega a tener opinion sobre propiedad: si la
        // conversion no existe, el argumento ni siquiera cabe en ese parametro, y dejar que
        // hable el calificador produciria un diagnostico sobre `move` cuando el problema es el
        // tipo. El veredicto se calcula igualmente para que `ownershipFits` tenga exactamente
        // un elemento por argumento --el diagnostico indexa por posicion-- pero solo tumba al
        // candidato cuando la fisica ya lo habia aceptado.
        ConstructorCandidateBinding candidate = evaluateConstructorCandidate(
            constructor,
            constructorOrdinal,
            accessible,
            std::span<ast::AstNode* const>(
                binding.syntax->children().data(), constructorArgumentCount),
            argumentTypes,
            conversions,
            matchingArity);
        anyAccessible = anyAccessible || candidate.accessible;
        anyMatchingArity = anyMatchingArity || matchingArity;
        accessibleMatchingArity = accessibleMatchingArity ||
            (candidate.accessible && matchingArity);
        binding.evaluatedCandidates.push_back(std::move(candidate));
    }
}

bool ProjectConstructionModel::resolveConstructionAccessibility(
    ObjectConstructionBinding& binding,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    bool anyAccessible,
    bool anyMatchingArity,
    bool accessibleMatchingArity,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!anyAccessible)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            binding.syntax->range(),
            std::string(binding.targetIsStruct ? "struct" : "class") +
                " has constructors, but none are accessible from this construction site");
        binding.resolution = ConstructionResolutionKind::Inaccessible;
        return false;
    }
    // C6 "struct members": a struct ALWAYS has a parameterless construction available, whether or
    // not it declares constructors of its own. This is where a struct and a class genuinely
    // differ, and the reason is not convenience: `Vector3 v;` already produces the zeroed value,
    // so refusing `new Vector3()` protects no invariant -- it only forces the same value to be
    // written a different way. C# settles it the same way and for the same reason. On a class,
    // declaring a constructor still removes the implicit default, because a class reference has
    // no meaningful "just zeroed" state to fall back on.
    if (!accessibleMatchingArity && binding.targetIsStruct && argumentTypes.empty())
    {
        binding.resolution = ConstructionResolutionKind::ResolvedImplicitDefault;
        binding.selectedImplicitDefault = true;
        return false;
    }
    if (accessibleMatchingArity)
    {
        return true;
    }
    if (anyMatchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            binding.syntax->range(),
            "matching constructor exists but is not accessible from this construction site");
        binding.resolution = ConstructionResolutionKind::Inaccessible;
    }
    else
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            binding.syntax->range(),
            "no accessible constructor accepts " +
                std::to_string(argumentTypes.size()) + " argument(s)");
        binding.resolution = ConstructionResolutionKind::NoViableCandidate;
    }
    return false;
}

void ProjectConstructionModel::selectConstructionCandidate(
    ObjectConstructionBinding& binding,
    std::size_t selectedIndex) const
{
    const ConstructorCandidateBinding& selected = binding.evaluatedCandidates[selectedIndex];
    binding.resolution = ConstructionResolutionKind::ResolvedExplicit;
    binding.selectedConstructorMemberOrdinal = selected.memberOrdinal;
    binding.selectedConversions = selected.conversions;
    binding.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    binding.selectedParameterPackElements = selected.parameterPackElements;
    binding.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    binding.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    binding.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    const ClassMemberSymbol& selectedSymbol = memberModel_->members()[*selected.memberOrdinal];
    for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
    {
        binding.selectedDefaultArguments.push_back(
            selectedSymbol.parameters[parameterOrdinal].defaultValueSyntax);
        binding.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
    }
    // NSC-LIMIT-010: las transferencias del candidato elegido viajan al binding final.
    binding.selectedOwnershipTransfers = selected.ownershipTransfers;
    binding.selectedFullExpressionBorrowArgumentOrdinals =
        selected.fullExpressionBorrowArgumentOrdinals;
}

void ProjectConstructionModel::resolveConstructionCandidates(
    ObjectConstructionBinding& binding,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::vector<std::size_t> viable =
        viableConstructorCandidates(binding.evaluatedCandidates);
    if (viable.empty())
    {
        // NSC-LIMIT-010. Si el unico estorbo fue el contrato de propiedad, se dice cual y
        // donde en vez del generico. La explicacion la da explainOwnershipRejection, que la
        // comparten este camino y el de `this(...)`/`base(...)`.
        if (explainOwnershipRejection(
                binding.evaluatedCandidates, argumentTypes.size(), binding.syntax, binding.syntax,
                diagnostics))
        {
            binding.resolution = ConstructionResolutionKind::NoViableCandidate;
            constructions_.push_back(std::move(binding));
            return;
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            binding.syntax->range(),
            "no viable constructor accepts the argument types");
        binding.resolution = ConstructionResolutionKind::NoViableCandidate;
        constructions_.push_back(std::move(binding));
        return;
    }

    const std::vector<std::size_t> nonDominated =
        nonDominatedConstructorCandidates(binding.evaluatedCandidates, viable);
    if (nonDominated.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            binding.syntax->range(),
            "ambiguous constructor invocation");
        binding.resolution = ConstructionResolutionKind::Ambiguous;
        constructions_.push_back(std::move(binding));
        return;
    }

    selectConstructionCandidate(binding, nonDominated.front());
    constructions_.push_back(std::move(binding));
}

void ProjectConstructionModel::bindConstruction(
    const ProjectConstructionSite& site,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = site.syntax;
    const std::size_t callerMemberOrdinal = site.callableOrdinal;
    ObjectConstructionBinding binding;
    binding.syntax = expression;
    binding.callerMemberOrdinal = callerMemberOrdinal;

    std::optional<std::size_t> targetOrdinal;
    bool targetIsStruct = false;
    if (!initializeConstructionBinding(
            site, binding, targetOrdinal, targetIsStruct, diagnostics))
    {
        constructions_.push_back(std::move(binding));
        return;
    }
    const std::span<const std::size_t> constructors =
        memberModel_->constructorsOfType(*targetOrdinal);
    binding.explicitConstructorMemberOrdinals.assign(constructors.begin(), constructors.end());

    const std::vector<MethodExpressionTypeBinding>& argumentTypes = site.argumentTypes;
    // NS304-T13-F06: the trailing `trailing: capacity` child is not a constructor argument.
    const bool trailingConstruction = expression->auxiliaryText() == "trailing";
    const std::size_t constructorArgumentCount =
        expression->children().size() - (trailingConstruction ? 1U : 0U);
    if (!site.argumentsTyped || argumentTypes.size() != constructorArgumentCount)
    {
        binding.resolution = ConstructionResolutionKind::NoViableCandidate;
        constructions_.push_back(std::move(binding));
        return;
    }

    if (binding.explicitConstructorMemberOrdinals.empty())
    {
        bindImplicitConstruction(
            binding,
            *targetOrdinal,
            callerMemberOrdinal,
            argumentTypes,
            targetIsStruct,
            expression,
            diagnostics);
        constructions_.push_back(std::move(binding));
        return;
    }
    bool anyAccessible = false;
    bool anyMatchingArity = false;
    bool accessibleMatchingArity = false;
    evaluateConstructionCandidates(
        binding,
        callerMemberOrdinal,
        argumentTypes,
        constructorArgumentCount,
        anyAccessible,
        anyMatchingArity,
        accessibleMatchingArity);
    if (!resolveConstructionAccessibility(
            binding,
            argumentTypes,
            anyAccessible,
            anyMatchingArity,
            accessibleMatchingArity,
            diagnostics))
    {
        constructions_.push_back(std::move(binding));
        return;
    }
    resolveConstructionCandidates(binding, argumentTypes, diagnostics);
}

}
