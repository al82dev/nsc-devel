#include "nativesharp/Frontend/ProjectNameLookup.h"

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
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
std::vector<std::string> splitQualifiedName(const std::string& name)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin <= name.size())
    {
        const std::size_t dot = name.find('.', begin);
        const std::size_t end = dot == std::string::npos ? name.size() : dot;
        if (end > begin)
        {
            segments.push_back(name.substr(begin, end - begin));
        }
        if (dot == std::string::npos)
        {
            break;
        }
        begin = dot + 1;
    }
    return segments;
}

std::optional<std::string> expressionQualifiedName(const ast::AstNode* expression)
{
    if (expression == nullptr)
    {
        return std::nullopt;
    }
    if (expression->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        return expression->text().empty()
            ? std::nullopt
            : std::optional<std::string>(expression->text());
    }
    if (expression->kind() != ast::AstNodeKind::MemberExpression ||
        expression->first() == nullptr || expression->text().empty())
    {
        return std::nullopt;
    }

    std::optional<std::string> prefix = expressionQualifiedName(expression->first());
    if (!prefix.has_value())
    {
        return std::nullopt;
    }
    prefix->push_back('.');
    *prefix += expression->text();
    return prefix;
}

std::string joinSegments(std::span<const std::string> segments)
{
    std::string result;
    for (const std::string& segment : segments)
    {
        if (!result.empty())
        {
            result.push_back('.');
        }
        result += segment;
    }
    return result;
}

std::string appendQualified(std::span<const std::string> prefix, const std::string& suffix)
{
    std::string result = joinSegments(prefix);
    if (!result.empty() && !suffix.empty())
    {
        result.push_back('.');
    }
    result += suffix;
    return result;
}

std::string pathKey(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode)
    {
        canonical = std::filesystem::absolute(path).lexically_normal();
    }

    std::string key = canonical.generic_string();
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}

void appendUnique(std::vector<std::size_t>& destination, std::span<const std::size_t> source)
{
    for (const std::size_t value : source)
    {
        if (std::find(destination.begin(), destination.end(), value) == destination.end())
        {
            destination.push_back(value);
        }
    }
}

std::string candidateList(
    std::span<const std::size_t> candidates,
    std::span<const NamedTypeDeclaration> declarations)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (index != 0)
        {
            output << ", ";
        }
        output << declarations[candidates[index]].identity.key();
    }
    return output.str();
}

bool isNamedTypeDeclaration(ast::AstNodeKind kind) noexcept
{
    return kind == ast::AstNodeKind::ClassDeclaration ||
        kind == ast::AstNodeKind::InterfaceDeclaration ||
        kind == ast::AstNodeKind::StructDeclaration ||
        kind == ast::AstNodeKind::EnumDeclaration ||
        kind == ast::AstNodeKind::DelegateDeclaration;
}

bool isTraitImplementationPart(const NamedTypeDeclarationPart& part) noexcept
{
    return part.syntax != nullptr && part.syntax->kind() == ast::AstNodeKind::ClassDeclaration &&
        part.syntax->auxiliaryText() == "ns3-trait-impl";
}

NamedTypeDeclarationKind namedTypeKind(ast::AstNodeKind kind) noexcept
{
    if (kind == ast::AstNodeKind::ClassDeclaration)
    {
        return NamedTypeDeclarationKind::Class;
    }
    if (kind == ast::AstNodeKind::InterfaceDeclaration)
    {
        return NamedTypeDeclarationKind::Interface;
    }
    if (kind == ast::AstNodeKind::EnumDeclaration)
    {
        return NamedTypeDeclarationKind::Enum;
    }
    if (kind == ast::AstNodeKind::DelegateDeclaration)
    {
        return NamedTypeDeclarationKind::Delegate;
    }
    return NamedTypeDeclarationKind::Struct;
}

compilation::TypeDeclarationKind compilationTypeKind(NamedTypeDeclarationKind kind) noexcept
{
    switch (kind)
    {
        case NamedTypeDeclarationKind::Class:
            return compilation::TypeDeclarationKind::Class;
        case NamedTypeDeclarationKind::Interface:
            return compilation::TypeDeclarationKind::Interface;
        case NamedTypeDeclarationKind::Struct:
            return compilation::TypeDeclarationKind::Struct;
        case NamedTypeDeclarationKind::Enum:
            return compilation::TypeDeclarationKind::Enum;
        case NamedTypeDeclarationKind::Delegate:
            return compilation::TypeDeclarationKind::Delegate;
    }
    return compilation::TypeDeclarationKind::Class;
}

compilation::DeclaredAccessibility declaredAccessibility(
    const NamedTypeDeclarationPart& part) noexcept
{
    if (!part.hasExplicitAccessibility)
    {
        return compilation::DeclaredAccessibility::Unspecified;
    }
    return part.accessibility == NamedTypeAccessibility::Public
        ? compilation::DeclaredAccessibility::Public
        : compilation::DeclaredAccessibility::Internal;
}

NamedTypeAccessibility namedAccessibility(
    compilation::DeclaredAccessibility accessibility) noexcept
{
    return accessibility == compilation::DeclaredAccessibility::Public
        ? NamedTypeAccessibility::Public
        : NamedTypeAccessibility::Internal;
}

std::string sourcePathForUnit(
    const compilation::ProjectGraph& graph,
    const ProjectSyntaxUnit& unit)
{
    const std::span<const compilation::Project> projects = graph.projects();
    if (unit.projectOrdinal() < projects.size())
    {
        for (const compilation::ProjectSource& source :
             projects[unit.projectOrdinal()].sourceSet().sources())
        {
            if (source.ordinal == unit.sourceOrdinal())
            {
                return source.absolutePath.generic_string();
            }
        }
    }
    return "<file-id:" + std::to_string(unit.fileId().value()) + ">";
}
}

struct ProjectNameLookup::LookupEnvironment final
{
    std::vector<std::string> importedNamespaces;
    std::unordered_map<std::string, std::string> aliases;
    // C3 "FFI aliases": a `using` alias whose target is a TYPE rather than a namespace, kept as
    // the parsed type syntax so the expansion can carry pointers, fixed arrays and builtins --
    // everything a C typedef says -- and not just a name. `aliases` above stays the namespace
    // half; a given alias name is in exactly one of the two maps.
    std::unordered_map<std::string, const ast::TypeSyntax*> typeAliases;
    // NSC-LIMIT-009. Los tipos dentro de los que se esta escribiendo, del mas exterior al mas
    // interior. Es lo que convierte `Item` en `ns.Outer.Item` sin que nadie lo escriba, y lo que
    // permite decidir si un anidado `private` se ve desde aqui.
    std::vector<std::size_t> enclosingTypeOrdinals;
};

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
    {
        return false;
    }

    const std::span<const compilation::Project> projects = graph.projects();
    modules_.reserve(projects.size());
    typesByModule_.resize(projects.size());
    nestedSourceNamesByModule_.resize(projects.size());
    reexportedTypesByModule_.resize(projects.size());
    packageByProject_.resize(projects.size());
    friendsByProject_.resize(projects.size());
    for (std::size_t projectOrdinal = 0; projectOrdinal < projects.size(); ++projectOrdinal)
    {
        modules_.push_back(std::make_unique<ModuleDeclarationSpace>(
            projectOrdinal,
            projects[projectOrdinal].moduleName()));
        packageByProject_[projectOrdinal] = std::string(projects[projectOrdinal].packageName());
        friendsByProject_[projectOrdinal].assign(
            projects[projectOrdinal].friendModules().begin(),
            projects[projectOrdinal].friendModules().end());
    }

    buildVisibility(graph);

    driver::CompilationWorkerPool workers(workerCount);
    std::vector<UnitDeclarationIndex> declarationIndexes(units.size());
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        if (units[index] == nullptr || units[index]->syntaxTree() == nullptr) continue;
        workers.submit([&, index](std::size_t) {
            indexUnitDeclarations(
                *units[index], sourcePathForUnit(graph, *units[index]), declarationIndexes[index]);
        });
    }
    workers.wait();
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        if (units[index] == nullptr) continue;
        UnitDeclarationIndex& unitIndex = declarationIndexes[index];
        for (const std::vector<std::string>& namespacePath : unitIndex.namespaces)
        {
            modules_[units[index]->projectOrdinal()]->globalNamespace().ensureNamespace(namespacePath);
        }
        pendingTypes_.insert(
            pendingTypes_.end(),
            std::make_move_iterator(unitIndex.pendingTypes.begin()),
            std::make_move_iterator(unitIndex.pendingTypes.end()));
        namespaceFunctions_.insert(
            namespaceFunctions_.end(),
            std::make_move_iterator(unitIndex.namespaceFunctions.begin()),
            std::make_move_iterator(unitIndex.namespaceFunctions.end()));
        namespaceStates_.insert(
            namespaceStates_.end(),
            std::make_move_iterator(unitIndex.namespaceStates.begin()),
            std::make_move_iterator(unitIndex.namespaceStates.end()));
    }
    finalizePredeclarations(diagnostics);
    finalizeReexports(units, diagnostics);
    std::sort(
        namespaceFunctions_.begin(),
        namespaceFunctions_.end(),
        [](const NamespaceFunctionDeclaration& left, const NamespaceFunctionDeclaration& right)
        {
            if (left.moduleName != right.moduleName) return left.moduleName < right.moduleName;
            if (left.namespaceName != right.namespaceName) return left.namespaceName < right.namespaceName;
            if (left.name != right.name) return left.name < right.name;
            if (left.sourceOrdinal != right.sourceOrdinal) return left.sourceOrdinal < right.sourceOrdinal;
            return left.range.begin().offset() < right.range.begin().offset();
        });
    std::sort(
        namespaceStates_.begin(),
        namespaceStates_.end(),
        [](const NamespaceStateDeclaration& left, const NamespaceStateDeclaration& right)
        {
            if (left.moduleName != right.moduleName) return left.moduleName < right.moduleName;
            if (left.namespaceName != right.namespaceName) return left.namespaceName < right.namespaceName;
            if (left.name != right.name) return left.name < right.name;
            if (left.sourceOrdinal != right.sourceOrdinal) return left.sourceOrdinal < right.sourceOrdinal;
            return left.range.begin().offset() < right.range.begin().offset();
        });
    for (std::size_t ordinal = 0; ordinal < namespaceFunctions_.size(); ++ordinal)
    {
        const NamespaceFunctionDeclaration& function = namespaceFunctions_[ordinal];
        std::vector<std::string> segments = splitQualifiedName(function.namespaceName);
        modules_[function.projectOrdinal]->globalNamespace().ensureNamespace(segments).addFunctionOrdinal(ordinal);
    }

    std::vector<UnitBindingIndex> bindingIndexes(units.size());
    std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
    localDiagnostics.reserve(units.size());
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        localDiagnostics.emplace_back(diagnostics.errorLimit());
        if (units[index] == nullptr || units[index]->syntaxTree() == nullptr) continue;
        workers.submit([&, index](std::size_t) {
            bindUnit(*units[index], localDiagnostics[index], bindingIndexes[index]);
        });
    }
    workers.wait();
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        UnitBindingIndex& unitIndex = bindingIndexes[index];
        const std::size_t bindingBase = bindings_.size();
        bindings_.insert(
            bindings_.end(),
            std::make_move_iterator(unitIndex.bindings.begin()),
            std::make_move_iterator(unitIndex.bindings.end()));
        for (const auto& [syntax, ordinal] : unitIndex.bindingBySyntax)
        {
            bindingBySyntax_.emplace(syntax, bindingBase + ordinal);
        }
        const std::size_t expressionBase = expressionTypeBindings_.size();
        expressionTypeBindings_.insert(
            expressionTypeBindings_.end(),
            std::make_move_iterator(unitIndex.expressionTypeBindings.begin()),
            std::make_move_iterator(unitIndex.expressionTypeBindings.end()));
        for (const auto& [syntax, ordinal] : unitIndex.expressionTypeBindingBySyntax)
        {
            expressionTypeBindingBySyntax_.emplace(syntax, expressionBase + ordinal);
        }
    }
    diagnostics.mergeDeterministically(localDiagnostics);

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

