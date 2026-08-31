#include "nativesharp/Frontend/ProjectNameLookup.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
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
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_ || !modules_.empty() || !pendingTypes_.empty() || !namedTypes_.empty() || !bindings_.empty())
    {
        return false;
    }

    const std::span<const compilation::Project> projects = graph.projects();
    modules_.reserve(projects.size());
    typesByModule_.resize(projects.size());
    for (std::size_t projectOrdinal = 0; projectOrdinal < projects.size(); ++projectOrdinal)
    {
        modules_.push_back(std::make_unique<ModuleDeclarationSpace>(
            projectOrdinal,
            projects[projectOrdinal].moduleName()));
    }

    buildVisibility(graph);

    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit != nullptr && unit->syntaxTree() != nullptr)
        {
            predeclareUnit(*unit, sourcePathForUnit(graph, *unit));
        }
    }
    finalizePredeclarations(diagnostics);

    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit != nullptr && unit->syntaxTree() != nullptr)
        {
            bindUnit(*unit, diagnostics);
        }
    }

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

void ProjectNameLookup::predeclareUnit(
    const ProjectSyntaxUnit& unit,
    const std::string& sourcePath)
{
    predeclareNode(unit, unit.syntaxTree(), {}, sourcePath);
}

void ProjectNameLookup::predeclareNode(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::string& sourcePath)
{
    if (node == nullptr)
    {
        return;
    }

    if (!node->genericParameters().empty())
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
        modules_[unit.projectOrdinal()]->globalNamespace().ensureNamespace(nestedNamespace);
        for (const ast::AstNode* child : node->children())
        {
            predeclareNode(unit, child, nestedNamespace, sourcePath);
        }
        return;
    }

    if (isNamedTypeDeclaration(node->kind()))
    {
        compilation::TypeIdentity identity(
            std::string(unit.moduleName()),
            namespaceSegments,
            {},
            node->text(),
            0);
        const bool hasExplicitAccessibility =
            node->tokenKind() == lexer::TokenKind::KwPublic ||
            node->tokenKind() == lexer::TokenKind::KwInternal;
        const NamedTypeAccessibility accessibility =
            node->tokenKind() == lexer::TokenKind::KwPublic
                ? NamedTypeAccessibility::Public
                : NamedTypeAccessibility::Internal;

        pendingTypes_.push_back(PendingTypeDeclaration{
            std::move(identity),
            NamedTypeDeclarationPart{
                namedTypeKind(node->kind()),
                accessibility,
                hasExplicitAccessibility,
                (node->kind() == ast::AstNodeKind::ClassDeclaration ||
                 node->kind() == ast::AstNodeKind::InterfaceDeclaration) && node->flag(),
                node->kind() == ast::AstNodeKind::ClassDeclaration &&
                    node->hasModifier(lexer::TokenKind::KwAbstract),
                node->kind() == ast::AstNodeKind::ClassDeclaration &&
                    node->hasModifier(lexer::TokenKind::KwSealed),
                unit.projectOrdinal(),
                unit.sourceOrdinal(),
                unit.fileId(),
                node->range(),
                node,
                sourcePath}});
        return;
    }

    if (node->kind() == ast::AstNodeKind::CompilationUnit)
    {
        for (const ast::AstNode* child : node->children())
        {
            predeclareNode(unit, child, namespaceSegments, sourcePath);
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
        for (std::size_t index = groupBegin; index < groupEnd; ++index)
        {
            const PendingTypeDeclaration& pending = pendingTypes_[index];
            const NamedTypeDeclarationPart& part = pending.part;
            compilation::TypePartDescriptor descriptor{
                pending.identity,
                compilationTypeKind(part.kind),
                declaredAccessibility(part),
                part.isPartial,
                part.isAbstract,
                part.isSealed,
                false,
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

        const bool aggregated = collected && aggregator.finalize(aggregationError);
        if (!aggregated)
        {
            const std::size_t diagnosticIndex = groupEnd - groupBegin > 1
                ? groupBegin + 1
                : groupBegin;
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
                    pendingTypes_[groupBegin].part.range,
                    "previous declaration of this type is here");
            }
        }

        const PendingTypeDeclaration& primary = pendingTypes_[groupBegin];
        NamedTypeDeclarationKind finalKind = primary.part.kind;
        NamedTypeAccessibility finalAccessibility = primary.part.accessibility;
        bool finalPartial = primary.part.isPartial;
        bool finalAbstract = primary.part.isAbstract;
        bool finalSealed = primary.part.isSealed;
        if (aggregated && !aggregator.types().empty())
        {
            finalAccessibility = namedAccessibility(aggregator.types().front().accessibility);
            finalAbstract = aggregator.types().front().isAbstract;
            finalSealed = aggregator.types().front().isSealed;
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
            finalPartial,
            finalAbstract,
            finalSealed,
            primary.part.projectOrdinal,
            primary.part.sourceOrdinal,
            primary.part.fileId,
            primary.part.range,
            primary.part.syntax,
            std::move(declarationParts)});

        const std::string fullyQualifiedName = primary.identity.fullyQualifiedName();
        typesByModule_[primary.part.projectOrdinal].emplace(fullyQualifiedName, typeOrdinal);
        ModuleDeclarationSpace& module = *modules_[primary.part.projectOrdinal];
        module.addTypeOrdinal(typeOrdinal);
        NamespaceSymbol& namespaceSymbol = module.globalNamespace().ensureNamespace(
            primary.identity.namespaceSegments());
        namespaceSymbol.addTypeOrdinal(typeOrdinal);

        groupBegin = groupEnd;
    }
}

