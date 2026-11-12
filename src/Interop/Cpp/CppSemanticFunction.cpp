#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Interop/CppSemanticEnumType.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Frontend/ObjectConstructionBinding.h"
#include "nativesharp/Frontend/ClassDestructionPlan.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "../Internal/CppCRecordMetadataCodec.h"

#include <algorithm>
#include <unordered_map>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

namespace nativesharp::interop
{
namespace
{
using RecordIndex = std::unordered_map<std::string, const CppRecordGroup*>;

CppType invocationTypeForC(const CppType& type, bool allowAggregate)
{
    CppType mapped = type;
    if (mapped.kind != CppTypeKind::Pointer || !mapped.element || !mapped.element->isVolatile)
        return mapped;
    const auto kind = mapped.element->kind;
    const bool scalar = kind == CppTypeKind::Void || kind == CppTypeKind::Boolean ||
        kind == CppTypeKind::SignedInteger || kind == CppTypeKind::UnsignedInteger ||
        kind == CppTypeKind::Floating;
    const bool aggregate = kind == CppTypeKind::Record || kind == CppTypeKind::Enumeration;
    if (!scalar && !(allowAggregate && aggregate)) return mapped;
    auto element = std::make_shared<CppType>(*mapped.element);
    element->isVolatile = false;
    mapped.element = std::move(element);
    return mapped;
}

// EN: T08-D22: a C record passed or returned by value through a C function pointer: the imported C struct or
//     union with a value layout of the same size and alignment, as a direct C call takes it (T08-D09).
// ES: T08-D22: un record C pasado o devuelto por valor a traves de un puntero a funcion C: el struct o union C
//     importado con layout de valor del mismo tamano y alineacion, como lo toma una llamada C directa (T08-D09).
std::optional<CppScalarTypeMapping> cCallbackRecordValue(const CppType& type, std::string_view moduleName,
    const frontend::ProjectNameLookup* lookup, std::optional<std::size_t> owner, bool allowCppRecordValue = false)
{
    if (!lookup || !owner || type.kind != CppTypeKind::Record || type.isConst || type.isVolatile ||
        type.canonicalSpelling.empty() || !type.sizeBits || !type.alignmentBits || *type.sizeBits % 8 != 0 ||
        *type.alignmentBits % 8 != 0) return std::nullopt;
    const auto identity = projectCppRecordTypeIdentity(type, moduleName);
    const auto ordinal = identity ? lookup->typeOrdinalForIdentity(identity->key()) : std::nullopt;
    const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
    const bool reconciled = identity && lookup->isReconciledCppTypeIdentity(identity->key());
    const bool cRecord = imported && !allowCppRecordValue && (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
                                      imported->origin == frontend::CppImportedTypeOrigin::CUnion);
    const bool cppRecord = imported && allowCppRecordValue && !reconciled && ordinal &&
        lookup->cppTrivialValueEligible(*ordinal);
    if (!imported || (!reconciled && imported->projectOrdinal != *owner) || !imported->objectLayout ||
        !imported->deferredImportError.empty() || imported->isIncomplete ||
        (!cRecord && !cppRecord) ||
        imported->objectLayout->sizeBytes != *type.sizeBits / 8 ||
        imported->objectLayout->alignmentBytes != *type.alignmentBits / 8 ||
        (type.recordIdentity && type.recordIdentity->clangUsr && imported->clangUsr &&
         type.recordIdentity->clangUsr != imported->clangUsr)) return std::nullopt;
    return CppScalarTypeMapping{"type:" + imported->identity.key(), type.canonicalSpelling, false, false};
}

std::optional<CppScalarTypeMapping> mapFunctionType(
    const CppType& type, bool allowReadonlyPointee = false, const RecordIndex* records = nullptr,
    std::string_view moduleName = {}, std::size_t depth = 0,
    const frontend::ProjectNameLookup* lookup = nullptr, std::optional<std::size_t> owner = std::nullopt,
    bool allowCRecordValue = false, bool allowCStoredRecordPointer = false, bool allowCppRecordValue = false)
{
    if (depth > 64) return std::nullopt;
    if (type.kind == CppTypeKind::Array && depth > 0 && allowCRecordValue)
    {
        const CppType* element = &type;
        std::vector<std::uint64_t> dimensions;
        bool readonly = false;
        while (element && element->kind == CppTypeKind::Array)
        {
            if (element->isVolatile || !element->arrayCount || *element->arrayCount == 0 ||
                !element->element || dimensions.size() >= 64) return std::nullopt;
            readonly |= element->isConst;
            dimensions.push_back(*element->arrayCount);
            element = element->element.get();
        }
        if (!element || element->isVolatile || element->kind == CppTypeKind::Pointer ||
            element->kind == CppTypeKind::Void) return std::nullopt;
        readonly |= element->isConst;
        CppType unqualified = *element;
        unqualified.isConst = false;
        std::optional<CppScalarTypeMapping> mapped;
        if (element->kind == CppTypeKind::Record || element->kind == CppTypeKind::Enumeration)
        {
            if (!lookup || !owner || !element->sizeBits || !element->alignmentBits ||
                *element->sizeBits % 8 || *element->alignmentBits % 8) return std::nullopt;
            mapped = mapFunctionType(unqualified, false, records, moduleName, depth + 1, lookup, owner,
                                     allowCRecordValue, allowCStoredRecordPointer);
            if (!mapped || !mapped->nativeSharpTypeKey.starts_with("type:")) return std::nullopt;
            const auto ordinal = lookup->typeOrdinalForIdentity(mapped->nativeSharpTypeKey.substr(5));
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            if (!imported || !imported->objectLayout || !imported->deferredImportError.empty() ||
                (imported->projectOrdinal != *owner &&
                 !lookup->isReconciledCppTypeIdentity(mapped->nativeSharpTypeKey.substr(5))) ||
                imported->objectLayout->sizeBytes != *element->sizeBits / 8 ||
                imported->objectLayout->alignmentBytes != *element->alignmentBits / 8) return std::nullopt;
        }
        else mapped = mapCppScalarType(unqualified);
        if (!mapped) return std::nullopt;
        auto key = mapped->nativeSharpTypeKey;
        for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
            key += "[" + std::to_string(*dimension) + "]";
        return CppScalarTypeMapping{std::move(key), type.canonicalSpelling, readonly, false};
    }
    if (type.kind == CppTypeKind::Enumeration && lookup && !type.isVolatile)
    {
        return owner ? projectCppSemanticEnumType(type, *owner, moduleName, lookup) : std::nullopt;
    }
    if (type.kind == CppTypeKind::Record)
    {
        if (!records && (allowCStoredRecordPointer || allowCppRecordValue) && depth == 2 && lookup && owner &&
            !type.isConst && !type.isVolatile && !type.canonicalSpelling.empty())
        {
            const auto identity = projectCppRecordTypeIdentity(type, moduleName);
            const auto ordinal = identity ? lookup->typeOrdinalForIdentity(identity->key()) : std::nullopt;
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            const bool reconciled = identity && lookup->isReconciledCppTypeIdentity(identity->key());
            const bool cRecord = imported && allowCStoredRecordPointer &&
                (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
                 imported->origin == frontend::CppImportedTypeOrigin::CUnion);
            const bool cppRecord = imported && allowCppRecordValue && ordinal &&
                imported->origin == frontend::CppImportedTypeOrigin::CppRecord &&
                lookup->cppTrivialValueEligible(*ordinal) && !imported->isIncomplete && imported->objectLayout &&
                type.recordIdentity && type.recordIdentity->clangUsr &&
                imported->clangUsr == type.recordIdentity->clangUsr && type.sizeBits && type.alignmentBits &&
                *type.sizeBits % 8 == 0 && *type.alignmentBits % 8 == 0 &&
                imported->objectLayout->sizeBytes == *type.sizeBits / 8 &&
                imported->objectLayout->alignmentBytes == *type.alignmentBits / 8;
            if (!imported || (!reconciled && imported->projectOrdinal != *owner) ||
                !imported->deferredImportError.empty() ||
                (!cRecord && !cppRecord) ||
                (type.recordIdentity && type.recordIdentity->clangUsr && imported->clangUsr &&
                 type.recordIdentity->clangUsr != imported->clangUsr)) return std::nullopt;
            return CppScalarTypeMapping{"type:" + imported->identity.key(), type.canonicalSpelling, false, false};
        }
        if (!records || type.isVolatile || type.canonicalSpelling.empty()) return std::nullopt;
        const auto identity = projectCppRecordTypeIdentity(type, moduleName);
        if (!identity) return std::nullopt;
        const auto found = records->find(identity->key());
        if (found == records->end() || !found->second ||
            (found->second->status != CppRecordGroupStatus::ForwardOnly &&
             found->second->status != CppRecordGroupStatus::OneDefinition &&
             found->second->status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible)) return std::nullopt;
        if (type.recordIdentity && type.recordIdentity->clangUsr && found->second->clangUsr &&
            type.recordIdentity->clangUsr != found->second->clangUsr) return std::nullopt;
        std::string projectedIdentity = identity->key();
        const bool reconciled = lookup && lookup->isReconciledCppTypeIdentity(identity->key());
        if (reconciled)
        {
            const auto ordinal = lookup->typeOrdinalForIdentity(identity->key());
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            if (!imported || (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
                              imported->origin != frontend::CppImportedTypeOrigin::CUnion)) return std::nullopt;
            projectedIdentity = imported->identity.key();
        }
        if (depth == 0)
        {
            // EN: T08-D09: a C record defined equivalently in several headers is passed by value once its imported
            //     type has a value layout (checked below); otherwise only one definition qualifies.
            // ES: T08-D09: un record C definido de forma equivalente en varios encabezados se pasa por valor si su
            //     tipo importado tiene layout de valor (se comprueba abajo); si no, solo vale una definicion.
            if ((found->second->status != CppRecordGroupStatus::OneDefinition &&
                 found->second->status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible) ||
                !lookup || !owner ||
                !type.sizeBits || !type.alignmentBits || *type.sizeBits % 8 || *type.alignmentBits % 8)
                return std::nullopt;
            const auto ordinal = lookup->typeOrdinalForIdentity(identity->key());
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            const bool cRecord = imported && allowCRecordValue &&
                (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
                 imported->origin == frontend::CppImportedTypeOrigin::CUnion);
            const bool cppTrivialRecord = imported && !allowCRecordValue && allowCppRecordValue && ordinal &&
                lookup->cppTrivialValueEligible(*ordinal);
            if (!imported || (!reconciled && imported->projectOrdinal != *owner) || !imported->objectLayout ||
                !imported->deferredImportError.empty() ||
                (!cRecord && !cppTrivialRecord) ||
                imported->objectLayout->sizeBytes != *type.sizeBits / 8 ||
                imported->objectLayout->alignmentBytes != *type.alignmentBits / 8)
                return std::nullopt;
        }
        if (lookup && type.recordIdentity && type.recordIdentity->clangUsr)
        {
            const auto ordinal = lookup->typeOrdinalForIdentity(identity->key());
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            if (imported && imported->clangUsr && imported->clangUsr != type.recordIdentity->clangUsr)
                return std::nullopt;
        }
        return CppScalarTypeMapping{"type:" + projectedIdentity, type.canonicalSpelling, false, false};
    }
    if (type.kind == CppTypeKind::Pointer && type.element && type.element->kind == CppTypeKind::Function)
    {
        if (type.isConst || type.isVolatile || type.sizeBits != 64 || type.canonicalSpelling.empty() ||
            !type.element->function || !type.element->function->hasPrototype) return std::nullopt;
        const auto& signature = *type.element->function;
        language::FunctionType function;
        if (signature.callingConvention == "cdecl") function.callingConvention = language::CallingConvention::C;
        else if (signature.callingConvention == "stdcall") function.callingConvention = language::CallingConvention::StdCall;
        else if (signature.callingConvention == "fastcall") function.callingConvention = language::CallingConvention::FastCall;
        else if (signature.callingConvention == "vectorcall") function.callingConvention = language::CallingConvention::VectorCall;
        else if (signature.callingConvention == "ms_abi") function.callingConvention = language::CallingConvention::Win64;
        else if (signature.callingConvention == "sysv_abi") function.callingConvention = language::CallingConvention::SysV;
        else if (signature.callingConvention == "regcall") function.callingConvention = language::CallingConvention::RegCall;
        else if (signature.callingConvention == "preserve_most") function.callingConvention = language::CallingConvention::PreserveMost;
        else if (signature.callingConvention == "preserve_all") function.callingConvention = language::CallingConvention::PreserveAll;
        else return std::nullopt;
        // C records retain T08-D22; C++ records require the verified trivial-value rule.
        const bool cRecordValues = allowCRecordValue || allowCStoredRecordPointer;
        const bool cppRecordValues = !cRecordValues &&
            (signature.callingConvention == "cdecl" || signature.callingConvention == "preserve_most" ||
             signature.callingConvention == "regcall" || signature.callingConvention == "sysv_abi");
        if (signature.returnType.kind == CppTypeKind::Record && !cRecordValues && !cppRecordValues) return std::nullopt;
        // EN: T08-002: the structural identity records `const` and `volatile` pointees per parameter and for
        //     the result, so a pointer to such data is mapped like a direct C call maps it (the carrier
        //     without the qualifier) and the qualifier becomes an identity flag instead of a rejection.
        // ES: T08-002: la identidad estructural registra datos apuntados `const` y `volatile` por parámetro y
        //     para el resultado, así que un puntero a esos datos se mapea como lo hace una llamada C directa
        //     (el portador sin calificador) y el calificador pasa a ser una marca de la identidad en lugar
        //     de un rechazo.
        const auto qualifiable = [](const CppType& slot)
        {
            return slot.kind == CppTypeKind::Pointer && slot.element &&
                slot.element->kind != CppTypeKind::Pointer && slot.element->kind != CppTypeKind::Function &&
                slot.element->kind != CppTypeKind::Array;
        };
        const auto pointeeConst = [&](const CppType& slot) { return qualifiable(slot) && slot.element->isConst; };
        const auto pointeeVolatile = [&](const CppType& slot)
        {
            return qualifiable(slot) && slot.element->isVolatile;
        };
        // EN: The carrier is mapped without the pointee qualifiers, which become identity flags below.
        // ES: El portador se mapea sin los calificadores del dato apuntado, que abajo pasan a ser marcas.
        const auto carrier = [&](const CppType& slot)
        {
            if (!pointeeConst(slot) && !pointeeVolatile(slot)) return slot;
            CppType unqualified = slot;
            auto element = std::make_shared<CppType>(*slot.element);
            element->isConst = false;
            element->isVolatile = false;
            unqualified.element = std::move(element);
            return unqualified;
        };
        const auto result = signature.returnType.kind == CppTypeKind::Record
            ? cCallbackRecordValue(signature.returnType, moduleName, lookup, owner, cppRecordValues)
            : mapFunctionType(carrier(signature.returnType), false, records, moduleName, depth + 1,
                              lookup, owner, false, allowCStoredRecordPointer, cppRecordValues);
        if (!result || result->isConst || result->isVolatile) return std::nullopt;
        function.result = result->nativeSharpTypeKey;
        function.resultPointeeReadonly = pointeeConst(signature.returnType);
        function.resultPointeeVolatile = pointeeVolatile(signature.returnType);
        for (const auto& parameter : signature.parameterTypes)
        {
            if (parameter.kind == CppTypeKind::Record && !cRecordValues && !cppRecordValues) return std::nullopt;
            const auto mapped = parameter.kind == CppTypeKind::Record
                ? cCallbackRecordValue(parameter, moduleName, lookup, owner, cppRecordValues)
                : mapFunctionType(carrier(parameter), true, records, moduleName, depth + 1, lookup,
                                  owner, false, allowCStoredRecordPointer, cppRecordValues);
            if (!mapped || mapped->isConst || mapped->isVolatile || parameter.kind == CppTypeKind::Void)
                return std::nullopt;
            function.parameters.push_back(mapped->nativeSharpTypeKey);
            function.parameterPointeeReadonly.push_back(pointeeConst(parameter));
            function.parameterPointeeVolatile.push_back(pointeeVolatile(parameter));
        }
        function.variadic = signature.variadic;
        return CppScalarTypeMapping{language::functionTypeIdentity(function), type.canonicalSpelling, false, false};
    }
    if (type.kind != CppTypeKind::Pointer) return mapCppScalarType(type);
    const bool readonlyScalar = allowReadonlyPointee && type.element && type.element->isConst &&
        type.element->kind != CppTypeKind::Pointer;
    if (type.canonicalSpelling.empty() || type.sizeBits != 64 || !type.element ||
        type.isConst || type.isVolatile || (type.element->isConst && !readonlyScalar) || type.element->isVolatile)
        return std::nullopt;
    auto element = mapFunctionType(*type.element, false, records, moduleName, depth + 1, lookup, owner,
                                   allowCRecordValue, allowCStoredRecordPointer, allowCppRecordValue);
    if (!element) return std::nullopt;
    if (type.element->kind == CppTypeKind::Array && element->isConst && !allowReadonlyPointee)
        return std::nullopt;
    return CppScalarTypeMapping{element->nativeSharpTypeKey + "*", type.canonicalSpelling,
                                readonlyScalar || (type.element->kind == CppTypeKind::Array && element->isConst), false};
}
}

std::optional<CppScalarTypeMapping> projectCppStoredFunctionPointer(
    const CppType& type, std::optional<std::size_t> projectOrdinal,
    std::string_view moduleName, const frontend::ProjectNameLookup* lookup, bool cppRecordValues)
{
    if (type.kind != CppTypeKind::Pointer || !type.element ||
        type.element->kind != CppTypeKind::Function) return std::nullopt;
    CppType unqualified = type;
    unqualified.isConst = false;
    unqualified.isVolatile = false;
    auto mapped = mapFunctionType(unqualified, false, nullptr, moduleName, 0, lookup, projectOrdinal,
                                  false, !cppRecordValues);
    if (mapped) { mapped->isConst = type.isConst; mapped->isVolatile = type.isVolatile; }
    return mapped;
}

namespace
{
CppSemanticFunctionResult projectFunction(
    const CppFunctionDeclaration& function, std::size_t projectOrdinal, std::string_view moduleName,
    const RecordIndex* records, const frontend::ProjectNameLookup* lookup = nullptr)
{
    auto failure = [](std::string message) -> CppSemanticFunctionResult {
        return CppSemanticFunctionFailure{std::move(message)};
    };
    if (moduleName.empty() || function.name.empty() || function.qualifiedName.empty() ||
        (!function.externalLinkage && !function.isCStaticInlineDefinition) ||
        !function.externalSymbol || function.externalSymbol->empty() ||
        function.callingConvention.empty())
        return failure("function requires a project, a name and an external symbol");
    if (function.variadic && (function.languageLinkage != CppLanguageLinkage::C ||
                              (function.callingConvention != "cdecl" && function.callingConvention != "sysv_abi")))
        return failure("variadic C++ or non-cdecl functions are not yet projected");
    const bool cUnprototyped = function.languageLinkage == CppLanguageLinkage::C && !function.hasPrototype;
    if (cUnprototyped && (function.callingConvention != "cdecl" || !function.parameters.empty()))
        return failure("unprototyped C function with unsupported definition or calling convention");
    const bool cLanguage = function.languageLinkage == CppLanguageLinkage::C || function.isCStaticInlineDefinition;
    const CppType mappedReturn = cLanguage
        ? invocationTypeForC(function.returnType, true) : function.returnType;
    const bool cRecordValues = cLanguage;
    const bool cppRecordValues = !cLanguage &&
        (function.callingConvention == "cdecl" || function.callingConvention == "preserve_most" ||
         function.callingConvention == "regcall" || function.callingConvention == "sysv_abi");
    const auto resultType = mapFunctionType(mappedReturn, true, records, moduleName, 0, lookup, projectOrdinal,
                                            cRecordValues, false, cppRecordValues);
    if (!resultType) return failure("C/C++ return type is not yet mapped");

    std::string cppNamespace;
    if (function.qualifiedName != function.name)
    {
        const auto separator = function.qualifiedName.rfind("::");
        if (separator == std::string::npos ||
            function.qualifiedName.substr(separator + 2) != function.name)
            return failure("C/C++ qualified name does not match its function name");
        cppNamespace = function.qualifiedName.substr(0, separator);
    }
    std::string nsNamespace;
    for (std::size_t i = 0; i < cppNamespace.size(); ++i)
    {
        if (cppNamespace[i] == ':' && i + 1 < cppNamespace.size() && cppNamespace[i + 1] == ':')
        { nsNamespace.push_back('.'); ++i; }
        else nsNamespace.push_back(cppNamespace[i]);
    }

    frontend::NamespaceFunctionSymbol symbol;
    symbol.projectOrdinal = projectOrdinal;
    symbol.moduleName = std::string(moduleName);
    symbol.namespaceName = std::move(nsNamespace);
    symbol.name = function.name;
    symbol.cppClangUsr = function.clangUsr;
    symbol.declaredTypeKey = resultType->nativeSharpTypeKey;
    symbol.mangledName = *function.externalSymbol;
    symbol.cppCallingConvention = function.callingConvention;
    symbol.cppAbiPlan = function.abiPlan;
    symbol.cppReturnType = function.returnType;
    symbol.cppDllImport = function.dllImport;
    symbol.isVariadic = function.variadic || cUnprototyped;
    symbol.cppCanonicalIdentity = *function.externalSymbol + "|" + resultType->canonicalCppType + "(";
    symbol.signatureKey = function.name + "(";
    if (function.location)
    {
        const auto encoded = function.location->file.generic_u8string();
        symbol.sourcePath.assign(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    }
    for (std::size_t index = 0; index < function.parameters.size(); ++index)
    {
        const auto& input = function.parameters[index];
        // Map only the carrier; the original Clang type retains its qualifiers below.
        const CppType mappedInput = cLanguage
            ? invocationTypeForC(input.type, true) : input.type;
        const auto parameterType = mapFunctionType(mappedInput, true, records, moduleName, 0, lookup, projectOrdinal,
                                                   cRecordValues, false, cppRecordValues);
        if (!parameterType || input.type.kind == CppTypeKind::Void)
            return failure("C/C++ parameter " + std::to_string(index) + " is not yet mapped");
        frontend::MethodParameterSymbol parameter;
        parameter.name = input.name;
        parameter.typeKey = parameterType->nativeSharpTypeKey;
        parameter.pointerPointeeReadonly = input.type.kind == CppTypeKind::Pointer &&
            input.type.element && input.type.element->isConst;
        parameter.pointerPointeeVolatile = input.type.kind == CppTypeKind::Pointer &&
            input.type.element && input.type.element->isVolatile;
        symbol.parameters.push_back(std::move(parameter));
        symbol.cppParameterTypes.push_back(input.type);
        if (index != 0) { symbol.signatureKey.push_back(','); symbol.cppCanonicalIdentity.push_back(','); }
        symbol.signatureKey += parameterType->nativeSharpTypeKey;
        symbol.cppCanonicalIdentity += parameterType->canonicalCppType;
    }
    if (symbol.isVariadic)
    {
        if (!function.parameters.empty()) { symbol.signatureKey.push_back(','); symbol.cppCanonicalIdentity.push_back(','); }
        symbol.signatureKey += "...";
        symbol.cppCanonicalIdentity += "...";
    }
    symbol.signatureKey.push_back(')');
    symbol.cppCanonicalIdentity.push_back(')');
    symbol.callableIdentityKey = "cpp:" + symbol.moduleName + "::" + symbol.cppCanonicalIdentity;
    return symbol;
}
}

CppSemanticFunctionResult projectCppSemanticFunction(
    const CppFunctionDeclaration& function, std::size_t projectOrdinal, std::string_view moduleName)
{
    return projectFunction(function, projectOrdinal, moduleName, nullptr);
}

CppSemanticFunctionResult projectCppSemanticFunctionWithRecords(
    const CppFunctionDeclaration& function, std::size_t projectOrdinal, std::string_view moduleName,
    const CppRecordGroups& records, const frontend::ProjectNameLookup* lookup)
{
    RecordIndex index;
    for (const auto& group : records.groups)
    {
        if (group.projectOrdinal != projectOrdinal) continue;
        if (!index.emplace(group.identity.key(), &group).second)
            return CppSemanticFunctionFailure{"duplicate record group for project"};
    }
    return projectFunction(function, projectOrdinal, moduleName, &index, lookup);
}

CppSemanticFunctionResult projectCppPrecompiledCFunction(
    const metadata_detail::CDependencyFunctionSnapshot& function,
    std::span<const CppCRecordSnapshot> records, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup)
{
    if (targetTriple != "x86_64-pc-windows-msvc" || !lookup.isBuilt() ||
        projectOrdinal >= lookup.modules().size() || !lookup.modules()[projectOrdinal] ||
        lookup.modules()[projectOrdinal]->moduleName() != moduleName ||
        moduleName.empty() || function.moduleName != moduleName || function.referenceIdentity.empty() ||
        function.linkSymbol.empty() || function.declaration.externalSymbol != function.linkSymbol ||
        !function.declaration.clangUsr || function.declaration.clangUsr->empty())
        return CppSemanticFunctionFailure{"precompiled C function has invalid owner, target or identity"};
    CppRecordGroups groups;
    std::set<std::string> identities;
    for (const auto& record : records)
    {
        if (record.moduleName != moduleName) continue;
        std::vector<compilation::TypeNameSegment> containing;
        for (const auto& name : record.containingRecords) containing.push_back({name, 0});
        compilation::TypeIdentity identity(record.moduleName, record.namespaceSegments, containing, record.name);
        if (!identity.isValid() || identity.key() != record.identityKey || record.targetTriple != targetTriple ||
            !identities.insert(record.identityKey).second)
            return CppSemanticFunctionFailure{"precompiled C function has invalid record closure"};
        groups.groups.push_back({projectOrdinal, std::move(identity), {},
            record.complete ? CppRecordGroupStatus::OneDefinition : CppRecordGroupStatus::ForwardOnly,
            record.clangUsr});
    }
    return projectCppSemanticFunctionWithRecords(function.declaration, projectOrdinal, moduleName, groups, &lookup);
}

CppSemanticInstanceMethodResult projectCppSemanticInstanceMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const CppRecordGroups* records)
{
    auto fail = [](std::string message) -> CppSemanticInstanceMethodResult {
        return CppSemanticFunctionFailure{std::move(message)};
    };
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || methodIndex >= record.methods.size())
        return fail("instance method requires a complete record and supported target");
    const auto identity = projectCppRecordIdentity(record, moduleName);
    if (!identity) return fail("instance method owner has no supported identity");
    const auto& method = record.methods[methodIndex];
    if (method.kind != CppMethodKind::Method || method.name.empty() ||
        method.name.starts_with("operator") || method.qualifiedName != record.qualifiedName + "::" + method.name ||
        method.access != CppAccess::Public || method.isStatic || method.isVirtual || method.isPureVirtual ||
        method.isDeleted || method.isVolatile || method.variadic ||
        method.refQualifier != CppMethodRefQualifier::None || method.callingConvention != "cdecl" ||
        !method.externalSymbol || method.externalSymbol->empty() || method.canonicalFunctionType.empty())
        return fail("C++ instance method contract is not yet mapped");
    RecordIndex recordIndex;
    if (records) for (const auto& group : records->groups)
        if (group.projectOrdinal == projectOrdinal && !recordIndex.emplace(group.identity.key(), &group).second)
            return fail("duplicate record group for project");
    auto result = mapFunctionType(method.returnType, true, records ? &recordIndex : nullptr, moduleName);
    if (!result) return fail("C++ instance method return type is not yet mapped");
    std::vector<CppScalarTypeMapping> parameters;
    parameters.reserve(method.parameters.size());
    for (const auto& parameter : method.parameters)
    {
        auto mapped = mapFunctionType(parameter.type, true, records ? &recordIndex : nullptr, moduleName);
        if (!mapped || parameter.type.kind == CppTypeKind::Void)
            return fail("C++ instance method parameter type is not yet mapped");
        parameters.push_back(std::move(*mapped));
    }
    return CppSemanticInstanceMethod{projectOrdinal, identity->key(), method.qualifiedName,
        *method.externalSymbol, method.canonicalFunctionType, method.isConst,
        std::move(*result), std::move(parameters), method.dllImport};
}

