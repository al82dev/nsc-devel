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
    return ProjectConversionClassifier(*inheritanceModel_, interfaceModel_, memberModel_).classifyImplicit(
        sourceTypeKey,
        targetTypeKey,
        integerConstant);
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

ProjectImplicitConversion ProjectMethodBodyModel::classifyCVariadicPromotion(
    std::string_view sourceTypeKey) const
{
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