void ProjectNameLookup::bindUnit(
    const ProjectSyntaxUnit& unit,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const LookupEnvironment environment;
    bindScope(unit, unit.syntaxTree(), {}, environment, diagnostics);
}

void ProjectNameLookup::bindScope(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* scopeNode,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& inheritedEnvironment,
    diagnostics::DiagnosticEngine& diagnostics)
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
            bindScope(unit, child, nestedNamespace, environment, diagnostics);
            continue;
        }
        bindNode(unit, child, namespaceSegments, environment, diagnostics);
    }
}

void ProjectNameLookup::bindNode(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node == nullptr)
    {
        return;
    }

    if (!node->genericParameters().empty())
    {
        return;
    }

    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        std::vector<std::string> nestedNamespace = namespaceSegments;
        const std::vector<std::string> segments = splitQualifiedName(node->text());
        nestedNamespace.insert(nestedNamespace.end(), segments.begin(), segments.end());
        bindScope(unit, node, nestedNamespace, environment, diagnostics);
        return;
    }

    if (node->typeSyntax() != nullptr)
    {
        bindTypeSyntax(unit, node->typeSyntax(), namespaceSegments, environment, diagnostics);
        for (const ast::TypeModifierSyntax& modifier : node->typeSyntax()->modifiers())
        {
            if (modifier.arrayCount != nullptr)
            {
                bindNode(unit, modifier.arrayCount, namespaceSegments, environment, diagnostics);
            }
        }
    }
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
    {
        bindTypeSyntax(unit, syntax, namespaceSegments, environment, diagnostics);
    }

    bindNode(unit, node->first(), namespaceSegments, environment, diagnostics);
    bindNode(unit, node->second(), namespaceSegments, environment, diagnostics);
    bindNode(unit, node->third(), namespaceSegments, environment, diagnostics);
    for (const ast::AstNode* child : node->children())
    {
        bindNode(unit, child, namespaceSegments, environment, diagnostics);
    }
}