void ProjectNameLookup::buildVisibility(const compilation::ProjectGraph& graph)
{
    const std::span<const compilation::Project> projects = graph.projects();
    std::unordered_map<std::string, std::size_t> projectByPath;
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        projectByPath.emplace(pathKey(projects[ordinal].projectFile()), ordinal);
    }

    for (std::size_t start = 0; start < projects.size(); ++start)
    {
        std::vector<bool> reachable(projects.size(), false);
        std::function<void(std::size_t)> visit = [&](std::size_t ordinal)
        {
            for (const std::filesystem::path& reference : projects[ordinal].projectReferences())
            {
                const auto iterator = projectByPath.find(pathKey(reference));
                if (iterator == projectByPath.end())
                {
                    continue;
                }
                const std::size_t dependency = iterator->second;
                if (reachable[dependency])
                {
                    continue;
                }
                reachable[dependency] = true;
                visit(dependency);
            }
        };
        visit(start);

        std::vector<std::size_t> visible;
        for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
        {
            if (reachable[ordinal])
            {
                visible.push_back(ordinal);
            }
        }
        modules_[start]->setVisibleProjectOrdinals(std::move(visible));
    }
}

void ProjectNameLookup::indexUnitDeclarations(
    const ProjectSyntaxUnit& unit,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    predeclareNode(unit, unit.syntaxTree(), {}, {}, sourcePath, index);
}

void ProjectNameLookup::predeclareNode(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    if (node == nullptr)
    {
        return;
    }

    if (!node->genericParameters().empty() && isNamedTypeDeclaration(node->kind()))
    {
        return;
    }

    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        std::vector<std::string> nestedNamespace = namespaceSegments;
        const std::vector<std::string> declaredSegments = splitQualifiedName(node->text());
        nestedNamespace.insert(
            nestedNamespace.end(),
            declaredSegments.begin(),
            declaredSegments.end());
        index.namespaces.push_back(nestedNamespace);
        for (const ast::AstNode* child : node->children())
        {
            predeclareNode(unit, child, nestedNamespace, containingTypes, sourcePath, index);
        }
        return;
    }

    if ((node->kind() == ast::AstNodeKind::FunctionDeclaration ||
         (node->kind() == ast::AstNodeKind::MethodDeclaration &&
          node->auxiliaryText().find("ns3-extension-static") != std::string::npos)) &&
        node->genericParameters().empty())
    {
        index.namespaceFunctions.push_back(NamespaceFunctionDeclaration{
            node->tokenKind() == lexer::TokenKind::KwPublic
                ? NamespaceFunctionAccessibility::Public
                : NamespaceFunctionAccessibility::Internal,
            unit.projectOrdinal(),
            unit.sourceOrdinal(),
            unit.fileId(),
            node->range(),
            node,
            sourcePath,
            std::string(unit.moduleName()),
            joinSegments(namespaceSegments),
            node->text(),
            node->first() != nullptr,
            unit.isPrecompiled()});
        return;
    }

    if (node->kind() == ast::AstNodeKind::GlobalVariableDeclaration &&
        !namespaceSegments.empty())
    {
        index.namespaceStates.push_back(NamespaceStateDeclaration{
            node->tokenKind() == lexer::TokenKind::KwPublic
                ? NamespaceFunctionAccessibility::Public
                : NamespaceFunctionAccessibility::Internal,
            unit.projectOrdinal(),
            unit.sourceOrdinal(),
            unit.fileId(),
            node->range(),
            node,
            sourcePath,
            std::string(unit.moduleName()),
            joinSegments(namespaceSegments),
            node->text(),
            node->flag(),
            unit.isPrecompiled()});
        return;
    }
    if (node->kind() == ast::AstNodeKind::ExtensionDeclaration)
    {
        for (const ast::AstNode* child : node->children())
        {
            predeclareNode(
                unit, child, namespaceSegments, containingTypes, sourcePath, index);
        }
        return;
    }
    if (isNamedTypeDeclaration(node->kind()))
    {
        const bool isNested = !containingTypes.empty();
        compilation::TypeIdentity identity(
            std::string(unit.moduleName()),
            namespaceSegments,
            containingTypes,
            node->text(),
            0);
        const bool hasExplicitAccessibility =
            node->tokenKind() == lexer::TokenKind::KwPublic ||
            node->tokenKind() == lexer::TokenKind::KwInternal ||
            node->tokenKind() == lexer::TokenKind::KwPrivate ||
            node->tokenKind() == lexer::TokenKind::KwProtected;
        // NSC-LIMIT-009. EL VALOR POR OMISION DEPENDE DEL SITIO, y esa es toda la regla: un tipo
        // de namespace sin modificador es `internal` --visible en su modulo-- y uno ANIDADO sin
        // modificador es `private`, visible solo dentro de su contenedor. Es la regla de C#, y
        // convertir el segundo en el primero seria publicar dentro del modulo cada tipo interno
        // que alguien escribiera para uso propio.
        NamedTypeAccessibility accessibility =
            isNested ? NamedTypeAccessibility::Private : NamedTypeAccessibility::Internal;
        if (node->tokenKind() == lexer::TokenKind::KwPublic)
        {
            accessibility = NamedTypeAccessibility::Public;
        }
        else if (node->tokenKind() == lexer::TokenKind::KwInternal)
        {
            accessibility = node->auxiliaryText().find("visibility=package") != std::string::npos
                ? NamedTypeAccessibility::Package
                : NamedTypeAccessibility::Internal;
        }
        else if (node->tokenKind() == lexer::TokenKind::KwProtected)
        {
            accessibility = NamedTypeAccessibility::Protected;
        }
        else if (node->tokenKind() == lexer::TokenKind::KwPrivate)
        {
            accessibility = NamedTypeAccessibility::Private;
        }

        NamedTypeDeclarationPart part{
            namedTypeKind(node->kind()),
            accessibility,
            hasExplicitAccessibility,
            isNested,
            (node->kind() == ast::AstNodeKind::ClassDeclaration ||
             node->kind() == ast::AstNodeKind::InterfaceDeclaration) && node->flag(),
            node->kind() == ast::AstNodeKind::ClassDeclaration &&
                node->hasModifier(lexer::TokenKind::KwAbstract),
            node->kind() == ast::AstNodeKind::ClassDeclaration &&
                node->hasModifier(lexer::TokenKind::KwSealed),
            node->kind() == ast::AstNodeKind::ClassDeclaration &&
                node->hasModifier(lexer::TokenKind::KwStatic),
            unit.isPrecompiled(),
            unit.projectOrdinal(),
            unit.sourceOrdinal(),
            unit.fileId(),
            node->range(),
            node,
            sourcePath};
        index.pendingTypes.push_back(PendingTypeDeclaration{std::move(identity), std::move(part)});

        // NSC-LIMIT-009. Y AHORA SE BAJA. La version anterior devolvia aqui, que es exactamente
        // por lo que un tipo dentro de otro no existia para nadie: se parseaba, se colgaba del
        // arbol, y ninguna fase volvia a mirarlo.
        //
        // La aridad de cada segmento se toma del nodo --no se supone cero-- aunque hoy solo pueda
        // valer cero: el parser rechaza un anidado generico, y esta linea es lo que hara que el
        // dia que deje de rechazarlo la identidad salga completa en vez de ambigua.
        std::vector<compilation::TypeNameSegment> nestedChain = containingTypes;
        nestedChain.push_back(compilation::TypeNameSegment{
            node->text(), static_cast<std::uint32_t>(node->genericParameters().size())});
        for (const ast::AstNode* child : node->children())
        {
            if (isNamedTypeDeclaration(child->kind()))
            {
                predeclareNode(unit, child, namespaceSegments, nestedChain, sourcePath, index);
            }
        }
        return;
    }

    if (node->kind() == ast::AstNodeKind::CompilationUnit)
    {
        for (const ast::AstNode* child : node->children())
        {
            predeclareNode(unit, child, namespaceSegments, containingTypes, sourcePath, index);
        }
    }
}

