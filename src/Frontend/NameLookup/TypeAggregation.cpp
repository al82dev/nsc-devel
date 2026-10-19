#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

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
namespace
{
bool isTraitImplementationPart(const NamedTypeDeclarationPart& part) noexcept
{
    return part.syntax != nullptr && part.syntax->kind() == ast::AstNodeKind::ClassDeclaration &&
        part.syntax->auxiliaryText() == "ns3-trait-impl";
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
}

bool ProjectNameLookup::pendingTypeLess(
    const PendingTypeDeclaration& left,
    const PendingTypeDeclaration& right)
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
}

std::size_t ProjectNameLookup::pendingTypeGroupEnd(std::size_t groupBegin) const
{
    std::size_t groupEnd = groupBegin + 1;
    while (groupEnd < pendingTypes_.size() &&
           pendingTypes_[groupEnd].identity == pendingTypes_[groupBegin].identity)
    {
        ++groupEnd;
    }
    return groupEnd;
}

bool ProjectNameLookup::collectPendingTypeGroup(
    std::size_t groupBegin,
    std::size_t groupEnd,
    compilation::PartialTypeAggregator& aggregator,
    std::string& aggregationError,
    std::optional<std::size_t>& primaryIndex) const
{
    for (std::size_t index = groupBegin; index < groupEnd; ++index)
    {
        const PendingTypeDeclaration& pending = pendingTypes_[index];
        const NamedTypeDeclarationPart& part = pending.part;
        if (isTraitImplementationPart(part)) continue;
        if (!primaryIndex.has_value()) primaryIndex = index;
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
        if (!aggregator.addPart(std::move(descriptor), aggregationError)) return false;
    }
    return true;
}

bool ProjectNameLookup::moduleOwnsTraitImplementation(
    std::size_t groupBegin,
    std::size_t groupEnd) const
{
    for (std::size_t index = groupBegin; index < groupEnd; ++index)
    {
        const PendingTypeDeclaration& pending = pendingTypes_[index];
        if (!isTraitImplementationPart(pending.part) || pending.part.syntax == nullptr) continue;
        const ast::TypeSyntax* trait = pending.part.syntax->typeSyntax();
        if (trait == nullptr || trait->baseName().empty()) continue;
        const std::string& traitName = trait->baseName();
        const std::size_t implProject = pending.part.projectOrdinal;
        for (const PendingTypeDeclaration& local : pendingTypes_)
        {
            if (local.part.syntax == nullptr ||
                local.part.syntax->kind() != ast::AstNodeKind::InterfaceDeclaration)
                continue;
            if (local.part.projectOrdinal == implProject && local.part.syntax->text() == traitName)
                return true;
        }
    }
    return false;
}

void ProjectNameLookup::finalizeOrphanTraitImplementationGroup(
    std::size_t groupBegin,
    std::size_t groupEnd,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T08-F05: orphan rule — the module may own the trait even when it does not own
    // the target type. `impl LocalTrait for ExternalType` is coherent when the trait is
    // declared by this module; owning neither is the orphan rejection.
    if (!moduleOwnsTraitImplementation(groupBegin, groupEnd))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidBaseType,
            diagnostics::DiagnosticSeverity::Error,
            pendingTypes_[groupBegin].part.range,
            "orphan impl: the module owns neither the target type '" +
                pendingTypes_[groupBegin].identity.fullyQualifiedName() + "' nor the trait");
        return;
    }
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

void ProjectNameLookup::validateTraitImplementationTarget(
    std::size_t groupBegin,
    std::size_t groupEnd,
    const PendingTypeDeclaration& primary,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const PendingTypeDeclaration* implementation = nullptr;
    for (std::size_t index = groupBegin; index < groupEnd; ++index)
    {
        if (isTraitImplementationPart(pendingTypes_[index].part))
        {
            implementation = &pendingTypes_[index];
            break;
        }
    }
    if (implementation == nullptr) return;
    // NS304-T11-F06: a borrowed (ref-like) struct view is a valid static witness target.
    const bool isBorrowedStructTarget =
        primary.part.kind == NamedTypeDeclarationKind::Struct &&
        primary.part.syntax != nullptr &&
        primary.part.syntax->auxiliaryText().find("borrowedview") != std::string::npos;
    if (isBorrowedStructTarget ||
        (primary.part.kind == NamedTypeDeclarationKind::Class && !primary.part.isStatic))
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidBaseType,
        diagnostics::DiagnosticSeverity::Error,
        implementation->part.range,
        "a trait impl target must be a non-static class declared by this module");
}

