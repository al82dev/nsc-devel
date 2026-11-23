#include "../CComplexConversion.h"
#include <algorithm>
#include <iterator>
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "Support/OwnershipArgumentClassification.h"
#include "Support/OperatorClassification.h"
#include "Support/IntegralConstantArithmetic.h"
#include "Support/OwnershipFlowSnapshotQueries.h"


#include "MethodBodyInternals.h"
#include "../Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

ProjectImplicitConversion ProjectMethodBodyModel::classifyImportedFlexibleConversion(
    const ExpressionResult& source, std::string_view targetTypeKey) const
{
    const auto element = ProjectMemberModel::flexibleArrayElementTypeKey(source.typeKey);
    if (source.cppFlexibleArrayMember && source.cppFlexibleArrayDecaySafe && element &&
        targetTypeKey == *element + "*")
        return {ProjectConversionKind::CFlexibleArrayToPointer, 1, source.typeKey,
                std::string(targetTypeKey), std::nullopt};
    return classifyImplicit(source.typeKey, targetTypeKey, source.integerConstant);
}

ProjectImplicitConversion ProjectMethodBodyModel::classifyCFrontierConversion(
    const ExpressionResult& source, const MethodParameterSymbol& parameter, std::string_view targetTypeKey,
    bool cLinkage) const
{
    const std::string target(targetTypeKey);
    // EN: Item 3.2 (D2). A literal reaches a C `const char*` (or `const unsigned char*`) as its NUL-terminated
    //     UTF-8 bytes. A parameter that may write through the pointer does not receive the literal.
    // ES: Item 3.2 (D2). Un literal llega a un `const char*` C (o `const unsigned char*`) como sus bytes UTF-8 con
    //     terminador NUL. Un parametro que puede escribir a traves del puntero no recibe el literal.
    if (source.typeKey == "string-literal" &&
        (target == "builtin:sbyte*" || target == "builtin:byte*" || target == "builtin:char*") &&
        parameter.passKind == ParameterPassKind::Value && parameter.pointerPointeeReadonly)
        return {ProjectConversionKind::StringLiteralToCharPointer, 4, source.typeKey, target, std::nullopt};
    // EN: Item 3.3 (D3). Between integer and floating types C converts implicitly to the parameter type
    //     (`double` to `float`, `int` to `unsigned int`); only at this boundary, never between NativeSharp values.
    //     `bool` and `char` keep NativeSharp's rules: a C `_Bool` takes any nonzero value as true only through an
    //     explicit cast.
    // ES: Item 3.3 (D3). Entre tipos enteros y flotantes C convierte implicitamente al tipo del parametro (`double`
    //     a `float`, `int` a `unsigned int`); solo en esta frontera, nunca entre valores NativeSharp. `bool` y `char`
    //     conservan las reglas NativeSharp: un `_Bool` C toma todo valor distinto de cero como verdadero solo con un
    //     cast explicito.
    static constexpr std::string_view arithmetic[] = {"builtin:sbyte", "builtin:byte", "builtin:short",
        "builtin:ushort", "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong", "builtin:float",
        "builtin:double"};
    const auto isArithmetic = [](std::string_view key)
    { return std::find(std::begin(arithmetic), std::end(arithmetic), key) != std::end(arithmetic); };
    // Only with C linkage: a C++ function keeps NativeSharp's conversions. / Solo con enlace C.
    if (cLinkage && parameter.passKind == ParameterPassKind::Value && isArithmetic(source.typeKey) &&
        isArithmetic(target))
        return {ProjectConversionKind::Numeric, 5, source.typeKey, target, std::nullopt};
    // EN: Item 7.1. A non-capturing lambda written as the argument of an imported C/C++ function reaches a C
    //     function pointer of a Win64-equivalent convention through its generated thunk. Only at this boundary:
    //     between NativeSharp values the lambda keeps the exact convention match (T08-F02).
    // ES: Item 7.1. Una lambda sin capturas escrita como argumento de una funcion C/C++ importada llega a un
    //     puntero a funcion C de convencion equivalente en Win64 por su thunk generado. Solo en esta frontera:
    //     entre valores NativeSharp la lambda conserva la igualdad exacta de convencion (T08-F02).
    if (parameter.passKind == ParameterPassKind::Value)
        if (const auto lambda = language::parseAnonymousLambdaTypeIdentity(source.typeKey))
            if (const auto function = language::parseFunctionTypeIdentity(target);
                function && function->carrier == language::FunctionCarrier::Pointer &&
                language::functionTypesAssignable(language::lambdaSignatureForTarget(lambda->signature, *function),
                                                  *function))
                return {ProjectConversionKind::Function, 1, source.typeKey, target, std::nullopt};
    // EN: C3-06. An argument of a member type of a C transparent union parameter is that member, as C passes it.
    // ES: C3-06. Un argumento de un tipo miembro de un parametro union transparente C es ese miembro, como lo pasa C.
    if (parameter.passKind == ParameterPassKind::Value &&
        std::find(parameter.cppTransparentUnionAlternatives.begin(), parameter.cppTransparentUnionAlternatives.end(),
                  source.typeKey) != parameter.cppTransparentUnionAlternatives.end())
        return {ProjectConversionKind::PointerToVoid, 2, source.typeKey, target, std::nullopt};
    // EN: Item 4.7. A fixed array decays to a pointer to its first element at a C boundary, as in C: `int[2][4]`
    //     becomes `int (*)[4]` and `int[3]` becomes `int*`. Only for an addressable array place.
    // ES: Item 4.7. Un arreglo fijo decae a puntero a su primer elemento en una frontera C, como en C: `int[2][4]`
    //     pasa a `int (*)[4]` e `int[3]` pasa a `int*`. Solo para un lugar arreglo direccionable.
    if (parameter.passKind == ParameterPassKind::Value && source.addressable)
        if (const auto element = ProjectMemberModel::fixedArrayElementTypeKey(source.typeKey);
            element && *element + "*" == target)
            return {ProjectConversionKind::CFlexibleArrayToPointer, 4, source.typeKey, target, std::nullopt};
    return {ProjectConversionKind::None, 1000, source.typeKey, target, std::nullopt};
}

