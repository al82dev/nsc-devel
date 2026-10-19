#include "nativesharp/Frontend/ProjectConstructionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{

bool ProjectConstructionModel::resolveConstructorInitializerTarget(
    ConstructorInitializerBinding& binding,
    const ClassMemberSymbol& constructor,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    binding.containingTypeOrdinal = constructor.declaringTypeOrdinal;
    binding.initializerSyntax = constructor.syntax != nullptr ? constructor.syntax->second() : nullptr;
    binding.explicitInitializer = binding.initializerSyntax != nullptr;
    const bool explicitThis = binding.initializerSyntax != nullptr &&
        binding.initializerSyntax->kind() == ast::AstNodeKind::ThisConstructorInitializer;
    binding.kind = explicitThis
        ? ConstructorInitializerKind::ExplicitThis
        : (binding.explicitInitializer
            ? ConstructorInitializerKind::ExplicitBase
            : ConstructorInitializerKind::ImplicitBase);
    binding.targetTypeOrdinal = explicitThis
        ? std::optional<std::size_t>(binding.containingTypeOrdinal)
        : inheritanceModel_->baseTypeOrdinal(binding.containingTypeOrdinal);
    if (binding.targetTypeOrdinal.has_value())
    {
        return true;
    }
    if (binding.kind == ConstructorInitializerKind::ExplicitBase)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            binding.initializerSyntax->range(),
            "base(...) cannot be used because the containing class has no base class");
        binding.resolution = ConstructorInitializerResolutionKind::Invalid;
    }
    else
    {
        binding.resolution = ConstructorInitializerResolutionKind::NoChainingRequired;
    }
    return false;
}

bool ProjectConstructionModel::collectConstructorInitializerArgumentTypes(
    const ConstructorInitializerBinding& binding,
    std::size_t constructorMemberOrdinal,
    std::vector<MethodExpressionTypeBinding>& argumentTypes) const
{
    if (binding.initializerSyntax == nullptr)
    {
        return true;
    }
    for (const ast::AstNode* argument : binding.initializerSyntax->children())
    {
        const std::optional<MethodExpressionTypeBinding> type =
            expressionType(argument, constructorMemberOrdinal);
        if (!type.has_value())
        {
            return false;
        }
        argumentTypes.push_back(*type);
    }
    return true;
}

void ProjectConstructionModel::bindImplicitConstructorInitializer(
    ConstructorInitializerBinding& binding,
    const ClassMemberSymbol& constructor,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // A class has an implicit default constructor only when it declares no explicit constructor.
    // Therefore this(...) always targets one of the current class's explicit declarations.  In
    // contrast, base(...) / implicit base() may target the base class's semantic implicit default.
    if (binding.kind == ConstructorInitializerKind::ExplicitThis)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            binding.initializerSyntax->range(),
            "this(...) cannot resolve because the containing class declares no target constructor");
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
        return;
    }
    ConstructorCandidateBinding candidate;
    candidate.implicitDefault = true;
    candidate.accessible = implicitDefaultAccessible(
        *binding.targetTypeOrdinal, binding.constructorMemberOrdinal, true);
    candidate.viable = candidate.accessible && argumentTypes.empty();
    const ImplicitDefaultConstructorBinding* implicitTarget =
        implicitDefaultConstructorBindingForType(*binding.targetTypeOrdinal);
    const bool implicitChainValid = implicitTarget != nullptr &&
        implicitTarget->resolution != ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor &&
        implicitTarget->resolution != ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor &&
        implicitTarget->resolution != ImplicitDefaultConstructorResolutionKind::Invalid;
    candidate.viable = candidate.viable && implicitChainValid;
    binding.evaluatedCandidates.push_back(candidate);
    if (!candidate.accessible)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InaccessibleMember,
            diagnostics::DiagnosticSeverity::Error,
            constructor.range,
            "implicit base constructor is not accessible from the derived constructor");
        binding.resolution = ConstructorInitializerResolutionKind::Inaccessible;
    }
    else if (!argumentTypes.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            binding.initializerSyntax != nullptr ? binding.initializerSyntax->range() : constructor.range,
            "base class has only an implicit parameterless constructor");
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
    }
    else if (!implicitChainValid)
    {
        binding.resolution = ConstructorInitializerResolutionKind::Invalid;
    }
    else
    {
        binding.resolution = ConstructorInitializerResolutionKind::ResolvedImplicitDefault;
        binding.selectedImplicitDefault = true;
        binding.selectedEntryConstructorMemberOrdinal = implicitTarget->entryConstructorMemberOrdinal;
    }
}

