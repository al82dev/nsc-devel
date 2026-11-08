#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"
#include "nativesharp/Frontend/CppAliasArrayType.h"
#include "nativesharp/Frontend/CppAliasEnumType.h"
#include "nativesharp/Frontend/CppAliasFunctionType.h"
#include "nativesharp/Frontend/CppAliasRecordType.h"
#include "nativesharp/Frontend/QualifiedNamespaceTarget.h"
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
#include <memory>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
std::vector<ast::TypeModifierSyntax> cppPointerModifiers(const interop::CppType& type)
{
    const interop::CppType* current = &type;
    while (current && current->kind == interop::CppTypeKind::Array) current = current->element.get();
    std::vector<ast::TypeModifierSyntax> result;
    while (current && current->kind == interop::CppTypeKind::Pointer)
    {
        ast::TypeModifierSyntax modifier;
        modifier.importedCRestrict = current->isRestrict;
        result.push_back(modifier);
        current = current->element.get();
    }
    std::reverse(result.begin(), result.end());
    return result;
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
}

void ProjectNameLookup::predeclareNamespaceFunction(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::string& sourcePath,
    UnitDeclarationIndex& index)
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
}

void ProjectNameLookup::predeclareNamespaceState(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::string& sourcePath,
    UnitDeclarationIndex& index)
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
}

void ProjectNameLookup::bindTypeSyntax(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (syntax == nullptr || index.bindingBySyntax.contains(syntax)) return;
    std::string constType;
    std::string constValue;
    if (language::parseConstGenericArgumentMarker(syntax->baseName(), constType, constValue))
        return;
    // `var`'s marker TypeSyntax (see Parser::parseVariableDeclaration) has no real named type to
    // resolve -- its real type is inferred entirely from the initializer expression, inside
    // ProjectMethodBodyModel::analyzeVariable, which never looks this binding up. Binding it here
    // like an ordinary identifier type would incorrectly report "unknown type name 'var'" (there
    // is, correctly, no class/interface/struct actually named "var").
    if (syntax->baseTokenKind() == lexer::TokenKind::KwVar) return;
    // The name the programmer actually WROTE, captured before any expansion: diagnostics and the
    // binding record must keep saying `PointAlias`, not the type it turned out to mean.
    const std::string writtenName = syntax->baseName();
    expandTypeAlias(syntax, environment, diagnostics);
    const CppImportedTypeAlias* expandedAlias = nullptr;
    if (!expandCppTypeAlias(unit, syntax, namespaceSegments, environment, diagnostics, expandedAlias)) return;
    bindChildTypeSyntaxes(unit, syntax, namespaceSegments, environment, diagnostics, index);
    // Stage 52 fix: a generic type argument substituted into a specialization cloned from a
    // referenced project's template (e.g. List<MyOwnClass> where List<T> lives in a shared
    // library) carries an explicit origin-project override -- see TypeSyntax::originProjectOrdinal
    // for the full explanation. Fall back to the enclosing unit's own project ordinal for every
    // ordinary (non-substituted) type syntax node, exactly as before.
    const std::size_t effectiveProjectOrdinal =
        syntax->originProjectOrdinal().value_or(unit.projectOrdinal());
    const std::vector<std::string> effectiveNamespaceSegments =
        effectiveTypeNamespace(syntax, namespaceSegments);
    TypeNameBinding binding;
    binding.syntax = syntax;
    binding.projectOrdinal = effectiveProjectOrdinal;
    binding.fileId = unit.fileId();
    binding.namespaceName = joinSegments(effectiveNamespaceSegments);
    binding.writtenName = writtenName;
    binding.expandedCppAlias = expandedAlias;
    binding.enclosingTypeOrdinals = environment.enclosingTypeOrdinals;
    const bool portableSimd = syntax->baseName() == "simd" || syntax->baseName() == "mask";
    if (isBuiltinTypeSurface(syntax, portableSimd))
        validateBuiltinTypeBinding(syntax, portableSimd, binding, diagnostics);
    else
        resolveTypeNameBinding(
            unit, syntax, effectiveNamespaceSegments, environment, binding, diagnostics);
    const std::size_t ordinal = index.bindings.size();
    index.bindings.push_back(std::move(binding));
    index.bindingBySyntax.emplace(syntax, ordinal);
}

