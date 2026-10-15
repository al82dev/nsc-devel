#include "nativesharp/Frontend/ProjectConstructionModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
struct NamedConstructorArgumentMap final
{
    bool valid = false;
    std::vector<std::size_t> explicitParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    std::vector<bool> parameterPackElements;
    std::optional<std::size_t> parameterPackOrdinal;
    bool usesExpandedParameterPack = false;
    bool synthesizesEmptyParameterPack = false;
};

NamedConstructorArgumentMap mapConstructorArguments(
    std::span<ast::AstNode* const> arguments,
    const ClassMemberSymbol& constructor)
{
    NamedConstructorArgumentMap result;
    if (!constructor.parameters.empty() && constructor.parameters.back().isParameterPack)
        result.parameterPackOrdinal = constructor.parameters.size() - 1U;
    std::vector<bool> assigned(constructor.parameters.size(), false);
    std::size_t nextPositional = 0;
    bool reachedExpandedParameterPack = false;
    bool assignedParameterPackDirectly = false;
    for (const ast::AstNode* argument : arguments)
    {
        if (argument == nullptr) return result;
        std::optional<std::size_t> parameterOrdinal;
        bool parameterPackElement = false;
        if (!argument->argumentName().empty())
        {
            if (reachedExpandedParameterPack) return result;
            for (std::size_t index = 0; index < constructor.parameters.size(); ++index)
            {
                if (constructor.parameters[index].name == argument->argumentName())
                {
                    parameterOrdinal = index;
                    break;
                }
            }
            if (parameterOrdinal == result.parameterPackOrdinal)
            {
                if (assignedParameterPackDirectly) return result;
                assignedParameterPackDirectly = true;
            }
        }
        else
        {
            while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
            if (nextPositional < assigned.size())
            {
                parameterOrdinal = nextPositional;
                if (parameterOrdinal == result.parameterPackOrdinal)
                {
                    if (assignedParameterPackDirectly) return result;
                    parameterPackElement = true;
                    reachedExpandedParameterPack = true;
                }
            }
        }
        if (!parameterOrdinal.has_value() ||
            (assigned[*parameterOrdinal] && !parameterPackElement)) return result;
        if (!parameterPackElement) assigned[*parameterOrdinal] = true;
        result.explicitParameterOrdinals.push_back(*parameterOrdinal);
        result.parameterPackElements.push_back(parameterPackElement);
        result.usesExpandedParameterPack =
            result.usesExpandedParameterPack || parameterPackElement;
        while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
    }
    for (std::size_t index = 0; index < constructor.parameters.size(); ++index)
    {
        if (index == result.parameterPackOrdinal)
        {
            result.synthesizesEmptyParameterPack =
                !result.usesExpandedParameterPack && !assignedParameterPackDirectly;
            continue;
        }
        if (assigned[index]) continue;
        if (constructor.parameters[index].defaultValueSyntax == nullptr) return result;
        result.defaultedParameterOrdinals.push_back(index);
    }
    result.valid = true;
    return result;
}

bool sameProject(
    std::size_t leftTypeOrdinal,
    std::size_t rightTypeOrdinal,
    const ProjectNameLookup& nameLookup) noexcept
{
    if (leftTypeOrdinal >= nameLookup.namedTypes().size() ||
        rightTypeOrdinal >= nameLookup.namedTypes().size())
    {
        return false;
    }
    return nameLookup.namedTypes()[leftTypeOrdinal].projectOrdinal ==
        nameLookup.namedTypes()[rightTypeOrdinal].projectOrdinal;
}

bool isStringLiteralBorrow(
    const MethodParameterSymbol& parameter,
    const MethodExpressionTypeBinding& argument) noexcept
{
    return parameter.ownership == sema::OwnershipKind::Borrowed &&
        parameter.typeKey == "builtin:string" && argument.typeKey == "string-literal";
}

bool isFreshOwnedStringBorrow(
    const MethodParameterSymbol& parameter,
    const MethodExpressionTypeBinding& argument) noexcept
{
    return parameter.ownership == sema::OwnershipKind::Borrowed &&
        parameter.typeKey == "builtin:string" &&
        argument.ownership == sema::OwnershipKind::Owned &&
        argument.ownershipTransfer && argument.ownershipFreshOwner;
}
}

bool ProjectConstructionModel::build(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectInheritanceModel& inheritanceModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectMethodBodyModel& methodBodyModel,
    const ProjectNamespaceFunctionModel& namespaceFunctionModel,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_)
    {
        return !diagnostics.hasErrors();
    }
    if (!nameLookup.isBuilt() || !memberModel.isBuilt() || !inheritanceModel.isBuilt() ||
        !interfaceModel.isBuilt() || !methodBodyModel.isBuilt())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            SourceRange(),
            "project construction binding requires completed name, member, inheritance, interface and callable-body models");
        built_ = true;
        return false;
    }

    nameLookup_ = &nameLookup;
    memberModel_ = &memberModel;
    inheritanceModel_ = &inheritanceModel;
    interfaceModel_ = &interfaceModel;
    methodBodyModel_ = &methodBodyModel;
    namespaceFunctionModel_ = &namespaceFunctionModel;
    statistics_ = {};
    initializerBindingByMember_.resize(memberModel.members().size());
    implicitDefaultBindingByType_.resize(nameLookup.namedTypes().size());

    // A source class with no explicit constructor still has a semantic constructor obligation.
    // Validate those chains first so explicit derived constructors and object constructions can
    // safely reference a completed implicit-default model.
    bindImplicitDefaultConstructors(diagnostics);

    // Constructor chaining is a declaration-level semantic obligation. Bind it before scanning
    // object-creation expressions so an invalid derived constructor cannot hide behind the fact
    // that no `new Derived(...)` expression happens to occur in this project.
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel.members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& constructor = memberModel.members()[memberOrdinal];
        if (constructor.kind == ClassMemberKind::Constructor &&
            !constructor.isStatic &&
            constructor.declaringTypeOrdinal < nameLookup.namedTypes().size() &&
            !nameLookup.namedTypes()[constructor.declaringTypeOrdinal].isPrecompiled)
        {
            bindConstructorInitializer(memberOrdinal, diagnostics);
        }
    }
    validateThisConstructorCycles(diagnostics);

    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit != nullptr && unit->syntaxTree() != nullptr)
        {
            scanUnsupportedContexts(unit->syntaxTree(), diagnostics);
        }
    }

    std::vector<const ProjectConstructionSite*> orderedSites;
    orderedSites.reserve(methodBodyModel.constructionSites().size());
    for (const ProjectConstructionSite& site : methodBodyModel.constructionSites())
    {
        orderedSites.push_back(&site);
    }
    std::ranges::sort(
        orderedSites,
        [](const ProjectConstructionSite* left, const ProjectConstructionSite* right)
        {
            if (left->callableOrdinal != right->callableOrdinal)
            {
                return left->callableOrdinal < right->callableOrdinal;
            }
            return left->sequenceWithinCallable < right->sequenceWithinCallable;
        });
    for (const ProjectConstructionSite* site : orderedSites)
    {
        ++statistics_.typedSitesConsumed;
        statistics_.typedArgumentsConsumed += site->argumentTypes.size();
        bindConstruction(*site, diagnostics);
    }

    built_ = true;
    return !diagnostics.hasErrors();
}

