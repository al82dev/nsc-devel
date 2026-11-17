#include "../Internal/CAtomicPointee.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/AST/AstNode.h"

#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Interop/CppSemanticEnumType.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Frontend/ObjectConstructionBinding.h"
#include "nativesharp/Frontend/ClassDestructionPlan.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Interop/CppMemberThunk.h"
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

// EN: T19-002. The carrier of a C pointer to `_Atomic T` is `cAtomicPointeeCarrier`, now in
//     src/Interop/Internal/CAtomicPointee.h (T19-038), with its description.
// ES: T19-002. El portador de un puntero C a `_Atomic T` es `cAtomicPointeeCarrier`, ahora en
//     src/Interop/Internal/CAtomicPointee.h (T19-038), con su descripcion.

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

// EN: T19-001. A record passed or returned by value through a C function pointer of an `extern "C"` function:
//     first as a C record (T08-D22); in a C++ header, where the records are C++ records, as a trivial C++ record
//     when the function's convention passes those (`allowCppRecordValue`), as a C++ function's callback does.
// ES: T19-001. Un record pasado o devuelto por valor por un puntero a funcion C de una funcion `extern "C"`:
//     primero como record C (T08-D22); en un encabezado C++, donde los records son records C++, como record C++
//     trivial si la convencion de la funcion los pasa (`allowCppRecordValue`), como el callback de una funcion C++.
std::optional<CppScalarTypeMapping> cCallbackRecordValueOfCFunction(const CppType& type, std::string_view moduleName,
    const frontend::ProjectNameLookup* lookup, std::optional<std::size_t> owner, bool allowCppRecordValue)
{
    auto mapped = cCallbackRecordValue(type, moduleName, lookup, owner, false);
    if (!mapped && allowCppRecordValue) mapped = cCallbackRecordValue(type, moduleName, lookup, owner, true);
    return mapped;
}

// EN: T13-007. A record type that is the C++ mirror of a NativeSharp struct maps to that struct, when the
//     program declares it; its value layout is the one Clang gave the mirror, checked against NativeSharp's.
// ES: T13-007. Un tipo record que es el espejo C++ de un struct NativeSharp corresponde a ese struct, si el
//     programa lo declara; su layout de valor es el que Clang dio al espejo, comprobado frente al de NativeSharp.
std::optional<std::string> nativeStructMirrorTypeKey(const CppType& type, const frontend::ProjectNameLookup* lookup)
{
    if (!lookup || type.kind != CppTypeKind::Record || type.isVolatile || !type.recordIdentity ||
        type.recordIdentity->namespaceSegments.empty() ||
        type.recordIdentity->namespaceSegments.front() != cppNativeStructNamespace) return std::nullopt;
    const auto identity = projectCppRecordTypeIdentity(type, type.recordIdentity->namespaceSegments.size() > 1 ?
        type.recordIdentity->namespaceSegments[1] : std::string());
    const auto ordinal = identity ? lookup->typeOrdinalForIdentity(identity->key()) : std::nullopt;
    if (!ordinal || lookup->cppImportedType(*ordinal) || !lookup->isCppNativeMirrorType(identity->key()))
        return std::nullopt;
    return "type:" + identity->key();
}

// EN: T19-001. Whether an array, of any number of dimensions, has scalar elements. Such an array behind a
//     pointer is mapped in a C function pointer too (the adjusted type of a callback parameter `double a[*][3]`);
//     arrays of records and enums keep needing the record values of a direct call.
// ES: T19-001. Si un arreglo, de cualquier numero de dimensiones, tiene elementos escalares. Un arreglo asi
//     detras de un puntero tambien se mapea en un puntero a funcion C (el tipo ajustado de un parametro de callback
//     `double a[*][3]`); los arreglos de records y enums siguen exigiendo los valores de record de una llamada
//     directa.
bool cScalarArray(const CppType& array)
{
    const CppType* element = &array;
    while (element && element->kind == CppTypeKind::Array) element = element->element.get();
    if (!element) return false;
    return element->kind == CppTypeKind::Boolean || element->kind == CppTypeKind::SignedInteger ||
        element->kind == CppTypeKind::UnsignedInteger || element->kind == CppTypeKind::Floating;
}