void ProjectNameLookup::finalizePredeclarations(
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::sort(
        pendingTypes_.begin(),
        pendingTypes_.end(),
        [](const PendingTypeDeclaration& left, const PendingTypeDeclaration& right)
        {
            if (left.identity.key() != right.identity.key())
            {
                return left.identity.key() < right.identity.key();
            }
            if (left.part.sourceOrdinal != right.part.sourceOrdinal)
            {
                return left.part.sourceOrdinal < right.part.sourceOrdinal;
            }
            return left.part.range.begin().offset() < right.part.range.begin().offset();
        });

    std::size_t groupBegin = 0;
    while (groupBegin < pendingTypes_.size())
    {
        std::size_t groupEnd = groupBegin + 1;
        while (groupEnd < pendingTypes_.size() &&
               pendingTypes_[groupEnd].identity == pendingTypes_[groupBegin].identity)
        {
            ++groupEnd;
        }

        compilation::PartialTypeAggregator aggregator;
        std::string aggregationError;
        bool collected = true;
        std::optional<std::size_t> primaryIndex;
        for (std::size_t index = groupBegin; index < groupEnd; ++index)
        {
            const PendingTypeDeclaration& pending = pendingTypes_[index];
            const NamedTypeDeclarationPart& part = pending.part;
            if (isTraitImplementationPart(part))
            {
                continue;
            }
            if (!primaryIndex.has_value())
            {
                primaryIndex = index;
            }
            compilation::TypePartDescriptor descriptor{
                pending.identity,
                compilationTypeKind(part.kind),
                declaredAccessibility(part),
                part.isPartial,
                part.isAbstract,
                part.isSealed,
                part.isStatic,
                std::nullopt,
                part.sourceOrdinal,
                part.range.begin().offset(),
                part.sourcePath};
            if (!aggregator.addPart(std::move(descriptor), aggregationError))
            {
                collected = false;
                break;
            }
        }

        if (!primaryIndex.has_value())
        {
            // NS304-T08-F05: orphan rule — the module may own the trait even when it does not own
            // the target type. `impl LocalTrait for ExternalType` is coherent when the trait is
            // declared by this module; owning neither is the orphan rejection.
            bool traitOwned = false;
            for (std::size_t index = groupBegin; index < groupEnd; ++index)
            {
                const PendingTypeDeclaration& pending = pendingTypes_[index];
                if (!isTraitImplementationPart(pending.part) || pending.part.syntax == nullptr)
                    continue;
                const ast::TypeSyntax* trait = pending.part.syntax->typeSyntax();
                if (trait == nullptr || trait->baseName().empty()) continue;
                const std::string& traitName = trait->baseName();
                const std::size_t implProject = pending.part.projectOrdinal;
                for (const PendingTypeDeclaration& local : pendingTypes_)
                {
                    if (local.part.syntax == nullptr ||
                        local.part.syntax->kind() != ast::AstNodeKind::InterfaceDeclaration)
                        continue;
                    if (local.part.projectOrdinal == implProject &&
                        local.part.syntax->text() == traitName)
                    {
                        traitOwned = true;
                        break;
                    }
                }
                if (traitOwned) break;
            }
            if (!traitOwned)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidBaseType,
                    diagnostics::DiagnosticSeverity::Error,
                    pendingTypes_[groupBegin].part.range,
                    "orphan impl: the module owns neither the target type '" +
                        pendingTypes_[groupBegin].identity.fullyQualifiedName() +
                        "' nor the trait");
            }
            else
            {
                // NS304-T08-F05: the module owns the trait, so the conformance over the external
                // target is coherent. Keep the trait-impl parts for the member model to register
                // on the external target's member surface.
                for (std::size_t index = groupBegin; index < groupEnd; ++index)
                {
                    const PendingTypeDeclaration& pending = pendingTypes_[index];
                    if (isTraitImplementationPart(pending.part))
                        externalTraitImplParts_.push_back(pending.part);
                }
            }
            groupBegin = groupEnd;
            continue;
        }

        const PendingTypeDeclaration& primary = pendingTypes_[*primaryIndex];
        const bool hasTraitImplementation = std::any_of(
            pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupBegin),
            pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupEnd),
            [](const PendingTypeDeclaration& pending)
            {
                return isTraitImplementationPart(pending.part);
            });
        if (hasTraitImplementation &&
            (primary.part.kind != NamedTypeDeclarationKind::Class || primary.part.isStatic))
        {
            const auto implementation = std::find_if(
                pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupBegin),
                pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupEnd),
                [](const PendingTypeDeclaration& pending)
                {
                    return isTraitImplementationPart(pending.part);
                });
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidBaseType,
                diagnostics::DiagnosticSeverity::Error,
                implementation->part.range,
                "a trait impl target must be a non-static class declared by this module");
        }
        const bool aggregated = collected && aggregator.finalize(aggregationError);
        if (!aggregated)
        {
            const std::size_t diagnosticIndex = *primaryIndex;
            const diagnostics::DiagnosticId diagnosticId = aggregationError.starts_with(
                "duplicate non-partial type declaration")
                    ? diagnostics::DiagnosticId::DuplicateSymbol
                    : diagnostics::DiagnosticId::PartialTypeConflict;
            diagnostics.report(
                diagnosticId,
                diagnostics::DiagnosticSeverity::Error,
                pendingTypes_[diagnosticIndex].part.range,
                aggregationError);
            if (groupEnd - groupBegin > 1)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::PreviousDeclaration,
                    diagnostics::DiagnosticSeverity::Note,
                    primary.part.range,
                    "previous declaration of this type is here");
            }
        }
        NamedTypeDeclarationKind finalKind = primary.part.kind;
        NamedTypeAccessibility finalAccessibility = primary.part.accessibility;
        bool finalPartial = primary.part.isPartial;
        bool finalAbstract = primary.part.isAbstract;
        bool finalSealed = primary.part.isSealed;
        bool finalStatic = primary.part.isStatic;
        // NSC-LIMIT-009. El agregador de partes solo conoce internal/public --es lo unico que un
        // tipo de namespace puede ser-- asi que para un ANIDADO se conserva la accesibilidad de
        // su primera parte. El agregador si aporta las demas propiedades: las clases an anidadas
        // `partial` se usan al reconstruir propietarios de plantillas desde NSMETA y deben unir
        // static/abstract/sealed igual que una clase de namespace.
        if (aggregated && !aggregator.types().empty())
        {
            finalAccessibility = primary.part.isNested
                ? primary.part.accessibility
                : namedAccessibility(aggregator.types().front().accessibility);
            if (!primary.part.isNested)
            {
                const bool anyPackage = std::any_of(
                    pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupBegin),
                    pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupEnd),
                    [](const PendingTypeDeclaration& pending)
                    {
                        return pending.part.accessibility == NamedTypeAccessibility::Package;
                    });
                const bool allPackage = std::all_of(
                    pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupBegin),
                    pendingTypes_.begin() + static_cast<std::ptrdiff_t>(groupEnd),
                    [](const PendingTypeDeclaration& pending)
                    {
                        return pending.part.accessibility == NamedTypeAccessibility::Package;
                    });
                if (anyPackage && !allPackage)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::PartialTypeConflict,
                        diagnostics::DiagnosticSeverity::Error,
                        primary.part.range,
                        "all parts of a package-visible partial type must declare package visibility");
                }
                if (allPackage) finalAccessibility = NamedTypeAccessibility::Package;
            }
            finalAbstract = aggregator.types().front().isAbstract;
            finalSealed = aggregator.types().front().isSealed;
            finalStatic = aggregator.types().front().isStatic;
            finalPartial = std::any_of(
                aggregator.types().front().parts.begin(),
                aggregator.types().front().parts.end(),
                [](const compilation::TypePartDescriptor& part)
                {
                    return part.isPartial;
                });
        }
        else if (!primary.part.hasExplicitAccessibility)
        {
            finalAccessibility = NamedTypeAccessibility::Internal;
        }

        std::vector<NamedTypeDeclarationPart> declarationParts;
        declarationParts.reserve(groupEnd - groupBegin);
        if (aggregated)
        {
            for (std::size_t index = groupBegin; index < groupEnd; ++index)
            {
                declarationParts.push_back(pendingTypes_[index].part);
            }
        }
        else
        {
            declarationParts.push_back(primary.part);
        }

        const std::size_t typeOrdinal = namedTypes_.size();
        namedTypes_.push_back(NamedTypeDeclaration{
            primary.identity,
            finalKind,
            finalAccessibility,
            std::nullopt,
            finalPartial,
            finalAbstract,
            finalSealed,
            finalStatic,
            primary.part.isPrecompiled,
            primary.part.projectOrdinal,
            primary.part.sourceOrdinal,
            primary.part.fileId,
            primary.part.range,
            primary.part.syntax,
            std::move(declarationParts)});

        const std::string fullyQualifiedName = primary.identity.fullyQualifiedName();
        typesByModule_[primary.part.projectOrdinal].emplace(fullyQualifiedName, typeOrdinal);
        // NSC-LIMIT-009. La forma FUENTE, con puntos, en un indice aparte. La clave canonica de
        // arriba lleva `+` y no se toca: son dos preguntas distintas --«que tipo es este» y «que
        // ha escrito el programador»-- y colapsarlas en un mapa haria indistinguible un anidado
        // `ns.Outer.Item` de un tipo `Item` en el namespace `ns.Outer`, que es precisamente la
        // ambiguedad que hay que poder detectar.
        if (!primary.identity.containingTypes().empty())
        {
            std::string sourceName;
            for (const std::string& segment : primary.identity.namespaceSegments())
            {
                if (!sourceName.empty()) sourceName.push_back('.');
                sourceName += segment;
            }
            for (const compilation::TypeNameSegment& segment : primary.identity.containingTypes())
            {
                if (!sourceName.empty()) sourceName.push_back('.');
                sourceName += segment.name;
            }
            if (!sourceName.empty()) sourceName.push_back('.');
            sourceName += primary.identity.name();
            nestedSourceNamesByModule_[primary.part.projectOrdinal].emplace(
                sourceName, typeOrdinal);
        }
        for (const NamedTypeDeclarationPart& part : namedTypes_[typeOrdinal].parts)
        {
            if (part.syntax != nullptr)
            {
                typeOrdinalBySyntax_[part.syntax] = typeOrdinal;
            }
        }
        ModuleDeclarationSpace& module = *modules_[primary.part.projectOrdinal];
        module.addTypeOrdinal(typeOrdinal);
        NamespaceSymbol& namespaceSymbol = module.globalNamespace().ensureNamespace(
            primary.identity.namespaceSegments());
        namespaceSymbol.addTypeOrdinal(typeOrdinal);

        groupBegin = groupEnd;
    }

    // NSC-LIMIT-009. Segunda pasada: el ORDINAL del contenedor de cada anidado. Se hace despues y
    // no durante el bucle porque el contenedor puede no estar registrado todavia cuando se
    // registra el anidado --el orden lo fija la ordenacion por clave, no el anidamiento-- y
    // depender de ese orden seria depender de que `+` ordene por detras del fin de cadena.
    for (NamedTypeDeclaration& type : namedTypes_)
    {
        if (type.identity.containingTypes().empty())
        {
            continue;
        }
        std::vector<compilation::TypeNameSegment> outerChain = type.identity.containingTypes();
        const compilation::TypeNameSegment outer = outerChain.back();
        outerChain.pop_back();
        const compilation::TypeIdentity containerIdentity(
            type.identity.moduleName(),
            type.identity.namespaceSegments(),
            outerChain,
            outer.name,
            outer.arity);
        const auto iterator =
            typesByModule_[type.projectOrdinal].find(containerIdentity.fullyQualifiedName());
        if (iterator != typesByModule_[type.projectOrdinal].end())
        {
            type.containingTypeOrdinal = iterator->second;
        }
    }
}