std::span<const ObjectConstructionBinding> ProjectConstructionModel::constructions() const noexcept
{
    return constructions_;
}

std::span<const ConstructorInitializerBinding> ProjectConstructionModel::constructorInitializerBindings() const noexcept
{
    return constructorInitializerBindings_;
}

const ConstructorInitializerBinding* ProjectConstructionModel::constructorInitializerBindingForMember(
    std::size_t memberOrdinal) const noexcept
{
    if (memberOrdinal >= initializerBindingByMember_.size() || !initializerBindingByMember_[memberOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *initializerBindingByMember_[memberOrdinal];
    return index < constructorInitializerBindings_.size() ? &constructorInitializerBindings_[index] : nullptr;
}

std::span<const ImplicitDefaultConstructorBinding>
ProjectConstructionModel::implicitDefaultConstructorBindings() const noexcept
{
    return implicitDefaultConstructorBindings_;
}

const ImplicitDefaultConstructorBinding*
ProjectConstructionModel::implicitDefaultConstructorBindingForType(
    std::size_t typeOrdinal) const noexcept
{
    if (typeOrdinal >= implicitDefaultBindingByType_.size() ||
        !implicitDefaultBindingByType_[typeOrdinal].has_value())
    {
        return nullptr;
    }
    const std::size_t index = *implicitDefaultBindingByType_[typeOrdinal];
    return index < implicitDefaultConstructorBindings_.size()
        ? &implicitDefaultConstructorBindings_[index]
        : nullptr;
}

const ProjectConstructionStatistics& ProjectConstructionModel::statistics() const noexcept
{
    return statistics_;
}

bool ProjectConstructionModel::isBuilt() const noexcept
{
    return built_;
}

void ProjectConstructionModel::scanUnsupportedContexts(
    const ast::AstNode* node,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<const ast::AstNode*> work;
    if (node != nullptr)
    {
        work.push_back(node);
    }
    while (!work.empty())
    {
        const ast::AstNode* current = work.back();
        work.pop_back();
        if (current == nullptr)
        {
            continue;
        }
        ++statistics_.unsupportedContextNodesVisited;
        if (current->kind() == ast::AstNodeKind::MethodDeclaration ||
            current->kind() == ast::AstNodeKind::ConstructorDeclaration ||
            current->kind() == ast::AstNodeKind::FieldDeclaration ||
            current->kind() == ast::AstNodeKind::FunctionDeclaration)
        {
            continue;
        }
        if (current->kind() == ast::AstNodeKind::ObjectCreationExpression ||
            current->kind() == ast::AstNodeKind::PlacementConstructionExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                current->range(),
                "object construction is not valid in this declaration context");
            continue;
        }

        const std::span<ast::AstNode* const> children = current->children();
        for (auto child = children.rbegin(); child != children.rend(); ++child)
        {
            work.push_back(*child);
        }
        work.push_back(current->third());
        work.push_back(current->second());
        work.push_back(current->first());
    }
}

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
    else
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
        }
        else
        {
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
    }

    implicitDefaultBindingByType_[typeOrdinal] = implicitDefaultConstructorBindings_.size();
    implicitDefaultConstructorBindings_.push_back(std::move(binding));
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

    if (!binding.targetTypeOrdinal.has_value())
    {
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
        initializerBindingByMember_[constructorMemberOrdinal] = constructorInitializerBindings_.size();
        constructorInitializerBindings_.push_back(std::move(binding));
        return;
    }

    const std::size_t targetTypeOrdinal = *binding.targetTypeOrdinal;
    const std::span<const std::size_t> constructors =
        memberModel_->constructorsOfType(targetTypeOrdinal);
    binding.explicitConstructorMemberOrdinals.assign(constructors.begin(), constructors.end());

    std::vector<MethodExpressionTypeBinding> argumentTypes;
    bool argumentsValid = true;
    if (binding.initializerSyntax != nullptr)
    {
        for (const ast::AstNode* argument : binding.initializerSyntax->children())
        {
            const std::optional<MethodExpressionTypeBinding> type =
                expressionType(argument, constructorMemberOrdinal);
            if (!type.has_value())
            {
                argumentsValid = false;
                break;
            }
            argumentTypes.push_back(*type);
        }
    }
    if (!argumentsValid)
    {
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
        initializerBindingByMember_[constructorMemberOrdinal] = constructorInitializerBindings_.size();
        constructorInitializerBindings_.push_back(std::move(binding));
        return;
    }

    // A class has an implicit default constructor only when it declares no explicit constructor.
    // Therefore this(...) always targets one of the current class's explicit declarations.  In
    // contrast, base(...) / implicit base() may target the base class's semantic implicit default.
    if (binding.explicitConstructorMemberOrdinals.empty())
    {
        if (binding.kind == ConstructorInitializerKind::ExplicitThis)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                binding.initializerSyntax->range(),
                "this(...) cannot resolve because the containing class declares no target constructor");
            binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
            initializerBindingByMember_[constructorMemberOrdinal] = constructorInitializerBindings_.size();
            constructorInitializerBindings_.push_back(std::move(binding));
            return;
        }

        ConstructorCandidateBinding candidate;
        candidate.implicitDefault = true;
        candidate.accessible = implicitDefaultAccessible(
            targetTypeOrdinal, constructorMemberOrdinal, true);
        candidate.viable = candidate.accessible && argumentTypes.empty();

        const ImplicitDefaultConstructorBinding* implicitTarget =
            implicitDefaultConstructorBindingForType(targetTypeOrdinal);
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
        initializerBindingByMember_[constructorMemberOrdinal] = constructorInitializerBindings_.size();
        constructorInitializerBindings_.push_back(std::move(binding));
        return;
    }

    bool anyAccessible = false;
    bool anyMatchingArity = false;
    bool accessibleMatchingArity = false;
    const ProjectConversionClassifier conversions(*inheritanceModel_, interfaceModel_, memberModel_);
    for (const std::size_t candidateOrdinal : binding.explicitConstructorMemberOrdinals)
    {
        const ClassMemberSymbol& candidateSymbol = memberModel_->members()[candidateOrdinal];
        ConstructorCandidateBinding candidate;
        candidate.memberOrdinal = candidateOrdinal;
        candidate.accessible = binding.kind == ConstructorInitializerKind::ExplicitThis ||
            constructorAccessible(candidateSymbol, constructorMemberOrdinal, true);
        anyAccessible = anyAccessible || candidate.accessible;
        const NamedConstructorArgumentMap argumentMapping = mapConstructorArguments(
            binding.initializerSyntax != nullptr
                ? std::span<ast::AstNode* const>(binding.initializerSyntax->children())
                : std::span<ast::AstNode* const>{},
            candidateSymbol);
        const bool matchingArity = argumentMapping.valid;
        anyMatchingArity = anyMatchingArity || matchingArity;
        if (!candidate.accessible || !matchingArity)
        {
            binding.evaluatedCandidates.push_back(std::move(candidate));
            continue;
        }

        accessibleMatchingArity = true;
        candidate.viable = true;
        candidate.argumentParameterOrdinals = argumentMapping.explicitParameterOrdinals;
        candidate.defaultedParameterOrdinals = argumentMapping.defaultedParameterOrdinals;
        candidate.parameterPackElements = argumentMapping.parameterPackElements;
        candidate.parameterPackOrdinal = argumentMapping.parameterPackOrdinal;
        candidate.usesExpandedParameterPack = argumentMapping.usesExpandedParameterPack;
        candidate.synthesizesEmptyParameterPack = argumentMapping.synthesizesEmptyParameterPack;
        if (candidate.usesExpandedParameterPack &&
            std::ranges::count(candidate.parameterPackElements, true) == 1 &&
            candidate.parameterPackOrdinal.has_value())
        {
            const auto element = std::ranges::find(candidate.parameterPackElements, true);
            const std::size_t argumentOrdinal = static_cast<std::size_t>(
                std::distance(candidate.parameterPackElements.begin(), element));
            const MethodParameterSymbol& pack =
                candidateSymbol.parameters[*candidate.parameterPackOrdinal];
            if (conversions.classifyImplicit(
                    argumentTypes[argumentOrdinal].typeKey,
                    pack.typeKey,
                    argumentTypes[argumentOrdinal].integerConstant).exists())
            {
                candidate.parameterPackElements[argumentOrdinal] = false;
                candidate.usesExpandedParameterPack = false;
            }
        }
        candidate.defaultedParameterCount = argumentMapping.defaultedParameterOrdinals.size();
        for (std::size_t argumentOrdinal = 0; argumentOrdinal < argumentTypes.size(); ++argumentOrdinal)
        {
            const std::size_t parameterOrdinal =
                candidate.argumentParameterOrdinals[argumentOrdinal];
            const bool packElement = argumentOrdinal < candidate.parameterPackElements.size() &&
                candidate.parameterPackElements[argumentOrdinal];
            const MethodParameterSymbol& parameter = candidateSymbol.parameters[parameterOrdinal];
            std::string conversionTarget = parameter.typeKey;
            if (packElement)
            {
                if (parameter.parameterPackElementTypeKey.empty()) candidate.viable = false;
                else conversionTarget = parameter.parameterPackElementTypeKey;
            }
            ProjectImplicitConversion conversion = conversions.classifyImplicit(
                argumentTypes[argumentOrdinal].typeKey,
                conversionTarget,
                argumentTypes[argumentOrdinal].integerConstant);
            if (!conversion.exists())
            {
                candidate.viable = false;
            }
            if (!packElement && argumentTypes[argumentOrdinal].pointerPointeeReadonly &&
                ProjectTypeConversion::isPointer(parameter.typeKey) &&
                !parameter.pointerPointeeReadonly)
            {
                candidate.viable = false;
            }
            // NSC-LIMIT-010. PRIMERO LA FISICA, DESPUES EL CONTRATO, y en ese orden.
            //
            // Un candidato fisicamente imposible no llega a tener opinion sobre propiedad: si la
            // conversion no existe, el argumento ni siquiera cabe en ese parametro, y dejar que
            // hable el calificador produciria un diagnostico sobre `move` cuando el problema es el
            // tipo. El veredicto se calcula igualmente para que `ownershipFits` tenga exactamente
            // un elemento por argumento --el diagnostico indexa por posicion-- pero solo tumba al
            // candidato cuando la fisica ya lo habia aceptado.
            const bool literalBorrow = !packElement &&
                isStringLiteralBorrow(parameter, argumentTypes[argumentOrdinal]);
            const bool freshBorrow = !packElement &&
                isFreshOwnedStringBorrow(parameter, argumentTypes[argumentOrdinal]);
            const OwnershipArgumentFit fit = packElement
                ? (argumentTypes[argumentOrdinal].ownership == sema::OwnershipKind::None &&
                   !argumentTypes[argumentOrdinal].ownershipTransfer &&
                   !argumentTypes[argumentOrdinal].ownershipFreshOwner
                       ? OwnershipArgumentFit::Ok
                       : OwnershipArgumentFit::UnqualifiedRejectsOwnership)
                : literalBorrow || freshBorrow
                ? OwnershipArgumentFit::Ok
                : classifyOwnershipArgument(
                methodBodyModel_->typeKeyIsOwnable(parameter.typeKey),
                parameter.ownership,
                argumentTypes[argumentOrdinal].ownership,
                argumentTypes[argumentOrdinal].ownershipTransfer,
                argumentTypes[argumentOrdinal].ownershipFreshOwner,
                argumentTypes[argumentOrdinal].typeKey == "builtin:null");
            candidate.ownershipFits.push_back(fit);
            const bool moveOnlyCopy = !packElement && conversion.exists() &&
                parameter.passKind == ParameterPassKind::Value &&
                memberModel_->typeIsMoveOnly(conversionTarget) &&
                sema::isPlaceCategory(argumentTypes[argumentOrdinal].valueCategory) &&
                !argumentTypes[argumentOrdinal].ownershipTransfer;
            candidate.moveOnlyCopyRejects.push_back(moveOnlyCopy);
            if (freshBorrow)
            {
                candidate.fullExpressionBorrowArgumentOrdinals.push_back(argumentOrdinal);
            }
            if (conversion.exists() && fit != OwnershipArgumentFit::Ok)
            {
                candidate.viable = false;
            }
            if (moveOnlyCopy) candidate.viable = false;
            if (fit == OwnershipArgumentFit::Ok && !freshBorrow &&
                argumentTypes[argumentOrdinal].ownershipTransfer)
            {
                ConstructorOwnershipTransfer transfer;
                transfer.parameterOrdinal = parameterOrdinal;
                transfer.sourceLocalOrdinal =
                    argumentTypes[argumentOrdinal].ownershipSourceLocalOrdinal;
                transfer.sourceParameterOrdinal =
                    argumentTypes[argumentOrdinal].ownershipSourceParameterOrdinal;
                candidate.ownershipTransfers.push_back(transfer);
            }
            candidate.conversions.push_back(std::move(conversion));
        }
        binding.evaluatedCandidates.push_back(std::move(candidate));
    }

    const char* targetName = binding.kind == ConstructorInitializerKind::ExplicitThis
        ? "this-constructor"
        : "base-constructor";
    if (!anyAccessible || (!accessibleMatchingArity && anyMatchingArity))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InaccessibleMember,
            diagnostics::DiagnosticSeverity::Error,
            binding.initializerSyntax != nullptr ? binding.initializerSyntax->range() : constructor.range,
            std::string("no accessible ") + targetName + " matches the constructor initializer");
        binding.resolution = ConstructorInitializerResolutionKind::Inaccessible;
    }
    else if (!accessibleMatchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            binding.initializerSyntax != nullptr ? binding.initializerSyntax->range() : constructor.range,
            std::string("no accessible ") + targetName + " accepts " +
                std::to_string(argumentTypes.size()) + " argument(s)");
        binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
    }
    else
    {
        std::vector<std::size_t> viable;
        for (std::size_t index = 0; index < binding.evaluatedCandidates.size(); ++index)
        {
            if (binding.evaluatedCandidates[index].viable)
            {
                viable.push_back(index);
            }
        }
        std::vector<std::size_t> nonDominated;
        for (const std::size_t index : viable)
        {
            bool dominated = false;
            for (const std::size_t other : viable)
            {
                if (index != other && betterCandidate(
                        binding.evaluatedCandidates[other], binding.evaluatedCandidates[index]))
                {
                    dominated = true;
                    break;
                }
            }
            if (!dominated)
            {
                nonDominated.push_back(index);
            }
        }
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
                    binding.initializerSyntax != nullptr ? binding.initializerSyntax->range() : constructor.range,
                    std::string("no applicable ") + targetName + " matches the constructor initializer argument types");
            }
            binding.resolution = ConstructorInitializerResolutionKind::NoViableCandidate;
        }
        else if (nonDominated.size() != 1)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                binding.initializerSyntax != nullptr ? binding.initializerSyntax->range() : constructor.range,
                std::string("ambiguous ") + targetName + " invocation");
            binding.resolution = ConstructorInitializerResolutionKind::Ambiguous;
        }
        else
        {
            const ConstructorCandidateBinding& selected = binding.evaluatedCandidates[nonDominated.front()];
            binding.resolution = ConstructorInitializerResolutionKind::ResolvedExplicit;
            binding.selectedConstructorMemberOrdinal = selected.memberOrdinal;
            binding.selectedEntryConstructorMemberOrdinal = selected.memberOrdinal;
            binding.selectedConversions = selected.conversions;
            binding.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
            binding.selectedParameterPackElements = selected.parameterPackElements;
            binding.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
            binding.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
            binding.selectedSynthesizesEmptyParameterPack =
                selected.synthesizesEmptyParameterPack;
            const ClassMemberSymbol& selectedSymbol =
                memberModel_->members()[*selected.memberOrdinal];
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
    }

    initializerBindingByMember_[constructorMemberOrdinal] = constructorInitializerBindings_.size();
    constructorInitializerBindings_.push_back(std::move(binding));
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