void ProjectNameLookup::reportTypeAggregationFailure(
    std::size_t groupBegin,
    std::size_t groupEnd,
    std::size_t primaryIndex,
    const std::string& aggregationError,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const diagnostics::DiagnosticId diagnosticId = aggregationError.starts_with(
        "duplicate non-partial type declaration")
            ? diagnostics::DiagnosticId::DuplicateSymbol
            : diagnostics::DiagnosticId::PartialTypeConflict;
    diagnostics.report(
        diagnosticId,
        diagnostics::DiagnosticSeverity::Error,
        pendingTypes_[primaryIndex].part.range,
        aggregationError);
    if (groupEnd - groupBegin > 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::PreviousDeclaration,
            diagnostics::DiagnosticSeverity::Note,
            pendingTypes_[primaryIndex].part.range,
            "previous declaration of this type is here");
    }
}

NamedTypeAccessibility ProjectNameLookup::aggregatedTypeAccessibility(
    std::size_t groupBegin,
    std::size_t groupEnd,
    const PendingTypeDeclaration& primary,
    const compilation::PartialTypeAggregator& aggregator,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (primary.part.isNested) return primary.part.accessibility;
    NamedTypeAccessibility accessibility =
        namedAccessibility(aggregator.types().front().accessibility);
    bool anyPackage = false;
    bool allPackage = true;
    for (std::size_t index = groupBegin; index < groupEnd; ++index)
    {
        const bool isPackage =
            pendingTypes_[index].part.accessibility == NamedTypeAccessibility::Package;
        anyPackage = anyPackage || isPackage;
        allPackage = allPackage && isPackage;
    }
    if (anyPackage && !allPackage)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::PartialTypeConflict,
            diagnostics::DiagnosticSeverity::Error,
            primary.part.range,
            "all parts of a package-visible partial type must declare package visibility");
    }
    if (allPackage) accessibility = NamedTypeAccessibility::Package;
    return accessibility;
}

void ProjectNameLookup::applyAggregatedTypeProperties(
    std::size_t groupBegin,
    std::size_t groupEnd,
    const PendingTypeDeclaration& primary,
    const compilation::PartialTypeAggregator& aggregator,
    diagnostics::DiagnosticEngine& diagnostics,
    NamedTypeAccessibility& accessibility,
    bool& isPartial,
    bool& isAbstract,
    bool& isSealed,
    bool& isStatic) const
{
    // NSC-LIMIT-009. El agregador de partes solo conoce internal/public --es lo unico que un
    // tipo de namespace puede ser-- asi que para un ANIDADO se conserva la accesibilidad de
    // su primera parte. El agregador si aporta las demas propiedades: las clases an anidadas
    // `partial` se usan al reconstruir propietarios de plantillas desde NSMETA y deben unir
    // static/abstract/sealed igual que una clase de namespace.
    accessibility = aggregatedTypeAccessibility(
        groupBegin, groupEnd, primary, aggregator, diagnostics);
    isAbstract = aggregator.types().front().isAbstract;
    isSealed = aggregator.types().front().isSealed;
    isStatic = aggregator.types().front().isStatic;
    isPartial = false;
    for (const compilation::TypePartDescriptor& part : aggregator.types().front().parts)
    {
        if (part.isPartial)
        {
            isPartial = true;
            break;
        }
    }
}

std::vector<NamedTypeDeclarationPart> ProjectNameLookup::pendingDeclarationParts(
    std::size_t groupBegin,
    std::size_t groupEnd,
    const PendingTypeDeclaration& primary,
    bool aggregated) const
{
    std::vector<NamedTypeDeclarationPart> declarationParts;
    declarationParts.reserve(groupEnd - groupBegin);
    if (aggregated)
    {
        for (std::size_t index = groupBegin; index < groupEnd; ++index)
            declarationParts.push_back(pendingTypes_[index].part);
    }
    else
    {
        declarationParts.push_back(primary.part);
    }
    return declarationParts;
}

std::size_t ProjectNameLookup::appendNamedType(
    const PendingTypeDeclaration& primary,
    NamedTypeAccessibility accessibility,
    bool isPartial,
    bool isAbstract,
    bool isSealed,
    bool isStatic,
    std::vector<NamedTypeDeclarationPart> declarationParts)
{
    const std::size_t typeOrdinal = namedTypes_.size();
    namedTypes_.push_back(NamedTypeDeclaration{
        primary.identity,
        primary.part.kind,
        accessibility,
        std::nullopt,
        isPartial,
        isAbstract,
        isSealed,
        isStatic,
        primary.part.isPrecompiled,
        primary.part.projectOrdinal,
        primary.part.sourceOrdinal,
        primary.part.fileId,
        primary.part.range,
        primary.part.syntax,
        std::move(declarationParts)});
    return typeOrdinal;
}