std::optional<CppScalarTypeMapping> mapFunctionType(
    const CppType& type, bool allowReadonlyPointee = false, const RecordIndex* records = nullptr,
    std::string_view moduleName = {}, std::size_t depth = 0,
    const frontend::ProjectNameLookup* lookup = nullptr, std::optional<std::size_t> owner = std::nullopt,
    bool allowCRecordValue = false, bool allowCStoredRecordPointer = false, bool allowCppRecordValue = false)
{
    if (depth > 64) return std::nullopt;
    // T19-001: also a fixed array of scalars behind a pointer of a C function pointer. / Tambien un arreglo fijo de
    //     escalares detras de un puntero de un puntero a funcion C.
    if (type.kind == CppTypeKind::Array && depth > 0 && (allowCRecordValue || cScalarArray(type)))
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
        if (auto mirror = nativeStructMirrorTypeKey(type, lookup))
            return CppScalarTypeMapping{std::move(*mirror), type.canonicalSpelling, false, false};
        if (!records && (allowCStoredRecordPointer || allowCppRecordValue) &&
            (depth == 2 || (allowCppRecordValue && depth > 2)) && lookup && owner &&
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
            // T19-001: with C record values too, only a C++ record. / Con valores de record C, solo un record C++.
            const bool cppTrivialRecord = imported && allowCppRecordValue && ordinal &&
                (!allowCRecordValue || imported->origin == frontend::CppImportedTypeOrigin::CppRecord) &&
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
        // EN: T19-004. A 128-bit integer is not mapped through a function pointer yet: the call would use LLVM's
        //     `i128` passing, not Clang's address and XMM0 (T19-044). A declared function keeps Clang's plan.
        // ES: T19-004. Un entero de 128 bits todavia no se mapea por un puntero a funcion: la llamada usaria el paso
        //     `i128` de LLVM, no la direccion y XMM0 de Clang (T19-044). Una funcion declarada conserva el plan.
        const auto wideInteger = [](const CppType& slot) {
            return (slot.kind == CppTypeKind::SignedInteger || slot.kind == CppTypeKind::UnsignedInteger) &&
                slot.sizeBits == 128;
        };
        if (wideInteger(signature.returnType) ||
            std::any_of(signature.parameterTypes.begin(), signature.parameterTypes.end(), wideInteger))
            return std::nullopt;
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
        else if (signature.callingConvention == "preserve_none") function.callingConvention = language::CallingConvention::PreserveNone;
        // EN: T19-028. swiftcall, swiftasynccall and intel_ocl_bicc pointers pass scalars as declared, like their
        //     direct calls; a Swift record value is expanded by Clang and stays out (T19-028 records).
        // ES: T19-028. Los punteros swiftcall, swiftasynccall e intel_ocl_bicc pasan los escalares como se
        //     declaran, como sus llamadas directas; un record Swift lo expande Clang y queda fuera.
        else if (signature.callingConvention == "swiftcall")
            function.callingConvention = language::CallingConvention::Swift;
        else if (signature.callingConvention == "swiftasynccall")
            function.callingConvention = language::CallingConvention::SwiftAsync;
        else if (signature.callingConvention == "intel_ocl_bicc")
            function.callingConvention = language::CallingConvention::IntelOclBicc;
        else return std::nullopt;
        if ((function.callingConvention == language::CallingConvention::Swift ||
             function.callingConvention == language::CallingConvention::SwiftAsync ||
             function.callingConvention == language::CallingConvention::IntelOclBicc) &&
            (signature.variadic || signature.returnType.kind == CppTypeKind::Record ||
             std::any_of(signature.parameterTypes.begin(), signature.parameterTypes.end(),
                         [](const CppType& slot) { return slot.kind == CppTypeKind::Record; })))
            return std::nullopt;
        // C records retain T08-D22; C++ records require the verified trivial-value rule.
        // EN: T19-036. A `__vectorcall` pointer passes a trivial C++ record like a direct `__vectorcall`
        //     declaration: homogeneous aggregates in vector registers, and a record that MSVC does not return
        //     in registers through a hidden pointer (cppIndirectReturn).
        // ES: T19-036. Un puntero `__vectorcall` pasa un record C++ trivial como una declaracion `__vectorcall`
        //     directa: agregados homogeneos en registros vectoriales, y por puntero oculto el record que MSVC no
        //     devuelve en registros (cppIndirectReturn).
        const bool cRecordValues = allowCRecordValue || allowCStoredRecordPointer;
        const bool cppRecordValues = !cRecordValues &&
            (signature.callingConvention == "cdecl" || signature.callingConvention == "preserve_most" || signature.callingConvention == "preserve_none" ||
             signature.callingConvention == "regcall" || signature.callingConvention == "sysv_abi" ||
             signature.callingConvention == "vectorcall");
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
        // EN: T19-038. A C `_Atomic T*` slot: its carrier is the pointer to T, with an atomic identity flag.
        // ES: T19-038. Una posicion C `_Atomic T*`: su portador es el puntero a T, con una marca atomica.
        const auto pointeeAtomic = [&](const CppType& slot) { return cAtomicPointeeCarrier(slot).has_value(); };
        // EN: The carrier is mapped without the pointee qualifiers, which become identity flags below.
        // ES: El portador se mapea sin los calificadores del dato apuntado, que abajo pasan a ser marcas.
        const auto carrier = [&](const CppType& original)
        {
            const auto atomic = cAtomicPointeeCarrier(original);
            const CppType& slot = atomic ? *atomic : original;
            if (!(qualifiable(slot) && (slot.element->isConst || slot.element->isVolatile))) return slot;
            CppType unqualified = slot;
            auto element = std::make_shared<CppType>(*slot.element);
            element->isConst = false;
            element->isVolatile = false;
            unqualified.element = std::move(element);
            return unqualified;
        };
        // T19-001: a C function of a C++ header also passes trivial C++ records. / Una funcion C de un encabezado
        //     C++ tambien pasa records C++ triviales.
        const bool cFunctionCppRecords = cRecordValues && allowCppRecordValue;
        const auto result = signature.returnType.kind != CppTypeKind::Record
            ? mapFunctionType(carrier(signature.returnType), false, records, moduleName, depth + 1,
                              lookup, owner, false, allowCStoredRecordPointer, cppRecordValues)
            : cRecordValues
            ? cCallbackRecordValueOfCFunction(signature.returnType, moduleName, lookup, owner, cFunctionCppRecords)
            : cCallbackRecordValue(signature.returnType, moduleName, lookup, owner, cppRecordValues);
        if (!result || result->isConst || result->isVolatile) return std::nullopt;
        function.result = result->nativeSharpTypeKey;
        function.resultPointeeReadonly = pointeeConst(signature.returnType);
        function.resultPointeeVolatile = pointeeVolatile(signature.returnType);
        function.resultPointeeAtomic = pointeeAtomic(signature.returnType);
        for (const auto& parameter : signature.parameterTypes)
        {
            if (parameter.kind == CppTypeKind::Record && !cRecordValues && !cppRecordValues) return std::nullopt;
            const auto mapped = parameter.kind != CppTypeKind::Record
                ? mapFunctionType(carrier(parameter), true, records, moduleName, depth + 1, lookup,
                                  owner, false, allowCStoredRecordPointer, cppRecordValues)
                : cRecordValues
                ? cCallbackRecordValueOfCFunction(parameter, moduleName, lookup, owner, cFunctionCppRecords)
                : cCallbackRecordValue(parameter, moduleName, lookup, owner, cppRecordValues);
            if (!mapped || mapped->isConst || mapped->isVolatile || parameter.kind == CppTypeKind::Void)
                return std::nullopt;
            function.parameters.push_back(mapped->nativeSharpTypeKey);
            function.parameterPointeeReadonly.push_back(pointeeConst(parameter));
            function.parameterPointeeVolatile.push_back(pointeeVolatile(parameter));
            function.parameterPointeeAtomic.push_back(pointeeAtomic(parameter));
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
// EN: T10-010. A C++ default argument that Clang evaluated to a constant becomes the literal a NativeSharp
//     declaration would spell, so the caller materializes it like any other default value. The nodes
//     live for the whole compilation, as declaration syntax does.
// ES: T10-010. Un argumento predeterminado C++ que Clang evaluo a una constante pasa a ser el literal que
//     escribiria una declaracion NativeSharp, asi que el llamador lo materializa como cualquier otro valor
//     predeterminado. Los nodos viven toda la compilacion, como la sintaxis de una declaracion.
const ast::AstNode* cppDefaultArgumentSyntax(const CppFunctionParameter& parameter, std::string_view typeKey)
{
    static std::mutex mutex;
    static std::deque<std::unique_ptr<ast::AstNode>> arena;
    const std::lock_guard lock(mutex);
    const auto make = [&](ast::AstNodeKind kind, lexer::TokenKind token, std::string text) {
        arena.push_back(std::make_unique<ast::AstNode>(kind, SourceRange{}));
        auto* node = arena.back().get();
        node->setTokenKind(token);
        node->setText(std::move(text));
        return node;
    };
    const auto negate = [&](ast::AstNode* operand) {
        auto* node = make(ast::AstNodeKind::UnaryExpression, lexer::TokenKind::Minus, "-");
        node->setFirst(operand);
        return node;
    };
    const auto bits = parameter.defaultArgumentBits;
    switch (parameter.defaultArgumentKind)
    {
        case CppDefaultArgumentKind::Boolean:
        {
            auto* node = make(ast::AstNodeKind::BooleanLiteralExpression,
                bits ? lexer::TokenKind::KwTrue : lexer::TokenKind::KwFalse, bits ? "true" : "false");
            node->setIntegerValue(bits ? 1U : 0U);
            return node;
        }
        case CppDefaultArgumentKind::NullPointer:
            return make(ast::AstNodeKind::NullLiteralExpression, lexer::TokenKind::KwNull, "null");
        case CppDefaultArgumentKind::Signed:
        case CppDefaultArgumentKind::Unsigned:
        {
            const bool negative = parameter.defaultArgumentKind == CppDefaultArgumentKind::Signed &&
                static_cast<std::int64_t>(bits) < 0;
            const std::uint64_t magnitude = negative ? std::uint64_t{0} - bits : bits;
            auto* literal = make(ast::AstNodeKind::IntegerLiteralExpression, lexer::TokenKind::IntegerLiteral,
                                 std::to_string(magnitude));
            literal->setIntegerValue(magnitude);
            return negative ? negate(literal) : literal;
        }
        case CppDefaultArgumentKind::Floating:
        {
            const double value = std::bit_cast<double>(bits);
            if (!std::isfinite(value)) return nullptr;
            const double magnitude = std::fabs(value);
            char buffer[64];
            const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), magnitude);
            if (converted.ec != std::errc{}) return nullptr;
            std::string text(buffer, converted.ptr);
            if (text.find_first_of(".e") == std::string::npos) text += ".0";
            if (typeKey == "builtin:float" || typeKey == "float") text += "f";
            auto* literal = make(ast::AstNodeKind::FloatingLiteralExpression, lexer::TokenKind::FloatingLiteral,
                                 std::move(text));
            literal->setFloatingValue(magnitude);
            return std::signbit(value) ? negate(literal) : literal;
        }
        default:
            return nullptr;
    }
}

// EN: T10-003. A C++ lvalue reference is passed and returned as the address of its referent, exactly like
//     a NativeSharp ref/in parameter or a ref return; the carrier is mapped as a pointer to the referent.
// ES: T10-003. Una referencia lvalue C++ se pasa y se devuelve como la direccion de su referente, igual
//     que un parametro ref/in o un retorno ref de NativeSharp; el portador se mapea como puntero al referente.
namespace
{
// EN: T19-012. A method of the record itself keeps its protected or private access, which NativeSharp checks at
//     each call, as for static methods (T10-007). A method called through a Clang adapter (virtual or inherited)
//     must be public, because the adapter calls it from outside the class.
// ES: T19-012. Un metodo del propio record conserva su acceso protegido o privado, que NativeSharp comprueba en
//     cada llamada, como los metodos estaticos (T10-007). Un metodo llamado por un adaptador de Clang (virtual o
//     heredado) debe ser publico, porque el adaptador lo llama desde fuera de la clase.
// EN: T19-006. The C++ spelling of each parameter of a method, and a signature suffix for the spellings that
//     NativeSharp writes with another type (`long` as int, `wchar_t` as ushort) and for a default-argument adapter,
//     so two C++ overloads are two members.
// ES: T19-006. La escritura C++ de cada parametro de un metodo, y un sufijo de firma para las escrituras que
//     NativeSharp escribe con otro tipo (`long` como int, `wchar_t` como ushort) y para un adaptador de
//     predeterminados, para que dos sobrecargas C++ sean dos miembros.
//     The declaration as the header writes it is the alternative a failed call lists (T17-003).
//     La declaracion como la escribe el encabezado es la alternativa que lista una llamada fallida (T17-003).
void describeCppMethodOverload(const CppMethodDeclaration& method, std::vector<std::string>& spellings,
                               std::string& suffix, std::string& declaration)
{
    static const std::set<std::string_view> renamed{"long", "unsigned long", "char", "wchar_t", "char8_t",
                                                     "char16_t", "char32_t", "long double"};
    const bool adapter = method.clangUsr && method.clangUsr->find("@nsc-default:") != std::string::npos;
    declaration = method.kind == CppMethodKind::Constructor ? std::string{} :
        std::string(method.isStatic ? "static " : "") + method.returnType.spelling + " ";
    declaration += method.qualifiedName + "(";
    for (std::size_t index = 0; index < method.parameters.size(); ++index)
    {
        const auto& spelling = method.parameters[index].type.unqualifiedCanonicalSpelling;
        spellings.push_back(spelling);
        if (renamed.contains(spelling)) suffix += "@" + std::to_string(index) + ":" + spelling;
        // T19-007: `T&&` and `T&` are both ref parameters. / Ambos son parametros ref.
        if (method.parameters[index].type.kind == CppTypeKind::RValueReference)
            suffix += "@" + std::to_string(index) + ":&&";
        declaration += (index ? ", " : "") + method.parameters[index].typeSpelling;
    }
    declaration += method.isConst ? ") const" : ")";
    if (adapter)
    {
        suffix += "@default";
        declaration += " with its default arguments";
    }
}

bool projectedMethodAccess(const CppMethodDeclaration& method, bool dispatched)
{
    if (method.access == CppAccess::Public) return true;
    if (dispatched || method.inherited) return false;
    return method.access == CppAccess::Protected || method.access == CppAccess::Private;
}
}

std::optional<CppType> cppReferenceCarrier(const CppType& type)
{
    if (type.kind != CppTypeKind::LValueReference || !type.element ||
        type.element->kind == CppTypeKind::Void || type.element->kind == CppTypeKind::Function ||
        type.element->kind == CppTypeKind::Array || type.element->kind == CppTypeKind::LValueReference ||
        type.element->kind == CppTypeKind::RValueReference || type.element->isVolatile ||
        type.element->canonicalSpelling.empty()) return std::nullopt;
    CppType carrier = type;
    carrier.kind = CppTypeKind::Pointer;
    carrier.isConst = false;
    carrier.isVolatile = false;
    carrier.sizeBits = 64;
    carrier.alignmentBits = 64;
    carrier.canonicalSpelling = type.element->canonicalSpelling + " *";
    carrier.spelling = carrier.canonicalSpelling;
    carrier.unqualifiedCanonicalSpelling = carrier.canonicalSpelling;
    return carrier;
}

namespace
{
// EN: T19-007. A `T&&` parameter travels as the address of its referent, like `T&`: the same carrier, from the
//     lvalue view of the type. NativeSharp passes `ref x` (as `std::move(x)`) or a value it materializes.
// ES: T19-007. Un parametro `T&&` viaja como la direccion de su referente, como `T&`: el mismo portador, desde la
//     vista lvalue del tipo. NativeSharp pasa `ref x` (como `std::move(x)`) o un valor que materializa.
std::optional<CppType> cppParameterReferenceCarrier(const CppType& type)
{
    if (type.kind != CppTypeKind::RValueReference) return cppReferenceCarrier(type);
    CppType view = type;
    view.kind = CppTypeKind::LValueReference;
    return cppReferenceCarrier(view);
}
}

// EN: T19-036. A 128-bit Clang vector by value (`__m128`, a `vector_size(16)` type of float, int or long long)
//     maps to its NativeSharp.Core.Numerics carrier (Float32x4, Int32x4, Int64x2) when that type is visible. The
//     NativeSharp struct is an ordinary record, so the value keeps its vector ABI only through Clang's plan of the
//     exact declaration: the position must be a direct vector of the same lanes there. The backend reads the same
//     plan. Under `__vectorcall`, only Float32x4 is a vector (T08-D22); integer vectors there are not mapped yet.
// ES: T19-036. Un vector de Clang de 128 bits por valor se mapea a su portador de NativeSharp.Core.Numerics
//     (Float32x4, Int32x4, Int64x2) cuando ese tipo es visible. El struct NativeSharp es un record ordinario, asi
//     que el valor conserva su ABI vectorial solo por el plan de Clang de la declaracion exacta: la posicion debe
//     ser ahi un vector directo de los mismos carriles. El backend lee el mismo plan. Con `__vectorcall`, solo
//     Float32x4 es un vector (T08-D22); los vectores enteros ahi aun no se mapean.
std::optional<CppScalarTypeMapping> cppVectorValueType(
    const CppType& type, const CppFunctionDeclaration& function, std::optional<std::size_t> parameterIndex,
    const frontend::ProjectNameLookup* lookup, std::string& reason)
{
    if (type.kind != CppTypeKind::FixedVector || !type.element || !type.arrayCount || !type.sizeBits ||
        (*type.sizeBits != 128 && *type.sizeBits != 256) || type.isConst || type.isVolatile)
    { reason = "only 128-bit and 256-bit vectors are mapped"; return std::nullopt; }
    const auto& lane = *type.element;
    const auto count = *type.arrayCount;
    // EN: T19-036. The NativeSharp.Core.Numerics carrier of each lane type and count: 128 bits (XMM) and 256 bits
    //     (YMM, which needs the CPU level x86-64-v3).
    // ES: T19-036. El portador de NativeSharp.Core.Numerics de cada tipo y numero de carriles: 128 bits (XMM) y
    //     256 bits (YMM, que necesita el nivel de CPU x86-64-v3).
    struct VectorCarrier
    {
        bool floating;
        std::uint64_t laneBits;
        std::uint64_t lanes;
        const char* name;
    };
    static constexpr VectorCarrier carriers[] = {
        {true, 32, 4, "NativeSharp.Core.Numerics.Float32x4"},
        {true, 64, 2, "NativeSharp.Core.Numerics.Float64x2"},
        {false, 32, 4, "NativeSharp.Core.Numerics.Int32x4"},
        {false, 64, 2, "NativeSharp.Core.Numerics.Int64x2"},
        {true, 32, 8, "NativeSharp.Core.Numerics.Float32x8"},
        {true, 64, 4, "NativeSharp.Core.Numerics.Float64x4"},
        {false, 32, 8, "NativeSharp.Core.Numerics.Int32x8"},
        {false, 64, 4, "NativeSharp.Core.Numerics.Int64x4"},
    };
    const bool floatingLane = lane.kind == CppTypeKind::Floating;
    const bool integerLane = lane.kind == CppTypeKind::SignedInteger;
    const char* carrier = nullptr;
    bool floating = false;
    std::uint64_t laneBits = 0;
    for (const auto& candidate : carriers)
    {
        if ((candidate.floating ? !floatingLane : !integerLane) || !lane.sizeBits ||
            *lane.sizeBits != candidate.laneBits || count != candidate.lanes) continue;
        carrier = candidate.name;
        floating = candidate.floating;
        laneBits = candidate.laneBits;
    }
    if (!carrier) { reason = "its lanes have no NativeSharp.Core.Numerics carrier"; return std::nullopt; }
    const bool vectorCall = function.callingConvention == "vectorcall";
    if (function.callingConvention != "cdecl" && function.callingConvention != "ms_abi" && !vectorCall)
    { reason = "the calling convention " + function.callingConvention + " is not mapped for vectors"; return std::nullopt; }
    if (!lookup) { reason = "the NativeSharp carrier is not resolved"; return std::nullopt; }
    std::optional<std::string> key;
    for (const auto ordinal : lookup->typeOrdinalsForName(carrier))
    {
        if (ordinal >= lookup->namedTypes().size()) continue;
        const auto& named = lookup->namedTypes()[ordinal];
        if (named.kind != frontend::NamedTypeDeclarationKind::Struct ||
            named.identity.fullyQualifiedNameView() != carrier) continue;
        if (key) { reason = "the NativeSharp carrier is ambiguous"; return std::nullopt; }
        key = "type:" + named.identity.key();
    }
    if (!key) { reason = std::string("it needs ") + carrier + " (NativeSharp.Core)"; return std::nullopt; }
    const auto* plan = function.abiPlan.get();
    if ((!plan || plan->variadic) && *type.sizeBits == 256)
    { reason = "a 256-bit vector needs AVX registers: set the project CpuLevel to x86-64-v3"; return std::nullopt; }
    if (!plan || plan->variadic || plan->logicalParameters.size() != function.parameters.size())
    { reason = "it has no Clang ABI plan"; return std::nullopt; }
    const abi_plan::Type* physical = nullptr;
    if (!parameterIndex)
    {
        if (plan->logicalReturn.passing != abi_plan::Passing::Direct) { reason = "Clang returns it indirectly"; return std::nullopt; }
        physical = &plan->physicalReturn;
    }
    else
    {
        const auto& logical = plan->logicalParameters[*parameterIndex];
        if (logical.passing != abi_plan::Passing::Direct || logical.physicalCount != 1 ||
            logical.firstPhysical >= plan->physicalParameters.size())
        { reason = "Clang does not pass it as one direct vector"; return std::nullopt; }
        physical = &plan->physicalParameters[logical.firstPhysical].type;
    }
    if (physical->kind != abi_plan::TypeKind::Vector || physical->widthOrCount != count ||
        physical->elements.size() != 1 ||
        (floating ? physical->elements[0].kind !=
                        (laneBits == 32 ? abi_plan::TypeKind::Float : abi_plan::TypeKind::Double)
                  : (physical->elements[0].kind != abi_plan::TypeKind::Integer ||
                     physical->elements[0].widthOrCount != laneBits)))
    { reason = "Clang's plan has another vector shape"; return std::nullopt; }
    return CppScalarTypeMapping{std::move(*key), type.canonicalSpelling, false, false};
}

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
    // EN: T19-028. A variadic intel_ocl_bicc function stays out: Clang emits it, but LLVM's verifier rejects that
    //     convention with varargs, so NSC cannot declare it (Clang's release build skips the verifier).
    // ES: T19-028. Una funcion intel_ocl_bicc variadica queda fuera: Clang la emite, pero el verificador de LLVM
    //     rechaza esa convencion con varargs, asi que NSC no puede declararla (la version de Clang omite el
    //     verificador).
    if (function.variadic && function.callingConvention != "cdecl" && function.callingConvention != "sysv_abi")
        return failure("variadic function calling convention is not yet projected");
    const bool cUnprototyped = function.languageLinkage == CppLanguageLinkage::C && !function.hasPrototype;
    if (cUnprototyped && (function.callingConvention != "cdecl" || !function.parameters.empty()))
        return failure("unprototyped C function with unsupported definition or calling convention");
    const bool cLanguage = function.languageLinkage == CppLanguageLinkage::C || function.isCStaticInlineDefinition;
    // EN: T14-007. A pointer to member has no NativeSharp type: its representation in the Microsoft ABI depends
    //     on the inheritance model of the class, so the declaration is not imported and a call says why.
    // ES: T14-007. Un puntero a miembro no tiene tipo NativeSharp: su representacion en el ABI de Microsoft
    //     depende del modelo de herencia de la clase, asi que la declaracion no se importa y una llamada dice por que.
    if (function.returnType.memberPointer)
        return failure("its return type '" + function.returnType.spelling + "' is a C++ pointer to member, "
                       "which has no NativeSharp type");
    for (const auto& parameter : function.parameters)
        if (parameter.type.memberPointer)
            return failure("its parameter '" + parameter.name + "' of type '" + parameter.type.spelling +
                           "' is a C++ pointer to member, which has no NativeSharp type");
    // EN: T19-007. A `T&&` result is a reference too, read-only: an xvalue is read or passed on, not assigned.
    // ES: T19-007. Un resultado `T&&` tambien es una referencia, de solo lectura: un xvalue se lee o se pasa.
    const bool referenceReturn = !cLanguage && (function.returnType.kind == CppTypeKind::LValueReference ||
        function.returnType.kind == CppTypeKind::RValueReference);
    const auto returnCarrier = referenceReturn ? cppParameterReferenceCarrier(function.returnType) : std::nullopt;
    // EN: T19-007. A `const T&` result is a read-only reference; the call site derives it from cppReturnType.
    // ES: T19-007. Un resultado `const T&` es una referencia de solo lectura; la llamada la deduce de cppReturnType.
    if (referenceReturn && !returnCarrier)
        return failure("C++ reference return type is not yet mapped");
    // T19-002: a C `_Atomic T*` result is a pointer to T marked atomic. / Un resultado C `_Atomic T*` es un puntero
    //     a T marcado atomico.
    const auto atomicReturn = cLanguage ? cAtomicPointeeCarrier(function.returnType) : std::nullopt;
    const CppType mappedReturn = cLanguage
        ? invocationTypeForC(atomicReturn ? *atomicReturn : function.returnType, true)
        : returnCarrier ? *returnCarrier : function.returnType;
    const bool cRecordValues = cLanguage;
    // EN: T19-036. `__vectorcall` passes a trivial C++ record like a C one (homogeneous aggregates in vector
    //     registers); the backend accepts it only when Clang's plan for the declaration agrees.
    // ES: T19-036. `__vectorcall` pasa un record C++ trivial como uno C (agregados homogeneos en registros
    //     vectoriales); el backend lo acepta solo si el plan de Clang de la declaracion coincide.
    // EN: T19-001. An `extern "C"` function of a C++ header takes and returns the trivial C++ records of that
    //     header by value too (its structs, and the carrier of `_Complex`): the C language linkage does not
    //     change how Clang passes a record. A C struct or union keeps its own rule (cRecordValues).
    // ES: T19-001. Una funcion `extern "C"` de un encabezado C++ tambien toma y devuelve por valor los records
    //     C++ triviales de ese encabezado (sus structs, y el portador de `_Complex`): el enlace de lenguaje C no
    //     cambia como pasa Clang un record. Un struct o union C conserva su propia regla (cRecordValues).
    const bool cppRecordValues =
        (function.callingConvention == "cdecl" || function.callingConvention == "preserve_most" || function.callingConvention == "preserve_none" ||
         function.callingConvention == "regcall" || function.callingConvention == "sysv_abi" ||
         function.callingConvention == "vectorcall" ||
         // T19-028: Swift records with Clang's checked plan. / Records Swift con el plan comprobado de Clang.
         ((function.callingConvention == "swiftcall" || function.callingConvention == "swiftasynccall") &&
          function.abiPlan));
    std::string vectorReason;
    const auto resultType = mappedReturn.kind == CppTypeKind::FixedVector
        ? cppVectorValueType(mappedReturn, function, std::nullopt, lookup, vectorReason)
        : mapFunctionType(mappedReturn, true, records, moduleName, 0, lookup, projectOrdinal,
                          cRecordValues, false, cppRecordValues);
    if (!resultType && !vectorReason.empty())
        return failure("C/C++ vector return type is not yet mapped: " + vectorReason);
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

    // T10-002: each alternate qualified name of the same function, as a NativeSharp namespace.
    std::vector<std::string> alternateNamespaces;
    for (const auto& alternate : function.alternateQualifiedNames)
    {
        // EN: T19-005. An alternate without a namespace names the function in the global namespace.
        // ES: T19-005. Un nombre alterno sin namespace nombra la funcion en el namespace global.
        const auto separator = alternate.rfind("::");
        if (separator == std::string::npos)
        {
            if (alternate == function.name && !nsNamespace.empty()) alternateNamespaces.push_back({});
            continue;
        }
        if (alternate.substr(separator + 2) != function.name) continue;
        std::string dotted;
        for (std::size_t i = 0; i < separator; ++i)
        {
            if (alternate[i] == ':' && i + 1 < separator && alternate[i + 1] == ':') { dotted.push_back('.'); ++i; }
            else dotted.push_back(alternate[i]);
        }
        if (!dotted.empty() && dotted != nsNamespace) alternateNamespaces.push_back(std::move(dotted));
    }

    frontend::NamespaceFunctionSymbol symbol;
    symbol.cppAlternateNamespaces = std::move(alternateNamespaces);
    symbol.projectOrdinal = projectOrdinal;
    symbol.moduleName = std::string(moduleName);
    symbol.namespaceName = std::move(nsNamespace);
    symbol.name = function.name;
    // EN: T10-016. A C++ free operator function becomes a NativeSharp extension operator named by its operator
    //     method (operator+ -> op_Addition), which the operator lookup of NativeSharp already consults.
    // ES: T10-016. Una funcion operador libre C++ pasa a ser un operador de extension NativeSharp con el nombre
    //     de su metodo de operador (operator+ -> op_Addition), que la busqueda de operadores ya consulta.
    if (!cLanguage && function.name.size() > 8 && function.name.starts_with("operator") &&
        !std::isalnum(static_cast<unsigned char>(function.name[8])) && function.name[8] != '_')
    {
        const auto method = language::operatorMethodName(std::string_view(function.name).substr(8),
                                                         function.parameters.size());
        if (method.empty() || function.variadic) return failure("C++ operator is not a NativeSharp operator");
        symbol.name = std::string(method);
        symbol.isExtension = true;
    }
    // EN: T19-039. The C operators of a `_Complex` carrier (ClangSession::declareCComplexOperators) are extension
    //     operators `op_<Operator>`; their conversion helpers are not imported.
    // ES: T19-039. Los operadores C de un portador `_Complex` (ClangSession::declareCComplexOperators) son
    //     operadores de extension `op_<Operador>`; sus funciones auxiliares de conversion no se importan.
    if (cLanguage && function.name.starts_with("__nsc_cx_"))
        return failure("NativeSharp complex conversion helper is not imported");
    if (cLanguage && function.cMacroWrapper.starts_with("__nsc_complex_op_"))
    {
        const auto method = function.name.starts_with("operator")
            ? language::operatorMethodName(std::string_view(function.name).substr(8), function.parameters.size())
            : std::string_view{};
        if (method.empty() || function.variadic) return failure("C complex operator is not a NativeSharp operator");
        symbol.name = std::string(method);
        symbol.isExtension = true;
    }
    symbol.cppClangUsr = function.clangUsr;
    symbol.declaredTypeKey = resultType->nativeSharpTypeKey;
    if (referenceReturn)
    {
        if (!symbol.declaredTypeKey.ends_with('*')) return failure("C++ reference return type is not yet mapped");
        symbol.declaredTypeKey.pop_back();
        symbol.isReferenceReturn = true;
    }
    symbol.mangledName = *function.externalSymbol;
    symbol.cppCallingConvention = function.callingConvention;
    symbol.cppAbiPlan = function.abiPlan;
    symbol.cppReturnType = function.returnType;
    symbol.cppDllImport = function.dllImport;
    symbol.cppTemplateSpecialization = function.templateSpecialization;
    symbol.isVariadic = function.variadic || cUnprototyped;
    symbol.cppCanonicalIdentity = *function.externalSymbol + "|" +
        (referenceReturn ? function.returnType.canonicalSpelling : resultType->canonicalCppType) + "(";
    symbol.signatureKey = symbol.name + "(";
    if (function.location)
    {
        const auto encoded = function.location->file.generic_u8string();
        symbol.sourcePath.assign(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    }
    // EN: T17-003. The declaration as the header writes it, for the alternatives of an overload diagnostic.
    // ES: T17-003. La declaracion como la escribe el encabezado, para las alternativas de una sobrecarga.
    symbol.cppLocation = function.location;
    symbol.cppDeclarationSpelling = function.returnTypeSpelling + " " + function.qualifiedName + "(";
    for (std::size_t index = 0; index < function.parameters.size(); ++index)
        symbol.cppDeclarationSpelling += (index ? ", " : "") + function.parameters[index].typeSpelling;
    if (function.variadic) symbol.cppDeclarationSpelling += function.parameters.empty() ? "..." : ", ...";
    symbol.cppDeclarationSpelling += ")";
    for (std::size_t index = 0; index < function.parameters.size(); ++index)
    {
        const auto& input = function.parameters[index];
        // Map only the carrier; the original Clang type retains its qualifiers below.
        const bool rvalue = !cLanguage && input.type.kind == CppTypeKind::RValueReference;
        // T19-003: the pointer of an lvalue macro argument is a `ref` parameter. / El puntero de un argumento lvalue
        //     de una macro es un parametro `ref`.
        const bool macroReference = cLanguage && input.cMacroReference && input.type.kind == CppTypeKind::Pointer &&
            input.type.element;
        const bool reference = (!cLanguage && (input.type.kind == CppTypeKind::LValueReference || rvalue)) ||
            macroReference;
        const auto carrier = macroReference ? std::optional<CppType>(input.type)
            : reference ? cppParameterReferenceCarrier(input.type) : std::nullopt;
        if (reference && !carrier)
            return failure("C/C++ parameter " + std::to_string(index) + " is not yet mapped");
        // T19-002: a C `_Atomic T*` parameter takes a pointer to atomic storage. / Un parametro C `_Atomic T*`
        //     toma un puntero a almacenamiento atomico.
        const auto atomicPointee = cLanguage ? cAtomicPointeeCarrier(input.type) : std::nullopt;
        const CppType& sourceType = atomicPointee ? *atomicPointee : input.type;
        CppType mappedInput = cLanguage
            ? invocationTypeForC(sourceType, true) : carrier ? *carrier : input.type;
        // Top-level qualifiers on a by-value pointer describe the callee's
        // local parameter, not the pointer expression at the call site.
        if (input.objectSizeMode && mappedInput.kind == CppTypeKind::Pointer)
        {
            mappedInput.isConst = false;
            mappedInput.isVolatile = false;
        }
        const auto parameterType = mappedInput.kind == CppTypeKind::FixedVector
            ? cppVectorValueType(mappedInput, function, index, lookup, vectorReason)
            : mapFunctionType(mappedInput, true, records, moduleName, 0, lookup, projectOrdinal,
                              cRecordValues, false, cppRecordValues);
        if (!parameterType && !vectorReason.empty())
            return failure("C/C++ vector parameter " + std::to_string(index) + " is not yet mapped: " + vectorReason);
        if (!parameterType || input.type.kind == CppTypeKind::Void)
            return failure("C/C++ parameter " + std::to_string(index) + " is not yet mapped");
        frontend::MethodParameterSymbol parameter;
        parameter.name = input.name;
        parameter.typeKey = parameterType->nativeSharpTypeKey;
        parameter.pointerPointeeReadonly = sourceType.kind == CppTypeKind::Pointer &&
            sourceType.element && sourceType.element->isConst;
        parameter.pointerPointeeVolatile = sourceType.kind == CppTypeKind::Pointer &&
            sourceType.element && sourceType.element->isVolatile;
        parameter.cppPointerPointeeAtomic = atomicPointee.has_value();
        if (reference)
        {
            // T10-003: `T&` is a ref parameter and `const T&` an in parameter; both name the referent type.
            if (!parameter.typeKey.ends_with('*'))
                return failure("C/C++ parameter " + std::to_string(index) + " is not yet mapped");
            parameter.passKind = input.type.element->isConst
                ? frontend::ParameterPassKind::In : frontend::ParameterPassKind::Ref;
            parameter.pointeeTypeKey = parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
            parameter.pointerPointeeReadonly = input.type.element->isConst;
            parameter.pointerNullable = false;
            parameter.lifetimeName = "$p" + std::to_string(index);
            // T19-007: `T&&` also binds a materialized value. / `T&&` tambien enlaza un valor materializado.
            parameter.cppRvalueReference = rvalue;
            // T19-007: the object of a member operator adapter. / El objeto de un adaptador de operador miembro.
            parameter.cppImplicitObject = index == 0 && !rvalue && input.name == "self" &&
                std::string_view(*function.externalSymbol).starts_with(cppMemberThunkPrefix);
        }
        symbol.parameters.push_back(std::move(parameter));
        symbol.cppParameterTypes.push_back(input.type);
        symbol.cppObjectSizeModes.push_back(input.objectSizeMode
            ? static_cast<std::uint8_t>(1 + *input.objectSizeMode + (input.objectSizeDynamic ? 4 : 0)) : 0);
        if (index != 0) { symbol.signatureKey.push_back(','); symbol.cppCanonicalIdentity.push_back(','); }
        if (rvalue) symbol.signatureKey += "rvalue ";
        if (atomicPointee) symbol.signatureKey += "atomic ";
        if (reference) symbol.signatureKey += input.type.element->isConst ? "in " : "ref ";
        symbol.signatureKey += parameterType->nativeSharpTypeKey;
        symbol.cppCanonicalIdentity += reference ? input.type.canonicalSpelling : parameterType->canonicalCppType;
    }
    // T10-010: defaults stay trailing, so they are assigned from the last parameter backwards and stop at
    //     the first one whose default is absent, unevaluated or not passed by value.
    if (!symbol.isVariadic)
        for (std::size_t index = function.parameters.size(); index-- > 0;)
        {
            auto& parameter = symbol.parameters[index];
            if (parameter.passKind != frontend::ParameterPassKind::Value) break;
            parameter.defaultValueSyntax = cppDefaultArgumentSyntax(function.parameters[index], parameter.typeKey);
            if (!parameter.defaultValueSyntax) break;
        }
    if (symbol.isVariadic)
    {
        if (!function.parameters.empty()) { symbol.signatureKey.push_back(','); symbol.cppCanonicalIdentity.push_back(','); }
        symbol.signatureKey += "...";
        symbol.cppCanonicalIdentity += "...";
    }
    symbol.signatureKey.push_back(')');
    symbol.cppCanonicalIdentity.push_back(')');
    if (referenceReturn)
    {
        // T10-003: a reference returned by C++ carries no lifetime proof; it is bound to the only ref/in
        // parameter when there is exactly one, otherwise to static storage, as NativeSharp infers it.
        std::vector<std::string> inputs;
        for (const auto& parameter : symbol.parameters)
            if (parameter.passKind == frontend::ParameterPassKind::Ref || parameter.passKind == frontend::ParameterPassKind::In)
                inputs.push_back(parameter.lifetimeName);
        symbol.returnLifetimeName = inputs.size() == 1 ? inputs.front() : "static";
    }
    // EN: T10-016. An operator takes its operands by value or by const reference (an in parameter, as a
    //     NativeSharp operator may) and at least one operand is an imported C++ record or enum.
    //     T19-007: also by `T&` (a ref operand, which needs an assignable place, as C++ binds it only to an
    //     lvalue), and it may return a reference, as a function does.
    // ES: T10-016. Un operador toma sus operandos por valor o por referencia const (parametro in, como un
    //     operador NativeSharp) y al menos uno es un record o enum C++ importado.
    //     T19-007: tambien por `T&` (un operando ref, que exige un lugar asignable, como C++ solo lo enlaza a un
    //     lvalue), y puede devolver una referencia, como una funcion.
    if (symbol.isExtension &&
        (std::any_of(symbol.parameters.begin(), symbol.parameters.end(), [](const auto& parameter) {
             return parameter.passKind != frontend::ParameterPassKind::Value &&
                 parameter.passKind != frontend::ParameterPassKind::In &&
                 parameter.passKind != frontend::ParameterPassKind::Ref;
         }) ||
         std::none_of(symbol.parameters.begin(), symbol.parameters.end(), [](const auto& parameter) {
             const auto& key = parameter.passKind != frontend::ParameterPassKind::Value
                 ? parameter.pointeeTypeKey : parameter.typeKey;
             return key.starts_with("type:") && !key.ends_with('*') && !key.ends_with(']');
         })))
        return failure("C++ operator needs by-value or reference operands of an imported C++ type");
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
    const CppRecordGroups* records, const frontend::ProjectNameLookup* lookup)
{
    auto fail = [](std::string message) -> CppSemanticInstanceMethodResult {
        return CppSemanticFunctionFailure{std::move(message)};
    };
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || methodIndex >= record.methods.size())
        return fail("instance method requires a complete record and supported target");
    const auto identity = projectCppRecordIdentity(record, moduleName);
    if (!identity) return fail("instance method owner has no supported identity");
    const auto& method = record.methods[methodIndex];
    // EN: T12-004, T12-005. A method a public base declares (`inherited`) and a virtual method carry the
    //     symbol of the Clang adapter (`dispatchSymbol`), which is called instead of the method itself: the
    //     adapter applies the this adjustment and the virtual dispatch. Without an adapter neither is mapped.
    // ES: T12-004, T12-005. Un metodo que declara una base publica (`inherited`) y un metodo virtual llevan
    //     el simbolo del adaptador de Clang (`dispatchSymbol`), que se llama en lugar del metodo: el adaptador
    //     aplica el ajuste de this y el despacho virtual. Sin adaptador ninguno se mapea.
    const bool dispatched = method.dispatchSymbol && !method.dispatchSymbol->empty();
    if (method.kind != CppMethodKind::Method || method.name.empty() ||
        method.name.starts_with("operator") ||
        (method.inherited ? !dispatched : method.qualifiedName != record.qualifiedName + "::" + method.name) ||
        !projectedMethodAccess(method, dispatched) || method.isStatic ||
        ((method.isVirtual || method.isPureVirtual) && !dispatched) ||
        method.isDeleted || method.isVolatile || method.variadic ||
        method.refQualifier != CppMethodRefQualifier::None || method.callingConvention != "cdecl" ||
        (!dispatched && (!method.externalSymbol || method.externalSymbol->empty())) || method.canonicalFunctionType.empty())
        return fail("C++ instance method contract is not yet mapped");
    RecordIndex recordIndex;
    if (records) for (const auto& group : records->groups)
        if (group.projectOrdinal == projectOrdinal && !recordIndex.emplace(group.identity.key(), &group).second)
            return fail("duplicate record group for project");
    // EN: T11-005. With the name lookup, imported enums and trivial C++ records map by value, and lvalue
    //     references map to their referent as ref/in parameters and a ref return, as in a free function.
    // ES: T11-005. Con la busqueda de nombres, los enums y records C++ triviales importados se mapean por
    //     valor, y las referencias lvalue a su referente como parametros ref/in y retorno ref, como en una
    //     funcion libre.
    // T19-007: a `T&&` result as a read-only reference. / Un resultado `T&&` como referencia de solo lectura.
    const bool referenceReturn = method.returnType.kind == CppTypeKind::LValueReference ||
        method.returnType.kind == CppTypeKind::RValueReference;
    const auto returnCarrier = referenceReturn ? cppParameterReferenceCarrier(method.returnType) : std::nullopt;
    // T19-007: a `const T&` result is a read-only reference. / Un resultado `const T&` es de solo lectura.
    if (referenceReturn && !returnCarrier)
        return fail("C++ instance method reference return type is not yet mapped");
    auto result = mapFunctionType(returnCarrier ? *returnCarrier : method.returnType, true,
                                  records ? &recordIndex : nullptr, moduleName, 0, lookup, projectOrdinal,
                                  false, false, lookup != nullptr);
    if (!result) return fail("C++ instance method return type is not yet mapped");
    std::vector<CppScalarTypeMapping> parameters;
    std::vector<std::uint8_t> objectSizeModes;
    std::vector<frontend::ParameterPassKind> passKinds;
    std::vector<bool> rvalueReferences;
    parameters.reserve(method.parameters.size());
    for (const auto& parameter : method.parameters)
    {
        const bool rvalue = parameter.type.kind == CppTypeKind::RValueReference;
        const bool reference = parameter.type.kind == CppTypeKind::LValueReference || rvalue;
        const auto carrier = reference ? cppParameterReferenceCarrier(parameter.type) : std::nullopt;
        if (reference && (!carrier || parameter.objectSizeMode))
            return fail("C++ instance method reference parameter is not yet mapped");
        passKinds.push_back(!reference ? frontend::ParameterPassKind::Value :
            parameter.type.element->isConst ? frontend::ParameterPassKind::In : frontend::ParameterPassKind::Ref);
        rvalueReferences.push_back(rvalue);
        CppType type = carrier ? *carrier : parameter.type;
        if (parameter.objectSizeMode)
        {
            if (*parameter.objectSizeMode > 3 || type.kind != CppTypeKind::Pointer)
                return fail("C++ instance method object size contract is invalid");
            type.isConst = false;
            type.isVolatile = false;
        }
        else if (parameter.objectSizeDynamic) return fail("C++ instance method object size contract is invalid");
        auto mapped = mapFunctionType(type, true, records ? &recordIndex : nullptr, moduleName, 0, lookup,
                                      projectOrdinal, false, false, lookup != nullptr);
        if (!mapped || parameter.type.kind == CppTypeKind::Void)
            return fail("C++ instance method parameter type is not yet mapped");
        if (reference) mapped->isConst = false;
        parameters.push_back(std::move(*mapped));
        objectSizeModes.push_back(parameter.objectSizeMode
            ? static_cast<std::uint8_t>(1 + *parameter.objectSizeMode + (parameter.objectSizeDynamic ? 4 : 0)) : 0);
    }
    if (referenceReturn) result->isConst = false;
    // The adapter is defined by NSC's own Clang emission, never imported from a DLL.
    // El adaptador lo define la emision Clang de NSC; nunca se importa de una DLL.
    CppSemanticInstanceMethod projected{projectOrdinal, identity->key(),
        method.inherited ? record.qualifiedName + "::" + method.name : method.qualifiedName,
        dispatched ? *method.dispatchSymbol : *method.externalSymbol, method.canonicalFunctionType, method.isConst,
        std::move(*result), std::move(parameters), dispatched ? false : method.dllImport, std::move(objectSizeModes),
        std::move(passKinds), referenceReturn};
    projected.rvalueReferences = std::move(rvalueReferences);
    projected.access = method.access;
    projected.readonlyReferenceReturn = referenceReturn &&
        (method.returnType.element->isConst || method.returnType.kind == CppTypeKind::RValueReference);
    describeCppMethodOverload(method, projected.cppParameterSpellings, projected.cppSignatureSuffix,
                              projected.cppDeclarationSpelling);
    return projected;
}

