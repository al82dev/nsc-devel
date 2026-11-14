#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"
#include "nativesharp/Interop/CppSysVRecordAbiValidation.h"
#include "nativesharp/Interop/CppSemanticEnumType.h"
#include "nativesharp/Frontend/CppAliasArrayType.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Frontend/QualifiedNameLadder.h"
#include "../Internal/CppCRecordMetadataCodec.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::interop
{
bool reconstructCppPrecompiledCAlias(const metadata_detail::CDependencyAliasSnapshot& alias,
    const compilation::ProjectGraph& graph, std::size_t projectOrdinal, std::size_t referenceIndex,
    std::string_view targetTriple, frontend::CppImportedTypeAlias& output, std::string& error)
{
    if (targetTriple != "x86_64-pc-windows-msvc" || projectOrdinal >= graph.projects().size() ||
        alias.moduleName.empty() || graph.projects()[projectOrdinal].moduleName() != alias.moduleName ||
        alias.referenceIdentity.empty() || alias.declaration.name.empty() ||
        alias.declaration.qualifiedName.empty() || alias.declaration.insideAnonymousNamespace ||
        !alias.declaration.clangUsr || alias.declaration.clangUsr->empty())
    {
        error = "precompiled C alias has invalid owner, target or identity";
        return false;
    }
    frontend::CppImportedTypeAlias value{projectOrdinal, graph.projects()[projectOrdinal].projectFile(),
        referenceIndex, std::string(targetTriple), alias.declaration, {}};
    value.declaration.location.reset();
    output = std::move(value);
    error.clear();
    return true;
}

std::vector<CppPublishedTemplateAliasCandidates> selectCppPublishedTemplateAliases(
    const frontend::ProjectCompilation& compilation, const LibraryMetadata& metadata,
    std::span<const CppSemanticTypeAliasEntry> inventory)
{
    const auto* model = compilation.genericModel();
    const auto* lookup = compilation.nameLookup();
    if (!model || !lookup) return {};
    std::set<std::string> published;
    for (const auto& item : metadata.genericTemplates) published.insert(item.canonicalKey);
    std::vector<CppPublishedTemplateAliasCandidates> result;
    std::unordered_map<const ast::TypeSyntax*, const frontend::TypeNameBinding*> bindings;
    for (const auto& binding : lookup->bindings()) bindings.emplace(binding.syntax, &binding);
    for (const auto& record : model->templates())
    {
        if (!published.contains(record.canonicalKey)) continue;
        for (std::size_t index = 0; index < record.foreignReferenceCandidates.size(); ++index)
        {
            const auto& candidate = record.foreignReferenceCandidates[index];
            if (!candidate.typeSyntax || candidate.writtenTypeName.empty()) continue;
            const auto owner = candidate.typeSyntax->originProjectOrdinal().value_or(record.projectOrdinal);
            if (owner >= lookup->modules().size() || !lookup->modules()[owner]) continue;
            const auto visible = lookup->modules()[owner]->visibleProjectOrdinals();
            const auto& lexicalNamespace = candidate.typeSyntax->originNamespace().value_or(candidate.lexicalNamespace);
            std::vector<std::string> segments;
            std::size_t start = 0;
            while (start < lexicalNamespace.size())
            {
                const auto end = lexicalNamespace.find('.', start);
                segments.push_back(lexicalNamespace.substr(start, end - start));
                if (end == std::string::npos) break;
                start = end + 1;
            }
            frontend::QualifiedNameSite site;
            site.namespaceSegments = segments;
            site.importedNamespaces = candidate.importedNamespaces;
            site.aliases = &candidate.aliases;
            std::set<std::string> spellings;
            for (const auto& rung : frontend::qualifiedNameLadder(site, candidate.writtenTypeName))
                spellings.insert(rung.spellings.begin(), rung.spellings.end());
            CppPublishedTemplateAliasCandidates found{record.canonicalKey, index, {}};
            if (const auto binding = bindings.find(candidate.typeSyntax); binding != bindings.end())
            {
                found.hasNameBinding = true;
                found.resolvedAlias = binding->second->expandedCppAlias;
            }
            for (std::size_t i = 0; i < inventory.size(); ++i)
            {
                const auto& alias = inventory[i];
                if (alias.declaration.insideAnonymousNamespace ||
                    (alias.projectOrdinal != owner &&
                     std::find(visible.begin(), visible.end(), alias.projectOrdinal) == visible.end())) continue;
                auto qualified = alias.declaration.qualifiedName;
                std::size_t pos = 0;
                while ((pos = qualified.find("::", pos)) != std::string::npos) qualified.replace(pos, 2, ".");
                // Keep possible enclosing-type matches for later semantic filtering: the
                // candidate currently does not retain every enclosing type spelling.
                if (spellings.contains(qualified) ||
                    qualified.ends_with("." + candidate.writtenTypeName)) found.inventoryIndices.push_back(i);
            }
            if (!found.inventoryIndices.empty()) result.push_back(std::move(found));
        }
    }
    return result;
}

bool captureCppTemplateAliasCandidates(const compilation::ProjectGraph& graph,
    std::span<const CppPublishedTemplateAliasCandidates> candidates,
    std::span<const CppSemanticTypeAliasEntry> inventory,
    std::vector<metadata_detail::CDependencyAliasSnapshot>& output, std::string& error)
{
    std::vector<metadata_detail::CDependencyAliasSnapshot> result;
    std::set<std::tuple<std::size_t, std::size_t, std::string>> seen;
    for (const auto& candidate : candidates)
    {
        std::vector<frontend::CppImportedTypeAlias> pending;
        if (candidate.hasNameBinding)
        {
            if (candidate.resolvedAlias) pending.push_back(*candidate.resolvedAlias);
        }
        else for (const auto index : candidate.inventoryIndices)
        {
            if (index >= inventory.size()) { error = "C alias candidate index is invalid"; return false; }
            const auto& entry = inventory[index];
            pending.push_back({entry.projectOrdinal, entry.projectFile, entry.referenceIndex,
                               entry.targetTriple, entry.declaration, {}});
        }
        for (const auto& alias : pending)
        {
            if (alias.projectOrdinal >= graph.projects().size())
            { error = "resolved C alias has no project owner"; return false; }
            const auto& project = graph.projects()[alias.projectOrdinal];
            if (alias.referenceIndex >= project.cppLibraryReferences().size())
            { error = "resolved C alias has no source reference"; return false; }
            const auto language = project.cppLibraryReferences()[alias.referenceIndex].language;
            if (language != CppLanguageMode::C)
            {
                const auto& type = alias.declaration.underlyingType;
                if (language != CppLanguageMode::Cpp || type.kind != CppTypeKind::Pointer ||
                    !type.element || type.element->kind != CppTypeKind::Function || !type.element->function)
                    continue;
            }
            const auto identity = std::make_tuple(alias.projectOrdinal, alias.referenceIndex,
                                                   alias.declaration.qualifiedName);
            if (!seen.insert(identity).second) continue;
            metadata_detail::CDependencyAliasSnapshot snapshot;
            const auto reference = "cpp-reference:" + std::to_string(alias.referenceIndex);
            if (!metadata_detail::captureCDependencyAlias(inventory, alias, project.moduleName(),
                                                           reference, snapshot, error)) return false;
            result.push_back(std::move(snapshot));
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}
namespace
{
std::optional<CppScalarTypeMapping> mapGlobalType(const CppType& type, std::size_t projectOrdinal,
    std::string_view moduleName, const frontend::ProjectNameLookup* lookup, bool cLanguage)
{
    if (cLanguage)
        if (auto callback = projectCppStoredFunctionPointer(type, projectOrdinal, moduleName, lookup)) return callback;
    if (cLanguage && type.kind == CppTypeKind::Array && type.isIncompleteArray)
    {
        // `extern T name[];` has no storage of known size: project it as the zero-storage T[*] already used
        // for C flexible array members, so indexing and decay keep C's meaning without inventing a bound.
        // `extern T name[];` no tiene almacenamiento de tamaño conocido: se proyecta como el T[*] sin
        // almacenamiento de los miembros flexibles C; indexar y decaer conservan el significado de C.
        const CppType* element = type.element.get();
        if (!element || element->kind == CppTypeKind::Array || element->kind == CppTypeKind::Void ||
            element->kind == CppTypeKind::Atomic || !element->sizeBits || *element->sizeBits == 0 ||
            *element->sizeBits % 8 != 0) return std::nullopt;
        if (element->kind == CppTypeKind::Pointer &&
            (!element->element || element->element->isConst || element->element->isVolatile))
            return std::nullopt;
        CppType unqualified = *element;
        unqualified.isConst = false;
        unqualified.isVolatile = false;
        auto mapped = mapGlobalType(unqualified, projectOrdinal, moduleName, lookup, cLanguage);
        if (!mapped || mapped->isConst || mapped->isVolatile ||
            mapped->nativeSharpTypeKey.starts_with("fn:")) return std::nullopt;
        mapped->nativeSharpTypeKey += "[*]";
        mapped->canonicalCppType = type.canonicalSpelling;
        mapped->isConst = type.isConst || element->isConst;
        mapped->isVolatile = type.isVolatile || element->isVolatile;
        return mapped;
    }
    if (type.kind == CppTypeKind::Array)
    {
        const CppType* base = &type;
        std::vector<std::uint64_t> fixedDimensions;
        bool readonly = false;
        bool isVolatile = false;
        while (base && base->kind == CppTypeKind::Array)
        {
            if (!base->arrayCount || *base->arrayCount == 0 ||
                !base->element || fixedDimensions.size() >= 64) return std::nullopt;
            readonly |= base->isConst;
            isVolatile |= base->isVolatile;
            fixedDimensions.push_back(*base->arrayCount);
            base = base->element.get();
        }
        if (base && (base->kind != CppTypeKind::Pointer ||
                     (cLanguage && base->isConst && !base->isVolatile)) &&
            (readonly || isVolatile || base->isConst || base->isVolatile))
        {
            readonly |= base->isConst;
            isVolatile |= base->isVolatile;
            CppType unqualified = *base;
            unqualified.isConst = false;
            unqualified.isVolatile = false;
            auto mapped = mapGlobalType(unqualified, projectOrdinal, moduleName, lookup, cLanguage);
            if (!mapped || mapped->isVolatile) return std::nullopt;
            for (auto dimension = fixedDimensions.rbegin(); dimension != fixedDimensions.rend(); ++dimension)
                mapped->nativeSharpTypeKey += "[" + std::to_string(*dimension) + "]";
            mapped->canonicalCppType = type.canonicalSpelling;
            mapped->isConst = readonly;
            mapped->isVolatile = isVolatile;
            return mapped;
        }
        const auto array = frontend::cppAliasArrayType(type);
        if (!array)
        {
            const CppType* element = &type;
            std::vector<std::uint64_t> dimensions;
            while (element && element->kind == CppTypeKind::Array)
            {
                if (element->isConst || element->isVolatile || !element->arrayCount ||
                    *element->arrayCount == 0 || !element->element || dimensions.size() >= 64)
                    return std::nullopt;
                dimensions.push_back(*element->arrayCount);
                element = element->element.get();
            }
            const bool volatilePointee = cLanguage && element && element->kind == CppTypeKind::Pointer &&
                element->element && element->element->isVolatile;
            const bool callbackElement = cLanguage && element &&
                projectCppStoredFunctionPointer(*element, projectOrdinal, moduleName, lookup).has_value();
            if (!element || (element->kind != CppTypeKind::Enumeration &&
                             element->kind != CppTypeKind::Record && !volatilePointee && !callbackElement) ||
                element->isConst || element->isVolatile)
                return std::nullopt;
            auto mapped = mapGlobalType(*element, projectOrdinal, moduleName, lookup, cLanguage);
            if (!mapped) return std::nullopt;
            for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
                mapped->nativeSharpTypeKey += "[" + std::to_string(*dimension) + "]";
            mapped->canonicalCppType = type.canonicalSpelling;
            return mapped;
        }
        if (array->element.pointeeConst &&
            (!cLanguage || array->element.pointerDepth != 1))
            return std::nullopt;
        const CppType* element = &type;
        while (element && element->kind == CppTypeKind::Array) element = element->element.get();
        if (!element) return std::nullopt;
        auto mapped = mapGlobalType(*element, projectOrdinal, moduleName, lookup, cLanguage);
        if (!mapped || mapped->isConst || mapped->isVolatile) return std::nullopt;
        for (auto dimension = array->dimensions.rbegin(); dimension != array->dimensions.rend(); ++dimension)
            mapped->nativeSharpTypeKey += "[" + std::to_string(*dimension) + "]";
        mapped->canonicalCppType = type.canonicalSpelling;
        return mapped;
    }
    if (type.kind != CppTypeKind::Pointer)
    {
        if (type.kind == CppTypeKind::Record)
        {
            if (!cLanguage || !lookup) return std::nullopt;
            const auto identity = projectCppRecordTypeIdentity(type, moduleName);
            const auto ordinal = identity ? lookup->typeOrdinalForIdentity(identity->key()) : std::nullopt;
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            // EN: T08-D26: a reconciled record names the canonical one of another producer.
            // ES: T08-D26: un record conciliado nombra el canonico de otro productor.
            const bool reconciled = identity && lookup->isReconciledCppTypeIdentity(identity->key());
            if (!identity || !imported || (!reconciled && imported->projectOrdinal != projectOrdinal) ||
                (type.recordIdentity && type.recordIdentity->clangUsr && imported->clangUsr &&
                 type.recordIdentity->clangUsr != imported->clangUsr) ||
                !imported->deferredImportError.empty() || imported->isIncomplete || !imported->objectLayout ||
                !type.sizeBits || *type.sizeBits % 8 != 0 ||
                *type.sizeBits / 8 != imported->objectLayout->sizeBytes || !type.alignmentBits ||
                *type.alignmentBits % 8 != 0 ||
                *type.alignmentBits / 8 != imported->objectLayout->alignmentBytes) return std::nullopt;
            return CppScalarTypeMapping{"type:" + imported->identity.key(), type.canonicalSpelling, type.isConst,
                                        type.isVolatile};
        }
        if (type.kind == CppTypeKind::Enumeration)
        {
            // Top-level volatile applies to the global access; enum identity/layout are unqualified.
            CppType unqualified = type;
            unqualified.isVolatile = false;
            auto projected = projectCppSemanticEnumType(unqualified, projectOrdinal, moduleName, lookup);
            if (projected) projected->isVolatile = type.isVolatile;
            return projected;
        }
        return mapCppScalarType(type);
    }
    const CppType* current = &type;
    std::size_t depth = 0;
    while (current->kind == CppTypeKind::Pointer)
    {
        if (current->canonicalSpelling.empty() || current->sizeBits != 64 || !current->element)
            return std::nullopt;
        current = current->element.get();
        const bool supportedVolatilePointee = cLanguage && depth == 0 &&
            (current->kind == CppTypeKind::Void ||
             current->kind == CppTypeKind::Boolean ||
             current->kind == CppTypeKind::SignedInteger ||
             current->kind == CppTypeKind::UnsignedInteger ||
             current->kind == CppTypeKind::Floating ||
             (lookup && (current->kind == CppTypeKind::Record ||
                         current->kind == CppTypeKind::Enumeration)));
        if ((current->isVolatile && !supportedVolatilePointee) || (current->isConst &&
            (depth != 0 || current->kind == CppTypeKind::Pointer))) return std::nullopt;
        ++depth;
    }
    if (cLanguage && lookup && current->kind == CppTypeKind::Record)
    {
        const auto identity = projectCppRecordTypeIdentity(*current, moduleName);
        const auto ordinal = identity ? lookup->typeOrdinalForIdentity(identity->key()) : std::nullopt;
        const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
        const bool reconciled = identity && lookup->isReconciledCppTypeIdentity(identity->key());
        if (identity && imported && imported->isIncomplete &&
            (reconciled || imported->projectOrdinal == projectOrdinal) &&
            imported->deferredImportError.empty() &&
            (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
             imported->origin == frontend::CppImportedTypeOrigin::CUnion) &&
            !(current->recordIdentity && current->recordIdentity->clangUsr && imported->clangUsr &&
              current->recordIdentity->clangUsr != imported->clangUsr))
            return CppScalarTypeMapping{"type:" + (reconciled ? imported->identity.key() : identity->key()) +
                                            std::string(depth, '*'),
                                        type.canonicalSpelling, type.isConst, type.isVolatile};
    }
    const auto element = mapGlobalType(*current, projectOrdinal, moduleName, lookup, cLanguage);
    if (!element) return std::nullopt;
    return CppScalarTypeMapping{element->nativeSharpTypeKey + std::string(depth, '*'),
                                type.canonicalSpelling, type.isConst, type.isVolatile};
}
}

static CppSemanticGlobalResult projectGlobalWithLookup(
    const CppGlobalDeclaration& global, std::size_t projectOrdinal, std::string_view moduleName,
    const frontend::ProjectNameLookup* lookup, bool cLanguage = false)
{
    auto failure = [](std::string reason) -> CppSemanticGlobalResult {
        return CppSemanticGlobalFailure{std::move(reason)};
    };
    if (moduleName.empty() || global.name.empty() || global.qualifiedName.empty() ||
        !global.externalLinkage || !global.externalSymbol || global.externalSymbol->empty())
        return failure("global requires an external symbol and a project owner");
    const auto kind = global.type.kind;
    const bool scalarTls = kind == CppTypeKind::Boolean || kind == CppTypeKind::SignedInteger ||
        kind == CppTypeKind::UnsignedInteger || kind == CppTypeKind::Floating;
    const bool aggregateTls = kind == CppTypeKind::Record || kind == CppTypeKind::Enumeration;
    const bool supportedPointerTls = kind == CppTypeKind::Pointer && global.type.element;
    const bool atomicIntTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::SignedInteger &&
        (global.type.element->unqualifiedCanonicalSpelling == "int" ||
         global.type.element->unqualifiedCanonicalSpelling == "long") &&
        global.type.element->sizeBits == std::optional<std::uint64_t>{32} &&
        global.type.sizeBits == std::optional<std::uint64_t>{32} &&
        global.declarationAlignmentBits && *global.declarationAlignmentBits >= 32;
    const bool atomicLongTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::SignedInteger &&
        global.type.element->unqualifiedCanonicalSpelling == "long long" &&
        global.type.element->sizeBits == std::optional<std::uint64_t>{64} &&
        global.type.sizeBits == std::optional<std::uint64_t>{64} &&
        global.declarationAlignmentBits && *global.declarationAlignmentBits >= 64;
    const bool atomicUIntTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::UnsignedInteger &&
        (global.type.element->unqualifiedCanonicalSpelling == "unsigned int" ||
         global.type.element->unqualifiedCanonicalSpelling == "unsigned long") &&
        global.type.element->sizeBits == std::optional<std::uint64_t>{32} &&
        global.type.sizeBits == std::optional<std::uint64_t>{32} &&
        global.declarationAlignmentBits && *global.declarationAlignmentBits >= 32;
    const bool atomicULongTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::UnsignedInteger &&
        global.type.element->unqualifiedCanonicalSpelling == "unsigned long long" &&
        global.type.element->sizeBits == std::optional<std::uint64_t>{64} &&
        global.type.sizeBits == std::optional<std::uint64_t>{64} &&
        global.declarationAlignmentBits && *global.declarationAlignmentBits >= 64;
    const bool atomicNarrowTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element &&
        (global.type.element->kind == CppTypeKind::SignedInteger ||
         global.type.element->kind == CppTypeKind::UnsignedInteger) &&
        (global.type.element->sizeBits == std::optional<std::uint64_t>{8} ||
         global.type.element->sizeBits == std::optional<std::uint64_t>{16}) &&
        global.type.sizeBits == global.type.element->sizeBits && global.declarationAlignmentBits &&
        *global.declarationAlignmentBits >= *global.type.sizeBits;
    const bool atomicFloatTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::Floating &&
        ((global.type.element->unqualifiedCanonicalSpelling == "float" &&
          global.type.element->sizeBits == std::optional<std::uint64_t>{32}) ||
         (global.type.element->unqualifiedCanonicalSpelling == "double" &&
          global.type.element->sizeBits == std::optional<std::uint64_t>{64})) &&
        global.type.sizeBits == global.type.element->sizeBits && global.declarationAlignmentBits &&
        *global.declarationAlignmentBits >= *global.type.sizeBits;
    const bool atomicBoolTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::Boolean &&
        global.type.element->sizeBits == std::optional<std::uint64_t>{8} &&
        global.type.sizeBits == std::optional<std::uint64_t>{8} &&
        global.declarationAlignmentBits && *global.declarationAlignmentBits >= 8;
    const bool atomicPointerTls = cLanguage && kind == CppTypeKind::Atomic &&
        global.type.element && global.type.element->kind == CppTypeKind::Pointer && global.type.element->element &&
        ((global.type.element->element->kind == CppTypeKind::Boolean) ||
         (global.type.element->element->kind == CppTypeKind::Record) ||
         (global.type.element->element->kind == CppTypeKind::Enumeration) ||
         (global.type.element->element->kind == CppTypeKind::Void) ||
         (global.type.element->element->kind == CppTypeKind::SignedInteger &&
          global.type.element->element->unqualifiedCanonicalSpelling == "int") ||
         (global.type.element->element->kind == CppTypeKind::UnsignedInteger &&
          global.type.element->element->unqualifiedCanonicalSpelling == "unsigned int" &&
          global.type.element->element->sizeBits == std::optional<std::uint64_t>{32}) ||
         (global.type.element->element->kind == CppTypeKind::SignedInteger &&
            global.type.element->element->unqualifiedCanonicalSpelling == "short" &&
            global.type.element->element->sizeBits == std::optional<std::uint64_t>{16}) ||
           (global.type.element->element->kind == CppTypeKind::SignedInteger &&
            global.type.element->element->unqualifiedCanonicalSpelling == "long long" &&
            global.type.element->element->sizeBits == std::optional<std::uint64_t>{64}) ||
           (global.type.element->element->kind == CppTypeKind::Floating &&
            global.type.element->element->unqualifiedCanonicalSpelling == "double" &&
            global.type.element->element->sizeBits == std::optional<std::uint64_t>{64}) ||
         (!global.type.element->element->isConst && !global.type.element->element->isVolatile &&
          ((global.type.element->element->kind == CppTypeKind::SignedInteger &&
            global.type.element->element->unqualifiedCanonicalSpelling == "signed char" &&
            global.type.element->element->sizeBits == std::optional<std::uint64_t>{8}) ||
           (global.type.element->element->kind == CppTypeKind::UnsignedInteger &&
            ((global.type.element->element->unqualifiedCanonicalSpelling == "unsigned char" &&
              global.type.element->element->sizeBits == std::optional<std::uint64_t>{8}) ||
             (global.type.element->element->unqualifiedCanonicalSpelling == "unsigned short" &&
              global.type.element->element->sizeBits == std::optional<std::uint64_t>{16}) ||
             (global.type.element->element->unqualifiedCanonicalSpelling == "unsigned long long" &&
              global.type.element->element->sizeBits == std::optional<std::uint64_t>{64}))))) ||
         (global.type.element->element->sizeBits == std::optional<std::uint64_t>{32} &&
          ((global.type.element->element->kind == CppTypeKind::Floating &&
            global.type.element->element->unqualifiedCanonicalSpelling == "float") ||
           (global.type.element->element->kind == CppTypeKind::SignedInteger &&
            global.type.element->element->unqualifiedCanonicalSpelling == "long") ||
           (global.type.element->element->kind == CppTypeKind::UnsignedInteger &&
            global.type.element->element->unqualifiedCanonicalSpelling == "unsigned long")))) &&
        global.type.sizeBits == std::optional<std::uint64_t>{64} && global.declarationAlignmentBits &&
        *global.declarationAlignmentBits >= 64;
    const bool atomicEnumTls = cLanguage && kind == CppTypeKind::Atomic && global.type.element &&
        global.type.element->kind == CppTypeKind::Enumeration && global.type.element->sizeBits &&
        global.type.sizeBits == global.type.element->sizeBits && global.declarationAlignmentBits &&
        (*global.type.sizeBits == 8 || *global.type.sizeBits == 16 ||
         *global.type.sizeBits == 32 || *global.type.sizeBits == 64) &&
        *global.declarationAlignmentBits >= *global.type.sizeBits;
    // EN: T08-D07. `_Atomic` over a C struct or union, also thread-local or volatile. Clang widens an atomic
    //     record of up to 16 bytes to the next power of two and aligns it to that width; larger ones keep their
    //     size and alignment. The width travels as the storage size; accesses copy the whole value.
    // ES: T08-D07. `_Atomic` sobre un struct o union C, tambien thread-local o volatile. Clang amplia un record
    //     atomico de hasta 16 bytes a la siguiente potencia de dos y lo alinea a ese ancho; los mayores conservan
    //     tamano y alineacion. El ancho viaja como tamano de almacenamiento; los accesos copian el valor entero.
    const bool atomicRecord = cLanguage && lookup && kind == CppTypeKind::Atomic && global.type.element &&
        global.type.element->kind == CppTypeKind::Record &&
        !global.type.element->isVolatile && global.type.sizeBits && global.type.element->sizeBits &&
        *global.type.sizeBits >= *global.type.element->sizeBits && *global.type.sizeBits % 8 == 0 &&
        *global.type.sizeBits > 0 && global.declarationAlignmentBits &&
        (*global.type.sizeBits > 128 || *global.declarationAlignmentBits >= *global.type.sizeBits);
    const bool supportedAtomicTls = atomicIntTls || atomicLongTls || atomicUIntTls || atomicULongTls ||
        atomicNarrowTls || atomicFloatTls || atomicBoolTls || atomicPointerTls || atomicEnumTls || atomicRecord;
    const CppType* arrayElement = &global.type;
    std::size_t arrayDepth = 0;
    bool fixedArray = kind == CppTypeKind::Array;
    while (fixedArray && arrayElement->kind == CppTypeKind::Array)
    {
        if (!arrayElement->arrayCount || *arrayElement->arrayCount == 0 || !arrayElement->element ||
            ++arrayDepth > 64)
        { fixedArray = false; break; }
        arrayElement = arrayElement->element.get();
    }
    const bool scalarArrayElement = arrayElement->kind == CppTypeKind::Boolean ||
        arrayElement->kind == CppTypeKind::SignedInteger || arrayElement->kind == CppTypeKind::UnsignedInteger ||
        arrayElement->kind == CppTypeKind::Floating;
    const bool supportedArrayElement = scalarArrayElement || arrayElement->kind == CppTypeKind::Record ||
        arrayElement->kind == CppTypeKind::Enumeration || arrayElement->kind == CppTypeKind::Pointer;
    const bool supportedArrayTls = fixedArray && supportedArrayElement;
    const bool supportedVolatileTls = !global.type.isVolatile || supportedArrayTls || supportedPointerTls || scalarTls || aggregateTls || supportedAtomicTls;
    if (global.threadLocal && (!cLanguage || global.dllImport ||
        (!scalarTls && !aggregateTls && !supportedPointerTls && !supportedArrayTls && !supportedAtomicTls) || !supportedVolatileTls))
        return failure("thread-local C/C++ global is not yet projected");
    auto type = mapGlobalType(supportedAtomicTls ? *global.type.element : global.type,
                              projectOrdinal, moduleName, lookup, cLanguage);
    if (!type || global.type.kind == CppTypeKind::Void)
        return failure("C/C++ global type is not yet mapped");
    if (supportedAtomicTls)
    {
        const auto pointerElement = atomicPointerTls ? global.type.element->element->unqualifiedCanonicalSpelling : "";
        const char* expected = atomicPointerTls ? (pointerElement == "signed char" ? "builtin:sbyte*" : pointerElement == "unsigned char" ? "builtin:byte*" : pointerElement == "unsigned short" ? "builtin:ushort*" : pointerElement == "unsigned long long" ? "builtin:ulong*" : pointerElement == "unsigned int" || pointerElement == "unsigned long" ? "builtin:uint*" : pointerElement == "short" ? "builtin:short*" : pointerElement == "long long" ? "builtin:long*" : pointerElement == "double" ? "builtin:double*" : pointerElement == "float" ? "builtin:float*" : "builtin:int*") : atomicBoolTls ? "builtin:bool" :
            atomicFloatTls ? (global.type.sizeBits == std::optional<std::uint64_t>{32} ? "builtin:float" : "builtin:double") :
            atomicIntTls ? "builtin:int" : atomicLongTls ? "builtin:long" :
            atomicUIntTls ? "builtin:uint" : atomicULongTls ? "builtin:ulong" :
            global.type.element->sizeBits == std::optional<std::uint64_t>{8}
                ? (global.type.element->kind == CppTypeKind::SignedInteger ? "builtin:sbyte" : "builtin:byte")
                : (global.type.element->kind == CppTypeKind::SignedInteger ? "builtin:short" : "builtin:ushort");
        if ((!atomicPointerTls && !atomicEnumTls && !atomicRecord && type->nativeSharpTypeKey != expected) ||
            ((atomicEnumTls || atomicRecord) && !type->nativeSharpTypeKey.starts_with("type:")) ||
            (atomicRecord && type->nativeSharpTypeKey.ends_with("*")) ||
            (atomicPointerTls && !type->nativeSharpTypeKey.ends_with("*")))
            return failure("C atomic integer mapping is inconsistent");
        type->canonicalCppType = global.type.canonicalSpelling;
        type->isConst = global.type.isConst;
        type->isVolatile = global.type.isVolatile;
    }
    std::string cppNamespace;
    if (global.qualifiedName != global.name)
    {
        const auto separator = global.qualifiedName.rfind("::");
        if (separator == std::string::npos || global.qualifiedName.substr(separator + 2) != global.name)
            return failure("C/C++ qualified name does not match its global name");
        cppNamespace = global.qualifiedName.substr(0, separator);
    }
    std::string nsNamespace;
    for (std::size_t i = 0; i < cppNamespace.size(); ++i)
        if (cppNamespace[i] == ':' && i + 1 < cppNamespace.size() && cppNamespace[i + 1] == ':')
        { nsNamespace.push_back('.'); ++i; }
        else nsNamespace.push_back(cppNamespace[i]);
    const CppType* pointerStorage = atomicPointerTls ? global.type.element.get() : &global.type;
    while (cLanguage && pointerStorage->kind == CppTypeKind::Array && pointerStorage->element)
        pointerStorage = pointerStorage->element.get();
    CppSemanticGlobal projected{projectOrdinal, std::string(moduleName), std::move(nsNamespace),
                             global.name, type->nativeSharpTypeKey, type->canonicalCppType,
                             *global.externalSymbol, type->isVolatile, type->isConst,
                             pointerStorage->kind == CppTypeKind::Pointer && pointerStorage->element &&
                                 pointerStorage->element->isConst, global.dllImport,
                             pointerStorage->kind == CppTypeKind::Pointer && pointerStorage->element &&
                                 pointerStorage->element->isVolatile, std::nullopt, global.type.sizeBits,
                             global.declarationAlignmentBits};
    projected.threadLocal = global.threadLocal;
    projected.atomicStorage = supportedAtomicTls;
    return projected;
}

CppSemanticGlobalResult projectCppSemanticGlobal(
    const CppGlobalDeclaration& global, std::size_t projectOrdinal, std::string_view moduleName)
{
    return projectGlobalWithLookup(global, projectOrdinal, moduleName, nullptr);
}

CppSemanticGlobalResult projectCppPrecompiledCGlobal(
    const metadata_detail::CDependencyGlobalSnapshot& global, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup)
{
    const auto& declaration = global.declaration;
    if (targetTriple != "x86_64-pc-windows-msvc" || !lookup.isBuilt() ||
        projectOrdinal >= lookup.modules().size() || !lookup.modules()[projectOrdinal] ||
        lookup.modules()[projectOrdinal]->moduleName() != moduleName || moduleName.empty() ||
        global.moduleName != moduleName || global.referenceIdentity.empty() || global.linkSymbol.empty() ||
        !declaration.clangUsr || declaration.clangUsr->empty() ||
        (declaration.externalLinkage && declaration.externalSymbol != global.linkSymbol))
        return CppSemanticGlobalFailure{"precompiled C global has invalid owner, target or identity"};
    auto projectedDeclaration = declaration;
    projectedDeclaration.externalLinkage = true;
    projectedDeclaration.externalSymbol = global.linkSymbol;
    auto projected = projectGlobalWithLookup(projectedDeclaration, projectOrdinal, moduleName, &lookup, true);
    if (auto* value = std::get_if<CppSemanticGlobal>(&projected))
    {
        value->clangUsr = declaration.clangUsr;
        value->microsoftVolatileSemantics = global.microsoftVolatileSemantics;
        value->dllImport = global.effectiveDllImport;
    }
    return projected;
}

CppSemanticGlobalInventoryResult collectCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticGlobalEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            if (!std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error) || error) continue;
            if (owner) return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner) return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->globalsSnapshot();
            if (!declarations) return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ session is not available"};
            const auto options = session->optionsSnapshot();
            const auto microsoftVolatileSemantics = session->microsoftVolatileSemantics();
            for (auto& declaration : *declarations)
            {
                auto projection = projectGlobalWithLookup(declaration, *owner,
                    graph.projects()[*owner].moduleName(), nullptr, options.language == CppLanguageMode::C);
                result.push_back({group.projectFile, index, *owner, options.targetTriple,
                                  std::move(declaration), std::move(projection),
                                  options.language == CppLanguageMode::C, microsoftVolatileSemantics});
            }
        }
    }
    return result;
}