ProjectImplicitConversion ProjectMethodBodyModel::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant) const
{
    if (sourceTypeKey == "builtin:null" &&
        delegateTypeOrdinalForTypeKey(targetTypeKey).has_value())
    {
        return ProjectImplicitConversion{
            ProjectConversionKind::NullReference, 4, std::string(sourceTypeKey),
            std::string(targetTypeKey), std::nullopt};
    }
    if (inheritanceModel_ == nullptr)
    {
        return ProjectTypeConversion::classifyImplicit(
            sourceTypeKey,
            targetTypeKey,
            integerConstant,
            isReferenceTypeKey(targetTypeKey));
    }
    ProjectImplicitConversion conversion =
        ProjectConversionClassifier(*inheritanceModel_, interfaceModel_, memberModel_).classifyImplicit(
            sourceTypeKey,
            targetTypeKey,
            integerConstant);
    // T19-039: C `_Complex` conversions. / Conversiones C `_Complex`.
    if (!conversion.exists())
        if (auto complex = classifyCComplexConversion(inheritanceModel_, memberModel_, namespaceFunctionModel_,
                                                      sourceTypeKey, targetTypeKey, integerConstant))
            conversion = std::move(*complex);
    return conversion;
}

bool ProjectMethodBodyModel::reportTupleConversionFailure(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const std::optional<std::vector<std::string>> source =
        ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
    const std::optional<std::vector<std::string>> target =
        ProjectTypeConversion::tupleElementTypeKeys(targetTypeKey);
    if (!source.has_value() || !target.has_value()) return false;
    if (source->size() != target->size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "tuple conversion requires equal arity; source has " +
                std::to_string(source->size()) + " element(s) and target has " +
                std::to_string(target->size()));
        return true;
    }
    for (std::size_t index = 0; index < source->size(); ++index)
    {
        if (classifyImplicit((*source)[index], (*target)[index]).exists()) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "cannot convert tuple element " + std::to_string(index + 1) + " from '" +
                (*source)[index] + "' to '" + (*target)[index] + "'");
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::isCompleteCRecordTypeKey(std::string_view typeKey) const
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:") || typeKey.ends_with("*")) return false;
    const std::optional<std::size_t> ordinal = nameLookup_->typeOrdinalForIdentity(typeKey.substr(5));
    const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
    return imported != nullptr && !imported->isIncomplete && imported->deferredImportError.empty() &&
        (imported->origin == CppImportedTypeOrigin::CStruct || imported->origin == CppImportedTypeOrigin::CUnion);
}