frontend::CppImportedInstanceMethod toCppImportedInstanceMethod(const CppSemanticInstanceMethod& method)
{
    const auto separator = method.qualifiedName.rfind("::");
    frontend::CppImportedInstanceMethod result{method.projectOrdinal, method.ownerIdentityKey,
        separator == std::string::npos ? std::string{} : method.qualifiedName.substr(separator + 2),
        method.externalSymbol, method.result.nativeSharpTypeKey, {}, method.receiverConst, method.dllImport, false, {}, {}, false};
    result.returnPointeeReadonly = method.result.nativeSharpTypeKey.ends_with('*') && method.result.isConst;
    for (const auto& parameter : method.parameters)
    {
        result.parameterTypeKeys.push_back(parameter.nativeSharpTypeKey);
        result.parameterPointeeReadonly.push_back(parameter.nativeSharpTypeKey.ends_with('*') && parameter.isConst);
    }
    result.objectSizeModes = method.objectSizeModes;
    result.parameterPassKinds = method.passKinds;
    result.returnsReference = method.referenceReturn;
    result.returnsReadonlyReference = method.readonlyReferenceReturn;
    result.cppParameterSpellings = method.cppParameterSpellings;
    result.cppSignatureSuffix = method.cppSignatureSuffix;
    result.cppDeclarationSpelling = method.cppDeclarationSpelling;
    result.parameterRvalueReferences = method.rvalueReferences;
    // T19-012: the C++ access, which NativeSharp checks at each call. / El acceso C++, que NativeSharp comprueba.
    result.accessibility = method.access == CppAccess::Private ? frontend::ClassMemberAccessibility::Private :
        method.access == CppAccess::Protected ? frontend::ClassMemberAccessibility::Protected :
        frontend::ClassMemberAccessibility::Public;
    // T11-005: a ref return names its referent type, as a NativeSharp ref return does.
    if (method.referenceReturn && result.returnTypeKey.ends_with('*')) result.returnTypeKey.pop_back();
    return result;
}