void ProjectConstructionModel::evaluateConstructorInitializerCandidates(
    ConstructorInitializerBinding& binding,
    std::size_t constructorMemberOrdinal,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    bool& anyAccessible,
    bool& anyMatchingArity,
    bool& accessibleMatchingArity) const
{
    const ProjectConversionClassifier conversions(*inheritanceModel_, interfaceModel_, memberModel_);
    for (const std::size_t candidateOrdinal : binding.explicitConstructorMemberOrdinals)
    {
        const ClassMemberSymbol& candidateSymbol = memberModel_->members()[candidateOrdinal];
        const bool accessible = binding.kind == ConstructorInitializerKind::ExplicitThis ||
            constructorAccessible(candidateSymbol, constructorMemberOrdinal, true);
        bool matchingArity = false;
        ConstructorCandidateBinding candidate = evaluateConstructorCandidate(
            candidateSymbol,
            candidateOrdinal,
            accessible,
            binding.initializerSyntax != nullptr
                ? std::span<ast::AstNode* const>(binding.initializerSyntax->children())
                : std::span<ast::AstNode* const>{},
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

void ProjectConstructionModel::selectConstructorInitializerCandidate(
    ConstructorInitializerBinding& binding,
    std::size_t selectedIndex) const
{
    const ConstructorCandidateBinding& selected = binding.evaluatedCandidates[selectedIndex];
    binding.resolution = ConstructorInitializerResolutionKind::ResolvedExplicit;
    binding.selectedConstructorMemberOrdinal = selected.memberOrdinal;
    binding.selectedEntryConstructorMemberOrdinal = selected.memberOrdinal;
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

void ProjectConstructionModel::resolveConstructorInitializerCandidates(
    ConstructorInitializerBinding& binding,
    const ClassMemberSymbol& constructor,
    std::span<const MethodExpressionTypeBinding> argumentTypes,
    bool anyAccessible,
    bool anyMatchingArity,
    bool accessibleMatchingArity,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const char* targetName = binding.kind == ConstructorInitializerKind::ExplicitThis
        ? "this-constructor"
        : "base-constructor";
    const SourceRange range = binding.initializerSyntax != nullptr
        ? binding.initializerSyntax->range()
        : constructor.range;
    if (!anyAccessible || (!accessibleMatchingArity && anyMatchingArity))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InaccessibleMember,
            diagnostics::DiagnosticSeverity::Error,
            range,
            std::string("no accessible ") + targetName + " matches the constructor initializer");
        binding.resolution = ConstructorInitializerResolutionKind::Inaccessible;
        return;
    }
    if (!accessibleMatchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            range,
            std::string("no accessible ") + targetName + " accepts " +
                std::to_string(argumentTypes.size()) + " argument(s)");
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
        return;
    }
    const std::vector<std::size_t> viable =
        viableConstructorCandidates(binding.evaluatedCandidates);
    if (viable.empty())
    {
        // NSC-LIMIT-010: misma explicacion que el camino de `new`. «no applicable
        // this-constructor matches the argument types» es cierto y no ayuda cuando los tipos
        // encajan y lo que falta es un `move`.
        if (!explainOwnershipRejection(
                binding.evaluatedCandidates, argumentTypes.size(),
                binding.initializerSyntax, binding.initializerSyntax, diagnostics))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                range,
                std::string("no applicable ") + targetName +
                    " matches the constructor initializer argument types");
        }
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
        return;
    }
    const std::vector<std::size_t> nonDominated =
        nonDominatedConstructorCandidates(binding.evaluatedCandidates, viable);
    if (nonDominated.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            range,
            std::string("ambiguous ") + targetName + " invocation");
        binding.resolution = ConstructorInitializerResolutionKind::Ambiguous;
        return;
    }
    selectConstructorInitializerCandidate(binding, nonDominated.front());
}

void ProjectConstructionModel::storeConstructorInitializerBinding(
    std::size_t constructorMemberOrdinal,
    ConstructorInitializerBinding&& binding)
{
    initializerBindingByMember_[constructorMemberOrdinal] = constructorInitializerBindings_.size();
    constructorInitializerBindings_.push_back(std::move(binding));
}