void ProjectNameLookup::finalizeReexports(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    struct PendingReexport final
    {
        std::size_t ownerProjectOrdinal = 0;
        std::string publicName;
        std::string targetName;
        SourceRange range;
    };
    std::vector<PendingReexport> pending;
    const auto collect = [&](const auto& self,
                             const ProjectSyntaxUnit& unit,
                             const ast::AstNode* node,
                             const std::string& namespaceName) -> void
    {
        if (node == nullptr) return;
        std::string childNamespace = namespaceName;
        if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        {
            if (!childNamespace.empty()) childNamespace.push_back('.');
            childNamespace += node->text();
        }
        if (node->flag() && node->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            std::string publicName = namespaceName;
            if (!publicName.empty()) publicName.push_back('.');
            publicName += node->text();
            pending.push_back(PendingReexport{
                unit.projectOrdinal(), std::move(publicName), node->auxiliaryText(), node->range()});
        }
        else if (node->flag() && node->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::string prefix = node->text() + ".";
            if (unit.projectOrdinal() < modules_.size())
            {
                for (const std::size_t visible :
                     modules_[unit.projectOrdinal()]->visibleProjectOrdinals())
                {
                    if (visible >= typesByModule_.size()) continue;
                    for (const auto& [name, ordinal] : typesByModule_[visible])
                    {
                        static_cast<void>(ordinal);
                        if (name.starts_with(prefix))
                            pending.push_back(PendingReexport{
                                unit.projectOrdinal(), name, name, node->range()});
                    }
                }
            }
        }
        for (const ast::AstNode* child : node->children())
            self(self, unit, child, childNamespace);
    };
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr) collect(collect, *unit, unit->syntaxTree(), std::string{});

    std::ranges::sort(pending, [](const PendingReexport& left, const PendingReexport& right)
    {
        return std::tie(left.ownerProjectOrdinal, left.publicName, left.targetName) <
            std::tie(right.ownerProjectOrdinal, right.publicName, right.targetName);
    });
    for (const PendingReexport& item : pending)
    {
        if (item.ownerProjectOrdinal >= modules_.size()) continue;
        std::vector<std::size_t> candidates;
        const auto append = [&](std::size_t moduleOrdinal)
        {
            const std::optional<std::size_t> found = findTypeInModule(moduleOrdinal, item.targetName);
            if (found.has_value() && std::find(candidates.begin(), candidates.end(), *found) ==
                    candidates.end())
                candidates.push_back(*found);
        };
        append(item.ownerProjectOrdinal);
        for (const std::size_t visible : modules_[item.ownerProjectOrdinal]->visibleProjectOrdinals())
            append(visible);
        if (candidates.size() != 1U)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::UnknownTypeName,
                diagnostics::DiagnosticSeverity::Error,
                item.range,
                candidates.empty()
                    ? "re-export target '" + item.targetName + "' does not name a visible type"
                    : "re-export target '" + item.targetName + "' is ambiguous");
            continue;
        }
        const std::size_t target = candidates.front();
        bool publiclyExportable = true;
        std::optional<std::size_t> current = target;
        while (current.has_value() && *current < namedTypes_.size())
        {
            if (namedTypes_[*current].accessibility != NamedTypeAccessibility::Public)
            {
                publiclyExportable = false;
                break;
            }
            current = namedTypes_[*current].containingTypeOrdinal;
        }
        if (!publiclyExportable)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::UnknownTypeName,
                diagnostics::DiagnosticSeverity::Error,
                item.range,
                "re-export target '" + item.targetName + "' is not public");
            continue;
        }
        if (typesByModule_[item.ownerProjectOrdinal].contains(item.publicName) ||
            nestedSourceNamesByModule_[item.ownerProjectOrdinal].contains(item.publicName))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                item.range,
                "re-export name '" + item.publicName + "' conflicts with a declaration");
            continue;
        }
        auto& table = reexportedTypesByModule_[item.ownerProjectOrdinal];
        const auto [entry, inserted] = table.emplace(item.publicName, target);
        if (!inserted && entry->second != target)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                item.range,
                "re-export name '" + item.publicName + "' resolves to multiple identities");
        }
    }
}

void ProjectNameLookup::bindUnit(
    const ProjectSyntaxUnit& unit,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    const LookupEnvironment environment;
    bindScope(unit, unit.syntaxTree(), {}, environment, diagnostics, index);
}

void ProjectNameLookup::bindScope(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* scopeNode,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& inheritedEnvironment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    LookupEnvironment environment = inheritedEnvironment;
    std::unordered_set<std::string> localAliases;

    for (const ast::AstNode* child : scopeNode->children())
    {
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::optional<std::string> resolved = resolveNamespaceTarget(
                unit.projectOrdinal(),
                namespaceSegments,
                child->text());
            if (!resolved.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::UnknownNamespace,
                    diagnostics::DiagnosticSeverity::Error,
                    child->range(),
                    "unknown namespace '" + child->text() + "'");
                continue;
            }
            if (std::find(
                    environment.importedNamespaces.begin(),
                    environment.importedNamespaces.end(),
                    *resolved) == environment.importedNamespaces.end())
            {
                environment.importedNamespaces.push_back(*resolved);
            }
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            if (!localAliases.insert(child->text()).second)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateUsingAlias,
                    diagnostics::DiagnosticSeverity::Error,
                    child->range(),
                    "duplicate using alias '" + child->text() + "' in the same declaration scope");
                continue;
            }
            const std::optional<std::string> namespaceTarget = child->auxiliaryText().empty()
                ? std::nullopt
                : resolveNamespaceTarget(
                    unit.projectOrdinal(),
                    namespaceSegments,
                    child->auxiliaryText());
            // C3: a target with modifiers (`void*`, `float[4]`) or a builtin base (`uint`) is a
            // TYPE by construction -- there is nothing to look up and nothing that could be
            // ambiguous, so the name-resolution checks below apply only to a bare name target.
            const bool targetIsWrittenAsType = child->auxiliaryText().empty();
            // C3 "alias chaining": `using LPDWORD = PDWORD;` names another ALIAS, not a type, so
            // the type-visibility check below would reject it. An alias already in scope is a
            // legitimate target -- expansion follows the chain at bind time -- and one that is
            // NOT in scope still falls through to the real diagnostic.
            const bool targetIsAliasInScope = !child->auxiliaryText().empty() &&
                (environment.aliases.contains(child->auxiliaryText()) ||
                 environment.typeAliases.contains(child->auxiliaryText()));
            if (!namespaceTarget.has_value() && !targetIsWrittenAsType && !targetIsAliasInScope)
            {
                const std::vector<std::size_t> typeCandidates = resolveTypeCandidates(
                    unit.projectOrdinal(),
                    namespaceSegments,
                    environment,
                    child->auxiliaryText());
                if (typeCandidates.empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidUsingTarget,
                        diagnostics::DiagnosticSeverity::Error,
                        child->range(),
                        "using alias target '" + child->auxiliaryText() +
                            "' does not name a visible namespace or type");
                    continue;
                }
                if (typeCandidates.size() > 1)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::AmbiguousTypeName,
                        diagnostics::DiagnosticSeverity::Error,
                        child->range(),
                        "using alias target '" + child->auxiliaryText() + "' is ambiguous; candidates: " +
                            candidateList(typeCandidates, namedTypes_));
                    continue;
                }
            }
            // C3 "FFI aliases": a NAMESPACE alias keeps the textual path (`aliases`); a TYPE
            // alias keeps its parsed type syntax (`typeAliases`), which is what lets the target
            // be `uint`, `void*` or `float[4]` and not only a name. resolveNamespaceTarget above
            // is what separates the two, so `using IO = NativeSharp.Core.IO;` is unaffected.
            if (namespaceTarget.has_value() ||
                (child->typeSyntax() == nullptr) ||
                environment.aliases.contains(child->auxiliaryText()))
            {
                environment.aliases[child->text()] = child->auxiliaryText();
            }
            else
            {
                environment.typeAliases[child->text()] = child->typeSyntax();
            }
        }
    }

    for (const ast::AstNode* child : scopeNode->children())
    {
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective ||
            child->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            continue;
        }
        if (child->kind() == ast::AstNodeKind::NamespaceDeclaration)
        {
            std::vector<std::string> nestedNamespace = namespaceSegments;
            const std::vector<std::string> segments = splitQualifiedName(child->text());
            nestedNamespace.insert(nestedNamespace.end(), segments.begin(), segments.end());
            bindScope(unit, child, nestedNamespace, environment, diagnostics, index);
            continue;
        }
        bindNode(unit, child, namespaceSegments, environment, diagnostics, index);
    }
}