CppSemanticGlobalSelection selectCppSemanticGlobal(
    std::span<const CppSemanticGlobalEntry> entries, std::size_t projectOrdinal,
    std::string_view qualifiedName)
{
    CppSemanticGlobalSelection result;
    std::vector<std::string_view> symbols;
    std::vector<std::string_view> usrs;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (entry.projectOrdinal != projectOrdinal || entry.declaration.qualifiedName != qualifiedName) continue;
        if (!entry.declaration.externalLinkage)
        {
            result.internalIndices.push_back(i);
            continue;
        }
        result.declarationIndices.push_back(i);
        if (std::holds_alternative<CppSemanticGlobalFailure>(entry.projection))
            result.unsupportedIndices.push_back(i);
        if (entry.declaration.externalSymbol && !entry.declaration.externalSymbol->empty())
            symbols.push_back(*entry.declaration.externalSymbol);
        if (entry.declaration.clangUsr && !entry.declaration.clangUsr->empty())
            usrs.push_back(*entry.declaration.clangUsr);
    }
    std::vector<bool> included(entries.size(), false);
    bool expanded = true;
    while (expanded)
    {
        expanded = false;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            const auto& entry = entries[i];
            if (included[i] || entry.projectOrdinal != projectOrdinal || !entry.declaration.externalLinkage ||
                entry.declaration.qualifiedName == qualifiedName) continue;
            const bool sameSymbol = entry.declaration.externalSymbol &&
                std::find(symbols.begin(), symbols.end(), *entry.declaration.externalSymbol) != symbols.end();
            const bool sameUsr = entry.declaration.clangUsr && !entry.declaration.clangUsr->empty() &&
                std::find(usrs.begin(), usrs.end(), *entry.declaration.clangUsr) != usrs.end();
            if (!sameSymbol && !sameUsr) continue;
            included[i] = true;
            result.aliasIndices.push_back(i);
            if (entry.declaration.externalSymbol && !entry.declaration.externalSymbol->empty())
                symbols.push_back(*entry.declaration.externalSymbol);
            if (entry.declaration.clangUsr && !entry.declaration.clangUsr->empty())
                usrs.push_back(*entry.declaration.clangUsr);
            expanded = true;
        }
    }
    if (result.declarationIndices.empty()) return result;
    const auto& first = entries[result.declarationIndices.front()];
    std::optional<std::string> knownUsr;
    for (const auto i : result.declarationIndices)
        if (const auto& usr = entries[i].declaration.clangUsr; usr && !usr->empty())
        {
            if (knownUsr && *knownUsr != *usr) result.conflict = "conflicting C/C++ global declaration USRs";
            else knownUsr = *usr;
        }
    const auto compatible = [&](const CppSemanticGlobalEntry& entry) {
        const auto& a = first.declaration;
        const auto& b = entry.declaration;
        return first.targetTriple == entry.targetTriple && a.externalSymbol == b.externalSymbol &&
            a.threadLocal == b.threadLocal && a.dllImport == b.dllImport && a.type.kind == b.type.kind &&
            a.type.canonicalSpelling == b.type.canonicalSpelling &&
            a.type.unqualifiedCanonicalSpelling == b.type.unqualifiedCanonicalSpelling &&
            a.type.ieeeBinary64 == b.type.ieeeBinary64 &&
            a.type.isConst == b.type.isConst && a.type.isVolatile == b.type.isVolatile &&
            a.type.sizeBits == b.type.sizeBits && a.type.alignmentBits == b.type.alignmentBits;
    };
    for (const auto i : result.declarationIndices)
        if (!compatible(entries[i]))
            result.conflict = "incompatible C/C++ global declarations";
    for (const auto i : result.aliasIndices)
        if (!compatible(entries[i]))
            result.conflict = "incompatible C/C++ global symbol alias";
    if (result.conflict.empty() && result.unsupportedIndices.empty())
        if (const auto* candidate = std::get_if<CppSemanticGlobal>(&first.projection))
        {
            result.candidate = *candidate;
            result.candidate->clangUsr = knownUsr;
            auto option = first.microsoftVolatileSemantics;
            auto alignment = first.declaration.declarationAlignmentBits;
            for (const auto i : result.declarationIndices)
            {
                if (entries[i].microsoftVolatileSemantics != option) option = std::nullopt;
                if (!entries[i].declaration.declarationAlignmentBits) alignment = std::nullopt;
                else if (alignment) alignment = std::min(*alignment, *entries[i].declaration.declarationAlignmentBits);
            }
            for (const auto i : result.aliasIndices)
            {
                if (entries[i].microsoftVolatileSemantics != option) option = std::nullopt;
                if (!entries[i].declaration.declarationAlignmentBits) alignment = std::nullopt;
                else if (alignment) alignment = std::min(*alignment, *entries[i].declaration.declarationAlignmentBits);
            }
            result.candidate->microsoftVolatileSemantics = option;
            result.candidate->declarationAlignmentBits = alignment;
        }
    return result;
}