void ProjectNameLookup::bindTypeSyntax(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (syntax == nullptr || bindingBySyntax_.contains(syntax))
    {
        return;
    }

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
        while (!lexer::isBuiltinTypeKeyword(mutableSyntax->baseTokenKind()) &&
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
            mutableSyntax->prependModifiers(target->modifiers());
        }
    }

    for (const ast::TypeSyntax* argument : syntax->typeArguments())
    {
        bindTypeSyntax(unit, argument, namespaceSegments, environment, diagnostics);
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
        : namespaceSegments;

    TypeNameBinding binding;
    binding.syntax = syntax;
    binding.projectOrdinal = effectiveProjectOrdinal;
    binding.fileId = unit.fileId();
    binding.namespaceName = joinSegments(effectiveNamespaceSegments);
    binding.writtenName = writtenName;

    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
    {
        binding.kind = TypeNameBindingKind::Builtin;
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
            diagnostics.report(
                diagnostics::DiagnosticId::UnknownTypeName,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "unknown type name '" + syntax->baseName() + "' in module '" +
                    std::string(unit.moduleName()) + "'");
        }
        else if (binding.candidateTypeOrdinals.size() == 1)
        {
            binding.kind = TypeNameBindingKind::Resolved;
            binding.targetTypeOrdinal = binding.candidateTypeOrdinals.front();
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

    const std::size_t ordinal = bindings_.size();
    bindings_.push_back(std::move(binding));
    bindingBySyntax_.emplace(syntax, ordinal);
}

std::vector<std::size_t> ProjectNameLookup::resolveTypeCandidates(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    const std::string& writtenName) const
{
    const std::vector<std::string> nameSegments = splitQualifiedName(writtenName);
    if (nameSegments.empty() || projectOrdinal >= typesByModule_.size())
    {
        return {};
    }

    const auto alias = environment.aliases.find(nameSegments.front());
    if (alias != environment.aliases.end())
    {
        std::string substituted = alias->second;
        for (std::size_t index = 1; index < nameSegments.size(); ++index)
        {
            substituted.push_back('.');
            substituted += nameSegments[index];
        }
        return resolveQualifiedTypeCandidates(projectOrdinal, namespaceSegments, substituted);
    }

    if (nameSegments.size() > 1)
    {
        return resolveQualifiedTypeCandidates(projectOrdinal, namespaceSegments, writtenName);
    }

    // Stage 21 lookup order: current/enclosing namespace declarations first.
    for (std::size_t depth = namespaceSegments.size(); depth > 0; --depth)
    {
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), depth),
            writtenName);
        const auto current = typesByModule_[projectOrdinal].find(candidate);
        if (current != typesByModule_[projectOrdinal].end())
        {
            return {current->second};
        }
    }

    // Imported namespaces form one lookup tier. Equal candidates are ambiguous.
    std::vector<std::size_t> imported;
    for (const std::string& importedNamespace : environment.importedNamespaces)
    {
        std::string qualified = importedNamespace;
        if (!qualified.empty())
        {
            qualified.push_back('.');
        }
        qualified += writtenName;
        const std::vector<std::size_t> candidates = findExactTypeCandidates(
            projectOrdinal,
            qualified,
            false);
        appendUnique(imported, candidates);
    }
    if (!imported.empty())
    {
        return imported;
    }

    // Current module global namespace precedes referenced-module exports.
    const auto global = typesByModule_[projectOrdinal].find(writtenName);
    if (global != typesByModule_[projectOrdinal].end())
    {
        return {global->second};
    }

    // Referenced public exports are considered last, preserving namespace nesting.
    for (std::size_t depth = namespaceSegments.size(); depth > 0; --depth)
    {
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), depth),
            writtenName);
        std::vector<std::size_t> referenced;
        for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
        {
            const auto iterator = typesByModule_[visibleOrdinal].find(candidate);
            if (iterator != typesByModule_[visibleOrdinal].end() &&
                namedTypes_[iterator->second].accessibility == NamedTypeAccessibility::Public)
            {
                referenced.push_back(iterator->second);
            }
        }
        if (!referenced.empty())
        {
            return referenced;
        }
    }

    std::vector<std::size_t> referencedGlobal;
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        const auto iterator = typesByModule_[visibleOrdinal].find(writtenName);
        if (iterator != typesByModule_[visibleOrdinal].end() &&
            namedTypes_[iterator->second].accessibility == NamedTypeAccessibility::Public)
        {
            referencedGlobal.push_back(iterator->second);
        }
    }
    return referencedGlobal;
}

std::vector<std::size_t> ProjectNameLookup::resolveQualifiedTypeCandidates(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const std::string& qualifiedName) const
{
    if (projectOrdinal >= typesByModule_.size())
    {
        return {};
    }

    // Exhaust all lexical namespaces in the current module before consulting references.
    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        const std::size_t prefixCount = depth - 1;
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), prefixCount),
            qualifiedName);
        const auto current = typesByModule_[projectOrdinal].find(candidate);
        if (current != typesByModule_[projectOrdinal].end())
        {
            return {current->second};
        }
    }

    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        const std::size_t prefixCount = depth - 1;
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), prefixCount),
            qualifiedName);
        std::vector<std::size_t> referenced;
        for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
        {
            const auto iterator = typesByModule_[visibleOrdinal].find(candidate);
            if (iterator != typesByModule_[visibleOrdinal].end() &&
                namedTypes_[iterator->second].accessibility == NamedTypeAccessibility::Public)
            {
                referenced.push_back(iterator->second);
            }
        }
        if (!referenced.empty())
        {
            return referenced;
        }
    }
    return {};
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
            namedTypes_[iterator->second].accessibility == NamedTypeAccessibility::Public)
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
        case NamedTypeAccessibility::Internal:
            return "internal";
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