void ProjectNameLookup::bindNode(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (node == nullptr)
    {
        return;
    }

    if (!node->genericParameters().empty())
    {
        // NSC-LIMIT-007, segunda vuelta. UNA PLANTILLA TAMBIEN RESUELVE SUS CONSTRAINTS, aunque
        // nadie la instancie.
        //
        // Este `return` existe porque el cuerpo de una plantilla habla de `T`, que no es un tipo y
        // no se puede resolver; la especializacion clonada es la que se visita. Pero los tipos
        // ESCRITOS EN UN `where` no son eso: `where T : IComponente` nombra a `IComponente`, que
        // existe con independencia de que `T` llegue a cerrarse. Sin esta vuelta, una plantilla
        // nunca instanciada se llevaba sus errores de DECLARACION --`where T : MiEnum`-- sin que
        // los viera nadie, y la promesa del ID quedaba a medias: el error existia solo si alguien
        // usaba la plantilla.
        //
        // Solo los tipos de constraint: el argumento sustituido no existe en una plantilla, y el
        // parser ya rechaza que un constraint nombre otro parametro generico, de modo que aqui no
        // puede llegar por construccion un nombre irresoluble.
        for (const ast::GenericConstraintClause& clause : node->genericConstraints())
        {
            for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            {
                bindTypeSyntax(unit, constraint, namespaceSegments, environment, diagnostics, index);
            }
            if (clause.equalityConstraint != nullptr &&
                std::find(
                    node->genericParameters().begin(), node->genericParameters().end(),
                    std::string(clause.equalityConstraint->baseName())) ==
                    node->genericParameters().end())
            {
                std::string constantType;
                std::string constantValue;
                if (!language::parseConstGenericArgumentMarker(
                        clause.equalityConstraint->baseName(), constantType, constantValue))
                {
                    bindTypeSyntax(
                        unit, clause.equalityConstraint, namespaceSegments, environment,
                        diagnostics, index);
                }
            }
        }
        return;
    }

    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        std::vector<std::string> nestedNamespace = namespaceSegments;
        const std::vector<std::string> segments = splitQualifiedName(node->text());
        nestedNamespace.insert(nestedNamespace.end(), segments.begin(), segments.end());
        bindScope(unit, node, nestedNamespace, environment, diagnostics, index);
        return;
    }

    // NSC-LIMIT-009. Entrar en un tipo AMPLIA el alcance: dentro de `Outer`, `Item` se busca
    // primero como `Outer.Item`, y un anidado `private` de `Outer` se ve. El entorno se copia solo
    // cuando de verdad se entra en un tipo; en cualquier otro nodo se sigue usando el heredado,
    // que es la referencia que llega, sin copias por nodo del arbol.
    LookupEnvironment scopedEnvironment;
    const LookupEnvironment* activeEnvironment = &environment;
    if (isNamedTypeDeclaration(node->kind()))
    {
        const auto ordinal = typeOrdinalBySyntax_.find(node);
        if (ordinal != typeOrdinalBySyntax_.end())
        {
            scopedEnvironment = environment;
            scopedEnvironment.enclosingTypeOrdinals.push_back(ordinal->second);
            activeEnvironment = &scopedEnvironment;
        }
    }
    const LookupEnvironment& scope = *activeEnvironment;

    if (node->kind() == ast::AstNodeKind::MemberExpression)
    {
        bindExpressionTypeReceiver(unit, node->first(), namespaceSegments, scope, index);
    }
    else if (node->kind() == ast::AstNodeKind::NameOfExpression)
    {
        const ast::AstNode* target = node->first();
        while (target != nullptr &&
               (target->kind() == ast::AstNodeKind::CallExpression ||
                target->kind() == ast::AstNodeKind::IndexExpression))
            target = target->first();
        if (node->typeSyntax() != nullptr && node->typeSyntax()->typeArguments().empty())
        {
            bindTypeSyntax(
                unit, node->typeSyntax(), namespaceSegments, scope, diagnostics, index);
        }
        else
        {
            bindExpressionTypeReceiver(unit, target, namespaceSegments, scope, index);
            if (node->typeSyntax() != nullptr)
            {
                for (const ast::TypeSyntax* argument : node->typeSyntax()->typeArguments())
                    bindTypeSyntax(
                        unit, argument, namespaceSegments, scope, diagnostics, index);
            }
        }
    }

    if (node->typeSyntax() != nullptr && node->kind() != ast::AstNodeKind::NameOfExpression)
    {
        bindTypeSyntax(unit, node->typeSyntax(), namespaceSegments, scope, diagnostics, index);
        for (const ast::TypeModifierSyntax& modifier : node->typeSyntax()->modifiers())
        {
            if (modifier.arrayCount != nullptr)
            {
                bindNode(unit, modifier.arrayCount, namespaceSegments, scope, diagnostics, index);
            }
        }
    }
    if (node->throwsTypeSyntax() != nullptr)
    {
        bindTypeSyntax(
            unit, node->throwsTypeSyntax(), namespaceSegments, scope, diagnostics, index);
    }
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
    {
        bindTypeSyntax(unit, syntax, namespaceSegments, scope, diagnostics, index);
    }
    // NSC-LIMIT-007. Los tipos escritos en un `where` son referencias de tipo como cualquier otra
    // y se resuelven por el mismo camino: con el mismo alcance, los mismos `using` y los mismos
    // alias. Sin esta vuelta quedaban sin resolver y la validacion de constraints solo veia
    // `unresolved:IComponente` -- que la habria obligado a llevar su propia resolucion de
    // nombres, o sea una segunda copia de la unica cosa que este fichero hace.
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
        {
            bindTypeSyntax(unit, constraint, namespaceSegments, scope, diagnostics, index);
        }
        if (clause.equalityConstraint != nullptr)
        {
            std::string constantType;
            std::string constantValue;
            if (!language::parseConstGenericArgumentMarker(
                    clause.equalityConstraint->baseName(), constantType, constantValue))
            {
                bindTypeSyntax(
                    unit, clause.equalityConstraint, namespaceSegments, scope, diagnostics, index);
            }
        }
        std::string substitutedConstantType;
        std::string substitutedConstantValue;
        if (clause.substitutedArgument != nullptr &&
            !language::parseConstGenericArgumentMarker(
                clause.substitutedArgument->baseName(),
                substitutedConstantType,
                substitutedConstantValue))
        {
            bindTypeSyntax(
                unit, clause.substitutedArgument, namespaceSegments, scope, diagnostics, index);
        }
    }

    bindNode(unit, node->first(), namespaceSegments, scope, diagnostics, index);
    bindNode(unit, node->second(), namespaceSegments, scope, diagnostics, index);
    bindNode(unit, node->third(), namespaceSegments, scope, diagnostics, index);
    for (const ast::AstNode* child : node->children())
    {
        bindNode(unit, child, namespaceSegments, scope, diagnostics, index);
    }
}

void ProjectNameLookup::bindExpressionTypeReceiver(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    UnitBindingIndex& index)
{
    if (expression == nullptr || index.expressionTypeBindingBySyntax.contains(expression))
    {
        return;
    }

    const std::optional<std::string> writtenName = expressionQualifiedName(expression);
    if (!writtenName.has_value())
    {
        return;
    }
    std::vector<std::size_t> candidates = resolveExpressionTypeCandidates(
        unit.projectOrdinal(), namespaceSegments, environment, *writtenName);
    if (candidates.empty())
    {
        return;
    }

    const std::size_t ordinal = index.expressionTypeBindings.size();
    index.expressionTypeBindings.push_back(ExpressionTypeNameBinding{
        expression, *writtenName, std::move(candidates), environment.enclosingTypeOrdinals});
    index.expressionTypeBindingBySyntax.emplace(expression, ordinal);
}