void ProjectConstructionModel::bindConstruction(
    const ProjectConstructionSite& site,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::AstNode* expression = site.syntax;
    const std::size_t callerMemberOrdinal = site.callableOrdinal;
    ObjectConstructionBinding binding;
    binding.syntax = expression;
    binding.callerMemberOrdinal = callerMemberOrdinal;

    // NS304-T09-F07: `new()` target-typed llega sin TypeSyntax; el tipo ya está resuelto en
    // site.targetTypeOrdinal. Se permite avanzar solo en ese caso.
    const bool targetTyped = expression != nullptr && expression->typeSyntax() == nullptr &&
        expression->auxiliaryText() == "target-typed";
    if (nameLookup_ == nullptr || memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        methodBodyModel_ == nullptr || expression == nullptr ||
        (expression->typeSyntax() == nullptr && !targetTyped))
    {
        constructions_.push_back(std::move(binding));
        return;
    }

    if (!targetTyped && !expression->typeSyntax()->modifiers().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->typeSyntax()->range(),
            "object construction requires an unmodified class type; pointer construction is not object creation");
        constructions_.push_back(std::move(binding));
        return;
    }

    // C6 "struct members": `new T(args)` now also accepts a STRUCT. On a class it means
    // "allocate on the heap and run the constructor on it"; on a struct it means "initialize
    // storage in place and run the constructor on it" -- no allocation, no descriptor, nothing
    // to free. The two share this entire routine because everything here is about WHICH
    // constructor runs (candidates, overload resolution, argument conversions), and none of that
    // depends on where the object lives.
    const std::optional<std::size_t> targetOrdinal = site.targetTypeOrdinal;
    const bool targetIsClass = targetOrdinal.has_value() &&
        *targetOrdinal < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*targetOrdinal].kind == NamedTypeDeclarationKind::Class;
    const bool targetIsStruct = targetOrdinal.has_value() &&
        *targetOrdinal < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*targetOrdinal].kind == NamedTypeDeclarationKind::Struct;
    if (!targetIsClass && !targetIsStruct)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidAllocationType,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "new T(args) requires T to resolve to a class or struct type");
        constructions_.push_back(std::move(binding));
        return;
    }

    binding.targetIsStruct = targetIsStruct;
    binding.allocatorSelection =
        site.kind == ProjectConstructionSiteKind::Allocator;
    binding.allocatorTypeKey = site.allocatorTypeKey;
    binding.allocatorAllocateMemberOrdinal = site.allocatorAllocateMemberOrdinal;
    binding.allocatorDeallocateMemberOrdinal = site.allocatorDeallocateMemberOrdinal;
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
        constructions_.push_back(std::move(binding));
        return;
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
        ConstructorCandidateBinding candidate;
        candidate.implicitDefault = true;
        candidate.accessible = implicitDefaultAccessible(*targetOrdinal, callerMemberOrdinal, false);
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
                implicitDefaultConstructorBindingForType(*targetOrdinal);
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
        constructions_.push_back(std::move(binding));
        return;
    }

    bool anyAccessible = false;
    bool anyMatchingArity = false;
    bool accessibleMatchingArity = false;
    const ProjectConversionClassifier conversions(*inheritanceModel_, interfaceModel_, memberModel_);
    for (const std::size_t constructorOrdinal : binding.explicitConstructorMemberOrdinals)
    {
        const ClassMemberSymbol& constructor = memberModel_->members()[constructorOrdinal];
        ConstructorCandidateBinding candidate;
        candidate.memberOrdinal = constructorOrdinal;
        candidate.accessible = constructorAccessible(constructor, callerMemberOrdinal, false);
        anyAccessible = anyAccessible || candidate.accessible;
        const NamedConstructorArgumentMap argumentMapping =
            mapConstructorArguments(
                std::span<ast::AstNode* const>(
                    expression->children().data(), constructorArgumentCount),
                constructor);
        const bool matchingArity = argumentMapping.valid;
        anyMatchingArity = anyMatchingArity || matchingArity;
        if (!candidate.accessible || !matchingArity)
        {
            binding.evaluatedCandidates.push_back(std::move(candidate));
            continue;
        }

        accessibleMatchingArity = true;
        candidate.viable = true;
        candidate.argumentParameterOrdinals = argumentMapping.explicitParameterOrdinals;
        candidate.defaultedParameterOrdinals = argumentMapping.defaultedParameterOrdinals;
        candidate.parameterPackElements = argumentMapping.parameterPackElements;
        candidate.parameterPackOrdinal = argumentMapping.parameterPackOrdinal;
        candidate.usesExpandedParameterPack = argumentMapping.usesExpandedParameterPack;
        candidate.synthesizesEmptyParameterPack = argumentMapping.synthesizesEmptyParameterPack;
        if (candidate.usesExpandedParameterPack &&
            std::ranges::count(candidate.parameterPackElements, true) == 1 &&
            candidate.parameterPackOrdinal.has_value())
        {
            const auto element = std::ranges::find(candidate.parameterPackElements, true);
            const std::size_t argumentOrdinal = static_cast<std::size_t>(
                std::distance(candidate.parameterPackElements.begin(), element));
            const MethodParameterSymbol& pack =
                constructor.parameters[*candidate.parameterPackOrdinal];
            if (conversions.classifyImplicit(
                    argumentTypes[argumentOrdinal].typeKey,
                    pack.typeKey,
                    argumentTypes[argumentOrdinal].integerConstant).exists())
            {
                candidate.parameterPackElements[argumentOrdinal] = false;
                candidate.usesExpandedParameterPack = false;
            }
        }
        candidate.defaultedParameterCount = argumentMapping.defaultedParameterOrdinals.size();
        for (std::size_t argumentOrdinal = 0; argumentOrdinal < argumentTypes.size(); ++argumentOrdinal)
        {
            const std::size_t parameterOrdinal =
                candidate.argumentParameterOrdinals[argumentOrdinal];
            const bool packElement = argumentOrdinal < candidate.parameterPackElements.size() &&
                candidate.parameterPackElements[argumentOrdinal];
            const MethodParameterSymbol& parameter = constructor.parameters[parameterOrdinal];
            std::string conversionTarget = parameter.typeKey;
            if (packElement)
            {
                if (parameter.parameterPackElementTypeKey.empty()) candidate.viable = false;
                else conversionTarget = parameter.parameterPackElementTypeKey;
            }
            ProjectImplicitConversion conversion = conversions.classifyImplicit(
                argumentTypes[argumentOrdinal].typeKey,
                conversionTarget,
                argumentTypes[argumentOrdinal].integerConstant);
            if (!conversion.exists())
            {
                candidate.viable = false;
            }
            if (!packElement && argumentTypes[argumentOrdinal].pointerPointeeReadonly &&
                ProjectTypeConversion::isPointer(parameter.typeKey) &&
                !parameter.pointerPointeeReadonly)
            {
                candidate.viable = false;
            }
            // NSC-LIMIT-010. PRIMERO LA FISICA, DESPUES EL CONTRATO, y en ese orden.
            //
            // Un candidato fisicamente imposible no llega a tener opinion sobre propiedad: si la
            // conversion no existe, el argumento ni siquiera cabe en ese parametro, y dejar que
            // hable el calificador produciria un diagnostico sobre `move` cuando el problema es el
            // tipo. El veredicto se calcula igualmente para que `ownershipFits` tenga exactamente
            // un elemento por argumento --el diagnostico indexa por posicion-- pero solo tumba al
            // candidato cuando la fisica ya lo habia aceptado.
            const bool literalBorrow = !packElement &&
                isStringLiteralBorrow(parameter, argumentTypes[argumentOrdinal]);
            const bool freshBorrow = !packElement &&
                isFreshOwnedStringBorrow(parameter, argumentTypes[argumentOrdinal]);
            const OwnershipArgumentFit fit = packElement
                ? (argumentTypes[argumentOrdinal].ownership == sema::OwnershipKind::None &&
                   !argumentTypes[argumentOrdinal].ownershipTransfer &&
                   !argumentTypes[argumentOrdinal].ownershipFreshOwner
                       ? OwnershipArgumentFit::Ok
                       : OwnershipArgumentFit::UnqualifiedRejectsOwnership)
                : literalBorrow || freshBorrow
                ? OwnershipArgumentFit::Ok
                : classifyOwnershipArgument(
                methodBodyModel_->typeKeyIsOwnable(parameter.typeKey),
                parameter.ownership,
                argumentTypes[argumentOrdinal].ownership,
                argumentTypes[argumentOrdinal].ownershipTransfer,
                argumentTypes[argumentOrdinal].ownershipFreshOwner,
                argumentTypes[argumentOrdinal].typeKey == "builtin:null");
            candidate.ownershipFits.push_back(fit);
            const bool moveOnlyCopy = !packElement && conversion.exists() &&
                parameter.passKind == ParameterPassKind::Value &&
                memberModel_->typeIsMoveOnly(conversionTarget) &&
                sema::isPlaceCategory(argumentTypes[argumentOrdinal].valueCategory) &&
                !argumentTypes[argumentOrdinal].ownershipTransfer;
            candidate.moveOnlyCopyRejects.push_back(moveOnlyCopy);
            if (freshBorrow)
            {
                candidate.fullExpressionBorrowArgumentOrdinals.push_back(argumentOrdinal);
            }
            if (conversion.exists() && fit != OwnershipArgumentFit::Ok)
            {
                candidate.viable = false;
            }
            if (moveOnlyCopy) candidate.viable = false;
            if (fit == OwnershipArgumentFit::Ok && !freshBorrow &&
                argumentTypes[argumentOrdinal].ownershipTransfer)
            {
                ConstructorOwnershipTransfer transfer;
                transfer.parameterOrdinal = parameterOrdinal;
                transfer.sourceLocalOrdinal =
                    argumentTypes[argumentOrdinal].ownershipSourceLocalOrdinal;
                transfer.sourceParameterOrdinal =
                    argumentTypes[argumentOrdinal].ownershipSourceParameterOrdinal;
                candidate.ownershipTransfers.push_back(transfer);
            }
            candidate.conversions.push_back(std::move(conversion));
        }
        binding.evaluatedCandidates.push_back(std::move(candidate));
    }

    if (!anyAccessible)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::string(targetIsStruct ? "struct" : "class") +
                " has constructors, but none are accessible from this construction site");
        binding.resolution = ConstructionResolutionKind::Inaccessible;
        constructions_.push_back(std::move(binding));
        return;
    }
    // C6 "struct members": a struct ALWAYS has a parameterless construction available, whether or
    // not it declares constructors of its own. This is where a struct and a class genuinely
    // differ, and the reason is not convenience: `Vector3 v;` already produces the zeroed value,
    // so refusing `new Vector3()` protects no invariant -- it only forces the same value to be
    // written a different way. C# settles it the same way and for the same reason. On a class,
    // declaring a constructor still removes the implicit default, because a class reference has
    // no meaningful "just zeroed" state to fall back on.
    if (!accessibleMatchingArity && targetIsStruct && argumentTypes.empty())
    {
        binding.resolution = ConstructionResolutionKind::ResolvedImplicitDefault;
        binding.selectedImplicitDefault = true;
        constructions_.push_back(std::move(binding));
        return;
    }
    if (!accessibleMatchingArity)
    {
        if (anyMatchingArity)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "matching constructor exists but is not accessible from this construction site");
            binding.resolution = ConstructionResolutionKind::Inaccessible;
        }
        else
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "no accessible constructor accepts " + std::to_string(argumentTypes.size()) + " argument(s)");
            binding.resolution = ConstructionResolutionKind::NoViableCandidate;
        }
        constructions_.push_back(std::move(binding));
        return;
    }

    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < binding.evaluatedCandidates.size(); ++index)
    {
        if (binding.evaluatedCandidates[index].viable)
        {
            viable.push_back(index);
        }
    }
    if (viable.empty())
    {
        // NSC-LIMIT-010. Si el unico estorbo fue el contrato de propiedad, se dice cual y
        // donde en vez del generico. La explicacion la da explainOwnershipRejection, que la
        // comparten este camino y el de `this(...)`/`base(...)`.
        if (explainOwnershipRejection(
                binding.evaluatedCandidates, argumentTypes.size(), expression, expression,
                diagnostics))
        {
            binding.resolution = ConstructionResolutionKind::NoViableCandidate;
            constructions_.push_back(std::move(binding));
            return;
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no viable constructor accepts the argument types");
        binding.resolution = ConstructionResolutionKind::NoViableCandidate;
        constructions_.push_back(std::move(binding));
        return;
    }

    std::vector<std::size_t> nonDominated;
    for (const std::size_t candidateIndex : viable)
    {
        bool dominated = false;
        for (const std::size_t otherIndex : viable)
        {
            if (candidateIndex != otherIndex &&
                betterCandidate(binding.evaluatedCandidates[otherIndex],
                                binding.evaluatedCandidates[candidateIndex]))
            {
                dominated = true;
                break;
            }
        }
        if (!dominated)
        {
            nonDominated.push_back(candidateIndex);
        }
    }

    if (nonDominated.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "ambiguous constructor invocation");
        binding.resolution = ConstructionResolutionKind::Ambiguous;
        constructions_.push_back(std::move(binding));
        return;
    }

    const ConstructorCandidateBinding& selected = binding.evaluatedCandidates[nonDominated.front()];
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
    constructions_.push_back(std::move(binding));
}