void ProjectConstructionModel::bindConstructorInitializer(
    std::size_t constructorMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ConstructorInitializerBinding binding;
    binding.constructorMemberOrdinal = constructorMemberOrdinal;
    if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        methodBodyModel_ == nullptr || constructorMemberOrdinal >= memberModel_->members().size())
    {
        constructorInitializerBindings_.push_back(std::move(binding));
        return;
    }

    const ClassMemberSymbol& constructor = memberModel_->members()[constructorMemberOrdinal];
    if (!resolveConstructorInitializerTarget(binding, constructor, diagnostics))
    {
        storeConstructorInitializerBinding(constructorMemberOrdinal, std::move(binding));
        return;
    }

    const std::span<const std::size_t> constructors =
        memberModel_->constructorsOfType(*binding.targetTypeOrdinal);
    binding.explicitConstructorMemberOrdinals.assign(constructors.begin(), constructors.end());
    std::vector<MethodExpressionTypeBinding> argumentTypes;
    if (!collectConstructorInitializerArgumentTypes(
            binding, constructorMemberOrdinal, argumentTypes))
    {
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
        storeConstructorInitializerBinding(constructorMemberOrdinal, std::move(binding));
        return;
    }
    if (binding.explicitConstructorMemberOrdinals.empty())
    {
        bindImplicitConstructorInitializer(binding, constructor, argumentTypes, diagnostics);
        storeConstructorInitializerBinding(constructorMemberOrdinal, std::move(binding));
        return;
    }

    bool anyAccessible = false;
    bool anyMatchingArity = false;
    bool accessibleMatchingArity = false;
    evaluateConstructorInitializerCandidates(
        binding,
        constructorMemberOrdinal,
        argumentTypes,
        anyAccessible,
        anyMatchingArity,
        accessibleMatchingArity);
    resolveConstructorInitializerCandidates(
        binding,
        constructor,
        argumentTypes,
        anyAccessible,
        anyMatchingArity,
        accessibleMatchingArity,
        diagnostics);
    storeConstructorInitializerBinding(constructorMemberOrdinal, std::move(binding));
}

void ProjectConstructionModel::validateThisConstructorCycles(
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberModel_ == nullptr)
    {
        return;
    }

    std::vector<bool> invalid(memberModel_->members().size(), false);
    for (const ConstructorInitializerBinding& startBinding : constructorInitializerBindings_)
    {
        if (startBinding.kind != ConstructorInitializerKind::ExplicitThis ||
            startBinding.resolution != ConstructorInitializerResolutionKind::ResolvedExplicit)
        {
            continue;
        }

        std::vector<std::size_t> path;
        std::size_t current = startBinding.constructorMemberOrdinal;
        while (current < initializerBindingByMember_.size() &&
               initializerBindingByMember_[current].has_value())
        {
            const std::size_t bindingIndex = *initializerBindingByMember_[current];
            if (bindingIndex >= constructorInitializerBindings_.size())
            {
                break;
            }
            ConstructorInitializerBinding& binding = constructorInitializerBindings_[bindingIndex];
            if (binding.kind != ConstructorInitializerKind::ExplicitThis ||
                binding.resolution != ConstructorInitializerResolutionKind::ResolvedExplicit ||
                !binding.selectedConstructorMemberOrdinal.has_value())
            {
                break;
            }

            const auto cycleBegin = std::find(path.begin(), path.end(), current);
            if (cycleBegin != path.end())
            {
                for (auto it = cycleBegin; it != path.end(); ++it)
                {
                    invalid[*it] = true;
                }
                invalid[current] = true;
                break;
            }
            path.push_back(current);
            current = *binding.selectedConstructorMemberOrdinal;
        }
    }

    bool reported = false;
    for (std::size_t memberOrdinal = 0; memberOrdinal < invalid.size(); ++memberOrdinal)
    {
        if (!invalid[memberOrdinal] || memberOrdinal >= initializerBindingByMember_.size() ||
            !initializerBindingByMember_[memberOrdinal].has_value())
        {
            continue;
        }
        ConstructorInitializerBinding& binding =
            constructorInitializerBindings_[*initializerBindingByMember_[memberOrdinal]];
        binding.resolution = ConstructorInitializerResolutionKind::Invalid;
        if (!reported)
        {
            const SourceRange range = binding.initializerSyntax != nullptr
                ? binding.initializerSyntax->range()
                : memberModel_->members()[memberOrdinal].range;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                range,
                "constructor delegation cycle detected in this(...) initializer chain");
            reported = true;
        }
    }
}

}
