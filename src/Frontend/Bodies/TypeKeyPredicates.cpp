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

bool ProjectMethodBodyModel::isEnumTypeKey(std::string_view typeKey) const noexcept
{
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != key) continue;
        return type.kind == NamedTypeDeclarationKind::Enum;
    }
    return false;
}

// CC1 fases 4B/4C -- ver la declaracion para el criterio.
//
// Se pregunta por DOS cosas, y las dos por separado porque son dos razones distintas:
//
//   * un destructor declarado: alguien escribio codigo que TIENE que correr al final de la vida
//     del objeto, y en memoria cruda no hay quien lo llame;
//   * un campo `owned`: el objeto posee otra asignacion, y liberar (o abandonar) el contenedor
//     sin descargarla la filtra.
//
// Los builtins y los punteros son triviales por construccion. Un enum tambien. Una CLASE como
// elemento tampoco es problema: lo que se reserva es un buffer de REFERENCIAS, no de objetos --
// las referencias son punteros y no poseen nada por si mismas.
bool ProjectMethodBodyModel::elementTypeHasTrivialLifetime(std::string_view typeKey) const noexcept
{
    if (const std::optional<std::vector<std::string>> tuple =
            ProjectTypeConversion::tupleElementTypeKeys(typeKey);
        tuple.has_value())
    {
        return std::ranges::all_of(*tuple, [&](const std::string& element)
        {
            return elementTypeHasTrivialLifetime(element);
        });
    }
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return true;
    if (!typeKey.starts_with("type:")) return true;

    const std::string_view identity = typeKey.substr(5);
    std::optional<std::size_t> typeOrdinal;
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        if (nameLookup_->namedTypes()[ordinal].identity.key() == identity)
        {
            typeOrdinal = ordinal;
            break;
        }
    }
    if (!typeOrdinal.has_value()) return true;
    if (*typeOrdinal < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclarationKind kind = nameLookup_->namedTypes()[*typeOrdinal].kind;
        // Una referencia no posee nada por si misma; un enum es un entero con nombre.
        if (kind != NamedTypeDeclarationKind::Struct) return true;
    }

    for (const ClassMemberSymbol& member : memberModel_->members())
    {
        if (member.declaringTypeOrdinal != *typeOrdinal) continue;
        if (member.kind == ClassMemberKind::Destructor) return false;
        if (member.kind == ClassMemberKind::Field && !member.isStatic &&
            member.declaredOwnership == sema::OwnershipKind::Owned)
        {
            return false;
        }
    }
    return true;
}

bool ProjectMethodBodyModel::isExternCDelegateTypeKey(std::string_view typeKey) const noexcept
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Pointer;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != key) continue;
        return type.kind == NamedTypeDeclarationKind::Delegate && type.syntax != nullptr &&
            type.syntax->hasModifier(lexer::TokenKind::KwExtern);
    }
    return false;
}

bool ProjectMethodBodyModel::isRawPointerTypeKey(std::string_view typeKey) noexcept
{
    // `T*` / `void*`. Native arrays ("...[]") are a different shape and are deliberately not
    // pointers here, even though both lower to `ptr`.
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Pointer;
    return !typeKey.empty() && typeKey.back() == '*';
}

bool ProjectMethodBodyModel::isPointerSizedIntegralTypeKey(std::string_view typeKey) noexcept
{
    // Deliberately NOT every integral: only the two spellings whose whole purpose is to hold an
    // address. Casting a function pointer through `int` would silently truncate on Win64.
    return typeKey == "builtin:nint" || typeKey == "builtin:nuint";
}

bool ProjectMethodBodyModel::canRuntimeReferenceConvert(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey) const noexcept
{
    if (sourceTypeKey == "builtin:null")
    {
        return isReferenceTypeKey(targetTypeKey);
    }
    if (inheritanceModel_ == nullptr || nameLookup_ == nullptr)
    {
        return false;
    }
    const std::optional<std::size_t> source = referenceTypeOrdinalForTypeKey(sourceTypeKey);
    const std::optional<std::size_t> target = referenceTypeOrdinalForTypeKey(targetTypeKey);
    if (!source.has_value() || !target.has_value())
    {
        return false;
    }
    if (*source == *target)
    {
        return true;
    }

    const bool sourceClass = inheritanceModel_->isClass(*source);
    const bool targetClass = inheritanceModel_->isClass(*target);
    const bool sourceInterface = interfaceModel_ != nullptr && interfaceModel_->isInterface(*source);
    const bool targetInterface = interfaceModel_ != nullptr && interfaceModel_->isInterface(*target);

    if (sourceClass && targetClass)
    {
        return inheritanceModel_->isDerivedFrom(*source, *target) ||
            inheritanceModel_->isDerivedFrom(*target, *source);
    }
    if (sourceInterface && targetInterface)
    {
        return true;
    }
    if (sourceClass && targetInterface)
    {
        const bool implements = interfaceModel_ != nullptr &&
            interfaceModel_->classImplementsInterface(*source, *target);
        return implements || (*source < nameLookup_->namedTypes().size() &&
            !nameLookup_->namedTypes()[*source].isSealed);
    }
    if (sourceInterface && targetClass)
    {
        const bool implements = interfaceModel_ != nullptr &&
            interfaceModel_->classImplementsInterface(*target, *source);
        return implements || (*target < nameLookup_->namedTypes().size() &&
            !nameLookup_->namedTypes()[*target].isSealed);
    }
    return false;
}

const ClassMemberSymbol* ProjectMethodBodyModel::methodSymbol(
    std::size_t methodMemberOrdinal) const noexcept
{
    if (memberModel_ == nullptr) return nullptr;
    if (methodMemberOrdinal < memberModel_->members().size())
        return &memberModel_->members()[methodMemberOrdinal];
    if (methodMemberOrdinal < namespaceCallableBase_) return nullptr;
    const std::size_t functionOrdinal = methodMemberOrdinal - namespaceCallableBase_;
    return functionOrdinal < namespaceCallableAdapters().size()
        ? &namespaceCallableAdapters()[functionOrdinal] : nullptr;
}

const ClassMemberSymbol* ProjectMethodBodyModel::callableSymbol(
    std::size_t methodMemberOrdinal) const noexcept
{
    return methodSymbol(methodMemberOrdinal);
}

std::span<const ClassMemberSymbol>
ProjectMethodBodyModel::namespaceCallableAdapters() const noexcept
{
    return namespaceCallableAdaptersView_;
}

bool ProjectMethodBodyModel::callableBelongsToGenericTypeSpecialization(
    std::size_t methodMemberOrdinal) const noexcept
{
    if (memberModel_ == nullptr || methodMemberOrdinal >= memberModel_->members().size())
    {
        return false;
    }
    const ClassMemberSymbol* callable = methodSymbol(methodMemberOrdinal);
    if (callable == nullptr || genericModel_ == nullptr || inheritanceModel_ == nullptr)
    {
        return false;
    }
    return genericModel_->specializationForTypeKey(
        inheritanceModel_->typeKeyForOrdinal(callable->declaringTypeOrdinal)) != nullptr;
}

}