void ProjectNameLookup::registerNestedSourceName(
    const PendingTypeDeclaration& primary,
    std::size_t typeOrdinal)
{
    // NSC-LIMIT-009. La forma FUENTE, con puntos, en un indice aparte. La clave canonica de
    // arriba lleva `+` y no se toca: son dos preguntas distintas --«que tipo es este» y «que
    // ha escrito el programador»-- y colapsarlas en un mapa haria indistinguible un anidado
    // `ns.Outer.Item` de un tipo `Item` en el namespace `ns.Outer`, que es precisamente la
    // ambiguedad que hay que poder detectar.
    if (primary.identity.containingTypes().empty()) return;
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
    nestedSourceNamesByModule_[primary.part.projectOrdinal].emplace(sourceName, typeOrdinal);
}

void ProjectNameLookup::registerTypeDeclarationIndexes(
    const PendingTypeDeclaration& primary,
    std::size_t typeOrdinal)
{
    const std::string fullyQualifiedName = primary.identity.fullyQualifiedName();
    typesByModule_[primary.part.projectOrdinal].emplace(fullyQualifiedName, typeOrdinal);
    registerNestedSourceName(primary, typeOrdinal);
    for (const NamedTypeDeclarationPart& part : namedTypes_[typeOrdinal].parts)
    {
        if (part.syntax != nullptr) typeOrdinalBySyntax_[part.syntax] = typeOrdinal;
    }
    ModuleDeclarationSpace& module = *modules_[primary.part.projectOrdinal];
    module.addTypeOrdinal(typeOrdinal);
    NamespaceSymbol& namespaceSymbol =
        module.globalNamespace().ensureNamespace(primary.identity.namespaceSegments());
    namespaceSymbol.addTypeOrdinal(typeOrdinal);
}

void ProjectNameLookup::finalizePredeclarationGroup(
    std::size_t groupBegin,
    std::size_t groupEnd,
    diagnostics::DiagnosticEngine& diagnostics)
{
    compilation::PartialTypeAggregator aggregator;
    std::string aggregationError;
    std::optional<std::size_t> primaryIndex;
    const bool collected = collectPendingTypeGroup(
        groupBegin, groupEnd, aggregator, aggregationError, primaryIndex);
    if (!primaryIndex.has_value())
    {
        finalizeOrphanTraitImplementationGroup(groupBegin, groupEnd, diagnostics);
        return;
    }
    const PendingTypeDeclaration& primary = pendingTypes_[*primaryIndex];
    validateTraitImplementationTarget(groupBegin, groupEnd, primary, diagnostics);
    const bool aggregated = collected && aggregator.finalize(aggregationError);
    if (!aggregated)
        reportTypeAggregationFailure(
            groupBegin, groupEnd, *primaryIndex, aggregationError, diagnostics);
    NamedTypeAccessibility accessibility = primary.part.accessibility;
    bool isPartial = primary.part.isPartial;
    bool isAbstract = primary.part.isAbstract;
    bool isSealed = primary.part.isSealed;
    bool isStatic = primary.part.isStatic;
    if (aggregated && !aggregator.types().empty())
        applyAggregatedTypeProperties(
            groupBegin, groupEnd, primary, aggregator, diagnostics,
            accessibility, isPartial, isAbstract, isSealed, isStatic);
    else if (!primary.part.hasExplicitAccessibility)
        accessibility = NamedTypeAccessibility::Internal;
    std::vector<NamedTypeDeclarationPart> declarationParts =
        pendingDeclarationParts(groupBegin, groupEnd, primary, aggregated);
    const std::size_t typeOrdinal = appendNamedType(
        primary, accessibility, isPartial, isAbstract, isSealed, isStatic,
        std::move(declarationParts));
    registerTypeDeclarationIndexes(primary, typeOrdinal);
}

void ProjectNameLookup::finalizeContainingTypeOrdinals()
{
    // NSC-LIMIT-009. Segunda pasada: el ORDINAL del contenedor de cada anidado. Se hace despues y
    // no durante el bucle porque el contenedor puede no estar registrado todavia cuando se
    // registra el anidado --el orden lo fija la ordenacion por clave, no el anidamiento-- y
    // depender de ese orden seria depender de que `+` ordene por detras del fin de cadena.
    for (NamedTypeDeclaration& type : namedTypes_)
    {
        if (type.identity.containingTypes().empty()) continue;
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
            type.containingTypeOrdinal = iterator->second;
    }
}

void ProjectNameLookup::finalizePredeclarations(
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::sort(pendingTypes_.begin(), pendingTypes_.end(), pendingTypeLess);
    std::size_t groupBegin = 0;
    while (groupBegin < pendingTypes_.size())
    {
        const std::size_t groupEnd = pendingTypeGroupEnd(groupBegin);
        finalizePredeclarationGroup(groupBegin, groupEnd, diagnostics);
        groupBegin = groupEnd;
    }
    finalizeContainingTypeOrdinals();
}

}