ProjectImplicitConversion ProjectMethodBodyModel::classifyCVariadicPromotion(
    std::string_view sourceTypeKey, bool preserveCppScopedEnum) const
{
    if (preserveCppScopedEnum && nameLookup_ && sourceTypeKey.starts_with("type:") && !sourceTypeKey.ends_with('*'))
    {
        const auto ordinal = nameLookup_->typeOrdinalForIdentity(sourceTypeKey.substr(5));
        const auto* imported = ordinal ? nameLookup_->cppImportedType(*ordinal) : nullptr;
        if (imported && imported->origin == CppImportedTypeOrigin::CppEnum)
        {
            if (!imported->cppEnumScoped || imported->isIncomplete || !imported->enumBase ||
                !imported->deferredImportError.empty())
                return ProjectImplicitConversion{ProjectConversionKind::None, 1000, std::string(sourceTypeKey), {}, std::nullopt};
            if (*imported->cppEnumScoped)
                return ProjectImplicitConversion{ProjectConversionKind::Identity, 10, std::string(sourceTypeKey),
                                                 std::string(sourceTypeKey), std::nullopt};
        }
    }
    // EN: T19-004. A 128-bit integer is not a C variadic argument yet: the Windows x64 C ABI passes it by address,
    //     and the variadic call would pass an LLVM `i128` value (T19-044).
    // ES: T19-004. Un entero de 128 bits todavia no es un argumento variadico C: el ABI C de Windows x64 lo pasa
    //     por direccion, y la llamada variadica pasaria un valor `i128` de LLVM (T19-044).
    // EN: T19-044. It is now: the call passes the address of an aligned copy (lowerStaticCall), as Clang does.
    // ES: T19-044. Ahora si: la llamada pasa la direccion de una copia alineada (lowerStaticCall), como Clang.
    std::string targetTypeKey(sourceTypeKey);
    ProjectConversionKind kind = ProjectConversionKind::Identity;

    if (sourceTypeKey == "builtin:float")
    {
        targetTypeKey = "builtin:double";
        kind = ProjectConversionKind::Numeric;
    }
    else if (sourceTypeKey == "builtin:bool")
    {
        targetTypeKey = "builtin:int";
        kind = ProjectConversionKind::IntegralPromotion;
    }
    else if (const builtins::IntegerTypeInfo* integer = builtins::findIntegerType(sourceTypeKey);
             integer != nullptr)
    {
        if (integer->width < 32U)
        {
            targetTypeKey = "builtin:int";
            kind = ProjectConversionKind::IntegralPromotion;
        }
    }
    else if (isEnumTypeKey(sourceTypeKey))
    {
        const NamedTypeDeclaration* type = namedTypeForTypeKey(sourceTypeKey);
        if (type != nullptr)
        {
            const std::string_view underlying = enumUnderlyingTypeKey(type->syntax);
            const builtins::IntegerTypeInfo* integer = builtins::findIntegerType(underlying);
            if (integer == nullptr)
            {
                return ProjectImplicitConversion{
                    ProjectConversionKind::None, 1000, std::string(sourceTypeKey), {}, std::nullopt};
            }
            if (integer->width < 32U)
            {
                targetTypeKey = "builtin:int";
            }
            else
            {
                targetTypeKey = underlying;
            }
            // An enum is passed through its C-compatible integer representation even when the
            // representation already has the width of int or wider. Keeping the named enum key
            // here would make the call-site type depend on a NativeSharp-only identity.
            kind = ProjectConversionKind::IntegralPromotion;
        }
    }
    else if (isCompleteCRecordTypeKey(sourceTypeKey))
    {
        // EN: T08-002: C passes a struct or union through `...` unchanged; the call lowering already
        //     gives it the Win64 by-value or by-copy carrier, exactly as for a fixed C parameter.
        // ES: T08-002: C pasa un struct o union por `...` sin cambios; el lowering de la llamada ya le da el
        //     portador Win64 por valor o por copia, igual que a un parámetro C fijo.
    }
    else if (sourceTypeKey != "builtin:double" &&
             !isRawPointerTypeKey(sourceTypeKey) &&
             !isExternCDelegateTypeKey(sourceTypeKey))
    {
        return ProjectImplicitConversion{
            ProjectConversionKind::None, 1000, std::string(sourceTypeKey), {}, std::nullopt};
    }

    // Vararg conversions deliberately rank below every conversion to a written parameter. This
    // keeps a fixed overload preferable when both it and a variadic import are otherwise viable.
    return ProjectImplicitConversion{
        kind, 10, std::string(sourceTypeKey), std::move(targetTypeKey), std::nullopt};
}

std::string ProjectMethodBodyModel::implicitThisTypeKey(std::size_t methodMemberOrdinal) const
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || nameLookup_ == nullptr ||
        method->declaringTypeOrdinal >= nameLookup_->namedTypes().size())
    {
        return "<error>";
    }
    return "type:" + nameLookup_->namedTypes()[method->declaringTypeOrdinal].identity.key();
}