void ProjectNameLookup::bindTypeSyntax(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (syntax == nullptr || index.bindingBySyntax.contains(syntax))
    {
        return;
    }

    std::string constType;
    std::string constValue;
    if (language::parseConstGenericArgumentMarker(syntax->baseName(), constType, constValue))
        return;

    // `var`'s marker TypeSyntax (see Parser::parseVariableDeclaration) has no real named type to
    // resolve -- its real type is inferred entirely from the initializer expression, inside
    // ProjectMethodBodyModel::analyzeVariable, which never looks this binding up. Binding it here
    // like an ordinary identifier type would incorrectly report "unknown type name 'var'" (there
    // is, correctly, no class/interface/struct actually named "var").
    if (syntax->baseTokenKind() == lexer::TokenKind::KwVar)
    {
        return;
    }

    // C3 "FFI aliases": expand a type alias INTO this node before anything else looks at it.
    // After this, the AST says what the alias said -- `DWORD` has literally become `uint`,
    // `HANDLE` has become `void*` -- so canonical type keys, object layout, the C ABI rules, HIR
    // and the backend never learn that aliases exist. That is exactly what a typedef means: an
    // alias is not a distinct type, it is another spelling.
    //
    // Chaining falls out of the loop (`using LPDWORD = PDWORD;` where `PDWORD = uint*`), and the
    // loop is bounded so a cycle (`using A = B; using B = A;`) is reported instead of hanging.
    // Modifiers written at the USE site stay outermost: `PDWORD*` is `uint**`, never `uint*`.
    // The name the programmer actually WROTE, captured before any expansion: diagnostics and the
    // binding record must keep saying `PointAlias`, not the type it turned out to mean.
    const std::string writtenName = syntax->baseName();
    {
        ast::TypeSyntax* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
        constexpr int kMaxAliasDepth = 64;
        int depth = 0;
        while (!mutableSyntax->isFunction() &&
               !lexer::isBuiltinTypeKeyword(mutableSyntax->baseTokenKind()) &&
               mutableSyntax->typeArguments().empty())
        {
            const auto alias = environment.typeAliases.find(mutableSyntax->baseName());
            if (alias == environment.typeAliases.end() || alias->second == nullptr) break;
            if (++depth > kMaxAliasDepth)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidUsingTarget,
                    diagnostics::DiagnosticSeverity::Error,
                    mutableSyntax->range(),
                    "using alias '" + mutableSyntax->baseName() + "' expands cyclically");
                break;
            }
            const ast::TypeSyntax* target = alias->second;
            if (target == mutableSyntax) break;
            mutableSyntax->setBaseTokenKind(target->baseTokenKind());
            mutableSyntax->setBaseName(target->baseName());
            if (target->isFunction())
            {
                mutableSyntax->setFunction();
                mutableSyntax->setFunctionCarrier(target->functionCarrier());
                mutableSyntax->setFunctionCallingConvention(
                    target->functionCallingConvention());
                mutableSyntax->setFunctionReturnType(target->functionReturnType());
                mutableSyntax->setFunctionErrorType(target->functionErrorType());
                mutableSyntax->setFunctionVariadic(target->functionIsVariadic());
                mutableSyntax->setFunctionUnsafe(target->functionIsUnsafe());
                mutableSyntax->setFunctionNoHeap(target->functionIsNoHeap());
                mutableSyntax->setFunctionAsync(target->functionIsAsync());
                mutableSyntax->setFunctionNullable(target->functionIsNullable());
                mutableSyntax->setFunctionLifetimeName(target->functionLifetimeName());
                mutableSyntax->clearTypeArguments();
                for (ast::TypeSyntax* parameter : target->typeArguments())
                    mutableSyntax->addTypeArgument(parameter);
            }
            mutableSyntax->setDynamicTrait(
                mutableSyntax->isDynamicTrait() || target->isDynamicTrait());
            mutableSyntax->setOpaqueResult(
                mutableSyntax->isOpaqueResult() || target->isOpaqueResult());
            if (mutableSyntax->opaqueResultTraitName().empty())
                mutableSyntax->setOpaqueResultTraitName(target->opaqueResultTraitName());
            mutableSyntax->prependModifiers(target->modifiers());
        }
    }

    for (const ast::TypeSyntax* argument : syntax->typeArguments())
    {
        bindTypeSyntax(unit, argument, namespaceSegments, environment, diagnostics, index);
    }
    if (syntax->isFunction())
    {
        bindTypeSyntax(
            unit, syntax->functionReturnType(), namespaceSegments,
            environment, diagnostics, index);
        bindTypeSyntax(
            unit, syntax->functionErrorType(), namespaceSegments,
            environment, diagnostics, index);
    }

    // Stage 52 fix: a generic type argument substituted into a specialization cloned from a
    // referenced project's template (e.g. List<MyOwnClass> where List<T> lives in a shared
    // library) carries an explicit origin-project override -- see TypeSyntax::originProjectOrdinal
    // for the full explanation. Fall back to the enclosing unit's own project ordinal for every
    // ordinary (non-substituted) type syntax node, exactly as before.
    const std::size_t effectiveProjectOrdinal = syntax->originProjectOrdinal().value_or(unit.projectOrdinal());
    // Companion override for the lexical namespace -- see TypeSyntax::originNamespace. Without
    // this, "Payload" would still be searched under the TEMPLATE's own namespace (e.g.
    // "NativeSharp.Core.Collections.Generic"), never finding a type declared only in the
    // requesting project's OWN namespace (e.g. "Stage52LibTest").
    const std::vector<std::string> effectiveNamespaceSegments = syntax->originNamespace().has_value()
        ? splitQualifiedName(syntax->originNamespace().value())
        // Older/current clones from a consumer in the global namespace carry the project
        // override but no namespace string. That still means "global namespace of the origin
        // project", never "template namespace".
        : (syntax->originProjectOrdinal().has_value() ? std::vector<std::string>{} : namespaceSegments);

    TypeNameBinding binding;
    binding.syntax = syntax;
    binding.projectOrdinal = effectiveProjectOrdinal;
    binding.fileId = unit.fileId();
    binding.namespaceName = joinSegments(effectiveNamespaceSegments);
    binding.writtenName = writtenName;
    binding.enclosingTypeOrdinals = environment.enclosingTypeOrdinals;

    const bool portableSimd = syntax->baseName() == "simd" || syntax->baseName() == "mask";
    // NS304-T05-F09: `Future<T>` is a structural opaque surface (future:<T>), not a nominal type.
    const bool futureSurface =
        syntax->baseName() == "Future" && syntax->typeArguments().size() == 1;
    // NS304-T06-F02: `Iterator<T>` is a structural opaque surface (iterator:<T>).
    const bool iteratorSurface =
        syntax->baseName() == "Iterator" && syntax->typeArguments().size() == 1;
    // NS304-T06-F07: `Iterator<T,E>` is also a structural opaque surface (iterator:<T>|<E>).
    const bool iteratorErrorSurface =
        syntax->baseName() == "Iterator" && syntax->typeArguments().size() == 2;
    // NS304-T07-F01: `AsyncIterator<T,E>` is a structural opaque surface (async-iterator:<T>|<E>).
    const bool asyncIteratorSurface =
        syntax->baseName() == "AsyncIterator" && syntax->typeArguments().size() == 2;
    if (syntax->isFunction() || syntax->isTuple() || portableSimd || futureSurface ||
        iteratorSurface || iteratorErrorSurface || asyncIteratorSurface ||
        lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
    {
        binding.kind = TypeNameBindingKind::Builtin;
        if (portableSimd)
        {
            const std::size_t expected = syntax->baseName() == "simd" ? 2U : 2U;
            if (syntax->typeArguments().size() != expected)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax->range(),
                    "'" + syntax->baseName() + "' requires a lane type and a constant lane count");
            }
            else
            {
                const ast::TypeSyntax* lane = syntax->typeArguments()[0];
                const ast::TypeSyntax* count = syntax->typeArguments()[1];
                std::string markerType;
                std::string markerValue;
                std::size_t laneCount = 0;
                const bool marker = language::parseConstGenericArgumentMarker(
                    count->baseName(), markerType, markerValue);
                const auto parsed = std::from_chars(
                    markerValue.data(), markerValue.data() + markerValue.size(), laneCount);
                std::string simdError;
                const std::string laneKey = lexer::isBuiltinTypeKeyword(lane->baseTokenKind())
                    ? "builtin:" + lane->baseName() : "unresolved:" + lane->baseName();
                if (!marker || parsed.ec != std::errc{} ||
                    parsed.ptr != markerValue.data() + markerValue.size() ||
                    !language::portableSimdType(
                        laneKey, laneCount, syntax->baseName() == "mask", simdError).has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        syntax->range(),
                        simdError.empty() ? "portable SIMD lane count must be a constant integer" : simdError);
                }
            }
        }
        if (syntax->isDynamicTrait())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "'dyn' requires a trait or interface type, not a builtin, tuple, or function type");
        }
    }
    else
    {
        binding.candidateTypeOrdinals = resolveTypeCandidates(
            effectiveProjectOrdinal,
            effectiveNamespaceSegments,
            environment,
            syntax->baseName());
        // C8 "type root": `object` is the lowercase spelling of NativeSharp.Core.Object, the way
        // `string` is of the native string type. Deliberately NOT a lexer keyword and resolved
        // only as a LAST resort, after ordinary lookup has failed: a project that does not have
        // the root in view keeps `object` as an ordinary identifier, and one that declares its own
        // type called `object` still wins. Same shape as the alias expansion just above -- the
        // binding's writtenName stays "object", so diagnostics say what the programmer wrote.
        if (binding.candidateTypeOrdinals.empty() && syntax->baseName() == "object")
        {
            for (std::size_t ordinal = 0; ordinal < namedTypes_.size(); ++ordinal)
            {
                if (namedTypes_[ordinal].kind == NamedTypeDeclarationKind::Class &&
                    namedTypes_[ordinal].identity.fullyQualifiedName() == "NativeSharp.Core.Object")
                {
                    binding.candidateTypeOrdinals.push_back(ordinal);
                    break;
                }
            }
        }
        if (binding.candidateTypeOrdinals.empty())
        {
            binding.kind = TypeNameBindingKind::Unresolved;
            // Before saying the name is unknown, check whether it EXISTS in a referenced project
            // and is simply not public. Reporting "unknown type name" for a type that is right
            // there sends the reader hunting for a typo that does not exist.
            //
            // This is half of what made the "an extern C delegate does not cross a
            // ProjectReference" report so hard to read. The generated binding declared
            //
            //     extern C delegate long WNDPROC(nint, uint, ulong, long);
            //
            // with no accessibility modifier -- so, internal -- while the classes and structs
            // beside it were written `public` and crossed the boundary without trouble. Only
            // public types cross a module boundary, which is the same rule as C# and is right;
            // what was wrong was that the parser had no branch letting an `extern C delegate`
            // carry an accessibility modifier at all, so it could not be made public even if you
            // tried. That half is fixed in Parser.cpp. This half is the diagnostic, which said
            // "unknown type name" and sent the reader looking for a typo instead of at the
            // declaration that was sitting right there.
            // NSC-LIMIT-009. Antes que nada: el nombre puede estar nombrando un anidado que SI
            // existe y que este sitio no puede ver. Es la misma clase de error que el de abajo
            // --el tipo esta escrito, la accesibilidad es lo que falla-- y merece el mismo trato.
            const std::optional<std::size_t> inaccessibleNested = findInaccessibleNestedType(
                effectiveProjectOrdinal, effectiveNamespaceSegments, syntax->baseName());

            std::string internalMatch;
            std::optional<std::size_t> inaccessibleOrdinal;
            if (!inaccessibleNested.has_value() &&
                effectiveProjectOrdinal < modules_.size() && modules_[effectiveProjectOrdinal] != nullptr)
            {
                const std::string& wanted = syntax->baseName();
                const std::string suffix = "." + wanted;
                for (const std::size_t visibleOrdinal :
                     modules_[effectiveProjectOrdinal]->visibleProjectOrdinals())
                {
                    if (visibleOrdinal >= typesByModule_.size()) continue;
                    for (const auto& entry : typesByModule_[visibleOrdinal])
                    {
                        if (entry.second >= namedTypes_.size()) continue;
                        if (typeIsVisibleTo(effectiveProjectOrdinal, entry.second))
                        {
                            continue;
                        }
                        if (entry.first == wanted ||
                            (entry.first.size() > suffix.size() &&
                             entry.first.compare(
                                 entry.first.size() - suffix.size(), suffix.size(), suffix) == 0))
                        {
                            internalMatch = entry.first;
                            inaccessibleOrdinal = entry.second;
                            break;
                        }
                    }
                    if (!internalMatch.empty()) break;
                }
            }
            if (inaccessibleNested.has_value())
            {
                const NamedTypeDeclaration& nested = namedTypes_[*inaccessibleNested];
                diagnostics.report(
                    diagnostics::DiagnosticId::UnknownTypeName,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax->range(),
                    "nested type '" + nested.identity.fullyQualifiedName() + "' is " +
                        namedTypeAccessibilityName(nested.accessibility) +
                        " and is not accessible here; it is visible only inside '" +
                        (nested.containingTypeOrdinal.has_value()
                             ? namedTypes_[*nested.containingTypeOrdinal].identity.fullyQualifiedName()
                             : std::string("its container")) +
                        "'");
            }
            else if (!internalMatch.empty())
            {
                const bool packageMismatch = inaccessibleOrdinal.has_value() &&
                    namedTypes_[*inaccessibleOrdinal].accessibility ==
                        NamedTypeAccessibility::Package;
                diagnostics.report(
                    diagnostics::DiagnosticId::UnknownTypeName,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax->range(),
                    packageMismatch
                        ? "type '" + internalMatch + "' is package-visible and the consuming "
                          "project has a different Package identity (module '" +
                              std::string(unit.moduleName()) + "')"
                        : "type '" + internalMatch + "' is declared in a referenced project but is "
                          "module-visible and the consumer is not an explicit Friend; declare it "
                          "'public' or add a Friend contract (module '" +
                              std::string(unit.moduleName()) + "')");
            }
            else
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::UnknownTypeName,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax->range(),
                    "unknown type name '" + syntax->baseName() + "' in module '" +
                        std::string(unit.moduleName()) + "'");
            }
        }
        else if (binding.candidateTypeOrdinals.size() == 1)
        {
            binding.kind = TypeNameBindingKind::Resolved;
            binding.targetTypeOrdinal = binding.candidateTypeOrdinals.front();
            if (syntax->isDynamicTrait() &&
                namedTypes_[*binding.targetTypeOrdinal].kind != NamedTypeDeclarationKind::Interface)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    syntax->range(),
                    "'dyn " + syntax->baseName() +
                        "' is invalid because the target is not a trait or interface");
            }
        }
        else
        {
            binding.kind = TypeNameBindingKind::Ambiguous;
            diagnostics.report(
                diagnostics::DiagnosticId::AmbiguousTypeName,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "ambiguous type name '" + syntax->baseName() + "'; candidates: " +
                    candidateList(binding.candidateTypeOrdinals, namedTypes_));
        }
    }

    const std::size_t ordinal = index.bindings.size();
    index.bindings.push_back(std::move(binding));
    index.bindingBySyntax.emplace(syntax, ordinal);
}