std::vector<CppPublishedTemplateGlobalCandidates> selectCppPublishedTemplateGlobals(
    const frontend::ProjectCompilation& compilation, const LibraryMetadata& metadata,
    std::span<const CppSemanticGlobalEntry> inventory)
{
    std::set<std::string> published;
    for (const auto& item : metadata.genericTemplates) published.insert(item.canonicalKey);
    std::vector<CppPublishedTemplateGlobalCandidates> result;
    const auto demands = compilation.cppTemplateDemands();
    for (std::size_t index = 0; index < demands.size(); ++index)
    {
        const auto& item = demands[index];
        if (!published.contains(item.templateKey) || item.demand.lookupName.empty()) continue;
        for (const auto project : item.demand.visibleProjects)
            for (const auto& ns : item.demand.lookupNamespaces)
            {
                std::string qualified;
                for (const char c : ns) qualified += c == '.' ? "::" : std::string(1, c);
                if (!qualified.empty()) qualified += "::";
                qualified += item.demand.lookupName;
                auto selection = selectCppSemanticGlobal(inventory, project, qualified);
                if (selection.declarationIndices.empty() && selection.internalIndices.empty()) continue;
                result.push_back({item.templateKey, index, project, ns, std::move(selection)});
            }
    }
    return result;
}