bool ProjectMethodBodyModel::isClassReferenceTypeKey(std::string_view typeKey) const noexcept
{
    return classTypeOrdinalForTypeKey(typeKey).has_value();
}

// C4 "language references": the call site and the signature must agree about `ref`/`out`/`in`.
// Overload resolution has already separated the candidates by TYPE -- a by-reference parameter
// takes `T*`, a by-value one takes `T` -- so what is left is the readability rule C# has for the
// same reason: a call must SHOW that its argument can change. Writing the keyword is therefore
// required where the parameter declares one, and refused where it does not.
void ProjectMethodBodyModel::checkArgumentPassKinds(
    const ast::AstNode* syntax,
    std::string_view methodName,
    std::size_t memberOrdinal,
    const std::vector<ExpressionResult>& arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::span<const std::size_t> parameterOrdinals) const
{
    if (memberModel_ == nullptr || memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& chosen = memberModel_->members()[memberOrdinal];
    for (std::size_t index = 0; index < arguments.size() && index < chosen.parameters.size(); ++index)
    {
        const std::size_t parameterOrdinal = parameterOrdinals.empty() ? index : parameterOrdinals[index];
        if (parameterOrdinal >= chosen.parameters.size()) continue;
        const ParameterPassKind expected = chosen.parameters[parameterOrdinal].passKind;
        const ParameterPassKind written = arguments[index].argumentPassKind;
        if (expected == written) continue;
        // T19-007: a value for a C++ `T&&` needs no keyword. / Un valor para un `T&&` C++ no lleva palabra clave.
        if (written == ParameterPassKind::Value && (chosen.parameters[parameterOrdinal].cppRvalueReference ||
                                                    chosen.parameters[parameterOrdinal].cppConstReference)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            expected == ParameterPassKind::Value
                ? "argument " + std::to_string(index + 1) + " of '" + std::string(methodName) +
                    "' is not declared '" + parameterPassKindKeyword(written) + "'"
                : "argument " + std::to_string(index + 1) + " of '" + std::string(methodName) +
                    "' must be passed with '" + parameterPassKindKeyword(expected) + "'");
    }
}

// C6 "struct members": the type ordinal of anything that can carry ACCESSOR members -- a class,
// an interface or a struct. Distinct from referenceTypeOrdinalForTypeKey, which answers only for
// the two that are references: a struct is a value, and that difference matters for how the
// receiver is passed (by address) but not at all for whether it may declare an indexer.
std::optional<std::size_t> ProjectMethodBodyModel::accessorReceiverTypeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (inheritanceModel_ == nullptr) return std::nullopt;
    const std::optional<std::size_t> ordinal = inheritanceModel_->typeOrdinalForTypeKey(typeKey);
    if (!ordinal.has_value()) return std::nullopt;
    if (inheritanceModel_->isClass(*ordinal) ||
        (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)))
    {
        return ordinal;
    }
    if (nameLookup_ != nullptr && *ordinal < nameLookup_->namedTypes().size() &&
        nameLookup_->namedTypes()[*ordinal].kind == NamedTypeDeclarationKind::Struct)
    {
        return ordinal;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::isReferenceTypeKey(std::string_view typeKey) const noexcept
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Reference;
    return referenceTypeOrdinalForTypeKey(typeKey).has_value();
}

bool ProjectMethodBodyModel::typeKeyIsOwnable(std::string_view typeKey) const noexcept
{
    // La cadena nativa entra aqui aunque no sea una referencia de clase: posee su propio
    // almacenamiento y `owned string` es legal. Es la misma expresion que la resolucion de
    // llamadas usaba en linea antes de NSC-LIMIT-010.
    return isReferenceTypeKey(typeKey) || typeKey == "builtin:string" ||
        (isAnyDelegateTypeKey(typeKey) && !isExternCDelegateTypeKey(typeKey));
}

// C1.1 "function-pointer round trip" -- the type-key predicates for the three sides of the trip.
// A named type key is "type:<identity>"; the delegate half of the answer therefore needs the name
// lookup (an enum, a struct and a delegate all share that prefix), and "is it EXTERN C" is the
// same `extern` modifier probe the backend's isExternCDelegateType already uses -- one rule, two
// places that must agree, so both read the syntax modifier rather than a cached flag.
bool ProjectMethodBodyModel::isAnyDelegateTypeKey(std::string_view typeKey) const noexcept
{
    if (language::isFunctionTypeIdentity(typeKey)) return true;
    const NamedTypeDeclaration* type = namedTypeForTypeKey(typeKey);
    return type != nullptr && type->kind == NamedTypeDeclarationKind::Delegate;
}

}