frontend::CppImportedInstanceMethod toCppImportedInstanceMethod(const CppSemanticInstanceMethod& method)
{
    const auto separator = method.qualifiedName.rfind("::");
    frontend::CppImportedInstanceMethod result{method.projectOrdinal, method.ownerIdentityKey,
        separator == std::string::npos ? std::string{} : method.qualifiedName.substr(separator + 2),
        method.externalSymbol, method.result.nativeSharpTypeKey, {}, method.receiverConst, method.dllImport, false, {}};
    result.returnPointeeReadonly = method.result.nativeSharpTypeKey.ends_with('*') && method.result.isConst;
    for (const auto& parameter : method.parameters)
    {
        result.parameterTypeKeys.push_back(parameter.nativeSharpTypeKey);
        result.parameterPointeeReadonly.push_back(parameter.nativeSharpTypeKey.ends_with('*') && parameter.isConst);
    }
    return result;
}

CppSemanticStaticMethodResult projectCppSemanticStaticMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const CppRecordGroups* records)
{
    auto fail = [](std::string message) -> CppSemanticStaticMethodResult {
        return CppSemanticFunctionFailure{std::move(message)};
    };
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || methodIndex >= record.methods.size())
        return fail("static method requires a complete record and supported target");
    const auto identity = projectCppRecordIdentity(record, moduleName);
    if (!identity) return fail("static method owner has no supported identity");
    const auto& method = record.methods[methodIndex];
    if (method.kind != CppMethodKind::Method || method.name.empty() || method.name.starts_with("operator") ||
        method.qualifiedName != record.qualifiedName + "::" + method.name || method.access != CppAccess::Public ||
        !method.isStatic || method.isVirtual || method.isPureVirtual || method.isDeleted || method.isVolatile ||
        method.variadic || method.refQualifier != CppMethodRefQualifier::None ||
        method.callingConvention != "cdecl" || !method.externalSymbol || method.externalSymbol->empty() ||
        method.canonicalFunctionType.empty()) return fail("C++ static method contract is not yet mapped");
    RecordIndex recordIndex;
    if (records) for (const auto& group : records->groups)
        if (group.projectOrdinal == projectOrdinal && !recordIndex.emplace(group.identity.key(), &group).second)
            return fail("duplicate record group for project");
    auto result = mapFunctionType(method.returnType, true, records ? &recordIndex : nullptr, moduleName);
    if (!result) return fail("C++ static method return type is not yet mapped");
    std::vector<CppScalarTypeMapping> parameters;
    for (const auto& parameter : method.parameters)
    {
        auto mapped = mapFunctionType(parameter.type, true, records ? &recordIndex : nullptr, moduleName);
        if (!mapped || parameter.type.kind == CppTypeKind::Void)
            return fail("C++ static method parameter type is not yet mapped");
        parameters.push_back(std::move(*mapped));
    }
    return CppSemanticStaticMethod{projectOrdinal, identity->key(), method.qualifiedName,
        *method.externalSymbol, method.canonicalFunctionType, std::move(*result), std::move(parameters), method.dllImport};
}