CppSemanticStaticMethodResult projectCppSemanticStaticMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const CppRecordGroups* records,
    const frontend::ProjectNameLookup* lookup)
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
        method.qualifiedName != record.qualifiedName + "::" + method.name ||
        (method.access != CppAccess::Public && method.access != CppAccess::Protected &&
         method.access != CppAccess::Private) ||
        !method.isStatic || method.isVirtual || method.isPureVirtual || method.isDeleted || method.isVolatile ||
        method.variadic || method.refQualifier != CppMethodRefQualifier::None ||
        method.callingConvention != "cdecl" || !method.externalSymbol || method.externalSymbol->empty() ||
        method.canonicalFunctionType.empty()) return fail("C++ static method contract is not yet mapped");
    RecordIndex recordIndex;
    if (records) for (const auto& group : records->groups)
        if (group.projectOrdinal == projectOrdinal && !recordIndex.emplace(group.identity.key(), &group).second)
            return fail("duplicate record group for project");
    // T10-005: with the name lookup, an imported C++ enum or trivial C++ record maps by value as in a cdecl
    //     free function: a static method has no implicit object and the same Win64 convention.
    // T19-007: lvalue references as in an instance method. / Referencias lvalue como en un metodo de instancia.
    // T19-007: a `T&&` result as a read-only reference. / Un resultado `T&&` como referencia de solo lectura.
    const bool referenceReturn = method.returnType.kind == CppTypeKind::LValueReference ||
        method.returnType.kind == CppTypeKind::RValueReference;
    const auto returnCarrier = referenceReturn ? cppParameterReferenceCarrier(method.returnType) : std::nullopt;
    if (referenceReturn && !returnCarrier) return fail("C++ static method reference return type is not yet mapped");
    auto result = mapFunctionType(returnCarrier ? *returnCarrier : method.returnType, true,
                                  records ? &recordIndex : nullptr, moduleName, 0, lookup,
                                  projectOrdinal, false, false, true);
    if (!result) return fail("C++ static method return type is not yet mapped");
    std::vector<CppScalarTypeMapping> parameters;
    std::vector<std::uint8_t> objectSizeModes;
    std::vector<frontend::ParameterPassKind> passKinds;
    std::vector<bool> rvalueReferences;
    for (const auto& parameter : method.parameters)
    {
        const bool rvalue = parameter.type.kind == CppTypeKind::RValueReference;
        const bool reference = parameter.type.kind == CppTypeKind::LValueReference || rvalue;
        const auto carrier = reference ? cppParameterReferenceCarrier(parameter.type) : std::nullopt;
        if (reference && (!carrier || parameter.objectSizeMode))
            return fail("C++ static method reference parameter is not yet mapped");
        passKinds.push_back(!reference ? frontend::ParameterPassKind::Value :
            parameter.type.element->isConst ? frontend::ParameterPassKind::In : frontend::ParameterPassKind::Ref);
        rvalueReferences.push_back(rvalue);
        CppType type = carrier ? *carrier : parameter.type;
        if (parameter.objectSizeMode)
        {
            if (*parameter.objectSizeMode > 3 || type.kind != CppTypeKind::Pointer)
                return fail("C++ static method object size contract is invalid");
            type.isConst = false;
            type.isVolatile = false;
        }
        else if (parameter.objectSizeDynamic) return fail("C++ static method object size contract is invalid");
        auto mapped = mapFunctionType(type, true, records ? &recordIndex : nullptr, moduleName, 0, lookup,
                                      projectOrdinal, false, false, true);
        if (!mapped || parameter.type.kind == CppTypeKind::Void)
            return fail("C++ static method parameter type is not yet mapped");
        if (reference) mapped->isConst = false;
        parameters.push_back(std::move(*mapped));
        objectSizeModes.push_back(parameter.objectSizeMode
            ? static_cast<std::uint8_t>(1 + *parameter.objectSizeMode + (parameter.objectSizeDynamic ? 4 : 0)) : 0);
    }
    if (referenceReturn) result->isConst = false;
    CppSemanticStaticMethod projected{projectOrdinal, identity->key(), method.qualifiedName,
        *method.externalSymbol, method.canonicalFunctionType, std::move(*result), std::move(parameters), method.dllImport,
        std::move(objectSizeModes), method.access};
    projected.passKinds = std::move(passKinds);
    projected.rvalueReferences = std::move(rvalueReferences);
    projected.referenceReturn = referenceReturn;
    projected.readonlyReferenceReturn = referenceReturn &&
        (method.returnType.element->isConst || method.returnType.kind == CppTypeKind::RValueReference);
    describeCppMethodOverload(method, projected.cppParameterSpellings, projected.cppSignatureSuffix,
                              projected.cppDeclarationSpelling);
    return projected;
}