bool ProjectNameLookup::expandCppTypeAlias(
    const ProjectSyntaxUnit& unit, const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments, const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics, const CppImportedTypeAlias*& expandedAlias) const
{
    if (cppImportedTypeAliases_.empty() || syntax->isFunction() ||
        lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) || !syntax->typeArguments().empty()) return true;
    const auto owner = syntax->originProjectOrdinal().value_or(unit.projectOrdinal());
    if (owner >= modules_.size()) return true;
    const auto effectiveNamespace = effectiveTypeNamespace(syntax, namespaceSegments);
    const auto enclosing = enclosingTypeSpellings(environment);
    QualifiedNameSite site;
    site.namespaceSegments = effectiveNamespace;
    site.enclosingTypeNames = enclosing;
    site.importedNamespaces = environment.importedNamespaces;
    site.aliases = &environment.aliases;
    const auto visible = modules_[owner]->visibleProjectOrdinals();
    const auto report = [&](diagnostics::DiagnosticId id, const std::string& message) {
        diagnostics.report(id, diagnostics::DiagnosticSeverity::Error, syntax->range(), message);
        return false;
    };
    for (const auto& rung : qualifiedNameLadder(site, syntax->baseName()))
    {
        const CppImportedTypeAlias* selected = nullptr;
        std::string selectedName;
        std::optional<std::string> knownAliasUsr;
        std::optional<std::string> knownRecordUsr;
        std::optional<std::string> knownEnumUsr;
        bool conflictingRecordUsr = false;
        bool conflictingEnumUsr = false;
        std::vector<std::size_t> nativeCandidates;
        for (const auto& spelling : rung.spellings)
        {
            const auto native = findExactTypeCandidates(owner, spelling, false);
            for (const auto ordinal : native)
                if (std::find(nativeCandidates.begin(), nativeCandidates.end(), ordinal) == nativeCandidates.end())
                    nativeCandidates.push_back(ordinal);
            for (const auto& alias : cppImportedTypeAliases_)
            {
                if (alias.declaration.insideAnonymousNamespace ||
                    (alias.projectOrdinal != owner &&
                     std::find(visible.begin(), visible.end(), alias.projectOrdinal) == visible.end())) continue;
                auto qualified = alias.declaration.qualifiedName;
                std::size_t pos = 0;
                while ((pos = qualified.find("::", pos)) != std::string::npos) qualified.replace(pos, 2, ".");
                if (qualified != spelling) continue;
                const auto reconciledRecordAlias = [&]() {
                    if (!selected || selected->projectOrdinal == alias.projectOrdinal ||
                        !selected->declaration.clangUsr || !alias.declaration.clangUsr ||
                        selected->declaration.clangUsr->empty() || alias.declaration.clangUsr->empty()) return false;
                    if (selected->declaration.clangUsr != alias.declaration.clangUsr)
                    {
                        const std::string suffix = "@T@" + alias.declaration.name;
                        const auto fileTypedef = [&](const std::string& value) {
                            return value.size() > suffix.size() + 2 && value.compare(0, 2, "c:") == 0 &&
                                value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
                        };
                        if (qualified != alias.declaration.name ||
                            !fileTypedef(*selected->declaration.clangUsr) ||
                            !fileTypedef(*alias.declaration.clangUsr)) return false;
                    }
                    const auto a = cppAliasRecordType(selected->declaration.underlyingType);
                    const auto b = cppAliasRecordType(alias.declaration.underlyingType);
                    const auto ea = cppAliasEnumType(selected->declaration.underlyingType);
                    const auto eb = cppAliasEnumType(alias.declaration.underlyingType);
                    std::string name;
                    std::optional<std::string> usr;
                    if (a && b && a->canonicalName == b->canonicalName && a->clangUsr && a->clangUsr == b->clangUsr)
                    { name = a->canonicalName; usr = a->clangUsr; }
                    else if (ea && eb && ea->name == eb->name && ea->clangUsr && ea->clangUsr == eb->clangUsr)
                    { name = ea->name; usr = ea->clangUsr; }
                    else return false;
                    for (const auto& live : cppImportedTypes_)
                    {
                        if (!live.reconciledIdentityKey || live.identity.fullyQualifiedName() != name ||
                            !live.clangUsr || live.clangUsr != usr ||
                            (live.projectOrdinal != selected->projectOrdinal && live.projectOrdinal != alias.projectOrdinal)) continue;
                        for (const auto& precompiled : cppImportedTypes_)
                            if (precompiled.identity.key() == *live.reconciledIdentityKey &&
                                precompiled.projectOrdinal != live.projectOrdinal &&
                                (precompiled.projectOrdinal == selected->projectOrdinal || precompiled.projectOrdinal == alias.projectOrdinal) &&
                                precompiled.clangUsr == live.clangUsr) return true;
                    }
                    return false;
                };
                const auto reconciliationDetails = [&]() {
                    std::string details;
                    for (const auto& type : cppImportedTypes_)
                        if (type.identity.name() == syntax->baseName())
                            details += " [" + std::to_string(type.projectOrdinal) + ":" + type.identity.key() +
                                ",usr=" + type.clangUsr.value_or("<none>") + ",target=" +
                                type.reconciledIdentityKey.value_or("<none>") + "]";
                    return details;
                };
                const auto compatibleFunctionAlias = [&]() {
                    if (!selected || !selected->declaration.clangUsr || !alias.declaration.clangUsr ||
                        selected->declaration.clangUsr->empty() || alias.declaration.clangUsr->empty()) return false;
                    const auto a = cppAliasFunctionType(selected->declaration.underlyingType);
                    const auto b = cppAliasFunctionType(alias.declaration.underlyingType);
                    if (!a || !b || a->callingConvention != b->callingConvention ||
                        a->variadic != b->variadic || a->parameters.size() != b->parameters.size()) return false;
                    const auto same = [](const auto& left, const auto& right) {
                        return left.token == right.token && left.pointerDepth == right.pointerDepth &&
                            left.pointeeConst == right.pointeeConst;
                    };
                    if (!same(a->result, b->result)) return false;
                    for (std::size_t i = 0; i < a->parameters.size(); ++i)
                        if (!same(a->parameters[i], b->parameters[i])) return false;
                    return true;
                };
                if (selected && (selectedName != qualified ||
                    (selected->projectOrdinal != alias.projectOrdinal && !reconciledRecordAlias() &&
                     !compatibleFunctionAlias()) ||
                    selected->targetTriple != alias.targetTriple ||
                    selected->declaration.underlyingType.canonicalSpelling != alias.declaration.underlyingType.canonicalSpelling))
                    return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                                  "conflicting C/C++ type alias '" + syntax->baseName() + "'" +
                                  " (projects " + std::to_string(selected->projectOrdinal) + "," +
                                  std::to_string(alias.projectOrdinal) + "; types '" +
                                  selected->declaration.underlyingType.canonicalSpelling + "','" +
                                  alias.declaration.underlyingType.canonicalSpelling + "'; USRs '" +
                                  selected->declaration.clangUsr.value_or("<none>") + "','" +
                                  alias.declaration.clangUsr.value_or("<none>") + "'; reconciled=" +
                                  (reconciledRecordAlias() ? "yes" : "no") + ")" + reconciliationDetails());
                const auto& usr = alias.declaration.clangUsr;
                const auto recordIdentity = cppAliasRecordType(alias.declaration.underlyingType);
                const auto enumIdentity = cppAliasEnumType(alias.declaration.underlyingType);
                if (usr && !usr->empty())
                {
                    if (knownAliasUsr && *knownAliasUsr != *usr)
                    {
                        // Clang's file-scope C typedef USR includes the header's
                        // basename, which is not part of the C type identity.
                        const std::string suffix = "@T@" + alias.declaration.name;
                        const auto isFileTypedef = [&](const std::string& value) {
                            return value.size() > suffix.size() + 2 && value.compare(0, 2, "c:") == 0 &&
                                value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
                        };
                        const auto compatible = [](const auto& left, const auto& right) {
                            return !left || left->empty() || !right || right->empty() || left == right;
                        };
                        if (qualified != alias.declaration.name || conflictingRecordUsr || conflictingEnumUsr ||
                            !isFileTypedef(*knownAliasUsr) ||
                            !isFileTypedef(*usr) ||
                            !compatible(knownRecordUsr, recordIdentity ? recordIdentity->clangUsr : std::nullopt) ||
                            !compatible(knownEnumUsr, enumIdentity ? enumIdentity->clangUsr : std::nullopt))
                            return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                                          "conflicting C/C++ type alias '" + syntax->baseName() + "'");
                    }
                    knownAliasUsr = usr;
                }
                if (recordIdentity && recordIdentity->clangUsr && !recordIdentity->clangUsr->empty())
                {
                    if (knownRecordUsr && knownRecordUsr != recordIdentity->clangUsr) conflictingRecordUsr = true;
                    else knownRecordUsr = recordIdentity->clangUsr;
                }
                if (enumIdentity && enumIdentity->clangUsr && !enumIdentity->clangUsr->empty())
                {
                    if (knownEnumUsr && knownEnumUsr != enumIdentity->clangUsr) conflictingEnumUsr = true;
                    else knownEnumUsr = enumIdentity->clangUsr;
                }
                selected = &alias;
                selectedName = qualified;
            }
        }
        if (!selected)
        {
            if (!nativeCandidates.empty()) return true;
            continue;
        }
        expandedAlias = selected;
        const auto& originalAlias = selected->declaration.underlyingType;
        const bool volatileAliasPointee = originalAlias.kind == interop::CppTypeKind::Pointer &&
            originalAlias.element && originalAlias.element->isVolatile &&
            (originalAlias.element->kind == interop::CppTypeKind::Record ||
             originalAlias.element->kind == interop::CppTypeKind::Enumeration);
        interop::CppType normalizedAlias = originalAlias;
        if (volatileAliasPointee)
        {
            auto element = std::make_shared<interop::CppType>(*originalAlias.element);
            element->isVolatile = false;
            normalizedAlias.element = std::move(element);
        }
        const auto record = cppAliasRecordType(normalizedAlias);
        const auto enumeration = cppAliasEnumType(normalizedAlias);
        const auto compatibleUsr = [](const auto& left, const auto& right) {
            return !left || !right || left == right;
        };
        if (!nativeCandidates.empty())
        {
            if (record && record->pointerDepth == 0 && !record->pointeeConst && nativeCandidates.size() == 1)
            {
                const auto* canonical = typeForOrdinal(nativeCandidates.front());
                if (canonical)
                    for (const auto& imported : cppImportedTypes_)
                        if (imported.projectOrdinal == selected->projectOrdinal && imported.reconciledIdentityKey &&
                            *imported.reconciledIdentityKey == canonical->identity.key() &&
                            imported.identity.fullyQualifiedName() == record->canonicalName &&
                            compatibleUsr(record->clangUsr, imported.clangUsr)) return true;
            }
            if (record && record->pointerDepth == 0 && !record->pointeeConst && nativeCandidates.size() == 1)
            {
                // Imported ordinal indices are finalized after binding. Inspect the already
                // aggregated declaration and its originating input instead of that later index.
                const auto* declaration = typeForOrdinal(nativeCandidates.front());
                if (declaration && declaration->projectOrdinal == selected->projectOrdinal &&
                    declaration->identity.fullyQualifiedName() == record->canonicalName &&
                    declaration->parts.size() == 1 && !declaration->parts.front().syntax)
                    for (const auto& imported : cppImportedTypes_)
                        if (imported.projectOrdinal == selected->projectOrdinal &&
                            imported.identity.key() == declaration->identity.key() &&
                            compatibleUsr(record->clangUsr, imported.clangUsr) &&
                            imported.sourcePath == declaration->parts.front().sourcePath) return true;
            }
            if (enumeration && enumeration->pointerDepth == 0 && !enumeration->pointeeConst &&
                nativeCandidates.size() == 1)
            {
                const auto* canonical = typeForOrdinal(nativeCandidates.front());
                if (canonical)
                    for (const auto& imported : cppImportedTypes_)
                        if (imported.projectOrdinal == selected->projectOrdinal && imported.enumBase &&
                            imported.reconciledIdentityKey &&
                            *imported.reconciledIdentityKey == canonical->identity.key() &&
                            imported.identity.fullyQualifiedName() == enumeration->name &&
                            compatibleUsr(enumeration->clangUsr, imported.clangUsr) &&
                            imported.enumBase->token == enumeration->base.token) return true;
                const auto* declaration = typeForOrdinal(nativeCandidates.front());
                if (declaration && declaration->projectOrdinal == selected->projectOrdinal &&
                    declaration->identity.fullyQualifiedName() == enumeration->name && declaration->parts.size() == 1)
                    for (const auto& imported : cppImportedTypes_)
                        if (imported.projectOrdinal == selected->projectOrdinal && imported.enumBase &&
                            imported.identity.key() == declaration->identity.key() &&
                            compatibleUsr(enumeration->clangUsr, imported.clangUsr) &&
                            imported.sourcePath == declaration->parts.front().sourcePath &&
                            imported.enumBase->token == enumeration->base.token) return true;
            }
            return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                          "C/C++ type alias conflicts with a native type '" + syntax->baseName() + "'");
        }
        if (enumeration && selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            const CppImportedTypeDeclaration* target = nullptr;
            for (const auto& imported : cppImportedTypes_)
                if (imported.projectOrdinal == selected->projectOrdinal && imported.enumBase &&
                    compatibleUsr(enumeration->clangUsr, imported.clangUsr) &&
                    imported.identity.fullyQualifiedName() == enumeration->name &&
                    imported.enumBase->token == enumeration->base.token)
                {
                    if (target) return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                                              "ambiguous imported enum for alias '" + syntax->baseName() + "'");
                    target = &imported;
                }
            if (!target || (volatileAliasPointee &&
                (!syntax->modifiers().empty() || syntax->isPointeeConst() || syntax->isPointeeVolatile())) ||
                (enumeration->pointeeConst && !syntax->modifiers().empty()) ||
                ((syntax->isPointeeConst() || syntax->isPointeeVolatile()) && enumeration->pointerDepth != 0))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C/C++ enum alias cannot yet be resolved safely: '" + syntax->baseName() + "'");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(lexer::TokenKind::Identifier);
            mutableSyntax->setBaseName(enumeration->name);
            mutableSyntax->setOriginProjectOrdinal(selected->projectOrdinal);
            mutableSyntax->setOriginNamespace("");
            mutableSyntax->prependModifiers(cppPointerModifiers(selected->declaration.underlyingType));
            mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || enumeration->pointeeConst);
            mutableSyntax->setPointeeVolatile(mutableSyntax->isPointeeVolatile() || volatileAliasPointee);
            return true;
        }
        if (record && selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            const CppImportedTypeDeclaration* target = nullptr;
            for (const auto& imported : cppImportedTypes_)
                if (imported.projectOrdinal == selected->projectOrdinal &&
                    compatibleUsr(record->clangUsr, imported.clangUsr) &&
                    imported.identity.fullyQualifiedName() == record->canonicalName)
                {
                    if (target) return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                                              "ambiguous imported record for alias '" + syntax->baseName() + "'");
                    target = &imported;
                }
            if (!target || (volatileAliasPointee &&
                (!syntax->modifiers().empty() || syntax->isPointeeConst() || syntax->isPointeeVolatile())) ||
                (record->pointeeConst && !syntax->modifiers().empty()) ||
                ((syntax->isPointeeConst() || syntax->isPointeeVolatile()) && record->pointerDepth != 0))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C/C++ record alias cannot yet be resolved safely: '" + syntax->baseName() + "'");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(lexer::TokenKind::Identifier);
            mutableSyntax->setBaseName(record->name);
            mutableSyntax->setOriginProjectOrdinal(selected->projectOrdinal);
            mutableSyntax->setOriginNamespace("");
            mutableSyntax->prependModifiers(cppPointerModifiers(selected->declaration.underlyingType));
            mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || record->pointeeConst);
            mutableSyntax->setPointeeVolatile(mutableSyntax->isPointeeVolatile() || volatileAliasPointee);
            return true;
        }
        const auto& incompleteArray = selected->declaration.underlyingType;
        if (selected->targetTriple == "x86_64-pc-windows-msvc" &&
            incompleteArray.kind == interop::CppTypeKind::Array && incompleteArray.isIncompleteArray)
        {
            const interop::CppType* element = &incompleteArray;
            std::vector<std::optional<std::uint64_t>> dimensions;
            while (element && element->kind == interop::CppTypeKind::Array)
            {
                if (element->isConst || element->isVolatile || !element->element ||
                    (!element->isIncompleteArray && (!element->arrayCount || *element->arrayCount == 0)) ||
                    (element->isIncompleteArray && !dimensions.empty()))
                    return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C incomplete array alias has an unsupported bound or qualifier");
                dimensions.push_back(element->isIncompleteArray ? std::nullopt : element->arrayCount);
                element = element->element.get();
            }
            const auto builtin = element ? cppAliasBuiltinType(*element) : std::nullopt;
            const auto enumeration = element ? cppAliasEnumType(*element) : std::nullopt;
            const auto record = element ? cppAliasRecordType(*element) : std::nullopt;
            if ((!builtin && !enumeration && !record) || syntax->isPointeeConst() ||
                (builtin && builtin->pointeeConst) || (enumeration && enumeration->pointeeConst) ||
                (record && record->pointeeConst))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C incomplete array element cannot be represented safely");
            const std::string name = builtin ? std::string(builtin->name) : enumeration ? enumeration->name : record->name;
            if (!builtin)
            {
                const std::string identity = enumeration ? enumeration->name : record->canonicalName;
                const auto found = std::find_if(cppImportedTypes_.begin(), cppImportedTypes_.end(), [&](const auto& item) {
                    return item.projectOrdinal == selected->projectOrdinal && item.identity.fullyQualifiedName() == identity &&
                        compatibleUsr(enumeration ? enumeration->clangUsr : record->clangUsr, item.clangUsr) &&
                        item.deferredImportError.empty() && (enumeration ? item.enumBase && item.enumBase->token == enumeration->base.token
                                                                              : item.kind == NamedTypeDeclarationKind::Struct);
                });
                if (found == cppImportedTypes_.end())
                    return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C incomplete array element type is unavailable");
            }
            auto& arena = const_cast<ProjectSyntaxUnit&>(unit).arena();
            auto modifiers = cppPointerModifiers(*element);
            for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
            {
                ast::TypeModifierSyntax modifier;
                modifier.range = syntax->range();
                if (!*dimension) modifier.kind = ast::TypeModifierSyntaxKind::IncompleteArray;
                else
                {
                    auto* count = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, syntax->range());
                    count->setTokenKind(lexer::TokenKind::IntegerLiteral);
                    count->setText(std::to_string(**dimension));
                    count->setIntegerValue(**dimension);
                    modifier.kind = ast::TypeModifierSyntaxKind::Array;
                    modifier.arrayCount = count;
                }
                modifiers.push_back(modifier);
            }
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(builtin ? builtin->token : lexer::TokenKind::Identifier);
            mutableSyntax->setBaseName(name);
            if (!builtin)
            {
                mutableSyntax->setOriginProjectOrdinal(selected->projectOrdinal);
                mutableSyntax->setOriginNamespace("");
            }
            mutableSyntax->prependModifiers(modifiers);
            return true;
        }
        const auto array = cppAliasArrayType(selected->declaration.underlyingType);
        if (array && selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            if ((array->element.pointeeConst && !syntax->modifiers().empty()) ||
                (syntax->isPointeeConst() && array->element.pointerDepth != 0))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C/C++ array alias has qualifiers that cannot yet be represented: '" +
                              syntax->baseName() + "'");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            auto& arena = const_cast<ProjectSyntaxUnit&>(unit).arena();
            std::vector<ast::TypeModifierSyntax> modifiers = cppPointerModifiers(selected->declaration.underlyingType);
            for (auto dimension = array->dimensions.rbegin(); dimension != array->dimensions.rend(); ++dimension)
            {
                auto* count = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, syntax->range());
                count->setTokenKind(lexer::TokenKind::IntegerLiteral);
                count->setText(std::to_string(*dimension));
                count->setIntegerValue(*dimension);
                ast::TypeModifierSyntax modifier;
                modifier.kind = ast::TypeModifierSyntaxKind::Array;
                modifier.arrayCount = count;
                modifier.range = syntax->range();
                modifiers.push_back(modifier);
            }
            mutableSyntax->setBaseTokenKind(array->element.token);
            mutableSyntax->setBaseName(std::string(array->element.name));
            mutableSyntax->prependModifiers(modifiers);
            mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || array->element.pointeeConst);
            return true;
        }
        const auto& arrayUnderlying = selected->declaration.underlyingType;
        if (arrayUnderlying.kind == interop::CppTypeKind::Array &&
            selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            const interop::CppType* element = &arrayUnderlying;
            std::vector<std::uint64_t> dimensions;
            while (element && element->kind == interop::CppTypeKind::Array)
            {
                if (element->isConst || element->isVolatile || !element->arrayCount ||
                    *element->arrayCount == 0 || !element->element)
                    return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C array alias has an unsupported bound or qualifier");
                dimensions.push_back(*element->arrayCount);
                element = element->element.get();
            }
            const auto enumElement = element ? cppAliasEnumType(*element) : std::nullopt;
            const auto recordElement = element ? cppAliasRecordType(*element) : std::nullopt;
            const std::string name = enumElement ? enumElement->name : recordElement ? recordElement->name : std::string{};
            const std::string identity = enumElement ? enumElement->name : recordElement ? recordElement->canonicalName : std::string{};
            if (name.empty() || (enumElement ? enumElement->pointeeConst : recordElement->pointeeConst) ||
                syntax->isPointeeConst())
                return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C array alias element cannot be represented safely");
            const auto imported = std::find_if(cppImportedTypes_.begin(), cppImportedTypes_.end(), [&](const auto& item) {
                return item.projectOrdinal == selected->projectOrdinal && item.identity.fullyQualifiedName() == identity &&
                    compatibleUsr(enumElement ? enumElement->clangUsr : recordElement->clangUsr, item.clangUsr) &&
                    item.deferredImportError.empty() && (enumElement ? item.enumBase && item.enumBase->token == enumElement->base.token
                                                                         : item.kind == NamedTypeDeclarationKind::Struct);
            });
            if (imported == cppImportedTypes_.end())
                return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C array alias element type is unavailable");
            auto& arena = const_cast<ProjectSyntaxUnit&>(unit).arena();
            std::vector<ast::TypeModifierSyntax> modifiers = cppPointerModifiers(arrayUnderlying);
            for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
            {
                auto* count = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, syntax->range());
                count->setTokenKind(lexer::TokenKind::IntegerLiteral);
                count->setText(std::to_string(*dimension));
                count->setIntegerValue(*dimension);
                ast::TypeModifierSyntax modifier;
                modifier.kind = ast::TypeModifierSyntaxKind::Array;
                modifier.arrayCount = count;
                modifier.range = syntax->range();
                modifiers.push_back(modifier);
            }
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(lexer::TokenKind::Identifier);
            mutableSyntax->setBaseName(name);
            mutableSyntax->setOriginProjectOrdinal(selected->projectOrdinal);
            mutableSyntax->setOriginNamespace("");
            mutableSyntax->prependModifiers(modifiers);
            return true;
        }
        const auto function = cppAliasFunctionType(selected->declaration.underlyingType);
        if (function && selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            if (syntax->isPointeeConst() ||
                !std::all_of(syntax->modifiers().begin(), syntax->modifiers().end(), [](const auto& modifier) {
                    return modifier.kind == ast::TypeModifierSyntaxKind::Pointer;
                }))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C function pointer alias has unsupported use-site modifiers: '" +
                              syntax->baseName() + "'");
            auto& arena = const_cast<ProjectSyntaxUnit&>(unit).arena();
            const auto& original = *selected->declaration.underlyingType.element->function;
            const auto makeType = [&](const CppAliasBuiltinType& type, const interop::CppType& source) {
                auto* result = arena.make<ast::TypeSyntax>(syntax->range(), type.token, std::string(type.name));
                result->prependModifiers(cppPointerModifiers(source));
                result->setPointeeConst(type.pointeeConst);
                return result;
            };
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(lexer::TokenKind::Invalid);
            mutableSyntax->setBaseName("$function");
            mutableSyntax->setFunction();
            mutableSyntax->setFunctionCarrier(ast::FunctionTypeCarrierSyntaxKind::Pointer);
            mutableSyntax->setFunctionCallingConvention(function->callingConvention);
            mutableSyntax->setFunctionReturnType(makeType(function->result, original.returnType));
            mutableSyntax->setFunctionVariadic(function->variadic);
            for (std::size_t i = 0; i < function->parameters.size(); ++i)
                mutableSyntax->addTypeArgument(makeType(function->parameters[i], original.parameterTypes[i]));
            return true;
        }
        const auto& underlying = selected->declaration.underlyingType;
        if (selected->targetTriple == "x86_64-pc-windows-msvc" &&
            underlying.kind == interop::CppTypeKind::Pointer && underlying.sizeBits == 64 &&
            !underlying.isConst && !underlying.isVolatile && underlying.element &&
            underlying.element->kind == interop::CppTypeKind::Function && underlying.element->function &&
            underlying.element->function->hasPrototype && !syntax->isPointeeConst() &&
            std::all_of(syntax->modifiers().begin(), syntax->modifiers().end(), [](const auto& modifier) {
                return modifier.kind == ast::TypeModifierSyntaxKind::Pointer;
            }))
        {
            const auto& signature = *underlying.element->function;
            auto convention = ast::FunctionCallingConventionSyntaxKind::NativeSharp;
            if (signature.callingConvention == "cdecl") convention = ast::FunctionCallingConventionSyntaxKind::C;
            else if (signature.callingConvention == "stdcall") convention = ast::FunctionCallingConventionSyntaxKind::StdCall;
            else if (signature.callingConvention == "fastcall") convention = ast::FunctionCallingConventionSyntaxKind::FastCall;
            else if (signature.callingConvention == "vectorcall") convention = ast::FunctionCallingConventionSyntaxKind::VectorCall;
            else if (signature.callingConvention == "ms_abi") convention = ast::FunctionCallingConventionSyntaxKind::Win64;
            else return report(diagnostics::DiagnosticId::InvalidUsingTarget, "unsupported C callback calling convention");
            auto& arena = const_cast<ProjectSyntaxUnit&>(unit).arena();
            const auto makeType = [&](const auto& self, const interop::CppType& type,
                                      std::size_t depth) -> ast::TypeSyntax* {
                if (depth >= 64) return nullptr;
                if (type.kind == interop::CppTypeKind::Pointer && type.sizeBits == 64 &&
                    !type.isConst && !type.isVolatile && type.element &&
                    type.element->kind == interop::CppTypeKind::Function && type.element->function &&
                    type.element->function->hasPrototype)
                {
                    const auto& nested = *type.element->function;
                    auto nestedConvention = ast::FunctionCallingConventionSyntaxKind::NativeSharp;
                    if (nested.callingConvention == "cdecl") nestedConvention = ast::FunctionCallingConventionSyntaxKind::C;
                    else if (nested.callingConvention == "stdcall") nestedConvention = ast::FunctionCallingConventionSyntaxKind::StdCall;
                    else if (nested.callingConvention == "fastcall") nestedConvention = ast::FunctionCallingConventionSyntaxKind::FastCall;
                    else if (nested.callingConvention == "vectorcall") nestedConvention = ast::FunctionCallingConventionSyntaxKind::VectorCall;
                    else if (nested.callingConvention == "ms_abi") nestedConvention = ast::FunctionCallingConventionSyntaxKind::Win64;
                    else return nullptr;
                    auto* returnType = self(self, nested.returnType, depth + 1);
                    if (!returnType) return nullptr;
                    std::vector<ast::TypeSyntax*> arguments;
                    for (const auto& parameter : nested.parameterTypes)
                    {
                        auto* argument = self(self, parameter, depth + 1);
                        if (!argument) return nullptr;
                        arguments.push_back(argument);
                    }
                    auto* value = arena.make<ast::TypeSyntax>(syntax->range(), lexer::TokenKind::Invalid, "$function");
                    value->setFunction();
                    value->setFunctionCarrier(ast::FunctionTypeCarrierSyntaxKind::Pointer);
                    value->setFunctionCallingConvention(nestedConvention);
                    value->setFunctionReturnType(returnType);
                    value->setFunctionVariadic(nested.variadic);
                    for (auto* argument : arguments) value->addTypeArgument(argument);
                    return value;
                }
                if (const auto builtin = cppAliasBuiltinType(type))
                {
                    auto* value = arena.make<ast::TypeSyntax>(syntax->range(), builtin->token, std::string(builtin->name));
                    value->prependModifiers(cppPointerModifiers(type));
                    value->setPointeeConst(builtin->pointeeConst);
                    return value;
                }
                const auto enumType = cppAliasEnumType(type);
                const auto recordType = cppAliasRecordType(type);
                const std::string name = enumType ? enumType->name : recordType ? recordType->name : std::string{};
                const std::string canonical = enumType ? enumType->name : recordType ? recordType->canonicalName : std::string{};
                if (name.empty()) return nullptr;
                const auto found = std::find_if(cppImportedTypes_.begin(), cppImportedTypes_.end(), [&](const auto& item) {
                    return item.projectOrdinal == selected->projectOrdinal && item.identity.fullyQualifiedName() == canonical &&
                        compatibleUsr(enumType ? enumType->clangUsr : recordType->clangUsr, item.clangUsr) &&
                        item.deferredImportError.empty() && (enumType ? item.enumBase && item.enumBase->token == enumType->base.token
                                                                     : item.kind == NamedTypeDeclarationKind::Struct);
                });
                if (found == cppImportedTypes_.end()) return nullptr;
                auto* value = arena.make<ast::TypeSyntax>(syntax->range(), lexer::TokenKind::Identifier, name);
                value->setOriginProjectOrdinal(selected->projectOrdinal);
                value->setOriginNamespace("");
                value->prependModifiers(cppPointerModifiers(type));
                value->setPointeeConst(enumType ? enumType->pointeeConst : recordType->pointeeConst);
                return value;
            };
            auto* result = makeType(makeType, signature.returnType, 0);
            std::vector<ast::TypeSyntax*> parameters;
            for (const auto& parameter : signature.parameterTypes) parameters.push_back(makeType(makeType, parameter, 0));
            if (!result || std::find(parameters.begin(), parameters.end(), nullptr) != parameters.end())
                return report(diagnostics::DiagnosticId::InvalidUsingTarget, "C callback signature contains an unavailable type");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(lexer::TokenKind::Invalid);
            mutableSyntax->setBaseName("$function");
            mutableSyntax->setFunction();
            mutableSyntax->setFunctionCarrier(ast::FunctionTypeCarrierSyntaxKind::Pointer);
            mutableSyntax->setFunctionCallingConvention(convention);
            mutableSyntax->setFunctionReturnType(result);
            mutableSyntax->setFunctionVariadic(signature.variadic);
            for (auto* parameter : parameters) mutableSyntax->addTypeArgument(parameter);
            return true;
        }
        const auto& aliasType = selected->declaration.underlyingType;
        if (selected->targetTriple == "x86_64-pc-windows-msvc" &&
            aliasType.kind == interop::CppTypeKind::Pointer && aliasType.element &&
            aliasType.element->isVolatile && aliasType.element->kind != interop::CppTypeKind::Pointer)
        {
            auto unqualifiedElement = std::make_shared<interop::CppType>(*aliasType.element);
            unqualifiedElement->isVolatile = false;
            interop::CppType normalized = aliasType;
            normalized.element = std::move(unqualifiedElement);
            const auto volatileAlias = cppAliasBuiltinType(normalized);
            if (!volatileAlias || volatileAlias->pointerDepth != 1 || !syntax->modifiers().empty() ||
                syntax->isPointeeConst() || syntax->isPointeeVolatile())
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C/C++ volatile pointer alias cannot yet be represented safely: '" +
                              syntax->baseName() + "'");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(volatileAlias->token);
            mutableSyntax->setBaseName(std::string(volatileAlias->name));
            mutableSyntax->prependModifiers(cppPointerModifiers(aliasType));
            mutableSyntax->setPointeeConst(volatileAlias->pointeeConst);
            mutableSyntax->setPointeeVolatile();
            return true;
        }
        const auto mapped = cppAliasBuiltinType(aliasType);
        if (selected->targetTriple != "x86_64-pc-windows-msvc" || !mapped ||
            (mapped->pointeeConst && !syntax->modifiers().empty()) ||
            (syntax->isPointeeConst() && mapped->pointerDepth != 0))
            return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                          "C/C++ type alias cannot yet be represented without losing type information: '" +
                          syntax->baseName() + "'");
        auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
        mutableSyntax->setBaseTokenKind(mapped->token);
        mutableSyntax->setBaseName(std::string(mapped->name));
        mutableSyntax->prependModifiers(cppPointerModifiers(selected->declaration.underlyingType));
        mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || mapped->pointeeConst);
        return true;
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

std::optional<std::string> ProjectNameLookup::resolveNamespaceTarget(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const std::string& target) const
{
    return resolveQualifiedNamespaceTarget(namespaceSegments, target, [&](const std::string& candidate) {
        return namespaceExistsVisible(projectOrdinal, candidate);
    });
}

}
