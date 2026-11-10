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

bool ProjectNameLookup::setImportedNamespaces(
    std::vector<std::pair<std::size_t, std::string>> namespaces)
{
    if (built_ || !modules_.empty()) return false;
    importedNamespaces_ = std::move(namespaces);
    return true;
}

bool ProjectNameLookup::setCppImportedTypeAliases(std::vector<CppImportedTypeAlias> aliases)
{
    if (built_ || !modules_.empty()) return false;
    cppImportedTypeAliases_ = std::move(aliases);
    return true;
}

bool ProjectNameLookup::setCppImportedTypes(std::vector<CppImportedTypeDeclaration> types)
{
    if (built_ || !modules_.empty()) return false;
    for (const auto& type : types)
    {
        if (type.enumBase)
        {
            using T = lexer::TokenKind;
            const auto& base = *type.enumBase;
            const std::array allowed{
                std::pair{T::KwSByte, "sbyte"}, std::pair{T::KwByte, "byte"},
                std::pair{T::KwShort, "short"}, std::pair{T::KwUShort, "ushort"},
                std::pair{T::KwInt, "int"}, std::pair{T::KwUInt, "uint"},
                std::pair{T::KwLong, "long"}, std::pair{T::KwULong, "ulong"}};
            if (type.kind != NamedTypeDeclarationKind::Enum ||
                std::none_of(allowed.begin(), allowed.end(), [&](const auto& item) {
                    return item.first == base.token && item.second == base.name;
                })) return false;
        }
        else if (type.kind == NamedTypeDeclarationKind::Enum) return false;
        for (const auto& layout : {type.objectLayout, type.memberAccessLayout})
        if (layout)
        {
            const auto size = layout->sizeBytes;
            const auto alignment = layout->alignmentBytes;
            if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0 || size % alignment != 0)
                return false;
        }
    }
    const auto sameLayout = [](const auto& left, const auto& right) {
        return left.has_value() == right.has_value() && (!left ||
            (left->sizeBytes == right->sizeBytes && left->alignmentBytes == right->alignmentBytes));
    };
    const auto sameEnumBase = [](const auto& left, const auto& right) {
        return left.has_value() == right.has_value() && (!left ||
            (left->token == right->token && left->name == right->name));
    };
    for (const auto& type : types)
    {
        if (!type.reconciledIdentityKey) continue;
        const auto& targetKey = *type.reconciledIdentityKey;
        if (targetKey.empty() || targetKey == type.identity.key() ||
            std::count_if(types.begin(), types.end(), [&](const auto& item) {
                return item.identity.key() == targetKey;
            }) != 1) return false;
        const auto target = std::find_if(types.begin(), types.end(), [&](const auto& item) {
            return item.identity.key() == targetKey;
        });
        // EN: T08-D16: a live incomplete C record (the consumer's own `struct Opaque;`) may name the complete record
        //     that a producer published with the same tag; it has no layout of its own to compare.
        // ES: T08-D16: un record C incompleto vivo (el `struct Opaque;` propio del consumidor) puede nombrar el record
        //     completo que publico un productor con la misma etiqueta; no tiene layout propio que comparar.
        const bool completesLiveRecord = type.isIncomplete && !target->isIncomplete &&
            type.origin != CppImportedTypeOrigin::CEnum && !type.objectLayout && !type.memberAccessLayout;
        if (target->reconciledIdentityKey || type.projectOrdinal == target->projectOrdinal ||
            (type.origin != CppImportedTypeOrigin::CStruct && type.origin != CppImportedTypeOrigin::CUnion &&
             type.origin != CppImportedTypeOrigin::CEnum) ||
            type.origin != target->origin || type.kind != target->kind ||
            type.identity.fullyQualifiedName() != target->identity.fullyQualifiedName() ||
            !type.targetTriple || type.targetTriple != target->targetTriple ||
            !type.clangUsr || type.clangUsr->empty() || type.clangUsr != target->clangUsr ||
            (type.isIncomplete != target->isIncomplete && !completesLiveRecord) || !type.deferredImportError.empty() ||
            !target->deferredImportError.empty() || !sameEnumBase(type.enumBase, target->enumBase) ||
            (type.origin == CppImportedTypeOrigin::CEnum && !type.enumBase) ||
            (!completesLiveRecord && (!sameLayout(type.objectLayout, target->objectLayout) ||
                                      !sameLayout(type.memberAccessLayout, target->memberAccessLayout)))) return false;
    }
    cppImportedTypes_ = std::move(types);
    return true;
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
    driver::CompilationWorkerPool workerPool(workerCount);
    return build(graph, units, diagnostics, workerCount, &workerPool);
}