frontend::CppImportedStaticMethod toCppImportedStaticMethod(const CppSemanticStaticMethod& method)
{
    const auto separator = method.qualifiedName.rfind("::");
    frontend::CppImportedStaticMethod result{method.projectOrdinal, method.ownerIdentityKey,
        separator == std::string::npos ? method.qualifiedName : method.qualifiedName.substr(separator + 2),
        method.externalSymbol, method.result.nativeSharpTypeKey, {}, method.dllImport, false, {}, {}};
    result.returnPointeeReadonly = method.result.nativeSharpTypeKey.ends_with('*') && method.result.isConst;
    for (const auto& parameter : method.parameters)
    {
        result.parameterTypeKeys.push_back(parameter.nativeSharpTypeKey);
        result.parameterPointeeReadonly.push_back(parameter.nativeSharpTypeKey.ends_with('*') && parameter.isConst);
    }
    result.objectSizeModes = method.objectSizeModes;
    result.cppParameterSpellings = method.cppParameterSpellings;
    result.cppSignatureSuffix = method.cppSignatureSuffix;
    result.cppDeclarationSpelling = method.cppDeclarationSpelling;
    // T19-007: references as in an instance method. / Referencias como en un metodo de instancia.
    result.parameterPassKinds = method.passKinds;
    result.parameterRvalueReferences = method.rvalueReferences;
    result.returnsReference = method.referenceReturn;
    result.returnsReadonlyReference = method.readonlyReferenceReturn;
    if (method.referenceReturn && result.returnTypeKey.ends_with('*')) result.returnTypeKey.pop_back();
    // T10-007: a protected or private C++ method is a member NativeSharp code cannot reach, with its diagnostic.
    result.accessibility = method.access == CppAccess::Private ? frontend::ClassMemberAccessibility::Private :
        method.access == CppAccess::Protected ? frontend::ClassMemberAccessibility::Protected :
        frontend::ClassMemberAccessibility::Public;
    return result;
}