bool captureCppTemplateGlobalCandidates(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::span<const CppSemanticGlobalEntry> inventory,
    std::vector<metadata_detail::CDependencyGlobalSnapshot>& output, std::string& error)
{
    const auto* graph = compilation.projectGraph();
    const auto* lookup = compilation.nameLookup();
    if (!graph || !lookup) { error = "C template global capture requires a project lookup"; return false; }
    const auto candidates = selectCppPublishedTemplateGlobals(compilation, metadata, inventory);
    std::vector<frontend::CppFunctionDemand> demands;
    for (const auto& item : compilation.cppTemplateDemands()) demands.push_back(item.demand);
    std::vector<frontend::NamespaceStateSymbol> states;
    if (!provideCppSemanticValuesWithLookup(*graph, compilation.cppProjectSessions(), *lookup,
                                            demands, states, error)) return false;
    std::vector<metadata_detail::CDependencyGlobalSnapshot> result;
    std::set<std::tuple<std::size_t, std::size_t, std::string>> seen;
    for (const auto& candidate : candidates)
    {
        const auto selected = std::find_if(states.begin(), states.end(), [&](const auto& state) {
            return state.projectOrdinal == candidate.projectOrdinal && state.namespaceName == candidate.namespaceName &&
                candidate.templateDemandIndex < compilation.cppTemplateDemands().size() &&
                state.name == compilation.cppTemplateDemands()[candidate.templateDemandIndex].demand.lookupName &&
                state.cppCGlobal && state.cppImportError.empty();
        });
        if (selected == states.end())
        {
            // A template that names a C global this producer cannot import still records the declaration it saw.
            // Without it, the consumer would bind the name in silence to another producer's declaration.
            // Una plantilla que nombra una global C que este productor no puede importar registra igualmente la
            // declaración que vio; sin ella el consumidor la uniría en silencio a la declaración de otro productor.
            if (!candidate.selection.conflict.empty() || candidate.selection.unsupportedIndices.empty() ||
                candidate.projectOrdinal >= graph->projects().size()) continue;
            for (const auto index : candidate.selection.unsupportedIndices)
            {
                if (index >= inventory.size()) { error = "C global candidate index is invalid"; return false; }
                const auto& entry = inventory[index];
                const auto& declaration = entry.declaration;
                if (!entry.cLanguage || entry.targetTriple != "x86_64-pc-windows-msvc" ||
                    !declaration.externalLinkage || !declaration.externalSymbol ||
                    declaration.externalSymbol->empty() || !declaration.clangUsr || declaration.clangUsr->empty() ||
                    !seen.emplace(entry.projectOrdinal, entry.referenceIndex, *declaration.externalSymbol).second)
                    continue;
                metadata_detail::CDependencyGlobalSnapshot snapshot;
                snapshot.moduleName = graph->projects()[entry.projectOrdinal].moduleName();
                snapshot.referenceIdentity = "cpp-reference:" + std::to_string(entry.referenceIndex);
                snapshot.linkSymbol = *declaration.externalSymbol;
                snapshot.microsoftVolatileSemantics = entry.microsoftVolatileSemantics;
                snapshot.effectiveDllImport = declaration.dllImport;
                snapshot.declaration = declaration;
                snapshot.declaration.location.reset();
                std::vector<std::uint8_t> encoded;
                std::string encodeError;
                if (!metadata_detail::encodeCDependencyGlobal(snapshot, encoded, encodeError)) continue;
                result.push_back(std::move(snapshot));
            }
            continue;
        }
        auto indices = candidate.selection.declarationIndices;
        indices.insert(indices.end(), candidate.selection.internalIndices.begin(), candidate.selection.internalIndices.end());
        for (const auto index : indices)
        {
            if (index >= inventory.size()) { error = "C global candidate index is invalid"; return false; }
            const auto& entry = inventory[index];
            if (!entry.cLanguage) continue;
            if (!seen.emplace(entry.projectOrdinal, entry.referenceIndex, selected->storageName).second) continue;
            metadata_detail::CDependencyGlobalSnapshot snapshot;
            auto reference = "cpp-reference:" + std::to_string(entry.referenceIndex);
            if (!selected->cppTlsRuntimeIdentity.empty())
                reference += "#tls-dll:" + selected->cppTlsRuntimeIdentity;
            if (!metadata_detail::captureCDependencyGlobal(inventory, *selected, entry.referenceIndex,
                                                             reference, snapshot, error)) return false;
            result.push_back(std::move(snapshot));
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

static bool provideGlobalsWithLookup(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error,
    const frontend::ProjectNameLookup* lookup)
{
    auto inventory = collectCppSemanticGlobals(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
    {
        error = "C/C++ global collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    auto& entries = std::get<std::vector<CppSemanticGlobalEntry>>(inventory);
    if (lookup)
        for (auto& entry : entries)
            entry.projection = projectGlobalWithLookup(entry.declaration, entry.projectOrdinal,
                graph.projects()[entry.projectOrdinal].moduleName(), lookup, entry.cLanguage);
    const auto supportedInternalC = [](const CppSemanticGlobalEntry& entry) {
        return entry.cLanguage;
    };
    std::set<std::pair<std::size_t, std::string_view>> availableNames;
    for (const auto& entry : entries)
        if (entry.declaration.externalLinkage || supportedInternalC(entry))
            availableNames.emplace(entry.projectOrdinal, entry.declaration.qualifiedName);
    std::set<std::tuple<std::size_t, std::string, std::string>> seen;
    std::vector<frontend::NamespaceStateSymbol> result;
    for (const auto& demand : demands)
        for (const auto owner : demand.visibleProjects)
            for (const auto& ns : demand.lookupNamespaces)
            {
                if (owner >= graph.projects().size() || demand.lookupName.empty() ||
                    !seen.emplace(owner, ns, demand.lookupName).second) continue;
                std::string qualified;
                for (const char c : ns) qualified += c == '.' ? "::" : std::string(1, c);
                if (!qualified.empty()) qualified += "::";
                qualified += demand.lookupName;
                if (!availableNames.contains({owner, qualified})) continue;
                auto selected = selectCppSemanticGlobal(entries, owner, qualified);
                std::vector<std::size_t> internalC;
                for (const auto index : selected.internalIndices)
                {
                    const auto& entry = entries[index];
                    if (entry.cLanguage)
                        internalC.push_back(index);
                }
                if (!internalC.empty())
                {
                    if (internalC.size() != 1 || !selected.declarationIndices.empty())
                    {
                        selected.conflict = "ambiguous C internal global origins";
                        selected.candidate.reset();
                        if (selected.declarationIndices.empty()) selected.declarationIndices = internalC;
                    }
                    else
                    {
                        const auto index = internalC.front();
                        const auto& entry = entries[index];
                        auto declaration = entry.declaration;
                        declaration.externalLinkage = true;
                        declaration.externalSymbol = cppCInlineLinkSymbol(
                            entry.projectFile, entry.referenceIndex, "global:" + declaration.name);
                        auto projection = projectGlobalWithLookup(declaration, owner,
                            graph.projects()[owner].moduleName(), lookup, true);
                        selected.declarationIndices.push_back(index);
                        if (auto* candidate = std::get_if<CppSemanticGlobal>(&projection))
                        {
                            candidate->clangUsr = declaration.clangUsr;
                            candidate->microsoftVolatileSemantics = entry.microsoftVolatileSemantics;
                            selected.candidate = std::move(*candidate);
                        }
                        else selected.conflict = std::get<CppSemanticGlobalFailure>(projection).message;
                    }
                }
                if (selected.declarationIndices.empty()) continue;
                frontend::NamespaceStateSymbol state;
                state.projectOrdinal = owner;
                state.moduleName = graph.projects()[owner].moduleName();
                state.namespaceName = ns;
                state.name = demand.lookupName;
                state.isCppExternal = true;
                state.accessibility = frontend::NamespaceFunctionAccessibility::Public;
                const auto references = graph.projects()[owner].cppLibraryReferences();
                std::optional<std::filesystem::path> tlsRuntime;
                bool tlsStatic = false;
                bool tlsOriginConflict = false;
                const auto inspectTlsOrigin = [&](std::size_t index) {
                    const auto referenceIndex = entries[index].referenceIndex;
                    if (referenceIndex >= references.size()) { tlsOriginConflict = true; return; }
                    const auto& path = references[referenceIndex].runtimePath;
                    if (path.empty()) { tlsStatic = true; return; }
                    auto normalized = path.lexically_normal();
                    if (tlsRuntime && *tlsRuntime != normalized) tlsOriginConflict = true;
                    else tlsRuntime = std::move(normalized);
                };
                if (selected.candidate && selected.candidate->threadLocal && internalC.empty())
                {
                    for (const auto index : selected.declarationIndices) inspectTlsOrigin(index);
                    for (const auto index : selected.aliasIndices) inspectTlsOrigin(index);
                    if (tlsRuntime)
                    {
                        const auto supportedTarget = [&](std::size_t index) {
                            const auto& triple = entries[index].targetTriple;
                            return triple.starts_with("x86_64-") && triple.find("windows-msvc") != std::string::npos;
                        };
                        if (!std::all_of(selected.declarationIndices.begin(), selected.declarationIndices.end(), supportedTarget) ||
                            !std::all_of(selected.aliasIndices.begin(), selected.aliasIndices.end(), supportedTarget))
                            tlsOriginConflict = true;
                    }
                }
                if (tlsOriginConflict || (tlsRuntime && tlsStatic))
                {
                    state.declaredTypeKey = "builtin:int"; // Body analysis reports the unsupported import.
                    state.cppImportError = "thread-local C/C++ global has ambiguous library origins";
                }
                else if (selected.candidate)
                {
                    state.declaredTypeKey = selected.candidate->typeKey;
                    state.storageName = selected.candidate->externalSymbol;
                    state.isVolatile = selected.candidate->isVolatile;
                    state.isCppReadonly = selected.candidate->isConst;
                    state.cppPointerPointeeReadonly = selected.candidate->pointerPointeeReadonly;
                    state.cppPointerPointeeVolatile = selected.candidate->pointerPointeeVolatile;
                    state.cppMicrosoftVolatileSemantics = selected.candidate->microsoftVolatileSemantics;
                    state.cppStorageSizeBits = selected.candidate->storageSizeBits;
                    state.cppDeclarationAlignmentBits = selected.candidate->declarationAlignmentBits;
                    state.cppClangUsr = selected.candidate->clangUsr;
                    state.cppDllImport = selected.candidate->dllImport || tlsRuntime.has_value();
                    state.cppThreadLocal = selected.candidate->threadLocal;
                    state.cppAtomicStorage = selected.candidate->atomicStorage;
                    state.cppCInternalLinkage = !internalC.empty();
                    state.cppCanonicalCType = selected.candidate->canonicalCppType;
                    if (tlsRuntime)
                    {
                        basic::Sha256Digest digest{};
                        std::uintmax_t size = 0;
                        if (!basic::sha256File(*tlsRuntime, digest, size, error)) return false;
                        const auto encodedName = tlsRuntime->filename().generic_u8string();
                        state.cppTlsRuntimeIdentity =
                            std::string(reinterpret_cast<const char*>(encodedName.data()), encodedName.size()) +
                            ":" + basic::sha256Hex(digest);
                    }
                    const auto fromC = [&](std::size_t index) { return entries[index].cLanguage; };
                    state.cppCGlobal = !selected.declarationIndices.empty() &&
                        std::all_of(selected.declarationIndices.begin(), selected.declarationIndices.end(), fromC) &&
                        std::all_of(selected.aliasIndices.begin(), selected.aliasIndices.end(), fromC);
                }
                else
                {
                    state.declaredTypeKey = "builtin:int"; // Never emitted; body analysis reports the reason.
                    if (!selected.conflict.empty()) state.cppImportError = selected.conflict;
                    else if (!selected.unsupportedIndices.empty())
                        state.cppImportError = std::get<CppSemanticGlobalFailure>(
                            entries[selected.unsupportedIndices.front()].projection).message;
                    else state.cppImportError = "C/C++ global could not be projected";
                }
                result.push_back(std::move(state));
            }
    states = std::move(result);
    error.clear();
    return true;
}

bool provideCppSemanticGlobals(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error)
{
    return provideGlobalsWithLookup(graph, projects, demands, states, error, nullptr);
}

bool provideCppInteropTypes(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& types, std::string& error)
{
    std::vector<frontend::CppImportedTypeDeclaration> records;
    std::vector<frontend::CppImportedTypeDeclaration> enums;
    if (!provideExperimentalCppRecordCandidates(graph, projects, records, error) ||
        !provideCppEnumTypes(graph, projects, enums, error)) return false;
    records.insert(records.end(), std::make_move_iterator(enums.begin()), std::make_move_iterator(enums.end()));
    types = std::move(records);
    error.clear();
    return true;
}

CppCEnumCatalogResult captureCppCEnumCatalog(
    std::span<const CppSemanticEnumEntry> inventory, std::size_t projectOrdinal,
    std::string_view moduleName, std::span<const std::string> requiredIdentityKeys)
{
    std::set<std::string> requested(requiredIdentityKeys.begin(), requiredIdentityKeys.end());
    std::map<std::string, CppCEnumSnapshot> result;
    const auto sameScalar = [](const CppType& a, const CppType& b) {
        return a.kind == b.kind && a.canonicalSpelling == b.canonicalSpelling &&
            a.unqualifiedCanonicalSpelling == b.unqualifiedCanonicalSpelling &&
            a.ieeeBinary64 == b.ieeeBinary64 &&
            a.isConst == b.isConst && a.isVolatile == b.isVolatile && a.isRestrict == b.isRestrict &&
            a.sizeBits == b.sizeBits && a.alignmentBits == b.alignmentBits;
    };
    const auto reconcileUsr = [](std::optional<std::string>& a, const std::optional<std::string>& b) {
        if (!b || b->empty()) return true;
        if (a && !a->empty() && a != b) return false;
        a = b;
        return true;
    };
    const auto supportedCppEnum = [](const CppEnumDeclaration& value) {
        return !value.isCxx || (!value.name.empty() && !value.qualifiedName.empty() &&
            value.clangUsr && !value.clangUsr->empty() && value.underlyingType &&
            ((value.underlyingType->kind == CppTypeKind::SignedInteger && value.underlyingType->canonicalSpelling == "int" &&
              value.sizeBits == 32 && value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "unsigned int" &&
              value.sizeBits == 32 && value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger && value.underlyingType->canonicalSpelling == "short" &&
              value.sizeBits == 16 && value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "unsigned short" &&
              value.sizeBits == 16 && value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger && value.underlyingType->canonicalSpelling == "long long" &&
              value.sizeBits == 64 && value.alignmentBits == 64) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "unsigned long long" &&
              value.sizeBits == 64 && value.alignmentBits == 64) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger && (value.underlyingType->canonicalSpelling == "signed char" || value.underlyingType->canonicalSpelling == "char") &&
              value.sizeBits == 8 && value.alignmentBits == 8) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "unsigned char" &&
              value.sizeBits == 8 && value.alignmentBits == 8) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "wchar_t" &&
              value.sizeBits == 16 && value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger && value.underlyingType->canonicalSpelling == "long" && value.sizeBits == 32 && value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "unsigned long" && value.sizeBits == 32 && value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "char8_t" && value.sizeBits == 8 && value.alignmentBits == 8) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "char16_t" && value.sizeBits == 16 && value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "char32_t" && value.sizeBits == 32 && value.alignmentBits == 32)));
    };
    // Forward declarations can precede a definition in another translation unit.
    // They still carry identity and target information that must not be ignored.
    std::map<std::string, std::pair<std::string, std::optional<std::string>>> observations;
    for (const auto& entry : inventory)
    {
        const auto& value = entry.declaration;
        if (entry.projectOrdinal != projectOrdinal || !supportedCppEnum(value) || value.name.empty()) continue;
        const auto identity = projectCppEnumDeclarationIdentity(value, moduleName);
        if (!identity || !requested.contains(identity->key())) continue;
        if (entry.targetTriple.empty())
            return CppCEnumSnapshotFailure{"C enum observation has no target: " + identity->key()};
        auto [it, inserted] = observations.try_emplace(identity->key(), entry.targetTriple, std::nullopt);
        if ((!inserted && it->second.first != entry.targetTriple) ||
            !reconcileUsr(it->second.second, value.clangUsr))
            return CppCEnumSnapshotFailure{"conflicting C enum observations: " + identity->key()};
    }
    for (const auto& entry : inventory)
    {
        const auto& value = entry.declaration;
        if (entry.projectOrdinal != projectOrdinal || !supportedCppEnum(value) || value.name.empty()) continue;
        const auto identity = projectCppEnumDeclarationIdentity(value, moduleName);
        if (!identity || !requested.contains(identity->key())) continue;
        if (!value.complete) continue;
        if (!value.underlyingType || !value.sizeBits || !value.alignmentBits ||
            !*value.sizeBits || !*value.alignmentBits)
            return CppCEnumSnapshotFailure{"incomplete C enum observation: " + identity->key()};
        const auto [it, inserted] = result.emplace(identity->key(), CppCEnumSnapshot{identity->key(), entry.targetTriple, value});
        if (inserted) continue;
        auto& prior = it->second.declaration;
        bool same = it->second.targetTriple == entry.targetTriple && prior.isCxx == value.isCxx && prior.scoped == value.scoped &&
            prior.sizeBits == value.sizeBits && prior.alignmentBits == value.alignmentBits &&
            sameScalar(*prior.underlyingType, *value.underlyingType) &&
            prior.enumerators.size() == value.enumerators.size() && reconcileUsr(prior.clangUsr, value.clangUsr);
        for (std::size_t i = 0; same && i < prior.enumerators.size(); ++i)
        {
            auto& a = prior.enumerators[i]; const auto& b = value.enumerators[i];
            same = a.name == b.name && a.decimalValue == b.decimalValue && a.valueBits == b.valueBits &&
                a.valueIsUnsigned == b.valueIsUnsigned && sameScalar(a.valueType, b.valueType) &&
                reconcileUsr(a.clangUsr, b.clangUsr);
        }
        if (!same) return CppCEnumSnapshotFailure{"conflicting C enum observations: " + identity->key()};
        if (prior.sourceProvenance != value.sourceProvenance) prior.sourceProvenance.reset();
    }
    if (result.size() != requested.size()) return CppCEnumSnapshotFailure{"required C enum is unavailable"};
    std::vector<CppCEnumSnapshot> catalog;
    catalog.reserve(result.size());
    for (auto& [key, value] : result)
    {
        value.declaration.clangUsr = observations.at(key).second;
        catalog.push_back(std::move(value));
    }
    return catalog;
}