bool ProjectNameLookup::build(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t,
    driver::CompilationWorkerPool* workerPool)
{
    if (built_ || !modules_.empty() || !pendingTypes_.empty() || !namedTypes_.empty() ||
        !namespaceFunctions_.empty() || !namespaceStates_.empty() || !bindings_.empty())
        return false;

    initializeModules(graph);
    if (!installImportedNamespaces(diagnostics)) return false;
    buildVisibility(graph);
    driver::CompilationWorkerPool& workers = *workerPool;
    collectDeclarationIndexes(graph, units, workers);
    cppImportedSyntheticSyntax_.assign(cppImportedTypes_.size(), nullptr);
    for (std::size_t importedIndex = 0; importedIndex < cppImportedTypes_.size(); ++importedIndex)
    {
        const auto& imported = cppImportedTypes_[importedIndex];
        if (imported.projectOrdinal >= graph.projects().size() || !imported.identity.isValid() ||
            imported.identity.moduleName() != graph.projects()[imported.projectOrdinal].moduleName() ||
            imported.sourcePath.empty())
            return false;
        NamedTypeDeclarationPart part;
        part.kind = imported.kind;
        part.accessibility = imported.accessibility;
        part.hasExplicitAccessibility = true;
        part.isNested = !imported.identity.containingTypes().empty();
        part.projectOrdinal = imported.projectOrdinal;
        part.sourcePath = imported.sourcePath;
        if (imported.enumBase)
        {
            if (!cppImportedSyntax_) cppImportedSyntax_ = std::make_unique<ast::AstArena>();
            auto* node = cppImportedSyntax_->make<ast::AstNode>(ast::AstNodeKind::EnumDeclaration, SourceRange{});
            node->setText(imported.identity.name());
            auto* base = cppImportedSyntax_->make<ast::TypeSyntax>(SourceRange{},
                imported.enumBase->token, imported.enumBase->name);
            node->setTypeSyntax(base);
            part.syntax = node;
            cppImportedSyntheticSyntax_[importedIndex] = node;
        }
        pendingTypes_.push_back({imported.identity, std::move(part)});
    }
    reportUnsupportedGlobalDeclarations(units, diagnostics);
    finalizePredeclarations(diagnostics);
    finalizeReexports(units, diagnostics);
    indexes_->rebuildTypes(namedTypes_);
    for (const auto& imported : cppImportedTypes_)
    {
        if (!imported.reconciledIdentityKey) continue;
        const auto live = indexes_->typeOrdinalForIdentity(imported.identity.key());
        const auto canonical = indexes_->typeOrdinalForIdentity(*imported.reconciledIdentityKey);
        const auto* target = canonical ? indexes_->typeForOrdinal(*canonical) : nullptr;
        if (!live || !canonical || !target || imported.projectOrdinal >= modules_.size() ||
            !modules_[imported.projectOrdinal] || !typeIsVisibleTo(imported.projectOrdinal, *canonical)) return false;
        const auto visible = modules_[imported.projectOrdinal]->visibleProjectOrdinals();
        const auto sees = [&](std::size_t project) {
            return std::find(visible.begin(), visible.end(), project) != visible.end();
        };
        // EN: T08-D26. A precompiled producer's C type reconciled with the equivalent one of another producer
        //     names the canonical type although neither library references the other; a live project reaches it
        //     through any visible producer that publishes the same declaration.
        // ES: T08-D26. El tipo C de un productor precompilado conciliado con el equivalente de otro productor
        //     nombra el tipo canonico aunque ninguna biblioteca referencie a la otra; un proyecto vivo llega a el
        //     por cualquier productor visible que publique la misma declaracion.
        const bool reachable = sees(target->projectOrdinal) || imported.fromPrecompiledCatalog ||
            std::any_of(cppImportedTypes_.begin(), cppImportedTypes_.end(), [&](const auto& other) {
                return other.fromPrecompiledCatalog && other.reconciledIdentityKey == imported.reconciledIdentityKey &&
                    sees(other.projectOrdinal);
            });
        if (!reachable || !reconciledCppTypeOrdinals_.emplace(*live, *canonical).second) return false;
    }
    finalizeNamespaceDeclarations();
    collectBindingIndexes(units, diagnostics, workers);
    built_ = true;
    if (diagnostics.hasErrors()) return false;
    std::unordered_map<std::size_t, std::size_t> importedIndices;
    for (std::size_t index = 0; index < cppImportedTypes_.size(); ++index)
    {
        const auto& imported = cppImportedTypes_[index];
        const auto ordinal = indexes_->typeOrdinalForIdentity(imported.identity.key());
        const auto* declaration = ordinal ? indexes_->typeForOrdinal(*ordinal) : nullptr;
        if (!declaration || declaration->projectOrdinal != imported.projectOrdinal ||
            declaration->parts.size() != 1 || declaration->parts.front().syntax != cppImportedSyntheticSyntax_[index] ||
            declaration->parts.front().sourcePath != imported.sourcePath ||
            !importedIndices.emplace(*ordinal, index).second)
            return false;
    }
    cppImportedTypeIndices_ = std::move(importedIndices);
    return true;
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
    const auto ordinal = indexes_->typeOrdinalForIdentity(identity);
    return ordinal ? std::optional<std::size_t>(canonicalCppTypeOrdinal(*ordinal)) : std::nullopt;
}

