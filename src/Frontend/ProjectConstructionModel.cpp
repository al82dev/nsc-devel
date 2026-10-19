#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "Internal/NamedConstructorArgumentMap.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
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