bool provideCppEnumTypes(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& types, std::string& error)
{
    auto inventory = collectCppSemanticEnums(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
    { error = failure->message; return false; }
    using Key = std::pair<std::size_t, std::string>;
    std::map<Key, const CppSemanticEnumEntry*> selected;
    std::map<Key, std::string> knownUsrs;
    std::map<std::pair<Key, std::string>, std::string> knownEnumeratorUsrs;
    std::set<Key> conflicts;
    for (const auto& entry : std::get<std::vector<CppSemanticEnumEntry>>(inventory))
    {
        const auto& value = entry.declaration;
        if ((value.isCxx && (value.qualifiedName.empty() ||
             !value.clangUsr || value.clangUsr->empty())) || value.name.empty() || !value.complete || !value.underlyingType ||
            !value.sizeBits || !value.alignmentBits || !value.location ||
            entry.projectOrdinal >= graph.projects().size()) continue;
        if (value.isCxx && !((value.underlyingType->kind == CppTypeKind::SignedInteger &&
              value.underlyingType->canonicalSpelling == "int" && *value.sizeBits == 32 && *value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger &&
              value.underlyingType->canonicalSpelling == "unsigned int" && *value.sizeBits == 32 && *value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger &&
              value.underlyingType->canonicalSpelling == "short" && *value.sizeBits == 16 && *value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger &&
              value.underlyingType->canonicalSpelling == "unsigned short" && *value.sizeBits == 16 && *value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger &&
              value.underlyingType->canonicalSpelling == "long long" && *value.sizeBits == 64 && *value.alignmentBits == 64) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger &&
              value.underlyingType->canonicalSpelling == "unsigned long long" && *value.sizeBits == 64 && *value.alignmentBits == 64) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger &&
              (value.underlyingType->canonicalSpelling == "signed char" || value.underlyingType->canonicalSpelling == "char") && *value.sizeBits == 8 && *value.alignmentBits == 8) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger &&
              value.underlyingType->canonicalSpelling == "unsigned char" && *value.sizeBits == 8 && *value.alignmentBits == 8) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger &&
              value.underlyingType->canonicalSpelling == "wchar_t" && *value.sizeBits == 16 && *value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::SignedInteger && value.underlyingType->canonicalSpelling == "long" && *value.sizeBits == 32 && *value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "unsigned long" && *value.sizeBits == 32 && *value.alignmentBits == 32) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "char8_t" && *value.sizeBits == 8 && *value.alignmentBits == 8) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "char16_t" && *value.sizeBits == 16 && *value.alignmentBits == 16) ||
             (value.underlyingType->kind == CppTypeKind::UnsignedInteger && value.underlyingType->canonicalSpelling == "char32_t" && *value.sizeBits == 32 && *value.alignmentBits == 32))) continue;
        const Key key{entry.projectOrdinal, value.isCxx ? value.qualifiedName : value.name};
        if (value.clangUsr && !value.clangUsr->empty())
        {
            const auto [known, inserted] = knownUsrs.emplace(key, *value.clangUsr);
            if (!inserted && known->second != *value.clangUsr) conflicts.insert(key);
        }
        for (const auto& enumerator : value.enumerators)
        {
            if (!enumerator.clangUsr || enumerator.clangUsr->empty()) continue;
            const auto [known, inserted] = knownEnumeratorUsrs.emplace(
                std::pair{key, enumerator.name}, *enumerator.clangUsr);
            if (!inserted && known->second != *enumerator.clangUsr) conflicts.insert(key);
        }
        const auto [where, inserted] = selected.emplace(key, &entry);
        if (!inserted)
        {
            const auto& prior = where->second->declaration;
            bool same = prior.isCxx == value.isCxx && prior.scoped == value.scoped &&
                where->second->targetTriple == entry.targetTriple &&
                (!prior.clangUsr || !value.clangUsr || prior.clangUsr == value.clangUsr) &&
                prior.sizeBits == value.sizeBits && prior.alignmentBits == value.alignmentBits &&
                prior.underlyingType && prior.underlyingType->canonicalSpelling == value.underlyingType->canonicalSpelling &&
                prior.enumerators.size() == value.enumerators.size();
            for (std::size_t i = 0; same && i < value.enumerators.size(); ++i)
                same = prior.enumerators[i].name == value.enumerators[i].name &&
                    prior.enumerators[i].decimalValue == value.enumerators[i].decimalValue &&
                    prior.enumerators[i].valueType.kind == value.enumerators[i].valueType.kind &&
                    prior.enumerators[i].valueType.canonicalSpelling == value.enumerators[i].valueType.canonicalSpelling &&
                    prior.enumerators[i].valueType.sizeBits == value.enumerators[i].valueType.sizeBits &&
                    prior.enumerators[i].valueType.alignmentBits == value.enumerators[i].valueType.alignmentBits;
            if (!same) conflicts.insert(key);
        }
    }
    std::vector<frontend::CppImportedTypeDeclaration> result;
    for (const auto& [key, entry] : selected)
    {
        const auto& value = entry->declaration;
        const auto base = frontend::cppAliasBuiltinType(*value.underlyingType);
        if (!base || base->pointerDepth || entry->targetTriple != "x86_64-pc-windows-msvc" ||
            *value.sizeBits == 0 || *value.alignmentBits == 0 || *value.sizeBits % 8 || *value.alignmentBits % 8 ||
            (value.underlyingType->kind != CppTypeKind::SignedInteger &&
             value.underlyingType->kind != CppTypeKind::UnsignedInteger)) continue;
        auto identity = projectCppEnumDeclarationIdentity(value, graph.projects()[key.first].moduleName());
        if (!identity)
        {
            if (value.isCxx) continue;
            error = "invalid C enum identity: " + value.name;
            return false;
        }
        const auto encodedFile = value.location->file.generic_u8string();
        frontend::CppImportedTypeDeclaration type{std::move(*identity), frontend::NamedTypeDeclarationKind::Enum,
            frontend::NamedTypeAccessibility::Public, key.first,
            std::string(reinterpret_cast<const char*>(encodedFile.data()), encodedFile.size())};
        type.objectLayout = frontend::CppImportedObjectLayout{*value.sizeBits / 8, *value.alignmentBits / 8};
        type.enumBase = frontend::CppImportedEnumBase{base->token, std::string(base->name)};
        if (value.isCxx && !conflicts.contains(key)) type.cppEnumScoped = value.scoped;
        if (value.isCxx && value.scoped && !conflicts.contains(key))
            for (std::size_t i = 0; i < value.enumerators.size(); ++i)
            {
                auto projected = projectCppCxxEnumConstant(*entry, i, graph.projects()[key.first].moduleName(), true);
                if (const auto* failure = std::get_if<CppSemanticEnumConstantFailure>(&projected))
                { type.deferredImportError = failure->message; break; }
                const auto& member = std::get<CppSemanticEnumConstant>(projected);
                type.scopedEnumMembers.push_back({member.name, member.bits, member.clangUsr});
            }
        if (!conflicts.contains(key))
        {
            type.origin = value.isCxx ? frontend::CppImportedTypeOrigin::CppEnum : frontend::CppImportedTypeOrigin::CEnum;
            if (!entry->targetTriple.empty()) type.targetTriple = entry->targetTriple;
        }
        if (const auto known = knownUsrs.find(key); known != knownUsrs.end() && !conflicts.contains(key))
            type.clangUsr = known->second;
        if (conflicts.contains(key)) type.deferredImportError = "conflicting C enum declarations: " + value.name;
        result.push_back(std::move(type));
    }
    types = std::move(result);
    error.clear();
    return true;
}