std::optional<MethodExpressionTypeBinding> ProjectConstructionModel::expressionType(
    const ast::AstNode* syntax,
    std::size_t callerMemberOrdinal) const
{
    if (methodBodyModel_ == nullptr || syntax == nullptr)
    {
        return std::nullopt;
    }
    const MethodExpressionTypeBinding* binding =
        methodBodyModel_->expressionTypeBinding(syntax, callerMemberOrdinal);
    return binding != nullptr
        ? std::optional<MethodExpressionTypeBinding>(*binding)
        : std::nullopt;
}

bool ProjectConstructionModel::explainOwnershipRejection(
    const std::vector<ConstructorCandidateBinding>& candidates,
    std::size_t argumentCount,
    const ast::AstNode* argumentListSyntax,
    const ast::AstNode* fallbackSyntax,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return false;
    const ConstructorCandidateBinding* unico = nullptr;
    std::size_t conAridad = 0;
    for (const ConstructorCandidateBinding& candidato : candidates)
    {
        if (!candidato.accessible || candidato.ownershipFits.size() != argumentCount ||
            candidato.moveOnlyCopyRejects.size() != argumentCount) continue;
        ++conAridad;
        unico = &candidato;
    }
    if (conAridad != 1 || unico == nullptr || !unico->memberOrdinal.has_value()) return false;
    if (*unico->memberOrdinal >= memberModel_->members().size()) return false;

    // La FISICA tiene que estar completa. Si alguna conversion no existe, el estorbo son los
    // tipos y hablar de `move` mandaria al lector al sitio equivocado.
    for (const ProjectImplicitConversion& conversion : unico->conversions)
    {
        if (!conversion.exists()) return false;
    }
    bool algunaObjecion = false;
    for (const OwnershipArgumentFit fit : unico->ownershipFits)
    {
        algunaObjecion = algunaObjecion || fit != OwnershipArgumentFit::Ok;
    }
    algunaObjecion = algunaObjecion || std::ranges::any_of(
        unico->moveOnlyCopyRejects, [](bool rejected) { return rejected; });
    if (!algunaObjecion) return false;

    const ClassMemberSymbol& elegido = memberModel_->members()[*unico->memberOrdinal];
    if (elegido.parameters.size() != argumentCount) return false;
    for (std::size_t index = 0; index < unico->ownershipFits.size(); ++index)
    {
        if (unico->ownershipFits[index] == OwnershipArgumentFit::Ok &&
            !unico->moveOnlyCopyRejects[index]) continue;
        const ast::AstNode* argumento =
            argumentListSyntax != nullptr && index < argumentListSyntax->children().size()
                ? argumentListSyntax->children()[index]
                : nullptr;
        const SourceRange range = argumento != nullptr
            ? argumento->range()
            : (fallbackSyntax != nullptr ? fallbackSyntax->range() : SourceRange{});
        diagnostics.report(
            unico->moveOnlyCopyRejects[index]
                ? diagnostics::DiagnosticId::NS3020InvalidValueCategory
                : diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            range,
            unico->moveOnlyCopyRejects[index]
                ? "argument " + std::to_string(index + 1) +
                    ": move-only value cannot be copied into constructor parameter '" +
                    elegido.parameters[index].name + "'; use move(argument)"
                : "argument " + std::to_string(index + 1) + ": " +
                    ownershipArgumentFitMessage(
                        unico->ownershipFits[index],
                        elegido.parameters[index].name,
                        elegido.parameters[index].typeKey));
    }
    return true;
}