frontend::CppImportedStaticMethod toCppImportedStaticMethod(const CppSemanticStaticMethod& method)
{
    const auto separator = method.qualifiedName.rfind("::");
    frontend::CppImportedStaticMethod result{method.projectOrdinal, method.ownerIdentityKey,
        separator == std::string::npos ? method.qualifiedName : method.qualifiedName.substr(separator + 2),
        method.externalSymbol, method.result.nativeSharpTypeKey, {}, method.dllImport, false, {}};
    result.returnPointeeReadonly = method.result.nativeSharpTypeKey.ends_with('*') && method.result.isConst;
    for (const auto& parameter : method.parameters)
    {
        result.parameterTypeKeys.push_back(parameter.nativeSharpTypeKey);
        result.parameterPointeeReadonly.push_back(parameter.nativeSharpTypeKey.ends_with('*') && parameter.isConst);
    }
    return result;
}

CppSemanticLifetimeMethodResult projectCppSemanticLifetimeMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple)
{
    auto fail = [](std::string message) -> CppSemanticLifetimeMethodResult {
        return CppSemanticFunctionFailure{std::move(message)};
    };
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || !record.bases.empty() ||
        methodIndex >= record.methods.size()) return fail("lifetime method requires a complete unbased record and supported target");
    const auto identity = projectCppRecordIdentity(record, moduleName);
    if (!identity) return fail("lifetime method owner has no supported identity");
    const auto& method = record.methods[methodIndex];
    if ((method.kind != CppMethodKind::Constructor && method.kind != CppMethodKind::Destructor) ||
        method.access != CppAccess::Public || method.isStatic || method.isVirtual || method.isPureVirtual ||
        method.isDeleted || method.isVolatile || method.variadic ||
        method.refQualifier != CppMethodRefQualifier::None || method.callingConvention != "cdecl" ||
        !method.externalSymbol || method.externalSymbol->empty() || method.canonicalFunctionType.empty() ||
        (method.kind == CppMethodKind::Destructor && !method.parameters.empty()))
        return fail("C++ lifetime method contract is not yet mapped");
    std::vector<CppScalarTypeMapping> parameters;
    for (const auto& parameter : method.parameters)
    {
        auto mapped = mapFunctionType(parameter.type, true);
        if (!mapped || parameter.type.kind == CppTypeKind::Void)
            return fail("C++ lifetime method parameter type is not yet mapped");
        parameters.push_back(std::move(*mapped));
    }
    return CppSemanticLifetimeMethod{projectOrdinal, identity->key(), method.kind, *method.externalSymbol,
                                    method.canonicalFunctionType, std::move(parameters),
                                    method.kind == CppMethodKind::Constructor
                                        ? CppLifetimeReturnAbi::ThisPointer : CppLifetimeReturnAbi::Void,
                                    method.dllImport, method.isNonThrowing};
}