static CppSemanticEnumConstantResult projectCppEnumConstantImpl(
    const CppSemanticEnumEntry& entry, std::size_t enumeratorIndex, bool allowCpp, bool allowScoped = false)
{
    if ((!allowCpp && entry.declaration.isCxx) || !entry.declaration.complete ||
        enumeratorIndex >= entry.declaration.enumerators.size())
        return CppSemanticEnumConstantFailure{"enum constant is unavailable for C projection"};
    const auto& value = entry.declaration.enumerators[enumeratorIndex];
    if (entry.declaration.isCxx &&
        ((entry.declaration.scoped && !allowScoped) || entry.declaration.name.empty() ||
         entry.declaration.qualifiedName.empty() ||
         !entry.declaration.clangUsr || entry.declaration.clangUsr->empty() ||
         !entry.declaration.underlyingType || entry.targetTriple != "x86_64-pc-windows-msvc" ||
         !((entry.declaration.underlyingType->kind == CppTypeKind::SignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "int" && entry.declaration.sizeBits == 32 && entry.declaration.alignmentBits == 32) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "unsigned int" && entry.declaration.sizeBits == 32 && entry.declaration.alignmentBits == 32) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::SignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "short" && entry.declaration.sizeBits == 16 && entry.declaration.alignmentBits == 16) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "unsigned short" && entry.declaration.sizeBits == 16 && entry.declaration.alignmentBits == 16) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::SignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "long long" && entry.declaration.sizeBits == 64 && entry.declaration.alignmentBits == 64) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "unsigned long long" && entry.declaration.sizeBits == 64 && entry.declaration.alignmentBits == 64) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::SignedInteger &&
            (entry.declaration.underlyingType->canonicalSpelling == "signed char" || entry.declaration.underlyingType->canonicalSpelling == "char") && entry.declaration.sizeBits == 8 && entry.declaration.alignmentBits == 8) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "unsigned char" && entry.declaration.sizeBits == 8 && entry.declaration.alignmentBits == 8) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger &&
            entry.declaration.underlyingType->canonicalSpelling == "wchar_t" && entry.declaration.sizeBits == 16 && entry.declaration.alignmentBits == 16) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::SignedInteger && entry.declaration.underlyingType->canonicalSpelling == "long" && entry.declaration.sizeBits == 32 && entry.declaration.alignmentBits == 32) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger && entry.declaration.underlyingType->canonicalSpelling == "unsigned long" && entry.declaration.sizeBits == 32 && entry.declaration.alignmentBits == 32) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger && entry.declaration.underlyingType->canonicalSpelling == "char8_t" && entry.declaration.sizeBits == 8 && entry.declaration.alignmentBits == 8) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger && entry.declaration.underlyingType->canonicalSpelling == "char16_t" && entry.declaration.sizeBits == 16 && entry.declaration.alignmentBits == 16) ||
           (entry.declaration.underlyingType->kind == CppTypeKind::UnsignedInteger && entry.declaration.underlyingType->canonicalSpelling == "char32_t" && entry.declaration.sizeBits == 32 && entry.declaration.alignmentBits == 32)) ||
         value.valueType.kind != CppTypeKind::Enumeration || !value.valueType.enumIdentity ||
         value.valueType.enumIdentity->clangUsr != entry.declaration.clangUsr ||
         value.valueType.sizeBits != entry.declaration.sizeBits || value.valueType.alignmentBits != entry.declaration.alignmentBits))
        return CppSemanticEnumConstantFailure{"C++ enumerator identity or representation is unsupported"};
    const auto& type = entry.declaration.isCxx ? *entry.declaration.underlyingType : value.valueType;
    const auto mapped = mapCppScalarType(type);
    if (!mapped || value.name.empty() || !type.sizeBits || *type.sizeBits == 0 || *type.sizeBits > 64 ||
        (type.kind != CppTypeKind::SignedInteger && type.kind != CppTypeKind::UnsignedInteger))
        return CppSemanticEnumConstantFailure{"enum constant type cannot be represented"};
    const auto width = static_cast<unsigned>(*type.sizeBits);
    const auto first = value.decimalValue.data();
    const auto last = first + value.decimalValue.size();
    std::uint64_t bits = 0;
    if (type.kind == CppTypeKind::SignedInteger)
    {
        std::int64_t number = 0;
        const auto parsed = std::from_chars(first, last, number);
        if (parsed.ec != std::errc{} || parsed.ptr != last)
            return CppSemanticEnumConstantFailure{"enum constant is outside signed 64-bit range"};
        if (width < 64)
        {
            const auto limit = std::int64_t{1} << (width - 1);
            if (number < -limit || number >= limit)
                return CppSemanticEnumConstantFailure{"enum constant is outside its declared type"};
        }
        bits = static_cast<std::uint64_t>(number);
    }
    else
    {
        const auto parsed = std::from_chars(first, last, bits);
        if (parsed.ec != std::errc{} || parsed.ptr != last ||
            (width < 64 && bits >= (std::uint64_t{1} << width)))
            return CppSemanticEnumConstantFailure{"enum constant is outside its unsigned type"};
    }
    if (width < 64) bits &= (std::uint64_t{1} << width) - 1;
    return CppSemanticEnumConstant{entry.projectOrdinal, value.name, mapped->nativeSharpTypeKey, bits, value.clangUsr};
}

CppSemanticEnumConstantResult projectCppEnumConstant(
    const CppSemanticEnumEntry& entry, std::size_t enumeratorIndex)
{
    return projectCppEnumConstantImpl(entry, enumeratorIndex, false);
}

CppSemanticEnumConstantResult projectCppCxxEnumConstant(
    const CppSemanticEnumEntry& entry, std::size_t enumeratorIndex, std::string_view moduleName, bool allowScoped)
{
    if (!entry.declaration.isCxx) return CppSemanticEnumConstantFailure{"C++ enum is required"};
    auto projected = projectCppEnumConstantImpl(entry, enumeratorIndex, true, allowScoped);
    auto* value = std::get_if<CppSemanticEnumConstant>(&projected);
    if (!value) return projected;
    const auto identity = projectCppEnumDeclarationIdentity(entry.declaration, moduleName);
    if (!identity) return CppSemanticEnumConstantFailure{"invalid C++ enum identity"};
    value->typeKey = "type:" + identity->key();
    return projected;
}

static bool provideValuesWithLookup(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error,
    const frontend::ProjectNameLookup* lookup)
{
    std::vector<frontend::NamespaceStateSymbol> globals;
    std::vector<frontend::NamespaceStateSymbol> enumerators;
    if (!provideGlobalsWithLookup(graph, projects, demands, globals, error, lookup) ||
        !provideCppSemanticEnumConstants(graph, projects, demands, enumerators, error)) return false;
    using Key = std::tuple<std::size_t, std::string, std::string>;
    std::map<Key, std::size_t> index;
    for (std::size_t i = 0; i < globals.size(); ++i)
        index.emplace(Key{globals[i].projectOrdinal, globals[i].namespaceName, globals[i].name}, i);
    for (auto& enumerator : enumerators)
    {
        const Key key{enumerator.projectOrdinal, enumerator.namespaceName, enumerator.name};
        const auto found = index.find(key);
        if (found == index.end())
        {
            index.emplace(key, globals.size());
            globals.push_back(std::move(enumerator));
            continue;
        }
        auto& candidate = globals[found->second];
        candidate.isCppExternal = true;
        candidate.isCppEnumConstant = false;
        candidate.isConst = false;
        candidate.storageName.clear();
        candidate.cppImportError = "C enumerator conflicts with another imported value";
    }
    states = std::move(globals);
    error.clear();
    return true;
}

bool provideCppSemanticValues(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error)
{
    return provideValuesWithLookup(graph, projects, demands, states, error, nullptr);
}

bool provideCppSemanticValuesWithLookup(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error)
{
    return provideValuesWithLookup(graph, projects, demands, states, error, &lookup);
}

bool provideCppSemanticEnumConstants(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& states, std::string& error)
{
    auto inventory = collectCppSemanticEnums(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
    {
        error = "C enum collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    const auto& entries = std::get<std::vector<CppSemanticEnumEntry>>(inventory);
    std::set<std::tuple<std::size_t, std::string, std::string>> seen;
    std::vector<frontend::NamespaceStateSymbol> result;
    for (const auto& demand : demands)
        for (const auto owner : demand.visibleProjects)
          for (const auto& ns : demand.lookupNamespaces)
        {
            if (owner >= graph.projects().size() || demand.lookupName.empty() ||
                !seen.emplace(owner, ns, demand.lookupName).second) continue;
            std::optional<CppSemanticEnumConstant> selected;
            std::optional<std::string> knownUsr;
            std::optional<std::string> targetTriple;
            std::string failure;
            bool found = false;
            for (const auto& entry : entries)
            {
                if (entry.projectOrdinal != owner) continue;
                if (!entry.declaration.isCxx && !ns.empty()) continue;
                if (entry.declaration.isCxx)
                {
                    if (entry.declaration.scoped || entry.declaration.name.empty()) continue;
                    const auto identity = projectCppEnumDeclarationIdentity(entry.declaration, graph.projects()[owner].moduleName());
                    if (!identity) continue;
                    std::string enumNamespace;
                    for (const auto& segment : identity->namespaceSegments())
                    {
                        if (!enumNamespace.empty()) enumNamespace += '.';
                        enumNamespace += segment;
                    }
                    if (enumNamespace != ns) continue;
                }
                for (std::size_t i = 0; i < entry.declaration.enumerators.size(); ++i)
                {
                    if (entry.declaration.enumerators[i].name != demand.lookupName) continue;
                    found = true;
                    if (targetTriple && *targetTriple != entry.targetTriple)
                        failure = "conflicting C enumerator target triples";
                    else targetTriple = entry.targetTriple;
                    auto projected = entry.declaration.isCxx ?
                        projectCppCxxEnumConstant(entry, i, graph.projects()[owner].moduleName()) :
                        projectCppEnumConstant(entry, i);
                    if (const auto* reason = std::get_if<CppSemanticEnumConstantFailure>(&projected))
                    { failure = reason->message; continue; }
                    auto value = std::get<CppSemanticEnumConstant>(std::move(projected));
                    if (value.clangUsr && !value.clangUsr->empty())
                    {
                        if (knownUsr && *knownUsr != *value.clangUsr)
                            failure = "conflicting C enumerator identities";
                        else knownUsr = value.clangUsr;
                    }
                    if (selected && (selected->typeKey != value.typeKey || selected->bits != value.bits))
                        failure = "conflicting C enumerator declarations";
                    else selected = std::move(value);
                }
            }
            if (!found) continue;
            frontend::NamespaceStateSymbol state;
            state.projectOrdinal = owner;
            state.moduleName = graph.projects()[owner].moduleName();
            state.namespaceName = ns;
            state.name = demand.lookupName;
            state.accessibility = frontend::NamespaceFunctionAccessibility::Public;
            if (!failure.empty() || !selected)
            {
                state.isCppExternal = true;
                state.declaredTypeKey = "builtin:int";
                state.cppImportError = failure.empty() ? "C enumerator cannot be represented" : failure;
            }
            else
            {
                state.isConst = true;
                state.isCppEnumConstant = true;
                state.cppClangUsr = knownUsr;
                state.declaredTypeKey = selected->typeKey;
                state.initializerKind = frontend::NamespaceStateInitializerKind::Integer;
                state.integerValue = selected->bits;
                state.identityKey = "state:" + state.moduleName + "::" + ns + "." + state.name;
                state.storageName = "__nsc_cenum_" + std::to_string(state.moduleName.size()) + "_" +
                    state.moduleName + "_" + std::to_string(state.name.size()) + "_" + state.name;
                if (!ns.empty()) state.storageName += "_ns_" + compilation::symbolFragmentForIdentityKey(ns);
            }
            result.push_back(std::move(state));
        }
    states = std::move(result);
    error.clear();
    return true;
}

CppSemanticEnumInventoryResult collectCppSemanticEnums(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticEnumEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner) return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->enumsSnapshot();
            if (!declarations)
                return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ enum snapshot is not available"};
            const auto target = session->optionsSnapshot().targetTriple;
            for (auto& declaration : *declarations)
                result.push_back({group.projectFile, index, *owner, target, std::move(declaration)});
        }
    }
    return result;
}