std::size_t ProjectNameLookup::canonicalCppTypeOrdinal(std::size_t ordinal) const noexcept
{
    const auto found = reconciledCppTypeOrdinals_.find(ordinal);
    return found == reconciledCppTypeOrdinals_.end() ? ordinal : found->second;
}

bool ProjectNameLookup::isReconciledCppTypeIdentity(std::string_view identity) const
{
    const auto ordinal = indexes_->typeOrdinalForIdentity(identity);
    return ordinal && reconciledCppTypeOrdinals_.contains(*ordinal);
}

bool ProjectNameLookup::isReconciledCppTypeVisibleToProject(
    std::size_t canonicalOrdinal, std::size_t projectOrdinal) const noexcept
{
    for (const auto& [localOrdinal, targetOrdinal] : reconciledCppTypeOrdinals_)
    {
        const auto* local = cppImportedType(localOrdinal);
        if (targetOrdinal == canonicalOrdinal && local && local->projectOrdinal == projectOrdinal)
            return true;
    }
    return false;
}

std::vector<std::pair<std::string, std::string>> ProjectNameLookup::reconciledCppTypeKeys() const
{
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& [localOrdinal, targetOrdinal] : reconciledCppTypeOrdinals_)
    {
        const auto* local = indexes_->typeForOrdinal(localOrdinal);
        const auto* target = indexes_->typeForOrdinal(targetOrdinal);
        if (local && target)
            result.emplace_back("type:" + local->identity.key(), "type:" + target->identity.key());
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.first.size() != right.first.size() ? left.first.size() > right.first.size() :
            left.first < right.first;
    });
    return result;
}

std::optional<std::size_t> ProjectNameLookup::cppImportedTypeIndex(std::size_t typeOrdinal) const noexcept
{
    const auto found = cppImportedTypeIndices_.find(typeOrdinal);
    return found == cppImportedTypeIndices_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
}

const CppImportedTypeDeclaration* ProjectNameLookup::cppImportedType(std::size_t typeOrdinal) const noexcept
{
    const auto index = cppImportedTypeIndex(typeOrdinal);
    return index && *index < cppImportedTypes_.size() ? &cppImportedTypes_[*index] : nullptr;
}

std::span<const std::size_t> ProjectNameLookup::typeOrdinalsForName(
    std::string_view name) const
{
    return indexes_->typeOrdinalsForName(name);
}

bool ProjectNameLookup::hasTypeIndex() const noexcept
{
    return indexes_->hasTypeIndex();
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