CppSemanticLifetimeMethodResult projectCppSemanticLifetimeMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const CppRecordGroups* records,
    const frontend::ProjectNameLookup* lookup)
{
    auto fail = [](std::string message) -> CppSemanticLifetimeMethodResult {
        return CppSemanticFunctionFailure{std::move(message)};
    };
    // EN: T12-006. A record with public or non-public non-virtual bases, polymorphic or not, is constructed and
    //     destroyed by its own Clang constructor and destructor, which construct the bases and set the vtable
    //     pointer. A virtual base needs the hidden most-derived argument of the Microsoft ABI: not mapped.
    // ES: T12-006. Un record con bases no virtuales, polimorfico o no, se construye y destruye con su propio
    //     constructor y destructor de Clang, que construyen las bases y fijan el puntero a la vtable. Una base
    //     virtual necesita el argumento oculto de objeto mas derivado del ABI de Microsoft: no se mapea.
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete ||
        std::any_of(record.bases.begin(), record.bases.end(), [](const auto& base) { return base.isVirtual; }) ||
        methodIndex >= record.methods.size()) return fail("lifetime method requires a complete record without virtual bases and supported target");
    const auto identity = projectCppRecordIdentity(record, moduleName);
    if (!identity) return fail("lifetime method owner has no supported identity");
    const auto& method = record.methods[methodIndex];
    if ((method.kind != CppMethodKind::Constructor && method.kind != CppMethodKind::Destructor) ||
        (method.access != CppAccess::Public && method.access != CppAccess::Protected &&
         method.access != CppAccess::Private) || method.isStatic ||
        // A local is destroyed by a direct call, also when the destructor is virtual, as C++ does.
        // Un local se destruye con una llamada directa, tambien si el destructor es virtual, como en C++.
        ((method.isVirtual || method.isPureVirtual) && method.kind != CppMethodKind::Destructor) ||
        method.isDeleted || method.isVolatile || method.variadic ||
        method.refQualifier != CppMethodRefQualifier::None || method.callingConvention != "cdecl" ||
        !method.externalSymbol || method.externalSymbol->empty() || method.canonicalFunctionType.empty() ||
        (method.kind == CppMethodKind::Destructor && !method.parameters.empty()))
        return fail("C++ lifetime method contract is not yet mapped");
    RecordIndex recordIndex;
    if (records) for (const auto& group : records->groups)
        if (group.projectOrdinal == projectOrdinal && !recordIndex.emplace(group.identity.key(), &group).second)
            return fail("duplicate record group for project");
    // EN: T11-006. With the name lookup, a constructor takes enums, trivial records, pointers to records and
    //     lvalue references (`const T&` as in, `T&` as ref), as a method does.
    // ES: T11-006. Con la busqueda de nombres, un constructor toma enums, records triviales, punteros a
    //     records y referencias lvalue (`const T&` como in, `T&` como ref), como un metodo.
    std::vector<CppScalarTypeMapping> parameters;
    std::vector<frontend::ParameterPassKind> passKinds;
    std::vector<bool> rvalueReferences;
    for (const auto& parameter : method.parameters)
    {
        if (parameter.objectSizeMode) return fail("C++ lifetime method object size is not yet mapped");
        // T11-016: the move constructor's `T&&` travels as the address of its source, like `T&`.
        CppType referenceType = parameter.type;
        if (method.kind == CppMethodKind::Constructor && method.parameters.size() == 1 &&
            referenceType.kind == CppTypeKind::RValueReference && referenceType.element &&
            referenceType.element->kind == CppTypeKind::Record)
            referenceType.kind = CppTypeKind::LValueReference;
        // T19-007: any other `T&&` too, as a ref that also binds a value. / Cualquier otro `T&&` tambien.
        const bool rvalue = referenceType.kind == CppTypeKind::RValueReference ||
            parameter.type.kind == CppTypeKind::RValueReference;
        const bool reference = referenceType.kind == CppTypeKind::LValueReference || rvalue;
        const auto carrier = reference ? cppParameterReferenceCarrier(referenceType) : std::nullopt;
        if (reference && !carrier) return fail("C++ lifetime method reference parameter is not yet mapped");
        auto mapped = mapFunctionType(carrier ? *carrier : parameter.type, true, records ? &recordIndex : nullptr,
                                      moduleName, 0, lookup, projectOrdinal, false, false, lookup != nullptr);
        if (!mapped || parameter.type.kind == CppTypeKind::Void)
            return fail("C++ lifetime method parameter type is not yet mapped");
        if (reference) mapped->isConst = false;
        passKinds.push_back(!reference ? frontend::ParameterPassKind::Value :
            referenceType.element->isConst ? frontend::ParameterPassKind::In : frontend::ParameterPassKind::Ref);
        rvalueReferences.push_back(rvalue);
        parameters.push_back(std::move(*mapped));
    }
    CppSemanticLifetimeMethod projected{projectOrdinal, identity->key(), method.kind, *method.externalSymbol,
                                    method.canonicalFunctionType, std::move(parameters),
                                    method.kind == CppMethodKind::Constructor
                                        ? CppLifetimeReturnAbi::ThisPointer : CppLifetimeReturnAbi::Void,
                                    method.dllImport, method.isNonThrowing, method.isTrivial,
                                    std::move(passKinds), method.access};
    projected.rvalueReferences = std::move(rvalueReferences);
    // T19-006: `S(int)` and `S(long)` stay two constructors. / Siguen siendo dos constructores.
    describeCppMethodOverload(method, projected.cppParameterSpellings, projected.cppSignatureSuffix,
                              projected.cppDeclarationSpelling);
    return projected;
}

frontend::CppImportedLifetimeMethod toCppImportedLifetimeMethod(const CppSemanticLifetimeMethod& method)
{
    frontend::CppImportedLifetimeMethod result{method.projectOrdinal, method.ownerIdentityKey,
        method.kind == CppMethodKind::Constructor ? frontend::CppImportedLifetimeKind::Constructor
                                                  : frontend::CppImportedLifetimeKind::Destructor,
        method.externalSymbol, {}, method.dllImport, method.isNonThrowing, false, method.isTrivial, {}};
    for (const auto& parameter : method.parameters)
    {
        result.parameterTypeKeys.push_back(parameter.nativeSharpTypeKey);
        result.parameterPointeeReadonly.push_back(parameter.nativeSharpTypeKey.ends_with('*') && parameter.isConst);
    }
    result.parameterPassKinds = method.passKinds;
    result.parameterRvalueReferences = method.rvalueReferences;
    result.cppParameterSpellings = method.cppParameterSpellings;
    result.cppSignatureSuffix = method.cppSignatureSuffix;
    result.cppDeclarationSpelling = method.cppDeclarationSpelling;
    // T11-007: a protected or private constructor or destructor keeps its access for NativeSharp to check.
    result.accessibility = method.access == CppAccess::Private ? frontend::ClassMemberAccessibility::Private :
        method.access == CppAccess::Protected ? frontend::ClassMemberAccessibility::Protected :
        frontend::ClassMemberAccessibility::Public;
    return result;
}

namespace
{
CppRecordGroups visibleMethodRecords(const CppRecordGroups& groups, const frontend::ProjectNameLookup& lookup);
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
    const auto visibleRecords = visibleMethodRecords(std::get<CppRecordGroups>(refined), lookup);
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
            const auto& declared = entry.declaration.methods[index];
            const auto kind = declared.kind;
            if (kind != CppMethodKind::Constructor && kind != CppMethodKind::Destructor) continue;
            // EN: T11-006. A deleted constructor cannot be called and one taking `T&&` has no NativeSharp
            //     argument; neither is a candidate, so they no longer hide the other constructors.
            // ES: T11-006. Un constructor eliminado no puede llamarse y uno que toma `T&&` no tiene argumento
            //     NativeSharp; ninguno es candidato, asi que ya no ocultan a los demas constructores.
            // EN: T11-016. The move constructor `T(T&&)` is the exception: NativeSharp calls it explicitly
            //     as `new T(ref a)`, so it is projected with a ref parameter. NativeSharp, like C#, does not
            //     overload on ref against in, so a callable copy constructor `T(const T&)` or `T(T&)` takes
            //     precedence and the move constructor stays out.
            // ES: T11-016. El constructor de movimiento `T(T&&)` es la excepcion: NativeSharp lo llama de forma
            //     explicita como `new T(ref a)`, asi que se proyecta con parametro ref. NativeSharp, como C#, no
            //     sobrecarga ref frente a in, asi que un constructor de copia invocable `T(const T&)` o `T(T&)`
            //     tiene prioridad y el de movimiento queda fuera.
            const auto selfReference = [&](const CppMethodDeclaration& candidate, CppTypeKind referenceKind) {
                return candidate.kind == CppMethodKind::Constructor && candidate.parameters.size() == 1 &&
                    candidate.parameters[0].type.kind == referenceKind && candidate.parameters[0].type.element &&
                    !candidate.parameters[0].type.element->isVolatile &&
                    candidate.parameters[0].type.element->kind == CppTypeKind::Record &&
                    candidate.parameters[0].type.element->recordIdentity &&
                    candidate.parameters[0].type.element->recordIdentity->clangUsr &&
                    candidate.parameters[0].type.element->recordIdentity->clangUsr == entry.declaration.clangUsr;
            };
            const bool moveConstructor = selfReference(declared, CppTypeKind::RValueReference) &&
                !declared.parameters[0].type.element->isConst &&
                std::none_of(entry.declaration.methods.begin(), entry.declaration.methods.end(), [&](const auto& other) {
                    return !other.isDeleted && selfReference(other, CppTypeKind::LValueReference);
                });
            // EN: T19-007. Another `T&&` parameter is a ref that also binds a value; only the move constructor that
            //     a copy constructor hides stays out (T11-016).
            // ES: T19-007. Otro parametro `T&&` es un ref que tambien enlaza un valor; solo queda fuera el
            //     constructor de movimiento que oculta un constructor de copia (T11-016).
            if (kind == CppMethodKind::Constructor &&
                (declared.isDeleted ||
                 (!moveConstructor && selfReference(declared, CppTypeKind::RValueReference)))) continue;
            auto projected = projectCppSemanticLifetimeMethod(entry.declaration, index, group.projectOrdinal,
                graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &visibleRecords, &lookup);
            if (auto* method = std::get_if<CppSemanticLifetimeMethod>(&projected))
            {
                std::vector<std::string> parameters;
                for (std::size_t i = 0; i < method->parameters.size(); ++i)
                {
                    const auto& parameter = method->parameters[i];
                    const auto passKind = i < method->passKinds.size() ? method->passKinds[i]
                                                                        : frontend::ParameterPassKind::Value;
                    parameters.push_back((passKind == frontend::ParameterPassKind::Ref ? "ref:" :
                        passKind == frontend::ParameterPassKind::In ? "in:" : "") +
                        (method->parameters.size() == 1 && parameter.isConst && parameter.nativeSharpTypeKey.ends_with('*')
                            ? "const:" + parameter.nativeSharpTypeKey : parameter.nativeSharpTypeKey));
                }
                // T19-006: the C++ spellings keep `S(int)` and `S(long)` apart. / Las separan.
                if (!method->cppSignatureSuffix.empty()) parameters.push_back(method->cppSignatureSuffix);
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
        const auto* imported = lookup.cppImportedType(binding.targetTypeOrdinal);
        if (!imported) continue;
        // T19-002: the zero value of a C struct or union has no lifetime. / El valor cero de un struct o una union
        //     C no tiene vida que emitir.
        if (binding.resolution == frontend::ConstructionResolutionKind::ResolvedImplicitDefault &&
            (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
             imported->origin == frontend::CppImportedTypeOrigin::CUnion)) continue;
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
        // T19-002: `new S()` of a C struct or union is a zero value without lifetime. / `new S()` de un struct o
        //     una union C es un valor cero sin vida.
        if (const auto* imported = lookup.cppImportedType(*value->targetTypeOrdinal); !value->memberOrdinal &&
            (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
             imported->origin == frontend::CppImportedTypeOrigin::CUnion)) continue;
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
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &visibleRecords, &lookup);
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
        // T19-006: the C++ spelling suffix keeps `g(int)` and `g(long)` apart. / Separa `g(int)` de `g(long)`.
        std::set<std::tuple<std::vector<std::string>, bool, std::string>> signatures;
        bool ambiguous = false;
        for (const auto& method : methods)
            if (!signatures.emplace(method.parameterTypeKeys,
                method.parameterTypeKeys.size() == 1 && method.parameterTypeKeys[0].ends_with('*') &&
                !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[0],
                method.cppSignatureSuffix).second) ambiguous = true;
        if (ambiguous) continue;
        for (auto& method : methods) result.push_back(std::move(method));
    }
    output = std::move(result);
    error.clear();
    return true;
}

