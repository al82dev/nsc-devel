#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticEnumType.h"
#include "nativesharp/Frontend/CppAliasArrayType.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace nativesharp::interop
{
namespace
{
std::optional<CppScalarTypeMapping> mapGlobalType(const CppType& type, std::size_t projectOrdinal,
    std::string_view moduleName, const frontend::ProjectNameLookup* lookup, bool cLanguage)
{
    if (cLanguage)
        if (auto callback = projectCppStoredFunctionPointer(type, projectOrdinal, moduleName, lookup)) return callback;
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
            if (!identity || !imported || imported->projectOrdinal != projectOrdinal ||
                (type.recordIdentity && type.recordIdentity->clangUsr && imported->clangUsr &&
                 type.recordIdentity->clangUsr != imported->clangUsr) ||
                !imported->deferredImportError.empty() || imported->isIncomplete || !imported->objectLayout ||
                !type.sizeBits || *type.sizeBits % 8 != 0 ||
                *type.sizeBits / 8 != imported->objectLayout->sizeBytes || !type.alignmentBits ||
                *type.alignmentBits % 8 != 0 ||
                *type.alignmentBits / 8 != imported->objectLayout->alignmentBytes) return std::nullopt;
            return CppScalarTypeMapping{"type:" + identity->key(), type.canonicalSpelling, type.isConst, type.isVolatile};
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
    const bool supportedAtomicTls = atomicIntTls || atomicLongTls || atomicUIntTls || atomicULongTls || atomicNarrowTls || atomicFloatTls || atomicBoolTls || atomicPointerTls || atomicEnumTls;
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
        if ((!atomicPointerTls && !atomicEnumTls && type->nativeSharpTypeKey != expected) ||
            (atomicEnumTls && !type->nativeSharpTypeKey.starts_with("type:")) ||
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
    // Forward declarations can precede a definition in another translation unit.
    // They still carry identity and target information that must not be ignored.
    std::map<std::string, std::pair<std::string, std::optional<std::string>>> observations;
    for (const auto& entry : inventory)
    {
        const auto& value = entry.declaration;
        if (entry.projectOrdinal != projectOrdinal || value.isCxx || value.name.empty()) continue;
        const compilation::TypeIdentity identity(std::string(moduleName), {}, {}, value.name);
        if (!identity.isValid() || !requested.contains(identity.key())) continue;
        if (entry.targetTriple.empty())
            return CppCEnumSnapshotFailure{"C enum observation has no target: " + identity.key()};
        auto [it, inserted] = observations.try_emplace(identity.key(), entry.targetTriple, std::nullopt);
        if ((!inserted && it->second.first != entry.targetTriple) ||
            !reconcileUsr(it->second.second, value.clangUsr))
            return CppCEnumSnapshotFailure{"conflicting C enum observations: " + identity.key()};
    }
    for (const auto& entry : inventory)
    {
        const auto& value = entry.declaration;
        if (entry.projectOrdinal != projectOrdinal || value.isCxx || value.name.empty()) continue;
        const compilation::TypeIdentity identity(std::string(moduleName), {}, {}, value.name);
        if (!identity.isValid() || !requested.contains(identity.key())) continue;
        if (!value.complete) continue;
        if (!value.underlyingType || !value.sizeBits || !value.alignmentBits ||
            !*value.sizeBits || !*value.alignmentBits)
            return CppCEnumSnapshotFailure{"incomplete C enum observation: " + identity.key()};
        const auto [it, inserted] = result.emplace(identity.key(), CppCEnumSnapshot{identity.key(), entry.targetTriple, value});
        if (inserted) continue;
        auto& prior = it->second.declaration;
        bool same = it->second.targetTriple == entry.targetTriple && prior.scoped == value.scoped &&
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
        if (!same) return CppCEnumSnapshotFailure{"conflicting C enum observations: " + identity.key()};
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
        if (value.isCxx || value.name.empty() || !value.complete || !value.underlyingType ||
            !value.sizeBits || !value.alignmentBits || !value.location ||
            entry.projectOrdinal >= graph.projects().size()) continue;
        const Key key{entry.projectOrdinal, value.name};
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
            bool same = where->second->targetTriple == entry.targetTriple &&
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
        compilation::TypeIdentity identity(graph.projects()[key.first].moduleName(), {}, {}, value.name);
        if (!identity.isValid()) { error = "invalid C enum identity: " + value.name; return false; }
        frontend::CppImportedTypeDeclaration type{std::move(identity), frontend::NamedTypeDeclarationKind::Enum,
            frontend::NamedTypeAccessibility::Public, key.first, value.location->file.generic_string()};
        type.objectLayout = frontend::CppImportedObjectLayout{*value.sizeBits / 8, *value.alignmentBits / 8};
        type.enumBase = frontend::CppImportedEnumBase{base->token, std::string(base->name)};
        if (!conflicts.contains(key))
        {
            type.origin = frontend::CppImportedTypeOrigin::CEnum;
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

CppSemanticEnumConstantResult projectCppEnumConstant(
    const CppSemanticEnumEntry& entry, std::size_t enumeratorIndex)
{
    if (entry.declaration.isCxx || !entry.declaration.complete ||
        enumeratorIndex >= entry.declaration.enumerators.size())
        return CppSemanticEnumConstantFailure{"enum constant is unavailable for C projection"};
    const auto& value = entry.declaration.enumerators[enumeratorIndex];
    const auto& type = value.valueType;
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
    std::set<std::pair<std::size_t, std::string>> seen;
    std::vector<frontend::NamespaceStateSymbol> result;
    for (const auto& demand : demands)
        for (const auto owner : demand.visibleProjects)
        {
            if (owner >= graph.projects().size() || demand.lookupName.empty() ||
                std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), std::string{}) ==
                    demand.lookupNamespaces.end() ||
                !seen.emplace(owner, demand.lookupName).second) continue;
            std::optional<CppSemanticEnumConstant> selected;
            std::optional<std::string> knownUsr;
            std::optional<std::string> targetTriple;
            std::string failure;
            bool found = false;
            for (const auto& entry : entries)
            {
                if (entry.projectOrdinal != owner || entry.declaration.isCxx) continue;
                for (std::size_t i = 0; i < entry.declaration.enumerators.size(); ++i)
                {
                    if (entry.declaration.enumerators[i].name != demand.lookupName) continue;
                    found = true;
                    if (targetTriple && *targetTriple != entry.targetTriple)
                        failure = "conflicting C enumerator target triples";
                    else targetTriple = entry.targetTriple;
                    auto projected = projectCppEnumConstant(entry, i);
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
                state.identityKey = "state:" + state.moduleName + "::." + state.name;
                state.storageName = "__nsc_cenum_" + std::to_string(state.moduleName.size()) + "_" +
                    state.moduleName + "_" + std::to_string(state.name.size()) + "_" + state.name;
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
                          std::move(entry.targetTriple), std::move(entry.declaration)});
    output = std::move(result);
    error.clear();
    return true;
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
                result.push_back({group.projectFile, index, *owner, target, std::move(declaration)});
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
    const std::string input = projectFile.lexically_normal().generic_string() + "\x1f" +
        std::to_string(referenceIndex) + "\x1f" + std::string(name);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(input.data());
    return "__nsc_c_inline_" + basic::sha256Hex(basic::sha256({bytes, input.size()}));
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
            for (auto& declaration : *declarations)
            {
                if (declaration.isCStaticInlineDefinition && !declaration.externalLinkage)
                    declaration.externalSymbol = cppCInlineLinkSymbol(group.projectFile, index, declaration.name);
                auto projection = projectCppSemanticFunction(
                    declaration, *owner, graph.projects()[*owner].moduleName());
                result.push_back({group.projectFile, index, *owner, options.targetTriple,
                                  std::move(declaration), std::move(projection)});
            }
        }
    }
    return result;
}
}