bool provideCppTypeAliases(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeAlias>& output, std::string& error)
{
    auto inventory = collectCppSemanticTypeAliases(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
    {
        error = "C/C++ type alias collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    std::vector<frontend::CppImportedTypeAlias> result;
    for (auto& entry : std::get<std::vector<CppSemanticTypeAliasEntry>>(inventory))
        result.push_back({entry.projectOrdinal, std::move(entry.projectFile), entry.referenceIndex,
                          std::move(entry.targetTriple), std::move(entry.declaration), {}});
    output = std::move(result);
    error.clear();
    return true;
}

bool provideCppTypeAliasesWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects,
    std::span<const frontend::PrecompiledSyntaxModule> modules,
    std::vector<frontend::CppImportedTypeAlias>& output, std::string& error)
{
    std::vector<frontend::CppImportedTypeAlias> result;
    if (!provideCppTypeAliases(graph, projects, result, error)) return false;
    std::map<std::string, std::size_t> owners;
    std::set<std::string> ambiguous;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
        if (!owners.emplace(graph.projects()[i].moduleName(), i).second)
            ambiguous.insert(graph.projects()[i].moduleName());
    std::map<std::pair<std::size_t, std::string>, std::size_t> references;
    std::vector<CppCRecordSnapshot> catalogRecords;
    std::vector<CppCEnumSnapshot> catalogEnums;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return false;
        catalogRecords.insert(catalogRecords.end(), records.begin(), records.end());
        catalogEnums.insert(catalogEnums.end(), enums.begin(), enums.end());
        for (const auto& alias : dependencies.aliases)
        {
            const auto owner = owners.find(alias.moduleName);
            if (owner == owners.end() || ambiguous.contains(alias.moduleName))
            { error = "precompiled C alias owner is missing or ambiguous: " + alias.moduleName; return false; }
            const auto key = std::make_pair(owner->second, alias.referenceIdentity);
            auto reference = references.find(key);
            if (reference == references.end())
            {
                std::size_t index = graph.projects()[owner->second].cppLibraryReferences().size();
                for (const auto& [existing, unused] : references)
                    if (existing.first == owner->second) ++index;
                reference = references.emplace(key, index).first;
            }
            frontend::CppImportedTypeAlias restored;
            if (!reconstructCppPrecompiledCAlias(alias, graph, owner->second, reference->second,
                    "x86_64-pc-windows-msvc", restored, error)) return false;
            restored.fromPrecompiledCatalog = true;
            result.push_back(std::move(restored));
        }
    }
    std::map<std::pair<std::string, std::string>, std::vector<std::size_t>> functionAliases;
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto& alias = result[i];
        const auto& type = alias.declaration.underlyingType;
        if (type.kind == CppTypeKind::Record || type.kind == CppTypeKind::Enumeration ||
            (type.kind == CppTypeKind::Pointer && type.element &&
             (type.element->kind == CppTypeKind::Function || type.element->kind == CppTypeKind::Record)))
            functionAliases[{alias.declaration.qualifiedName, alias.targetTriple}].push_back(i);
    }
    for (const auto& [key, indices] : functionAliases)
        for (std::size_t leftIndex = 0; leftIndex < indices.size(); ++leftIndex)
        for (std::size_t rightIndex = leftIndex + 1; rightIndex < indices.size(); ++rightIndex)
        {
            auto& a = result[indices[leftIndex]];
            auto& b = result[indices[rightIndex]];
            if (a.projectOrdinal == b.projectOrdinal || a.targetTriple != b.targetTriple ||
                a.declaration.qualifiedName != b.declaration.qualifiedName ||
                a.declaration.underlyingType.canonicalSpelling != b.declaration.underlyingType.canonicalSpelling) continue;
            bool comparedNominal = false;
            const auto sameType = [&](const auto& self, const CppType& left, const CppType& right,
                                      unsigned depth) -> bool {
                if (depth > 64 || left.kind != right.kind || left.isConst != right.isConst ||
                    left.isVolatile != right.isVolatile || left.isRestrict != right.isRestrict ||
                    left.sizeBits != right.sizeBits || left.alignmentBits != right.alignmentBits ||
                    left.canonicalSpelling != right.canonicalSpelling) return false;
                if (left.kind == CppTypeKind::Record)
                {
                    if (!left.recordIdentity || !right.recordIdentity || !left.recordIdentity->clangUsr ||
                        left.recordIdentity->clangUsr != right.recordIdentity->clangUsr) return false;
                    const auto find = [&](std::size_t project, const auto& usr) {
                        return std::find_if(catalogRecords.begin(), catalogRecords.end(), [&](const auto& record) {
                            return record.moduleName == graph.projects()[project].moduleName() && record.clangUsr == usr;
                        });
                    };
                    const auto l = find(a.projectOrdinal, left.recordIdentity->clangUsr);
                    const auto r = find(b.projectOrdinal, right.recordIdentity->clangUsr);
                    if (l == catalogRecords.end() || r == catalogRecords.end()) return false;
                    metadata_detail::CRecordContractComparison verdict{};
                    std::string comparisonError;
                    if (!metadata_detail::compareCRecordClosureEvidence(catalogRecords, catalogEnums, l->identityKey,
                            catalogRecords, catalogEnums, r->identityKey, verdict, comparisonError, true) ||
                        verdict != metadata_detail::CRecordContractComparison::Matching) return false;
                    comparedNominal = true;
                }
                else if (left.kind == CppTypeKind::Enumeration)
                {
                    if (!left.enumIdentity || !right.enumIdentity || !left.enumIdentity->clangUsr ||
                        left.enumIdentity->clangUsr != right.enumIdentity->clangUsr ||
                        left.enumIdentity->name.empty() || right.enumIdentity->name != left.enumIdentity->name)
                        return false;
                    const auto find = [&](std::size_t project) {
                        const compilation::TypeIdentity identity(graph.projects()[project].moduleName(), {}, {},
                                                                 left.enumIdentity->name);
                        return std::find_if(catalogEnums.begin(), catalogEnums.end(), [&](const auto& enumeration) {
                            return identity.isValid() && enumeration.identityKey == identity.key() &&
                                enumeration.declaration.clangUsr == left.enumIdentity->clangUsr;
                        });
                    };
                    const auto l = find(a.projectOrdinal);
                    const auto r = find(b.projectOrdinal);
                    if (l == catalogEnums.end() || r == catalogEnums.end()) return false;
                    metadata_detail::CRecordContractComparison verdict{};
                    std::string comparisonError;
                    if (!metadata_detail::compareCEnumSourceEvidence(*l, *r, verdict, comparisonError, true, true) ||
                        verdict != metadata_detail::CRecordContractComparison::Matching) return false;
                    comparedNominal = true;
                }
                else if (left.kind == CppTypeKind::Unsupported)
                    return false;
                if (bool(left.element) != bool(right.element) || bool(left.function) != bool(right.function)) return false;
                if (left.element && !self(self, *left.element, *right.element, depth + 1)) return false;
                if (left.function)
                {
                    const auto& x = *left.function;
                    const auto& y = *right.function;
                    if (x.callingConvention != y.callingConvention || x.hasPrototype != y.hasPrototype ||
                        x.variadic != y.variadic || x.parameterTypes.size() != y.parameterTypes.size() ||
                        !self(self, x.returnType, y.returnType, depth + 1)) return false;
                    for (std::size_t k = 0; k < x.parameterTypes.size(); ++k)
                        if (!self(self, x.parameterTypes[k], y.parameterTypes[k], depth + 1)) return false;
                }
                return true;
            };
            if (sameType(sameType, a.declaration.underlyingType, b.declaration.underlyingType, 0) && comparedNominal)
            {
                a.equivalentRecordFunctionAliases.emplace_back(b.projectOrdinal, b.referenceIndex);
                b.equivalentRecordFunctionAliases.emplace_back(a.projectOrdinal, a.referenceIndex);
            }
        }
    output = std::move(result);
    error.clear();
    return true;
}

// A C alias of a preserve_none function pointer receives the Clang plan of that alias
// only when a shared preserve_none validator accepts its scalar or record signature.
static void attachCPreserveNonePointerPlan(const CppInteropSession& session, CppTypeAliasDeclaration& declaration)
{
    const auto& pointer = declaration.underlyingType;
    if (!declaration.clangUsr || pointer.kind != CppTypeKind::Pointer || !pointer.element ||
        !pointer.element->function) return;
    const auto& signature = *pointer.element->function;
    if (signature.callingConvention != "preserve_none" || signature.variadic || !signature.hasPrototype) return;
    auto plan = session.cFunctionPointerAbiPlan(declaration.name, pointer.canonicalSpelling);
    if (plan && (abi_plan::isPreserveNoneCppScalarPlan(*plan, signature, "__nsc_pointer_abi_probe") ||
                 abi_plan::isPreserveNoneCppRecordPointerRoundTrip(*plan, signature) ||
                 abi_plan::isPreserveNoneCppScalarWithRecord(*plan, signature, "__nsc_pointer_abi_probe")))
        declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
}