bool ProjectConstructionModel::constructorAccessible(
    const ClassMemberSymbol& constructor,
    std::size_t callerMemberOrdinal,
    bool baseInitializer) const noexcept
{
    if (memberModel_ == nullptr)
    {
        return false;
    }
    if (callerMemberOrdinal >= memberModel_->members().size())
    {
        if (namespaceFunctionModel_ == nullptr) return false;
        const std::size_t functionOrdinal = callerMemberOrdinal - memberModel_->members().size();
        if (functionOrdinal >= namespaceFunctionModel_->functions().size()) return false;
        return constructorAccessibleFromProject(
            constructor,
            namespaceFunctionModel_->functions()[functionOrdinal].projectOrdinal);
    }
    return constructorAccessibleFromType(
        constructor,
        memberModel_->members()[callerMemberOrdinal].declaringTypeOrdinal,
        baseInitializer);
}

bool ProjectConstructionModel::constructorAccessibleFromProject(
    const ClassMemberSymbol& constructor,
    std::size_t callerProjectOrdinal) const noexcept
{
    if (nameLookup_ == nullptr ||
        constructor.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    switch (constructor.accessibility)
    {
        case ClassMemberAccessibility::Public:
            return true;
        case ClassMemberAccessibility::Internal:
            return nameLookup_->namedTypes()[constructor.declaringTypeOrdinal].projectOrdinal ==
                callerProjectOrdinal;
        case ClassMemberAccessibility::Private:
        case ClassMemberAccessibility::Protected:
            return false;
    }
    return false;
}

bool ProjectConstructionModel::constructorAccessibleFromType(
    const ClassMemberSymbol& constructor,
    std::size_t callerTypeOrdinal,
    bool baseInitializer) const noexcept
{
    if (nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        callerTypeOrdinal >= nameLookup_->namedTypes().size() ||
        constructor.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    if (callerTypeOrdinal == constructor.declaringTypeOrdinal)
    {
        return true;
    }

    switch (constructor.accessibility)
    {
        case ClassMemberAccessibility::Public:
            return true;
        case ClassMemberAccessibility::Internal:
            return sameProject(callerTypeOrdinal, constructor.declaringTypeOrdinal, *nameLookup_);
        case ClassMemberAccessibility::Private:
            return false;
        case ClassMemberAccessibility::Protected:
            return baseInitializer &&
                inheritanceModel_->isDerivedFrom(callerTypeOrdinal, constructor.declaringTypeOrdinal);
    }
    return false;
}

bool ProjectConstructionModel::implicitDefaultAccessible(
    std::size_t targetTypeOrdinal,
    std::size_t callerMemberOrdinal,
    bool baseInitializer) const noexcept
{
    if (memberModel_ == nullptr)
    {
        return false;
    }
    if (callerMemberOrdinal >= memberModel_->members().size())
    {
        if (namespaceFunctionModel_ == nullptr) return false;
        const std::size_t functionOrdinal = callerMemberOrdinal - memberModel_->members().size();
        if (functionOrdinal >= namespaceFunctionModel_->functions().size()) return false;
        return implicitDefaultAccessibleFromProject(
            targetTypeOrdinal,
            namespaceFunctionModel_->functions()[functionOrdinal].projectOrdinal);
    }
    return implicitDefaultAccessibleFromType(
        targetTypeOrdinal,
        memberModel_->members()[callerMemberOrdinal].declaringTypeOrdinal,
        baseInitializer);
}

bool ProjectConstructionModel::implicitDefaultAccessibleFromProject(
    std::size_t targetTypeOrdinal,
    std::size_t callerProjectOrdinal) const noexcept
{
    if (nameLookup_ == nullptr || targetTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    const NamedTypeDeclaration& target = nameLookup_->namedTypes()[targetTypeOrdinal];
    return target.accessibility == NamedTypeAccessibility::Public ||
        target.projectOrdinal == callerProjectOrdinal;
}

bool ProjectConstructionModel::implicitDefaultAccessibleFromType(
    std::size_t targetTypeOrdinal,
    std::size_t callerTypeOrdinal,
    bool baseInitializer) const noexcept
{
    if (nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        callerTypeOrdinal >= nameLookup_->namedTypes().size() ||
        targetTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return false;
    }
    if (callerTypeOrdinal == targetTypeOrdinal)
    {
        return true;
    }
    const NamedTypeDeclaration& target = nameLookup_->namedTypes()[targetTypeOrdinal];
    if (baseInitializer && inheritanceModel_->isDerivedFrom(callerTypeOrdinal, targetTypeOrdinal))
    {
        return target.accessibility == NamedTypeAccessibility::Public ||
            sameProject(callerTypeOrdinal, targetTypeOrdinal, *nameLookup_);
    }
    if (target.accessibility == NamedTypeAccessibility::Public)
    {
        return true;
    }
    return sameProject(callerTypeOrdinal, targetTypeOrdinal, *nameLookup_);
}

bool ProjectConstructionModel::betterCandidate(
    const ConstructorCandidateBinding& left,
    const ConstructorCandidateBinding& right) const noexcept
{
    if (!left.viable || !right.viable || left.conversions.size() != right.conversions.size())
    {
        return false;
    }
    bool strictlyBetter = false;
    for (std::size_t index = 0; index < left.conversions.size(); ++index)
    {
        if (left.conversions[index].rank > right.conversions[index].rank)
        {
            return false;
        }
        if (left.conversions[index].rank < right.conversions[index].rank)
        {
            strictlyBetter = true;
        }
    }
    if (!strictlyBetter && left.defaultedParameterCount != right.defaultedParameterCount)
    {
        return left.defaultedParameterCount < right.defaultedParameterCount;
    }
    if (!strictlyBetter &&
        left.usesExpandedParameterPack != right.usesExpandedParameterPack)
    {
        return !left.usesExpandedParameterPack;
    }
    return strictlyBetter;
}

const char* constructionResolutionKindName(ConstructionResolutionKind kind) noexcept
{
    switch (kind)
    {
        case ConstructionResolutionKind::ResolvedExplicit: return "resolved-explicit";
        case ConstructionResolutionKind::ResolvedImplicitDefault: return "resolved-implicit-default";
        case ConstructionResolutionKind::NoViableCandidate: return "no-viable-candidate";
        case ConstructionResolutionKind::Ambiguous: return "ambiguous";
        case ConstructionResolutionKind::Inaccessible: return "inaccessible";
        case ConstructionResolutionKind::InvalidTarget: return "invalid-target";
    }
    return "invalid-target";
}

const char* constructorInitializerKindName(ConstructorInitializerKind kind) noexcept
{
    switch (kind)
    {
        case ConstructorInitializerKind::ImplicitBase: return "implicit-base";
        case ConstructorInitializerKind::ExplicitBase: return "explicit-base";
        case ConstructorInitializerKind::ExplicitThis: return "explicit-this";
    }
    return "unknown";
}

const char* constructorInitializerResolutionKindName(ConstructorInitializerResolutionKind kind) noexcept
{
    switch (kind)
    {
        case ConstructorInitializerResolutionKind::NoChainingRequired: return "no-chaining-required";
        case ConstructorInitializerResolutionKind::ResolvedExplicit: return "resolved-explicit";
        case ConstructorInitializerResolutionKind::ResolvedImplicitDefault: return "resolved-implicit-default";
        case ConstructorInitializerResolutionKind::NoViableCandidate: return "no-viable-candidate";
        case ConstructorInitializerResolutionKind::Ambiguous: return "ambiguous";
        case ConstructorInitializerResolutionKind::Inaccessible: return "inaccessible";
        case ConstructorInitializerResolutionKind::Invalid: return "invalid";
    }
    return "invalid";
}
const char* implicitDefaultConstructorResolutionKindName(
    ImplicitDefaultConstructorResolutionKind kind) noexcept
{
    switch (kind)
    {
        case ImplicitDefaultConstructorResolutionKind::NoBaseRequired: return "no-base-required";
        case ImplicitDefaultConstructorResolutionKind::ResolvedExplicitBaseEntry: return "resolved-explicit-base-entry";
        case ImplicitDefaultConstructorResolutionKind::ResolvedImplicitBaseChain: return "resolved-implicit-base-chain";
        case ImplicitDefaultConstructorResolutionKind::NoViableBaseConstructor: return "no-viable-base-constructor";
        case ImplicitDefaultConstructorResolutionKind::InaccessibleBaseConstructor: return "inaccessible-base-constructor";
        case ImplicitDefaultConstructorResolutionKind::Invalid: return "invalid";
    }
    return "invalid";
}

}