// EN: T10-005. A type key of a live observation and of a saved method name the same C++ type when they are equal,
//     or when both name imported types with the same Clang USR, origin and qualified name in different modules,
//     behind the same pointer and array suffix.
// ES: T10-005. Una clave de tipo de una observacion viva y de un metodo guardado nombran el mismo tipo C++ si son
//     iguales, o si ambas nombran tipos importados con el mismo USR de Clang, origen y nombre calificado en modulos
//     distintos, tras el mismo sufijo de punteros y arreglos.
static bool sameSharedCppTypeKey(std::string_view live, std::string_view saved, const frontend::ProjectNameLookup& lookup)
{
    if (live == saved) return true;
    while (!live.empty() && !saved.empty() && live.back() == saved.back() && (live.back() == '*' || live.back() == ']'))
    {
        if (live.back() == ']')
        {
            const auto liveOpen = live.rfind('['), savedOpen = saved.rfind('[');
            if (liveOpen == std::string_view::npos || savedOpen == std::string_view::npos ||
                live.substr(liveOpen) != saved.substr(savedOpen)) return false;
            live = live.substr(0, liveOpen);
            saved = saved.substr(0, savedOpen);
            continue;
        }
        live.remove_suffix(1);
        saved.remove_suffix(1);
    }
    if (!live.starts_with("type:") || !saved.starts_with("type:")) return false;
    const auto liveOrdinal = lookup.typeOrdinalForIdentity(live.substr(5));
    const auto savedOrdinal = lookup.typeOrdinalForIdentity(saved.substr(5));
    const auto* a = liveOrdinal ? lookup.cppImportedType(*liveOrdinal) : nullptr;
    const auto* b = savedOrdinal ? lookup.cppImportedType(*savedOrdinal) : nullptr;
    return a && b && a->clangUsr && b->clangUsr && !a->clangUsr->empty() && *a->clangUsr == *b->clangUsr &&
        a->origin == b->origin && a->identity.fullyQualifiedName() == b->identity.fullyQualifiedName();
}

bool provideCppStaticMethodsWithPrecompiledCatalogs(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::PrecompiledSyntaxModule> modules, const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedStaticMethod>& output, std::string& error)
{
    std::vector<frontend::CppImportedStaticMethod> result;
    if (!provideExperimentalCppStaticMethods(graph, projects, lookup, result, error)) return false;
    std::map<std::string, std::size_t> owners;
    std::set<std::string> ambiguous;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
        if (!owners.emplace(graph.projects()[i].moduleName(), i).second)
            ambiguous.insert(graph.projects()[i].moduleName());
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return false;
        for (const auto& saved : dependencies.staticMethods)
        {
            const auto& function = saved.function;
            const auto owner = owners.find(function.moduleName);
            if (owner == owners.end() || ambiguous.contains(function.moduleName))
            { error = "precompiled C++ static method owner project is missing or ambiguous"; return false; }
            const auto record = std::find_if(records.begin(), records.end(), [&](const auto& value) {
                return value.identityKey == saved.ownerIdentityKey;
            });
            const auto ordinal = lookup.typeOrdinalForIdentity(saved.ownerIdentityKey);
            const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (record == records.end() || record->moduleName != function.moduleName || !record->complete ||
                record->targetTriple != "x86_64-pc-windows-msvc" || !record->cppValueObservation ||
                !record->clangUsr || *record->clangUsr != saved.ownerClangUsr || !imported ||
                imported->projectOrdinal != owner->second || imported->origin != frontend::CppImportedTypeOrigin::CppRecord ||
                !imported->clangUsr || *imported->clangUsr != saved.ownerClangUsr)
            { error = "precompiled C++ static method owner is incompatible"; return false; }
            CppRecordDeclaration declaration;
            declaration.name = record->name;
            declaration.namespaceSegments = record->namespaceSegments;
            declaration.containingRecords = record->containingRecords;
            declaration.kind = record->kind;
            declaration.complete = true;
            declaration.clangUsr = record->clangUsr;
            const auto separator = function.declaration.qualifiedName.rfind("::");
            if (separator == std::string::npos)
            { error = "precompiled C++ static method has no qualified owner"; return false; }
            declaration.qualifiedName = function.declaration.qualifiedName.substr(0, separator);
            CppMethodDeclaration method;
            method.name = function.declaration.name;
            method.qualifiedName = function.declaration.qualifiedName;
            method.canonicalFunctionType = function.declaration.canonicalFunctionType;
            method.callingConvention = function.declaration.callingConvention;
            method.returnType = function.declaration.returnType;
            method.parameters = function.declaration.parameters;
            method.access = CppAccess::Public;
            method.isStatic = true;
            method.isNonThrowing = function.declaration.isNonThrowing;
            method.dllImport = function.declaration.dllImport;
            method.externalSymbol = function.declaration.externalSymbol;
            method.clangUsr = function.declaration.clangUsr;
            declaration.methods.push_back(std::move(method));
            CppRecordGroups groups;
            for (const auto& item : records)
            {
                if (item.moduleName != function.moduleName) continue;
                std::vector<compilation::TypeNameSegment> containing;
                for (const auto& name : item.containingRecords) containing.push_back({name, 0});
                compilation::TypeIdentity identity(item.moduleName, item.namespaceSegments, containing, item.name);
                if (!identity.isValid() || identity.key() != item.identityKey || item.targetTriple != record->targetTriple)
                { error = "precompiled C++ static method record closure is invalid"; return false; }
                groups.groups.push_back({owner->second, std::move(identity), {},
                    item.complete ? CppRecordGroupStatus::OneDefinition : CppRecordGroupStatus::ForwardOnly, item.clangUsr});
            }
            auto projected = projectCppSemanticStaticMethod(declaration, 0, owner->second, function.moduleName,
                                                              record->targetTriple, &groups, &lookup);
            if (const auto* failure = std::get_if<CppSemanticFunctionFailure>(&projected))
            { error = "precompiled C++ static method cannot be projected: " + failure->message; return false; }
            auto value = toCppImportedStaticMethod(std::get<CppSemanticStaticMethod>(projected));
            if (value.ownerIdentityKey != saved.ownerIdentityKey || value.externalSymbol != function.linkSymbol)
            { error = "precompiled C++ static method identity changed during projection"; return false; }
            const auto existing = std::find_if(result.begin(), result.end(), [&](const auto& item) {
                return item.projectOrdinal == value.projectOrdinal && item.externalSymbol == value.externalSymbol;
            });
            if (existing != result.end() && existing->ownerIdentityKey != value.ownerIdentityKey)
            { error = "conflicting precompiled C++ static method owner"; return false; }
            for (auto& candidate : result)
            {
                if (candidate.externalSymbol != value.externalSymbol) continue;
                // Only reconcile a live observation with this saved method when Clang identity,
                // the logical contract and the actual linked artifacts all agree.
                auto inventory = collectCppSemanticRecords(graph, projects);
                const auto* entries = std::get_if<std::vector<CppSemanticRecordEntry>>(&inventory);
                if (!entries) { error = "cannot verify shared C++ static method observation"; return false; }
                auto refined = refineCppRecordGroupsForPointerTransport(*entries, groupCppSemanticRecords(*entries), projects);
                const auto* grouped = std::get_if<CppRecordGroups>(&refined);
                if (!grouped) { error = "cannot reconcile shared C++ static method owner"; return false; }
                const auto group = std::find_if(grouped->groups.begin(), grouped->groups.end(), [&](const auto& item) {
                    return item.projectOrdinal == candidate.projectOrdinal && item.identity.key() == candidate.ownerIdentityKey;
                });
                const auto selected = group == grouped->groups.end() ? std::nullopt :
                    selectCppRecordMemberDefinition(*entries, *group);
                if (!selected || *selected >= entries->size() || candidate.projectOrdinal >= graph.projects().size())
                { error = "shared C++ static method observation is ambiguous"; return false; }
                const auto& live = (*entries)[*selected];
                const auto& project = graph.projects()[candidate.projectOrdinal];
                if (!live.declaration.clangUsr || *live.declaration.clangUsr != saved.ownerClangUsr ||
                    live.referenceIndex >= project.cppLibraryReferences().size())
                { error = "shared C++ static method owner differs"; return false; }
                const auto method = std::find_if(live.declaration.methods.begin(), live.declaration.methods.end(),
                    [&](const auto& item) { return item.externalSymbol && *item.externalSymbol == value.externalSymbol; });
                if (method == live.declaration.methods.end() || method->clangUsr != function.declaration.clangUsr ||
                    method->canonicalFunctionType != function.declaration.canonicalFunctionType ||
                    candidate.name != value.name || !sameSharedCppTypeKey(candidate.returnTypeKey, value.returnTypeKey, lookup) ||
                    candidate.parameterTypeKeys.size() != value.parameterTypeKeys.size() ||
                    !std::equal(candidate.parameterTypeKeys.begin(), candidate.parameterTypeKeys.end(),
                        value.parameterTypeKeys.begin(), [&](const auto& a, const auto& b) {
                            return sameSharedCppTypeKey(a, b, lookup);
                        }) || candidate.objectSizeModes != value.objectSizeModes ||
                    candidate.dllImport != value.dllImport || candidate.returnPointeeReadonly != value.returnPointeeReadonly ||
                    candidate.parameterPointeeReadonly != value.parameterPointeeReadonly)
                { error = "shared C++ static method declaration differs"; return false; }
                const auto& reference = project.cppLibraryReferences()[live.referenceIndex];
                if (reference.libraryPath.empty()) { error = "shared C++ static method has no library identity"; return false; }
                basic::Sha256Digest digest{};
                std::uintmax_t size = 0;
                if (!basic::sha256File(reference.libraryPath, digest, size, error)) return false;
                std::string artifacts = "#lib:" + basic::sha256Hex(digest);
                if (!reference.runtimePath.empty())
                {
                    if (!basic::sha256File(reference.runtimePath, digest, size, error)) return false;
                    const auto name = reference.runtimePath.filename().generic_u8string();
                    artifacts += "#dll:" + std::string(reinterpret_cast<const char*>(name.data()), name.size()) +
                        ":" + basic::sha256Hex(digest);
                }
                const auto marker = function.referenceIdentity.find("#lib:");
                if (marker == std::string::npos || function.referenceIdentity.substr(marker) != artifacts)
                { error = "shared C++ static method library differs"; return false; }
                candidate.sharedExternalSymbolVerified = true;
                value.sharedExternalSymbolVerified = true;
            }
            if (existing != result.end()) continue;
            result.push_back(std::move(value));
        }
    }
    output = std::move(result); error.clear(); return true;
}

