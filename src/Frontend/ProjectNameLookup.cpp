#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "Internal/LookupEnvironment.h"
#include "Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
ProjectNameLookup::ProjectNameLookup()
    : ownedIndexes_(std::make_unique<ProjectCompilationIndexes>()),
      indexes_(ownedIndexes_.get())
{
}

ProjectNameLookup::ProjectNameLookup(ProjectCompilationIndexes& indexes) noexcept
    : indexes_(&indexes)
{
}

ModuleDeclarationSpace::ModuleDeclarationSpace(
    std::size_t projectOrdinal,
    std::string moduleName)
    : projectOrdinal_(projectOrdinal),
      moduleName_(std::move(moduleName))
{
}

std::size_t ModuleDeclarationSpace::projectOrdinal() const noexcept
{
    return projectOrdinal_;
}

const std::string& ModuleDeclarationSpace::moduleName() const noexcept
{
    return moduleName_;
}

GlobalNamespace& ModuleDeclarationSpace::globalNamespace() noexcept
{
    return globalNamespace_;
}

const GlobalNamespace& ModuleDeclarationSpace::globalNamespace() const noexcept
{
    return globalNamespace_;
}

void ModuleDeclarationSpace::addTypeOrdinal(std::size_t ordinal)
{
    typeOrdinals_.push_back(ordinal);
}

std::span<const std::size_t> ModuleDeclarationSpace::typeOrdinals() const noexcept
{
    return typeOrdinals_;
}

void ModuleDeclarationSpace::setVisibleProjectOrdinals(std::vector<std::size_t> ordinals)
{
    visibleProjectOrdinals_ = std::move(ordinals);
}

std::span<const std::size_t> ModuleDeclarationSpace::visibleProjectOrdinals() const noexcept
{
    return visibleProjectOrdinals_;
}

bool ProjectNameLookup::build(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t workerCount)
{
    if (built_ || !modules_.empty() || !pendingTypes_.empty() || !namedTypes_.empty() ||
        !namespaceFunctions_.empty() || !namespaceStates_.empty() || !bindings_.empty())
        return false;

    initializeModules(graph);
    buildVisibility(graph);
    driver::CompilationWorkerPool workers(workerCount);
    collectDeclarationIndexes(graph, units, workers);
    finalizePredeclarations(diagnostics);
    finalizeReexports(units, diagnostics);
    indexes_->rebuildTypes(namedTypes_);
    finalizeNamespaceDeclarations();
    collectBindingIndexes(units, diagnostics, workers);
    built_ = true;
    return !diagnostics.hasErrors();
}

std::span<const std::unique_ptr<ModuleDeclarationSpace>> ProjectNameLookup::modules() const noexcept
{
    return modules_;
}

std::span<const NamedTypeDeclaration> ProjectNameLookup::namedTypes() const noexcept
{
    return namedTypes_;
}

const NamedTypeDeclaration* ProjectNameLookup::typeForOrdinal(std::size_t ordinal) const noexcept
{
    return indexes_->typeForOrdinal(ordinal);
}

std::optional<std::size_t> ProjectNameLookup::typeOrdinalForIdentity(
    std::string_view identity) const
{
    return indexes_->typeOrdinalForIdentity(identity);
}

std::span<const std::size_t> ProjectNameLookup::typeOrdinalsForName(
    std::string_view name) const
{
    return indexes_->typeOrdinalsForName(name);
}

std::span<const NamedTypeDeclarationPart> ProjectNameLookup::externalTraitImplParts() const noexcept
{
    return externalTraitImplParts_;
}

std::span<const NamespaceFunctionDeclaration> ProjectNameLookup::namespaceFunctions() const noexcept
{
    return namespaceFunctions_;
}

std::span<const NamespaceStateDeclaration> ProjectNameLookup::namespaceStates() const noexcept
{
    return namespaceStates_;
}
std::span<const TypeNameBinding> ProjectNameLookup::bindings() const noexcept
{
    return bindings_;
}

const NamedTypeDeclaration* ProjectNameLookup::resolvedDeclaration(const ast::TypeSyntax* syntax) const noexcept
{
    const auto bindingIterator = bindingBySyntax_.find(syntax);
    if (bindingIterator == bindingBySyntax_.end())
    {
        return nullptr;
    }

    const TypeNameBinding& binding = bindings_[bindingIterator->second];
    if (binding.kind != TypeNameBindingKind::Resolved || !binding.targetTypeOrdinal.has_value())
    {
        return nullptr;
    }
    return &namedTypes_[*binding.targetTypeOrdinal];
}

std::optional<std::size_t> ProjectNameLookup::resolvedTypeOrdinal(
    const ast::TypeSyntax* syntax) const noexcept
{
    if (syntax == nullptr)
    {
        return std::nullopt;
    }
    const auto iterator = bindingBySyntax_.find(syntax);
    if (iterator == bindingBySyntax_.end() || iterator->second >= bindings_.size())
    {
        return std::nullopt;
    }
    const TypeNameBinding& binding = bindings_[iterator->second];
    if (binding.kind != TypeNameBindingKind::Resolved || !binding.targetTypeOrdinal.has_value())
    {
        return std::nullopt;
    }
    return binding.targetTypeOrdinal;
}

const ExpressionTypeNameBinding* ProjectNameLookup::expressionTypeBinding(
    const ast::AstNode* expression) const noexcept
{
    const auto iterator = expressionTypeBindingBySyntax_.find(expression);
    if (iterator == expressionTypeBindingBySyntax_.end() ||
        iterator->second >= expressionTypeBindings_.size())
    {
        return nullptr;
    }
    return &expressionTypeBindings_[iterator->second];
}

std::span<const ExpressionTypeNameBinding> ProjectNameLookup::expressionTypeBindings() const noexcept
{
    return expressionTypeBindings_;
}

bool ProjectNameLookup::isBuilt() const noexcept
{
    return built_;
}

const char* namedTypeDeclarationKindName(NamedTypeDeclarationKind kind) noexcept
{
    switch (kind)
    {
        case NamedTypeDeclarationKind::Class:
            return "class";
        case NamedTypeDeclarationKind::Interface:
            return "interface";
        case NamedTypeDeclarationKind::Struct:
            return "struct";
        case NamedTypeDeclarationKind::Enum:
            return "enum";
        case NamedTypeDeclarationKind::Delegate:
            return "delegate";
    }
    return "unknown";
}

const char* namedTypeAccessibilityName(NamedTypeAccessibility accessibility) noexcept
{
    switch (accessibility)
    {
        case NamedTypeAccessibility::Private:
            return "private";
        case NamedTypeAccessibility::Internal:
            return "internal";
        case NamedTypeAccessibility::Package:
            return "package";
        case NamedTypeAccessibility::Protected:
            return "protected";
        case NamedTypeAccessibility::Public:
            return "public";
    }
    return "unknown";
}

const char* typeNameBindingKindName(TypeNameBindingKind kind) noexcept
{
    switch (kind)
    {
        case TypeNameBindingKind::Builtin:
            return "builtin";
        case TypeNameBindingKind::Resolved:
            return "resolved";
        case TypeNameBindingKind::Unresolved:
            return "unresolved";
        case TypeNameBindingKind::Ambiguous:
            return "ambiguous";
    }
    return "unknown";
}
}