// NSC-LIMIT-007/009. La escritura COMPLETA de cada tipo contenedor del sitio, de fuera adentro.
// Es la mitad textual de `enclosingTypeOrdinals`, y existe para poder darsela a
// `qualifiedNameLadder` -- que no conoce ordinales a proposito: la fase generica, que usa la misma
// escala, no tiene modelo de nombres del que sacarlos.
std::vector<std::string> ProjectNameLookup::enclosingTypeSpellings(
    const LookupEnvironment& environment) const
{
    std::vector<std::string> spellings;
    spellings.reserve(environment.enclosingTypeOrdinals.size());
    for (const std::size_t enclosing : environment.enclosingTypeOrdinals)
    {
        if (enclosing >= namedTypes_.size()) continue;
        const compilation::TypeIdentity& identity = namedTypes_[enclosing].identity;
        std::string candidate;
        for (const std::string& segment : identity.namespaceSegments())
        {
            if (!candidate.empty()) candidate.push_back('.');
            candidate += segment;
        }
        for (const compilation::TypeNameSegment& segment : identity.containingTypes())
        {
            if (!candidate.empty()) candidate.push_back('.');
            candidate += segment.name;
        }
        if (!candidate.empty()) candidate.push_back('.');
        candidate += identity.name();
        spellings.push_back(std::move(candidate));
    }
    return spellings;
}

std::vector<std::size_t> ProjectNameLookup::resolveTypeCandidates(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    const std::string& writtenName) const
{
    const std::size_t moduleSeparator = writtenName.find("::");
    if (moduleSeparator != std::string::npos)
    {
        const std::string_view requestedModule(writtenName.data(), moduleSeparator);
        const std::string requestedType = writtenName.substr(moduleSeparator + 2U);
        if (requestedModule.empty() || requestedType.empty()) return {};

        std::vector<std::size_t> found;
        for (std::size_t ordinal = 0; ordinal < modules_.size(); ++ordinal)
        {
            if (modules_[ordinal] == nullptr || modules_[ordinal]->moduleName() != requestedModule)
                continue;
            const std::optional<std::size_t> target = findTypeInModule(ordinal, requestedType);
            if (!target.has_value()) continue;
            if (ordinal == projectOrdinal || typeIsVisibleTo(projectOrdinal, *target))
            {
                const std::array<std::size_t, 1> single{*target};
                appendUnique(found, single);
            }
        }
        return found;
    }

    const std::vector<std::string> nameSegments = splitQualifiedName(writtenName);
    if (nameSegments.empty() || projectOrdinal >= typesByModule_.size())
    {
        return {};
    }

    // LA ESCALA VIENE DE FUERA, de `qualifiedNameLadder`. Este bucle NO decide el orden: lo
    // recorre. Lo que decide, y lo que sigue viviendo aqui, es COMO se sondea cada escalon --que
    // tabla, y con que regla de visibilidad--, porque eso es propio del modelo de nombres y no
    // tiene sentido en la fase generica, que sondea plantillas.
    //
    // Antes el orden estaba escrito aqui y COPIADO, mas simple y por tanto distinto, dentro de
    // `ProjectGenericModel`. Ver QualifiedNameLadder.h.
    const std::vector<std::string> enclosing = enclosingTypeSpellings(environment);
    const bool simpleName = nameSegments.size() == 1 &&
        environment.aliases.find(nameSegments.front()) == environment.aliases.end();

    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = enclosing;
    site.aliases = &environment.aliases;
    site.importedNamespaces = environment.importedNamespaces;

    for (const QualifiedNameRung& rung : qualifiedNameLadder(site, writtenName))
    {
        std::vector<std::size_t> found;
        for (const std::string& candidate : rung.spellings)
        {
            switch (rung.tier)
            {
                case QualifiedNameTier::EnclosingType:
                {
                    // Este escalon significa literalmente «miembro de un tipo contenedor».
                    // Su escritura fuente usa puntos (Ns.Outer.Item), pero esa misma cadena
                    // puede ser tambien la clave canonica de un tipo Item declarado en el
                    // namespace Ns.Outer. findTypeInModule consulta primero esa clave canonica
                    // y, en la colision, devolvia el tipo de namespace antes de mirar el
                    // anidado. Aqui no hay tal ambiguedad: por definicion del escalon solo
                    // participa el indice de nombres fuente de tipos anidados.
                    const auto own =
                        nestedSourceNamesByModule_[projectOrdinal].find(candidate);
                    if (own != nestedSourceNamesByModule_[projectOrdinal].end() &&
                        nestedTypeIsVisible(own->second, environment))
                    {
                        const std::array<std::size_t, 1> single{own->second};
                        appendUnique(found, single);
                    }
                    break;
                }
                case QualifiedNameTier::LexicalNamespace:
                {
                    // UN NOMBRE SIMPLE SONDEA SOLO LA CLAVE CANONICA; uno con puntos pasa tambien
                    // por el indice de nombres fuente, que es lo que hace que `ns.Outer.Item`
                    // pueda ser un anidado. La diferencia estaba ya en las dos ramas de antes y se
                    // conserva tal cual: cambiarla movería la resolucion de nombres de todo el
                    // arbol para arreglar un defecto que no esta aqui.
                    if (simpleName)
                    {
                        const std::optional<std::size_t> current =
                            findTypeInModule(projectOrdinal, candidate);
                        if (current.has_value())
                        {
                            const std::array<std::size_t, 1> single{*current};
                            appendUnique(found, single);
                        }
                        break;
                    }
                    const std::optional<std::size_t> own =
                        findTypeInModule(projectOrdinal, candidate);
                    if (own.has_value() && nestedTypeIsVisible(*own, environment))
                    {
                        const std::array<std::size_t, 1> single{*own};
                        appendUnique(found, single);
                    }
                    break;
                }
                case QualifiedNameTier::ImportedNamespace:
                {
                    if (simpleName)
                    {
                        appendUnique(
                            found, findExactTypeCandidates(projectOrdinal, candidate, false));
                        break;
                    }
                    const std::optional<std::size_t> own =
                        findTypeInModule(projectOrdinal, candidate);
                    if (own.has_value() && nestedTypeIsVisible(*own, environment))
                    {
                        const std::array<std::size_t, 1> single{*own};
                        appendUnique(found, single);
                    }
                    for (const std::size_t visibleOrdinal :
                         modules_[projectOrdinal]->visibleProjectOrdinals())
                    {
                        const std::optional<std::size_t> other =
                            findTypeInModule(visibleOrdinal, candidate);
                        if (other.has_value() && typeIsVisibleTo(projectOrdinal, *other))
                        {
                            const std::array<std::size_t, 1> single{*other};
                            appendUnique(found, single);
                        }
                    }
                    break;
                }
                case QualifiedNameTier::Global:
                {
                    const std::optional<std::size_t> global =
                        findTypeInModule(projectOrdinal, candidate);
                    if (global.has_value())
                    {
                        const std::array<std::size_t, 1> single{*global};
                        appendUnique(found, single);
                    }
                    break;
                }
                case QualifiedNameTier::ReferencedModule:
                {
                    for (const std::size_t visibleOrdinal :
                         modules_[projectOrdinal]->visibleProjectOrdinals())
                    {
                        if (simpleName)
                        {
                            const std::optional<std::size_t> candidateType =
                                findTypeInModule(visibleOrdinal, candidate);
                            if (candidateType.has_value() &&
                                typeIsVisibleTo(projectOrdinal, *candidateType))
                            {
                                const std::array<std::size_t, 1> single{*candidateType};
                                appendUnique(found, single);
                            }
                            continue;
                        }
                        const std::optional<std::size_t> other =
                            findTypeInModule(visibleOrdinal, candidate);
                        if (other.has_value() && typeIsVisibleTo(projectOrdinal, *other))
                        {
                            const std::array<std::size_t, 1> single{*other};
                            appendUnique(found, single);
                        }
                    }
                    break;
                }
            }
        }
        if (!found.empty())
        {
            return found;
        }
    }
    return {};
}