bool provideCppInstanceMethodsWithPrecompiledCatalogs(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::span<const frontend::PrecompiledSyntaxModule> modules, const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedInstanceMethod>& output, std::string& error)
{
    std::vector<frontend::CppImportedInstanceMethod> result;
    if (!provideExperimentalCppInstanceMethods(graph, projects, lookup, result, error)) return false;
    std::map<std::string, std::size_t> owners;
    std::set<std::string> ambiguous;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
        if (!owners.emplace(graph.projects()[i].moduleName(), i).second)
            ambiguous.insert(graph.projects()[i].moduleName());
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return false;
        for (const auto& savedInstance : dependencies.instanceMethods)
        {
            const auto& saved = savedInstance.method;
            const auto& function = saved.function;
            const auto owner = owners.find(function.moduleName);
            if (owner == owners.end() || ambiguous.contains(function.moduleName))
            { error = "precompiled C++ instance method owner project is missing or ambiguous"; return false; }
            const auto record = std::find_if(records.begin(), records.end(), [&](const auto& value) {
                return value.identityKey == saved.ownerIdentityKey;
            });
            const auto ordinal = lookup.typeOrdinalForIdentity(saved.ownerIdentityKey);
            const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (record == records.end() || record->moduleName != function.moduleName || !record->complete ||
                record->targetTriple != "x86_64-pc-windows-msvc" || !record->cppValueObservation ||
                !record->clangUsr || *record->clangUsr != saved.ownerClangUsr || !imported ||
                imported->projectOrdinal != owner->second || imported->origin != frontend::CppImportedTypeOrigin::CppRecord ||
                !imported->clangUsr || *imported->clangUsr != saved.ownerClangUsr)
            { error = "precompiled C++ instance method owner is incompatible"; return false; }
            CppRecordDeclaration declaration;
            declaration.name = record->name;
            declaration.namespaceSegments = record->namespaceSegments;
            declaration.containingRecords = record->containingRecords;
            declaration.kind = record->kind;
            declaration.complete = true;
            declaration.clangUsr = record->clangUsr;
            const auto separator = function.declaration.qualifiedName.rfind("::");
            if (separator == std::string::npos)
            { error = "precompiled C++ instance method has no qualified owner"; return false; }
            declaration.qualifiedName = function.declaration.qualifiedName.substr(0, separator);
            CppMethodDeclaration method;
            method.name = function.declaration.name;
            method.qualifiedName = function.declaration.qualifiedName;
            method.canonicalFunctionType = function.declaration.canonicalFunctionType;
            method.callingConvention = function.declaration.callingConvention;
            method.returnType = function.declaration.returnType;
            method.parameters = function.declaration.parameters;
            method.access = CppAccess::Public;
            method.isStatic = false;
            method.isConst = savedInstance.receiverConst;
            method.isNonThrowing = function.declaration.isNonThrowing;
            method.dllImport = function.declaration.dllImport;
            method.externalSymbol = function.declaration.externalSymbol;
            method.clangUsr = function.declaration.clangUsr;
            // EN: T19-006. A method saved with a Clang adapter's symbol is called through that adapter.
            // ES: T19-006. Un metodo guardado con el simbolo de un adaptador de Clang se llama por ese adaptador.
            if (std::string_view(function.linkSymbol).starts_with(cppMemberThunkPrefix))
                method.dispatchSymbol = function.linkSymbol;
            declaration.methods.push_back(std::move(method));
            CppRecordGroups groups;
            for (const auto& item : records)
            {
                if (item.moduleName != function.moduleName) continue;
                std::vector<compilation::TypeNameSegment> containing;
                for (const auto& name : item.containingRecords) containing.push_back({name, 0});
                compilation::TypeIdentity identity(item.moduleName, item.namespaceSegments, containing, item.name);
                if (!identity.isValid() || identity.key() != item.identityKey || item.targetTriple != record->targetTriple)
                { error = "precompiled C++ instance method record closure is invalid"; return false; }
                groups.groups.push_back({owner->second, std::move(identity), {},
                    item.complete ? CppRecordGroupStatus::OneDefinition : CppRecordGroupStatus::ForwardOnly, item.clangUsr});
            }
            auto projected = projectCppSemanticInstanceMethod(declaration, 0, owner->second, function.moduleName,
                                                              record->targetTriple, &groups, &lookup);
            if (const auto* failure = std::get_if<CppSemanticFunctionFailure>(&projected))
            { error = "precompiled C++ instance method cannot be projected: " + failure->message; return false; }
            auto value = toCppImportedInstanceMethod(std::get<CppSemanticInstanceMethod>(projected));
            if (value.ownerIdentityKey != saved.ownerIdentityKey || value.externalSymbol != function.linkSymbol)
            { error = "precompiled C++ instance method identity changed during projection"; return false; }
            const auto existing = std::find_if(result.begin(), result.end(), [&](const auto& item) {
                return item.projectOrdinal == value.projectOrdinal && item.externalSymbol == value.externalSymbol;
            });
            if (existing != result.end() && existing->ownerIdentityKey != value.ownerIdentityKey)
            { error = "conflicting precompiled C++ instance method owner"; return false; }
            for (auto& candidate : result)
            {
                if (candidate.externalSymbol != value.externalSymbol) continue;
                // Only reconcile a live observation with this saved method when Clang identity,
                // the logical contract and the actual linked artifacts all agree.
                auto inventory = collectCppSemanticRecords(graph, projects);
                const auto* entries = std::get_if<std::vector<CppSemanticRecordEntry>>(&inventory);
                if (!entries) { error = "cannot verify shared C++ instance method observation"; return false; }
                auto refined = refineCppRecordGroupsForPointerTransport(*entries, groupCppSemanticRecords(*entries), projects);
                const auto* grouped = std::get_if<CppRecordGroups>(&refined);
                if (!grouped) { error = "cannot reconcile shared C++ instance method owner"; return false; }
                const auto group = std::find_if(grouped->groups.begin(), grouped->groups.end(), [&](const auto& item) {
                    return item.projectOrdinal == candidate.projectOrdinal && item.identity.key() == candidate.ownerIdentityKey;
                });
                const auto selected = group == grouped->groups.end() ? std::nullopt :
                    selectCppRecordMemberDefinition(*entries, *group);
                if (!selected || *selected >= entries->size() || candidate.projectOrdinal >= graph.projects().size())
                { error = "shared C++ instance method observation is ambiguous"; return false; }
                const auto& live = (*entries)[*selected];
                const auto& project = graph.projects()[candidate.projectOrdinal];
                if (!live.declaration.clangUsr || *live.declaration.clangUsr != saved.ownerClangUsr ||
                    live.referenceIndex >= project.cppLibraryReferences().size())
                { error = "shared C++ instance method owner differs"; return false; }
                // T19-006: an adapter is found by its own symbol. / Un adaptador se busca por su propio simbolo.
                const auto method = std::find_if(live.declaration.methods.begin(), live.declaration.methods.end(),
                    [&](const auto& item) {
                        const auto& symbol = item.dispatchSymbol && !item.dispatchSymbol->empty()
                            ? item.dispatchSymbol : item.externalSymbol;
                        return symbol && *symbol == value.externalSymbol;
                    });
                if (method == live.declaration.methods.end() || method->clangUsr != function.declaration.clangUsr ||
                    method->canonicalFunctionType != function.declaration.canonicalFunctionType ||
                    candidate.receiverConst != value.receiverConst || method->isConst != savedInstance.receiverConst ||
                    candidate.name != value.name || candidate.returnTypeKey != value.returnTypeKey ||
                    candidate.parameterTypeKeys != value.parameterTypeKeys || candidate.objectSizeModes != value.objectSizeModes ||
                    candidate.dllImport != value.dllImport || candidate.returnPointeeReadonly != value.returnPointeeReadonly ||
                    candidate.parameterPointeeReadonly != value.parameterPointeeReadonly)
                { error = "shared C++ instance method declaration differs"; return false; }
                const auto& reference = project.cppLibraryReferences()[live.referenceIndex];
                if (reference.libraryPath.empty()) { error = "shared C++ instance method has no library identity"; return false; }
                basic::Sha256Digest digest{};
                std::uintmax_t size = 0;
                if (!basic::sha256File(reference.libraryPath, digest, size, error)) return false;
                std::string artifacts = "#lib:" + basic::sha256Hex(digest);
                if (!reference.runtimePath.empty())
                {
                    if (!basic::sha256File(reference.runtimePath, digest, size, error)) return false;
                    const auto name = reference.runtimePath.filename().generic_u8string();
                    artifacts += "#dll:" + std::string(reinterpret_cast<const char*>(name.data()), name.size()) +
                        ":" + basic::sha256Hex(digest);
                }
                const auto marker = function.referenceIdentity.find("#lib:");
                if (marker == std::string::npos || function.referenceIdentity.substr(marker) != artifacts)
                { error = "shared C++ instance method library differs"; return false; }
                candidate.sharedExternalSymbolVerified = true;
                value.sharedExternalSymbolVerified = true;
            }
            if (existing != result.end()) continue;
            result.push_back(std::move(value));
        }
    }
    output = std::move(result); error.clear(); return true;
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
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &visibleRecords, &lookup);
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
        // T19-006: the C++ spelling suffix keeps `f(int)` and `f(long)` apart. / Separa `f(int)` de `f(long)`.
        std::set<std::tuple<std::vector<std::string>, bool, bool, std::string>> signatures;
        bool ambiguous = false;
        for (const auto& method : methods)
            if (!signatures.emplace(method.parameterTypeKeys,
                method.parameterTypeKeys.size() == 1 && method.parameterTypeKeys[0].ends_with('*') &&
                !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[0],
                method.receiverConst, method.cppSignatureSuffix).second) ambiguous = true;
        if (ambiguous) continue;
        for (auto& method : methods) result.push_back(std::move(method));
    }
    output = std::move(result);
    error.clear();
    return true;
}
}