frontend::CppImportedLifetimeMethod toCppImportedLifetimeMethod(const CppSemanticLifetimeMethod& method)
{
    frontend::CppImportedLifetimeMethod result{method.projectOrdinal, method.ownerIdentityKey,
        method.kind == CppMethodKind::Constructor ? frontend::CppImportedLifetimeKind::Constructor
                                                  : frontend::CppImportedLifetimeKind::Destructor,
        method.externalSymbol, {}, method.dllImport, method.isNonThrowing, false, false, {}};
    for (const auto& parameter : method.parameters)
    {
        result.parameterTypeKeys.push_back(parameter.nativeSharpTypeKey);
        result.parameterPointeeReadonly.push_back(parameter.nativeSharpTypeKey.ends_with('*') && parameter.isConst);
    }
    return result;
}

bool collectCppSemanticLifetimeFamilies(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<CppSemanticLifetimeFamily>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    std::vector<CppSemanticLifetimeFamily> result;
    for (const auto& group : std::get<CppRecordGroups>(refined).groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal) continue;
        const auto& entry = entries[*selected];
        CppSemanticLifetimeFamily family{group.projectOrdinal, group.identity.key(), {}, {}, false,
                                         entry.declaration.implicitLifetimeMembers, std::nullopt};
        const CppInteropSession* definingSession = nullptr;
        bool ambiguousSession = false;
        for (const auto& project : projects)
        {
            std::error_code ec;
            if (!std::filesystem::equivalent(project.projectFile, entry.projectFile, ec) || ec ||
                entry.referenceIndex >= project.sessions.size() || !project.sessions[entry.referenceIndex]) continue;
            if (definingSession) { ambiguousSession = true; break; }
            definingSession = project.sessions[entry.referenceIndex].get();
        }
        if (definingSession && !ambiguousSession && family.implicitMembers)
            family.implicitDeclarations = definingSession->implicitLifetimeDeclarations(entry.declaration.qualifiedName);
        std::set<std::pair<CppMethodKind, std::vector<std::string>>> signatures;
        for (std::size_t index = 0; index < entry.declaration.methods.size(); ++index)
        {
            const auto kind = entry.declaration.methods[index].kind;
            if (kind != CppMethodKind::Constructor && kind != CppMethodKind::Destructor) continue;
            auto projected = projectCppSemanticLifetimeMethod(entry.declaration, index, group.projectOrdinal,
                graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple);
            if (auto* method = std::get_if<CppSemanticLifetimeMethod>(&projected))
            {
                std::vector<std::string> parameters;
                for (const auto& parameter : method->parameters)
                    parameters.push_back(method->parameters.size() == 1 && parameter.isConst &&
                        parameter.nativeSharpTypeKey.ends_with('*')
                            ? "const:" + parameter.nativeSharpTypeKey : parameter.nativeSharpTypeKey);
                if (!signatures.emplace(kind, std::move(parameters)).second)
                    family.hasProjectedSignatureCollision = true;
                family.methods.push_back(std::move(*method));
            }
            else family.deferred.push_back({index, kind, std::get<CppSemanticFunctionFailure>(projected).message});
        }
        if (!family.methods.empty() || !family.deferred.empty() || family.implicitMembers)
            result.push_back(std::move(family));
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool provideExperimentalCppLifetimeMethods(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedLifetimeMethod>& output, std::string& error)
{
    std::vector<CppSemanticLifetimeFamily> families;
    if (!collectCppSemanticLifetimeFamilies(graph, projects, lookup, families, error)) return false;
    std::vector<frontend::CppImportedLifetimeMethod> result;
    for (const auto& family : families)
    {
        if (!family.deferred.empty() || family.hasProjectedSignatureCollision || !family.implicitDeclarations) continue;
        for (const auto& method : family.methods) result.push_back(toCppImportedLifetimeMethod(method));
        for (const auto& method : *family.implicitDeclarations)
        {
            if ((method.kind != CppImplicitLifetimeKind::DefaultConstructor &&
                 method.kind != CppImplicitLifetimeKind::Destructor) ||
                method.access != CppAccess::Public || method.isDeleted ||
                !method.parameters.empty() || !method.abiSymbol || method.abiSymbol->empty()) continue;
            result.push_back(frontend::CppImportedLifetimeMethod{
                family.projectOrdinal, family.ownerIdentityKey,
                method.kind == CppImplicitLifetimeKind::Destructor
                    ? frontend::CppImportedLifetimeKind::Destructor : frontend::CppImportedLifetimeKind::Constructor,
                *method.abiSymbol, {}, method.dllImport,
                method.exceptionSpecification == CppExceptionSpecification::NonThrowing, true, method.isTrivial, {}});
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

namespace
{
CppRecordGroups visibleMethodRecords(const CppRecordGroups& groups, const frontend::ProjectNameLookup& lookup)
{
    CppRecordGroups result;
    for (const auto& group : groups.groups)
    {
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (imported && imported->projectOrdinal == group.projectOrdinal) result.groups.push_back(group);
    }
    return result;
}
}

bool planCppImplicitLifetimeEmission(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::ProjectMemberModel& members,
    std::span<const std::size_t> selectedMembers,
    std::vector<CppImplicitLifetimeEmissionRequest>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    const auto groups = groupCppSemanticRecords(entries);
    std::vector<CppImplicitLifetimeEmissionRequest> planned;
    std::set<std::size_t> seen;
    for (const auto ordinal : selectedMembers)
    {
        if (!seen.insert(ordinal).second) continue;
        if (ordinal >= members.members().size())
        { error = "C++ lifetime member ordinal is invalid"; return false; }
        const auto& member = members.members()[ordinal];
        if (!member.isCppExternal || !member.cppLifetimeImplicit || !member.parameters.empty() ||
            (member.kind != frontend::ClassMemberKind::Constructor && member.kind != frontend::ClassMemberKind::Destructor) ||
            member.declaringTypeOrdinal >= lookup.namedTypes().size())
        { error = "selected member is not an implicit C++ default constructor or destructor"; return false; }
        const auto* owner = lookup.cppImportedType(member.declaringTypeOrdinal);
        if (!owner || owner->projectOrdinal >= graph.projects().size())
        { error = "selected C++ lifetime owner is unavailable"; return false; }
        const auto key = lookup.namedTypes()[member.declaringTypeOrdinal].identity.key();
        const CppRecordGroup* group = nullptr;
        for (const auto& candidate : groups.groups)
            if (candidate.projectOrdinal == owner->projectOrdinal && candidate.identity.key() == key)
            {
                if (group) { error = "ambiguous C++ lifetime record"; return false; }
                group = &candidate;
            }
        if (!group || group->status != CppRecordGroupStatus::OneDefinition)
        { error = "C++ lifetime emission requires one unambiguous definition"; return false; }
        const auto chosen = selectCppRecordMemberDefinition(entries, *group);
        if (!chosen) { error = "C++ lifetime definition is unavailable"; return false; }
        const auto& entry = entries[*chosen];
        const CppInteropSession* session = nullptr;
        for (const auto& project : projects)
        {
            std::error_code ec;
            if (!std::filesystem::equivalent(project.projectFile, entry.projectFile, ec) || ec ||
                entry.referenceIndex >= project.sessions.size() || !project.sessions[entry.referenceIndex]) continue;
            if (session) { error = "ambiguous C++ lifetime session"; return false; }
            session = project.sessions[entry.referenceIndex].get();
        }
        if (!session) { error = "C++ lifetime session is unavailable"; return false; }
        const auto kind = member.kind == frontend::ClassMemberKind::Constructor
            ? CppImplicitLifetimeKind::DefaultConstructor : CppImplicitLifetimeKind::Destructor;
        const auto declarations = session->implicitLifetimeDeclarations(entry.declaration.qualifiedName);
        if (!declarations || std::none_of(declarations->begin(), declarations->end(), [&](const auto& candidate) {
            return candidate.kind == kind && !candidate.isDeleted && candidate.access == CppAccess::Public &&
                candidate.abiSymbol && *candidate.abiSymbol == member.cppExternalSymbol;
        }))
        { error = "C++ lifetime declaration no longer matches the selected symbol"; return false; }
        planned.push_back({owner->projectOrdinal, ordinal, entry.declaration.qualifiedName,
                           member.cppExternalSymbol, kind, session});
    }
    output = std::move(planned);
    error.clear();
    return true;
}

bool planCppImplicitLifetimeFromBindings(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::ProjectMemberModel& members,
    std::span<const frontend::ObjectConstructionBinding> constructions,
    std::span<const frontend::ClassDestructionPlan> destructions,
    std::vector<CppImplicitLifetimeEmissionRequest>& output, std::string& error)
{
    std::vector<std::size_t> selected;
    auto add = [&](std::size_t ordinal, frontend::ClassMemberKind kind) {
        if (ordinal >= members.members().size() || members.members()[ordinal].kind != kind)
        { error = "lifetime binding refers to an invalid member"; return false; }
        const auto& member = members.members()[ordinal];
        if (member.isCppExternal && member.cppLifetimeImplicit) selected.push_back(ordinal);
        return true;
    };
    for (const auto& binding : constructions)
    {
        if (binding.targetTypeOrdinal >= lookup.namedTypes().size())
        { error = "construction binding has an invalid target"; return false; }
        if (!lookup.cppImportedType(binding.targetTypeOrdinal)) continue;
        if (binding.resolution != frontend::ConstructionResolutionKind::ResolvedExplicit ||
            !binding.selectedConstructorMemberOrdinal ||
            *binding.selectedConstructorMemberOrdinal >= members.members().size() ||
            members.members()[*binding.selectedConstructorMemberOrdinal].declaringTypeOrdinal != binding.targetTypeOrdinal ||
            !members.members()[*binding.selectedConstructorMemberOrdinal].isCppExternal)
        { error = "C++ construction has no resolved matching constructor"; return false; }
        if (!add(*binding.selectedConstructorMemberOrdinal, frontend::ClassMemberKind::Constructor)) return false;
    }
    for (const auto& plan : destructions)
    {
        if (plan.typeOrdinal >= lookup.namedTypes().size())
        { error = "destruction plan has an invalid target"; return false; }
        if (!lookup.cppImportedType(plan.typeOrdinal)) continue;
        if (!plan.explicitDestructorMemberOrdinal && plan.destructorMemberOrdinals.empty())
        { error = "C++ destruction plan has no selected destructor"; return false; }
        if (plan.explicitDestructorMemberOrdinal &&
            (*plan.explicitDestructorMemberOrdinal >= members.members().size() ||
             members.members()[*plan.explicitDestructorMemberOrdinal].declaringTypeOrdinal != plan.typeOrdinal ||
             !members.members()[*plan.explicitDestructorMemberOrdinal].isCppExternal))
        { error = "C++ destruction has no matching destructor"; return false; }
        if (plan.explicitDestructorMemberOrdinal &&
            !add(*plan.explicitDestructorMemberOrdinal, frontend::ClassMemberKind::Destructor)) return false;
        for (const auto ordinal : plan.destructorMemberOrdinals)
        {
            if (ordinal >= members.members().size() ||
                members.members()[ordinal].declaringTypeOrdinal != plan.typeOrdinal ||
                !members.members()[ordinal].isCppExternal)
            { error = "C++ destruction chain contains a mismatched destructor"; return false; }
            if (!add(ordinal, frontend::ClassMemberKind::Destructor)) return false;
        }
    }
    return planCppImplicitLifetimeEmission(graph, projects, lookup, members, selected, output, error);
}

bool planCppImplicitLifetimeFromValues(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::ProjectMemberModel& members,
    std::span<const frontend::ObjectConstructionBinding> constructions,
    const frontend::ProjectOopMirModel& mir,
    std::vector<CppImplicitLifetimeEmissionRequest>& output, std::string& error)
{
    if (!mir.isBuilt()) { error = "C++ lifetime planning requires built MIR"; return false; }
    std::vector<std::size_t> selected;
    std::vector<const frontend::ProjectOopMirNode*> pending;
    for (const auto& function : mir.functions()) if (function.body) pending.push_back(function.body.get());
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        for (const auto& child : node->children) if (child) pending.push_back(child.get());
        const bool discarded = node->kind == frontend::ProjectOopMirNodeKind::ExpressionStatement;
        const bool local = node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration;
        if ((!discarded && !local) || node->children.size() != 1) continue;
        const auto* value = node->children.front().get();
        if (!value || value->kind != frontend::ProjectOopMirNodeKind::ValueConstruct ||
            !value->targetTypeOrdinal || !lookup.cppImportedType(*value->targetTypeOrdinal)) continue;
        if (local && (!node->localOrdinal || !node->cleanupRegionOrdinal || !node->automaticDestruction ||
            node->staticLocal || node->referenceLocal || node->typeKey != value->typeKey))
        { error = "C++ local lifetime has incomplete cleanup or type identity"; return false; }
        if (!value->constructionOrdinal || *value->constructionOrdinal >= constructions.size() || !value->memberOrdinal ||
            *value->memberOrdinal >= members.members().size())
        { error = "discarded C++ value has incomplete construction identity"; return false; }
        const auto& binding = constructions[*value->constructionOrdinal];
        const auto& constructor = members.members()[*value->memberOrdinal];
        if (binding.targetTypeOrdinal != *value->targetTypeOrdinal ||
            binding.selectedConstructorMemberOrdinal != value->memberOrdinal ||
            constructor.declaringTypeOrdinal != *value->targetTypeOrdinal ||
            constructor.kind != frontend::ClassMemberKind::Constructor || !constructor.isCppExternal)
        { error = "discarded C++ value constructor does not match its binding"; return false; }
        const auto destructors = members.findMembers(*value->targetTypeOrdinal, ".dtor");
        if (destructors.size() != 1 || destructors[0] >= members.members().size())
        { error = "discarded C++ value requires one matching destructor"; return false; }
        const auto& destructor = members.members()[destructors[0]];
        if (!destructor.isCppExternal || destructor.kind != frontend::ClassMemberKind::Destructor ||
            destructor.declaringTypeOrdinal != *value->targetTypeOrdinal)
        { error = "discarded C++ value destructor does not match its type"; return false; }
        if (constructor.cppLifetimeImplicit) selected.push_back(*value->memberOrdinal);
        if (destructor.cppLifetimeImplicit) selected.push_back(destructors[0]);
    }
    return planCppImplicitLifetimeEmission(graph, projects, lookup, members, selected, output, error);
}

bool provideExperimentalCppStaticMethods(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedStaticMethod>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined);
    const auto visibleRecords = visibleMethodRecords(groups, lookup);
    using Family = std::pair<std::string, std::string>;
    std::map<Family, std::vector<frontend::CppImportedStaticMethod>> families;
    std::set<Family> blocked;
    for (const auto& group : groups.groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal) continue;
        for (const auto index : {*selected})
        {
            const auto& entry = entries[index];
            if (!entry.declaration.complete) continue;
            for (std::size_t i = 0; i < entry.declaration.methods.size(); ++i)
            {
                const auto& method = entry.declaration.methods[i];
                if (method.kind != CppMethodKind::Method) continue;
                const Family family{group.identity.key(), method.name};
                if (!method.isStatic) { blocked.insert(family); continue; }
                auto projected = projectCppSemanticStaticMethod(entry.declaration, i, group.projectOrdinal,
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &visibleRecords);
                if (const auto* mapped = std::get_if<CppSemanticStaticMethod>(&projected))
                    families[family].push_back(toCppImportedStaticMethod(*mapped));
                else blocked.insert(family);
            }
        }
    }
    std::vector<frontend::CppImportedStaticMethod> result;
    for (auto& [family, methods] : families)
    {
        if (blocked.contains(family)) continue;
        std::set<std::pair<std::vector<std::string>, bool>> signatures;
        bool ambiguous = false;
        for (const auto& method : methods)
            if (!signatures.emplace(method.parameterTypeKeys,
                method.parameterTypeKeys.size() == 1 && method.parameterTypeKeys[0].ends_with('*') &&
                !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[0]).second) ambiguous = true;
        if (ambiguous) continue;
        for (auto& method : methods) result.push_back(std::move(method));
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool provideExperimentalCppInstanceMethods(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedInstanceMethod>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined);
    const auto visibleRecords = visibleMethodRecords(groups, lookup);
    using Family = std::pair<std::string, std::string>;
    std::map<Family, std::vector<frontend::CppImportedInstanceMethod>> families;
    std::set<Family> blocked;
    for (const auto& group : groups.groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal) continue;
        for (const auto index : {*selected})
        {
            const auto& entry = entries[index];
            if (!entry.declaration.complete) continue;
            for (std::size_t methodIndex = 0; methodIndex < entry.declaration.methods.size(); ++methodIndex)
            {
                const auto& method = entry.declaration.methods[methodIndex];
                if (method.kind != CppMethodKind::Method) continue;
                const Family family{group.identity.key(), method.name};
                if (method.isStatic) { blocked.insert(family); continue; }
                auto mapped = projectCppSemanticInstanceMethod(entry.declaration, methodIndex, group.projectOrdinal,
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &visibleRecords);
                if (const auto* instance = std::get_if<CppSemanticInstanceMethod>(&mapped))
                    families[family].push_back(toCppImportedInstanceMethod(*instance));
                else blocked.insert(family);
            }
        }
    }
    std::vector<frontend::CppImportedInstanceMethod> result;
    for (auto& [family, methods] : families)
    {
        if (blocked.contains(family)) continue;
        std::set<std::tuple<std::vector<std::string>, bool, bool>> signatures;
        bool ambiguous = false;
        for (const auto& method : methods)
            if (!signatures.emplace(method.parameterTypeKeys,
                method.parameterTypeKeys.size() == 1 && method.parameterTypeKeys[0].ends_with('*') &&
                !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[0],
                method.receiverConst).second) ambiguous = true;
        if (ambiguous) continue;
        for (auto& method : methods) result.push_back(std::move(method));
    }
    output = std::move(result);
    error.clear();
    return true;
}
}