std::vector<std::size_t> ProjectNameLookup::resolveExpressionTypeCandidates(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    const std::string& writtenName) const
{
    if (writtenName.find('.') == std::string::npos)
    {
        const auto alias = environment.typeAliases.find(writtenName);
        if (alias != environment.typeAliases.end() && alias->second != nullptr)
        {
            const ast::TypeSyntax* target = alias->second;
            std::unordered_set<std::string> visitedAliases;
            visitedAliases.insert(writtenName);
            while (target != nullptr &&
                   !lexer::isBuiltinTypeKeyword(target->baseTokenKind()) &&
                   target->typeArguments().empty() && target->modifiers().empty())
            {
                const std::string& targetName = target->baseName();
                if (!visitedAliases.insert(targetName).second)
                {
                    return {};
                }

                const auto chainedAlias = environment.typeAliases.find(targetName);
                if (chainedAlias == environment.typeAliases.end())
                {
                    return resolveTypeCandidates(
                        projectOrdinal, namespaceSegments, environment, targetName);
                }
                target = chainedAlias->second;
            }
            return {};
        }
    }
    return resolveTypeCandidates(projectOrdinal, namespaceSegments, environment, writtenName);
}

// NSC-LIMIT-009. Que un tipo cruce el limite de modulo. Para uno de namespace es la pregunta de
// siempre --`public`-- y para un ANIDADO son todas: `public class Outer { public struct Item }`
// exporta `Item`, y `class Outer { public struct Item }` no exporta nada, porque el contenedor no
// sale. Sin esta cadena, un `public` interior habria colado al anidado por encima de su
// contenedor internal.
bool ProjectNameLookup::typeIsVisibleTo(
    std::size_t accessingProjectOrdinal,
    std::size_t typeOrdinal) const
{
    // NSC-LIMIT-009, segunda vuelta. `protected` TAMBIEN CRUZA -- o mejor dicho, tiene derecho a
    // que se le pregunte. Un anidado protegido de un contenedor public es alcanzable desde una
    // clase derivada, y esa clase puede estar en OTRO proyecto: es el caso normal de una
    // biblioteca que declara `protected class Reservada` para que sus derivadas la usen.
    //
    // La version anterior lo descartaba aqui, con lo cual el nombre no llegaba a producir un
    // binding resuelto y `checkProtectedNestedTypeUses` --que es quien sabe decidirlo-- no
    // recibia nunca la oportunidad. `private` e `internal` si cortan: no hay ninguna fase
    // posterior que pueda convertirlos en alcanzables desde fuera del modulo.
    //
    // El ESLABON SUPERIOR se exige public aparte, en el bucle: un contenedor `protected` de
    // namespace no existe --solo un anidado puede serlo-- asi que un tipo raiz que no sea public
    // no sale del modulo por ningun camino.
    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value() && *current < namedTypes_.size())
    {
        const NamedTypeDeclaration& type = namedTypes_[*current];
        const bool nested = type.containingTypeOrdinal.has_value();
        bool crosses = type.accessibility == NamedTypeAccessibility::Public ||
            (nested && type.accessibility == NamedTypeAccessibility::Protected);
        if (!nested && type.projectOrdinal < packageByProject_.size() &&
            accessingProjectOrdinal < packageByProject_.size())
        {
            const auto fold = [](std::string value)
            {
                std::ranges::transform(value, value.begin(), [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
                return value;
            };
            if (type.accessibility == NamedTypeAccessibility::Package &&
                fold(packageByProject_[type.projectOrdinal]) ==
                    fold(packageByProject_[accessingProjectOrdinal]))
            {
                crosses = true;
            }
            if (type.accessibility == NamedTypeAccessibility::Internal &&
                type.projectOrdinal < friendsByProject_.size() &&
                accessingProjectOrdinal < modules_.size())
            {
                const std::string accessing = fold(modules_[accessingProjectOrdinal]->moduleName());
                crosses = std::ranges::any_of(
                    friendsByProject_[type.projectOrdinal],
                    [&](const std::string& name) { return fold(name) == accessing; });
            }
        }
        if (!crosses)
        {
            return false;
        }
        current = type.containingTypeOrdinal;
    }
    return true;
}

std::optional<std::size_t> ProjectNameLookup::findTypeInModule(
    std::size_t moduleOrdinal,
    const std::string& dottedName) const
{
    if (moduleOrdinal >= typesByModule_.size())
    {
        return std::nullopt;
    }
    const auto canonical = typesByModule_[moduleOrdinal].find(dottedName);
    if (canonical != typesByModule_[moduleOrdinal].end())
    {
        return canonical->second;
    }
    const auto nested = nestedSourceNamesByModule_[moduleOrdinal].find(dottedName);
    if (nested != nestedSourceNamesByModule_[moduleOrdinal].end())
    {
        return nested->second;
    }
    if (moduleOrdinal < reexportedTypesByModule_.size())
    {
        const auto facade = reexportedTypesByModule_[moduleOrdinal].find(dottedName);
        if (facade != reexportedTypesByModule_[moduleOrdinal].end()) return facade->second;
    }
    return std::nullopt;
}

bool ProjectNameLookup::nestedTypeIsVisible(
    std::size_t typeOrdinal,
    const LookupEnvironment& environment) const
{
    // Se recorre la cadena de contenedores hacia fuera. Basta con que UNO de ellos declare el
    // eslabon como `private` y no este entre los contenedores lexicos del sitio de uso para que
    // el nombre no se vea: `Outer.Privada.Item` no es alcanzable desde fuera de `Outer` aunque
    // `Item` sea public dentro de `Privada`.
    std::optional<std::size_t> current = typeOrdinal;
    while (current.has_value() && *current < namedTypes_.size())
    {
        const NamedTypeDeclaration& type = namedTypes_[*current];
        const std::optional<std::size_t> container = type.containingTypeOrdinal;
        if (!container.has_value())
        {
            return true;
        }
        if (type.accessibility == NamedTypeAccessibility::Private)
        {
            const bool insideContainer = std::find(
                environment.enclosingTypeOrdinals.begin(),
                environment.enclosingTypeOrdinals.end(),
                *container) != environment.enclosingTypeOrdinals.end();
            if (!insideContainer)
            {
                return false;
            }
        }
        // `Protected` NO se decide aqui, y no por comodidad: la pregunta es «el tipo que escribe
        // esto deriva del contenedor?», y el modelo de herencia se construye DESPUES de resolver
        // nombres. Responder aqui obligaria a una de dos mentiras: tratarlo como private
        // --rechazando el uso legitimo desde una derivada-- o como internal, que deja pasar el
        // modulo entero. Se acepta, se guarda la cadena del sitio en el binding, y lo comprueba
        // ProjectInheritanceModel, que es la primera fase que tiene la respuesta.
        current = container;
    }
    return true;
}

std::optional<std::size_t> ProjectNameLookup::findInaccessibleNestedType(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const std::string& writtenName) const
{
    if (projectOrdinal >= nestedSourceNamesByModule_.size())
    {
        return std::nullopt;
    }
    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), depth - 1),
            writtenName);
        const auto iterator = nestedSourceNamesByModule_[projectOrdinal].find(candidate);
        if (iterator != nestedSourceNamesByModule_[projectOrdinal].end())
        {
            return iterator->second;
        }
    }
    return std::nullopt;
}

std::vector<std::size_t> ProjectNameLookup::findExactTypeCandidates(
    std::size_t projectOrdinal,
    const std::string& fullyQualifiedName,
    bool currentModuleFirst) const
{
    if (projectOrdinal >= typesByModule_.size())
    {
        return {};
    }

    const auto current = typesByModule_[projectOrdinal].find(fullyQualifiedName);
    if (currentModuleFirst && current != typesByModule_[projectOrdinal].end())
    {
        return {current->second};
    }

    std::vector<std::size_t> candidates;
    if (current != typesByModule_[projectOrdinal].end())
    {
        candidates.push_back(current->second);
    }
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        const auto iterator = typesByModule_[visibleOrdinal].find(fullyQualifiedName);
        if (iterator != typesByModule_[visibleOrdinal].end() &&
            typeIsVisibleTo(projectOrdinal, iterator->second))
        {
            candidates.push_back(iterator->second);
        }
    }
    return candidates;
}

bool ProjectNameLookup::namespaceExistsVisible(
    std::size_t projectOrdinal,
    const std::string& namespaceName) const
{
    const std::vector<std::string> segments = splitQualifiedName(namespaceName);
    if (projectOrdinal >= modules_.size())
    {
        return false;
    }

    if (modules_[projectOrdinal]->globalNamespace().findNamespace(segments) != nullptr)
    {
        return true;
    }
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        if (modules_[visibleOrdinal]->globalNamespace().findNamespace(segments) != nullptr)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::string> ProjectNameLookup::resolveNamespaceTarget(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const std::string& target) const
{
    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        const std::size_t prefixCount = depth - 1;
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), prefixCount),
            target);
        if (namespaceExistsVisible(projectOrdinal, candidate))
        {
            return candidate;
        }
    }
    return std::nullopt;
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