CppSemanticTypeAliasInventoryResult collectCppSemanticTypeAliases(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticTypeAliasEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner) return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->typeAliasesSnapshot();
            if (!declarations)
                return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ alias snapshot is not available"};
            const auto target = session->optionsSnapshot().targetTriple;
            for (auto& declaration : *declarations)
            {
                const auto& pointer = declaration.underlyingType;
                if (session->optionsSnapshot().language == CppLanguageMode::C)
                    attachCPreserveNonePointerPlan(*session, declaration);
                if (session->optionsSnapshot().language == CppLanguageMode::Cpp && declaration.clangUsr &&
                    pointer.kind == CppTypeKind::Pointer && pointer.element && pointer.element->function &&
                    pointer.element->function->callingConvention == "preserve_none")
                {
                    auto plan = session->cppFunctionPointerAbiPlan(declaration.qualifiedName,
                        *declaration.clangUsr, pointer.canonicalSpelling);
                    if (plan && (abi_plan::isPreserveNoneCppRecordPointerRoundTrip(*plan, *pointer.element->function) ||
                                 abi_plan::isPreserveNoneCppScalarPlan(*plan, *pointer.element->function, "__nsc_pointer_abi_probe") ||
                                 abi_plan::isPreserveNoneCppScalarWithRecord(*plan, *pointer.element->function, "__nsc_pointer_abi_probe")))
                        declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
                }
                if (session->optionsSnapshot().language == CppLanguageMode::Cpp && declaration.clangUsr &&
                    pointer.kind == CppTypeKind::Pointer && pointer.element && pointer.element->function &&
                    pointer.element->function->callingConvention == "sysv_abi")
                {
                    auto plan = session->cppFunctionPointerAbiPlan(declaration.qualifiedName,
                        *declaration.clangUsr, pointer.canonicalSpelling);
                    if (plan && (abi_plan::isSysVCppRecordPointerRoundTrip(*plan, *pointer.element->function) ||
                                 abi_plan::isSysVCppRecordPointerScalarReturn(*plan, *pointer.element->function)))
                        declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
                }
                if (session->optionsSnapshot().language == CppLanguageMode::Cpp && declaration.clangUsr &&
                    pointer.kind == CppTypeKind::Pointer && pointer.element && pointer.element->function &&
                    pointer.element->function->callingConvention == "regcall" &&
                    !pointer.element->function->parameterTypes.empty())
                {
                    auto plan = session->cppFunctionPointerAbiPlan(declaration.qualifiedName,
                        *declaration.clangUsr, pointer.canonicalSpelling);
                    const auto& type = pointer.element->function->returnType;
                    const auto& parameters = pointer.element->function->parameterTypes;
                    const bool recordReturn = type.kind == CppTypeKind::Record && type.sizeBits && type.alignmentBits &&
                        *type.sizeBits % 8 == 0 && *type.alignmentBits % 8 == 0 && plan &&
                        (abi_plan::isRegCallWin64FloatRecordInterleaved(*plan, "__regcall3____nsc_pointer_abi_probe",
                            *type.sizeBits / 8, *type.alignmentBits / 8) ||
                         abi_plan::isRegCallWin64DoubleRecordInterleaved(*plan, "__regcall3____nsc_pointer_abi_probe",
                            *type.sizeBits / 8, *type.alignmentBits / 8) ||
                         abi_plan::isRegCallWin64FloatRecordsReturningCompactRecord(*plan, "__regcall3____nsc_pointer_abi_probe",
                            *type.sizeBits / 8, *type.alignmentBits / 8) ||
                         abi_plan::isRegCallWin64FloatRecordsReturningIndirectRecord(*plan, "__regcall3____nsc_pointer_abi_probe",
                            *type.sizeBits / 8, *type.alignmentBits / 8));
                    const bool intReturn = (type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger) && plan &&
                        (abi_plan::isRegCallWin64FloatRecordsReturningInt(*plan, "__regcall3____nsc_pointer_abi_probe") ||
                         abi_plan::isRegCallWin64IndirectRecordsReturningInt(*plan, "__regcall3____nsc_pointer_abi_probe")) &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool boolReturn = type.kind == CppTypeKind::Boolean && plan &&
                        abi_plan::isRegCallWin64FloatRecordsReturningBool(*plan, "__regcall3____nsc_pointer_abi_probe") &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool narrowReturn = (type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger) && plan &&
                        abi_plan::isRegCallWin64FloatRecordsReturningNarrowInteger(*plan, "__regcall3____nsc_pointer_abi_probe") &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool int64Return = (type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger) && plan &&
                        abi_plan::isRegCallWin64FloatRecordsReturningInt64(*plan, "__regcall3____nsc_pointer_abi_probe") &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool pointerReturn = abi_plan::regCallFloatRecordScalarPointerReturnSupported(type) && plan &&
                        abi_plan::isRegCallWin64FloatRecordsReturningPointer(*plan, "__regcall3____nsc_pointer_abi_probe") &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool doubleReturn = type.kind == CppTypeKind::Floating && plan &&
                        (abi_plan::isRegCallWin64FloatRecordsReturningDouble(*plan, "__regcall3____nsc_pointer_abi_probe") ||
                         abi_plan::isRegCallWin64IndirectRecordsReturningDouble(*plan, "__regcall3____nsc_pointer_abi_probe")) &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool floatReturn = type.kind == CppTypeKind::Floating && plan &&
                        abi_plan::isRegCallWin64FloatRecordsReturningFloat(*plan, "__regcall3____nsc_pointer_abi_probe") &&
                        abi_plan::regCallFloatRecordScalarMatches(plan->physicalReturn, type);
                    const bool voidReturn = type.kind == CppTypeKind::Void && type.canonicalSpelling == "void" &&
                        !type.isConst && !type.isVolatile && !type.isRestrict && plan &&
                        (abi_plan::isRegCallWin64FloatRecordsReturningVoid(*plan, "__regcall3____nsc_pointer_abi_probe") ||
                         abi_plan::isRegCallWin64IndirectRecordsReturningVoid(*plan, "__regcall3____nsc_pointer_abi_probe"));
                    if (plan && (recordReturn || intReturn || boolReturn || narrowReturn || int64Return || pointerReturn || doubleReturn || floatReturn || voidReturn) &&
                        abi_plan::regCallFloatRecordArgumentsMatch(*plan, type, parameters, true, true))
                        declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
                }
                result.push_back({group.projectFile, index, *owner, target, std::move(declaration)});
            }
        }
    }
    return result;
}

CppSignatureEnumResult collectCppSignatureEnumDependencies(
    const compilation::ProjectGraph& graph, std::span<const CppSemanticInventoryEntry> functions,
    std::span<const frontend::CppImportedTypeDeclaration> enumCandidates)
{
    std::map<std::pair<std::size_t, std::string>, std::size_t> candidates;
    for (std::size_t index = 0; index < enumCandidates.size(); ++index)
    {
        const auto& candidate = enumCandidates[index];
        if (candidate.kind != frontend::NamedTypeDeclarationKind::Enum ||
            !candidates.emplace(std::make_pair(candidate.projectOrdinal, candidate.identity.key()), index).second)
            return CppSignatureEnumFailure{"invalid or duplicate enum candidate"};
    }
    std::vector<CppSignatureEnumDependency> result;
    for (std::size_t functionIndex = 0; functionIndex < functions.size(); ++functionIndex)
    {
        const auto& function = functions[functionIndex];
        if (function.projectOrdinal >= graph.projects().size())
            return CppSignatureEnumFailure{"function owner is outside the project graph"};
        const auto collect = [&](const CppType& root, std::optional<std::size_t> parameter) {
            struct Frame { const CppType* type; std::size_t depth; std::size_t nesting; };
            std::vector<Frame> pending{{&root, 0, 0}};
            while (!pending.empty())
            {
                const auto [type, depth, nesting] = pending.back();
                pending.pop_back();
                if (!type || nesting > 64) return false;
                if (type->kind == CppTypeKind::Pointer || type->kind == CppTypeKind::Array ||
                    type->kind == CppTypeKind::LValueReference || type->kind == CppTypeKind::RValueReference)
                {
                    pending.push_back({type->element.get(), depth + 1, nesting + 1});
                    continue;
                }
                if (type->kind == CppTypeKind::Function)
                {
                    if (!type->function) return false;
                    pending.push_back({&type->function->returnType, depth, nesting + 1});
                    for (const auto& item : type->function->parameterTypes)
                        pending.push_back({&item, depth, nesting + 1});
                    continue;
                }
                if (type->kind != CppTypeKind::Enumeration) continue;
                if (!type->enumIdentity || type->enumIdentity->anonymousContext || type->enumIdentity->name.empty() ||
                    type->enumIdentity->qualifiedName != type->enumIdentity->name)
                {
                    result.push_back({functionIndex, parameter, depth, CppSignatureEnumStatus::Unprojectable, std::nullopt});
                    continue;
                }
                const compilation::TypeIdentity identity(graph.projects()[function.projectOrdinal].moduleName(), {}, {},
                                                         type->enumIdentity->name);
                const auto found = candidates.find({function.projectOrdinal, identity.key()});
                if (found == candidates.end())
                    result.push_back({functionIndex, parameter, depth, CppSignatureEnumStatus::Missing, std::nullopt});
                else if (type->enumIdentity->clangUsr && enumCandidates[found->second].clangUsr &&
                         type->enumIdentity->clangUsr != enumCandidates[found->second].clangUsr)
                    result.push_back({functionIndex, parameter, depth, CppSignatureEnumStatus::Unprojectable, std::nullopt});
                else
                    result.push_back({functionIndex, parameter, depth, CppSignatureEnumStatus::Matched, found->second});
            }
            return true;
        };
        if (!collect(function.declaration.returnType, std::nullopt))
            return CppSignatureEnumFailure{"incomplete or excessively nested function return type"};
        for (std::size_t index = 0; index < function.declaration.parameters.size(); ++index)
            if (!collect(function.declaration.parameters[index].type, index))
                return CppSignatureEnumFailure{"incomplete or excessively nested function parameter type"};
    }
    return result;
}

std::string cppCInlineLinkSymbol(const std::filesystem::path& projectFile,
    std::size_t referenceIndex, std::string_view name)
{
    const auto encoded = projectFile.lexically_normal().generic_u8string();
    const std::string input = std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()) + "\x1f" +
        std::to_string(referenceIndex) + "\x1f" + std::string(name);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(input.data());
    return "__nsc_c_inline_" + basic::sha256Hex(basic::sha256({bytes, input.size()}));
}

// Attach only the plan of the exact C declaration when its observed signature,
// symbol and physical ABI pass a shared preserve_none validator.
static void attachCPreserveNonePlan(const CppInteropSession& session, CppFunctionDeclaration& declaration)
{
    if (declaration.callingConvention != "preserve_none" || declaration.variadic || !declaration.hasPrototype ||
        !declaration.externalLinkage || !declaration.externalSymbol)
        return;
    auto plan = session.cFunctionAbiPlan(declaration.name, declaration.canonicalFunctionType);
    if (!plan || plan->symbol != *declaration.externalSymbol) return;
    CppFunctionType signature;
    signature.returnType = declaration.returnType;
    for (const auto& parameter : declaration.parameters) signature.parameterTypes.push_back(parameter.type);
    signature.callingConvention = declaration.callingConvention;
    signature.hasPrototype = declaration.hasPrototype;
    signature.variadic = declaration.variadic;
    if (abi_plan::isPreserveNoneCppScalarPlan(*plan, signature, *declaration.externalSymbol) ||
        abi_plan::isPreserveNoneCppRecordRoundTrip(*plan, signature, *declaration.externalSymbol) ||
        abi_plan::isPreserveNoneCppScalarWithRecord(*plan, signature, *declaration.externalSymbol))
        declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
}

// EN: T09-003. A C function with swiftcall, swiftasynccall or intel_ocl_bicc keeps the Clang plan of
//     its exact declaration only for a scalar or pointer signature checked by the shared validator.
// ES: T09-003. Una funcion C swiftcall, swiftasynccall o intel_ocl_bicc conserva el plan de Clang solo
//     para una firma escalar o de punteros comprobada por el validador compartido.
static void attachCPlannedScalarConventionPlan(const CppInteropSession& session, CppFunctionDeclaration& declaration)
{
    if (!abi_plan::cppPlannedScalarConventionId(declaration.callingConvention) || declaration.variadic ||
        !declaration.hasPrototype || !declaration.externalLinkage || !declaration.externalSymbol) return;
    auto plan = session.cFunctionAbiPlan(declaration.name, declaration.canonicalFunctionType);
    if (!plan || plan->symbol != *declaration.externalSymbol) return;
    CppFunctionType signature;
    signature.returnType = declaration.returnType;
    for (const auto& parameter : declaration.parameters) signature.parameterTypes.push_back(parameter.type);
    signature.callingConvention = declaration.callingConvention;
    signature.hasPrototype = declaration.hasPrototype;
    signature.variadic = declaration.variadic;
    if (abi_plan::isCppPlannedScalarConventionPlan(*plan, signature, *declaration.externalSymbol))
        declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
}

CppSemanticInventoryResult collectCppSemanticFunctions(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticInventoryEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t i = 0; i < graph.projects().size(); ++i)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[i].projectFile(), error);
            if (error || !same) continue;
            if (owner)
                return CppSemanticInventoryFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = i;
        }
        if (!owner)
            return CppSemanticInventoryFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session)
                return CppSemanticInventoryFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->freeFunctionsSnapshot();
            if (!declarations)
                return CppSemanticInventoryFailure{group.projectFile, index, "C/C++ session is not available"};
            const auto options = session->optionsSnapshot();
            const auto microsoftVolatileSemantics = session->microsoftVolatileSemantics();
            for (auto& declaration : *declarations)
            {
                const bool hasRecordValue = declaration.returnType.kind == CppTypeKind::Record ||
                    std::any_of(declaration.parameters.begin(), declaration.parameters.end(),
                        [](const CppFunctionParameter& parameter) { return parameter.type.kind == CppTypeKind::Record; });
                if (options.language == CppLanguageMode::C && declaration.externalLinkage &&
                    (declaration.callingConvention == "sysv_abi" || declaration.callingConvention == "regcall") && hasRecordValue)
                {
                    auto plan = session->cFunctionAbiPlan(declaration.name, declaration.canonicalFunctionType);
                    if (plan) declaration.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
                }
                if (options.language == CppLanguageMode::C)
                {
                    attachCPreserveNonePlan(*session, declaration);
                    attachCPlannedScalarConventionPlan(*session, declaration);
                }
                if (declaration.isCStaticInlineDefinition && !declaration.externalLinkage)
                    declaration.externalSymbol = cppCInlineLinkSymbol(group.projectFile, index, declaration.name);
                auto projection = projectCppSemanticFunction(
                    declaration, *owner, graph.projects()[*owner].moduleName());
                // EN: T08-D13b. The function keeps its reference's volatile mode.
                // ES: T08-D13b. La funcion conserva el modo volatile de su referencia.
                if (auto* symbol = std::get_if<frontend::NamespaceFunctionSymbol>(&projection))
                    symbol->cppMicrosoftVolatileSemantics = microsoftVolatileSemantics;
                result.push_back({group.projectFile, index, *owner, options.targetTriple,
                                  std::move(declaration), std::move(projection)});
            }
        }
    }
    return result;
}
}
